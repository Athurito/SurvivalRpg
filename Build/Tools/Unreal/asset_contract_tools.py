"""Optional editor-only MCP tools for reflected asset contracts and owned references.

Load from an editor Python startup script. Standard asset/Blueprint/object tools
still own duplication, property edits, compilation and saves. These operations
fill gaps in the UE 5.8 toolsets: complete exports, instanced reference remapping,
precise montage notify timing and fresh package reloads.
"""
import json
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
