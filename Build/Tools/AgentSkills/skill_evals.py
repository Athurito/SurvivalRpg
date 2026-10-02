"""Validate, run, and grade the agent-skill evaluation scenarios in evals/.

Each evals/<skill>.json holds at least three scenarios in the evaluation structure of Anthropic's
skill-authoring guide (skills, query, files, expected_behavior) plus a stable id and optional
excluded_skills. `run` starts one fresh, read-only headless Claude Code or Codex session per scenario,
keeps the raw transcript under Saved/AgentSkillEvals, and checks triggering: every skill in `skills`
must load and no skill in `excluded_skills` may. `grade` has a judge model score each final answer
against `expected_behavior`; `report` prints a Markdown table for the results log.
Exit codes: 0 passed, 1 failed, 2 setup or usage error. See README.md next to this script.
"""
from __future__ import annotations

import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
from dataclasses import dataclass, field
from datetime import datetime
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import time

import sync

REPO = sync.REPO
EVALS = "Build/Tools/AgentSkills/evals"
RUNS = REPO / "Saved/AgentSkillEvals"
TOOLS = ("claude", "codex")

PASSED, FAILED, SETUP = 0, 1, 2
MIN_SCENARIOS = 3        # the skill-authoring guide asks for at least three evaluations per skill
TIMEOUT_MINUTES = 15     # per session; only stops hung sessions, raise it with --timeout for slow models
MAX_TOOL_LINE = 500      # characters kept per condensed tool call; Codex chains several file reads per command
MAX_TOOL_CALLS = 200     # tool calls shown to the judge
MAX_ANSWER = 40000       # answer characters shown to the judge

REQUIRED_FIELDS = ("id", "skills", "query", "expected_behavior")
LIST_FIELDS = ("skills", "excluded_skills", "files", "expected_behavior")
# Appended to every query. Sessions must not change the checkout, and headless runs have no
# interactive question tool, so decisions that belong to the user go into the answer text.
READ_ONLY_NOTE = (
    "\n\n---\nBewertungslauf: Arbeite nur lesend. Untersuche das Repository, soweit nötig, und antworte "
    "mit deinem konkreten Vorgehen oder Plan, statt Dateien zu ändern, Builds zu starten oder Assets "
    "anzulegen. Entscheidungen, die beim Nutzer liegen, stellst du als Frage im Text.")
JUDGE_SCHEMA = {
    "type": "object",
    "properties": {
        "items": {"type": "array", "items": {
            "type": "object",
            "properties": {"index": {"type": "integer"}, "met": {"type": "boolean"}, "evidence": {"type": "string"}},
            "required": ["index", "met", "evidence"],
            "additionalProperties": False}},
        "summary": {"type": "string"},
    },
    "required": ["items", "summary"],
    "additionalProperties": False,
}


class SetupError(RuntimeError):
    """Scenarios, executables, or arguments are not usable; no session started."""


# --- Scenarios ---------------------------------------------------------------------------------

def scenario_problems(root: Path, where: str, suite: str, scenario, names: list[str],
                      identifiers: set[str]) -> list[str]:
    if not isinstance(scenario, dict):
        return [f"{where}: expected an object"]
    problems = [f"{where}: missing field '{name}'" for name in REQUIRED_FIELDS if name not in scenario]
    for name in LIST_FIELDS:
        value = scenario.get(name, [])
        if not isinstance(value, list) or not all(isinstance(item, str) and item.strip() for item in value):
            problems.append(f"{where}: {name} must be a list of non-empty strings")
    if not isinstance(scenario.get("query"), str) or not scenario["query"].strip():
        problems.append(f"{where}: query must be a non-empty string")
    if problems:
        return problems
    problems += [f"{where}: unknown field '{name}'" for name in scenario if name not in REQUIRED_FIELDS + LIST_FIELDS]
    if not isinstance(scenario["id"], str) or not sync.NAME_PATTERN.match(scenario["id"]):
        problems.append(f"{where}: id must be lowercase words joined by hyphens")
    skills, excluded = scenario["skills"], scenario.get("excluded_skills", [])
    if not skills:
        problems.append(f"{where}: skills must name at least one skill")
    if not scenario["expected_behavior"]:
        problems.append(f"{where}: expected_behavior must list at least one behavior")
    problems += [f"{where}: unknown skill '{name}'" for name in skills + excluded if name not in names]
    problems += [f"{where}: '{name}' is both expected and excluded" for name in sorted(set(skills) & set(excluded))]
    if suite not in skills + excluded:
        problems.append(f"{where}: name '{suite}' in skills or excluded_skills; its file evaluates that skill")
    problems += [f"{where}: missing file {name}" for name in scenario.get("files", []) if not (root / name).is_file()]
    text = "\n".join([scenario["query"], *scenario["expected_behavior"]])
    return problems + sync.span_problems(root, where, text, identifiers)


