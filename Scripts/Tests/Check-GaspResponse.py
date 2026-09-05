"""Check completed GASP response captures; scalar blend evidence is not a skinning verdict.

Accept one fps-* directory or its run parent. Exit 0 means every required observed
response passed, 1 means missing/failed evidence, and 2 means invalid arguments.
No Unreal process, assets, or source files are modified by this checker.

The per-role repeated_update_root_playback object reports counter availability,
same-update pairs, qualified repeated evaluations and every root-delta violation.
Only server observations contribute a playback acceptance check. Zero qualifying
pairs are reported as unexercised coverage, not a required synthetic duplicate.
Use --require-animation-counters for final captures; legacy CSVs can omit them.
"""
import argparse
import csv
import json
import math
import re
import statistics
from pathlib import Path


TURN = re.compile(r"_Turn_(045|090|135|180)_([LR])(?:[._]|$)")
TURN_PHASES = tuple(f"{mode}_{phase}" for mode in ("combat", "aim") for phase in (
    "stationary_45", "stationary_90", "stationary_180", "active_followup", "active_opposite"))
LAND_PHASES = ("run_land_stop", "run_land_walk")
REQUIRED = {"player_id", "utc_ticks", "phase", "phase_seconds", "delta_seconds", "actor_yaw",
            "mesh_yaw", "root_yaw", "speed", "acceleration", "gait", "jump_phase", "tir_state",
            "selected_clip", "selected_asset_time", "blend_index", "blend_clip",
            "blend_scalar_weight", "mm_node", "input_scale"}
ANIMATION_COUNTERS = ("anim_update_counter", "bone_revision", "engine_frame")


def number(value):
    try:
        result = float(value)
        return result if math.isfinite(result) else None
    except (TypeError, ValueError):
        return None


def yaw_delta(start, end):
    return (end - start + 180.0) % 360.0 - 180.0


def turn_angle(clip):
    match = TURN.search(clip)
    return int(match[1]) * (1 if match[2] == "R" else -1) if match else None


def short_clip(clip):
    return clip.rsplit("/", 1)[-1].split(".")[0]


def run_landing(clip):
    return "/Jump/Lands/" in clip and "_Land_Run_" in clip


def ground_idle(clip):
    # The authored PSD_Rpg_Stand_Idle also owns this exact planted-foot stand-up
    # recovery. It is valid after a bent-knee landing; other crouch transitions
    # are not idle responses. Membership and compressed pose evidence were audited.
    crouch_to_stand = (
        "/RpgGaspLocomotion/Animations/Crouch/Transitions/M_Neutral_Transition_Crouch_to_Stand."
        "M_Neutral_Transition_Crouch_to_Stand")
    return clip == crouch_to_stand or ("/Stand/Idle/" in clip and turn_angle(clip) is None)


def ground_walk(clip):
    # Walk includes authored Run-to-Walk transitions, but a braking pose is not a Walk response.
    return "/Stand/Walk/" in clip and "/Stops/" not in clip and "_Stop_" not in short_clip(clip)


def ground_clip(clip):
    return ground_idle(clip) or ("/Stand/" in clip and turn_angle(clip) is None)


def frame_evidence(frame):
    return {"csv_line": frame["line"], "phase_seconds": frame["t"],
            "actor_yaw": frame["actor_yaw"], "root_gap_degrees": root_gap(frame),
            "state": frame["tir_state"], "clip": short_clip(frame["selected_clip"]),
            "asset_time": frame["selected_asset_time"]}


def root_gap(frame):
    # Both values are in the skeletal mesh's world-facing domain; no hardcoded Manny -90.
    root = frame.get("offset_root_yaw")
    root = frame["root_yaw"] if root is None else root
    return yaw_delta(root, frame["mesh_yaw"])


def load_frames(path, subject):
    frames = []
    with path.open(encoding="utf-8-sig", newline="") as stream:
        reader = csv.DictReader(stream)
        missing = REQUIRED - set(reader.fieldnames or ())
        if missing:
            raise ValueError(f"{path.name}: missing columns {sorted(missing)}")
        current = None
        for row in reader:
            if row["player_id"] != subject:
                continue
            key = row["utc_ticks"]
            if current is None or key != current["key"]:
                current = dict(row, key=key, line=reader.line_num, blends=[])
                for column in ("phase_seconds", "delta_seconds", "actor_yaw", "mesh_yaw", "root_yaw",
                               "offset_root_yaw", "speed", "acceleration", "selected_asset_time", "input_scale"):
                    current[column] = number(row.get(column))
                for column in ("phase_seconds", "delta_seconds", "actor_yaw", "mesh_yaw", "root_yaw",
                               "speed", "acceleration", "selected_asset_time"):
                    if current[column] is None:
                        raise ValueError(f"{path.name}:{reader.line_num}: unavailable/nonfinite {column}")
                current["t"] = current["phase_seconds"]
                frames.append(current)
            weight = number(row["blend_scalar_weight"])
            if row["blend_index"] != "-1" and (weight is None or not -1e-5 <= weight <= 1.00001):
                raise ValueError(f"{path.name}:{reader.line_num}: invalid BlendStack scalar weight")
            if row["blend_index"] != "-1" and weight is not None:
                current["blends"].append((row["blend_clip"], weight))
    return frames


