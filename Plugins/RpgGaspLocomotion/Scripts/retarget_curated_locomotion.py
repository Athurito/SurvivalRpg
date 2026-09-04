"""Retarget curated poses in an isolated Unreal Editor scripting session.

run("D:/Repos/GameAnimationSample/Content", target_packages=[...], apply=True)
Defaults to a dry run over the manifest. Candidates and source assets are never
saved; apply copies non-root tracks into existing selected targets, preserving
their asset identity and gameplay metadata. A JSON audit is written under Saved.
Requires the built SurvivalRpgEditor RpgAnimationRetargetLibrary API.
"""

import csv
import hashlib
import json
import math
from pathlib import Path
import uuid

import unreal


SOURCE_ROOT = "/Game/Characters/"
STAGING_ROOT = "/RetargetFix/"
RETARGETER = SOURCE_ROOT + "UE5_Mannequins/Rigs/RTG_UEFN_to_UE5_Mannequin"
SOURCE_MESH = SOURCE_ROOT + "UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin"
TARGET_MESH = "/Game/SurvivalRpg/Characters/Mannequins/Meshes/SKM_Manny_Simple"
LEG_CHAINS = {"LeftLeg", "RightLeg"}
LEG_BONES = ("thigh_l", "calf_l", "foot_l", "thigh_r", "calf_r", "foot_r")


def _require(condition, message):
    if not condition:
        raise RuntimeError(message)
    return condition


def _load(package):
    return _require(unreal.load_asset(package), f"Missing asset: {package}")


def _export(value):
    if value is None or isinstance(value, (bool, int, float, str)):
        return value
    if hasattr(value, "export_text"):
        return value.export_text()
    if hasattr(value, "get_path_name"):
        return value.get_path_name()
    return str(value)


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode("utf-8")).hexdigest()


def _shape(sequence):
    model = sequence.get_editor_property("data_model_interface")
    return (sequence.get_play_length(), model.get_number_of_keys(),
            model.get_number_of_frames(), _export(model.get_frame_rate()))


def _bone_names(sequence):
    return {str(name) for name in sequence.get_editor_property("data_model_interface").get_bone_track_names()}


def _raw_pose_signature(sequence, bones=("root",)):
    # Deprecated model track getters can return empty tracks in UE 5.8. Evaluate RAW
    # local poses at every authored key instead; never sample compressed data here.
    options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.RAW,
        should_retarget=False, extract_root_motion=False, incorporate_root_motion_into_pose=True,
        retrieve_additive_as_full_pose=False, evaluate_curves=False)
    result = []
    for frame in range(_shape(sequence)[1]):
        pose = sequence.get_anim_pose_at_frame(frame, options)
        _require(unreal.AnimPoseExtensions.is_valid(pose), f"Invalid raw pose at frame {frame}: {sequence.get_path_name()}")
        result.append([_export(unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL)) for bone in bones])
    return result


def _metadata(sequence):
    # Native reflection reaches protected metadata and the active animation data
    # model; Python property names alone are not a complete contract in UE 5.8.
    result = unreal.RpgAnimationRetargetLibrary.export_preserved_metadata(sequence)
    _require(isinstance(result, str) and result, f"Cannot export authoring metadata: {sequence.get_path_name()}")
    return {"shape": _shape(sequence), "authoring_data": result}


def _leg_pose_delta(sequence, candidate):
    options = unreal.AnimPoseEvaluationOptions(evaluation_type=unreal.AnimDataEvalType.RAW,
        should_retarget=False, extract_root_motion=False, incorporate_root_motion_into_pose=True,
        retrieve_additive_as_full_pose=False, evaluate_curves=False)
    maximum = [0.0, 0.0, 0.0]
    for frame in range(_shape(sequence)[1]):
        poses = [asset.get_anim_pose_at_frame(frame, options) for asset in (sequence, candidate)]
        for bone in LEG_BONES:
            a, b = [unreal.AnimPoseExtensions.get_bone_pose(pose, bone, unreal.AnimPoseSpaces.LOCAL) for pose in poses]
            point = lambda vector: [vector.x, vector.y, vector.z]
            qa = [a.rotation.x, a.rotation.y, a.rotation.z, a.rotation.w]
            qb = [b.rotation.x, b.rotation.y, b.rotation.z, b.rotation.w]
            dot = abs(sum(x*y for x, y in zip(qa, qb))) / math.sqrt(sum(x*x for x in qa) * sum(x*x for x in qb))
            delta = [math.dist(point(a.translation), point(b.translation)),
                     math.degrees(2.0 * math.acos(min(1.0, dot))),
                     max(abs(x-y) for x, y in zip(point(a.scale3d), point(b.scale3d)))]
            _require(all(math.isfinite(value) for value in delta), "Non-finite copied pose")
            maximum = [max(x, y) for x, y in zip(maximum, delta)]
    return maximum


