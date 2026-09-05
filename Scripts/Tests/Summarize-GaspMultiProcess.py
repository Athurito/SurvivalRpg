"""Summarize instrumented GASP CSV captures without turning presentation metrics into pass/fail claims."""
import argparse
import csv
import json
import math
import re
import statistics
from collections import Counter, defaultdict
from pathlib import Path


def number(value):
    try:
        result = float(value)
        return result if math.isfinite(result) else None
    except (TypeError, ValueError):
        return None


def subject_player_id(path):
    marker = path.parent / "owner.ready"
    if not marker.is_file():
        raise ValueError(f"Missing owner.ready beside {path}; use --all-pawns for an unfiltered capture")
    match = re.search(r"\bplayer_id=(-?\d+)\b", marker.read_text(encoding="utf-8-sig"))
    if not match:
        raise ValueError(f"Missing player_id in {marker}")
    return match.group(1)


def summarize(path, subject_id=None):
    groups = defaultdict(lambda: {
        "frames": 0, "dt": [], "clips": Counter(), "mm_modes": Counter(),
        "rates": [], "root_mesh_yaw": [], "offset_mesh_yaw": [], "corrections": [],
        "clip_changes": 0, "last_clip": None, "landing_ground_search_released": Counter(),
        "missing_mm_frames": 0, "per_bone_blend_profile_status": Counter(),
        "database_roles": Counter(), "search_databases": Counter(), "jump_phases": Counter(),
        "phase_times": [], "fps_limits": Counter(),
    })
    with path.open(encoding="utf-8-sig", newline="") as stream:
        for row in csv.DictReader(stream):
            if subject_id is not None and row["player_id"] != subject_id:
                continue
            # Every pawn frame has one row per BlendStack player. Count frame measurements only once.
            if row["blend_index"] not in ("0", "-1"):
                continue
            key = (row["player_id"], row["player_name"], row["local_role"], row["phase"])
            g = groups[key]
            g["frames"] += 1
            g["fps_limits"][row["fps_limit"]] += 1
            g["database_roles"][row["mm_database_role"]] += 1
            g["search_databases"][row.get("mm_search_databases", "unavailable")] += 1
            g["jump_phases"][row.get("jump_phase", "unavailable")] += 1
            phase_time = number(row["phase_seconds"])
            if phase_time is not None:
                g["phase_times"].append(phase_time)
            for column, output in (("delta_seconds", "dt"), ("play_rate", "rates"), ("client_corrections", "corrections")):
                value = number(row[column])
                if value is not None:
                    g[output].append(value)
            for column, output in (("root_yaw", "root_mesh_yaw"), ("offset_root_yaw", "offset_mesh_yaw")):
                yaw, mesh = number(row[column]), number(row["mesh_yaw"])
                if yaw is not None and mesh is not None:
                    g[output].append(abs((yaw - mesh + 180.0) % 360.0 - 180.0))
            clip = row["selected_clip"]
            g["clips"][clip] += 1
            g["mm_modes"][row["mm_interrupt"]] += 1
            if g["last_clip"] is not None and clip != g["last_clip"]:
                g["clip_changes"] += 1
            g["last_clip"] = clip
            g["missing_mm_frames"] += row["mm_node"] != "present"
            g["per_bone_blend_profile_status"][row["has_per_bone_blend_profile"]] += 1
            g["landing_ground_search_released"][row["landing_ground_search_released"]] += 1

    result = []
    for (player_id, name, role, phase), g in sorted(groups.items()):
        def maximum(values):
            return max(values) if values else None

        dt = [value for value in g.pop("dt") if value > 0.0]
        rates = g.pop("rates")
        corrections = g.pop("corrections")
        phase_times = g.pop("phase_times")
        g.pop("last_clip")
        g["max_abs_root_vs_mesh_yaw_degrees"] = maximum(g.pop("root_mesh_yaw"))
        g["max_abs_offset_root_vs_mesh_yaw_degrees"] = maximum(g.pop("offset_mesh_yaw"))
        g["fps_from_mean_delta"] = 1.0 / statistics.mean(dt) if dt else None
        g["max_frame_delta_seconds"] = maximum(dt)
        g["median_frame_delta_seconds"] = statistics.median(dt) if dt else None
        g["play_rate_min_max"] = [min(rates), max(rates)] if rates else None
        g["correction_counter_min_max"] = [min(corrections), max(corrections)] if corrections else None
        g["phase_seconds_min_max"] = [min(phase_times), max(phase_times)] if phase_times else None
        result.append(dict(player_id=player_id, player_name=name, local_role=role, phase=phase, **g))
    done = path.with_suffix(".done")
    return dict(csv=str(path), subject_player_id=subject_id,
                capture_result=done.read_text(encoding="utf-8-sig").strip() if done.is_file() else "missing_done_marker",
                groups=result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("capture_directory", type=Path)
    parser.add_argument("--all-pawns", action="store_true", help="Include host, observer and AI instead of the owner.ready subject only")
    args = parser.parse_args()
    directory = args.capture_directory.resolve(strict=True)
    files = sorted(directory.rglob("*.csv"))
    if not files:
        parser.error("No role CSV files in the capture directory")
    report = {
        "scope": "Observed instrumented frames. Requested FPS is only a cap; use measured delta. Yaw separation and clip changes are diagnostics, not defects by themselves.",
        "weight_contract": "CSV blend_scalar_weight reconstructs BlendStack's scalar player contribution. Per-bone blend profiles can produce different final bone weights.",
        "subject_contract": "Player ID from each case's owner.ready marker by default; names are not stable identities. --all-pawns also includes AI.",
        "captures": [summarize(path, None if args.all_pawns else subject_player_id(path)) for path in files],
    }
    output = directory / "summary.json"
    output.write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(output)


if __name__ == "__main__":
    main()
