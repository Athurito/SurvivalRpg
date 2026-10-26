"""Optional editor-only MCP AI authoring tools.

Import in the editor's Python bootstrap after SurvivalRpgEditor is loaded. The engine's StateTree toolset only
inspects; this toolset writes a StateTree from a JSON description through URpgAIAssetTools. Behavior stays
designer-authored data in the description; no state, task or tuning is built into the tool.
"""
import json

import unreal
import toolset_registry
from toolset_registry.registration import Registration


def _guard():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop PIE before authoring AI assets')


@unreal.uclass()
class AIAssetTools(unreal.ToolsetDefinition):
    """Author project AI assets, such as StateTrees, that the engine toolsets can only inspect."""

    @toolset_registry.tool_call
    @staticmethod
    def build_state_tree(asset_path: str, description_json: str) -> str:
        """Replace a StateTree's evaluators, global tasks and states with description_json and compile it; does not save.

        description_json is an object:
          {"evaluators": [node], "global_tasks": [node], "root": state}
        state: {"name": unique name, "type": "State"|"Group", "selection": EStateTreeStateSelectionBehavior name,
                "tasks_completion": "Any"|"All", "description": text, "enter_conditions": [node], "tasks": [node],
                "transitions": [transition], "children": [state]}
        node: {"struct": "/Script/Module.StructName", "values": {"Property": Unreal text value},
               "operand": "And"|"Or", "indent": 0..4}
          Values are set on the node's instance data first, then on the node itself (for example bInvert), in
          ImportText form: numbers, True/False, enum names, (A,B) arrays, (GameplayTags=((TagName="A.B"))) containers.
        transition: {"trigger": EStateTreeTransitionTrigger name, "to": state name or "#Succeeded"|"#Failed"|
                     "#NextState"|"#NextSelectableState", "event": tag for OnEvent, "priority": name,
                     "delay": seconds, "delay_variance": seconds, "conditions": [node]}
        Returns the asset path; raises with every error found.
        """
        _guard()
        if not isinstance(description_json, str):
            raise RuntimeError('description_json must be a JSON string')
        json.loads(description_json)
        state_tree = unreal.load_asset(asset_path)
        if not isinstance(state_tree, unreal.StateTree):
            raise RuntimeError('Not a StateTree asset: ' + asset_path)
        errors = unreal.RpgAIAssetTools.build_state_tree_from_json(state_tree, description_json)
        if errors:
            raise RuntimeError(errors)
        return state_tree.get_path_name()


registration = Registration([AIAssetTools])
registration.register()
unreal.log('RPG_AI_ASSET_TOOLS_READY')