def _root_motion(sequence):
    samples = []
    for alpha in (0.25, 0.5, 0.75, 1.0):
        value = unreal.MotionWarpingUtilities.extract_root_motion_from_animation(
            sequence, 0.0, sequence.get_play_length() * alpha)
        samples.append(([value.translation.x, value.translation.y, value.translation.z],
                        [value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w]))
    _require(all(math.isfinite(x) for pair in samples for vector in pair for x in vector), "Non-finite root motion")
    return samples


def _root_delta(before, after):
    position, angle = 0.0, 0.0
    for (p, q), (r, s) in zip(before, after):
        position = max(position, math.dist(p, r))
        denominator = math.sqrt(sum(x*x for x in q) * sum(x*x for x in s))
        dot = abs(sum(x*y for x, y in zip(q, s))) / _require(denominator, "Invalid root quaternion")
        angle = max(angle, math.degrees(2.0 * math.acos(min(1.0, dot))))
    return position, angle


def _guard_dependencies(registry, packages):
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True, include_hard_package_references=True,
        include_searchable_names=False, include_soft_management_references=True,
        include_hard_management_references=True)
    pending, visited = list(packages), set()
    while pending:
        package = str(pending.pop())
        if package in visited:
            continue
        visited.add(package)
        _require(not package.startswith((SOURCE_ROOT, STAGING_ROOT)),
                 f"Curated target depends on temporary/source content: {package}")
        if not package.startswith("/Script/"):
            pending.extend(registry.get_dependencies(package, options) or [])


