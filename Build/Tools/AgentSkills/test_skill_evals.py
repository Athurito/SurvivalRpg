"""Offline tests for skill_evals.py: scenario validation, transcript parsing, and judge verdicts."""

from __future__ import annotations

import argparse
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

import skill_evals


NAMES = ["alpha-skill", "beta-skill"]


def scenario(sid: str, **fields) -> dict:
    data = {"id": sid, "skills": ["alpha-skill"], "query": "Do the thing.", "expected_behavior": ["Does it."]}
    data.update(fields)
    return data


def lines(*events: dict) -> list[str]:
    return [json.dumps(event) for event in events] + ["not json"]


def tool_use(name: str, arguments: dict) -> dict:
    return {"type": "assistant", "message": {"content": [{"type": "tool_use", "name": name, "input": arguments}]}}


def command(item_id: str, text: str, output: str = "", kind: str = "item.completed") -> dict:
    return {"type": kind, "item": {"id": item_id, "type": "command_execution", "command": text,
                                   "aggregated_output": output, "exit_code": 0}}


class ScenarioTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        for name in NAMES:
            self.write(f".agents/skills/{name}/SKILL.md",
                       f"---\nname: {name}\ndescription: Guides {name} work. Use when testing evals.\n---\n\n# Skill\n")
            self.write_evals(name, [scenario(f"{name}-{index}", skills=[name]) for index in range(3)])
        self.write("Source/Demo/Thing.h", "class URpgThing {};\n")
        self.write("docs/guide.md", "# Guide\n")

    def tearDown(self) -> None:
        self.temp.cleanup()

    def write(self, relative: str, text: str) -> None:
        path = self.root / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8", newline="\n")

    def write_evals(self, name: str, data) -> None:
        self.write(f"{skill_evals.EVALS}/{name}.json", json.dumps(data))

    def problems(self) -> list[str]:
        return skill_evals.load_scenarios(self.root)[1]

    def test_valid_scenarios_load_with_their_suite(self) -> None:
        scenarios, problems = skill_evals.load_scenarios(self.root)
        self.assertEqual(problems, [])
        self.assertEqual(len(scenarios), 6)
        self.assertEqual({item["suite"] for item in scenarios}, set(NAMES))

    def test_each_skill_needs_three_scenarios(self) -> None:
        self.write_evals("beta-skill", [scenario("beta-only", skills=["beta-skill"])])
        self.assertIn(f"{skill_evals.EVALS}/beta-skill.json: 1 valid scenarios, at least 3 required", self.problems())
        (self.root / skill_evals.EVALS / "beta-skill.json").unlink()
        self.assertIn(f"{skill_evals.EVALS}/beta-skill.json: 0 valid scenarios, at least 3 required", self.problems())

    def test_field_and_skill_problems_are_reported(self) -> None:
        self.write_evals("alpha-skill", [
            scenario("alpha-0", typo=True),
            scenario("Alpha One", skills=["gamma-skill"]),
            scenario("alpha-2", skills=["beta-skill"]),
            scenario("alpha-3", excluded_skills=["alpha-skill"]),
            scenario("alpha-4", expected_behavior=[]),
            {"skills": ["alpha-skill"]},
        ])
        problems = "\n".join(self.problems())
        self.assertIn("#1: unknown field 'typo'", problems)
        self.assertIn("#2: id must be lowercase words joined by hyphens", problems)
        self.assertIn("#2: unknown skill 'gamma-skill'", problems)
        self.assertIn("#3: name 'alpha-skill' in skills or excluded_skills", problems)
        self.assertIn("#4: 'alpha-skill' is both expected and excluded", problems)
        self.assertIn("#5: expected_behavior must list at least one behavior", problems)
        self.assertIn("#6: missing field 'id'", problems)
        self.assertNotIn("#6: id must be", problems)

    def test_excluded_suite_skill_counts_as_named(self) -> None:
        self.write_evals("alpha-skill", [scenario(f"alpha-{index}", skills=["beta-skill"], excluded_skills=["alpha-skill"])
                                         for index in range(3)])
        self.assertEqual(self.problems(), [])

    def test_references_ids_files_and_json_are_checked(self) -> None:
        self.write_evals("alpha-skill", [
            scenario("alpha-0", expected_behavior=["Reads `docs/guide.md` and uses `URpgThing`."], files=["docs/guide.md"]),
            scenario("alpha-1", expected_behavior=["Reads `docs/missing.md` and `URpgGhost`; ignores `URpgThing_*`."]),
            scenario("alpha-2", files=["docs/absent.md"]),
            scenario("beta-skill-0"),
        ])
        self.write(f"{skill_evals.EVALS}/gamma-skill.json", "[]")
        self.write(f"{skill_evals.EVALS}/beta-skill.json", "{broken")
        problems = "\n".join(self.problems())
        self.assertNotIn("alpha-skill.json #1", problems)
        self.assertIn("#2: missing path docs/missing.md", problems)
        self.assertIn("#2: identifier URpgGhost not found in Source/", problems)
        self.assertNotIn("URpgThing_", problems)
        self.assertIn("#3: missing file docs/absent.md", problems)
        self.assertIn("gamma-skill.json: no canonical skill named 'gamma-skill'", problems)
        self.assertIn("beta-skill.json: invalid JSON", problems)

    def test_duplicate_ids_are_reported(self) -> None:
        self.write_evals("beta-skill", [scenario("alpha-skill-0", skills=["beta-skill"])]
                         + [scenario(f"beta-{index}", skills=["beta-skill"]) for index in range(3)])
        self.assertIn(f"{skill_evals.EVALS}: duplicate id alpha-skill-0", self.problems())

    def test_select_filters_by_id_or_suite_and_rejects_unknown_names(self) -> None:
        scenarios, _ = skill_evals.load_scenarios(self.root)
        chosen = skill_evals.select(scenarios, ["alpha-skill-0"], ["beta-skill"])
        self.assertEqual([item["id"] for item in chosen], ["alpha-skill-0", "beta-skill-0", "beta-skill-1", "beta-skill-2"])
        self.assertEqual(len(skill_evals.select(scenarios, [], [])), 6)
        with self.assertRaises(skill_evals.SetupError):
            skill_evals.select(scenarios, ["nope"], [])


