"""Optional editor-only MCP animation authoring tools.

Import in the editor's Python bootstrap after SurvivalRpgEditor is loaded. Asset
content remains designer-authored: no gameplay graph, class or asset path is built
into this toolset. Existing Blueprint/Object tools own graph nodes, CDO defaults,
ordinary property edits, compilation and explicit saves.
"""
import json
import math

import unreal
import toolset_registry
from toolset_registry.registration import Registration


def _guard():
    if unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world():
        raise RuntimeError('Stop PIE before authoring animation assets')


def _package(path):
    if not isinstance(path, str) or not unreal.RpgAnimationAssetTools.is_project_content_package(path):
        raise RuntimeError('Expected a mounted package path inside this project or its plugins')
    return path


def _asset(path, expected_type):
    _package(path.split('.', 1)[0])
    value = unreal.load_asset(path)
    if not isinstance(value, expected_type):
        raise RuntimeError('Asset has wrong type or is missing: ' + path)
    return value


def _class(path, expected_type):
    value = unreal.load_class(None, path)
    base = unreal.load_class(None, expected_type) if isinstance(expected_type, str) else expected_type.static_class()
    if not value or not base or not unreal.MathLibrary.class_is_child_of(value, base):
        raise RuntimeError('Missing or incompatible class: ' + path)
    return value


def _create(path, asset_class, factory):
    _package(path)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        raise RuntimeError('Creation never overwrites an existing asset: ' + path)
    folder, name = path.rsplit('/', 1)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, asset_class, factory)
    if not isinstance(asset, asset_class):
        raise RuntimeError('Factory did not create the requested asset type: ' + path)
    return asset.get_path_name()


