"""Compare sampled visual-root paths at equal montage phase, without a pass threshold.

This complements the unchanged X-plane analyzer. It observes mesh-root positions,
not bone poses, native NetworkPrediction history, or simultaneous network latency.
Only one unambiguous forward phase passage per player/montage/mode is paired.
"""
import argparse
from collections import Counter
from dataclasses import dataclass
import json
import math
from pathlib import Path


@dataclass(frozen=True)
class Sample:
    utc: float
    frame: int
    phase: float
    position: tuple
    player_id: int
    montage: str
    mode: str
    pawn: str

    @property
    def key(self):
        return self.player_id, self.montage, self.mode

    def report(self):
        return {"utc": self.utc, "frame": self.frame, "phase": self.phase,
                "visual_position": self.position}


def finite(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) and math.isfinite(value)


def segments(records, player_id, start_utc, net_role):
    """Invalid/missing observations break continuity; a phase rewind starts a new passage."""
    result, discarded, current = [], [], []
    for record in records:
        if record.get("kind") != "sample":
            continue
        if finite(record.get("utc")) and record["utc"] < start_utc:
            continue
        pawns = [pawn for pawn in record.get("pawns", []) if pawn.get("player_id") == player_id]
        pawn = pawns[0] if len(pawns) == 1 else {}
        reason = None
        if not finite(record.get("utc")):
            reason = "invalid_numeric_sample"
        elif len(pawns) != 1:
            reason = "missing_or_duplicate_target"
        elif net_role not in pawn.get("net_role", ""):
            reason = "different_network_role"
        elif pawn.get("mode") != "Traversing" or not pawn.get("montage"):
            reason = "outside_active_montage"
        elif (not finite(pawn.get("montage_position")) or not finite(record.get("utc"))
              or not isinstance(pawn.get("visual_position"), (list, tuple))
              or len(pawn["visual_position"]) != 3 or not all(map(finite, pawn["visual_position"]))):
            reason = "invalid_numeric_sample"
        if reason:
            if current:
                result.append(current)
                current = []
            discarded.append({"frame": record.get("frame"), "utc": record.get("utc"), "reason": reason})
            continue
        sample = Sample(record["utc"], record.get("frame"), pawn["montage_position"],
                        tuple(pawn["visual_position"]), player_id, pawn["montage"], pawn["mode"], pawn.get("pawn"))
        if current and (sample.key != current[-1].key or sample.pawn != current[-1].pawn
                        or sample.phase < current[-1].phase or sample.utc <= current[-1].utc):
            result.append(current)
            current = []
        current.append(sample)
    if current:
        result.append(current)
    return result, discarded


def bracket(samples, phase):
    """Use observed endpoints only. A repeated phase with differing roots is ambiguous."""
    exact = [sample for sample in samples if sample.phase == phase]
    if exact:
        if any(sample.position != exact[0].position for sample in exact[1:]):
            return None, "ambiguous_phase_plateau"
        return (exact[0], exact[0], 0.0), None
    if phase < samples[0].phase or phase > samples[-1].phase:
        return None, "no_authority_bracket"
    for before, after in zip(samples, samples[1:]):
        if before.phase < phase < after.phase:
            return (before, after, (phase - before.phase) / (after.phase - before.phase)), None
    return None, "no_authority_bracket"


def packet_gaps(records, player_id):
    """Read recorded local receipt-loss windows; these are not native recovery boundaries."""
    gaps = []
    for record in records:
        if record.get("kind") == "packet_gap_start":
            if record.get("observed", {}).get("player_id", player_id) != player_id:
                continue
            if gaps and gaps[-1]["end_utc"] is None:
                raise ValueError("Nested packet-gap starts cannot be classified safely")
            gaps.append({"start_utc": record["utc"], "end_utc": None, "completed": False, "aborted": False,
                         "requested_ms": record.get("requested_ms")})
        elif record.get("kind") in ("packet_gap_end", "packet_gap_aborted"):
            if not gaps or gaps[-1]["end_utc"] is not None:
                raise ValueError("Packet-gap end has no matching start")
            if record["utc"] < gaps[-1]["start_utc"]:
                raise ValueError("Packet-gap timestamps run backwards")
            gaps[-1]["end_utc"] = record["utc"]
            gaps[-1]["actual_ms"] = (record["utc"] - gaps[-1]["start_utc"]) * 1000
            gaps[-1]["completed"] = record["kind"] == "packet_gap_end"
            gaps[-1]["aborted"] = record["kind"] == "packet_gap_aborted"
            if gaps[-1]["aborted"]:
                gaps[-1]["abort_reason"] = record.get("reason")
    return gaps


def period(utc, gaps):
    after_gap = False
    for gap in gaps:
        if utc < gap["start_utc"]:
            break
        if gap["end_utc"] is None or utc <= gap["end_utc"]:
            return "packet_gap"
        after_gap = True
    return "post_gap" if after_gap else "normal"


def residual(proxy, reference, endpoints):
    before, after, alpha = endpoints
    expected = [a + (b - a) * alpha for a, b in zip(before.position, after.position)]
    delta = [a - b for a, b in zip(proxy.position, expected)]
    nearest = min(reference, key=lambda sample: abs(sample.phase - proxy.phase))
    raw_delta = [a - b for a, b in zip(proxy.position, nearest.position)]
    return {
        "proxy": proxy.report(), "authority_position_at_phase": expected,
        "signed_xyz_cm": delta, "distance_cm": math.sqrt(sum(value * value for value in delta)),
        "authority_bracket": {"before": before.report(), "after": after.report(), "alpha": alpha,
                              "phase_span_ms": (after.phase - before.phase) * 1000,
                              "wall_span_ms": (after.utc - before.utc) * 1000},
        "nearest_raw_authority": {**nearest.report(),
                                  "authority_minus_proxy_phase_ms": (nearest.phase - proxy.phase) * 1000,
                                  "signed_xyz_cm": raw_delta,
                                  "distance_cm": math.sqrt(sum(value * value for value in raw_delta))},
    }


