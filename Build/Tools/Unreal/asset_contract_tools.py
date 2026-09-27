"""Optional editor-only MCP tools for asset contracts, owned references and local PIE inspection.

Load from an editor Python startup script. Standard asset/Blueprint/object tools
still own duplication, property edits, compilation and saves. These operations
fill gaps in the UE 5.8 toolsets: complete exports, instanced reference remapping,
precise montage notify timing, fresh package reloads and gameplay input/view inspection in PIE.
"""
import json
import math
import os
from pathlib import Path

import unreal
import toolset_registry
from toolset_registry.registration import Registration


def _guard():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop PIE before inspecting or modifying asset contracts')


def _asset(path):
    value = unreal.load_asset(path)
    if not value:
        raise RuntimeError('Cannot load asset: ' + path)
    return value


def _status():
    return {'pid': os.getpid(),
            'project': unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()),
            'dirty_content': [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()],
            'dirty_maps': [p.get_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages()]}


@unreal.uclass()
class AssetContractTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def set_pie_view_rotation(controller_path: str, pitch: float, yaw: float) -> bool:
        """Aim a local PIE player's view for gameplay inspection; no actor teleport or asset edit."""
        controller = unreal.find_object(None, controller_path)
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if (not isinstance(controller, unreal.PlayerController) or not world
                or controller.get_world() != world or not controller.is_local_player_controller()
                or not math.isfinite(pitch) or not math.isfinite(yaw) or abs(pitch) > 85):
            raise RuntimeError('Expected a local current-PIE controller and finite view angles (pitch -85..85 degrees)')
        controller.set_control_rotation(unreal.Rotator(pitch=pitch, yaw=yaw, roll=0))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def set_pie_input_key(controller_path: str, key_name: str, pressed: bool) -> bool:
        """Send a digital key transition through a local PIE controller; release in a later call after a game tick."""
        controller = unreal.find_object(None, controller_path)
        if not isinstance(controller, unreal.PlayerController):
            raise RuntimeError('Expected an existing PIE PlayerController object path')
        return unreal.RpgEditorPlaytestTools.set_pie_input_key(controller, key_name, pressed)

    @toolset_registry.tool_call
    @staticmethod
    def editor_status() -> str:
        """Return editor PID, project and unsaved packages; never save implicitly."""
        _guard()
        return json.dumps(_status())

    @toolset_registry.tool_call
    @staticmethod
    def export_asset(asset_path: str, saved_relative_path: str) -> str:
        """Export complete reflected T3D to a new file below project Saved for audit."""
        _guard()
        root = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())).resolve()
        target = (root / saved_relative_path).resolve()
        if not target.is_relative_to(root) or target.suffix != '.t3d' or target.exists():
            raise RuntimeError('Expected a new .t3d file below project Saved')
        target.parent.mkdir(parents=True, exist_ok=True)
        task = unreal.AssetExportTask()
        for name, value in {'object': _asset(asset_path), 'exporter': unreal.ObjectExporterT3D(),
                            'filename': str(target), 'selected': False, 'replace_identical': False,
                            'prompt': False, 'automated': True, 'use_file_archive': False,
                            'write_empty_files': True}.items():
            task.set_editor_property(name, value)
        if not unreal.Exporter.run_asset_export_task(task) or task.get_editor_property('errors'):
            raise RuntimeError('Reflected export failed: ' + asset_path)
        return str(target)

    @toolset_registry.tool_call
    @staticmethod
    def remap_owned_references(asset_path: str, replacements_json: str) -> int:
        """Remap serialized references inside one project asset and its owned subobjects; does not save."""
        _guard()
        if not asset_path.startswith('/Game/'):
            raise RuntimeError('Only project-owned assets can be edited')
        pairs = json.loads(replacements_json)
        if not isinstance(pairs, dict) or not pairs:
            raise RuntimeError('Expected a nonempty source-to-target asset path mapping')
        replacements = {_asset(source): _asset(target) for source, target in pairs.items()}
        if any(source.get_class() != target.get_class() for source, target in replacements.items()):
            raise RuntimeError('Reference replacement requires matching asset classes')
        count = unreal.RpgBlueprintAssetTools.remap_owned_object_references(_asset(asset_path), replacements)
        if count < 0:
            raise RuntimeError('Owned reference remap failed')
        return count

    @toolset_registry.tool_call
    @staticmethod
    def montage_contract(asset_path: str) -> str:
        """Read montage duration and full notify records with precise trigger times and notify object paths."""
        _guard()
        montage = _asset(asset_path)
        if not isinstance(montage, unreal.AnimMontage):
            raise RuntimeError('Expected an AnimMontage')
        records = []
        for event in unreal.AnimationLibrary.get_animation_notify_events(montage):
            state = event.get_editor_property('notify_state_class')
            records.append({'record': event.export_text(),
                            'trigger_time': unreal.AnimationLibrary.get_anim_notify_event_trigger_time(event),
                            'state': state.get_path_name() if state else None,
                            'state_class': state.get_class().get_path_name() if state else None})
        return json.dumps({'path': montage.get_path_name(), 'length': montage.get_play_length(), 'notifies': records})

    @toolset_registry.tool_call
    @staticmethod
    def remap_animation_notify_classes(asset_path: str, replacements_json: str,
                                       type_replacements_json: str = '{}', object_replacements_json: str = '{}') -> int:
        """Replace owned notify instances using compatible copied Blueprint classes; preserve events and do not save.

        Both JSON objects map source paths to target paths. Notify paths must be generated class paths (_C).
        Optional type mappings explicitly identify copied Blueprint parents, enums or script structs.
        Optional object mappings identify exact asset values for direct hard object properties whose class changes.
        """
        _guard()
        if not asset_path.startswith('/Game/'):
            raise RuntimeError('Only project-owned animations can be edited')
        animation = _asset(asset_path)
        if not isinstance(animation, unreal.AnimSequenceBase):
            raise RuntimeError('Expected an AnimSequenceBase')
        pairs = json.loads(replacements_json)
        types = json.loads(type_replacements_json)
        objects = json.loads(object_replacements_json)
        if not isinstance(pairs, dict) or not pairs or not isinstance(types, dict) or not isinstance(objects, dict):
            raise RuntimeError('Expected notify-class and optional type/object path mappings')
        def load_type(path, require_class=False):
            if not isinstance(path, str) or not path.startswith('/'):
                raise RuntimeError('Expected an absolute Unreal object or generated-class path')
            value = unreal.load_class(None, path) if require_class or path.endswith('_C') else unreal.load_object(None, path)
            if not value:
                raise RuntimeError('Cannot load mapped type: ' + path)
            return value
        classes = {load_type(source, True): load_type(target, True) for source, target in pairs.items()}
        type_objects = {load_type(source): load_type(target) for source, target in types.items()}
        object_values = {_asset(source): _asset(target) for source, target in objects.items()}
        count = unreal.RpgBlueprintAssetTools.remap_animation_notify_classes(animation, classes, type_objects, object_values)
        if count < 0:
            raise RuntimeError('Notify class remap rejected: incompatible schema, ownership or editor context')
        return count

    @toolset_registry.tool_call
    @staticmethod
    def reload_assets(asset_paths: list[str]) -> bool:
        """Freshly reload explicitly named clean asset packages; refuse all unsaved target packages."""
        _guard()
        packages = list(dict.fromkeys(_asset(path).get_outermost() for path in asset_paths))
        dirty = set(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())
        dirty.update(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())
        if dirty.intersection(packages):
            raise RuntimeError('Save or explicitly resolve dirty target packages before reloading')
        reloaded, error = unreal.EditorLoadingAndSavingUtils.reload_packages(
            packages, unreal.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
        if str(error) or not reloaded:
            raise RuntimeError('Package reload failed: ' + str(error))
        return True

    @toolset_registry.tool_call
    @staticmethod
    def close_clean_editor(expected_pid: int) -> str:
        """Close only the explicitly identified editor process, with no PIE or unsaved packages."""
        _guard()
        state = _status()
        if state['pid'] != expected_pid or state['dirty_content'] or state['dirty_maps']:
            raise RuntimeError('Editor identity or unsaved packages prevent shutdown')
        unreal.SystemLibrary.quit_editor()
        return json.dumps(state)

    @toolset_registry.tool_call
    @staticmethod
    def set_local_function_thread_safety(blueprint_path: str, function_name: str,
                                         b_thread_safe: bool, description: str) -> bool:
        """Set a local function's thread-safe declaration and description; undoable, no save/full compile.

        The declaration requires a reviewed function body. Compile and validate the Blueprint afterwards.
        Native/inherited/interface functions are rejected; no other metadata is changed.
        """
        _guard()
        path = _pose_path(blueprint_path)
        if (not path.startswith('/Game/') or not isinstance(function_name, str) or not function_name
                or len(function_name) > 128 or any(ord(c) < 32 for c in function_name)
                or not isinstance(b_thread_safe, bool) or not isinstance(description, str)
                or len(description) > 4096 or '\x00' in description):
            raise RuntimeError('Expected a project Blueprint, local function name, boolean and description up to 4096 characters')
        blueprint = _asset(path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise RuntimeError('Expected a Blueprint asset')
        if not unreal.RpgBlueprintAssetTools.set_local_function_thread_safety(blueprint, function_name, b_thread_safe, description):
            raise RuntimeError('Function metadata rejected: expected an editable locally authored function outside PIE')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def skeleton_bone_translation_modes(skeleton_path: str, bone_names_json: str) -> str:
        """Read authored translation retarget modes for explicitly named bones; no edits or saves."""
        _guard()
        names = _pose_names(bone_names_json)
        skeleton = _asset(_pose_path(skeleton_path))
        if not isinstance(skeleton, unreal.Skeleton):
            raise RuntimeError('Expected a Skeleton asset')
        modes = unreal.RpgBlueprintAssetTools.get_skeleton_bone_translation_modes(skeleton, names)
        if len(modes) != len(names):
            raise RuntimeError('Unknown, repeated or virtual bone, or unavailable editor context')
        return json.dumps({'skeleton': skeleton.get_path_name(), 'modes': {str(k): str(v) for k, v in modes.items()}})

    @toolset_registry.tool_call
    @staticmethod
    def set_skeleton_bone_translation_modes(skeleton_path: str, bone_modes_json: str) -> str:
        """Set explicit bone-to-mode pairs on one project Skeleton; validate all names, undoable, no recursion/save.

        Modes: Animation, Skeleton, AnimationScaled, AnimationRelative, OrientAndScale.
        This changes a skeleton-wide animation contract, not only the currently inspected clip.
        """
        _guard()
        path = _pose_path(skeleton_path)
        if not path.startswith('/Game/') or not isinstance(bone_modes_json, str) or len(bone_modes_json) > 8192:
            raise RuntimeError('Expected a project Skeleton path and a small bone-to-mode JSON object')
        pairs = json.loads(bone_modes_json)
        if not isinstance(pairs, dict):
            raise RuntimeError('Expected a bone-to-mode JSON object')
        names = _pose_names(json.dumps(list(pairs)))
        allowed = {'Animation', 'Skeleton', 'AnimationScaled', 'AnimationRelative', 'OrientAndScale'}
        if (len({name.casefold() for name in names}) != len(names)
                or any(not isinstance(mode, str) or mode not in allowed for mode in pairs.values())):
            raise RuntimeError('Expected unique bone names and canonical translation retarget mode names')
        skeleton = _asset(path)
        if not isinstance(skeleton, unreal.Skeleton):
            raise RuntimeError('Expected a Skeleton asset')
        before = unreal.RpgBlueprintAssetTools.get_skeleton_bone_translation_modes(skeleton, names)
        if len(before) != len(names):
            raise RuntimeError('Unknown or virtual bone, or unavailable editor context')
        count = unreal.RpgBlueprintAssetTools.set_skeleton_bone_translation_modes(skeleton, pairs)
        if count < 0:
            raise RuntimeError('Skeleton retarget modes rejected; no changes applied')
        after = unreal.RpgBlueprintAssetTools.get_skeleton_bone_translation_modes(skeleton, names)
        return json.dumps({'skeleton': skeleton.get_path_name(), 'changed': count,
                           'before': {str(k): str(v) for k, v in before.items()},
                           'after': {str(k): str(v) for k, v in after.items()}})

    @toolset_registry.tool_call
    @staticmethod
    def animation_pose_contract(asset_path: str, mesh_path: str, sample_times_json: str,
                                bone_names_json: str, b_should_retarget: bool) -> str:
        """Read compressed sequence poses on an explicit mesh without PIE, graph evaluation, edits or saves.

        Times are seconds (1..32 samples); names select 1..64 existing bones.
        Skeleton-global means component space, not actor/world space. Root-lock
        rules are respected; root motion is not extracted and additive clips are
        returned as full poses. This isolates sequence/compatible-skeleton evaluation.
        """
        _guard()
        times = _pose_array(sample_times_json, 32)
        names = _pose_names(bone_names_json)
        if (not isinstance(b_should_retarget, bool)
                or any(type(t) not in (int, float) or not math.isfinite(t) or t < 0 for t in times)):
            raise RuntimeError('Expected a retarget boolean and finite nonnegative sample seconds')
        sequence = _asset(_pose_path(asset_path))
        mesh = _asset(_pose_path(mesh_path))
        if not isinstance(sequence, unreal.AnimSequence) or not isinstance(mesh, unreal.SkeletalMesh):
            raise RuntimeError('Expected an AnimSequence and a SkeletalMesh')
        length = sequence.get_play_length()
        if any(t > length for t in times):
            raise RuntimeError('Sample time exceeds sequence duration')
        options = unreal.AnimPoseEvaluationOptions()
        settings = {'evaluation_type': unreal.AnimDataEvalType.COMPRESSED,
                    'should_retarget': b_should_retarget, 'extract_root_motion': False,
                    'incorporate_root_motion_into_pose': False, 'optional_skeletal_mesh': mesh,
                    'retrieve_additive_as_full_pose': True, 'evaluate_curves': True}
        for key, value in settings.items():
            options.set_editor_property(key, value)
        extension = unreal.AnimPoseExtensions
        records = []
        all_names = None
        for time in times:
            pose = extension.get_anim_pose_at_time(sequence, float(time), options)
            if not extension.is_valid(pose):
                raise RuntimeError('Sequence evaluation did not return a valid pose')
            pose_names = [str(n) for n in extension.get_bone_names(pose)]
            if len(pose_names) > 4096 or any(n not in pose_names for n in names):
                raise RuntimeError('Requested bone is absent, or pose exceeds the bone limit')
            if all_names is not None and all_names != pose_names:
                raise RuntimeError('Pose bone mapping changed between samples')
            all_names = pose_names
            bones = {}
            for name in names:
                bones[name] = {
                    'local': _pose_transform(extension.get_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)),
                    'skeleton_global': _pose_transform(extension.get_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD)),
                    'reference_local': _pose_transform(extension.get_ref_bone_pose(pose, name, unreal.AnimPoseSpaces.LOCAL)),
                    'reference_skeleton_global': _pose_transform(extension.get_ref_bone_pose(pose, name, unreal.AnimPoseSpaces.WORLD))}
            records.append({'time_seconds': time, 'bones': bones})
        return json.dumps({'sequence': sequence.get_path_name(), 'mesh': mesh.get_path_name(),
                           'sequence_skeleton': sequence.get_editor_property('skeleton').get_path_name(),
                           'mesh_skeleton': mesh.get_editor_property('skeleton').get_path_name(),
                           'length_seconds': length, 'bone_names': all_names,
                           'evaluation': {'data': 'Compressed', 'should_retarget': b_should_retarget,
                                          'extract_root_motion': False, 'ignore_root_lock': False,
                                          'additive_as_full_pose': True, 'evaluate_curves': True},
                           'sequence_root_motion': {
                               'enabled': sequence.get_editor_property('enable_root_motion'),
                               'force_root_lock': sequence.get_editor_property('force_root_lock'),
                               'root_lock': str(sequence.get_editor_property('root_motion_root_lock'))},
                           'samples': records}, allow_nan=False)

    @toolset_registry.tool_call
    @staticmethod
    def pie_mesh_bone_contract(mesh_path: str, bone_names_json: str) -> str:
        """Read the last evaluated bone transforms of one existing current-PIE mesh; never tick or alter it.

        Local uses ParentBoneSpace, component uses Component, and world includes
        the component transform. This is a snapshot of published transforms, not
        a forced evaluation or a guarantee that the mesh evaluated this frame.
        """
        names = _pose_names(bone_names_json)
        component = unreal.find_object(None, _pose_path(mesh_path))
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if (not world or not isinstance(component, unreal.SkeletalMeshComponent)
                or component.get_world() != world or not component.get_owner()
                or component.get_owner().get_world() != world):
            raise RuntimeError('Expected an existing SkeletalMeshComponent in the current PIE world')
        mesh = component.get_skeletal_mesh_asset()
        if not mesh or any(component.get_bone_index(name) < 0 for name in names):
            raise RuntimeError('Mesh asset or requested bone is missing')
        bones = {}
        for name in names:
            bones[name] = {
                'parent': str(component.get_parent_bone(name)),
                'local': _pose_transform(component.get_bone_transform(name, unreal.RelativeTransformSpace.RTS_PARENT_BONE_SPACE)),
                'component': _pose_transform(component.get_bone_transform(name, unreal.RelativeTransformSpace.RTS_COMPONENT)),
                'world': _pose_transform(component.get_bone_transform(name, unreal.RelativeTransformSpace.RTS_WORLD))}
        return json.dumps({'component': component.get_path_name(), 'owner': component.get_owner().get_path_name(),
                           'world': world.get_path_name(), 'world_time_seconds': unreal.GameplayStatics.get_time_seconds(world),
                           'mesh': mesh.get_path_name(),
                           'animation_instance': component.get_anim_instance().get_path_name() if component.get_anim_instance() else None,
                           'component_to_world': _pose_transform(component.get_world_transform()),
                           'observation': 'Last published pose; no tick or evaluation requested',
                           'bones': bones}, allow_nan=False)