def load_scenarios(root: Path) -> tuple[list[dict], list[str]]:
    """Every valid scenario with its file's skill under `suite`, and every problem found."""
    names = sync.discover(root)
    identifiers = sync.source_identifiers(root)
    scenarios, problems, counts = [], [], dict.fromkeys(names, 0)
    for path in sorted((root / EVALS).glob("*.json")):
        label = sync.rel(root, path)
        if path.stem not in names:
            problems.append(f"{label}: no canonical skill named '{path.stem}'")
            continue
        try:
            data = json.loads(path.read_text(encoding="utf-8"))
        except json.JSONDecodeError as error:
            problems.append(f"{label}: invalid JSON: {error}")
            continue
        if not isinstance(data, list):
            problems.append(f"{label}: expected a JSON array of scenarios")
            continue
        for index, scenario in enumerate(data, start=1):
            found = scenario_problems(root, f"{label} #{index}", path.stem, scenario, names, identifiers)
            problems += found
            if not found:
                scenarios.append(dict(scenario, suite=path.stem))
                counts[path.stem] += 1
    seen: set[str] = set()
    for scenario in scenarios:
        if scenario["id"] in seen:
            problems.append(f"{EVALS}: duplicate id {scenario['id']}")
        seen.add(scenario["id"])
    problems += [f"{EVALS}/{name}.json: {count} valid scenarios, at least {MIN_SCENARIOS} required"
                 for name, count in counts.items() if count < MIN_SCENARIOS]
    return scenarios, problems


def select(scenarios: list[dict], ids: list[str], suites: list[str]) -> list[dict]:
    unknown = sorted(set(ids) - {scenario["id"] for scenario in scenarios})
    unknown += sorted(set(suites) - {scenario["suite"] for scenario in scenarios})
    if unknown:
        raise SetupError(f"no scenario or skill named: {', '.join(unknown)}")
    if not ids and not suites:
        return scenarios
    return [scenario for scenario in scenarios if scenario["id"] in ids or scenario["suite"] in suites]


# --- Transcripts -------------------------------------------------------------------------------

def compact(text: str, limit: int = MAX_TOOL_LINE) -> str:
    text = " ".join(text.split())
    return text if len(text) <= limit else text[: limit - 3] + "..."


def skill_pattern(names: list[str]) -> re.Pattern:
    """A skill file named in a tool call: .agents/skills/<name>/... or .claude/skills/<name>/..., either slash."""
    alternatives = "|".join(re.escape(name) for name in sorted(names, key=len, reverse=True))
    return re.compile(rf"skills[\\/]+({alternatives})[\\/]+([\w.\-\\/]*?\.md)\b", re.IGNORECASE)


@dataclass
class Transcript:
    """What one session did: loaded skills, other skill files read, condensed tool calls, final answer."""
    pattern: re.Pattern
    loaded: list[str] = field(default_factory=list)
    skill_files: list[str] = field(default_factory=list)
    tools: list[str] = field(default_factory=list)
    available: list[str] = field(default_factory=list)
    answer: str = ""
    usage: dict = field(default_factory=dict)
    error: str = ""

    def load(self, skill: str) -> None:
        if skill and skill not in self.loaded:
            self.loaded.append(skill)

    def call(self, name: str, detail: str) -> None:
        """Record a tool call; only its input is scanned, so listings or search output never count as a load."""
        self.tools.append(compact(f"{name} {detail}"))
        for match in self.pattern.finditer(detail):
            skill, path = match.group(1).lower(), re.sub(r"[\\/]+", "/", match.group(2))
            if path.lower() == "skill.md":
                self.load(skill)
            elif f"{skill}/{path}" not in self.skill_files:
                self.skill_files.append(f"{skill}/{path}")


def events(lines: list[str]):
    for line in lines:
        try:
            event = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(event, dict):
            yield event


