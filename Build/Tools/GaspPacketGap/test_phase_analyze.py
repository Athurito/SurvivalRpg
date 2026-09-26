"""Offline contracts for phase pairing; no Unreal process or third-party dependency."""
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from phase_analyze import Sample, analyze_run, bracket, compare, main, packet_gaps, period, segments
import probe_gap_analyze


MONTAGE = "/Game/Traversal/Vault/Run.Run"


def sample(phase, position, utc=None, montage=MONTAGE, frame=0):
    return Sample(phase if utc is None else utc, frame, phase, tuple(position), 7,
                  montage, "Traversing", "Pawn")


def record(value, role="ROLE_AUTHORITY", mode="Traversing"):
    return {"kind": "sample", "utc": value.utc, "frame": value.frame,
            "pawns": [{"player_id": value.player_id, "pawn": value.pawn,
                       "net_role": f"<NetRole.{role}: 3>", "mode": mode,
                       "montage": value.montage, "montage_position": value.phase,
                       "visual_position": value.position}]}


class PhasePairingTests(unittest.TestCase):
    def test_chord_differs_from_curve_at_identical_phase(self):
        authority = [sample(0, (0, 0, 0)), sample(.5, (50, 0, 80)), sample(1, (100, 0, 0))]
        proxy = [sample(.5, (50, 0, 0))]
        result = compare([authority], [proxy], [])
        measured = result["comparisons"][0]
        self.assertEqual(measured["signed_xyz_cm"], [0, 0, -80])
        self.assertEqual(measured["distance_cm"], 80)
        self.assertEqual(measured["nearest_raw_authority"]["authority_minus_proxy_phase_ms"], 0)
        self.assertEqual(result["periods"]["normal"]["max_distance_cm"], 80)
        self.assertNotIn("passed", result)

    def test_nonmonotone_x_does_not_select_another_phase_passage(self):
        authority = [sample(0, (0, 0, 0)), sample(.5, (100, 0, 50)), sample(1, (0, 0, 100))]
        # X=50 occurs twice. Phase .75 must select the returning portion, not X's first crossing.
        result = compare([authority], [[sample(.75, (50, 0, 75))]], [])
        measured = result["comparisons"][0]
        self.assertEqual(measured["authority_position_at_phase"], [50, 0, 75])
        self.assertEqual(measured["distance_cm"], 0)
        self.assertEqual(measured["authority_bracket"]["before"]["phase"], .5)

    def test_missing_bracket_is_excluded_without_extrapolation(self):
        authority = [sample(.2, (0, 0, 0)), sample(.4, (20, 0, 0))]
        proxy = [sample(.1, (-10, 0, 0)), sample(.5, (30, 0, 0))]
        result = compare([authority], [proxy], [])
        self.assertEqual(result["comparisons"], [])
        self.assertEqual(result["exclusion_counts"], {"no_authority_bracket": 2})
        self.assertIsNone(result["summary"]["max_distance_cm"])

    def test_montage_switch_cannot_supply_an_interpolation_endpoint(self):
        rows = [record(sample(.2, (0, 0, 0), utc=1)),
                record(sample(.4, (100, 0, 0), utc=2, montage="/Game/Other.Other"))]
        authority, _ = segments(rows, 7, 0, "ROLE_AUTHORITY")
        result = compare(authority, [[sample(.3, (50, 0, 0))]], [])
        self.assertEqual(len(authority), 2)
        self.assertEqual(result["exclusion_counts"], {"no_authority_bracket": 1})

    def test_phase_reset_same_asset_is_ambiguous_without_play_identity(self):
        rows = [record(sample(phase, (phase, 0, 0), utc=index + 1))
                for index, phase in enumerate((.2, .8, .2, .8))]
        authority, _ = segments(rows, 7, 0, "ROLE_AUTHORITY")
        result = compare(authority, [[sample(.5, (.5, 0, 0))]], [])
        self.assertEqual(len(authority), 2)
        self.assertEqual(result["exclusion_counts"], {"ambiguous_repeated_passage": 1})

    def test_proxy_repeated_passage_is_not_silently_matched_to_one_authority_play(self):
        authority = [[sample(0, (0, 0, 0)), sample(1, (100, 0, 0))]]
        result = compare(authority, [[sample(.5, (50, 0, 0))], [sample(.5, (50, 0, 0), utc=5)]], [])
        self.assertEqual(result["exclusion_counts"], {"ambiguous_repeated_passage": 2})

    def test_missing_target_or_mode_change_breaks_continuity(self):
        for middle in ({"kind": "sample", "utc": 2, "pawns": []},
                       record(sample(.4, (0, 0, 0), utc=2), mode="Walking")):
            with self.subTest(middle=middle):
                rows = [record(sample(.2, (0, 0, 0), utc=1)), middle,
                        record(sample(.6, (0, 0, 0), utc=3))]
                authority, discarded = segments(rows, 7, 0, "ROLE_AUTHORITY")
                self.assertEqual(len(authority), 2)
                self.assertEqual(len(discarded), 1)

    def test_equal_phase_with_different_authority_roots_is_ambiguous(self):
        authority = [sample(.5, (10, 0, 0), utc=1), sample(.5, (20, 0, 0), utc=2)]
        self.assertEqual(bracket(authority, .5), (None, "ambiguous_phase_plateau"))
        equivalent = [authority[0], sample(.5, (10, 0, 0), utc=2)]
        self.assertIsNone(bracket(equivalent, .5)[1])

    def test_invalid_timestamp_breaks_the_observed_segment(self):
        for timestamp in (None, "invalid", float("nan")):
            with self.subTest(timestamp=timestamp):
                invalid = record(sample(.4, (0, 0, 0), utc=2))
                invalid["utc"] = timestamp
                rows = [record(sample(.2, (0, 0, 0), utc=1)), invalid,
                        record(sample(.6, (0, 0, 0), utc=3))]
                authority, discarded = segments(rows, 7, 0, "ROLE_AUTHORITY")
                self.assertEqual(len(authority), 2)
                self.assertEqual(discarded[0]["reason"], "invalid_numeric_sample")

    def test_gap_periods_and_raw_phase_witness_remain_separate(self):
        authority = [[sample(0, (0, 0, 0)), sample(1, (100, 0, 0))]]
        proxy = [[sample(.1, (11, -2, 3), utc=1), sample(.3, (34, 0, 0), utc=3),
                  sample(.8, (80, 0, 12), utc=5)]]
        gaps = packet_gaps([{"kind": "packet_gap_start", "utc": 2, "requested_ms": 2000},
                            {"kind": "packet_gap_end", "utc": 4}], 7)
        result = compare(authority, proxy, gaps)
        self.assertEqual([row["period"] for row in result["comparisons"]], ["normal", "packet_gap", "post_gap"])
        self.assertEqual(result["periods"]["post_gap"]["max_abs_xyz_cm"], [0, 0, 12])
        row = result["comparisons"][-1]
        self.assertAlmostEqual(row["nearest_raw_authority"]["authority_minus_proxy_phase_ms"], 200)
        self.assertEqual(row["nearest_raw_authority"]["signed_xyz_cm"], [-20, 0, 12])
        self.assertEqual(row["authority_bracket"]["phase_span_ms"], 1000)

    def test_run_uses_actual_simulated_proxy_role_not_process_label(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary)
            (run / "manifest.json").write_text(json.dumps({"status": "measured", "processes": {"host": {}, "owner": {}}}), encoding="utf-8")
            (run / "trigger.json").write_text(json.dumps({"start_utc": 0, "owner_player_id": 7, "driver": "host"}), encoding="utf-8")
            authority = [record(sample(0, (0, 0, 0), utc=1)), record(sample(1, (100, 0, 0), utc=2))]
            proxy = [record(sample(.5, (50, 0, 20), utc=3), "ROLE_SIMULATED_PROXY")]
            for role, rows in (("host", authority), ("owner", proxy)):
                (run / f"{role}-samples.jsonl").write_text("\n".join(map(json.dumps, rows)), encoding="utf-8")
            result = analyze_run(run)
            self.assertEqual(result["roles"]["owner"]["summary"]["max_distance_cm"], 20)
            json.dumps(result, allow_nan=False)
            self.assertNotIn("passed", result)

    def test_aborted_gap_has_real_restore_boundary_but_is_never_completed(self):
        events = [{"kind": "packet_gap_start", "utc": 2, "requested_ms": 2000},
                  {"kind": "packet_gap_aborted", "utc": 3, "reason": "quit"}]
        gaps = packet_gaps(events, 7)
        self.assertFalse(gaps[0]["completed"])
        self.assertTrue(gaps[0]["aborted"])
        self.assertEqual(gaps[0]["abort_reason"], "quit")
        self.assertEqual(gaps[0]["actual_ms"], 1000)
        self.assertEqual(period(2.5, gaps), "packet_gap")
        self.assertEqual(period(3.1, gaps), "post_gap")
        self.assertFalse(packet_gaps(events[:1], 7)[0]["completed"])

    def test_cli_cannot_overwrite_an_earlier_report(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "phase-analysis.json"
            output.write_text("earlier evidence", encoding="utf-8")
            with patch("sys.argv", ["phase_analyze.py", temporary]), patch("phase_analyze.analyze_run", return_value={"roles": {}}):
                with self.assertRaises(FileExistsError):
                    main()
            self.assertEqual(output.read_text(encoding="utf-8"), "earlier evidence")


class FrozenGapTests(unittest.TestCase):
    LOG = "PktIncomingLoss set to 100\nPktIncomingLoss set to 0"

    def rows(self, xs, phases=None):
        phases = phases or [.5] * len(xs)
        rows = [{"kind": "packet_gap_start", "utc": 0}]
        for index, (x, phase) in enumerate(zip(xs, phases)):
            row = record(sample(phase, (x or 0, 0, 0), utc=index * .02), "ROLE_SIMULATED_PROXY")
            if x is None:
                row["pawns"] = []
            rows.append(row)
        rows.append({"kind": "packet_gap_end", "utc": len(xs) * .02})
        return rows

    def test_fragmented_frozen_pairs_cannot_accumulate_a_contiguous_pass(self):
        measured = probe_gap_analyze.analyze_role(self.rows([0, 0, 10, 10, 20, 20]), 7, self.LOG)
        self.assertEqual(len(measured["frozen_frame_pairs"]), 3)
        self.assertEqual([run["pair_count"] for run in measured["consecutive_frozen_runs"]], [1, 1, 1])
        self.assertFalse(measured["sufficient_frozen_interval"])
        self.assertFalse(measured["passed"])

    def test_continuous_frozen_pairs_pass_without_changing_the_threshold(self):
        measured = probe_gap_analyze.analyze_role(self.rows([0, 0, 0, 0]), 7, self.LOG)
        self.assertTrue(measured["passed"])
        self.assertEqual(measured["consecutive_frozen_runs"][0]["pair_count"], 3)
        self.assertAlmostEqual(measured["consecutive_frozen_runs"][0]["duration_ms"], 60)

    def test_missing_target_cannot_bridge_two_short_frozen_runs(self):
        measured = probe_gap_analyze.analyze_role(self.rows([0, 0, None, 0, 0]), 7, self.LOG)
        self.assertEqual(len(measured["frozen_frame_pairs"]), 2)
        self.assertFalse(measured["passed"])

    def test_montage_advancement_while_movement_is_frozen_fails(self):
        measured = probe_gap_analyze.analyze_role(self.rows([0, 0, 0, 0], [.5, .502, .504, .506]), 7, self.LOG)
        self.assertTrue(measured["sufficient_frozen_interval"])
        self.assertFalse(measured["montage_held_with_frozen_movement"])
        self.assertFalse(measured["passed"])

    def test_aborted_gap_never_passes_even_with_sufficient_frozen_samples(self):
        rows = self.rows([0, 0, 0, 0])
        rows[-1]["kind"] = "packet_gap_aborted"
        measured = probe_gap_analyze.analyze_role(rows, 7, self.LOG)
        self.assertTrue(measured["sufficient_frozen_interval"])
        self.assertFalse(measured["gap_completed"])
        self.assertFalse(measured["passed"])

    def test_failed_process_manifest_cannot_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            run = Path(temporary)
            (run / "manifest.json").write_text(json.dumps({"status": "failed", "arguments": {"driver": "host"},
                                                           "processes": {"host": {}, "owner": {}}}), encoding="utf-8")
            (run / "trigger.json").write_text(json.dumps({"owner_player_id": 7}), encoding="utf-8")
            (run / "owner-samples.jsonl").write_text("\n".join(map(json.dumps, self.rows([0, 0, 0, 0]))), encoding="utf-8")
            (run / "owner.log").write_text(self.LOG, encoding="utf-8")
            measured = probe_gap_analyze.analyze_run(run)
            self.assertTrue(measured["roles"]["owner"]["passed"])
            self.assertFalse(measured["passed"])


if __name__ == "__main__":
    unittest.main()