def _pose_array(value, limit):
    if not isinstance(value, str) or len(value) > 8192:
        raise RuntimeError('Expected a small JSON array')
    result = json.loads(value)
    if not isinstance(result, list) or not 1 <= len(result) <= limit:
        raise RuntimeError('JSON array length must be 1..' + str(limit))
    return result


def _pose_names(value):
    names = _pose_array(value, 64)
    if any(not isinstance(n, str) or not n or len(n) > 128 or any(ord(c) < 32 for c in n) for n in names):
        raise RuntimeError('Expected nonempty bone names up to 128 characters')
    if len(set(names)) != len(names):
        raise RuntimeError('Bone names must be unique')
    return names


def _pose_path(value):
    if not isinstance(value, str) or not value.startswith('/') or len(value) > 1024 or any(ord(c) < 32 for c in value):
        raise RuntimeError('Expected an absolute Unreal asset or object path')
    return value


def _pose_transform(value):
    return {'translation_cm': [value.translation.x, value.translation.y, value.translation.z],
            'rotation_xyzw': [value.rotation.x, value.rotation.y, value.rotation.z, value.rotation.w],
            'scale': [value.scale3d.x, value.scale3d.y, value.scale3d.z]}


registration = Registration([AssetContractTools])
registration.register()
unreal.log('RPG_ASSET_CONTRACT_TOOLS_READY')
