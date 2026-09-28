"""Editor MCP authoring for explicit, derived animation batches using UE's IK APIs.

No source asset or shared skeleton is edited. Outputs are never overwritten or
implicitly saved. Partial failures retain new assets for inspection, not deletion.
"""
import json
import math
import re

import unreal
import toolset_registry
from toolset_registry.registration import Registration


_SOURCE_TAG = 'RpgRetarget.SourceSequence'
_MESH_TAG = 'RpgRetarget.TargetMesh'
_NORMALIZED_TAG = 'RpgRetarget.Normalized'


def _guard():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop PIE before authoring derived animations')


def _package(path):
    if (not isinstance(path, str) or not re.fullmatch(r'/[A-Za-z0-9_]+(?:/[A-Za-z0-9_]+)+', path)
            or not unreal.RpgAnimationAssetTools.is_project_content_package(path)):
        raise ValueError('Expected a project-content long package name: ' + str(path))
    return path


def _load(path, expected_type):
    value = unreal.load_asset(_package(path))
    if not isinstance(value, expected_type):
        raise ValueError('Unexpected or missing asset type: ' + path)
    return value


def _array(encoded, limit, empty=False):
    if not isinstance(encoded, str) or len(encoded) > 65536:
        raise ValueError('JSON input exceeds the authoring limit')
    values = json.loads(encoded)
    if not isinstance(values, list) or len(values) > limit or (not values and not empty):
        raise ValueError('Expected a bounded JSON array')
    return values


def _name(value):
    if not isinstance(value, str) or not re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]{0,127}', value) or value.lower() == 'none':
        raise ValueError('Expected a nonempty bone, marker or curve name')
    return value


def _path(asset):
    return asset.get_path_name().split('.', 1)[0]


def _chains(controller):
    return [str(chain.get_editor_property('chain_name')) for chain in controller.get_retarget_chains()]


def _bone_names(skeleton):
    pose = unreal.AnimPoseExtensions.get_reference_pose(skeleton)
    return [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]


def _op_contract(controller):
    records = []
    for index in range(controller.get_num_retarget_ops()):
        op = controller.get_op_controller(index)
        row = {'index': index, 'controller': op.get_class().get_name(),
               'enabled': controller.get_retarget_op_enabled(index)}
        if isinstance(op, unreal.IKRetargetPelvisMotionController):
            row.update(source_pelvis=str(op.get_source_pelvis_bone()),
                       target_pelvis=str(op.get_target_pelvis_bone()))
        if isinstance(op, unreal.IKRetargetRootMotionController):
            row.update(source_root=str(op.get_source_root_bone()),
                       target_root=str(op.get_target_root_bone()),
                       target_pelvis=str(op.get_target_pelvis_bone()),
                       root_motion_source=str(op.get_settings().get_editor_property('root_motion_source')),
                       root_height_source=str(op.get_settings().get_editor_property('root_height_source')))
        records.append(row)
    return records


def _new_rig(directory, name, mesh, created):
    rig = unreal.IKRigDefinitionFactory.create_new_ik_rig_asset(directory, name)
    if rig:
        created.append(_path(rig))
    if not rig or _path(rig) != directory + '/' + name:
        raise RuntimeError('Rig factory did not create the exact preflighted path')
    controller = unreal.IKRigController.get_controller(rig)
    if not controller.set_skeletal_mesh(mesh) or not controller.apply_auto_generated_retarget_definition():
        raise RuntimeError('Automatic rig generation failed for ' + mesh.get_path_name())
    if not _chains(controller):
        raise RuntimeError('Automatic rig generation produced no retarget chains')
    return rig, controller


