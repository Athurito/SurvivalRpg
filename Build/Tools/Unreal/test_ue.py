"""Unreal-free tests for ue.py: engine resolution, log and report summaries, filters and map names."""
import json
from pathlib import Path
import tempfile
import unittest

import ue

SOURCE = ue.REPO.as_posix() + "/Source/SurvivalRpg/Foo.cpp"


def fake_engine(root: Path, version=(5, 8, 2)) -> Path:
    build = root / "Engine/Build"
    build.mkdir(parents=True)
    keys = ("MajorVersion", "MinorVersion", "PatchVersion")
    (build / "Build.version").write_text(json.dumps(dict(zip(keys, version))), encoding="utf-8")
    return root


def none(_association):
    return []


def temporary(case: unittest.TestCase) -> Path:
    folder = tempfile.TemporaryDirectory()
    case.addCleanup(folder.cleanup)
    return Path(folder.name)


class EngineResolution(unittest.TestCase):
    def setUp(self):
        self.temp = temporary(self)
        self.good = fake_engine(self.temp / "UE_5.8")
        self.old = fake_engine(self.temp / "UE_5.7", (5, 7, 4))

    def resolve(self, explicit=None, environ=None, registry=none, agents=none):
        return ue.resolve_engine(explicit, environ or {}, "5.8", registry, agents)

    def test_accepts_install_root_and_engine_directory(self):
        self.assertEqual(self.resolve(self.good).root, self.good)
        engine = self.resolve(self.good / "Engine")
        self.assertEqual((engine.root, engine.version, engine.source), (self.good, "5.8.2", "--engine"))

    def test_environment_variable_wins_over_registry_and_is_binding(self):
        registry = lambda _: [("registry", self.good)]
        self.assertEqual(self.resolve(environ={ue.ENGINE_ENV: str(self.good)}).source, ue.ENGINE_ENV)
        with self.assertRaisesRegex(ue.SetupError, "needs 5.8"):
            self.resolve(environ={ue.ENGINE_ENV: str(self.old)}, registry=registry)

    def test_explicit_path_wins_over_environment(self):
        engine = self.resolve(self.good, environ={ue.ENGINE_ENV: str(self.old)})
        self.assertEqual(engine.source, "--engine")

    def test_falls_through_wrong_registry_entries_to_agents_md(self):
        registry = lambda _: [("registry", self.temp / "missing"), ("registry", self.old)]
        agents = lambda _: [("AGENTS.md", self.good)]
        self.assertEqual(self.resolve(registry=registry, agents=agents).source, "AGENTS.md")

    def test_reports_every_candidate_when_nothing_fits(self):
        registry = lambda _: [("registry", self.old)]
        with self.assertRaises(ue.SetupError) as raised:
            self.resolve(registry=registry)
        self.assertIn("No UE 5.8 installation found", str(raised.exception))
        self.assertIn("UE 5.7.4", str(raised.exception))

    def test_minor_version_must_match_exactly(self):
        later = fake_engine(self.temp / "UE_5.80", (5, 80, 0))
        with self.assertRaisesRegex(ue.SetupError, "needs 5.8"):
            self.resolve(later)

    def test_agents_md_local_environment_entry(self):
        agents_md = self.temp / "AGENTS.md"
        agents_md.write_text("## Local environment\n\n- Unreal Engine 5.7: `X:/Old`\n"
                             "- Unreal Engine 5.8: `D:/Programme/UE_5.8`\n", encoding="utf-8")
        self.assertEqual(ue.agents_engines("5.8", agents_md), [("AGENTS.md Local environment", Path("D:/Programme/UE_5.8"))])
        self.assertEqual(ue.agents_engines("5.8", self.temp / "missing.md"), [])


