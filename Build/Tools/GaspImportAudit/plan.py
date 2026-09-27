"""Offline package-reference planner. This module never loads or changes Unreal assets.

Candidates are exactly the mapping sources. Every other inventoried package is a
retained root, including targets and packages in apparently imported directories.
Both supplied dependency kinds are followed; provenance records are not edges.
An unreferenced candidate is only an observation about this inventory, not a
deletion recommendation or proof about runtime/string references.
"""

from __future__ import annotations

import argparse
from collections import Counter, defaultdict, deque
import json
from pathlib import Path
import sys


DEPENDENCY_KINDS = ("package_dependencies", "management_dependencies")
STATUSES = ("retained_reference", "missing_source", "missing_target", "candidate_unreferenced")


def _package(value: object, context: str) -> str:
    if (not isinstance(value, str) or not value.startswith("/")
            or len(value.split("/")) < 3 or any(not part for part in value.split("/")[1:])
            or "." in value or "\\" in value or value.strip() != value):
        raise ValueError(f"{context} must be a long package name, not an object or directory path: {value!r}")
    return value


def _package_list(value: object, context: str) -> list[str]:
    if not isinstance(value, list):
        raise ValueError(f"{context} must be a list of package names")
    return [_package(item, context) for item in value]


def _canonical(value: object) -> str:
    try:
        return json.dumps(value, sort_keys=True, ensure_ascii=False, allow_nan=False)
    except (TypeError, ValueError) as error:
        raise ValueError("mapping provenance must be finite JSON data") from error