@unreal.uclass()
class AnimationRetargetTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def retarget_animation_batch(source_mesh_path: str, target_mesh_path: str,
                                 sequence_paths_json: str, target_directory: str,
                                 excluded_target_bones_json: str = '[]',
                                 target_rig_path: str = '', bone_map_json: str = '{}',
                                 chain_aliases_json: str = '{}') -> str:
        """Derive 1..128 explicit sequences in a new empty directory; no save or overwrite.

        Creates IK_Source, IK_Target (unless an existing target rig is supplied),
        RTG_SourceToTarget and <source-name>_Retargeted. Uses exact chains, target
        autoalignment, optional named pose exclusions and FK translation None.
        Existing target rigs are read only. Generated poses still require review.
        bone_map_json optionally overrides source_pelvis, target_pelvis,
        source_root and target_root on the NEW retargeter, never the shared rig.
        chain_aliases_json explicitly maps target-chain names to source chains.
        """
        _guard()
        directory = _package(target_directory)
        source_mesh = _load(source_mesh_path, unreal.SkeletalMesh)
        target_mesh = _load(target_mesh_path, unreal.SkeletalMesh)
        if source_mesh == target_mesh:
            raise ValueError('Source and target meshes must differ')
        source_skeleton = source_mesh.get_editor_property('skeleton')
        target_skeleton = target_mesh.get_editor_property('skeleton')
        source_bones = _bone_names(source_skeleton)
        target_bone_names = _bone_names(target_skeleton)
        if not source_bones or not target_bone_names:
            raise ValueError('Source and target reference skeletons must contain bones')
        if len(bone_map_json) > 4096 or len(chain_aliases_json) > 16384:
            raise ValueError('Retarget mapping input exceeds the authoring limit')
        bone_overrides = json.loads(bone_map_json)
        aliases = json.loads(chain_aliases_json)
        if (not isinstance(bone_overrides, dict)
                or set(bone_overrides) - {'source_pelvis', 'target_pelvis', 'source_root', 'target_root'}):
            raise ValueError('Unexpected bone_map_json key')
        for key, value in bone_overrides.items():
            if _name(value) not in (source_bones if key.startswith('source_') else target_bone_names):
                raise ValueError('Mapped bone is absent from the corresponding skeleton: ' + value)
        if not isinstance(aliases, dict) or len(aliases) > 128:
            raise ValueError('Expected a bounded target-chain to source-chain dictionary')
        for target_chain, source_chain in aliases.items():
            _name(target_chain)
            _name(source_chain)
        paths = [_package(p) for p in _array(sequence_paths_json, 128)]
        if len(set(p.lower() for p in paths)) != len(paths):
            raise ValueError('Duplicate source sequence')
        sequences = [_load(p, unreal.AnimSequence) for p in paths]
        if any(s.get_editor_property('skeleton') != source_skeleton for s in sequences):
            raise ValueError('Every source sequence must use the supplied source mesh skeleton')
        if any(p.startswith(directory + '/') for p in paths):
            raise ValueError('Source sequences cannot be inside the output directory')
        excluded = [_name(n) for n in _array(excluded_target_bones_json, 128, empty=True)]
        if len(set(excluded)) != len(excluded):
            raise ValueError('Duplicate excluded bone')
        target_bones = set(target_bone_names)
        if not target_bones or any(n not in target_bones for n in excluded):
            raise ValueError('An excluded bone does not exist in the target skeleton')
        target_rig = None
        target_controller = None
        if target_rig_path:
            target_rig = _load(target_rig_path, unreal.IKRigDefinition)
            target_controller = unreal.IKRigController.get_controller(target_rig)
            if not target_controller.is_skeletal_mesh_compatible(target_mesh) or not _chains(target_controller):
                raise ValueError('Existing target rig is incompatible or has no chains')
        output_names = [s.get_name() + '_Retargeted' for s in sequences]
        new_names = ['IK_Source', 'RTG_SourceToTarget'] + ([] if target_rig else ['IK_Target']) + output_names
        if len(set(n.lower() for n in new_names)) != len(new_names):
            raise ValueError('Source names collide in the output directory')
        if unreal.EditorAssetLibrary.list_assets(directory, recursive=True, include_folder=False):
            raise ValueError('Output directory must contain no existing assets')
        if any(unreal.EditorAssetLibrary.does_asset_exist(directory + '/' + n) for n in new_names):
            raise ValueError('An output already exists')

        created = []
        try:
            source_rig, source_controller = _new_rig(directory, 'IK_Source', source_mesh, created)
            if not target_rig:
                target_rig, target_controller = _new_rig(directory, 'IK_Target', target_mesh, created)
            retargeter = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
                'RTG_SourceToTarget', directory, unreal.IKRetargeter, unreal.IKRetargetFactory())
            if retargeter:
                created.append(_path(retargeter))
            if not retargeter or _path(retargeter) != directory + '/RTG_SourceToTarget':
                raise RuntimeError('Retargeter factory did not create the exact preflighted path')
            controller = unreal.IKRetargeterController.get_controller(retargeter)
            source_role = unreal.RetargetSourceOrTarget.SOURCE
            target_role = unreal.RetargetSourceOrTarget.TARGET
            controller.set_ik_rig(source_role, source_rig)
            controller.set_ik_rig(target_role, target_rig)
            controller.set_preview_mesh(source_role, source_mesh)
            controller.set_preview_mesh(target_role, target_mesh)
            controller.add_default_ops()
            initial_ops = _op_contract(controller)
            bone_map = {'source_pelvis': str(source_controller.get_retarget_root()),
                        'target_pelvis': str(target_controller.get_retarget_root()),
                        'source_root': source_bones[0], 'target_root': target_bone_names[0]}
            bone_map.update(bone_overrides)
            for key, value in bone_map.items():
                if value not in (source_bones if key.startswith('source_') else target_bone_names):
                    raise ValueError('Rig has an invalid bone role; supply bone_map_json: ' + key + '=' + value)
            # Stock OnAddedToStack copies GetPelvis from the rigs. Older rigs
            # can intentionally use the skeleton root there; anatomical role
            # overrides belong to this newly authored retargeter, not that rig.
            for index in range(controller.get_num_retarget_ops()):
                op = controller.get_op_controller(index)
                if isinstance(op, unreal.IKRetargetPelvisMotionController):
                    op.set_source_pelvis_bone(bone_map['source_pelvis'])
                    op.set_target_pelvis_bone(bone_map['target_pelvis'])
                if isinstance(op, unreal.IKRetargetRootMotionController):
                    op.set_source_root_bone(bone_map['source_root'])
                    op.set_target_root_bone(bone_map['target_root'])
                    op.set_target_pelvis_bone(bone_map['target_pelvis'])
                    settings = op.get_settings()
                    generate = bone_map['source_root'] == bone_map['source_pelvis']
                    settings.set_editor_property('root_motion_source',
                        unreal.RootMotionSource.GENERATE_FROM_TARGET_PELVIS if generate
                        else unreal.RootMotionSource.COPY_FROM_SOURCE_ROOT)
                    settings.set_editor_property('root_height_source',
                        unreal.RootMotionHeightSource.SNAP_TO_GROUND if generate
                        else unreal.RootMotionHeightSource.COPY_HEIGHT_FROM_SOURCE)
                    settings.set_editor_property('rotate_with_pelvis', generate)
                    settings.set_editor_property('maintain_offset_from_pelvis', True)
                    op.set_settings(settings)
                    controller.set_retarget_op_enabled(index, bone_map['target_root'] != bone_map['target_pelvis'])
            controller.auto_map_chains(unreal.AutoMapChainType.EXACT, True)
            source_chains = set(_chains(source_controller))
            target_chains = set(_chains(target_controller))
            for target_chain, source_chain in aliases.items():
                if target_chain not in target_chains or source_chain not in source_chains:
                    raise ValueError('Explicit chain alias names an absent chain: ' + target_chain + '<-' + source_chain)
                if not controller.set_source_chain(source_chain, target_chain):
                    raise RuntimeError('Could not apply explicit chain alias: ' + target_chain)
            chain_map = []
            for target_chain in _chains(target_controller):
                mapped = str(controller.get_source_chain(target_chain))
                expected = aliases.get(target_chain, target_chain if target_chain in source_chains else 'None')
                if mapped != expected:
                    raise RuntimeError('Exact chain mapping differs from the expected mapping: ' + target_chain)
                chain_map.append({'target': target_chain, 'source': None if mapped == 'None' else mapped})
            if not any(row['source'] for row in chain_map):
                raise RuntimeError('No exact source/target chain matches')
            pose_name = controller.get_current_retarget_pose_name(target_role)
            controller.reset_retarget_pose(pose_name, [], target_role)
            controller.auto_align_all_bones(target_role)
            if excluded:
                controller.reset_retarget_pose(pose_name, excluded, target_role)
            ops = []
            fk_chains = []
            disabled_ik = 0
            for index in range(controller.get_num_retarget_ops()):
                op = controller.get_op_controller(index)
                if isinstance(op, unreal.IKRetargetRunIKRigController):
                    if not controller.set_retarget_op_enabled(index, False):
                        raise RuntimeError('Could not disable IK Solve')
                    disabled_ik += 1
                if isinstance(op, unreal.IKRetargetFKChainsController):
                    for chain in op.get_settings().get_editor_property('chains_to_retarget'):
                        mode = chain.get_editor_property('translation_mode')
                        if mode != unreal.FKChainTranslationMode.NONE:
                            raise RuntimeError('Unexpected non-None default FK translation mode')
                        fk_chains.append(str(chain.get_editor_property('target_chain_name')))
                ops.append({'index': index, 'controller': op.get_class().get_name(),
                            'enabled': controller.get_retarget_op_enabled(index)})
            if not disabled_ik or not fk_chains:
                raise RuntimeError('Expected default IK Solve and FK Chains operations were absent')
            inputs = unreal.IKRetargetBatchOperationInputs()
            settings = {'assets_to_retarget': [unreal.EditorAssetLibrary.find_asset_data(p) for p in paths],
                        'source_mesh': source_mesh, 'target_mesh': target_mesh,
                        'ik_retarget_asset': retargeter, 'target_path': directory,
                        'suffix': '_Retargeted', 'use_source_path': False,
                        'include_referenced_assets': False, 'overwrite_existing_files': False,
                        'retain_additive_flags': True}
            for key, value in settings.items():
                inputs.set_editor_property(key, value)
            results = unreal.IKRetargetBatchOperation.run_batch_retarget(inputs)
            result_assets = [data.get_asset() for data in results]
            created.extend(_path(asset) for asset in result_assets if asset)
            expected_paths = [directory + '/' + name for name in output_names]
            if (len(result_assets) != len(sequences)
                    or set(_path(a) for a in result_assets if a) != set(expected_paths)):
                raise RuntimeError('Batch result differs from the explicit source/output mapping')
            mapping = []
            for source_path, target_path in zip(paths, expected_paths):
                sequence = _load(target_path, unreal.AnimSequence)
                if sequence.get_editor_property('skeleton') != target_skeleton:
                    raise RuntimeError('Output does not use the target skeleton: ' + target_path)
                unreal.EditorAssetLibrary.set_metadata_tag(sequence, _SOURCE_TAG, source_path)
                unreal.EditorAssetLibrary.set_metadata_tag(sequence, _MESH_TAG, _path(target_mesh))
                mapping.append({'source': source_path, 'target': target_path,
                                'length_seconds': sequence.get_play_length()})
            warnings = ['Autoalignment is a starting pose, not visual or contact validation.',
                        'IK Solve is disabled; no automatic IK-bone pin operation is added.',
                        'Source root motion and additive flags are retained until explicit normalization.']
            unmapped = [row['target'] for row in chain_map if row['source'] is None]
            if unmapped:
                warnings.append('Unmapped target chains: ' + ', '.join(unmapped))
            return json.dumps({'created': created, 'mapping': mapping, 'chain_mapping': chain_map,
                               'source_chain_names': _chains(source_controller),
                               'target_chain_names': _chains(target_controller),
                               'source_skeleton': source_skeleton.get_path_name(),
                               'target_skeleton': target_skeleton.get_path_name(),
                               'target_rig': target_rig.get_path_name(), 'operations': _op_contract(controller),
                               'initial_operations': initial_ops, 'bone_map': bone_map,
                               'explicit_chain_aliases': aliases,
                               'fk_translation_none_chains': fk_chains,
                               'excluded_target_bones': excluded, 'saved': False,
                               'warnings': warnings}, indent=2)
        except Exception as error:
            raise RuntimeError(str(error) + '; newly created assets retained: ' + json.dumps(created)) from error

    @toolset_registry.tool_call
    @staticmethod
    def retarget_asset_contract(asset_path: str) -> str:
        """Read rig anatomical roles, chain names and root/pelvis operations without edits."""
        _guard()
        asset = _load(asset_path, unreal.IKRetargeter)
        controller = unreal.IKRetargeterController.get_controller(asset)
        rigs = {}
        for label, role in [('source', unreal.RetargetSourceOrTarget.SOURCE),
                            ('target', unreal.RetargetSourceOrTarget.TARGET)]:
            rig = controller.get_ik_rig(role)
            if rig:
                rig_controller = unreal.IKRigController.get_controller(rig)
                rigs[label] = {'asset': _path(rig),
                               'retarget_pelvis': str(rig_controller.get_retarget_root()),
                               'root_motion_bone': str(rig_controller.get_root_motion_bone()),
                               'chains': _chains(rig_controller)}
        return json.dumps({'asset': _path(asset), 'rigs': rigs,
                           'operations': _op_contract(controller)}, indent=2)

    @toolset_registry.tool_call
    @staticmethod
    def normalize_derived_sequence(asset_path: str, root_yaw_curve_name: str,
                                   contact_markers_json: str, looping: bool,
                                   marker_track_name: str = 'FootContacts',
                                   root_bone_name: str = 'root') -> str:
        """Normalize a wrapper-derived sequence once, without changing its source or saving.

        Samples unwrapped root yaw (degrees relative to frame zero) BEFORE forcing
        first-frame root lock. Markers are explicit [{"name":"Left" or "Right",
        "time_seconds":number}]; [] is allowed. Loop is the asset preview/default,
        not an override of a sequence player's loop setting. Existing curves and
        notifies are retained; the requested yaw curve must not already exist.
        """
        _guard()
        sequence = _load(asset_path, unreal.AnimSequence)
        curve_name = _name(root_yaw_curve_name)
        track_name = _name(marker_track_name)
        root_name = _name(root_bone_name)
        if not isinstance(looping, bool):
            raise ValueError('looping must be a boolean')
        source_path = unreal.EditorAssetLibrary.get_metadata_tag(sequence, _SOURCE_TAG)
        mesh_path = unreal.EditorAssetLibrary.get_metadata_tag(sequence, _MESH_TAG)
        if (not source_path or source_path == _path(sequence) or not mesh_path
                or unreal.EditorAssetLibrary.get_metadata_tag(sequence, _NORMALIZED_TAG)):
            raise ValueError('Expected an unnormalized output created by retarget_animation_batch')
        source = _load(source_path, unreal.AnimSequence)
        mesh = _load(mesh_path, unreal.SkeletalMesh)
        if source == sequence or sequence.get_editor_property('skeleton') != mesh.get_editor_property('skeleton'):
            raise ValueError('Derived sequence provenance does not match its target skeleton')
        # Additive sequences need a separate authoring contract for root locking.
        if sequence.get_editor_property('additive_anim_type') != unreal.AdditiveAnimationType.AAT_NONE:
            raise ValueError('In-place normalization accepts only non-additive sequences')
        library = unreal.AnimationLibrary
        if library.does_curve_exist(sequence, curve_name, unreal.RawCurveTrackTypes.RCT_FLOAT):
            raise ValueError('Requested root-yaw curve already exists')
        length = sequence.get_play_length()
        count = library.get_num_keys(sequence)
        if not math.isfinite(length) or length <= 0 or not 2 <= count <= 10000:
            raise ValueError('Expected a finite sequence with 2..10000 sampled keys')
        markers = _array(contact_markers_json, 512, empty=True)
        seen = set()
        for marker in markers:
            if not isinstance(marker, dict) or set(marker) != {'name', 'time_seconds'}:
                raise ValueError('Contact markers require name and time_seconds')
            time = marker['time_seconds']
            if (marker['name'] not in ('Left', 'Right') or type(time) not in (int, float)
                    or not math.isfinite(time) or not 0 <= time <= length):
                raise ValueError('Contact marker must be Left/Right at a valid sequence time')
            pair = (marker['name'], float(time))
            if pair in seen:
                raise ValueError('Duplicate contact marker')
            seen.add(pair)
        existing_markers = library.get_animation_sync_markers(sequence)
        for marker in existing_markers:
            if (str(marker.get_editor_property('marker_name')),
                    float(marker.get_editor_property('time'))) in seen:
                raise ValueError('A requested contact marker already exists')
        options = unreal.AnimPoseEvaluationOptions()
        for key, value in {'evaluation_type': unreal.AnimDataEvalType.RAW,
                           'should_retarget': True, 'extract_root_motion': False,
                           'incorporate_root_motion_into_pose': True,
                           'optional_skeletal_mesh': mesh,
                           'retrieve_additive_as_full_pose': True,
                           'evaluate_curves': False}.items():
            options.set_editor_property(key, value)
        times = [float(library.get_time_at_frame(sequence, i)) for i in range(count)]
        yaw_values = []
        previous = None
        unwrapped = 0.0
        for time in times:
            pose = unreal.AnimPoseExtensions.get_anim_pose_at_time(sequence, time, options)
            names = [str(n) for n in unreal.AnimPoseExtensions.get_bone_names(pose)]
            if not unreal.AnimPoseExtensions.is_valid(pose) or not names or names[0] != root_name:
                raise RuntimeError('Requested root is not the evaluated skeleton root')
            transform = unreal.AnimPoseExtensions.get_bone_pose(pose, root_name, unreal.AnimPoseSpaces.WORLD)
            yaw = float(transform.rotation.rotator().yaw)
            if not math.isfinite(yaw):
                raise RuntimeError('Nonfinite root yaw')
            if previous is not None:
                unwrapped += (yaw - previous + 180.0) % 360.0 - 180.0
            yaw_values.append(unwrapped)
            previous = yaw
        # All sampling and validation precede edits. Public controller/property
        # APIs own changes; no raw data writes or shared skeleton-mode changes.
        with unreal.ScopedEditorTransaction('Normalize derived animation'):
            sequence.modify()
            library.add_curve(sequence, curve_name, unreal.RawCurveTrackTypes.RCT_FLOAT, False)
            library.add_float_curve_keys(sequence, curve_name, times, yaw_values)
            if markers and not library.is_valid_anim_notify_track_name(sequence, track_name):
                library.add_animation_notify_track(sequence, track_name)
            for marker in sorted(markers, key=lambda item: item['time_seconds']):
                library.add_animation_sync_marker(sequence, marker['name'], float(marker['time_seconds']), track_name)
            sequence.set_editor_property('root_motion_root_lock', unreal.RootMotionRootLock.ANIM_FIRST_FRAME)
            sequence.set_editor_property('enable_root_motion', False)
            sequence.set_editor_property('force_root_lock', True)
            sequence.set_editor_property('loop', looping)
            unreal.EditorAssetLibrary.set_metadata_tag(sequence, _NORMALIZED_TAG, curve_name)
        return json.dumps({'asset': _path(sequence), 'source': source_path, 'saved': False,
                           'curve': curve_name, 'units': 'degrees relative to first frame, unwrapped',
                           'samples': count, 'final_yaw_degrees': yaw_values[-1],
                           'min_yaw_degrees': min(yaw_values), 'max_yaw_degrees': max(yaw_values),
                           'sampling': 'raw, root motion incorporated, before root lock',
                           'root_motion_root_lock': 'AnimFirstFrame', 'force_root_lock': True,
                           'enable_root_motion': False, 'loop': looping,
                           'added_contact_markers': markers, 'marker_track': track_name,
                           'warnings': ['Marker times and pose/contact continuity require authored review.',
                                        'Sync-marker insertion uses the engine notify-cache refresh.']}, indent=2)


registration = Registration([AnimationRetargetTools])
registration.register()
unreal.log('RPG_ANIMATION_RETARGET_TOOLS_READY')