def two_frames(frames, start):
    """Use the next two actual observed frame intervals, including a captured hitch."""
    after = [f for f in frames if f["t"] > start + 1e-6][:2]
    if len(after) != 2:
        return None
    return after[-1]["t"] - start


def blend_weight(frame, predicate):
    return sum(weight for clip, weight in frame["blends"] if predicate(clip))


def add(checks, scope, name, passed, **evidence):
    checks.append(dict(scope=scope, check=name, passed=bool(passed), **evidence))


def source_default(repo, field):
    path = repo / "Source/SurvivalRpg/Animation/RpgGaspLocomotionConfig.h"
    match = re.search(r"\bfloat\s+" + re.escape(field) + r"\s*=\s*([\d.]+)f", path.read_text())
    if not match:
        raise ValueError(f"Cannot read native default {field} from {path}")
    return float(match[1])


def infer_mesh_basis(frames, options):
    """Infer actor-to-mesh yaw only from an observed stable phase tail.

    A replicated actor can reach its new yaw before network smoothing moves the
    mesh there. The basis must therefore come from settled observations, never
    from the actor/mesh difference on the input-event frame.
    """
    end = frames[-1]["t"] if frames else 0.0
    tail_start = next((i for i, f in enumerate(frames) if f["t"] >= end - options.settle_window_seconds), 0)
    required_span = options.settle_window_seconds * 0.75
    # Basis inference may include a preceding stationary sample at sparse frame rates.
    # This never extends the separate response or settled-root acceptance windows.
    while tail_start > 0 and (len(frames) - tail_start < 3 or end - frames[tail_start]["t"] + 1e-6 < required_span):
        tail_start -= 1
    tail = frames[tail_start:]
    result = {"available": False, "samples": len(tail), "basis_yaw_degrees": None,
              "required_span_seconds": required_span,
              "basis_tolerance_degrees": 0.1, "conflicting_stable_windows": []}
    span = tail[-1]["t"] - tail[0]["t"] if tail else 0.0
    result["span_seconds"] = span
    if len(tail) < 3 or span + 1e-6 < result["required_span_seconds"]:
        result["reason"] = "Insufficient stable-tail coverage to infer the mesh basis"
        return result
    bases = [yaw_delta(f["actor_yaw"], f["mesh_yaw"]) for f in tail]
    basis = yaw_delta(0.0, bases[0] + statistics.mean(yaw_delta(bases[0], value) for value in bases))
    actor_drift = max(abs(yaw_delta(tail[0]["actor_yaw"], f["actor_yaw"])) for f in tail)
    basis_drift = max(abs(yaw_delta(basis, value)) for value in bases)
    result.update(actor_drift_degrees=actor_drift, basis_drift_degrees=basis_drift,
                  csv_lines=[tail[0]["line"], tail[-1]["line"]])
    if actor_drift > 0.1 or basis_drift > result["basis_tolerance_degrees"]:
        result["reason"] = "Actor or relative mesh yaw is still changing in the phase tail"
        return result
    # Reject an actual basis change within this phase. Short earlier plateaus count
    # only when both actor and relative mesh yaw are nearly constant, excluding the
    # visibly moving part of network smoothing. No model of smoothing speed is assumed.
    for index, start in enumerate(frames):
        stop = next((i for i in range(index + 1, len(frames)) if frames[i]["t"] - start["t"] >= 0.1), None)
        if stop is None or stop - index < 2:
            continue
        window = frames[index:stop + 1]
        first_basis = yaw_delta(start["actor_yaw"], start["mesh_yaw"])
        stable = all(abs(yaw_delta(start["actor_yaw"], f["actor_yaw"])) <= 0.01 and
                     abs(yaw_delta(first_basis, yaw_delta(f["actor_yaw"], f["mesh_yaw"]))) <= 0.01
                     for f in window)
        if stable and abs(yaw_delta(basis, first_basis)) > result["basis_tolerance_degrees"]:
            result["conflicting_stable_windows"].append({
                "csv_lines": [start["line"], frames[stop]["line"]], "basis_yaw_degrees": first_basis})
    if result["conflicting_stable_windows"]:
        result["reason"] = "Different stable actor-to-mesh bases were observed within the phase"
        return result
    result.update(available=True, basis_yaw_degrees=basis)
    return result


