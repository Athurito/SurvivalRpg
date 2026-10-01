"""Build, test and cook SurvivalRpg from one entry point with a compact result.

Every command resolves the engine, verifies the checkout-local NetworkPrediction/Mover
overrides, keeps the full log (and the automation report) under Saved/ToolRuns and
prints a short summary. Exit codes: 0 passed, 1 failed, 2 setup or usage error.
See README.md next to this script.
"""
from __future__ import annotations

import argparse
from dataclasses import dataclass
from datetime import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import time

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
PROJECT = REPO / "SurvivalRpg.uproject"
AGENTS_MD = REPO / "AGENTS.md"
PREPARE = REPO / "Build/Patches/NetworkPrediction/prepare.py"
RUNS = REPO / "Saved/ToolRuns"
ENGINE_ENV = "SURVIVALRPG_UE_ROOT"
TEST_MAP = "/Engine/Maps/Entry"

PASSED, FAILED, SETUP = 0, 1, 2
MAX_ERRORS = 25      # error lines listed in a summary
MAX_WARNINGS = 5     # warning lines listed in a summary
MAX_ENTRIES = 3      # error entries listed per failed test
MAX_CHARS = 400      # characters kept per listed line

VERSION = re.compile(r"^\d+\.\d+$")
LOCAL_ENGINE = re.compile(r"^- Unreal Engine (?P<version>[\d.]+): `(?P<path>[^`]+)`", re.MULTILINE)
# MSVC, linker, UHT and UBT diagnostics; codes and keywords stay English in localized toolchains.
ERROR_LINE = re.compile(r"(?:^|[\s:)])(?:fatal )?error(?: [A-Z]+\d+)?\s*:", re.IGNORECASE)
WARNING_LINE = re.compile(r"(?:^|[\s:)])warning(?: [A-Z]+\d+)?\s*:", re.IGNORECASE)
BUILD_RESULT = re.compile(r"^Result: (?P<result>.+?)\s*$", re.MULTILINE)
COMMANDLET_RESULT = re.compile(r"LogInit: Display: (?P<result>Success|Failure) - (?P<errors>\d+) error\(s\), (?P<warnings>\d+) warning\(s\)")
SUMMARY_START = "LogInit: Display: Warning/Error Summary (Unique only)"
SUMMARY_ENTRY = re.compile(r"LogInit: Display: (?P<entry>.+?)\s*$")
NO_TESTS = re.compile(r"No automation tests matched '[^'\n]*'")
CRASH = re.compile(r"=== Critical error: ===|Fatal error!|Unhandled Exception: ")
ENSURE = re.compile(r"Ensure condition failed:")
NETWORK_PREDICTION = re.compile(r"InternalLoadLibrary: 'NetworkPrediction' \('(?P<path>[^']+)'\)")
LOG_PREFIX = re.compile(r"^\[[^\]]*\]\[[^\]]*\]")


class SetupError(RuntimeError):
    """The engine, project, overrides or arguments are not usable; Unreal never started."""


@dataclass
class Engine:
    """An Unreal Engine install root (the folder that contains Engine/) and where it came from."""
    root: Path
    version: str
    source: str

    @property
    def build_bat(self) -> Path:
        return self.root / "Engine/Build/BatchFiles/Build.bat"

    @property
    def editor_cmd(self) -> Path:
        return self.root / "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"


@dataclass
class Outcome:
    """A compact verdict; `lines` follow the headline, and paths name the full evidence."""
    passed: bool
    headline: str
    lines: list[str]


# --- Engine resolution -------------------------------------------------------------------------

def engine_association(project: Path = PROJECT) -> str:
    return json.loads(project.read_text(encoding="utf-8-sig"))["EngineAssociation"]


def engine_root(path: Path) -> Path | None:
    """Accept the install root or its Engine directory."""
    path = Path(path)
    roots = [path, path.parent] if path.name.lower() == "engine" else [path]
    return next((root for root in roots if (root / "Engine/Build/Build.version").is_file()), None)


def engine_version(root: Path) -> str:
    data = json.loads((root / "Engine/Build/Build.version").read_text(encoding="utf-8-sig"))
    return f"{data['MajorVersion']}.{data['MinorVersion']}.{data['PatchVersion']}"