class TranscriptTests(unittest.TestCase):
    def test_claude_skill_calls_and_skill_file_reads_load_skills(self) -> None:
        transcript = skill_evals.parse_claude(lines(
            {"type": "system", "subtype": "init", "skills": NAMES + ["other"]},
            tool_use("Skill", {"skill": "alpha-skill"}),
            tool_use("Read", {"file_path": "D:\\Repo\\.claude\\skills\\beta-skill\\SKILL.md"}),
            tool_use("Read", {"file_path": "D:/Repo/.claude/skills/beta-skill/references/notes.md"}),
            tool_use("Grep", {"pattern": "SKILL", "path": ".agents/skills"}),
            {"type": "user", "message": {"content": [{"type": "tool_result", "content": ".agents/skills/alpha-skill/SKILL.md"}]}},
            tool_use("ExitPlanMode", {"plan": "1. Plan the thing."}),
            {"type": "result", "subtype": "success", "is_error": False, "result": "Plan ready.", "total_cost_usd": 0.5},
        ), NAMES)
        self.assertEqual(transcript.loaded, ["alpha-skill", "beta-skill"])
        self.assertEqual(transcript.skill_files, ["beta-skill/references/notes.md"])
        self.assertEqual(transcript.available, NAMES + ["other"])
        self.assertEqual(transcript.answer, "1. Plan the thing.\n\nPlan ready.")
        self.assertEqual(transcript.usage, {"total_cost_usd": 0.5})
        self.assertEqual(transcript.error, "")
        self.assertEqual(len(transcript.tools), 5)

    def test_claude_output_from_tool_results_does_not_load_skills(self) -> None:
        transcript = skill_evals.parse_claude(lines(
            tool_use("Glob", {"pattern": "**/SKILL.md"}),
            {"type": "user", "message": {"content": [{"type": "tool_result", "content": ".agents/skills/beta-skill/SKILL.md"}]}},
            {"type": "assistant", "message": {"content": [{"type": "text", "text": "Final answer."}]}},
            {"type": "result", "subtype": "success", "result": ""},
        ), NAMES)
        self.assertEqual(transcript.loaded, [])
        self.assertEqual(transcript.answer, "Final answer.")

    def test_claude_errors_are_reported(self) -> None:
        failed = skill_evals.parse_claude(lines({"type": "result", "subtype": "success", "is_error": True,
                                                 "result": "Not logged in"}), NAMES)
        self.assertEqual(failed.error, "Not logged in")
        limited = skill_evals.parse_claude(lines({"type": "result", "subtype": "error_max_budget_usd", "is_error": True,
                                                  "result": "Budget"}), NAMES)
        self.assertEqual(limited.error, "error_max_budget_usd: Budget")
        self.assertEqual(skill_evals.parse_claude(lines(), NAMES).error, "no result event")

    def test_codex_skill_reads_load_skills_without_counting_output(self) -> None:
        transcript = skill_evals.parse_codex(lines(
            {"type": "thread.started", "thread_id": "t"},
            command("1", "Get-Content .agents\\skills\\alpha-skill\\SKILL.md", kind="item.started"),
            command("1", "Get-Content .agents\\skills\\alpha-skill\\SKILL.md"),
            command("2", "rg -l SKILL .agents/skills", output=".agents/skills/beta-skill/SKILL.md"),
            command("3", "sed -n '1,80p' .agents/skills/alpha-skill/references/notes.md"),
            {"type": "item.completed", "item": {"id": "4", "type": "agent_message", "text": "Looking around."}},
            {"type": "item.completed", "item": {"id": "5", "type": "reasoning", "text": "skills/beta-skill/SKILL.md"}},
            {"type": "item.completed", "item": {"id": "6", "type": "agent_message", "text": "The plan."}},
            {"type": "turn.completed", "usage": {"input_tokens": 10, "output_tokens": 5}},
        ), NAMES)
        self.assertEqual(transcript.loaded, ["alpha-skill"])
        self.assertEqual(transcript.skill_files, ["alpha-skill/references/notes.md"])
        self.assertEqual(len(transcript.tools), 3)
        self.assertEqual(transcript.answer, "The plan.")
        self.assertEqual(transcript.usage, {"input_tokens": 10, "output_tokens": 5})
        self.assertEqual(transcript.error, "")

    def test_codex_failures_are_reported(self) -> None:
        failed = skill_evals.parse_codex(lines({"type": "turn.failed", "error": {"message": "quota"}}), NAMES)
        self.assertEqual(failed.error, "quota")
        self.assertEqual(skill_evals.parse_codex(lines({"type": "error", "message": "boom"}), NAMES).error, "boom")
        self.assertEqual(skill_evals.parse_codex(lines(), NAMES).error, "no agent message")

    def test_trigger_reports_missing_and_unexpected_skills(self) -> None:
        item = scenario("x", skills=["alpha-skill"], excluded_skills=["beta-skill"])
        self.assertEqual(skill_evals.trigger(item, ["alpha-skill"]), ([], []))
        self.assertEqual(skill_evals.trigger(item, ["beta-skill"]), (["alpha-skill"], ["beta-skill"]))