def check_opposite_turn_response(checks, scope, frames, old, changed, delta, options):
    """Require a counterclip, or measured sub-activation recovery of the actual root gap.

    The runtime intentionally recovers opposite residuals below TurnActivationThreshold.
    State alone is insufficient: the old turn must exit, world root yaw must progress
    toward the unchanged target, and the residual must reach TurnCancelThreshold.
    """
    after = [f for f in frames if f["t"] >= changed["t"]]
    observed_two = two_frames(frames, changed["t"])
    response_bound = max(options.turn_response_seconds, options.blend_seconds + (observed_two or 0))
    basis = infer_mesh_basis(frames, options)
    if not basis["available"]:
        add(checks, scope, "opposite_turn_selection_response", False,
            requested_delta_degrees=delta, bound_seconds=response_bound, mesh_basis=basis,
            reason="The new actor target cannot be mapped to a proven stable mesh basis")
        return
    target_yaw = yaw_delta(0.0, changed["actor_yaw"] + basis["basis_yaw_degrees"])
    def root_yaw(f):
        return f["root_yaw"] if f.get("offset_root_yaw") is None else f["offset_root_yaw"]
    def target_gap(f):
        return yaw_delta(root_yaw(f), target_yaw)
    def actor_drift_until(end_frame):
        return max((abs(yaw_delta(changed["actor_yaw"], f["actor_yaw"]))
                    for f in after if end_frame and f["t"] <= end_frame["t"]), default=None)
    gap = target_gap(changed)
    direction = 1 if gap > 0 else -1

    def is_counterclip(f):
        angle = turn_angle(f["selected_clip"])
        new_playback = f["selected_clip"] != old["selected_clip"] or (
            f["selected_asset_time"] + 1e-4 < old["selected_asset_time"])
        return (f["tir_state"] == "Active" and angle is not None
                and angle * direction > 0 and new_playback)

    counterclip = next((f for f in after if is_counterclip(f)), None)
    counter_target_drift = actor_drift_until(counterclip)
    clip_passed = counterclip is not None and observed_two is not None \
        and abs(gap) >= options.root_gap_degrees \
        and counterclip["t"] - changed["t"] <= response_bound + 1e-6 \
        and counter_target_drift is not None and counter_target_drift <= 1.0
    evidence = dict(requested_delta_degrees=delta, actual_gap_at_request=gap,
                    target_mesh_yaw_degrees=target_yaw, mesh_basis=basis,
                    actor_target_drift_degrees=counter_target_drift,
                    activation_degrees=options.turn_activation_degrees,
                    selected=frame_evidence(counterclip) if counterclip else None,
                    latency_seconds=round(counterclip["t"] - changed["t"], 6) if counterclip else None,
                    bound_seconds=response_bound, response_contract="counterclip")
    recovery_passed = False
    if not clip_passed and options.root_gap_degrees <= abs(gap) < options.turn_activation_degrees:
        def is_recovery_pose(f):
            return f["tir_state"] in ("Recovering", "Inactive") and turn_angle(f["selected_clip"]) is None

        release = next((f for f in after if is_recovery_pose(f)), None)
        release_two = two_frames(frames, release["t"]) if release else None
        recovery_bound = max(options.turn_recovery_seconds, options.blend_seconds) + (release_two or 0)
        recovery = [f for f in after if release and f["t"] >= release["t"]]
        converged = next((f for f in recovery if abs(target_gap(f)) <= options.root_gap_degrees), None)
        measured = [f for f in recovery if converged and f["t"] <= converged["t"]]
        # Include the preceding observation to measure the root movement on the release frame.
        before_release = next((f for f in reversed(after) if release and f["t"] < release["t"]), changed)
        motion = [before_release] + measured
        deltas = [direction * yaw_delta(root_yaw(a), root_yaw(b)) for a, b in zip(motion, motion[1:])]
        progress = sum(deltas)
        wrong_direction = [dict(csv_lines=[a["line"], b["line"]], signed_root_delta_degrees=round(step, 6))
                           for a, b, step in zip(motion, motion[1:], deltas) if step < -0.1]
        target_drift = actor_drift_until(converged)
        release_in_time = release is not None and observed_two is not None \
            and release["t"] - changed["t"] <= response_bound + 1e-6
        convergence_in_time = converged is not None and release_two is not None \
            and converged["t"] - release["t"] <= recovery_bound + 1e-6
        recovery_passed = release_in_time and convergence_in_time and bool(measured) \
            and all(is_recovery_pose(f) for f in measured) and progress > 0.1 \
            and not wrong_direction and target_drift is not None and target_drift <= 1.0
        evidence.update(response_contract="sub_activation_recovery", recovery={
            "release": frame_evidence(release) if release else None,
            "release_latency_seconds": round(release["t"] - changed["t"], 6) if release else None,
            "release_within_response_budget": release_in_time,
            "converged": frame_evidence(converged) if converged else None,
            "converged_target_gap_degrees": target_gap(converged) if converged else None,
            "seconds_from_release_to_convergence": round(converged["t"] - release["t"], 6) if converged else None,
            "recovery_blend_budget_seconds": recovery_bound,
            "convergence_within_budget": convergence_in_time,
            "directed_root_progress_degrees": round(progress, 6),
            "wrong_direction_steps": wrong_direction,
            "actor_target_drift_degrees": target_drift,
            "measured_samples": len(measured),
            "non_turn_selection_sustained": bool(measured) and all(is_recovery_pose(f) for f in measured),
        })
    add(checks, scope, "opposite_turn_selection_response", clip_passed or recovery_passed, **evidence)