def parse_claude(lines: list[str], names: list[str]) -> Transcript:
    """Claude Code stream-json: a Skill call or a SKILL.md read loads a skill; ExitPlanMode carries the plan."""
    transcript = Transcript(skill_pattern(names))
    plan, texts, result = "", [], None
    for event in events(lines):
        kind = event.get("type")
        if kind == "system" and event.get("subtype") == "init":
            transcript.available = [str(name) for name in event.get("skills") or []]
        elif kind == "assistant":
            for block in (event.get("message") or {}).get("content") or []:
                if block.get("type") == "text":
                    texts.append(str(block.get("text", "")))
                elif block.get("type") == "tool_use":
                    name, arguments = str(block.get("name", "")), block.get("input") or {}
                    if name == "Skill":
                        transcript.load(str(arguments.get("skill", "")).lstrip("/"))
                    elif name == "ExitPlanMode":
                        plan = str(arguments.get("plan", ""))
                    transcript.call(name, json.dumps(arguments, ensure_ascii=False))
        elif kind == "result":
            result = event
    final = str(result.get("result") or "") if result else ""
    final = final or (texts[-1] if texts else "")
    parts = [plan] if plan.strip() else []
    if final.strip() and final.strip() not in plan:
        parts.append(final)
    transcript.answer = "\n\n".join(parts)
    if result is None:
        transcript.error = "no result event"
    else:
        transcript.usage = {key: result[key] for key in ("total_cost_usd", "num_turns", "duration_ms") if key in result}
        if result.get("is_error") or result.get("subtype", "success") != "success":
            transcript.error = compact(f"{result.get('subtype', 'error')}: {final}")
    return transcript


def parse_codex(lines: list[str], names: list[str]) -> Transcript:
    """Codex exec --json: a command that reads skills/<name>/SKILL.md loads it; the last agent message answers."""
    transcript = Transcript(skill_pattern(names))
    seen = set()
    for event in events(lines):
        kind = event.get("type")
        if kind in ("item.started", "item.completed"):
            item = event.get("item") or {}
            item_type = str(item.get("type", ""))
            if item_type == "agent_message":
                if kind == "item.completed":
                    transcript.answer = str(item.get("text", ""))
            elif item_type not in ("reasoning", "todo_list") and (item.get("id") is None or item["id"] not in seen):
                seen.add(item.get("id"))
                inputs = {key: value for key, value in item.items()
                          if key not in ("id", "type", "status", "aggregated_output", "exit_code", "result", "output", "error")}
                detail = item.get("command") if isinstance(item.get("command"), str) else json.dumps(inputs, ensure_ascii=False)
                transcript.call(item_type, detail)
        elif kind == "turn.completed":
            transcript.usage = event.get("usage") or {}
        elif kind in ("turn.failed", "error"):
            error = event.get("error")
            message = error.get("message") if isinstance(error, dict) else event.get("message")
            transcript.error = compact(str(message or kind))
    if not transcript.answer and not transcript.error:
        transcript.error = "no agent message"
    return transcript


def trigger(scenario: dict, loaded: list[str]) -> tuple[list[str], list[str]]:
    """(expected skills that did not load, excluded skills that did)."""
    missing = [name for name in scenario["skills"] if name not in loaded]
    unexpected = [name for name in scenario.get("excluded_skills", []) if name in loaded]
    return missing, unexpected


# --- Sessions ----------------------------------------------------------------------------------

def shown(path: Path) -> str:
    try:
        return path.resolve().relative_to(REPO).as_posix()
    except ValueError:
        return str(path)


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def write_json(path: Path, data: dict) -> None:
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def executable(tool: str, explicit: str | None) -> str:
    found = shutil.which(explicit or tool)
    if not found:
        hint = "pass --exe with the path of a logged-in CLI" if explicit is None else "check --exe"
        raise SetupError(f"{explicit or tool} not found; {hint}")
    return found


def output_of(command: list[str], cwd: Path = REPO) -> str:
    try:
        completed = subprocess.run(command, cwd=cwd, stdin=subprocess.DEVNULL, capture_output=True, timeout=60)
    except (OSError, subprocess.TimeoutExpired):
        return "unknown"
    text = completed.stdout.decode("utf-8", errors="replace").strip()
    return text.splitlines()[0] if completed.returncode == 0 and text else "unknown"