def registry_engines(association: str) -> list[tuple[str, Path]]:
    """Launcher installs register by version under HKLM; source builds by association under HKCU."""
    try:
        import winreg
    except ImportError:
        return []
    found = []
    for hive, label, key, value in (
            (winreg.HKEY_LOCAL_MACHINE, "HKLM", rf"SOFTWARE\EpicGames\Unreal Engine\{association}", "InstalledDirectory"),
            (winreg.HKEY_CURRENT_USER, "HKCU", r"SOFTWARE\Epic Games\Unreal Engine\Builds", association)):
        try:
            with winreg.OpenKey(hive, key) as handle:
                found.append((f"registry {label}\\{key}", Path(winreg.QueryValueEx(handle, value)[0])))
        except OSError:
            continue
    return found


def agents_engines(association: str, agents_md: Path = AGENTS_MD) -> list[tuple[str, Path]]:
    """The "Local environment" entry in AGENTS.md, e.g. - Unreal Engine 5.8: `D:/Programme/UE_5.8`."""
    if not agents_md.is_file():
        return []
    return [(f"{agents_md.name} Local environment", Path(match["path"]))
            for match in LOCAL_ENGINE.finditer(agents_md.read_text(encoding="utf-8"))
            if match["version"] == association]


def resolve_engine(explicit: Path | None = None, environ=os.environ, association: str | None = None,
                   registry=registry_engines, agents=agents_engines) -> Engine:
    """--engine, then the environment variable, then the registry, then AGENTS.md.

    An explicit path or the variable is binding: a wrong value fails instead of falling through.
    """
    association = association or engine_association()
    if explicit:
        candidates, binding = [("--engine", Path(explicit))], True
    elif environ.get(ENGINE_ENV):
        candidates, binding = [(ENGINE_ENV, Path(environ[ENGINE_ENV]))], True
    else:
        candidates, binding = [*registry(association), *agents(association)], False
    tried = []
    for source, path in candidates:
        root = engine_root(path)
        if root is None:
            problem = f"{path} contains no Engine/Build/Build.version"
        else:
            version = engine_version(root)
            if not VERSION.match(association) or f"{version}.".startswith(f"{association}."):
                return Engine(root, version, source)
            problem = f"{root} is UE {version}, the project needs {association}"
        if binding:
            raise SetupError(f"{source}: {problem}")
        tried.append(f"{source}: {problem}")
    raise SetupError(
        f"No UE {association} installation found. Pass --engine, set {ENGINE_ENV} to the install root, "
        "or list it under \"Local environment\" in AGENTS.md." + "".join(f"\n  tried {entry}" for entry in tried))


# --- Shared helpers ----------------------------------------------------------------------------

def shown(path: Path) -> str:
    """Repository-relative forward-slash path for compact output."""
    try:
        return Path(path).resolve().relative_to(REPO).as_posix()
    except ValueError:
        return Path(path).as_posix()


REPO_PREFIX = re.compile("|".join(re.escape(str(REPO).replace("\\", sep) + sep) for sep in ("\\", "/")), re.IGNORECASE)


def compact(line: str) -> str:
    line = REPO_PREFIX.sub("", LOG_PREFIX.sub("", line.strip()))
    return line if len(line) <= MAX_CHARS else line[:MAX_CHARS - 3] + "..."


def unique(lines) -> list[str]:
    return list(dict.fromkeys(compact(line) for line in lines))


def listed(prefix: str, lines: list[str], limit: int) -> list[str]:
    result = [f"{prefix}{line}" for line in lines[:limit]]
    if len(lines) > limit:
        result.append(f"{prefix}... {len(lines) - limit} more in the log")
    return result


def plural(count: int, word: str) -> str:
    return f"{count} {word}{'' if count == 1 else 's'}"


def run_directory(kind: str, requested: Path | None) -> Path:
    """A fresh folder per run so a stale report can never be mistaken for a new one."""
    if requested:
        path = Path(requested).resolve()
        if path.exists() and any(path.iterdir()):
            raise SetupError(f"--out must be a new or empty directory: {path}")
    else:
        stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        path, index = RUNS / f"{stamp}-{kind}", 2
        while path.exists():
            path, index = RUNS / f"{stamp}-{kind}-{index}", index + 1
    path.mkdir(parents=True, exist_ok=True)
    return path