def check_turn(checks, scope, frames, previous, options):
    # Each peer anchors to its observed actor rotation, never its local driver's stale input_yaw.
    changes = []
    prior = previous
    for frame in frames:
        if prior is not None:
            delta = yaw_delta(prior["actor_yaw"], frame["actor_yaw"])
            if abs(delta) >= options.yaw_event_degrees:
                changes.append((prior, frame, delta))
        prior = frame
    add(checks, scope, "observed_yaw_request", bool(changes),
        events=[dict(delta_degrees=round(delta, 3), **frame_evidence(f)) for _, f, delta in changes])
    if not changes:
        return
    before_request = changes[0][0]
    initial = changes[0][1]
    active = next((f for f in frames if f["t"] >= initial["t"] and f["tir_state"] == "Active"
                   and turn_angle(f["selected_clip"]) is not None
                   and (before_request["tir_state"] != "Active"
                        or f["selected_clip"] != before_request["selected_clip"]
                        or f["selected_asset_time"] + 1e-4 < before_request["selected_asset_time"])), None)
    bound_frames = two_frames(frames, initial["t"])
    bound = options.turn_response_seconds + (bound_frames or 0)
    add(checks, scope, "first_turn_dispatch", active is not None and bound_frames is not None
        and active["t"] - initial["t"] <= bound + 1e-6,
        request=frame_evidence(initial), selected=frame_evidence(active) if active else None,
        latency_seconds=round(active["t"] - initial["t"], 6) if active else None, bound_seconds=bound)
    if "stationary_" in scope and active:
        nominal = int(scope.rsplit("_", 1)[1])
        actual = turn_angle(active["selected_clip"])
        expected_sign = 1 if changes[0][2] > 0 else -1
        add(checks, scope, "first_turn_nominal_angle", abs(actual) == nominal
            and (nominal == 180 or actual * expected_sign > 0),
            expected_magnitude=nominal, selected_angle=actual,
            direction_contract="180 L/R endpoints are equivalent; no forced side", selected=frame_evidence(active))
    if "active_" in scope:
        followup = next((event for event in changes[1:]
                         if event[1]["t"] - initial["t"] >= 0.15), None)
        add(checks, scope, "followup_arrived_during_turn", followup is not None
            and followup[0]["tir_state"] == "Active" and turn_angle(followup[0]["selected_clip"]) is not None,
            prior=frame_evidence(followup[0]) if followup else None,
            changed=frame_evidence(followup[1]) if followup else None,
            observed_delta_degrees=followup[2] if followup else None)
        if followup and scope.endswith("active_opposite"):
            old, changed, delta = followup
            check_opposite_turn_response(checks, scope, frames, old, changed, delta, options)
    end = frames[-1]["t"]
    settled = [f for f in frames if f["t"] >= end - options.settle_window_seconds]
    actor_drift = max(abs(yaw_delta(settled[0]["actor_yaw"], f["actor_yaw"])) for f in settled)
    gap_max = max(abs(root_gap(f)) for f in settled)
    add(checks, scope, "settled_root_convergence", len(settled) >= 3
        and settled[-1]["t"] - settled[0]["t"] >= options.settle_window_seconds * 0.75
        and end - changes[-1][1]["t"] >= options.settle_window_seconds
        and actor_drift <= 1.0 and gap_max <= options.root_gap_degrees,
        sampled_window_seconds=round(end - settled[0]["t"], 6), samples=len(settled),
        actor_drift_degrees=round(actor_drift, 3), max_root_gap_degrees=round(gap_max, 3),
        allowed_root_gap_degrees=options.root_gap_degrees, last=frame_evidence(frames[-1]))
    restarts = []
    for prior, frame in zip(frames, frames[1:]):
        if (frame["t"] >= settled[0]["t"] and frame["tir_state"] == "Active"
                and turn_angle(frame["selected_clip"]) is not None
                and (prior["tir_state"] != "Active" or prior["selected_clip"] != frame["selected_clip"]
                     or frame["selected_asset_time"] + 1e-4 < prior["selected_asset_time"])):
            restarts.append(frame_evidence(frame))
    add(checks, scope, "settled_input_does_not_retrigger_turns", not restarts and actor_drift <= 1.0,
        actor_drift_degrees=round(actor_drift, 3), new_turn_playbacks=restarts)