class BuildSummary(unittest.TestCase):
    def test_success_counts_unique_warnings(self):
        log = (f"{SOURCE}(1,2): warning C4996: deprecated\n{SOURCE}(1,2): warning C4996: deprecated\n"
               "Result: Succeeded\nTotal execution time: 28.30 seconds\n")
        outcome = ue.summarize_build(log, 0, False)
        self.assertTrue(outcome.passed)
        self.assertEqual(outcome.headline, "Succeeded, 0 errors, 1 warning")
        self.assertEqual(outcome.lines, ["  warning: Source/SurvivalRpg/Foo.cpp(1,2): warning C4996: deprecated"])

    def test_failure_lists_deduplicated_errors_without_repository_prefix(self):
        log = (f"{SOURCE}(40,18): error C2079: \"Sync\" verwendet undefiniertes struct\n" * 2 +
               "Foo.cpp.obj : error LNK2001: Nicht aufgelöstes externes Symbol\n"
               "x.dll : fatal error LNK1120: 1 nicht aufgelöste Externe\n"
               "Result: Failed (OtherCompilationError)\n")
        outcome = ue.summarize_build(log, 6, False)
        self.assertFalse(outcome.passed)
        self.assertEqual(outcome.headline, "Failed (OtherCompilationError), 3 errors, 0 warnings")
        self.assertEqual(outcome.lines[0], '  Source/SurvivalRpg/Foo.cpp(40,18): error C2079: "Sync" verwendet undefiniertes struct')

    def test_live_coding_and_locked_binaries_hints(self):
        live = ue.summarize_build("Result: Failed (LiveCodingSessionActive)\n", 7, False)
        self.assertTrue(any("Live Coding" in line for line in live.lines))
        locked = ue.summarize_build("LINK : fatal error LNK1104: x.dll\nResult: Failed (OtherCompilationError)\n", 6, False)
        self.assertTrue(any("locks the binaries" in line for line in locked.lines))

    def test_missing_result_line_shows_log_tail(self):
        outcome = ue.summarize_build("dotnet crashed\n", 1, False)
        self.assertFalse(outcome.passed)
        self.assertIn("  dotnet crashed", outcome.lines)

    def test_diagnostic_patterns(self):
        for line in ("Foo.h(12): Error: Unrecognized type", "ERROR: Unable to build", "a.obj : error LNK2019: x"):
            self.assertTrue(ue.ERROR_LINE.search(line), line)
        for line in ("Success - 0 error(s), 3 warning(s)", "Log file: C:/errors.txt", "  Executing up to 16 processes"):
            self.assertFalse(ue.ERROR_LINE.search(line), line)


def report(*tests):
    return {"tests": [{"fullTestPath": path, "state": state, "warnings": warnings, "entries": entries}
                      for path, state, warnings, entries in tests]}