def verify_overrides(engine: Engine) -> None:
    """Builds and runs must use the staged NetworkPrediction/Mover overrides, never stale or missing ones."""
    result = subprocess.run([sys.executable, str(PREPARE), "verify"], cwd=REPO, capture_output=True, text=True)
    if result.returncode:
        detail = (result.stderr or result.stdout).strip().splitlines() or [f"exit code {result.returncode}"]
        raise SetupError(
            f"NetworkPrediction/Mover overrides do not verify: {detail[-1]}\n"
            "  If they are missing, close the editor and stage them:\n"
            f"  python Build/Patches/NetworkPrediction/prepare.py stage --engine {(engine.root / 'Engine').as_posix()}\n"
            "  Otherwise see Build/Patches/NetworkPrediction/README.md")


def kill_tree(process: subprocess.Popen) -> None:
    if os.name == "nt":
        subprocess.run(["taskkill", "/T", "/F", "/PID", str(process.pid)], capture_output=True)
    else:
        process.kill()
    process.wait()


def run_process(command: list, output: Path, timeout_minutes: float | None) -> tuple[int | None, bool]:
    """Run with stdout and stderr in `output`. Returns (exit code, timed out); kills the tree on timeout or Ctrl+C."""
    with output.open("wb") as stream:
        process = subprocess.Popen([str(part) for part in command], cwd=REPO, stdin=subprocess.DEVNULL,
                                   stdout=stream, stderr=subprocess.STDOUT,
                                   creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0))
        try:
            return process.wait(timeout=timeout_minutes * 60 if timeout_minutes else None), False
        except subprocess.TimeoutExpired:
            kill_tree(process)
            return None, True
        except KeyboardInterrupt:
            kill_tree(process)
            raise


def read_log(path: Path) -> str:
    return path.read_bytes().decode("utf-8", errors="replace") if path.is_file() else ""


def tail(text: str, count: int = 12) -> list[str]:
    return [compact(line) for line in text.strip().splitlines()[-count:]]


def finish(kind: str, outcome: Outcome, seconds: float, evidence: list[tuple[str, Path]]) -> int:
    state = "PASSED" if outcome.passed else "FAILED"
    print(f"{kind} {state} in {seconds:.0f}s: {outcome.headline}")
    for line in outcome.lines:
        print(line)
    for label, path in evidence:
        print(f"{label}: {shown(path)}")
    return PASSED if outcome.passed else FAILED


# --- Build -------------------------------------------------------------------------------------

def build_command(engine: Engine, target: str, configuration: str) -> list:
    return [engine.build_bat, target, "Win64", configuration, f"-Project={PROJECT}", "-WaitMutex", "-NoHotReloadFromIDE"]


def summarize_build(text: str, exit_code: int | None, timed_out: bool) -> Outcome:
    lines = text.splitlines()
    errors = unique(line for line in lines if ERROR_LINE.search(line))
    warnings = unique(line for line in lines if WARNING_LINE.search(line) and not ERROR_LINE.search(line))
    results = BUILD_RESULT.findall(text)
    result = results[-1] if results else "no result line"
    passed = not timed_out and exit_code == 0 and result == "Succeeded" and not errors
    headline = f"{result}, {plural(len(errors), 'error')}, {plural(len(warnings), 'warning')}"
    if timed_out:
        headline = f"timed out, {headline}"
    details = listed("  ", errors, MAX_ERRORS) + listed("  warning: ", warnings, MAX_WARNINGS)
    if "LiveCoding" in result or "Live Coding" in text:
        details.append("hint: Live Coding is active in a running editor; close it, or compile there with Ctrl+Alt+F11.")
    if any("LNK1104" in error for error in errors):
        details.append("hint: a running editor or game probably locks the binaries; close it and build again.")
    if not passed and not errors:
        details += [f"  {line}" for line in tail(text)]
    return Outcome(passed, headline, details)


def command_build(args, engine: Engine) -> int:
    verify_overrides(engine)
    run = run_directory("build", args.out)
    log = run / "build.log"
    print(f"build {args.target} Win64 {args.config} with UE {engine.version}; log: {shown(log)}", flush=True)
    started = time.monotonic()
    exit_code, timed_out = run_process(build_command(engine, args.target, args.config), log, args.timeout)
    outcome = summarize_build(read_log(log), exit_code, timed_out)
    return finish("BUILD", outcome, time.monotonic() - started, [("log", log)])


# --- Automation tests --------------------------------------------------------------------------

