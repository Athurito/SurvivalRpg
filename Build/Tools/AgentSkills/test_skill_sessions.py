"""Offline session lifecycle tests using bounded local Python processes."""

from __future__ import annotations

import argparse
from concurrent.futures import CancelledError
import contextlib
import io
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest import mock

import skill_evals


class SessionTests(unittest.TestCase):
    def test_taskkill_failure_warns_and_terminates_parent(self) -> None:
        denied = skill_evals.subprocess.CompletedProcess([], 1, stderr=b"Access denied")
        for failure in (denied, OSError("taskkill unavailable")):
            with self.subTest(failure=failure):
                process = mock.Mock(pid=123)
                output = io.StringIO()
                with (mock.patch.object(skill_evals.os, "name", "nt"),
                      mock.patch.object(skill_evals.subprocess, "run", side_effect=[failure]),
                      contextlib.redirect_stderr(output)):
                    skill_evals.kill_tree(process)
                process.kill.assert_called_once_with()
                process.wait.assert_called_once_with(timeout=5)
                self.assertIn("Descendants may still be running", output.getvalue())
                self.assertIn("Access denied" if failure is denied else "taskkill unavailable", output.getvalue())

    def test_cancellation_continues_after_one_parent_cannot_be_stopped(self) -> None:
        processes = [mock.Mock(pid=123), mock.Mock(pid=456)]
        processes[0].kill.side_effect = PermissionError("Parent termination denied")
        cancellation = skill_evals.SessionCancellation()
        with mock.patch.object(skill_evals.subprocess, "Popen", side_effect=processes):
            for _ in processes:
                cancellation.start(["local-test"])
        output = io.StringIO()
        denied = skill_evals.subprocess.CompletedProcess([], 1, stderr=b"Access denied")
        with (mock.patch.object(skill_evals.os, "name", "nt"),
              mock.patch.object(skill_evals.subprocess, "run", return_value=denied),
              contextlib.redirect_stderr(output)):
            cancellation.cancel()
        for process in processes:
            process.kill.assert_called_once_with()
        processes[1].wait.assert_called_once_with(timeout=5)
        self.assertIn("process cleanup failed: Parent termination denied", output.getvalue())

    def test_cancelled_queue_cannot_launch_a_process(self) -> None:
        cancellation = skill_evals.SessionCancellation()
        cancellation.cancel()
        with mock.patch.object(skill_evals.subprocess, "Popen") as launch:
            with self.assertRaises(CancelledError):
                cancellation.start([sys.executable, "-c", "pass"])
        launch.assert_not_called()

    def test_standalone_session_preserves_prompt_and_output(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            code, timed_out, _ = skill_evals.run_session(
                [sys.executable, "-c", "import sys; sys.stdout.buffer.write(sys.stdin.buffer.read())"],
                "Unicode prompt: ä", root, root / "stdout", root / "stderr", 0.1)
            self.assertEqual(code, 0)
            self.assertFalse(timed_out)
            self.assertEqual((root / "stdout").read_bytes(), "Unicode prompt: ä".encode("utf-8"))

    def test_timeout_stops_the_process(self) -> None:
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            started = time.monotonic()
            code, timed_out, _ = skill_evals.run_session(
                [sys.executable, "-c", "import time; time.sleep(10)"], "", root,
                root / "stdout", root / "stderr", 0.001)
            self.assertIsNone(code)
            self.assertTrue(timed_out)
            self.assertLess(time.monotonic() - started, 5)

    def test_interrupt_stops_active_trees_and_queued_scenarios(self) -> None:
        # Each fake CLI starts a child with an observable heartbeat. Both have a
        # fixed lifetime so a failed lifecycle test cannot leave a lasting worker.
        child = (
            "import pathlib, sys, time; path = pathlib.Path(sys.argv[1]); "
            "deadline = time.monotonic() + 10\n"
            "while time.monotonic() < deadline:\n"
            " with path.open('ab') as stream: stream.write(b'x')\n"
            " time.sleep(0.03)\n"
        )
        script = (
            "import pathlib, subprocess, sys, time\n"
            "root = pathlib.Path(sys.argv[1]); sid = sys.stdin.read().splitlines()[0]\n"
            f"child = subprocess.Popen([sys.executable, '-c', {child!r}, str(root / (sid + '.heartbeat'))])\n"
            "print(child.pid, flush=True)\n"
            "(root / (sid + '.started')).write_text(str(child.pid))\n"
            "time.sleep(10)\n"
        )
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            scenarios = [{"id": f"test-{index}", "query": f"test-{index}", "suite": "test", "skills": [],
                          "expected_behavior": ["Answers."]} for index in range(8)]
            args = argparse.Namespace(tool="codex", exe=None, model=None, budget=None,
                                      out=root / "run", id=[], skill=[], timeout=0.5, jobs=2)

            def interrupt_when_running(*unused, **options):
                self.assertLessEqual(options["timeout"], 0.2)
                deadline = time.monotonic() + 5
                while time.monotonic() < deadline:
                    heartbeats = list(root.glob("*.heartbeat"))
                    if len(heartbeats) == 2 and all(path.stat().st_size >= 2 for path in heartbeats):
                        raise KeyboardInterrupt()
                    time.sleep(0.02)
                self.fail("Two local session trees did not start within five seconds")

            with contextlib.ExitStack() as stack:
                stack.enter_context(mock.patch.object(skill_evals, "REPO", root))
                stack.enter_context(mock.patch.object(skill_evals, "load_scenarios", return_value=(scenarios, [])))
                stack.enter_context(mock.patch.object(skill_evals.sync, "discover", return_value=[]))
                stack.enter_context(mock.patch.object(skill_evals, "executable", return_value=sys.executable))
                stack.enter_context(mock.patch.object(skill_evals, "session_command",
                                                      return_value=[sys.executable, "-c", script, str(root)]))
                stack.enter_context(mock.patch.object(skill_evals, "output_of", return_value="local test"))
                stack.enter_context(mock.patch.object(skill_evals, "commit", return_value="test"))
                stack.enter_context(mock.patch.object(skill_evals, "wait", side_effect=interrupt_when_running))
                stack.enter_context(contextlib.redirect_stdout(io.StringIO()))
                started = time.monotonic()
                with self.assertRaises(KeyboardInterrupt):
                    skill_evals.command_run(args)
                self.assertLess(time.monotonic() - started, 7)

            self.assertEqual(sorted(path.stem for path in root.glob("*.started")), ["test-0", "test-1"])
            sizes = {path: path.stat().st_size for path in root.glob("*.heartbeat")}
            time.sleep(0.2)
            self.assertEqual({path: path.stat().st_size for path in sizes}, sizes,
                             "A CLI descendant kept running after cancellation")


if __name__ == "__main__":
    unittest.main()
