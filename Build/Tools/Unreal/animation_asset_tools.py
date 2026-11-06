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


def _vector(text, name):
    value = json.loads(text)
    if not isinstance(value, list) or len(value) != 3:
        raise RuntimeError(name + ' must be [x, y, z]')
    return unreal.Vector(*[float(v) for v in value])


def _string_weight(position, start, axis, length, radius):
    """Bowstring weight of one vertex: 1 at the string's centre, falling to 0 at its ends, 0 off the string."""
    along = (position - start).dot(axis)
    if along < 0.0 or along > length or (position - (start + axis * along)).length() > radius:
        return 0.0
    return max(0.0, 1.0 - abs(2.0 * along / length - 1.0))


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
    def create_skeleton_from_static_mesh(static_mesh_path: str, skeleton_path: str, relative_root_json: str = '[0.5, 0.5, 0.0]') -> str:
        """Create a project skeleton with one root bone for turning a static mesh into a skeletal mesh; does not save.

        relative_root_json places the root in the mesh's bounding box, 0 to 1 per axis, as [x, y, z]; the default is
        the bottom centre. Build the skeletal mesh from it with GeometryScript and add bones with SkeletonModifier.
        Refuses an existing asset or a package outside the project.
        """
        _guard()
        mesh = _asset(static_mesh_path, unreal.StaticMesh)
        _package(skeleton_path)
        root = json.loads(relative_root_json)
        if not isinstance(root, list) or len(root) != 3:
            raise RuntimeError('relative_root_json must be [x, y, z]')
        skeleton = unreal.RpgAnimationAssetTools.create_skeleton_from_static_mesh(mesh, skeleton_path, unreal.Vector(*[float(v) for v in root]))
        if not skeleton:
            raise RuntimeError('Skeleton creation failed: ' + skeleton_path)
        return skeleton.get_path_name()

    @toolset_registry.tool_call
    @staticmethod
    def rig_bow_mesh(static_mesh_path: str, skeletal_mesh_path: str, skeleton_path: str, string_bone: str,
                     string_start_json: str, string_end_json: str, draw_direction_json: str = '[0, 0, 1]',
                     string_radius: float = 1.0) -> str:
        """Create a skeletal bow mesh on a shared bow skeleton from a static bow mesh; does not save.

        The skeleton needs a root bone and string_bone as its child, as the shared bow skeleton has. Every vertex
        follows the root, except the string: vertices within string_radius cm of the segment between
        string_start_json and string_end_json (the string's two ends in mesh space, cm, as [x, y, z]) follow
        string_bone with a weight that falls from 1 at the centre to 0 at the ends, so a pull bends the string
        into a V and the limbs stay rigid. string_bone rests at the segment's centre with X along the string and
        Z along draw_direction_json (mesh space, toward the archer); the bow AnimBP pulls along that Z. Only this
        mesh's reference pose moves the bone; the shared skeleton stays unchanged. Returns JSON with the mesh
        path and the number of string vertices. Refuses an existing asset, a package outside the project, and a
        segment that catches no vertex. Save the mesh afterwards, and its materials, which their first skeletal
        use marks dirty.
        """
        _guard()
        static_mesh = _asset(static_mesh_path, unreal.StaticMesh)
        skeleton = _asset(skeleton_path, unreal.Skeleton)
        _package(skeletal_mesh_path)
        if unreal.EditorAssetLibrary.does_asset_exist(skeletal_mesh_path):
            raise RuntimeError('Creation never overwrites an existing asset: ' + skeletal_mesh_path)
        start = _vector(string_start_json, 'string_start_json')
        end = _vector(string_end_json, 'string_end_json')
        draw = _vector(draw_direction_json, 'draw_direction_json')
        length = (end - start).length()
        if length < 1.0 or string_radius <= 0.0:
            raise RuntimeError('The string needs two ends at least 1 cm apart and a positive radius')
        axis = (end - start) * (1.0 / length)
        draw = draw - axis * draw.dot(axis)
        if draw.length() < 0.01:
            raise RuntimeError('draw_direction_json must not run along the string')

        mesh = unreal.DynamicMesh()
        mesh, outcome = unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh_v2(
            static_mesh, mesh, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD(), False)
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS:
            raise RuntimeError('Could not read the static mesh: ' + static_mesh_path)
        mesh = unreal.GeometryScript_BoneWeights.copy_bones_from_skeleton(skeleton, mesh)
        bones = unreal.GeometryScript_BoneWeights.get_all_bones_info(mesh)[1]
        root = [b for b in bones if b.get_editor_property('parent_index') == -1]
        string = [b for b in bones if str(b.get_editor_property('name')) == string_bone]
        if len(root) != 1 or not string or string[0].get_editor_property('parent_index') != root[0].get_editor_property('index'):
            raise RuntimeError('The skeleton needs one root bone with ' + string_bone + ' as its child')
        root_index = root[0].get_editor_property('index')
        string_index = string[0].get_editor_property('index')

        unreal.GeometryScript_BoneWeights.mesh_create_bone_weights(mesh, True)
        unreal.GeometryScript_BoneWeights.set_all_vertex_bone_weights(
            mesh, [unreal.GeometryScriptBoneWeight(bone_index=root_index, weight=1.0)])
        string_vertices = 0
        positions = unreal.GeometryScript_MeshQueries.get_all_vertex_positions(mesh, False)[1].convert_vector_list_to_array()
        for vertex, position in enumerate(positions):
            weight = _string_weight(position, start, axis, length, string_radius)
            if weight > 0.0 and unreal.GeometryScript_MeshQueries.is_valid_vertex_id(mesh, vertex):
                string_vertices += 1
                unreal.GeometryScript_BoneWeights.set_vertex_bone_weights(mesh, vertex, [
                    unreal.GeometryScriptBoneWeight(bone_index=string_index, weight=weight),
                    unreal.GeometryScriptBoneWeight(bone_index=root_index, weight=1.0 - weight)])
        if string_vertices == 0:
            raise RuntimeError('No vertex lies within string_radius of the string segment')

        materials, slots = unreal.GeometryScript_AssetUtils.get_material_list_from_static_mesh(static_mesh)
        options = unreal.GeometryScriptCreateNewSkeletalMeshAssetOptions()
        options.set_editor_property('materials', unreal.GeometryScript_AssetUtils.convert_material_list_to_material_map(materials, slots))
        skeletal_mesh, outcome = unreal.GeometryScript_NewAssetUtils.create_new_skeletal_mesh_asset_from_mesh(
            mesh, skeleton, skeletal_mesh_path, options)
        if outcome != unreal.GeometryScriptOutcomePins.SUCCESS or not skeletal_mesh:
            raise RuntimeError('Skeletal mesh creation failed: ' + skeletal_mesh_path)

        modifier = unreal.SkeletonModifier()
        modifier.set_skeletal_mesh(skeletal_mesh)
        root_name = modifier.get_parent_name(string_bone)
        rest = unreal.Transform(location=(start + end) * 0.5,
                                rotation=unreal.MathLibrary.make_rot_from_xz(axis, draw.normal()))
        local = unreal.MathLibrary.compose_transforms(rest, unreal.MathLibrary.invert_transform(modifier.get_bone_transform(root_name, True)))
        if not local.is_near_equal(modifier.get_bone_transform(string_bone, False)):
            if not modifier.set_bone_transform(string_bone, local, False) or not modifier.commit_skeleton_to_skeletal_mesh():
                raise RuntimeError('Could not place ' + string_bone + ' on ' + skeletal_mesh_path)
        return json.dumps({'mesh': skeletal_mesh.get_path_name(), 'string_vertices': string_vertices})

    @toolset_registry.tool_call
    @staticmethod
    def register_animation_slot(skeleton_path: str, slot_name: str, group_name: str = '') -> bool:
        """Register a named slot on a project skeleton and mark it dirty; never changes bone retargeting.

        An empty group_name uses the default group. Another name creates that slot group if needed, so the
        slot's montages do not stop montages of other groups. An existing slot in a different group is refused.
        """
        _guard()
        skeleton = _asset(skeleton_path, unreal.Skeleton)
        if not unreal.RpgAnimationAssetTools.register_animation_slot(skeleton, slot_name, group_name or 'None'):
            raise RuntimeError('Animation slot registration failed')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def copy_skeleton_blend_profiles(source_skeleton_path: str, target_skeleton_path: str, profile_names_json: str) -> int:
        """Copy named blend profiles by bone name to a project skeleton and mark it dirty; never saves.

        Keeps each profile's mode and per-bone scales. Every name must exist on the source,
        be absent on the target, and use only bones the target has; otherwise nothing changes.
        """
        _guard()
        names = json.loads(profile_names_json)
        if not isinstance(names, list) or not names or not all(isinstance(n, str) and n for n in names):
            raise RuntimeError('Expected a nonempty JSON array of blend profile names')
        source = unreal.load_asset(source_skeleton_path)
        if not isinstance(source, unreal.Skeleton):
            raise RuntimeError('Source skeleton is missing: ' + source_skeleton_path)
        count = unreal.RpgAnimationAssetTools.copy_skeleton_blend_profiles(
            source, _asset(target_skeleton_path, unreal.Skeleton), names)
        if count < 0:
            raise RuntimeError('Blend profile copy was refused; see LogRpgAnimationAssetTools')
        return count

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
    def set_montage_section_sequences(asset_path: str, sections_json: str) -> int:
        """Replace a montage's first slot and sections: one sequence per section, unlinked; does not save.

        sections_json is [{"section":"F","sequence":"/Mount/Sequence"}], in playback order.
        Each section stops and blends out at its own end, for directional variants in one montage.
        Sequences must share the montage's skeleton; names must be unique.
        """
        _guard()
        sections = json.loads(sections_json)
        if not isinstance(sections, list) or not 1 <= len(sections) <= 64:
            raise RuntimeError('Expected 1..64 explicit sections')
        names, sequences = [], []
        for section in sections:
            if not isinstance(section, dict) or set(section) != {'section', 'sequence'}:
                raise RuntimeError('Each section needs exactly section and sequence')
            if not isinstance(section['section'], str) or not section['section'].strip():
                raise RuntimeError('Section names must be nonempty strings')
            names.append(unreal.Name(section['section']))
            sequences.append(_asset(section['sequence'], unreal.AnimSequence))
        if not unreal.RpgAnimationAssetTools.set_montage_section_sequences(
                _asset(asset_path, unreal.AnimMontage), names, sequences):
            raise RuntimeError('Montage rejected the sections; check project content, skeletons and unique names')
        return len(names)

    @toolset_registry.tool_call
    @staticmethod
    def retime_montage_range(asset_path: str, start_time: float, end_time: float, play_rate: float) -> float:
        """Play start_time..end_time (seconds) of a single-segment montage at play_rate (0.05 to 1); does not save.

        For telegraphs, such as a held wind-up before an attack window. Notifies after the range move later by the
        added time, notifies inside it stretch with it. Returns the new montage length.
        """
        _guard()
        values = (start_time, end_time, play_rate)
        if any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in values):
            raise RuntimeError('Times and play rate must be finite numbers')
        length = unreal.RpgAnimationAssetTools.retime_montage_range(
            _asset(asset_path, unreal.AnimMontage), float(start_time), float(end_time), float(play_rate))
        if length < 0.0:
            raise RuntimeError('Montage or range does not qualify: one unscaled segment, a range inside it, play rate 0.05 to 1')
        return length

    @toolset_registry.tool_call
    @staticmethod
    def trim_montage_range(asset_path: str, start_time: float, end_time: float) -> float:
        """Play only start_time..end_time (seconds of the current montage time) of a single-segment montage, from montage time 0; does not save.

        For one cycle cut from a looping clip, or a trimmed montage cut shorter again; on an untrimmed montage the range
        is animation time. Notifies keep their animation frame and move earlier by start_time; a notify outside the range
        refuses the trim. Returns the new montage length.
        """
        _guard()
        values = (start_time, end_time)
        if any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in values):
            raise RuntimeError('Times must be finite numbers')
        length = unreal.RpgAnimationAssetTools.trim_montage_to_range(
            _asset(asset_path, unreal.AnimMontage), float(start_time), float(end_time))
        if length < 0.0:
            raise RuntimeError('Montage or range does not qualify: one unscaled segment, at most one section, every notify inside the range')
        return length

    @toolset_registry.tool_call
    @staticmethod
    def set_montage_clip_ranges(asset_path: str, ranges_json: str) -> float:
        """Lay out a one-sequence montage as consecutive ranges of that sequence, each at its own rate; does not save.

        ranges_json is [{"start": s, "end": s, "rate": r}, ...] in animation seconds, back to back inside the sequence,
        rates 0.05 to 4: for example a restored wind-up before a trimmed start, or a fast downswing. Trimmed, retimed and
        laid-out montages qualify. Notifies keep their animation frame; a notify the ranges do not cover refuses the
        change. Returns the new montage length.
        """
        _guard()
        entries = json.loads(ranges_json)
        if not isinstance(entries, list) or not entries:
            raise RuntimeError('Expected a non-empty list of ranges')
        ranges = []
        for entry in entries:
            values = [entry.get(key) if isinstance(entry, dict) else None for key in ('start', 'end', 'rate')]
            if any(isinstance(v, bool) or not isinstance(v, (int, float)) or not math.isfinite(v) for v in values):
                raise RuntimeError('Each range needs finite start, end and rate')
            clip_range = unreal.RpgMontageClipRange()
            clip_range.anim_start_time, clip_range.anim_end_time, clip_range.play_rate = (float(v) for v in values)
            ranges.append(clip_range)
        length = unreal.RpgAnimationAssetTools.set_montage_clip_ranges(_asset(asset_path, unreal.AnimMontage), ranges)
        if length < 0.0:
            raise RuntimeError('Montage or ranges do not qualify: one sequence played forward, at most one section, '
                               'back-to-back ranges inside it at rates 0.05 to 4, every notify covered')
        return length

    @toolset_registry.tool_call
    @staticmethod
    def rotate_sequence_root(asset_path: str, yaw_degrees: float) -> bool:
        """Turn every root bone key of an in-place sequence by yaw_degrees about Z; does not save.

        Re-orients a clip that faces another direction than its owner, for example a mocap swing that lands to one
        side. Root translation keys turn with it; other bones keep their local keys. Refused with root motion enabled.
        """
        _guard()
        if isinstance(yaw_degrees, bool) or not isinstance(yaw_degrees, (int, float)) or not math.isfinite(yaw_degrees):
            raise RuntimeError('Yaw must be a finite number of degrees')
        if not unreal.RpgAnimationAssetTools.rotate_sequence_root(_asset(asset_path, unreal.AnimSequence), float(yaw_degrees)):
            raise RuntimeError('Sequence refused the rotation: project content, a root bone track, root motion off')
        return True

    @toolset_registry.tool_call
    @staticmethod
    def create_cached_pose_node(graph_path: str, cache_name: str, use: bool) -> str:
        """Add a Save Cached Pose node named cache_name (use false) or a Use Cached Pose node reading it (use true).

        graph_path is the AnimBP graph object path, for example '<AnimBP path>:AnimGraph'. Save names are unique per
        AnimBP. The editor's action index offers Use Cached Pose only after it refreshes. Returns the node path;
        connect pins and compile afterwards. Does not save.
        """
        _guard()
        graph = unreal.find_object(None, graph_path)
        if not isinstance(graph, unreal.EdGraph):
            raise RuntimeError('Graph not found: ' + graph_path)
        if use:
            node = unreal.RpgAnimationAssetTools.create_use_cached_pose_node(graph, cache_name)
        else:
            node = unreal.RpgAnimationAssetTools.create_save_cached_pose_node(graph, cache_name)
        if not node:
            raise RuntimeError('Cached pose node refused: project AnimBP graph, unique save name, existing save for a use')
        return node.get_path_name()

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

    @toolset_registry.tool_call
    @staticmethod
    def set_chooser_disabled_rows(asset_path: str, nested_table_name: str, disabled_rows_json: str) -> bool:
        """Set the editor-only disabled flags of a root Chooser or one embedded table; never saves.

        nested_table_name is the embedded table's name as shown in the Chooser editor, or empty for
        the root. Pass one boolean per result row. Disabled rows are skipped in editor evaluation and
        stripped when cooking; rows, columns and results are otherwise unchanged.
        """
        _guard()
        flags = json.loads(disabled_rows_json)
        if not isinstance(flags, list) or not all(isinstance(f, bool) for f in flags):
            raise RuntimeError('Expected a JSON array of booleans')
        if not unreal.RpgAnimationAssetTools.set_chooser_disabled_rows(
                _asset(asset_path, unreal.ChooserTable), nested_table_name or 'None', flags):
            raise RuntimeError('Disabled rows rejected; see LogRpgAnimationAssetTools')
        return True


registration = Registration([AnimationAssetTools])
registration.register()
unreal.log('RPG_ANIMATION_ASSET_TOOLS_READY')
