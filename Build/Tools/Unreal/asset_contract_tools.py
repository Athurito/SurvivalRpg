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


registration = Registration([AssetContractTools])
registration.register()
unreal.log('RPG_ASSET_CONTRACT_TOOLS_READY')