def check_landing(checks, scope, frames, role, options):
    landing = next((f for f in frames if run_landing(f["selected_clip"])
                    and f["jump_phase"] == "Landing"), None)
    stop = scope.endswith("run_land_stop")
    def changed_input(f):
        if f["jump_phase"] == "Airborne":
            return False
        if stop:
            return f["input_scale"] == 0 if role == "owner" else f["acceleration"] <= options.velocity_tolerance
        return f["gait"] == "Walk"
    change = next((f for f in frames if changed_input(f)), None)
    add(checks, scope, "input_changes_after_selected_run_landing", landing is not None and change is not None
        and change["t"] > landing["t"], landing=frame_evidence(landing) if landing else None,
        change=frame_evidence(change) if change else None,
        change_observation="owner input_scale" if role == "owner" and stop else "peer acceleration/gait")
    if landing is None or change is None or change["t"] <= landing["t"]:
        return
    target = ground_clip if stop else ground_walk
    selected = next((f for f in frames if f["t"] >= change["t"] and target(f["selected_clip"])), None)
    observed_two = two_frames(frames, change["t"])
    selection_bound = (observed_two or 0) + options.search_throttle_seconds
    add(checks, scope, "ground_selection_after_input_change", selected is not None and observed_two is not None
        and selected["t"] - change["t"] <= selection_bound + 1e-6,
        selected=frame_evidence(selected) if selected else None,
        latency_seconds=round(selected["t"] - change["t"], 6) if selected else None,
        bound_seconds=selection_bound)
    if not stop:
        majority = next((f for f in frames if f["t"] >= change["t"] and blend_weight(f, ground_walk) > 0.5), None)
        bound = options.blend_seconds + (observed_two or 0) + options.search_throttle_seconds
        add(checks, scope, "walk_scalar_blend_majority", majority is not None and observed_two is not None
            and majority["t"] - change["t"] <= bound + 1e-6,
            majority=frame_evidence(majority) if majority else None,
            latency_seconds=round(majority["t"] - change["t"], 6) if majority else None, bound_seconds=bound)
        add(checks, scope, "walk_response_has_ground_velocity", selected is not None and majority is not None
            and all(f["jump_phase"] != "Airborne" and f["speed"] > options.velocity_tolerance
                    for f in (selected, majority)),
            speed_tolerance_cm_s=options.velocity_tolerance,
            selected_speed_cm_s=selected["speed"] if selected else None,
            majority_speed_cm_s=majority["speed"] if majority else None,
            selected=frame_evidence(selected) if selected else None,
            majority=frame_evidence(majority) if majority else None)
        return
    stopped = next((f for f in frames if f["t"] >= change["t"]
                    and f["speed"] <= options.velocity_tolerance and f["jump_phase"] != "Airborne"), None)
    add(checks, scope, "cmc_reaches_idle_speed", stopped is not None,
        speed_tolerance_cm_s=options.velocity_tolerance, stopped=frame_evidence(stopped) if stopped else None)
    if stopped is None:
        return
    observed_two = two_frames(frames, stopped["t"])
    bound = (observed_two or 0) + options.search_throttle_seconds
    idle = next((f for f in frames if f["t"] >= stopped["t"] and ground_idle(f["selected_clip"])), None)
    add(checks, scope, "stop_selection_exits_at_idle_speed", idle is not None and observed_two is not None
        and idle["t"] - stopped["t"] <= bound + 1e-6,
        previous_clip=short_clip(stopped["selected_clip"]), idle=frame_evidence(idle) if idle else None,
        latency_seconds=round(idle["t"] - stopped["t"], 6) if idle else None, bound_seconds=bound)
    majority = next((f for f in frames if f["t"] >= stopped["t"] and blend_weight(f, ground_idle) > 0.5), None)
    blend_bound = options.blend_seconds + bound
    add(checks, scope, "idle_scalar_blend_majority", majority is not None and observed_two is not None
        and majority["t"] - stopped["t"] <= blend_bound + 1e-6,
        majority=frame_evidence(majority) if majority else None,
        latency_seconds=round(majority["t"] - stopped["t"], 6) if majority else None, bound_seconds=blend_bound)

    # Preserve the full transition budget, then inspect every remaining frame. An initial
    # Idle sample must not hide a later return to a Stop continuing pose at standstill.
    # This phase's contract ends at the next observed movement request or airborne state.
    sustained = []
    window_end = "end_of_phase"
    for frame in frames:
        if frame["t"] < stopped["t"]:
            continue
        if not changed_input(frame):
            window_end = "next_movement_request_or_airborne"
            break
        if frame["t"] + 1e-6 >= stopped["t"] + blend_bound:
            sustained.append(frame)
    window = dict(samples=len(sustained), transition_budget_seconds=blend_bound, window_end=window_end,
                  first=frame_evidence(sustained[0]) if sustained else None,
                  last=frame_evidence(sustained[-1]) if sustained else None)
    wrong_selection = [frame_evidence(f) for f in sustained if not ground_idle(f["selected_clip"])]
    wrong_weight = [dict(frame_evidence(f), idle_scalar_weight=blend_weight(f, ground_idle))
                    for f in sustained if blend_weight(f, ground_idle) <= 0.5]
    add(checks, scope, "idle_selection_sustained_until_input_change",
        observed_two is not None and bool(sustained) and not wrong_selection,
        **window, violation_count=len(wrong_selection), first_violations=wrong_selection[:8])
    add(checks, scope, "idle_scalar_majority_sustained_until_input_change",
        observed_two is not None and bool(sustained) and not wrong_weight,
        **window, violation_count=len(wrong_weight), first_violations=wrong_weight[:8])