def run(source_content, target_packages=None, apply=False):
    """Apply the no-leg-source-blend profile only to selected existing manifest targets.

    source_content is the local reference project's Content directory. None selects
    all manifest rows; [] selects none and fails. apply=False never mutates targets.
    No asset replacement, source/candidate saving, or manifest editing occurs.
    """
    project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
    manifest = project / "Plugins/RpgGaspLocomotion/Docs/CuratedAssetManifest.csv"
    with manifest.open(encoding="utf-8-sig", newline="") as handle:
        rows = list(csv.DictReader(handle))
    mappings = {row["TargetPackage"]: row for row in rows}
    _require(len(mappings) == len(rows), "Duplicate manifest targets")
    _require(not isinstance(target_packages, str), "target_packages must be an iterable of package names")
    selected = set(mappings if target_packages is None else target_packages)
    _require(selected and selected <= mappings.keys(), f"Unknown/empty target selection: {selected - mappings.keys()}")
    rows = [mappings[package] for package in sorted(selected)]
    sources = [row["SourcePackage"] for row in rows]
    _require(len({path.rsplit("/", 1)[-1] for path in sources}) == len(rows), "Candidate names are not unique")
    _require(all(path.startswith(SOURCE_ROOT) for path in sources), "Unexpected manifest source mount")
    _require(all(path.startswith("/RpgGaspLocomotion/Animations/") for path in selected), "Unexpected target mount")
    _require(hasattr(unreal, "RpgAnimationRetargetLibrary"), "Build SurvivalRpgEditor before running this script")
    source_dir = Path(source_content).resolve() / "Characters"
    _require(source_dir.is_dir(), f"Missing source Characters directory: {source_dir}")
    run_id = uuid.uuid4().hex
    output_dir = project / "Saved/RetargetFix" / run_id
    output_dir.mkdir(parents=True, exist_ok=False)
    staging = STAGING_ROOT + run_id
    mounts, audit = [], {"apply": bool(apply), "profile": "rpg_no_leg_source_blend_v1", "clips": []}
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        for mount, folder in ((SOURCE_ROOT, source_dir), (staging + "/", output_dir)):
            physical = folder.as_posix().rstrip("/") + "/"
            _require(not any(c in physical for c in '\"\r\n'), "Unsupported mount path")
            unreal.SystemLibrary.execute_console_command(None, f'PackageName.RegisterMountPoint {mount} "{physical}"')
            mounts.append((mount, physical))
        registry.scan_paths_synchronous([SOURCE_ROOT, "/RpgGaspLocomotion/", "/Game/SurvivalRpg/"], force_rescan=True)
        registry.wait_for_completion()
        targets = {package: _load(package) for package in selected}
        source_assets = [_load(package) for package in sources]
        _require(all(isinstance(asset, unreal.AnimSequence) for asset in [*targets.values(), *source_assets]),
                 "Manifest contains a non-sequence asset")
        _guard_dependencies(registry, selected)
        before = {package: (_metadata(asset), _raw_pose_signature(asset), _root_motion(asset)) for package, asset in targets.items()}
        controller = unreal.IKRetargeterController.get_controller(_load(RETARGETER))
        _require(str(controller.get_op_name(2)) == "Blend to Source" and controller.get_retarget_op_enabled(2),
                 "Reference retargeter Blend to Source operation changed")
        blend = _require(controller.get_op_controller(2), "Missing blend controller")
        original = blend.get_settings()
        original_text = _export(original)
        fixed = blend.get_settings()
        chains, changed = list(fixed.get_editor_property("chains")), set()
        for chain in chains:
            name = str(chain.get_editor_property("target_chain_name"))
            if name in LEG_CHAINS:
                _require(abs(chain.get_editor_property("blend_to_source") - 1.0) < 1e-6, "Reference leg blend is not stock")
                chain.set_editor_property("blend_to_source", 0.0)
                changed.add(name)
        _require(changed == LEG_CHAINS, "Missing retarget leg chains")
        fixed.set_editor_property("chains", chains)
        try:
            blend.set_settings(fixed)
            inputs = unreal.IKRetargetBatchOperationInputs()
            settings = dict(assets_to_retarget=[unreal.EditorAssetLibrary.find_asset_data(a.get_path_name()) for a in source_assets],
                            source_mesh=_load(SOURCE_MESH), target_mesh=_load(TARGET_MESH), ik_retarget_asset=_load(RETARGETER),
                            target_path=staging, include_referenced_assets=False, overwrite_existing_files=False, retain_additive_flags=True)
            for name, value in settings.items():
                inputs.set_editor_property(name, value)
            generated = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs) or []
        finally:
            blend.set_settings(original)
            _require(_export(blend.get_settings()) == original_text, "Reference retargeter settings were not restored")
        candidates = {str(data.package_name): data.get_asset() for data in generated}
        expected = {staging + "/" + path.rsplit("/", 1)[-1] for path in sources}
        _require(set(candidates) == expected, f"Unexpected candidate set: {set(candidates) ^ expected}")
        for row in rows:
            package = row["TargetPackage"]
            target, candidate = targets[package], candidates[staging + "/" + row["SourcePackage"].rsplit("/", 1)[-1]]
            metadata, raw_root, root = before[package]
            _require(isinstance(candidate, unreal.AnimSequence) and _shape(candidate) == _shape(target), f"Candidate shape changed: {package}")
            _require(candidate.get_editor_property("skeleton") == target.get_editor_property("skeleton"), f"Skeleton mismatch: {package}")
            bones = _bone_names(target)
            _require(_bone_names(candidate) == bones and {"root", *LEG_BONES} <= bones, f"Bone domain mismatch: {package}")
            _require(_metadata(target) == metadata and _raw_pose_signature(target) == raw_root, f"Retarget batch touched target: {package}")
            if apply:
                result = unreal.RpgAnimationRetargetLibrary.copy_retargeted_bone_tracks(candidate, target)
                # Unreal Python folds a bool return plus one out parameter into None on
                # failure or that out parameter on success (the empty error string here).
                _require(result is not None, f"Native track copy rejected {package}")
                _require(result == "", f"Unexpected native copy diagnostic: {result!r}")
            after_root = _root_motion(target)
            _require(_metadata(target) == metadata and _raw_pose_signature(target) == raw_root, f"Metadata/raw root changed: {package}")
            delta = _root_delta(root, after_root)
            _require(delta[0] <= 0.1 and delta[1] <= 0.01, f"Runtime root motion changed: {package}: {delta}")
            leg_delta = None
            if apply:
                leg_delta = _leg_pose_delta(target, candidate)
                # The controller stores float transforms; quaternion normalization can
                # differ from the in-memory retarget candidate by a few 1e-5 degrees.
                _require(leg_delta[0] <= 0.001 and leg_delta[1] <= 0.001 and leg_delta[2] <= 1e-6,
                         f"Copied leg pose differs: {package}: cm/degrees/scale={leg_delta}")
                _guard_dependencies(registry, [package])
                _require(unreal.EditorAssetLibrary.save_loaded_asset(target, only_if_is_dirty=False), f"Save failed: {package}")
                _require(_metadata(target) == metadata and _raw_pose_signature(target) == raw_root, f"Save changed contract: {package}")
                after_root = _root_motion(target)
                delta = _root_delta(root, after_root)
                _require(delta[0] <= 0.1 and delta[1] <= 0.01, f"Saved root motion changed: {package}: {delta}")
                _guard_dependencies(registry, [package])
            audit["clips"].append(dict(package=package, saved=bool(apply), metadata_sha256=_digest(metadata),
                                      copied_leg_delta_cm_degrees_scale=leg_delta,
                                      raw_root_sha256=_digest(raw_root), root_motion_before=root, root_motion_after=after_root,
                                      root_translation_delta_cm=delta[0], root_rotation_delta_degrees=delta[1]))
            unreal.log(f"Retarget curated: {'saved' if apply else 'validated'} {package}")
        audit["complete"] = True
        return audit
    except Exception as error:
        audit["error"] = str(error)
        raise
    finally:
        (output_dir / "audit.json").write_text(json.dumps(audit, indent=2, sort_keys=True), encoding="utf-8")
        for mount, physical in reversed(mounts):
            unreal.SystemLibrary.execute_console_command(None, f'PackageName.UnregisterMountPoint {mount} "{physical}"')