def build_plan(records: dict[str, dict], mappings: list[dict],
               retained_roots: list[str] | None = None,
               external_mounts: list[str] | None = None) -> dict:
    """Return a deterministic reference report without modifying its inputs.

    Each record must provide both dependency lists, even when empty. Each mapping
    must provide source, target and JSON provenance. Multiple targets/provenances
    are preserved. Status priority is missing source, retained reference, missing
    target, then unreferenced candidate; missing facts are also reported separately.

    /Script and /Engine are explicit traversal boundaries only for absent records.
    Other external mounts must be supplied by a caller with independent evidence.
    Unknown dependencies and missing explicit roots remain separate limitations;
    they do not fabricate reference edges or mark every candidate as referenced.
    """
    if not isinstance(records, dict) or not isinstance(mappings, list):
        raise ValueError("records must be an object and mappings must be a list")
    explicit_roots = set(_package_list([] if retained_roots is None else retained_roots, "retained_roots"))
    if external_mounts is not None and not isinstance(external_mounts, list):
        raise ValueError("external_mounts must be a list of mount names")
    declared_external = {"/Engine"}
    for mount in external_mounts or []:
        if (not isinstance(mount, str) or not mount.startswith("/")
                or mount.count("/") != 1 or len(mount) < 2
                or any(char in mount for char in ".\\ \t\r\n")):
            raise ValueError(f"external mount must have the form /PluginName: {mount!r}")
        if mount in ("/Game", "/Script"):
            raise ValueError(f"{mount} cannot be declared an external plugin mount")
        declared_external.add(mount)

    graph: dict[str, set[str]] = {}
    incoming: dict[str, dict[str, set[str]]] = defaultdict(lambda: defaultdict(set))
    for package, record in records.items():
        _package(package, "record key")
        if not isinstance(record, dict):
            raise ValueError(f"record {package} must be an object")
        graph[package] = set()
        for kind in DEPENDENCY_KINDS:
            if kind not in record:
                raise ValueError(f"record {package} is missing {kind}; an omitted list is not an empty inventory")
            for dependency in _package_list(record[kind], f"{package}.{kind}"):
                graph[package].add(dependency)
                incoming[dependency][package].add(kind)

    grouped: dict[str, dict[str, dict]] = defaultdict(dict)
    for mapping in mappings:
        if not isinstance(mapping, dict) or not {"source", "target", "provenance"} <= mapping.keys():
            raise ValueError("each mapping must have source, target and provenance")
        source = _package(mapping["source"], "mapping source")
        target = _package(mapping["target"], "mapping target")
        # Round-trip produces an independent JSON value; callers cannot change the report later.
        provenance = json.loads(_canonical(mapping["provenance"]))
        normalized = {"source": source, "target": target, "provenance": provenance}
        grouped[source][_canonical(normalized)] = normalized

    candidates = set(grouped)
    roots = (set(records) - candidates) | explicit_roots
    # Sorted multi-source BFS chooses a shortest witness, with stable lexical ties.
    parents: dict[str, str | None] = {root: None for root in sorted(roots)}
    pending = deque(sorted(roots))
    while pending:
        package = pending.popleft()
        for dependency in sorted(graph.get(package, ())):
            if dependency not in parents:
                parents[dependency] = package
                pending.append(dependency)

    def reference_path(package: str) -> list[str]:
        path = []
        while package in parents:
            path.append(package)
            parent = parents[package]
            if parent is None:
                break
            package = parent
        return list(reversed(path))

    def referencers(package: str) -> list[dict]:
        return [{"package": source, "kinds": sorted(kinds)}
                for source, kinds in sorted(incoming.get(package, {}).items())]

    missing_dependencies, external_dependencies, script_dependencies = [], [], []
    for package in sorted(set(incoming) - set(records)):
        entry = {"package": package, "direct_referencers": referencers(package)}
        mount = "/" + package.split("/")[1]
        if mount == "/Script":
            script_dependencies.append(entry)
        elif mount in declared_external:
            external_dependencies.append(entry)
        else:
            missing_dependencies.append(entry)

    sources = []
    for source in sorted(candidates):
        entries = [grouped[source][key] for key in sorted(grouped[source])]
        targets = sorted({entry["target"] for entry in entries})
        provenance_values = {_canonical(entry["provenance"]): entry["provenance"] for entry in entries}
        missing_targets = sorted(set(targets) - set(records))
        path = reference_path(source)
        if source not in records:
            status = "missing_source"
        elif path:
            status = "retained_reference"
        elif missing_targets:
            status = "missing_target"
        else:
            status = "candidate_unreferenced"
        sources.append({
            "source": source, "targets": targets,
            "provenance": [provenance_values[key] for key in sorted(provenance_values)],
            "mappings": entries, "status": status, "source_present": source in records,
            "missing_targets": missing_targets, "reference_path": path,
            "direct_referencers": referencers(source), "explicitly_retained": source in explicit_roots,
        })

    counts = Counter(entry["status"] for entry in sources)
    unique_mapping_count = sum(len(entries) for entries in grouped.values())
    return {
        "schema_version": 1,
        "scope": "Inventoried package and management references only; candidate_unreferenced is not a deletion approval.",
        "sources": sources,
        "counts": {status: counts[status] for status in STATUSES},
        "record_count": len(records), "candidate_count": len(sources),
        "mapping_count": len(mappings), "unique_mapping_count": unique_mapping_count,
        "duplicate_mapping_count": len(mappings) - unique_mapping_count,
        "retained_roots": sorted(roots), "retained_root_count": len(roots),
        "inventoried_retained_root_count": len(roots & set(records)),
        "missing_retained_roots": sorted(explicit_roots - set(records)),
        "missing_sources": [entry["source"] for entry in sources if not entry["source_present"]],
        "missing_targets": sorted({target for entry in sources for target in entry["missing_targets"]}),
        "multiple_target_sources": [entry["source"] for entry in sources if len(entry["targets"]) > 1],
        "missing_package_dependencies": missing_dependencies,
        "missing_package_dependency_count": len(missing_dependencies),
        "external_mounts": sorted(declared_external),
        "external_package_dependencies": external_dependencies,
        "script_package_dependencies": script_dependencies,
        "limitations": [
            "Unknown dependencies have no outgoing inventory; their references cannot be reconstructed.",
            "External and Script boundaries are reported, not checked for loadability or hidden referencers.",
            "Runtime, string, editor-only or other references absent from the supplied lists are outside this report.",
            "Mapping provenance is caller-supplied evidence, not an assertion of behavioral equivalence.",
        ],
    }


def _read(path: Path, wrapper: str) -> object:
    value = json.loads(path.read_text(encoding="utf-8-sig"))
    return value[wrapper] if isinstance(value, dict) and wrapper in value else value


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--records", type=Path, required=True, help="Record object, or object with a records member")
    parser.add_argument("--mappings", type=Path, required=True, help="Mapping list, or object with a mappings member")
    parser.add_argument("--retain", action="append", default=[], metavar="PACKAGE")
    parser.add_argument("--external-mount", action="append", default=[], metavar="/PLUGIN")
    parser.add_argument("--output", type=Path, help="Create a new report exclusively; otherwise print JSON")
    args = parser.parse_args(argv)
    try:
        result = build_plan(_read(args.records, "records"), _read(args.mappings, "mappings"),
                            args.retain, args.external_mount)
        serialized = json.dumps(result, indent=2, ensure_ascii=False, allow_nan=False) + "\n"
        if args.output:
            with args.output.open("x", encoding="utf-8", newline="\n") as stream:
                stream.write(serialized)
        else:
            print(serialized, end="")
    except (OSError, ValueError) as error:
        print(f"Import audit failed: {error}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