def check_repeated_update_root_playback(checks, role, frames, options):
    """Measure repeated evaluations without confusing them with new animation updates.

    Actor stillness includes location (0.01 cm) and yaw (0.01 degrees). The
    selected clip and asset time (0.00001 seconds) must also be unchanged.
    Counter availability is mandatory when columns exist or explicitly required.
    A 0.1-degree root tolerance excludes float conversion noise; it does not
    relax any turn-response, convergence or selection threshold.
    """
    samples = [f for f in frames if f["phase"].startswith(("before_late/", "after_late/"))]
    diagnostic = {"status": "unavailable", "frames": len(samples), "available_frames": 0,
                  "counter_columns": list(ANIMATION_COUNTERS), "same_update_pairs": 0,
                  "qualified_repeated_evaluations": 0, "max_abs_root_delta_degrees": None,
                  "root_tolerance_degrees": 0.1, "segments": {}, "violations": [],
                  "contract": "Same phase, animation update counter, selected clip and asset time; "
                  "actor location/yaw stationary; bone revision and engine frame both advance. "
                  "Only server root changes above 0.1 degrees fail playback acceptance. "
                  "No qualifying observation means this contract was not exercised."}
    valid = []
    for frame in samples:
        try:
            counters = tuple(int(frame.get(key, "unavailable")) for key in ANIMATION_COUNTERS)
            position = tuple(number(frame.get(key)) for key in ("actor_x", "actor_y", "actor_z"))
            if any(value < 0 for value in counters) or any(value is None for value in position) or frame.get("offset_root_yaw") is None:
                raise ValueError("Unavailable counter, position or offset root")
            valid.append((frame, counters, position))
        except (TypeError, ValueError):
            valid.append(None)
    diagnostic["available_frames"] = sum(item is not None for item in valid)
    available = bool(samples) and diagnostic["available_frames"] == len(samples)
    has_counter_columns = any(any(key in f for key in ANIMATION_COUNTERS) for f in samples)
    if options.require_animation_counters or has_counter_columns:
        add(checks, role, "animation_counter_evidence_available", available,
            available_frames=diagnostic["available_frames"], required_frames=len(samples),
            missing_columns=[key for key in ANIMATION_COUNTERS if not any(key in f for f in samples)],
            explicitly_required=options.require_animation_counters)
    if not available:
        diagnostic["reason"] = "Legacy missing columns or incomplete/nonfinite counter, offset-root or actor-position evidence"
        return diagnostic
    maximum = 0.0
    for before, after in zip(valid, valid[1:]):
        old, old_counters, old_position = before
        new, new_counters, new_position = after
        if old["phase"] != new["phase"]:
            continue
        segment = new["phase"].split("/")[0]
        coverage = diagnostic["segments"].setdefault(segment, {
            "frame_pairs": 0, "same_update_pairs": 0, "qualified_repeated_evaluations": 0, "violations": 0})
        coverage["frame_pairs"] += 1
        if old_counters[0] != new_counters[0]:
            continue
        diagnostic["same_update_pairs"] += 1
        coverage["same_update_pairs"] += 1
        if (old["selected_clip"] != new["selected_clip"] or new["mm_node"] != "present"
                or abs(new["selected_asset_time"] - old["selected_asset_time"]) > 0.00001
                or abs(yaw_delta(old["actor_yaw"], new["actor_yaw"])) > 0.01
                or sum((a - b) ** 2 for a, b in zip(old_position, new_position)) > 0.01 ** 2
                or new_counters[1] <= old_counters[1] or new_counters[2] <= old_counters[2]):
            continue
        diagnostic["qualified_repeated_evaluations"] += 1
        coverage["qualified_repeated_evaluations"] += 1
        delta = yaw_delta(old["offset_root_yaw"], new["offset_root_yaw"])
        maximum = max(maximum, abs(delta))
        if abs(delta) > diagnostic["root_tolerance_degrees"]:
            coverage["violations"] += 1
            diagnostic["violations"].append({
                "phase": new["phase"], "csv_lines": [old["line"], new["line"]],
                "phase_seconds": [old["t"], new["t"]], "anim_update_counter": new_counters[0],
                "bone_revisions": [old_counters[1], new_counters[1]],
                "engine_frames": [old_counters[2], new_counters[2]],
                "clip": short_clip(new["selected_clip"]),
                "asset_times": [old["selected_asset_time"], new["selected_asset_time"]],
                "offset_root_yaws": [old["offset_root_yaw"], new["offset_root_yaw"]],
                "root_delta_degrees": round(delta, 6),
            })
    observations = diagnostic["qualified_repeated_evaluations"]
    diagnostic["status"] = "observed" if observations else "available_no_qualifying_observation"
    diagnostic["max_abs_root_delta_degrees"] = round(maximum, 6) if observations else None
    diagnostic["violation_count"] = len(diagnostic["violations"])
    if role == "server" and observations:
        add(checks, role, "repeated_update_root_playback", not diagnostic["violations"],
            qualified_repeated_evaluations=observations,
            max_abs_root_delta_degrees=diagnostic["max_abs_root_delta_degrees"],
            tolerance_degrees=diagnostic["root_tolerance_degrees"],
            violation_count=diagnostic["violation_count"],
            evidence_location="roles.server.repeated_update_root_playback.violations")
    return diagnostic