class TestSummary(unittest.TestCase):
    def summarize(self, value, filters=("SurvivalRpg",), log="", expected=None, exit_code=0, timed_out=False):
        return ue.summarize_tests(value, log, list(filters), expected, exit_code, timed_out)

    def test_all_passed(self):
        outcome = self.summarize(report(("SurvivalRpg.A", "Success", 0, []), ("SurvivalRpg.B", "Success", 2, [])))
        self.assertTrue(outcome.passed)
        self.assertEqual(outcome.headline, "2 run: 2 passed (1 with warnings), 0 failed, 0 not run")
        self.assertEqual(outcome.lines, [])

    def test_failed_test_lists_its_first_errors(self):
        error = {"event": {"type": "Error", "message": "Expected x"}, "filename": SOURCE, "lineNumber": 7}
        warning = {"event": {"type": "Warning", "message": "noise"}}
        outcome = self.summarize(report(("SurvivalRpg.A", "Fail", 1, [warning, error] + [error] * 4),
                                        ("SurvivalRpg.B", "NotRun", 0, [])))
        self.assertFalse(outcome.passed)
        self.assertEqual(outcome.lines[:2], ["FAIL SurvivalRpg.A", "  Expected x (Source/SurvivalRpg/Foo.cpp:7)"])
        self.assertIn("  ... 2 more in the log", outcome.lines)
        self.assertIn("NOT RUN SurvivalRpg.B (NotRun)", outcome.lines)

    def test_unmatched_filter_and_expected_count_fail(self):
        value = report(("SurvivalRpg.Combat.A", "Success", 0, []))
        unmatched = self.summarize(value, filters=("SurvivalRpg.Combat", "SurvivalRpg.Missing"))
        self.assertFalse(unmatched.passed)
        self.assertIn("no test matched filter: SurvivalRpg.Missing", unmatched.lines)
        counted = self.summarize(value, expected=2)
        self.assertFalse(counted.passed)
        self.assertIn("expected 2 tests, the report has 1", counted.lines)
        self.assertTrue(self.summarize(value, filters=("Group:Smoke",), expected=1).passed)

    def test_no_report_reports_the_unmatched_filter(self):
        log = "LogAutomationCommandLine: Error: No automation tests matched 'SurvivalRpg.Nope'\n"
        outcome = self.summarize(None, log=log)
        self.assertFalse(outcome.passed)
        self.assertEqual(outcome.headline, "No automation tests matched 'SurvivalRpg.Nope' (editor exit code 0)")

    def test_empty_report_crash_and_timeout_fail(self):
        self.assertFalse(self.summarize(report()).passed)
        passed = report(("SurvivalRpg.A", "Success", 0, []))
        self.assertFalse(self.summarize(passed, log="=== Critical error: ===\n").passed)
        self.assertFalse(self.summarize(passed, timed_out=True).passed)

    def test_ensures_and_engine_network_prediction_are_reported(self):
        log = ("Ensure condition failed: x\nLogModuleManager: InternalLoadLibrary: 'NetworkPrediction' "
               "('C:/UE/Engine/Plugins/Runtime/NetworkPrediction/Binaries/Win64/UnrealEditor-NetworkPrediction.dll')\n")
        outcome = self.summarize(report(("SurvivalRpg.A", "Success", 0, [])), log=log)
        self.assertIn("1 ensure in the log", outcome.lines)
        self.assertTrue(any("not the project override" in line for line in outcome.lines))

    def test_filter_semantics_follow_run_tests(self):
        path = "SurvivalRpg.Combat.Block.Lifecycle"
        self.assertTrue(ue.filter_matches(path, "combat.block"))
        self.assertTrue(ue.filter_matches(path, "^SurvivalRpg.Combat"))
        self.assertFalse(ue.filter_matches(path, "^Combat"))
        self.assertTrue(ue.filter_matches(path, "Lifecycle$"))
        self.assertTrue(ue.filter_matches(path, "^SurvivalRpg.Combat.Block.Lifecycle$"))
        self.assertTrue(ue.filter_matches(path, "StartsWith:SurvivalRpg.Combat"))
        self.assertFalse(ue.filter_matches(path, "StartsWith:SurvivalRpg.Com"))
        self.assertIsNone(ue.filter_matches(path, "Group:Smoke"))

    def test_exec_commands(self):
        self.assertEqual(ue.exec_commands(["A", "B"], ["t.MaxFPS 30"]), "t.MaxFPS 30,Automation RunTests A+B; Quit")
        with self.assertRaisesRegex(ue.SetupError, "Commas"):
            ue.exec_commands(["A"], ["np.ForceReconcile 0,np.SkipReconcile 0"])


COOK_LOG = """[2026.10.01-10.00.00:000][  0]LogInit: Display: Warning/Error Summary (Unique only)
[2026.10.01-10.00.00:000][  0]LogInit: Display: -----------------------------------
[2026.10.01-10.00.00:000][  0]LogInit: Display: LogCook: Error: Could not find package at file X!
[2026.10.01-10.00.00:000][  0]LogInit: Display: LogAbilitySystem: Warning: No GameplayCueNotifyPaths were specified
[2026.10.01-10.00.00:000][  0]LogInit: Display:
[2026.10.01-10.00.00:000][  0]LogInit: Display: {result} - {errors} error(s), 1 warning(s)
[2026.10.01-10.00.00:000][  0]LogInit: Display:
"""


class CookSummary(unittest.TestCase):
    def test_failure_lists_summary_entries(self):
        outcome = ue.summarize_cook(COOK_LOG.format(result="Failure", errors=1), 1, False)
        self.assertFalse(outcome.passed)
        self.assertEqual(outcome.headline, "Failure, 1 error, 1 warning")
        self.assertEqual(outcome.lines, ["  LogCook: Error: Could not find package at file X!",
                                         "  warning: LogAbilitySystem: Warning: No GameplayCueNotifyPaths were specified"])

    def test_success_and_missing_summary(self):
        self.assertTrue(ue.summarize_cook(COOK_LOG.format(result="Success", errors=0), 0, False).passed)
        self.assertFalse(ue.summarize_cook(COOK_LOG.format(result="Success", errors=0), 3, False).passed)
        self.assertEqual(ue.summarize_cook("crash\n", None, True).headline, "timed out (exit code None)")