def session_command(tool: str, exe: str, model: str | None, budget: float | None) -> list[str]:
    """Fresh, read-only, non-persisted session that reads the prompt from stdin and streams JSON events."""
    if tool == "claude":
        command = [exe, "-p", "--permission-mode", "plan", "--output-format", "stream-json", "--verbose",
                   "--no-session-persistence"]
        if budget:
            command += ["--max-budget-usd", str(budget)]
    else:
        command = [exe, "exec", "--sandbox", "read-only", "--json", "--ephemeral", "-C", str(REPO)]
    if model:
        command += ["--model", model]
    return command + (["-"] if tool == "codex" else [])


def kill_tree(process: subprocess.Popen) -> None:
    if os.name == "nt":
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(process.pid)], capture_output=True)
    else:
        process.kill()
    process.wait()


def run_session(command: list[str], prompt: str, cwd: Path, stdout_path: Path, stderr_path: Path,
                timeout_minutes: float) -> tuple[int | None, bool, float]:
    """Run with the prompt on stdin. Returns (exit code, timed out, seconds); kills the tree on timeout or Ctrl+C."""
    started = time.monotonic()
    with stdout_path.open("wb") as stdout, stderr_path.open("wb") as stderr:
        process = subprocess.Popen(command, cwd=cwd, stdin=subprocess.PIPE, stdout=stdout, stderr=stderr,
                                   creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            process.communicate(prompt.encode("utf-8"), timeout=timeout_minutes * 60)
            return process.returncode, False, time.monotonic() - started
        except subprocess.TimeoutExpired:
            kill_tree(process)
            return None, True, time.monotonic() - started
        except KeyboardInterrupt:
            kill_tree(process)
            raise


def run_scenario(tool: str, command: list[str], scenario: dict, names: list[str], run_dir: Path,
                 timeout_minutes: float) -> dict:
    sid = scenario["id"]
    files = scenario.get("files", [])
    prompt = scenario["query"] + (f"\n\nDateien: {', '.join(files)}" if files else "") + READ_ONLY_NOTE
    transcript_path = run_dir / f"{sid}.jsonl"
    code, timed_out, seconds = run_session(command, prompt, REPO, transcript_path, run_dir / f"{sid}.stderr.txt",
                                           timeout_minutes)
    lines = transcript_path.read_bytes().decode("utf-8", errors="replace").splitlines()
    transcript = (parse_claude if tool == "claude" else parse_codex)(lines, names)
    error = "timed out" if timed_out else transcript.error or (f"exit code {code}" if code else "")
    missing, unexpected = trigger(scenario, transcript.loaded)
    result = {key: scenario.get(key, []) for key in ("id", "suite", "query", "skills", "excluded_skills", "expected_behavior")}
    result.update({
        "status": "error" if error else "failed" if missing or unexpected else "passed",
        "error": error, "exit_code": code, "seconds": round(seconds),
        "loaded": transcript.loaded, "missing": missing, "unexpected": unexpected,
        "skill_files": transcript.skill_files, "available_skills": transcript.available,
        "usage": transcript.usage, "tools": transcript.tools, "answer": transcript.answer,
    })
    write_json(run_dir / f"{sid}.result.json", result)
    return result


def trigger_state(result: dict) -> str:
    if result["status"] == "error":
        return f"error: {result['error']}"
    problems = [f"missing {', '.join(result['missing'])}"] if result["missing"] else []
    if result["unexpected"]:
        problems.append(f"unexpected {', '.join(result['unexpected'])}")
    return "; ".join(problems) or "pass"


def result_line(result: dict) -> str:
    label = {"passed": "PASS", "failed": "FAIL", "error": "ERROR"}[result["status"]]
    return (f"{label} {result['id']}: {trigger_state(result)} "
            f"(loaded: {', '.join(result['loaded']) or 'none'}, {result['seconds']}s)")


def run_directory(label: str, requested: Path | None) -> Path:
    """A fresh folder per run so old transcripts are never mistaken for new ones."""
    if requested:
        path = Path(requested).resolve()
        if path.exists() and any(path.iterdir()):
            raise SetupError(f"--out must be a new or empty directory: {path}")
    else:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        safe = re.sub(r"[^\w.-]+", "_", label)
        path, index = RUNS / f"{stamp}-{safe}", 2
        while path.exists():
            path, index = RUNS / f"{stamp}-{safe}-{index}", index + 1
    path.mkdir(parents=True, exist_ok=True)
    return path


def commit() -> str:
    head = output_of(["git", "rev-parse", "--short", "HEAD"])
    dirty = subprocess.run(["git", "status", "--porcelain"], cwd=REPO, stdin=subprocess.DEVNULL,
                           capture_output=True).stdout.strip()
    return head + ("+dirty" if dirty else "")


def command_run(args) -> int:
    scenarios, problems = load_scenarios(REPO)
    if problems:
        raise SetupError("invalid scenarios, see the check command:\n  " + "\n  ".join(problems))
    selected = select(scenarios, args.id, args.skill)
    names = sync.discover(REPO)
    exe = executable(args.tool, args.exe)
    command = session_command(args.tool, exe, args.model, args.budget)
    run_dir = run_directory(f"{args.tool}-{args.model}" if args.model else args.tool, args.out)
    write_json(run_dir / "run.json", {
        "tool": args.tool, "model": args.model or "default", "version": output_of([exe, "--version"]),
        "commit": commit(), "started": datetime.now().isoformat(timespec="seconds"), "command": command,
        "scenarios": [scenario["id"] for scenario in selected]})
    print(f"run: {shown(run_dir)} ({len(selected)} scenarios, {args.tool} {args.model or 'default model'})", flush=True)
    results = []
    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        futures = [pool.submit(run_scenario, args.tool, command, scenario, names, run_dir, args.timeout)
                   for scenario in selected]
        for future in as_completed(futures):
            results.append(future.result())
            print(result_line(results[-1]), flush=True)
    counts = {status: sum(result["status"] == status for result in results) for status in ("passed", "failed", "error")}
    print(f"TRIGGER {'PASSED' if counts['passed'] == len(results) else 'FAILED'}: "
          f"{counts['passed']} passed, {counts['failed']} failed, {counts['error']} errors")
    print(f"next: python Build/Tools/AgentSkills/skill_evals.py grade {shown(run_dir)}")
    return PASSED if counts["passed"] == len(results) else FAILED


# --- Grading -----------------------------------------------------------------------------------

def judge_prompt(result: dict, expected: list[str]) -> str:
    rubric = "\n".join(f"{index}. {item}" for index, item in enumerate(expected, start=1))
    tools = "\n".join(result["tools"][:MAX_TOOL_CALLS]) or "(none)"
    if len(result["tools"]) > MAX_TOOL_CALLS:
        tools += f"\n... {len(result['tools']) - MAX_TOOL_CALLS} more calls"
    return (
        "You grade one evaluation run of a coding agent in the SurvivalRpg Unreal Engine repository. The agent was "
        "told to work read-only and to answer with its approach or plan instead of changing files, building, or "
        "creating assets, so nothing has to be executed: a planned step counts when the answer commits to it "
        "concretely.\n\n"
        f"<task>\n{result['query']}\n</task>\n\n"
        f"<expected_behavior>\n{rubric}\n</expected_behavior>\n\n"
        f"<skills_loaded>{', '.join(result['loaded']) or 'none'}</skills_loaded>\n\n"
        f"<tool_calls>\n{tools}\n</tool_calls>\n\n"
        f"<final_answer>\n{result['answer'][:MAX_ANSWER]}\n</final_answer>\n\n"
        "Judge each expected behavior independently, using only the final answer and the tool calls. Mark it met "
        "when they clearly show its core behavior; missing, vague, or contradicted behavior is not met. Examples "
        "introduced with 'such as' or 'for example' illustrate the behavior and are not a checklist. An inspection "
        "counts when the tool calls show it or the answer reports concrete findings from those files. Give short "
        "evidence for every item, quoting the answer where possible. Reply with JSON only: "
        '{"items": [{"index": 1, "met": true, "evidence": "..."}], "summary": "one sentence"}')


def json_object(text: str):
    start, end = text.find("{"), text.rfind("}")
    if start < 0 or end < start:
        raise ValueError("no JSON object in judge output")
    return json.loads(text[start:end + 1])


def index_of(item: dict) -> int | None:
    try:
        return int(item.get("index"))
    except (TypeError, ValueError):
        return None


def parse_verdict(text: str, expected: list[str]) -> dict:
    """The judge's verdict, from Claude's json result event or from Codex's schema-checked last message."""
    data = json_object(text)
    if data.get("type") == "result":
        if data.get("is_error"):
            raise ValueError(compact(str(data.get("result", "judge error"))))
        data = json_object(str(data.get("result", "")))
    verdicts = {index_of(item): item for item in data.get("items", []) if isinstance(item, dict)}
    items = []
    for index, behavior in enumerate(expected, start=1):
        verdict = verdicts.get(index)
        if verdict is None:
            raise ValueError(f"judge gave no verdict for item {index}")
        items.append({"expected": behavior, "met": verdict.get("met") is True, "evidence": str(verdict.get("evidence", ""))})
    return {"met": sum(item["met"] for item in items), "total": len(items), "items": items,
            "summary": str(data.get("summary", ""))}


def judge(tool: str, exe: str, model: str | None, prompt: str, expected: list[str], timeout_minutes: float) -> dict:
    """Ask a tool-less judge session outside the repository, so project instructions and skills cannot leak in."""
    with tempfile.TemporaryDirectory(prefix="skill-eval-judge-") as folder:
        workdir = Path(folder)
        if tool == "claude":
            command = [exe, "-p", "--tools", "", "--output-format", "json", "--no-session-persistence",
                       "--disable-slash-commands"]
        else:
            (workdir / "schema.json").write_text(json.dumps(JUDGE_SCHEMA), encoding="utf-8")
            command = [exe, "exec", "--sandbox", "read-only", "--ephemeral", "--skip-git-repo-check", "-C", str(workdir),
                       "--output-schema", str(workdir / "schema.json"), "-o", str(workdir / "verdict.json")]
        if model:
            command += ["--model", model]
        command += ["-"] if tool == "codex" else []
        stdout, stderr = workdir / "stdout.txt", workdir / "stderr.txt"
        code, timed_out, _ = run_session(command, prompt, workdir, stdout, stderr, timeout_minutes)
        if timed_out:
            raise ValueError("judge timed out")
        source = workdir / "verdict.json" if tool == "codex" else stdout
        text = source.read_bytes().decode("utf-8", errors="replace") if source.is_file() else ""
        if code and not text:
            raise ValueError(f"judge exit code {code}: {compact(stderr.read_bytes().decode('utf-8', errors='replace'))}")
        return parse_verdict(text, expected)


def run_folder(value: str) -> Path:
    path = Path(value)
    path = path if path.is_absolute() else (Path.cwd() / path)
    if not (path / "run.json").is_file():
        raise SetupError(f"not a run folder (no run.json): {value}")
    return path.resolve()


def rubric(result: dict, scenarios: dict[str, dict]) -> list[str]:
    """The scenario's current expected_behavior while its query is unchanged, else the one stored with the run."""
    scenario = scenarios.get(result["id"])
    return scenario["expected_behavior"] if scenario and scenario["query"] == result["query"] else result["expected_behavior"]


def command_grade(args) -> int:
    run_dir = run_folder(args.run)
    exe = executable(args.judge, args.exe)
    parse = parse_claude if read_json(run_dir / "run.json")["tool"] == "claude" else parse_codex
    names = sync.discover(REPO)
    scenarios = {scenario["id"]: scenario for scenario in load_scenarios(REPO)[0]}
    met = total = graded = skipped = 0
    for path in sorted(run_dir.glob("*.result.json")):
        result = read_json(path)
        grade_path = run_dir / f"{result['id']}.grade.json"
        if result["status"] == "error":
            skipped += 1
            print(f"SKIP {result['id']}: session error ({result['error']})")
            continue
        expected = rubric(result, scenarios)
        grade = read_json(grade_path) if grade_path.is_file() else None
        if args.force or grade is None or [item["expected"] for item in grade["items"]] != expected:
            # Re-read the raw transcript so parser fixes reach the judge, as for old runs graded again.
            transcript_path = run_dir / f"{result['id']}.jsonl"
            if transcript_path.is_file():
                transcript = parse(transcript_path.read_bytes().decode("utf-8", errors="replace").splitlines(), names)
                result = dict(result, tools=transcript.tools, answer=transcript.answer or result["answer"])
            try:
                grade = judge(args.judge, exe, args.model, judge_prompt(result, expected), expected, args.timeout)
            except (ValueError, KeyError, json.JSONDecodeError) as error:
                skipped += 1
                print(f"SKIP {result['id']}: {error}")
                continue
            grade.update({"judge": args.judge, "model": args.model or "default"})
            write_json(grade_path, grade)
        graded += 1
        met, total = met + grade["met"], total + grade["total"]
        print(f"{grade['met']}/{grade['total']} {result['id']}")
        for index, item in enumerate(grade["items"], start=1):
            if not item["met"]:
                print(f"  not met {index}: {compact(item['expected'], 120)}")
                print(f"    {compact(item['evidence'], 160)}")
    complete = graded and met == total and not skipped
    print(f"BEHAVIOR {'PASSED' if complete else 'FAILED'}: {met}/{total} expectations met in {graded} scenarios, "
          f"{skipped} not graded")
    return PASSED if complete else FAILED


# --- Report ------------------------------------------------------------------------------------

def command_report(args) -> int:
    print("| Scenario | Tool, model | Trigger | Loaded skills | Behavior | Not met |")
    print("| --- | --- | --- | --- | --- | --- |")
    headers = []
    for value in args.runs:
        run_dir = run_folder(value)
        run = read_json(run_dir / "run.json")
        headers.append(f"- {run_dir.name}: {run['tool']} {run['version']}, model {run['model']}, "
                       f"commit {run['commit']}, started {run['started']}")
        for path in sorted(run_dir.glob("*.result.json")):
            result = read_json(path)
            grade_path = run_dir / f"{result['id']}.grade.json"
            grade = read_json(grade_path) if grade_path.is_file() else None
            behavior = f"{grade['met']}/{grade['total']}" if grade else "–"
            not_met = [str(index) for index, item in enumerate(grade["items"], start=1) if not item["met"]] if grade else []
            cells = [result["id"], f"{run['tool']}, {run['model']}", trigger_state(result),
                     ", ".join(result["loaded"]) or "none", behavior, ", ".join(not_met) or "–"]
            print("| " + " | ".join(cell.replace("|", "\\|") for cell in cells) + " |")
    print()
    print("\n".join(headers))
    return PASSED


def command_check(args) -> int:
    scenarios, problems = load_scenarios(REPO)
    for problem in problems:
        print(problem, file=sys.stderr)
    if problems:
        return FAILED
    print(f"ok: {len(scenarios)} skill evaluation scenarios are valid")
    return PASSED


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    commands = root.add_subparsers(dest="command", required=True)

    check = commands.add_parser("check", help="validate the scenario files (CI)")
    check.set_defaults(handler=command_check)

    run = commands.add_parser("run", help="run scenarios in fresh read-only sessions and check skill triggering")
    run.add_argument("--tool", required=True, choices=TOOLS)
    run.add_argument("--model", help="model for the sessions (default: the tool's configured model)")
    run.add_argument("--id", action="append", default=[], help="run only this scenario id (repeatable)")
    run.add_argument("--skill", action="append", default=[], help="run only the scenarios of this skill's file (repeatable)")
    run.add_argument("--jobs", type=int, default=1, help="parallel sessions (default 1)")
    run.add_argument("--budget", type=float, metavar="USD", help="Claude only: --max-budget-usd per session")
    run.add_argument("--out", type=Path, help="new or empty run folder (default Saved/AgentSkillEvals/<time>-<tool>[-<model>])")
    run.set_defaults(handler=command_run)

    grade = commands.add_parser("grade", help="score a run's answers against expected_behavior with a judge model")
    grade.add_argument("run", help="run folder printed by the run command")
    grade.add_argument("--judge", default="claude", choices=TOOLS, help="CLI that runs the judge (default claude)")
    grade.add_argument("--model", help="judge model (default: the tool's configured model)")
    grade.add_argument("--force", action="store_true", help="grade again even when a grade exists")
    grade.set_defaults(handler=command_grade)

    report = commands.add_parser("report", help="print a Markdown table of one or more runs")
    report.add_argument("runs", nargs="+", help="run folders")
    report.set_defaults(handler=command_report)

    for command in (run, grade):
        command.add_argument("--exe", help="CLI executable or path (default: the tool name on PATH)")
        command.add_argument("--timeout", type=float, default=TIMEOUT_MINUTES, metavar="MINUTES",
                             help=f"per session (default {TIMEOUT_MINUTES})")
    return root


def main(argv: list[str] | None = None) -> int:
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    args = parser().parse_args(argv)
    try:
        if getattr(args, "jobs", 1) < 1:
            raise SetupError("--jobs must be at least 1")
        return args.handler(args)
    except (SetupError, sync.SkillError) as error:
        print(f"SETUP ERROR: {error}", file=sys.stderr)
        return SETUP
    except KeyboardInterrupt:
        print("cancelled; running sessions were stopped", file=sys.stderr)
        return FAILED


if __name__ == "__main__":
    sys.exit(main())