def check_case(directory, options):
    checks = []
    result = {"directory": str(directory), "checks": checks, "roles": {}}
    marker = directory / "owner.ready"
    match = re.search(r"\bplayer_id=(-?\d+)\b", marker.read_text(encoding="utf-8-sig")) if marker.is_file() else None
    if not match:
        add(checks, "capture", "owner_subject_available", False, reason="Missing owner.ready player_id")
        return result
    subject = result["subject_player_id"] = match[1]
    for role in ("owner", "server", "late"):
        done = directory / f"{role}.done"
        status = done.read_text(encoding="utf-8-sig").strip() if done.is_file() else "missing"
        add(checks, role, "capture_completed", status == "success", marker_status=status)
        try:
            frames = load_frames(directory / f"{role}.csv", subject)
        except (OSError, ValueError) as error:
            add(checks, role, "read_subject_frames", False, reason=str(error))
            continue
        deltas = [f["delta_seconds"] for f in frames if f["delta_seconds"] > 0]
        result["roles"][role] = dict(frames=len(frames), measured_fps=1 / statistics.mean(deltas) if deltas else None,
                                   max_delta_seconds=max(deltas) if deltas else None)
        result["roles"][role]["repeated_update_root_playback"] = check_repeated_update_root_playback(
            checks, role, frames, options)
        feedback = [str(f.get("tir_root_feedback", "unavailable")).lower() for f in frames]
        available = sum(value in ("true", "false", "1", "0") for value in feedback)
        turn_feedback = [str(f.get("tir_root_feedback", "unavailable")).lower() for f in frames
                         if f["tir_state"] in ("Active", "Collecting")]
        turn_available = sum(value in ("true", "false", "1", "0") for value in turn_feedback)
        result["roles"][role]["root_feedback_diagnostic"] = {
            "available_frames": available,
            "availability_percent": 100 * available / len(frames) if frames else None,
            "active_or_collecting_frames": len(turn_feedback),
            "turn_frame_availability_percent": 100 * turn_available / len(turn_feedback) if turn_feedback else None,
            "enabled_percent_of_available_turn_frames": 100 * sum(value in ("true", "1") for value in turn_feedback)
            / turn_available if turn_available else None,
            "contract": "Optional diagnostic; legacy captures without this column do not fail on its absence.",
        }
        by_phase = {}
        previous = {}
        last = None
        for frame in frames:
            phase = frame["phase"]
            if phase not in by_phase:
                by_phase[phase] = []
                previous[phase] = last if last and last["phase"].split("/")[0] == phase.split("/")[0] else None
            by_phase[phase].append(frame)
            last = frame
        for segment in (("after_late",) if role == "late" else ("before_late", "after_late")):
            for name in TURN_PHASES + LAND_PHASES:
                phase = f"{segment}/{name}"
                scope = f"{role}/{phase}"
                samples = by_phase.get(phase, [])
                if not samples:
                    add(checks, scope, "required_phase_observed", False, reason="No subject frames")
                    continue
                add(checks, scope, "motion_matching_available", all(f["mm_node"] == "present" for f in samples),
                    missing_frames=sum(f["mm_node"] != "present" for f in samples))
                if name in TURN_PHASES:
                    check_turn(checks, scope, samples, previous[phase], options)
                else:
                    check_landing(checks, scope, samples, role, options)
    return result


