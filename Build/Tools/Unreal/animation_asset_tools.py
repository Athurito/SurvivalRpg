"""Optional editor-only MCP animation authoring tools.

Import in the editor's Python bootstrap after SurvivalRpgEditor is loaded. Asset
content remains designer-authored: no gameplay graph, class or asset path is built
into this toolset. Existing Blueprint/Object tools own graph nodes, CDO defaults,
ordinary property edits, compilation and explicit saves.
"""
import json
import math
import struct

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
    def animation_notify_contract(asset_path: str) -> str:
        """Read animation tracks, complete notify records and precise engine timing; never edit or save.

        Numeric notify times are public engine trigger times, including its offset;
        author time/offset and all remaining fields stay in the complete export record.
        Instant/state objects include their actual class, outer and ownership. Sequence
        sync markers are separate from notifies; other animation types return null for
        markers. Non-finite numeric data fails JSON serialization instead of being hidden.
        """
        _guard()
        animation = _asset(asset_path, unreal.AnimSequenceBase)
        library = unreal.AnimationLibrary

        def object_contract(value):
            if value is None:
                return None
            outer = value.get_outer()
            return {'path': value.get_path_name(), 'class': value.get_class().get_path_name(),
                    'outer': outer.get_path_name() if outer else None,
                    'owned_by_animation': outer == animation}

        tracks = [str(name) for name in library.get_animation_notify_track_names(animation)]
        events = []
        for index, track in enumerate(tracks):
            for event in library.get_animation_notify_events_for_track(animation, track):
                events.append({'track_index': index, 'track_name': track,
                               'trigger_time_seconds': library.get_anim_notify_event_trigger_time(event),
                               'duration_seconds': library.get_anim_notify_event_duration(event),
                               'notify': object_contract(event.get_editor_property('notify')),
                               'notify_state': object_contract(event.get_editor_property('notify_state_class')),
                               'record': event.export_text()})
        if sorted(event['record'] for event in events) != sorted(
                event.export_text() for event in library.get_animation_notify_events(animation)):
            raise ValueError('Not every notify belongs to a valid named track')
        markers = None
        if isinstance(animation, unreal.AnimSequence):
            markers = []
            for index, track in enumerate(tracks):
                for marker in library.get_animation_sync_markers_for_track(animation, track):
                    markers.append({'name': str(marker.get_editor_property('marker_name')),
                                    'time_seconds': marker.get_editor_property('time'),
                                    'track_index': index, 'track_name': track,
                                    'record': marker.export_text()})
            if sorted(marker['record'] for marker in markers) != sorted(
                    marker.export_text() for marker in library.get_animation_sync_markers(animation)):
                raise ValueError('Not every sync marker belongs to a valid named track')
        return json.dumps({'asset': animation.get_path_name(), 'class': animation.get_class().get_path_name(),
                           'length_seconds': animation.get_play_length(), 'tracks': tracks,
                           'event_count': len(events), 'events': events, 'sync_markers': markers,
                           'read_only': True}, allow_nan=False)

    @toolset_registry.tool_call
    @staticmethod
    def add_animation_notifies(asset_path: str, track_name: str, events_json: str) -> str:
        """Add explicit instant notifies on one new track, with undo and no implicit save.

        events_json is [{"time_seconds":number,"notify_class_path":"/Mount/Notify.Notify_C"}].
        Uses class defaults; notify states and event-property overrides are not accepted.
        All inputs are checked before changing the animation. Existing event records and
        sync markers are preserved, although the engine may sort their array order.
        An existing track or duplicate class/time rejects the request, including retries.
        The caller owns asset selection and any derived-asset/provenance restriction.
        """
        _guard()
        animation = _asset(asset_path, unreal.AnimSequenceBase)
        if (not isinstance(track_name, str) or not track_name.strip() or track_name != track_name.strip()
                or track_name.casefold() == 'none' or len(track_name) > 128 or '\x00' in track_name):
            raise ValueError('Expected a nonempty new notify track name, at most 128 characters')
        events = json.loads(events_json)
        if not isinstance(events, list) or not 1 <= len(events) <= 512:
            raise ValueError('Expected 1..512 explicit instant notify events')
        library = unreal.AnimationLibrary
        tracks_before = [str(name) for name in library.get_animation_notify_track_names(animation)]
        if library.is_valid_anim_notify_track_name(animation, track_name):
            raise ValueError('The requested track already exists; no events were changed')
        length = animation.get_play_length()
        if not math.isfinite(length) or length <= 0:
            raise ValueError('Expected an animation with a finite positive duration')
        old_events = list(library.get_animation_notify_events(animation))
        old_records = sorted(event.export_text() for event in old_events)
        old_markers = (sorted(marker.export_text() for marker in library.get_animation_sync_markers(animation))
                       if isinstance(animation, unreal.AnimSequence) else None)
        # The public getter includes UE's +/-1e-4 s trigger offset. Conservatively
        # reject potential duplicates within both offsets plus float32 rounding;
        # private author-time/offset properties are not exposed to Python.
        def coincident_existing_time(first, second):
            return math.isclose(first, second, rel_tol=2. ** -23, abs_tol=2.1e-4)

        old_pairs, tracked_records = [], []
        for track in tracks_before:
            times = []
            for event in library.get_animation_notify_events_for_track(animation, track):
                # RefreshCacheData repairs invalid/overlapping tracks. Reject those
                # inputs beforehand so this append operation cannot edit old records.
                time = library.get_anim_notify_event_trigger_time(event)
                if not math.isfinite(time):
                    raise ValueError('Existing notify has an invalid time')
                if any(coincident_existing_time(time, other) for other in times):
                    raise ValueError('Existing same-track notify times may overlap; repair them separately')
                times.append(time)
                tracked_records.append(event.export_text())
                notify = event.get_editor_property('notify')
                if notify:
                    old_pairs.append((notify.get_class().get_path_name(), time))
        if sorted(tracked_records) != old_records:
            raise ValueError('Existing notify has an invalid track')
        prepared, classes, new_times = [], {}, []
        for event in events:
            if not isinstance(event, dict) or set(event) != {'time_seconds', 'notify_class_path'}:
                raise ValueError('Each event requires exactly time_seconds and notify_class_path')
            time, class_path = event['time_seconds'], event['notify_class_path']
            if type(time) not in (int, float) or not math.isfinite(time) or not 0 <= time <= length:
                raise ValueError('Notify time must be finite and within the animation duration')
            # Engine event times are floats. Check collisions after the same conversion.
            time = struct.unpack('f', struct.pack('f', float(time)))[0]
            if any(abs(time - other) <= 1.e-8 for other in new_times):
                raise ValueError('A single new track cannot contain coincident notify times')
            if not isinstance(class_path, str) or not class_path.startswith('/'):
                raise ValueError('Expected an explicit notify class path')
            if class_path not in classes:
                cls = _class(class_path, unreal.AnimNotify)
                if cls.get_name().startswith(('SKEL_', 'REINST_', 'TRASHCLASS_')):
                    raise ValueError('Compile the notify Blueprint before adding its events')
                # Python NewObject rejects abstract classes before its native creation.
                # The validation instance belongs to the transient package, never the asset/CDO.
                probe = unreal.new_object(cls)
                if not isinstance(probe, unreal.AnimNotify):
                    raise ValueError('Notify class could not be instantiated')
                classes[class_path] = (cls, probe)
            cls = classes[class_path][0]
            if any(name == cls.get_path_name() and coincident_existing_time(time, other)
                   for name, other in old_pairs):
                raise ValueError('An event with this notify class and coincident trigger time may already exist')
            prepared.append((time, cls))
            new_times.append(time)

        def unchanged_existing(created):
            records = sorted(event.export_text() for event in library.get_animation_notify_events(animation)
                             if event.get_editor_property('notify') not in created)
            markers = (sorted(marker.export_text() for marker in library.get_animation_sync_markers(animation))
                       if old_markers is not None else None)
            return records == old_records and markers == old_markers

        created = []
        with unreal.ScopedEditorTransaction('Add animation notifies on a new track'):
            animation.modify()
            try:
                library.add_animation_notify_track(animation, track_name)
                for time, cls in sorted(prepared, key=lambda item: item[0]):
                    notify = library.add_animation_notify_event(animation, track_name, time, cls)
                    if not notify or notify.get_outer() != animation or notify.get_class() != cls:
                        raise RuntimeError('Engine did not create the requested owned notify')
                    created.append(notify)
                tracks_after = [str(name) for name in library.get_animation_notify_track_names(animation)]
                added = list(library.get_animation_notify_events_for_track(animation, track_name))
                if (tracks_after != tracks_before + [track_name] or len(added) != len(prepared)
                        or not unchanged_existing(created)):
                    raise RuntimeError('Engine refresh did not preserve the requested event/track contract')
            except Exception:
                # Only our last, previously absent track is removed. No pre-existing
                # event/marker is intentionally rewritten, including on failure.
                if library.is_valid_anim_notify_track_name(animation, track_name):
                    library.remove_animation_notify_track(animation, track_name)
                if not unchanged_existing([]):
                    raise RuntimeError('Unexpected engine change to existing records; undo this transaction')
                raise
        return json.dumps({'asset': animation.get_path_name(), 'track': track_name,
                           'added': len(added), 'existing_events_preserved': len(old_events), 'saved': False,
                           'notifies': [{'class': event.get_editor_property('notify').get_class().get_path_name(),
                                        'object': event.get_editor_property('notify').get_path_name(),
                                        'trigger_time_seconds': library.get_anim_notify_event_trigger_time(event),
                                        'record': event.export_text()} for event in added]}, allow_nan=False)

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
