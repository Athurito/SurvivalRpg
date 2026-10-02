"""Offline regression tests for interrupted and incomplete evaluation runs."""
from __future__ import annotations

import argparse
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

import skill_evals as evals


class RunResultsTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.run = Path(self.temp.name)
        self.manifest(["first", "second"])

    def manifest(self, ids: list[str]) -> None:
        evals.write_json(self.run / "run.json", {
            "tool": "codex", "version": "test", "model": "default", "commit": "test", "scenarios": ids,
        })

    def result(self, sid: str, *, status: str = "passed") -> None:
        evals.write_json(self.run / f"{sid}.result.json", {
            "id": sid, "query": "Task", "expected_behavior": ["Does it."], "tools": [], "answer": "Done.",
            "status": status, "error": "session failed" if status == "error" else "",
            "loaded": ["alpha-skill"], "missing": [], "unexpected": [],
        })
        self.grade(sid)

    def grade(self, sid: str) -> None:
        evals.write_json(self.run / f"{sid}.grade.json", {
            "met": 1, "total": 1, "items": [{"expected": "Does it.", "met": True, "evidence": "Done."}],
        })

    def run_grade(self) -> tuple[int, str]:
        args = argparse.Namespace(run=str(self.run), judge="codex", exe=None, model=None, force=False, timeout=1)
        output = io.StringIO()
        with (contextlib.redirect_stdout(output), patch.object(evals, "executable", return_value="unused"),
              patch.object(evals.sync, "discover", return_value=["alpha-skill"]),
              patch.object(evals, "load_scenarios", return_value=([], [])),
              patch.object(evals, "judge", side_effect=AssertionError("Cached grades need no model call"))):
            code = evals.command_grade(args)
        return code, output.getvalue()

    def report(self) -> tuple[int, str]:
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            code = evals.command_report(argparse.Namespace(runs=[str(self.run)]))
        return code, output.getvalue()

    def test_missing_session_cannot_pass_grading(self) -> None:
        self.result("first")
        self.grade("second")  # A stale grade cannot stand in for a missing session result.
        code, output = self.run_grade()
        self.assertEqual(code, evals.FAILED)
        self.assertIn("SKIP second: missing result", output)
        self.assertIn("BEHAVIOR FAILED: 1/1 expectations met in 1 scenarios, 1 not graded", output)

    def test_report_keeps_scheduled_denominator_and_missing_row(self) -> None:
        self.result("first")
        self.grade("second")
        code, output = self.report()
        self.assertEqual(code, evals.FAILED)
        self.assertIn("| second | A | missing result |", output)
        self.assertIn("trigger 1/2, behavior 1/1; INCOMPLETE: 1 missing results", output)

    def test_run_without_any_results_is_incomplete(self) -> None:
        grade_code, grade_output = self.run_grade()
        report_code, report_output = self.report()
        self.assertEqual((grade_code, report_code), (evals.FAILED, evals.FAILED))
        self.assertIn("2 not graded", grade_output)
        self.assertIn("trigger 0/2", report_output)

    def test_complete_run_reuses_grades_and_passes(self) -> None:
        self.result("first")
        self.result("second")
        grade_code, grade_output = self.run_grade()
        report_code, report_output = self.report()
        self.assertEqual((grade_code, report_code), (evals.PASSED, evals.PASSED))
        self.assertIn("BEHAVIOR PASSED: 2/2", grade_output)
        self.assertIn("trigger 2/2, behavior 2/2", report_output)

    def test_completed_session_error_remains_a_grading_failure(self) -> None:
        self.result("first")
        self.result("second", status="error")
        code, output = self.run_grade()
        self.assertEqual(code, evals.FAILED)
        self.assertIn("SKIP second: session error", output)

    def test_regrading_uses_confirmed_loads_from_raw_transcript(self) -> None:
        self.manifest(["first"])
        self.result("first")
        events = [
            {"type": "item.completed", "item": {"id": "read", "type": "command_execution",
                "command": "Test-Path .agents/skills/alpha-skill/SKILL.md", "exit_code": 0,
                "aggregated_output": "True"}},
            {"type": "item.completed", "item": {"id": "answer", "type": "agent_message", "text": "Plan."}},
            {"type": "turn.completed"},
        ]
        (self.run / "first.jsonl").write_text("\n".join(json.dumps(event) for event in events), encoding="utf-8")
        args = argparse.Namespace(run=str(self.run), judge="codex", exe=None, model=None, force=True, timeout=1)
        grade = evals.read_json(self.run / "first.grade.json")
        with (contextlib.redirect_stdout(io.StringIO()), patch.object(evals, "executable", return_value="unused"),
              patch.object(evals.sync, "discover", return_value=["alpha-skill"]),
              patch.object(evals, "load_scenarios", return_value=([], [])),
              patch.object(evals, "judge", return_value=grade) as judge):
            self.assertEqual(evals.command_grade(args), evals.PASSED)
        self.assertIn("<skills_loaded>none</skills_loaded>", judge.call_args.args[3])
        self.assertEqual(evals.read_json(self.run / "first.result.json")["loaded"], ["alpha-skill"])

    def test_manifest_cannot_be_empty_duplicate_or_escape_run_folder(self) -> None:
        for ids in ([], ["first", "first"], ["../elsewhere"]):
            with self.subTest(ids=ids):
                self.manifest(ids)
                with self.assertRaises(evals.SetupError):
                    evals.run_results(self.run)

    def test_unscheduled_result_is_rejected(self) -> None:
        self.result("extra")
        with self.assertRaisesRegex(evals.SetupError, "results not scheduled"):
            evals.run_results(self.run)

    def test_result_cannot_impersonate_another_scenario(self) -> None:
        evals.write_json(self.run / "first.result.json", {"id": "second"})
        with self.assertRaisesRegex(evals.SetupError, "result id does not match"):
            evals.run_results(self.run)


if __name__ == "__main__":
    unittest.main()