def summarize(comparisons):
    if not comparisons:
        return {"count": 0, "max_distance_cm": None, "signed_xyz_min_cm": None,
                "signed_xyz_max_cm": None, "max_abs_xyz_cm": None, "peak": None}
    peak = max(comparisons, key=lambda row: row["distance_cm"])
    axes = list(zip(*(row["signed_xyz_cm"] for row in comparisons)))
    return {"count": len(comparisons), "max_distance_cm": peak["distance_cm"],
            "signed_xyz_min_cm": [min(axis) for axis in axes],
            "signed_xyz_max_cm": [max(axis) for axis in axes],
            "max_abs_xyz_cm": [max(map(abs, axis)) for axis in axes], "peak": peak}


def compare(authority_segments, proxy_segments, gaps):
    """Refuse repeated same-asset passages: historical samples have no cross-peer play ID."""
    authority_by_key = {}
    for segment in authority_segments:
        authority_by_key.setdefault(segment[0].key, []).append(segment)
    proxy_counts = Counter(segment[0].key for segment in proxy_segments)
    comparisons, excluded = [], []
    for segment_index, samples in enumerate(proxy_segments):
        matches = authority_by_key.get(samples[0].key, [])
        reason = ("no_matching_authority_segment" if not matches else
                  "ambiguous_repeated_passage" if len(matches) != 1 or proxy_counts[samples[0].key] != 1 else None)
        for sample in samples:
            endpoints = None
            sample_reason = reason
            if not sample_reason:
                endpoints, sample_reason = bracket(matches[0], sample.phase)
            if sample_reason:
                excluded.append({"segment": segment_index, "proxy": sample.report(), "reason": sample_reason})
                continue
            row = residual(sample, matches[0], endpoints)
            row.update({"segment": segment_index, "period": period(sample.utc, gaps),
                        "identity": {"player_id": sample.player_id, "montage": sample.montage, "mode": sample.mode}})
            comparisons.append(row)
    return {"summary": summarize(comparisons),
            "periods": {name: summarize([row for row in comparisons if row["period"] == name])
                        for name in ("normal", "packet_gap", "post_gap")},
            "comparisons": comparisons, "excluded": excluded,
            "exclusion_counts": dict(Counter(row["reason"] for row in excluded))}


def analyze_run(run):
    run = Path(run).resolve()
    manifest = json.loads((run / "manifest.json").read_text(encoding="utf-8"))
    trigger = json.loads((run / "trigger.json").read_text(encoding="utf-8"))
    player_id, start = trigger["owner_player_id"], trigger["start_utc"]
    records = {role: [json.loads(line) for line in (run / f"{role}-samples.jsonl").read_text(encoding="utf-8").splitlines() if line.strip()]
               for role in manifest["processes"]}
    authority, discarded = segments(records["host"], player_id, start, "ROLE_AUTHORITY")
    result = {
        "contract_version": 1, "run": str(run), "process_status": manifest.get("status"),
        "driver": trigger.get("driver"), "player_id": player_id,
        "contract": "Proxy visual root minus authority visual root at equal montage phase, using an observed authority bracket; never extrapolate or pair repeated passages.",
        "limits": ["Diagnostic only: no acceptance threshold or replacement of the historical analyzer's failed results.",
                   "Mesh/component root, not bone or contact pose; no native NP payload or exact cross-peer GAS play identity.",
                   "Authority positions between samples are linear estimates; bracket phase span and nearest raw sample remain explicit.",
                   "Normal means before the first recorded packet gap (or a run without a gap). Post-gap means after receipt loss is disabled, not proof of an internal NP recovery endpoint.",
                   "Same-asset repeated passages or rewinds are excluded without a cross-peer play identity; missing coverage is not success."],
        "authority_segments": len(authority),
        "authority_discarded_counts": dict(Counter(row["reason"] for row in discarded)), "roles": {},
    }
    for role, rows in records.items():
        if role == "host":
            continue
        proxy, rejected = segments(rows, player_id, start, "ROLE_SIMULATED_PROXY")
        gaps = packet_gaps(rows, player_id)
        result["roles"][role] = {**compare(authority, proxy, gaps), "gaps": gaps,
                                 "proxy_segments": len(proxy),
                                 "discarded_counts": dict(Counter(row["reason"] for row in rejected))}
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run", type=Path)
    parser.add_argument("--output", type=Path, help="Output JSON; defaults to RUN/phase-analysis.json")
    args = parser.parse_args()
    result = analyze_run(args.run)
    output = args.output or args.run / "phase-analysis.json"
    # A fresh name preserves earlier measured evidence when the analyzer or runtime changes.
    with output.open("x", encoding="utf-8") as stream:
        stream.write(json.dumps(result, indent=2, allow_nan=False) + "\n")
    print(json.dumps({"output": str(output.resolve()), "diagnostic_only": True,
                      "roles": {role: {"periods": {name: {key: value for key, value in summary.items() if key != "peak"}
                                                    for name, summary in data["periods"].items()},
                                       "exclusion_counts": data["exclusion_counts"]}
                                for role, data in result["roles"].items()}}, indent=2))


if __name__ == "__main__":
    main()