@unreal.uclass()
class AnimationAssetTools(unreal.ToolsetDefinition):
    """Author project animation asset structure, using Unreal's native editor mechanisms."""

    @toolset_registry.tool_call
    @staticmethod
    def create_anim_blueprint(asset_path: str, parent_class_path: str,
                              skeleton_path: str, is_template: bool = False) -> str:
        """Create a new AnimBP or data-only derived AnimBP; compile parent first, set child defaults separately.

        Use an empty skeleton_path only for a skeleton-independent template.
        Returns the new asset object path; no implicit save.
        """
        _guard()
        parent = _class(parent_class_path, unreal.AnimInstance)
        skeleton = _asset(skeleton_path, unreal.Skeleton) if skeleton_path else None
        if is_template == bool(skeleton):
            raise RuntimeError('Templates require no skeleton; concrete AnimBPs require a skeleton')
        factory = unreal.AnimBlueprintFactory()
        factory.set_editor_property('parent_class', parent)
        factory.set_editor_property('target_skeleton', skeleton)
        factory.set_editor_property('template', is_template)
        return _create(asset_path, unreal.AnimBlueprint, factory)

    @toolset_registry.tool_call
    @staticmethod
    def create_animation_layer_interface(asset_path: str) -> str:
        """Create a new real Anim Layer Interface; add named layers, then compile before implementing it."""
        _guard()
        return _create(asset_path, unreal.AnimBlueprint, unreal.AnimLayerInterfaceFactory())

    @toolset_registry.tool_call
    @staticmethod
    def set_anim_blueprint_abstract(asset_path: str, abstract: bool) -> bool:
        """Set an AnimBP's engine class option; compile afterwards to apply the class flag.

        Use abstract for reusable graph bases without a concrete animation set. Concrete
        data children remain instantiable and must pass their inherited asset validation.
        """
        _guard()
        blueprint = _asset(asset_path, unreal.AnimBlueprint)
        with unreal.ScopedEditorTransaction('Set Animation Blueprint Abstract Class'):
            blueprint.modify()
            blueprint.set_editor_property('generate_abstract_class', abstract)
        return bool(blueprint.get_editor_property('generate_abstract_class')) == abstract

    @toolset_registry.tool_call
    @staticmethod
    def implement_animation_layer_interface(asset_path: str, interface_class_path: str) -> bool:
        """Implement a compiled Anim Layer Interface using the engine's native interface graph setup; compile afterwards."""
        _guard()
        blueprint = _asset(asset_path, unreal.AnimBlueprint)
        interface = _class(interface_class_path, '/Script/Engine.AnimLayerInterface')
        if not unreal.RpgBlueprintAssetTools.implement_interface(blueprint, interface):
            raise RuntimeError('Animation layer interface implementation failed')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def add_animation_layer(asset_path: str, layer_name: str, input_pose_name: str = 'InputPose',
                            inputs_json: str = '[]') -> str:
        """Add an engine AnimationGraph with an optional pose and context signature; does not overwrite.

        inputs_json is [{"Name":"Speed","Type":{"PinCategory":"real","PinSubCategory":"float"}}].
        FEdGraphPinType supports struct/object types through PinSubCategoryObject paths. Empty pose name
        creates a layer without inputs. Compile interface before implementing it on other AnimBPs.
        """
        _guard()
        graph = unreal.RpgAnimationAssetTools.add_animation_layer(
            _asset(asset_path, unreal.AnimBlueprint), layer_name, input_pose_name, inputs_json)
        if not graph:
            raise RuntimeError('Animation layer signature was rejected; inspect editor diagnostics')
        return graph.get_path_name()

    @toolset_registry.tool_call
    @staticmethod
    def configure_linked_animation_layer(node_path: str, interface_class_path: str,
                                         layer_name: str, instance_class_path: str = '') -> bool:
        """Configure an existing LinkedAnimLayer node and reconstruct pins; empty instance uses externally linked layer."""
        _guard()
        node = unreal.find_object(None, node_path)
        if not isinstance(node, unreal.AnimGraphNode_LinkedAnimLayer):
            raise RuntimeError('Expected an existing LinkedAnimLayer editor node path')
        interface = _class(interface_class_path, '/Script/Engine.AnimLayerInterface')
        instance = _class(instance_class_path, unreal.AnimInstance) if instance_class_path else None
        if not unreal.RpgAnimationAssetTools.configure_linked_animation_layer(node, interface, layer_name, instance):
            raise RuntimeError('Linked layer binding was rejected')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def set_local_function_thread_safety(asset_path: str, function_name: str, thread_safe: bool) -> bool:
        """Set serialized function-entry metadata. This declares thread-safety intent; compile and review actual calls."""
        _guard()
        if not unreal.RpgAnimationAssetTools.set_local_function_thread_safety(
                _asset(asset_path, unreal.Blueprint), function_name, thread_safe):
            raise RuntimeError('Expected a function declared in this Blueprint with one function entry')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def set_variable_tooltip(asset_path: str, variable_name: str, tooltip: str) -> bool:
        """Document a Blueprint's own variable using serialized engine metadata; no compile or save."""
        _guard()
        if not unreal.RpgAnimationAssetTools.set_variable_tooltip(
                _asset(asset_path, unreal.Blueprint), variable_name, tooltip):
            raise RuntimeError('Expected a local Blueprint variable and a nonempty tooltip')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def create_linked_animation_layer_node(graph_path: str, interface_class_path: str, layer_name: str) -> str:
        """Create an interface-bound linked-layer node using the native graph schema, without menu-index dependence."""
        _guard()
        graph = unreal.find_object(None, graph_path)
        if not isinstance(graph, unreal.EdGraph):
            raise RuntimeError('Expected an existing animation graph')
        interface = _class(interface_class_path, '/Script/Engine.AnimLayerInterface')
        node = unreal.RpgAnimationAssetTools.create_linked_animation_layer_node(graph, interface, layer_name)
        if not node:
            raise RuntimeError('Linked layer node creation failed')
        return node.get_path_name()

    @toolset_registry.tool_call
    @staticmethod
    def register_animation_slot(skeleton_path: str, slot_name: str) -> bool:
        """Register a named default-group slot on a project skeleton and mark it dirty; never changes bone retargeting."""
        _guard()
        if not unreal.RpgAnimationAssetTools.register_animation_slot(_asset(skeleton_path, unreal.Skeleton), slot_name):
            raise RuntimeError('Animation slot registration failed')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def create_montage_from_sequence(asset_path: str, sequence_path: str) -> str:
        """Create a single-segment montage with the source sequence's skeleton and duration.

        Uses the engine montage factory, including its default section. Configure slots,
        blend times and other presentation settings separately, then save explicitly.
        Existing assets and source sequences are never changed by this operation.
        """
        _guard()
        sequence = _asset(sequence_path, unreal.AnimSequence)
        factory = unreal.AnimMontageFactory()
        factory.set_editor_property('target_skeleton', sequence.get_editor_property('skeleton'))
        factory.set_editor_property('source_animation', sequence)
        return _create(asset_path, unreal.AnimMontage, factory)

    @toolset_registry.tool_call
    @staticmethod
    def create_blend_space(asset_path: str, skeleton_path: str, one_dimensional: bool = False) -> str:
        """Create a new BlendSpace. Configure axis ranges/tuning using ObjectTools before adding samples."""
        _guard()
        factory = unreal.BlendSpaceFactory1D() if one_dimensional else unreal.BlendSpaceFactoryNew()
        factory.set_editor_property('target_skeleton', _asset(skeleton_path, unreal.Skeleton))
        asset_class = unreal.BlendSpace1D if one_dimensional else unreal.BlendSpace
        return _create(asset_path, asset_class, factory)

    @toolset_registry.tool_call
    @staticmethod
    def set_blend_space_samples(asset_path: str, samples_json: str) -> int:
        """Replace complete samples and rebuild interpolation data; default rate 1, unmirrored.

        samples_json is [{"animation":"/Mount/Sequence","position":[x,y,z]}].
        Coordinates must lie within the already authored axis ranges. Does not save.
        """
        _guard()
        samples = json.loads(samples_json)
        if not isinstance(samples, list) or not 1 <= len(samples) <= 1024:
            raise RuntimeError('Expected 1..1024 explicit samples')
        animations, positions = [], []
        for sample in samples:
            if not isinstance(sample, dict) or set(sample) != {'animation', 'position'}:
                raise RuntimeError('Each sample needs exactly animation and position')
            position = sample['position']
            if (not isinstance(position, list) or len(position) != 3
                    or any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in position)):
                raise RuntimeError('Sample position must contain three finite coordinates')
            animations.append(_asset(sample['animation'], unreal.AnimSequence))
            positions.append(unreal.Vector(*position))
        count = unreal.RpgAnimationAssetTools.set_blend_space_samples(
            _asset(asset_path, unreal.BlendSpace), animations, positions)
        if count < 0:
            raise RuntimeError('BlendSpace rejected samples; check ranges, duplicates, skeleton and additive compatibility')
        return count

    @toolset_registry.tool_call
    @staticmethod
    def create_chooser(asset_path: str) -> str:
        """Create an empty root Chooser using the engine factory, then configure its signature/rows explicitly."""
        _guard()
        return _create(asset_path, unreal.ChooserTable, unreal.ChooserTableFactory())

    @toolset_registry.tool_call
    @staticmethod
    def configure_chooser(asset_path: str, configuration_json: str) -> bool:
        """Configure explicit Chooser fields and compile bindings; records undo and never saves.

        Allowed fields: OutputObjectType, ResultType, ContextData, ColumnsStructs, ResultsStructs,
        FallbackResult, DisabledRows. Concrete structs use _structType (e.g. /Script/Chooser.AssetChooser).
        Unknown fields, wrong struct families and mismatched row counts fail before the asset is edited.
        Existing nested tables/objects require their own authoring workflow and are rejected here.
        """
        _guard()
        if not unreal.RpgAnimationAssetTools.configure_chooser(_asset(asset_path, unreal.ChooserTable), configuration_json):
            raise RuntimeError('Chooser configuration rejected; inspect editor diagnostics')
        return True


registration = Registration([AnimationAssetTools])
registration.register()
unreal.log('RPG_ANIMATION_ASSET_TOOLS_READY')
