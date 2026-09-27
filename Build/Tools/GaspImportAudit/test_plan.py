"""Reference-graph contracts for the portable offline import planner."""

import contextlib
from copy import deepcopy
import io
import json
from pathlib import Path
import tempfile
import unittest

from plan import build_plan, main


def record(*dependencies, management=()):
    return {"package_dependencies": list(dependencies), "management_dependencies": list(management)}


def mapping(source="/Game/Source", target="/Game/Target", provenance="manifest:1"):
    return {"source": source, "target": target, "provenance": provenance}


def by_source(result):
    return {entry["source"]: entry for entry in result["sources"]}


class PlanTests(unittest.TestCase):
    def test_candidates_are_exact_mapping_sources_and_all_other_records_are_roots(self):
        records = {"/Game/Source": record(), "/Game/Target": record(),
                   "/Game/Imported/Unmapped": record("/Game/Source"), "/Feature/Retained": record()}
        result = build_plan(records, [mapping()])
        self.assertEqual(result["candidate_count"], 1)
        self.assertEqual(result["retained_roots"], ["/Feature/Retained", "/Game/Imported/Unmapped", "/Game/Target"])
        self.assertEqual(result["sources"][0]["status"], "retained_reference")
        self.assertEqual(result["sources"][0]["reference_path"], ["/Game/Imported/Unmapped", "/Game/Source"])

    def test_transitive_candidate_chain_has_shortest_retained_root_witness(self):
        records = {"/Game/Root": record("/Game/A"), "/Game/A": record("/Game/B"),
                   "/Game/B": record("/Game/C"), "/Game/C": record(), "/Game/Target": record()}
        rows = [mapping(f"/Game/{name}") for name in "ABC"]
        result = by_source(build_plan(records, rows))
        self.assertEqual(result["/Game/C"]["reference_path"], ["/Game/Root", "/Game/A", "/Game/B", "/Game/C"])
        records["/Game/Root"]["package_dependencies"].append("/Game/C")
        self.assertEqual(by_source(build_plan(records, rows))["/Game/C"]["reference_path"], ["/Game/Root", "/Game/C"])

    def test_management_edges_and_both_direct_reference_kinds_are_followed(self):
        records = {"/Game/Root": record("/Game/Source", management=("/Game/Source",)),
                   "/Game/Source": record(), "/Game/Target": record()}
        source = build_plan(records, [mapping()])["sources"][0]
        self.assertEqual(source["direct_referencers"], [{"package": "/Game/Root", "kinds": ["management_dependencies", "package_dependencies"]}])
        records["/Game/Root"]["package_dependencies"] = []
        self.assertEqual(build_plan(records, [mapping()])["sources"][0]["status"], "retained_reference")

    def test_cycle_without_retained_inbound_remains_unreferenced_then_inbound_retains_both(self):
        records = {"/Game/A": record("/Game/B"), "/Game/B": record("/Game/A"), "/Game/Target": record()}
        rows = [mapping("/Game/A"), mapping("/Game/B")]
        self.assertEqual(build_plan(records, rows)["counts"]["candidate_unreferenced"], 2)
        records["/Game/Target"] = record(management=("/Game/B",))
        result = build_plan(records, rows)
        self.assertEqual(result["counts"]["retained_reference"], 2)
        self.assertEqual(by_source(result)["/Game/A"]["reference_path"], ["/Game/Target", "/Game/B", "/Game/A"])

    def test_missing_source_keeps_observed_reference_evidence_and_missing_targets(self):
        result = build_plan({"/Game/Root": record("/Game/Source")}, [mapping()])
        source = result["sources"][0]
        self.assertEqual(source["status"], "missing_source")
        self.assertEqual(source["reference_path"], ["/Game/Root", "/Game/Source"])
        self.assertEqual(source["missing_targets"], ["/Game/Target"])
        self.assertEqual(result["missing_sources"], ["/Game/Source"])
        self.assertEqual(result["missing_package_dependencies"][0]["package"], "/Game/Source")

    def test_every_target_must_exist_and_retention_does_not_hide_missing_target_fact(self):
        records = {"/Game/Source": record(), "/Game/Target": record()}
        rows = [mapping(), mapping(target="/Game/MissingTarget")]
        source = build_plan(records, rows)["sources"][0]
        self.assertEqual(source["status"], "missing_target")
        self.assertEqual(source["targets"], ["/Game/MissingTarget", "/Game/Target"])
        records["/Game/Root"] = record("/Game/Source")
        source = build_plan(records, rows)["sources"][0]
        self.assertEqual(source["status"], "retained_reference")
        self.assertEqual(source["missing_targets"], ["/Game/MissingTarget"])

    def test_duplicate_mapping_and_different_targets_preserve_provenance_per_pair(self):
        first = mapping(provenance={"file": "first.json", "row": 2})
        second = mapping(target="/Game/OtherTarget", provenance="second:3")
        third = mapping(provenance="third:4")
        records = {package: record() for package in ("/Game/Source", "/Game/Target", "/Game/OtherTarget")}
        result = build_plan(records, [first, second, first, third])
        self.assertEqual(result["sources"][0]["targets"], ["/Game/OtherTarget", "/Game/Target"])
        self.assertCountEqual(result["sources"][0]["mappings"], [first, second, third])
        self.assertEqual(result["duplicate_mapping_count"], 1)
        self.assertEqual(result["multiple_target_sources"], ["/Game/Source"])

    def test_explicit_candidate_root_has_zero_edge_witness(self):
        result = build_plan({"/Game/Source": record(), "/Game/Target": record()}, [mapping()], ["/Game/Source"])
        self.assertEqual(result["sources"][0]["status"], "retained_reference")
        self.assertEqual(result["sources"][0]["reference_path"], ["/Game/Source"])
        self.assertTrue(result["sources"][0]["explicitly_retained"])
        self.assertEqual(result["retained_root_count"], 2)

    def test_missing_explicit_root_is_reported_without_inventing_edges(self):
        result = build_plan({"/Game/Source": record(), "/Game/Target": record()}, [mapping()], ["/Game/AbsentRoot"])
        self.assertEqual(result["missing_retained_roots"], ["/Game/AbsentRoot"])
        self.assertEqual(result["retained_root_count"], 2)
        self.assertEqual(result["inventoried_retained_root_count"], 1)
        self.assertEqual(result["sources"][0]["status"], "candidate_unreferenced")
        self.assertEqual(result["sources"][0]["reference_path"], [])

    def test_unknown_dependency_is_separate_and_custom_mount_is_not_guessed_external(self):
        records = {"/Game/Source": record("/Game/Absent", "/MysteryPlugin/Absent", "/Engine/Material", "/Script/Engine"),
                   "/Game/Target": record()}
        result = build_plan(records, [mapping()])
        self.assertEqual([row["package"] for row in result["missing_package_dependencies"]], ["/Game/Absent", "/MysteryPlugin/Absent"])
        self.assertEqual([row["package"] for row in result["external_package_dependencies"]], ["/Engine/Material"])
        self.assertEqual([row["package"] for row in result["script_package_dependencies"]], ["/Script/Engine"])
        self.assertEqual(result["sources"][0]["status"], "candidate_unreferenced")
        explicit = build_plan(records, [mapping()], external_mounts=["/MysteryPlugin"])
        self.assertEqual([row["package"] for row in explicit["missing_package_dependencies"]], ["/Game/Absent"])
        self.assertEqual(len(explicit["external_package_dependencies"]), 2)
        self.assertNotIn("passed", result)
        self.assertNotIn("safe_to_delete", result["sources"][0])

    def test_inventoried_external_mount_package_is_still_a_retained_root_with_followed_edges(self):
        records = {"/VerifiedPlugin/Asset": record("/Game/Source"), "/Game/Source": record(), "/Game/Target": record()}
        result = build_plan(records, [mapping()], external_mounts=["/VerifiedPlugin"])
        self.assertEqual(result["sources"][0]["reference_path"], ["/VerifiedPlugin/Asset", "/Game/Source"])

    def test_source_equal_target_and_target_itself_candidate_do_not_become_implicit_roots(self):
        result = build_plan({"/Game/A": record(), "/Game/B": record()},
                            [mapping("/Game/A", "/Game/B"), mapping("/Game/B", "/Game/B")])
        self.assertEqual(result["retained_root_count"], 0)
        self.assertEqual(result["counts"]["candidate_unreferenced"], 2)
        self.assertTrue(all(not row["reference_path"] for row in result["sources"]))

    def test_all_ordering_and_shortest_path_ties_are_deterministic(self):
        records = {"/Game/ZRoot": record("/Game/B", "/Game/A"), "/Game/ARoot": record("/Game/B", "/Game/A"),
                   "/Game/A": record("/Game/C"), "/Game/B": record("/Game/C"), "/Game/C": record(), "/Game/Target": record()}
        rows = [mapping(f"/Game/{name}", provenance={"row": 1, "file": "manifest"}) for name in "ABC"]
        before = build_plan(records, rows, ["/Game/ZRoot", "/Game/ARoot"])
        reordered = {key: {kind: list(reversed(value)) for kind, value in records[key].items()} for key in reversed(records)}
        after = build_plan(reordered, list(reversed(rows)), ["/Game/ARoot", "/Game/ZRoot"])
        self.assertEqual(json.dumps(before), json.dumps(after))
        self.assertEqual(by_source(before)["/Game/C"]["reference_path"], ["/Game/ARoot", "/Game/A", "/Game/C"])

    def test_inputs_are_unchanged_and_provenance_is_independent(self):
        records = {"/Game/Source": record(), "/Game/Target": record()}
        rows = [mapping(provenance={"rows": [1]})]
        originals = deepcopy((records, rows))
        result = build_plan(records, rows, ["/Game/Source"])
        self.assertEqual((records, rows), originals)
        rows[0]["provenance"]["rows"].append(2)
        self.assertEqual(result["sources"][0]["provenance"], [{"rows": [1]}])
        self.assertEqual(build_plan(records, [mapping()])["sources"][0]["status"], "candidate_unreferenced")

    def test_malformed_or_incomplete_inventory_fails_instead_of_becoming_empty_edges(self):
        invalid_records = [[], {"/Game/Source": {}}, {"/Game/Source": {"package_dependencies": [], "management_dependencies": ""}},
                           {"/Game/Source.Source": record()}, {"/Game/Source": record("relative")}, {"/Game/Source": None}]
        for records in invalid_records:
            with self.subTest(records=records), self.assertRaises(ValueError):
                build_plan(records, [mapping()])
        for rows in ([{"source": "/Game/A", "target": "/Game/B"}], [mapping(provenance=float("nan"))]):
            with self.subTest(rows=rows), self.assertRaises(ValueError):
                build_plan({}, rows)
        for mount in ("/Game", "/Script", "Plugin", "/Nested/Plugin"):
            with self.subTest(mount=mount), self.assertRaises(ValueError):
                build_plan({}, [], external_mounts=[mount])
        with self.assertRaises(ValueError):
            build_plan({}, [], retained_roots={})

    def test_empty_input_has_no_candidates_or_roots(self):
        result = build_plan({}, [])
        self.assertEqual(result["sources"], [])
        self.assertEqual(result["retained_root_count"], 0)
        self.assertEqual(sum(result["counts"].values()), 0)

    def test_cli_reads_wrapped_records_and_creates_output_exclusively(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            records, mappings, output = root / "records.json", root / "mappings.json", root / "plan.json"
            records.write_text(json.dumps({"records": {"/Game/Source": record(), "/Game/Target": record()}}), encoding="utf-8")
            mappings.write_text(json.dumps({"mappings": [mapping()]}), encoding="utf-8")
            arguments = ["--records", str(records), "--mappings", str(mappings), "--retain", "/Game/Source", "--output", str(output)]
            self.assertEqual(main(arguments), 0)
            original = output.read_bytes()
            self.assertEqual(json.loads(original)["sources"][0]["status"], "retained_reference")
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(main(arguments), 2)
            self.assertEqual(output.read_bytes(), original)


if __name__ == "__main__":
    unittest.main()