class ReportTests(unittest.TestCase):
    def test_report_lists_runs_side_by_side_with_totals(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            runs = []
            for name, loaded, met in (("before", [], 1), ("after", ["alpha-skill"], 2)):
                run_dir = Path(folder) / name
                run_dir.mkdir()
                skill_evals.write_json(run_dir / "run.json", {"tool": "codex", "version": "codex-cli 1.0", "model": "default",
                                                              "commit": "abc", "started": "now"})
                result = {"id": "x", "status": "passed" if loaded else "failed", "error": "", "loaded": loaded,
                          "missing": [] if loaded else ["alpha-skill"], "unexpected": []}
                skill_evals.write_json(run_dir / "x.result.json", result)
                skill_evals.write_json(run_dir / "x.grade.json", {"met": met, "total": 2, "items": [
                    {"expected": "One.", "met": True, "evidence": ""}, {"expected": "Two.", "met": met == 2, "evidence": ""}]})
                runs.append(str(run_dir))
            output = io.StringIO()
            with contextlib.redirect_stdout(output):
                self.assertEqual(skill_evals.command_report(argparse.Namespace(runs=runs)), skill_evals.PASSED)
        text = output.getvalue()
        self.assertIn("| x | A | missing alpha-skill | none | 1/2 | 2 |\n| x | B | pass | alpha-skill | 2/2 | – |", text)
        self.assertIn("- A: `before`, codex codex-cli 1.0, model default, commit abc; trigger 0/1, behavior 1/2", text)
        self.assertIn("- B: `after`", text)


class VerdictTests(unittest.TestCase):
    EXPECTED = ["First behavior.", "Second behavior."]

    def test_claude_result_with_fenced_json(self) -> None:
        verdict = {"items": [{"index": 1, "met": True, "evidence": "quoted"}, {"index": "2", "met": False, "evidence": "absent"}],
                   "summary": "Half."}
        output = json.dumps({"type": "result", "is_error": False, "result": f"```json\n{json.dumps(verdict)}\n```"})
        grade = skill_evals.parse_verdict(output, self.EXPECTED)
        self.assertEqual((grade["met"], grade["total"], grade["summary"]), (1, 2, "Half."))
        self.assertEqual(grade["items"][1], {"expected": "Second behavior.", "met": False, "evidence": "absent"})

    def test_codex_verdict_and_errors(self) -> None:
        verdict = {"items": [{"index": 1, "met": True, "evidence": "a"}, {"index": 2, "met": True, "evidence": "b"}],
                   "summary": "All."}
        self.assertEqual(skill_evals.parse_verdict(json.dumps(verdict), self.EXPECTED)["met"], 2)
        with self.assertRaisesRegex(ValueError, "no verdict for item 2"):
            skill_evals.parse_verdict(json.dumps({"items": verdict["items"][:1], "summary": ""}), self.EXPECTED)
        with self.assertRaisesRegex(ValueError, "Not logged in"):
            skill_evals.parse_verdict(json.dumps({"type": "result", "is_error": True, "result": "Not logged in"}), self.EXPECTED)
        with self.assertRaises(ValueError):
            skill_evals.parse_verdict("no json here", self.EXPECTED)

    def test_judge_prompt_lists_expectations_and_limits_tool_calls(self) -> None:
        result = {"query": "Task", "expected_behavior": ["Stored behavior."], "loaded": [], "answer": "Answer",
                  "tools": [f"Read file{index}" for index in range(skill_evals.MAX_TOOL_CALLS + 2)]}
        prompt = skill_evals.judge_prompt(result, self.EXPECTED)
        self.assertIn("1. First behavior.\n2. Second behavior.", prompt)
        self.assertNotIn("Stored behavior.", prompt)
        self.assertIn("<skills_loaded>none</skills_loaded>", prompt)
        self.assertIn("... 2 more calls", prompt)

    def test_rubric_follows_current_scenario_only_while_the_query_is_unchanged(self) -> None:
        scenarios = {"x": {"query": "Task", "expected_behavior": ["Current."]}}
        self.assertEqual(skill_evals.rubric({"id": "x", "query": "Task", "expected_behavior": ["Old."]}, scenarios),
                         ["Current."])
        self.assertEqual(skill_evals.rubric({"id": "x", "query": "Older task", "expected_behavior": ["Old."]}, scenarios),
                         ["Old."])
        self.assertEqual(skill_evals.rubric({"id": "gone", "query": "Task", "expected_behavior": ["Old."]}, scenarios),
                         ["Old."])


if __name__ == "__main__":
    unittest.main()