def exec_commands(filters: list[str], pre: list[str]) -> str:
    """-ExecCmds splits on commas, so no part may contain one."""
    for part in (*filters, *pre):
        if "," in part:
            raise SetupError(f"Commas are not allowed in filters or --exec commands: {part}")
        if '"' in part:
            raise SetupError(f"Quotes are not allowed in filters or --exec commands: {part}")
    return ",".join([*pre, f"Automation RunTests {'+'.join(filters)}; Quit"])


def test_command(engine: Engine, args, run: Path) -> list:
    command = [engine.editor_cmd, PROJECT, args.map,
               "-unattended", "-nop4", "-nosteam", "-nosplash", "-nosound", "-NoSaveConfig",
               "-NullRHI" if args.null_rhi else "-RenderOffscreen",
               f"-ExecCmds={exec_commands(args.filters, args.exec)}",
               "-TestExit=Automation Test Queue Empty",
               f"-ReportExportPath={run / 'report'}",
               f"-abslog={run / 'editor.log'}"]
    if args.log_cmds:
        command.append(f"-LogCmds={', '.join(args.log_cmds)}")
    return command + args.ue_arg


def filter_matches(test_path: str, test_filter: str) -> bool | None:
    """RunTests semantics: substring, ^start, end$, StartsWith:; None for groups that cannot be checked here."""
    path, text = test_path.lower(), test_filter.strip().lower()
    if text.startswith("group:"):
        return None
    if text.startswith("startswith:"):
        prefix = text[len("startswith:"):].strip()
        return path.startswith(prefix if prefix.endswith(".") else prefix + ".")
    start, end = text.startswith("^"), text.endswith("$")
    text = text[1 if start else 0:len(text) - 1 if end else len(text)]
    if start and end:
        return path == text
    return path.startswith(text) if start else path.endswith(text) if end else text in path


def load_report(path: Path) -> dict | None:
    if not path.is_file():
        return None
    return json.loads(path.read_bytes().decode("utf-8-sig"))


def entry_line(entry: dict) -> str:
    message = entry.get("event", {}).get("message", "")
    source = entry.get("filename") or ""
    location = f" ({compact(source)}:{entry.get('lineNumber')})" if source else ""
    return compact(message + location)


def summarize_tests(report: dict | None, log_text: str, filters: list[str], expected: int | None,
                    exit_code: int | None, timed_out: bool) -> Outcome:
    lines: list[str] = []
    crashed = bool(CRASH.search(log_text))
    if report is None:
        reason = "timed out" if timed_out else "crashed" if crashed else "no report"
        unmatched = NO_TESTS.search(log_text)
        if unmatched:
            reason = unmatched.group(0)
        lines += [f"  {line}" for line in tail(log_text)] if not unmatched else []
        return Outcome(False, f"{reason} (editor exit code {exit_code})", lines)

    tests = report.get("tests", [])
    failed = [test for test in tests if test.get("state") == "Fail"]
    not_run = [test for test in tests if test.get("state") not in ("Success", "Fail")]
    succeeded = [test for test in tests if test.get("state") == "Success"]
    warned = sum(1 for test in succeeded if test.get("warnings"))
    headline = (f"{len(tests)} run: {len(succeeded)} passed ({warned} with warnings), "
                f"{len(failed)} failed, {len(not_run)} not run")
    for test in failed:
        lines.append(f"FAIL {test.get('fullTestPath')}")
        errors = [entry for entry in test.get("entries", []) if entry.get("event", {}).get("type") == "Error"]
        lines += listed("  ", [entry_line(entry) for entry in errors], MAX_ENTRIES)
    lines += [f"NOT RUN {test.get('fullTestPath')} ({test.get('state')})" for test in not_run]

    paths = [test.get("fullTestPath", "") for test in tests]
    unmatched = [test_filter for test_filter in filters
                 if all(filter_matches(path, test_filter) is False for path in paths)]
    lines += [f"no test matched filter: {test_filter}" for test_filter in unmatched]
    if expected is not None and len(tests) != expected:
        lines.append(f"expected {expected} tests, the report has {len(tests)}")
    ensures = len(ENSURE.findall(log_text))
    if ensures:
        lines.append(f"{plural(ensures, 'ensure')} in the log")
    module = NETWORK_PREDICTION.search(log_text)
    if module and not module["path"].replace("\\", "/").lower().startswith(REPO.as_posix().lower() + "/"):
        lines.append(f"warning: NetworkPrediction loaded from {module['path']}, not the project override")
    if crashed:
        lines.append("the editor crashed; see the log")
    if timed_out:
        lines.append("timed out; the report may be incomplete")
    passed = bool(tests) and not failed and not not_run and not unmatched and not crashed and not timed_out \
        and (expected is None or len(tests) == expected)
    return Outcome(passed, headline, lines)