class Maps(unittest.TestCase):
    def setUp(self):
        self.repo = temporary(self)
        for relative in ("Content/Maps/Test/Lvl_A.umap", "Plugins/GameFeatures/GF_X/Content/Maps/Lvl_B.umap",
                         "Content/One/Lvl_Dup.umap", "Content/Two/Lvl_Dup.umap"):
            (self.repo / relative).parent.mkdir(parents=True, exist_ok=True)
            (self.repo / relative).touch()
        (self.repo / "Plugins/GameFeatures/GF_X/GF_X.uplugin").touch()

    def test_names_and_package_paths(self):
        self.assertEqual(ue.resolve_map("Lvl_A", self.repo), "/Game/Maps/Test/Lvl_A")
        self.assertEqual(ue.resolve_map("Lvl_B", self.repo), "/GF_X/Maps/Lvl_B")
        self.assertEqual(ue.resolve_map("/Game/Maps/Test/Lvl_A", self.repo), "/Game/Maps/Test/Lvl_A")
        self.assertEqual(ue.resolve_map("/GF_X/Maps/Lvl_B", self.repo), "/GF_X/Maps/Lvl_B")

    def test_unknown_or_ambiguous_maps_fail_early(self):
        for name in ("Lvl_Missing", "Lvl_Dup", "/Game/Maps/Lvl_Missing", "/GF_Missing/Maps/Lvl_B"):
            with self.assertRaises(ue.SetupError, msg=name):
                ue.resolve_map(name, self.repo)


class CommandLine(unittest.TestCase):
    def test_engine_option_before_or_after_the_command(self):
        self.assertEqual(ue.parser().parse_args(["--engine", "X", "build"]).engine, Path("X"))
        self.assertEqual(ue.parser().parse_args(["build", "--engine", "Y"]).engine, Path("Y"))
        self.assertIsNone(ue.parser().parse_args(["build"]).engine)

    def test_unreal_arguments_and_defaults(self):
        args = ue.parser().parse_args(["test", "SurvivalRpg.A", "--ue-arg=-ini:Engine:[Core.Log]:LogX=Verbose"])
        self.assertEqual((args.ue_arg, args.timeout, args.map), (["-ini:Engine:[Core.Log]:LogX=Verbose"], 60.0, ue.TEST_MAP))

    def test_test_command_line(self):
        engine = ue.Engine(Path("E"), "5.8.2", "test")
        args = ue.parser().parse_args(["test", "A", "--exec", "t.MaxFPS 30", "--log-cmds", "LogA Verbose",
                                       "--log-cmds", "LogB Log", "--null-rhi"])
        command = [str(part) for part in ue.test_command(engine, args, Path("run"))]
        self.assertIn("-NullRHI", command)
        self.assertIn("-ExecCmds=t.MaxFPS 30,Automation RunTests A; Quit", command)
        self.assertIn("-LogCmds=LogA Verbose, LogB Log", command)

    def test_unverified_overrides_stop_with_the_stage_command(self):
        script = temporary(self) / "prepare.py"
        script.write_text("import sys\nprint('ERROR: Missing staging marker', file=sys.stderr)\nsys.exit(1)\n")
        original = ue.PREPARE
        ue.PREPARE = script
        self.addCleanup(setattr, ue, "PREPARE", original)
        with self.assertRaises(ue.SetupError) as raised:
            ue.verify_overrides(ue.Engine(Path("/UE_5.8"), "5.8.2", "test"))
        self.assertIn("do not verify: ERROR: Missing staging marker", str(raised.exception))
        self.assertIn("prepare.py stage --engine /UE_5.8/Engine", str(raised.exception))

    def test_out_must_be_new_or_empty(self):
        folder = temporary(self)
        self.assertEqual(ue.run_directory("test", folder), folder.resolve())
        (folder / "report").mkdir()
        with self.assertRaisesRegex(ue.SetupError, "new or empty"):
            ue.run_directory("test", folder)


if __name__ == "__main__":
    unittest.main()
