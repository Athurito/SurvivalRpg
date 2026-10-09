"""Optional editor-only MCP tools for asset contracts, owned references and local PIE inspection.

Load from an editor Python startup script. Standard asset/Blueprint/object tools
still own duplication, property edits, compilation and saves. These operations
fill gaps in the UE 5.8 toolsets: complete exports, instanced reference remapping,
precise montage notify timing, fresh package reloads, typed function inputs, MVVM function
bindings and gameplay input/view inspection in PIE.
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
    def implement_blueprint_interface(blueprint_path: str, interface_class_path: str) -> bool:
        """Implement a native or compiled Blueprint interface on a project-owned Blueprint.

        Pass a native class path or a generated Blueprint interface class path
        ending in _C. Already implemented or inherited interfaces are unchanged.
        The native helper owns the transaction; compile and save explicitly afterwards.
        """
        _guard()
        if (not isinstance(blueprint_path, str)
                or not unreal.RpgAnimationAssetTools.is_project_content_package(
                    blueprint_path.split('.', 1)[0])):
            raise RuntimeError('Expected a Blueprint inside this project or its plugins')
        blueprint = _asset(blueprint_path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise ValueError('Expected a Blueprint')
        if not unreal.RpgAnimationAssetTools.is_project_content_package(blueprint.get_outermost().get_name()):
            raise RuntimeError('Resolved Blueprint is not owned by this project or its plugins')
        interface = unreal.load_class(None, interface_class_path)
        interface_base = unreal.load_class(None, '/Script/CoreUObject.Interface')
        if (not interface or not interface_base
                or not unreal.MathLibrary.class_is_child_of(interface, interface_base)):
            raise ValueError('Expected a native or compiled Blueprint interface class: ' + interface_class_path)
        if not unreal.RpgBlueprintAssetTools.implement_interface(blueprint, interface):
            raise RuntimeError('Blueprint interface implementation failed')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def ensure_event_graph(blueprint_path: str) -> bool:
        """Add the standard EventGraph to a project Blueprint that has none, so events can be authored.

        Widget Blueprints authored without graphs have no EventGraph, and the engine
        BlueprintTools only add events to an existing one. An existing EventGraph is
        left unchanged. Compile and save the Blueprint afterwards.
        """
        _guard()
        if (not isinstance(blueprint_path, str)
                or not unreal.RpgAnimationAssetTools.is_project_content_package(
                    blueprint_path.split('.', 1)[0])):
            raise RuntimeError('Expected a Blueprint inside this project or its plugins')
        blueprint = _asset(blueprint_path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise ValueError('Expected a Blueprint')
        if not unreal.RpgBlueprintAssetTools.ensure_event_graph(blueprint):
            raise RuntimeError('Could not add an EventGraph to this Blueprint')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_variable_tooltips(blueprint_path: str, tooltips_json: str) -> bool:
        """Document locally authored Blueprint members from a name-to-tooltip JSON object; no compile or save."""
        _guard()
        if not blueprint_path.startswith('/Game/'):
            raise RuntimeError('Only project-owned assets can be edited')
        tooltips = json.loads(tooltips_json)
        if (not isinstance(tooltips, dict) or not tooltips
                or any(not isinstance(name, str) or not name.strip()
                       or not isinstance(tooltip, str) or not tooltip.strip()
                       for name, tooltip in tooltips.items())):
            raise ValueError('Expected a nonempty mapping of variable names to nonempty tooltip strings')
        blueprint = _asset(blueprint_path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise ValueError('Expected a Blueprint')
        own_names = {str(name).lower()
                     for name in unreal.BlueprintEditorLibrary.list_member_variable_names(blueprint, False)}
        if any(name.lower() not in own_names for name in tooltips):
            raise ValueError('All tooltip targets must be locally authored Blueprint variables')
        if len({name.lower() for name in tooltips}) != len(tooltips):
            raise ValueError('Variable names must be unique ignoring case')
        with unreal.ScopedEditorTransaction('Set Blueprint Variable Tooltips'):
            blueprint.modify()
            for name, tooltip in tooltips.items():
                if not unreal.RpgAnimationAssetTools.set_variable_tooltip(blueprint, name, tooltip):
                    raise RuntimeError('Could not set variable tooltip; undo the last transaction: ' + name)
        return True

    @toolset_registry.tool_call
    @staticmethod
    def add_soft_class_variable(blueprint_path: str, variable_name: str,
                                base_class_path: str, tooltip: str) -> bool:
        """Add an instance-editable soft class member with serialized tooltip metadata.

        Use a native or generated class object path for base_class_path. The
        member starts empty; compile, assign its soft class default and save
        explicitly using the standard tools. Existing names are never replaced.
        """
        _guard()
        if not blueprint_path.startswith('/Game/'):
            raise RuntimeError('Only project-owned assets can be edited')
        if not variable_name.isidentifier() or variable_name.lower() == 'none' or not tooltip.strip():
            raise ValueError('Expected an identifier for the new variable and a nonempty tooltip')
        blueprint = _asset(blueprint_path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise ValueError('Expected a Blueprint')
        base_class = unreal.load_class(None, base_class_path)
        if not base_class:
            raise ValueError('Cannot load base class: ' + base_class_path)
        library = unreal.BlueprintEditorLibrary
        pin_type = library.get_class_reference_type(base_class)
        pin_text = pin_type.export_text()
        if 'PinCategory="class"' not in pin_text:
            raise ValueError('Base class does not support Blueprint variables: ' + base_class_path)
        # AddMemberVariable otherwise silently picks a different, unique name.
        names = [str(name).rsplit('.', 1)[-1] for name in library.list_member_variable_names(blueprint)]
        names.extend(str(name) for name in library.list_graph_names(blueprint))
        names.extend(str(info.get_editor_property('name'))
                     for info in list(library.list_functions(blueprint)) + list(library.list_events(blueprint)))
        if variable_name.lower() in {name.lower() for name in names}:
            raise ValueError('Blueprint member or graph name is already in use: ' + variable_name)
        # FEdGraphPinType fields are not Python editor properties in UE 5.8.
        # Struct text import preserves the base class and all remaining type data.
        pin_type.import_text(pin_text.replace('PinCategory="class"', 'PinCategory="softclass"', 1))
        if 'PinCategory="softclass"' not in pin_type.export_text():
            raise RuntimeError('Could not construct a soft class pin type')
        with unreal.ScopedEditorTransaction('Add Blueprint Soft Class Variable'):
            blueprint.modify()
            if not library.add_member_variable(blueprint, variable_name, pin_type):
                raise RuntimeError('Could not add soft class variable: ' + variable_name)
            added_type = library.get_member_variable_type(blueprint, variable_name)
            if added_type is None or 'PinCategory="softclass"' not in added_type.export_text():
                raise RuntimeError('Editor did not create the requested soft class member; undo the last transaction')
            library.set_blueprint_variable_instance_editable(blueprint, variable_name, True)
            if not unreal.RpgAnimationAssetTools.set_variable_tooltip(blueprint, variable_name, tooltip):
                raise RuntimeError('Could not set variable tooltip; undo the last transaction')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def add_function_input(blueprint_path: str, function_name: str, param_name: str, type_path: str) -> bool:
        """Add an input of an enum, struct or object type to an own Blueprint function graph.

        BlueprintTools.add_function_param only knows basic types. Pass the type's
        object path: a UEnum such as /Script/SurvivalRpg.ERpgItemRarity, a
        UScriptStruct or a UClass (object reference). Existing inputs are never
        replaced. Write the graph with write_graph_dsl afterwards, which compiles.
        """
        _guard()
        if not blueprint_path.startswith('/Game/'):
            raise RuntimeError('Only project-owned assets can be edited')
        if not param_name.isidentifier():
            raise ValueError('Expected an identifier for the new input')
        blueprint = _asset(blueprint_path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise ValueError('Expected a Blueprint')
        graph = unreal.BlueprintEditorLibrary.find_graph(blueprint, function_name)
        if graph is None:
            raise ValueError('Blueprint has no own function graph named ' + function_name)
        editor = unreal.BlueprintGraphEditor.get_graph_editor(graph)
        type_object = unreal.load_object(None, type_path)
        if isinstance(type_object, unreal.Enum):
            category = 'byte'
        elif isinstance(type_object, unreal.ScriptStruct):
            category = 'struct'
        elif isinstance(type_object, unreal.Class):
            category = 'object'
        else:
            raise ValueError('Expected an enum, struct or class path: ' + type_path)
        # FEdGraphPinType fields are not Python editor properties in UE 5.8; build the type through struct text.
        pin_type = unreal.BlueprintEditorLibrary.get_basic_type_by_name('byte')
        sub_object = '"{}\'{}\'"'.format(type_object.get_class().get_path_name(), type_object.get_path_name())
        pin_type.import_text(pin_type.export_text()
                             .replace('PinCategory="byte"', 'PinCategory="{}"'.format(category), 1)
                             .replace('PinSubCategoryObject=None', 'PinSubCategoryObject=' + sub_object, 1))
        if type_object.get_path_name() not in pin_type.export_text():
            raise RuntimeError('Could not construct a pin type for ' + type_path)
        with unreal.ScopedEditorTransaction('Add Blueprint Function Input'):
            blueprint.modify()
            if not editor.add_graph_input_parameter(param_name, pin_type):
                raise RuntimeError('Could not add function input: ' + param_name)
        return True

    @toolset_registry.tool_call
    @staticmethod
    def add_view_function_binding(widget_blueprint_path: str, view_model_class_path: str,
                                  source_property: str, function_name: str) -> str:
        """Bind a view model property one way to a function of the Widget Blueprint itself.

        MVVMToolset.CreateViewBinding only binds properties to properties. A
        widget function with one input, for example SetRarityRing(Rarity),
        receives the value whenever the field changes. The view model must
        already be on the widget. Returns the new binding id, or the existing
        one when the same binding exists. Compile and save explicitly afterwards.
        """
        _guard()
        if not widget_blueprint_path.startswith('/Game/'):
            raise RuntimeError('Only project-owned assets can be edited')
        blueprint = _asset(widget_blueprint_path)
        if not isinstance(blueprint, unreal.WidgetBlueprint):
            raise ValueError('Expected a Widget Blueprint')
        if str(function_name) not in [str(n) for n in unreal.BlueprintEditorLibrary.list_graph_names(blueprint)]:
            raise ValueError('Widget Blueprint has no own function named ' + function_name)
        view_model_class = unreal.load_class(None, view_model_class_path)
        if not view_model_class or not source_property.isidentifier():
            raise ValueError('Expected a loadable view model class and a property name')
        view = unreal.get_editor_subsystem(unreal.MVVMEditorSubsystem).get_view(blueprint)
        class_text = "'{}'".format(view_model_class.get_path_name())
        context_id = None
        for context in view.get_editor_property('available_view_models'):
            text = context.export_text()
            if class_text in text:
                context_id = text.split('ViewModelContextId=', 1)[1].split(',', 1)[0]
        if context_id is None:
            raise ValueError('Add the view model to the widget first: ' + view_model_class_path)
        bindings = list(view.get_editor_property('bindings'))
        for binding in bindings:
            text = binding.export_text()
            if ('MemberName="{}"'.format(source_property) in text and 'MemberName="{}"'.format(function_name) in text
                    and 'ContextId=' + context_id in text):
                return text.split('BindingId=', 1)[1].split(',', 1)[0]
        binding_id = unreal.GuidLibrary.new_guid().to_string()
        binding = unreal.MVVMBlueprintViewBinding()
        binding.import_text(
            '(SourcePath=(Paths=((BindingReference=(MemberParent="/Script/CoreUObject.Class{cls}",MemberName="{src}"),'
            'BindingKind=Property)),WidgetName="",ContextId={ctx},Source=ViewModel,bIsComponent=False,bDeprecatedSource=True),'
            'DestinationPath=(Paths=((BindingReference=(MemberName="{fn}",bSelfContext=True),BindingKind=Function)),'
            'WidgetName="",ContextId=00000000000000000000000000000000,Source=SelfContext,bIsComponent=False,'
            'bDeprecatedSource=True),BindingType=OneWayToDestination,bOverrideExecutionMode=False,'
            'OverrideExecutionMode=Immediate,Conversion=(DestinationToSourceConversion=None,'
            'SourceToDestinationConversion=None),BindingId={id},bEnabled=True,bCompile=True)'.format(
                cls=class_text, src=source_property, ctx=context_id, fn=function_name, id=binding_id))
        if 'MemberName="{}"'.format(function_name) not in binding.export_text():
            raise RuntimeError('Could not construct the view binding')
        with unreal.ScopedEditorTransaction('Add MVVM Function Binding'):
            view.modify()
            bindings.append(binding)
            view.set_editor_property('bindings', bindings)
        return binding_id

    @toolset_registry.tool_call
    @staticmethod
    def editable_blueprint_component(blueprint_path: str, component_name: str) -> unreal.ActorComponent:
        """Resolve one editable component for this Blueprint, creating an inherited override when necessary.

        Standard ActorTools exposes the ancestor SCS template. This uses the editor's
        SubobjectData API to author a child-only override instead. No values, compile,
        or save are performed; callers use ObjectTools and BlueprintTools afterwards.
        """
        _guard()
        blueprint = _asset(blueprint_path)
        if not isinstance(blueprint, unreal.Blueprint):
            raise ValueError('Expected an actor Blueprint')
        subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
        library = unreal.SubobjectDataBlueprintFunctionLibrary
        matches = []
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
            data = library.get_data(handle)
            obj = library.get_associated_object(data)
            if isinstance(obj, unreal.ActorComponent) and obj.get_name() == component_name:
                matches.append(data)
        if len(matches) != 1:
            raise ValueError('Expected exactly one named component; found ' + str(len(matches)))
        blueprint.modify()
        result = library.get_object_for_blueprint(matches[0], blueprint)
        if not isinstance(result, unreal.ActorComponent) or result.get_outermost() != blueprint.get_outermost():
            raise RuntimeError('Editor did not provide a component owned by the requested Blueprint')
        return result

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