def command_test(args, engine: Engine) -> int:
    verify_overrides(engine)
    run = run_directory("test", args.out)
    command = test_command(engine, args, run)
    log = run / "editor.log"
    print(f"test {' '.join(args.filters)} with UE {engine.version}; log: {shown(log)}", flush=True)
    started = time.monotonic()
    exit_code, timed_out = run_process(command, run / "console.log", args.timeout)
    report_path = run / "report/index.json"
    outcome = summarize_tests(load_report(report_path), read_log(log) or read_log(run / "console.log"),
                              args.filters, args.expect_tests, exit_code, timed_out)
    evidence = [("report", report_path)] if report_path.is_file() else []
    return finish("TEST", outcome, time.monotonic() - started, evidence + [("log", log)])


# --- Cook --------------------------------------------------------------------------------------

def package_path(map_file: Path, repo: Path = REPO) -> str:
    """Content/A/B.umap -> /Game/A/B; Plugins/.../P/Content/A/B.umap -> /P/A/B."""
    relative = map_file.relative_to(repo).with_suffix("")
    parts = relative.parts
    if parts[0] == "Content":
        return "/Game/" + "/".join(parts[1:])
    content = parts.index("Content")
    return f"/{parts[content - 1]}/" + "/".join(parts[content + 1:])


def resolve_map(name: str, repo: Path = REPO) -> str:
    """Accept a long package path or a unique map name and fail early on unknown maps."""
    if name.startswith("/"):
        mount, _, rest = name.strip("/").partition("/")
        roots = [repo / "Content"] if mount == "Game" else [
            plugin.parent / "Content" for plugin in (repo / "Plugins").rglob(f"{mount}.uplugin")]
        if not rest or not any((root / f"{rest}.umap").is_file() for root in roots):
            raise SetupError(f"Map not found: {name}")
        return name
    matches = sorted({package_path(path, repo) for folder in ("Content", "Plugins")
                      for path in (repo / folder).rglob(f"{name}.umap")})
    if len(matches) != 1:
        raise SetupError(f"Map name {name} matches {len(matches)} maps" + "".join(f"\n  {match}" for match in matches))
    return matches[0]


def cook_command(engine: Engine, maps: list[str], platform: str, log: Path, extra: list[str]) -> list:
    return [engine.editor_cmd, PROJECT, "-run=cook", f"-TargetPlatform={platform}", f"-Map={'+'.join(maps)}",
            "-unattended", "-nop4", "-nosteam", "-NoSaveConfig", f"-abslog={log}", *extra]


def summarize_cook(text: str, exit_code: int | None, timed_out: bool) -> Outcome:
    results = list(COMMANDLET_RESULT.finditer(text))
    if not results:
        reason = "timed out" if timed_out else "no commandlet summary"
        return Outcome(False, f"{reason} (exit code {exit_code})", [f"  {line}" for line in tail(text)])
    result = results[-1]
    entries, collecting = [], False
    for line in text.splitlines():
        if SUMMARY_START in line:
            entries, collecting = [], True
        elif collecting:
            match = SUMMARY_ENTRY.search(line)
            if not match or COMMANDLET_RESULT.search(line):
                collecting = False
            elif not match["entry"].startswith("---"):
                entries.append(match["entry"])
    errors = unique(entry for entry in entries if ERROR_LINE.search(entry))
    warnings = unique(entry for entry in entries if WARNING_LINE.search(entry) and not ERROR_LINE.search(entry))
    error_count, warning_count = int(result["errors"]), int(result["warnings"])
    passed = not timed_out and exit_code == 0 and result["result"] == "Success" and error_count == 0
    headline = f"{result['result']}, {plural(error_count, 'error')}, {plural(warning_count, 'warning')}"
    return Outcome(passed, headline, listed("  ", errors, MAX_ERRORS) + listed("  warning: ", warnings, MAX_WARNINGS))