def markdown(report):
    lines = ["# GASP response evidence", "", f"**{'PASS' if report['passed'] else 'FAIL'}** — "
             f"{report['passed_checks']}/{report['total_checks']} checks passed.", "", report["scope"], "",
             "Bounds and every measurement, including failures, are retained in response-check.json.", ""]
    for case in report["captures"]:
        lines.extend([f"## {Path(case['directory']).name}", "", f"Subject player ID: {case.get('subject_player_id', 'missing')}", "",
                      "| Role | Measured FPS | Max frame seconds | Turn feedback available / enabled |",
                      "| --- | ---: | ---: | --- |"])
        for role, measurements in case["roles"].items():
            fps = measurements["measured_fps"]
            maximum = measurements["max_delta_seconds"]
            feedback = measurements["root_feedback_diagnostic"]
            availability = feedback["turn_frame_availability_percent"]
            enabled = feedback["enabled_percent_of_available_turn_frames"]
            feedback_text = (f"{availability:.1f}% / {enabled:.1f}%" if enabled is not None
                             else "unavailable (optional)")
            lines.append(f"| {role} | {fps:.2f} | {maximum:.4f} | {feedback_text} |" if fps and maximum
                         else f"| {role} | unavailable | unavailable | {feedback_text} |")
        lines.extend(["", "| Repeated-update evidence | Availability | Same-update pairs | Qualified evaluations | Root violations |",
                      "| --- | --- | ---: | ---: | ---: |"])
        for role, measurements in case["roles"].items():
            replay = measurements["repeated_update_root_playback"]
            lines.append(f"| {role} | {replay['status']} ({replay['available_frames']}/{replay['frames']}) | "
                         f"{replay['same_update_pairs']} | {replay['qualified_repeated_evaluations']} | "
                         f"{replay.get('violation_count', 'unavailable')} |")
        lines.extend(["", "Zero qualified evaluations means no duplicate was observed; it is not a replay-proof pass. "
                      "Every violation retains CSV lines, frame/bone/update counters and measured root delta in JSON."])
        failures = [check for check in case["checks"] if not check["passed"]]
        lines.extend(["", "| Failed scope | Contract | Evidence |", "| --- | --- | --- |"] if failures else ["", "All required observed contracts passed."])
        for check in failures:
            facts = {key: value for key, value in check.items() if key not in ("scope", "check", "passed", "events")}
            lines.append(f"| {check['scope']} | {check['check']} | {json.dumps(facts, ensure_ascii=False).replace('|', '/')} |")
        lines.append("")
    return "\n".join(lines)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_directory", type=Path)
    parser.add_argument("--output-directory", type=Path, help="Defaults to the input capture directory")
    parser.add_argument("--stdout-only", action="store_true", help="Print JSON; write no evidence files")
    parser.add_argument("--require-animation-counters", action="store_true",
                        help="Final capture acceptance: require complete animation update, bone revision and engine frame evidence")
    parser.add_argument("--velocity-tolerance", type=float, help="Override native ChooserVelocityTolerance if the capture used different tuning")
    parser.add_argument("--root-gap-degrees", type=float, help="Defaults to native TurnCancelThreshold")
    parser.add_argument("--turn-activation-degrees", type=float, help="Defaults to native TurnActivationThreshold; smaller opposite residuals may use measured recovery")
    parser.add_argument("--turn-recovery-seconds", type=float, help="Defaults to native TurnRecoveryDuration; recovery must converge within max(recovery, blend) plus two measured frames")
    parser.add_argument("--blend-seconds", type=float, default=0.2, help="Pilot MM blend contract: 0.2 seconds")
    parser.add_argument("--search-throttle-seconds", type=float, default=0.0, help="Active pilot MM search throttle; default zero")
    parser.add_argument("--turn-response-seconds", type=float, default=0.3)
    parser.add_argument("--settle-window-seconds", type=float, default=0.4)
    parser.add_argument("--yaw-event-degrees", type=float, default=5.0, help="Minimum observed actor step; all detected events remain in evidence")
    options = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    try:
        if options.velocity_tolerance is None:
            options.velocity_tolerance = source_default(repo, "ChooserVelocityTolerance")
        if options.root_gap_degrees is None:
            options.root_gap_degrees = source_default(repo, "TurnCancelThreshold")
        if options.turn_activation_degrees is None:
            options.turn_activation_degrees = source_default(repo, "TurnActivationThreshold")
        if options.turn_recovery_seconds is None:
            options.turn_recovery_seconds = source_default(repo, "TurnRecoveryDuration")
        for name in ("velocity_tolerance", "root_gap_degrees", "blend_seconds", "search_throttle_seconds",
                     "turn_activation_degrees", "turn_recovery_seconds", "turn_response_seconds", "settle_window_seconds", "yaw_event_degrees"):
            value = getattr(options, name)
            if not math.isfinite(value) or value < 0 or (name in ("settle_window_seconds", "yaw_event_degrees") and value == 0):
                parser.error(f"Invalid {name}: {value}")
        if options.turn_activation_degrees <= options.root_gap_degrees:
            parser.error("turn_activation_degrees must exceed root_gap_degrees")
        directory = options.capture_directory.resolve(strict=True)
        if not directory.is_dir():
            parser.error("capture_directory must be a directory")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    cases = [directory] if (directory / "owner.ready").is_file() else sorted({p.parent for p in directory.glob("fps-*/owner.ready")})
    if not cases:
        cases = [directory]  # Missing capture evidence is a failed acceptance, never an empty success.
    report = {"scope": "Observed subject animation selection, peer-local movement response and world root offsets. "
              "BlendStack scalar contribution does not prove per-bone skinning, foot contact quality or visual approval. "
              "Held Run landing duration alone is intentionally not a failure. Missing evidence fails acceptance.",
              "configuration": {key: value for key, value in vars(options).items()
                                if key not in ("capture_directory", "output_directory", "stdout_only")},
              "configuration_sources": ["Source/SurvivalRpg/Animation/RpgGaspLocomotionConfig.h: ChooserVelocityTolerance / TurnCancelThreshold / TurnActivationThreshold / TurnRecoveryDuration",
                                        "Source/SurvivalRpg/Animation/RpgTurnInPlaceRuntime.cpp: Active counter-input requests at activation, otherwise recovery",
                                        "Source/SurvivalRpgEditor/Private/Animation/RpgGaspPilotAssetTests.cpp: MM BlendTime 0.2 contract",
                                        "Search throttle is an explicit capture assumption/CLI override; CSV does not serialize asset tuning"],
              "captures": [check_case(case, options) for case in cases]}
    checks = [check for case in report["captures"] for check in case["checks"]]
    report["total_checks"] = len(checks)
    report["passed_checks"] = sum(check["passed"] for check in checks)
    report["passed"] = bool(checks) and report["passed_checks"] == len(checks)
    serialized = json.dumps(report, indent=2, ensure_ascii=False, allow_nan=False)
    if options.stdout_only:
        print(serialized)
    else:
        output = options.output_directory or directory
        output.mkdir(parents=True, exist_ok=True)
        (output / "response-check.json").write_text(serialized + "\n", encoding="utf-8")
        (output / "response-check.md").write_text(markdown(report), encoding="utf-8")
        print(f"{'PASS' if report['passed'] else 'FAIL'}: {report['passed_checks']}/{len(checks)}; {output / 'response-check.json'}; {output / 'response-check.md'}")
    return 0 if report["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