def command_cook(args, engine: Engine) -> int:
    maps = [resolve_map(name) for name in args.maps]
    verify_overrides(engine)
    run = run_directory("cook", args.out)
    log = run / "cook.log"
    print(f"cook {' '.join(maps)} for {args.platform} with UE {engine.version}; log: {shown(log)}", flush=True)
    started = time.monotonic()
    exit_code, timed_out = run_process(cook_command(engine, maps, args.platform, log, args.ue_arg),
                                       run / "console.log", args.timeout)
    outcome = summarize_cook(read_log(log) or read_log(run / "console.log"), exit_code, timed_out)
    return finish("COOK", outcome, time.monotonic() - started, [("log", log)])


# --- Command line ------------------------------------------------------------------------------

def command_engine(args, engine: Engine) -> int:
    """Print only the Engine directory on stdout so other tools can consume it."""
    print((engine.root / "Engine").as_posix())
    print(f"UE {engine.version} from {engine.source}", file=sys.stderr)
    return PASSED


def parser() -> argparse.ArgumentParser:
    root = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    root.add_argument("--engine", type=Path, help=f"Engine install root; overrides {ENGINE_ENV}, registry and AGENTS.md")
    commands = root.add_subparsers(dest="command", required=True)

    commands.add_parser("engine", help="print the resolved Engine directory")

    build = commands.add_parser("build", help="compile a target with UnrealBuildTool")
    build.add_argument("--target", default="SurvivalRpgEditor", choices=("SurvivalRpgEditor", "SurvivalRpg"))
    build.add_argument("--config", default="Development", choices=("Development", "DebugGame", "Shipping"))

    test = commands.add_parser("test", help="run automation tests in a separate editor process")
    test.add_argument("filters", nargs="+", help="RunTests filters, joined with +: substring, ^start, end$ or StartsWith:")
    test.add_argument("--exec", action="append", default=[], metavar="CMD",
                      help="console command run before the tests, e.g. \"t.MaxFPS 30\"; repeatable")
    test.add_argument("--log-cmds", action="append", default=[], metavar="CATEGORY_LEVEL",
                      help="log verbosity, e.g. \"LogRpgAbilitySystem Verbose\"; repeatable")
    test.add_argument("--map", default=TEST_MAP, help=f"startup map (default {TEST_MAP})")
    test.add_argument("--null-rhi", action="store_true",
                      help="no rendering; faster, but never for PIE network tests, which need rendered PIE")
    test.add_argument("--expect-tests", type=int, metavar="N", help="fail unless the report holds exactly N tests")

    cook = commands.add_parser("cook", help="cook maps with the cook commandlet")
    cook.add_argument("maps", nargs="+", help="/Game/... package path or unique map name")
    cook.add_argument("--platform", default="Windows", help="cook target platform (default Windows)")

    for command, timeout in ((build, None), (test, 60.0), (cook, None)):
        # Also accepted after the command; SUPPRESS keeps the value given before it.
        command.add_argument("--engine", type=Path, default=argparse.SUPPRESS, help=argparse.SUPPRESS)
        command.add_argument("--ue-arg", action="append", default=[], metavar="ARG",
                             help="extra Unreal argument, written as --ue-arg=-Switch=Value; repeatable")
        command.add_argument("--timeout", type=float, default=timeout, metavar="MINUTES",
                             help=f"kill the run after this many minutes (default {timeout or 'none'})")
        command.add_argument("--out", type=Path, help="new or empty run folder (default Saved/ToolRuns/<time>-<command>)")
    build.set_defaults(run=command_build)
    test.set_defaults(run=command_test)
    cook.set_defaults(run=command_cook)
    commands.choices["engine"].set_defaults(run=command_engine)
    return root


def main(argv: list[str] | None = None) -> int:
    # Logs are UTF-8 with localized compiler text; pipes would otherwise use the ANSI code page.
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")
    args = parser().parse_args(argv)
    try:
        engine = resolve_engine(args.engine)
        if args.command != "engine":
            tool = engine.build_bat if args.command == "build" else engine.editor_cmd
            if not tool.is_file():
                raise SetupError(f"Missing {tool}; Windows with an installed UE {engine.version} is required")
        return args.run(args, engine)
    except SetupError as error:
        print(f"SETUP ERROR: {error}", file=sys.stderr)
        return SETUP
    except KeyboardInterrupt:
        print("cancelled; any started Unreal process tree was stopped", file=sys.stderr)
        return FAILED


if __name__ == "__main__":
    sys.exit(main())
