# Unreal MCP Asset Authoring

Use this workflow whenever work creates or changes Blueprint, Widget Blueprint, Gameplay Ability, or DataAsset assets. The toolset and tool names below match UE 5.8 editor sessions of this project; `unreal-mcp:list_toolsets` and `unreal-mcp:describe_toolset` remain the authority for the running editor.

## Contents

- [Required workflow](#required-workflow)
- [Calling toolset tools](#calling-toolset-tools)
- [Tools by task](#tools-by-task)
- [Project toolsets](#project-toolsets)
- [Other engine toolsets](#other-engine-toolsets)
- [Capability gaps](#capability-gaps)

Observed tool behavior lives in separate references: [Blueprint graph pitfalls](mcp-blueprint-graph-pitfalls.md), [Widget, CommonUI, and MVVM recipes](mcp-widget-recipes.md), [Niagara and material pitfalls](mcp-niagara-pitfalls.md), [Editor sessions and PIE](editor-session-and-pie.md), and [GAS and network test pitfalls](gas-and-network-test-pitfalls.md).

## Required workflow

1. Check that Unreal Editor is running and that the project MCP endpoint is reachable at `http://127.0.0.1:8000/mcp`. The server is configured as `unreal-mcp` in `.codex/config.toml` for Codex and in `.mcp.json` for Claude Code; coordinate a shared editor session with other worktrees or agents before authoring. Ports, parallel editors, and modals are covered in [Editor sessions and PIE](editor-session-and-pie.md).
2. Call `unreal-mcp:list_toolsets` and `unreal-mcp:describe_toolset` before deciding that an asset operation is unsupported. Availability depends on the enabled engine plugins and on the project toolsets the editor session loaded.
3. Inspect the existing parent classes, related assets, Blueprint graphs, widget trees, object properties, references, and project naming/placement conventions before authoring.
4. Author through the narrowest applicable toolset from [Tools by task](#tools-by-task). Use the [project toolsets](#project-toolsets) only for operations the engine toolsets cannot perform.
5. Compile affected Blueprints and Widget Blueprints, save the changed assets by path, and validate parentage, compile status, required properties, graphs/bindings, references, and asset contracts.

Do not replace an asset-authoring step with a runtime C++ content class merely because editing `.uasset` files is less convenient than editing source.

## Calling toolset tools

The project runs the MCP server in tool-search mode (`bEnableToolSearch=True` in `Config/DefaultEditorPerProjectUserSettings.ini`). The server therefore lists only three tools, and every toolset tool is called through them. This reference writes MCP tools as `unreal-mcp:<tool>`; clients add their own prefix, for example `mcp__unreal-mcp__call_tool` in Claude Code.

| MCP tool | Arguments | Result |
| --- | --- | --- |
| `unreal-mcp:list_toolsets` | none | One line per toolset: qualified name and description |
| `unreal-mcp:describe_toolset` | `toolset_name` | JSON schema with each tool's name, description, and input schema |
| `unreal-mcp:call_tool` | `toolset_name`, `tool_name`, `arguments` | The tool's JSON result, with the value under `returnValue` |

```json
{
  "toolset_name": "editor_toolset.toolsets.blueprint.BlueprintTools",
  "tool_name": "compile_blueprint",
  "arguments": {"blueprint": "<Blueprint object path>", "warnings_as_errors": true}
}
```

- Toolset names are qualified: `<Plugin>.<Class>` for C++ toolsets (`UMGToolSet.UMGToolSet`) and `<module path>.<Class>` for Python toolsets (`editor_toolset.toolsets.object.ObjectTools`). `tool_name` is the bare function name, such as `compile_blueprint` or `CompileWidgetBlueprint`.
- Pass an object as its object path string or as `{"refPath": "<object path>"}`; results return objects in the `refPath` form. Pass a class by class path: `/Script/<Module>.<Class>` for a native class, `<package path>.<Asset>_C` for a Blueprint class.
- `ProgrammaticToolset.execute_tool_script` batches many calls into one script after `get_execution_environment`. Its sandbox reaches tools through `execute_tool("<toolset>.<tool>", <JSON>)` and imports only `json`, `math`, `re`, `copy`, `datetime`, and `time`, so it cannot load project modules.
- Read each tool's description from `describe_toolset` for preconditions. The tag and cue tools require explicit user approval before they add, remove, or rename anything.
- To recheck this reference without an MCP client, for example after an engine upgrade, run `UnrealEditor-Cmd.exe <project>.uproject -run=ModelContextProtocolToolHashMapping -Output=<file>.json -nullrhi`; it writes every registered `<toolset>.<tool>` name. Add `"-ExecCmds=py <script path>"` to include the project toolsets. It loads the project binaries, so do not run it while another session builds.

## Tools by task

The task lists use these short toolset names:

| Short name | `toolset_name` |
| --- | --- |
| AssetTools | `editor_toolset.toolsets.asset.AssetTools` |
| ObjectTools | `editor_toolset.toolsets.object.ObjectTools` |
| BlueprintTools | `editor_toolset.toolsets.blueprint.BlueprintTools` |
| DataAssetTools | `editor_toolset.toolsets.data_asset.DataAssetTools` |
| ActorTools | `editor_toolset.toolsets.actor.ActorTools` |
| UMGToolSet | `UMGToolSet.UMGToolSet` |
| GameplayTagsToolset | `GameplayTagsToolset.GameplayTagsToolset` |
| GameplayCueToolset | `GASToolsets.GameplayCueToolset` |
| AttributeSetToolset | `GASToolsets.AttributeSetToolset` |
| AbilitySystemInspectorToolset | `GASToolsets.AbilitySystemInspectorToolset` |
| GameFeaturesToolset | `GameFeaturesToolset.GameFeaturesToolset` |
| EditorAppToolset | `EditorToolset.EditorAppToolset` |
| LogsToolset | `EditorToolset.LogsToolset` |
| AutomationTestToolset | `AutomationTestToolset.AutomationTestToolset` |
| LiveCodingToolset | `LiveCodingToolset.LiveCodingToolset` |

### Inspect

- Find and classify assets with `AssetTools.find_assets`, `exists`, `get_asset_class`, `get_dependencies`, and `get_referencers`.
- Read reflected state with `ObjectTools.get_class`, `search_subclasses`, `list_properties`, and `get_properties`. A Blueprint argument resolves to its class default object (CDO).
- `ActorTools.get_components` takes an actor Blueprint's class default object (`<asset>.Default__<Name>_C`), not the Blueprint asset. It returns native components under the CDO and a parent Blueprint's components as SCS templates (`<parent asset>.<Parent>_C:<Variable>_GEN_VARIABLE`), whose `BodyInstance` `ObjectTools` reads and writes (HRW-03).
- Read Blueprints with `BlueprintTools.get_parent`, `list_graphs`, `list_variables`, `list_functions`, `find_nodes`, `get_node_infos`, and `read_graph_dsl`.
- Confirm the asset kind on a sibling before creating one. Experiences, ItemDefinitions, EquipmentDefinitions, GameplayAbilities, and GameplayEffects are Blueprint classes; AbilitySets and PawnData are DataAsset instances.

### Create, edit, and compile a Blueprint

1. `BlueprintTools.create(folder_path, asset_name, asset_type)` creates a Blueprint whose parent is `asset_type`; `set_parent` reparents an existing one.
2. `ObjectTools.set_properties(instance, values)` with the Blueprint and a JSON object of property values sets class defaults. `values` is that object serialized to a string, with the keys `get_properties` returns; an unserialized object only returns `false`.
   - Shrinking an array of structs in one call can fail with "removed elements are ambiguous", even when the kept elements are unchanged. Set the array to `[]` first, then to its new contents (MST-06a).
   - `FColor` values such as a light's `LightColor` read and write as normalized floats 0 to 1, for example `{"r": 1.0, "g": 0.47, "b": 0.16, "a": 1.0}`. Byte values 0 to 255 clamp to white, and import text such as `(R=255,G=120,B=40)` writes black (MST-06b).
3. Add members with `add_variable`, `add_struct_variable`, `add_object_variable`, `set_variable_replication`, `add_function_graph`, `add_event`, or `add_component_bound_event`.
4. For graphs, read `get_graph_dsl_docs` once, then use `read_graph_dsl` and `write_graph_dsl` (which also compiles) for whole graphs, or `find_node_types`, `create_node`, `connect_pins`, and `set_pin_value` for single nodes. A rewrite leaves stale nodes behind and `read_graph_dsl` is lossy; read [Blueprint graph pitfalls](mcp-blueprint-graph-pitfalls.md) first, and leave the graph structured, arranged, and commented as its [Readable graphs](mcp-blueprint-graph-pitfalls.md#readable-graphs) section describes.
5. Change components with `ActorTools.add_component` and `remove_component`. To edit an inherited component template, first get a child-owned override from `AssetContractTools.editable_blueprint_component`. It matches the template's object name: `<Variable>_GEN_VARIABLE` for a component a parent Blueprint added, the subobject name (`StaticMeshComponent0`, `DisplayMesh`) for a native one.
   - A collision change belongs on the parent's template when every child should follow it. Read the whole `BodyInstance`, change `collisionProfileName`, `objectType`, `collisionEnabled` and `collisionResponses` together, and set the struct back. After compiling the parent, a child's inherited template and the placed instances in a loaded map had the new profile; the child Blueprint did not change on disk (HRW-03, `BP_HarvestNode_StaticMeshBase` and `BP_HarvestNode_IronVein`).
6. Compile with `BlueprintTools.compile_blueprint(blueprint, warnings_as_errors=true)`.

A `GameplayCueNotify_Burst` `OnBurst` override is const, so it cannot call the Blueprint's own functions, and no tool sets a function's const flag. Write its logic inline and split independent parts with `Utilities|FlowControl|Sequence` and `(:then_0 ...)`, `(:then_1 ...)`.

### AnimBP graphs

- `read_graph_dsl` returns nothing for an AnimGraph. Find nodes with `find_nodes`, read wiring with `get_node_infos`, and edit with `create_node`, `break_pins` and `connect_pins`. Pose node type ids include `Animation|Montage|Slot'DefaultSlot'`, `Animation|Blends|Layeredblendperbone`, `Animation|Poses|AdditiveIdentityPose` and `Animation|Blends|ApplyAdditive`.
- A pose node's settings live in its `Node` struct, which `ObjectTools` does not write. In Python, `node = unreal.find_object(None, '<AnimBP path>:AnimGraph.<node name>')`, then `anim_node = node.get_editor_property('node')`, change it (`slot_name`, `layer_setup` with `unreal.InputBlendPose` and `unreal.BranchFilter`), and `node.set_editor_property('node', anim_node)`.
- The engine tools create *Save cached pose* but no *Use cached pose* node, because the editor indexes that action only after a refresh. `AnimationAssetTools.create_cached_pose_node(graph_path, cache_name, use)` creates both; wire them with `connect_pins` (HRW-01 `UpperBody` path in `ABP_RpgGasp_CMC_Manny`). For an additive slot masked to part of the body, use an additive identity pose into the slot, a *Layered blend per bone* with another additive identity as its base, and *Apply Additive* onto the main pose (CMB-04 `HitFlinch`).
- `unreal.find_object(None, '<AnimBP path>:AnimGraph.<node>')` returns `None` until the AnimBP is loaded in the session; call `unreal.load_asset` first.
- A montage's slot track element edited in place lands on a copy. Read `slot_anim_tracks`, change the element, put it back into the list and set the whole list with `set_editor_property` (HRW-01), or use `ObjectTools.set_properties`.
- No tool links montage sections: `CompositeSections` and `SequenceLength` are not readable or writable through `ObjectTools` or Python, and `AnimationAssetTools.set_montage_section_sequences` leaves every section unlinked. A segment added through `SlotAnimTracks` does not extend `SequenceLength`, so playback still ends after the first one. To play one cycle of a longer clip, `AnimationAssetTools.trim_montage_range` sets the single segment's range and the length natively; it also cuts a trimmed montage again, in that montage's own time (HRW-04b). `set_montage_clip_ranges` lays a one-sequence montage out as back-to-back clip ranges at their own rates (0.05 to 4), for example a wind-up restored before a trimmed start and a sped-up downswing; notifies keep their animation frames (HRW-04e). Loops and chained sections still need a new tool on the native helper.
- A swing sped up by clip ranges also shortens its attack window in real time. The server traces a remote player's swing on a montage that advances in move-sized steps, and closes the window on whichever of notify, montage poll and clock comes first. At about 20 game frames per second, a pickaxe window of 0.12 s committed 11 of 27 swings in two-player PIE, against 16 of 16 for 0.24 s (HRW-04e). Keep a tool window near 0.2 s by opening it where the downswing starts and closing it shortly before the blend-out; move notifies with `AnimationLibrary.remove_animation_notify_events_by_name` and `add_animation_notify_event` on the same track, whose event defaults (queued, server, chance 1) meet the window contract.
- A montage that should end right after its strike, such as a tool swing that blends into the next wind-up, needs `BlendOutTriggerTime` 0: by default the auto blend-out starts its blend time before the end, which at a 1.5 play rate and 0.25 s fell before the attack window. With 0 the last pose holds while it blends out (HRW-04b).
- `ObjectTools.get_properties` cannot read a montage's `Notifies` or a static mesh's `Sockets`; use `AssetContractTools.montage_contract`, and `find_socket` in editor Python.
- `AnimationAssetTools.set_blend_space_samples` rejects the whole set when one coordinate lies outside an axis range, even by 0.2.

### GameplayAbility

- Create `GA_*` assets with `BlueprintTools.create`, parented to `/Script/SurvivalRpg.RpgGameplayAbility` or to the generated class (`_C`) of a Blueprint family base.
- Set tags, costs, cooldowns, and activation policy with `ObjectTools.set_properties`, and build behavior with the Blueprint graph tools.
- Grant the ability by referencing it from an AbilitySet (see [DataAssets](#dataassets-and-itemdefinition-fragments)).
- Tag-keyed maps such as `FailureTagToUserFacingMessages`: write each key as import text, `(TagName="Ability.ActivateFail.Cost")`. A plain tag name imports the value but trips a handled ensure, "added key ... not found in map after import" (HRW-05c).

### GameplayEffects, cues, tags, and attributes

- GameplayEffects (`GE_*`) are Blueprints parented to `/Script/GameplayAbilities.GameplayEffect`. Set modifiers, duration, and GE components with `ObjectTools.set_properties`; GE components follow the instanced-property rules below.
  - A `MultiplyAdditive` magnitude is the factor, not the change: 0.75 for -25 %, 1.5 for +50 %. GAS sums `magnitude - 1` over all such modifiers, so -0.25 as a magnitude multiplies by -0.25 and the attribute clamps to 0 (MST-10b).
  - A SetByCaller add, such as `GE_HarvestFocus_Gain` (HRW-05b): a `Modifiers` element with `attribute` (`attributeName`, `attribute` as `/Script/<Module>.<Set>:<Attribute>`, `attributeOwner`), `modifierOp` `AddBase` (the asset name of `Additive`) and `modifierMagnitude` `{"magnitudeCalculationType": "SetByCaller", "setByCallerMagnitude": {"dataName": "None", "dataTag": {"tagName": ...}}}`. A `GEComponents` element `{"refPath": "/Script/GameplayAbilities.TargetTagRequirementsGameplayEffectComponent"}` creates the component as `Default__<GE>_C:TargetTagRequirementsGameplayEffectComponent_0`; set its `ApplicationTagRequirements` there in a second call.
- A new GameFeatureData action: set `Actions` to the existing subobject paths from `get_properties` plus the new action's class path, such as `/Script/SurvivalRpg.RpgGameFeatureAction_AddWidgets`. The asset creates `<Action class>_0`; configure it on that subobject path (HRW-05b).
- Adding a `PrimaryAssetTypesToScan` entry to a GameFeatureData in a running editor scans nothing until the editor restarts. The server still finds a skill tree through the equipped weapon, but clients list trees only from the asset manager, so a client misses the new tree until then (MST-10b).
- Gameplay tags: `GameplayTagsToolset.ListTags`, `GetTagInfo`, and `FindReferencersByTag` read. `AddTag(TagName, Comment, TagSource)`, `RemoveTag`, and `RenameTag` change the INI tag sources and need explicit user approval. Check `Source/` for a native declaration before adding a tag to an INI source. `AddTag` rewrites the whole INI file, escaping every apostrophe in the comments and adding a blank line; restore the file and keep only the new `+GameplayTagList` line, which the running editor already knows.
- Gameplay cues: `GameplayCueToolset.ListCues`, `GetCueInfo`, `FindCueNotifyAssets`, and `FindCueTagsWithoutNotifies` read. `CreateCueNotifyAsset` and `AddCueTag` need explicit user approval.
  - A cost that passes only on a full resource, such as `GE_HarvestFocus_Spend` (HRW-05c): an instant `AddBase` modifier on the resource with an `AttributeBased` magnitude, coefficient -1 and the maximum captured from the `Source`. `CheckCost` builds the spec from the caster, so a `CheckCost` test proves the capture.
  - A looping cue whose effect belongs on a held tool: the placement of `GameplayCueNotify_Looping` attaches only to the target's mesh or root. Spawn the system in `EventOnLoopingStart` instead: `Actor|GetAttachedActors` of the target, `Actor|GetComponentsByClass` with `StaticMeshComponent`, `Transformation|DoesSocketExist`, then `Niagara|SpawnSystemAttached` with auto destroy. Keep the components in an array and `Deactivate` them in `EventOnRemoval` (`GCN_Harvest_Spell_WideSwing`).
  - A variant of a burst notify, such as stone chips beside wood chips, can be a Blueprint child (`BlueprintTools.create` with the parent's `_C` class) with its own child tag in `GameplayCueTag`. It inherits the parent's `OnBurst` graph and variables, and only its defaults differ. Until its tag is set it answers the parent's tag too, so set the tag in the same session; a tag added to an INI file needs an editor restart first (HRW-04b).
- Attribute sets are native schema; `AttributeSetToolset.FindAttributeSetClasses` and `ListAttributes` only read them.
- During PIE, `AbilitySystemInspectorToolset.GetGrantedAbilities`, `GetActiveTags`, `GetActiveEffects`, and `GetAttributeValues` show an actor's ability system state.

### Widget Blueprint

- `UMGToolSet.CreateWidgetBlueprint(FolderPath, AssetName, ParentClass)` creates the asset; `GetWidgets` reports an existing Widget Blueprint's `ParentClass` and widget tree.
- Build the tree with `ListWidgetClasses`, `AddWidget`, `SetNamedSlotContent`, `GetNamedSlots`, `MoveWidget`, `WrapWidgets`, `RenameWidget`, `RemoveWidget`, and the `ReplaceWidgetWith*` tools.
- Expose `BindWidget` members with `ToggleWidgetAsVariable`, and bind delegate events such as `OnClicked` with `BindToEventProperty`.
- A Widget Blueprint authored without graphs has no EventGraph, and `BlueprintTools.add_event` then fails. `AssetContractTools.ensure_event_graph` adds the standard one first.
- Set widget and slot properties (padding, alignment, anchors) by passing the widget or its `Slot` object from `GetWidgets` to `ObjectTools.set_properties`.
- `CompileWidgetBlueprint` reports missing `BindWidget` bindings and type errors.
- MVVM: the project enables the engine's `MVVMToolset.MVVMToolset` (`AllToolsets` does not include it).
  - `AddViewModelToWidget(widgetBlueprint, viewModelClass)` adds a view model with its first allowed creation type. For a view model with `MVVMAllowedContextCreationType = "Resolver"`, it also adds the project's default resolver from `Config/DefaultModelViewViewModel.ini` (`URpgLocalPlayerViewModelResolver`).
  - `CreateViewBinding(widgetBlueprint, sourceContext, sourcePropertyPath, destinationContext, destinationPropertyPath, conversionName)` binds one property to another: the view model class as source context, a widget (`<Widget Blueprint>:WidgetTree.<Name>`) and its property, such as `Text` or `Percent`, as destination.
  - `ListWidgetViewModels`, `ListWidgetViewBindings`, `RemoveWidgetViewBinding` and `ListConversionFunctions` read and clean up.
  - It cannot bind to a widget function. Use `AssetContractTools.add_view_function_binding` for a function with one input, and `AssetContractTools.add_function_input` to give that function an enum, struct or object input, which `BlueprintTools.add_function_param` cannot type. Its `soft_reference` flag makes a class input a soft object reference, such as a view model's soft texture.

### DataAssets and ItemDefinition fragments

- DataAsset instances such as `URpgAbilitySet` and `URpgPawnData`: create with `DataAssetTools.create(folder_path, asset_name, asset_type)`, then set values with `ObjectTools.set_properties`.
- Blueprint-class definitions such as `URpgExperienceDefinition`, `URpgInventoryItemDefinition`, and `URpgEquipmentDefinition`: create with `BlueprintTools.create` and the native definition class as `asset_type`, then call `ObjectTools.set_properties` on the Blueprint.
- Instanced properties such as an ItemDefinition's `Fragments`: an element `{"refPath": "/Script/SurvivalRpg.<FragmentClass>"}` creates a new fragment instance owned by the asset, and an existing subobject path keeps that instance. Read the result back with `get_properties`, then configure each fragment by calling `set_properties` on its subobject path.
- Make a variant of an item, such as a test sword, from the native `URpgInventoryItemDefinition` and copy the fragment values, never as a Blueprint child of the original item. A child's inline fragments keep the parent's fragments as archetypes, which are private objects in another package, so the save fails with "Illegal reference to private object" (CMB-07). To recover such a child, `set_parent` it to the native class, set `Fragments` to `[]`, then to new fragment instances. EquipmentDefinitions, weapon instances and actors can be Blueprint children.
- Arrays are replaced as a whole, so send the complete new array with the existing elements as `get_properties` returned them. A call that changes the array size and existing elements together is rejected; split it into separate calls.
  - The rule also applies to arrays nested inside elements, such as a rune rank's `AbilityTunings` inside `Ranks`. The call then reports "ArrayRemove: elements changed alongside the size change" and "could not be set: Ranks", and the other members of the call are still applied. Change the nested elements first, then remove the extra ones in a second call (MST-09a).
- `AssetTools.delete` on a GameplayCue notify that the cue manager has loaded returns `true` and logs "Force Deleting 1 Package(s)", but leaves the `.uasset` on disk, and `exists` stays true. After `get_referencers` shows no referencers, close the editor and remove such a file with `git rm` (MST-09d).
- Duplicating an item whose data validation checks another asset, such as a rune whose `BaseRecipe` must output it, fails `compile_blueprint` until that asset matches. Point the duplicated recipe's output at the new item before you compile the item (MST-09a).
- A new DataAsset takes a complete array of structs in one call (MST-03b: a skill tree's `Nodes` with nested tunings). Use the camelCase member keys `get_properties` prints, tags as `{"tagName": ...}`, and `FText` members as `NSLOCTEXT(...)` strings. A `TMap` such as `TaggedSkillConfigs` is replaced as a whole too: read it, add the entry, and send the full map back.
- Float curves (`UCurveFloat`), such as a level curve a DataAsset references: no engine toolset creates them or edits their keys, and `set_properties` rejects their key data. Create or replace them with `AssetContractTools.import_float_curve`, then assign the curve with `set_properties` and read the keys back with `export_asset`.

### Niagara systems

- Shared FX building blocks, Effect Types and templates live in `Content/SurvivalRpg/FX`; feature effects belong to their GameFeature. `docs/vfx-roadmap.md` holds the rules and `docs/vfx-foundation.md` the inventory.
- Author with `NiagaraToolsets.NiagaraToolset_System`:
  - Structure: `CreateNiagaraSystem`, `AddEmitter`, `AddModule`, `AddRenderer`.
  - Values: `SetStackInputData`, `SetRendererData`, `SetEmitterData`, `SetSystemData`. The data setters take `propertyValues` as a JSON string.
  - Enum inputs need the internal name; resolve it from the display name with `NiagaraToolset_Info.UEnum_Info`.
- `AddModule` appends to the end of the stack. Re-add `SolveForcesAndVelocity` after adding a force.
- After replacing an input with *Float From Curve* and setting its keys, the compiled system still evaluates the empty curve (particles read 0). Open the system with `EditorAppToolset.OpenEditorForAsset` to recompile, save, and check one frame with the console variable `fx.DumpParticleData 1`.
- Check culling with `fx.DumpNiagaraScalabilityState` and component state with `fx.Niagara.DumpComponents`.
- Attribute trimming (`bTrimAttributes`) can drop an attribute that the spawn script writes and only an HLSL expression input in a CPU emitter's update script reads. The emitter then falls back to defaults or vanishes. Turn trimming off for such systems and confirm the attributes with `fx.DumpParticleData 1`.
- Edit and rebuild systems only with PIE stopped. Edits made during Simulate applied only partly, and the save failed.

### Static meshes and simple props

Observed in HRW-01 (editor Python run as [Editor sessions and PIE](editor-session-and-pie.md#launch-an-editor) describes):

- Sockets: `sockets` is protected. Create `unreal.StaticMeshSocket(mesh)`, set `socket_name` and `relative_location`, then `mesh.add_socket(socket)`; `find_socket` and `remove_socket` read and replace.
- A simple self-built prop, such as the pickaxe: build a `unreal.DynamicMesh` with `GeometryScript_Primitives` (`append_cylinder`, `append_box`, `append_simple_swept_polygon`, whose sweep path is a list of `Vector`s), one `material_id` per part through `GeometryScriptPrimitiveOptions`. Create the asset with `GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh`, then name and fill the material slots with `GeometryScript_AssetUtils.copy_mesh_to_static_mesh` (`replace_materials`, `new_materials`, `new_material_slot_names`).
- Read a mesh's vertices with `copy_mesh_from_static_mesh` and `GeometryScript_MeshQueries.get_all_vertex_positions`, which returns a tuple; convert its `GeometryScriptVectorList` with `GeometryScript_List.convert_vector_list_to_array`.
- `MaterialInstanceConstantFactoryNew` exposes no `initial_parent` to Python; create the instance, then call `MaterialEditingLibrary.set_material_instance_parent`.
- Blueprint components without `ActorTools`: `SubobjectDataSubsystem.k2_gather_subobject_data_for_blueprint` lists each component twice; `delete_subobject(actor_handle, handle, blueprint)`, `add_new_subobject(unreal.AddNewSubobjectParams(...))` and `rename_subobject` edit them, followed by a compile.
- `Texture2D.blueprint_get_size_x` reports the resident mip (32 px) of a texture that has not streamed; read the source size from the asset registry tag `Dimensions`.
- Move a mesh pivot without changing the placed result (HRW-04c, `SM_Axe_02` and `SM_Tool_Pickaxe`). With the new pivot frame P given in old mesh space:
  - Copy each source LOD that has a mesh description (`get_static_mesh_description(lod)` is not `None`) with `copy_mesh_from_static_mesh`, `apply_build_settings` off and `lod_type` `SOURCE_MODEL`. Apply `transform_mesh` with P's inverse and write it back with `copy_mesh_to_static_mesh` and `use_original_vertex_order`. Reduction LODs rebuild from LOD0 on their own.
  - `copy_mesh_to_static_mesh` writes its `enable_recompute_normals`, `enable_recompute_tangents` and `enable_remove_degenerates` into the target LOD's build settings, so pass the LOD's current values. Leave `replace_materials` and `apply_nanite_settings` off.
  - Sockets and simple collision stay in old mesh space. Transform each socket's location and rotation with P's inverse (`MathLibrary.compose_transforms(socket, inverse)`), and each box center the same way; a quarter turn about Z swaps the box's X and Y extents.
  - Set each component that shows the mesh to `compose_transforms(P, old relative transform)`, then compile its Blueprint.
  - Verify in actor space, not with the mesh bounds, which are axis-aligned in mesh space: compare every socket and every triangle's corners, normals and UVs from `RENDER_DATA` per LOD before and after. Match triangles by centroid and winding normal, because back-to-back triangles share a centroid. The generated lightmap UVs moved by up to 0.002 there; everything else matched within 0.0001 cm and 0.0001°.

### PCG graphs

Observed in HRW-03:

- A Static Mesh Spawner's mesh entries carry their own instance descriptor, including a full `BodyInstance`. It overrides the collision that the descriptor's component class sets in its constructor, so a spawner authored with `BlockAll` kept it after the class changed.
- Edit it on the spawner's selector, `<graph>:<SpawnerNode>.PCGStaticMeshSpawnerSettings_1.DefaultSelectorInstance`: read `MeshEntries`, change `descriptor.bodyInstance`, and set the whole array back with `ObjectTools.set_properties`. Node names come from `PCGToolset.GetGraphStructure`.
- A denser copy of a stand (HRW-05c `PCG_HarvestDensePines`):
  1. `AssetTools.duplicate` the graph.
  2. Change the grid node with `PCGToolset.UpdateNode` (`jsonParams` with `cellSize` and `gridExtents`).
  3. Place it with `SpawnGraphInstance` and run `ExecuteGraphInstance`.
  4. Save the map with `AssetTools.save_assets`. `SceneTools.save_actor` refuses actors of a map without external actors.
- Save the graph, then run `PCGToolset.ExecuteGraphInstance` for every volume from `ListGraphInstances` in each map that uses it, check the generated components with `ActorTools.get_components` on the volume, and save the map. Generation finishes asynchronously; wait a few seconds before reading. Unchanged seeds produced the same instance counts and transforms, so harvest stock keys stayed valid.

### Save and validate

- Save explicitly with `AssetTools.save_assets(asset_paths)` and the changed paths. An empty list saves every dirty package, which can include another agent's work in a shared editor.
- Check unsaved state with `AssetTools.is_dirty` or `AssetContractTools.editor_status`.
- Read back parentage, properties, widget trees, and references with `BlueprintTools.get_parent`, `ObjectTools.get_properties`, `UMGToolSet.GetWidgets`, and `AssetTools.get_dependencies`.
- Prove a fresh load by calling `AssetContractTools.reload_assets` before that readback; `export_asset` writes a complete T3D export for diffs.
- Read compile and load warnings with `LogsToolset.GetLogEntries`.
- Run automation tests inside the editor with `AutomationTestToolset.DiscoverTests`, then `RunTestsByFilter`, `GetTestStatus`, and `GetTestResults`. Command-line builds, tests, and cooks go through `Build/Tools/Unreal/ue.py` instead.
- Check gameplay with `EditorAppToolset.StartPIE`, `IsPIERunning`, `CaptureViewport`, and `StopPIE`.
- Python cannot spawn actors into a PIE world. For a temporary test actor, place it in the editor level with `EditorActorSubsystem.spawn_actor_from_class`, start PIE, and remove it afterwards with `unreal.ToolsetLibrary.undo_transaction()`. This also places GameFeature actors that a core map may not reference. Never `reload_packages` the open map: the editor crashes. Undo also reverts other recent transactions, such as PIE settings changed through `ToolsetLibrary.set_object_properties`, so check them again afterwards. `AssetContractTools.set_pie_input_key` and `set_pie_view_rotation` drive a local PIE player, and `SlateInspectorToolset.SlateInspectorToolset` (`Snapshot`, `Click`, `Screenshot`) inspects UI.
- Check Game Feature activation with `GameFeaturesToolset.ListEnabledGameFeaturePlugins`, `GetGameFeatureState`, and `RequestActivateGameFeature`.

### Compile C++ in the running editor

The project enables the engine's `LiveCodingToolset` (`AllToolsets` does not include it), so C++ can change during an MCP session without closing the editor.

- `LiveCodingToolset.CompileLiveCoding` starts a Live Coding compile, waits for it, and returns `Result: Success`, `NoChanges`, or a failure status. Live Coding must be enabled in the editor preferences.
- In UE 5.8.2 a compile error returns only `CompileNotStarted` and a pointer to the Live Coding console, without the compiler messages, which are missing from the editor log too. Read them from the changed file's `.cpp.sarif` under `Intermediate/Build/Win64/x64/UnrealEditor/Development/<Module>/` after checking its timestamp, or run `ue.py build` with the editor closed.
- Use it only for function bodies in `.cpp` files. Header, reflection (`UCLASS`, `UPROPERTY`, `UFUNCTION`), constructor or default-value, `.Build.cs` and new-file changes need `ue.py build` with the editor closed.
- A patch does not construct file-static objects. In CMB-06 a patch to one `.cpp` relinked a function of another file that reads a file-static `TAutoConsoleVariable`; in PIE it read the unconstructed copy and crashed the editor with a null access. After a Live Coding patch, avoid PIE paths through files with static console variables, or build with `ue.py build` instead.
- Live Coding patches only the running editor process. Run `ue.py build` after closing the editor and before tests or a PR; a successful Live Coding compile does not prove that the project builds.

## Project toolsets

`Build/Tools/Unreal/` holds optional, editor-only Python toolsets that fill gaps in the stock UE 5.8 toolsets. They contain no content, never save assets, and refuse asset work during PIE. All but the import audit call native helpers in `Source/SurvivalRpgEditor`. The engine toolsets still own duplication, ordinary property edits, compilation, and saving.

Each toolset is named `Game.Build.Tools.Unreal.<module>.<Class>`:

- `AssetContractTools` in `Build/Tools/Unreal/asset_contract_tools.py`: asset contracts, Blueprint members the engine tools cannot author, and local PIE inspection.
  - Contracts and editor state: `editor_status`, `export_asset`, `reload_assets`, `montage_contract`, `animation_pose_contract`, `remap_owned_references`, `remap_animation_notify_classes`, and `close_clean_editor`.
  - Blueprint members: `implement_blueprint_interface`, `ensure_event_graph`, `set_blueprint_variable_tooltips`, `add_soft_class_variable`, `add_function_input`, `add_view_function_binding`, and `editable_blueprint_component`.
  - Curves: `import_float_curve`.
  - PIE: `set_pie_input_key`, `set_pie_view_rotation`, and `pie_mesh_bone_contract`.
- `AnimationAssetTools` in `Build/Tools/Unreal/animation_asset_tools.py`: animation asset structure.
  - AnimBPs and layers: `create_anim_blueprint`, `set_anim_blueprint_abstract`, `create_animation_layer_interface`, `implement_animation_layer_interface`, `add_animation_layer`, `create_linked_animation_layer_node`, and `configure_linked_animation_layer`.
  - Blueprint metadata: `set_local_function_thread_safety` and `set_variable_tooltip`.
  - Montages and notifies: `create_montage_from_sequence`, `set_montage_section_sequences` (one unlinked section per sequence, for directional variants), `retime_montage_range` (plays a time range of a single-segment montage slower, for example a held wind-up as an enemy telegraph; later notifies move with it), `trim_montage_range` (plays only one time range of a single-segment montage from time 0, for example one cycle of a looping clip; a trimmed montage takes the range in its own time), `set_montage_clip_ranges` (back-to-back clip ranges of one sequence at their own rates; notifies keep their frames), `register_animation_slot` (an optional `group_name` puts the slot in its own group, so its montages do not stop montages of the default group), `add_animation_notifies`, and `animation_notify_contract`. `add_animation_notifies` uses class defaults; set a notify's own fields, such as the `EventTag` of `AnimNotify_RpgGameplayEvent`, afterwards with `ObjectTools.set_properties` on `<montage>:<notify object name>` from its result.
  - Sequences: `rotate_sequence_root` turns an in-place clip's root keys about Z, for a clip that faces another direction than its owner.
  - AnimGraph nodes: `create_cached_pose_node` (see [AnimBP graphs](#animbp-graphs)).
  - Skeletons: `copy_skeleton_blend_profiles`.
  - BlendSpaces and Choosers: `create_blend_space`, `set_blend_space_samples`, `create_chooser`, `configure_chooser`, and `set_chooser_disabled_rows` (also for embedded tables).
- `AIAssetTools` in `Build/Tools/Unreal/ai_asset_tools.py`: `build_state_tree` writes a StateTree's evaluators, global tasks, states, conditions and transitions from a JSON description and compiles it; the engine's `StateTreeTools` only inspect. Keep the description next to the task report, so the tree can be rebuilt and reviewed as text.
- `AnimationRetargetTools` in `Build/Tools/Unreal/animation_retarget_tools.py`: derived animation batches through the IK retargeter. Tools: `retarget_animation_batch`, `retarget_asset_contract`, and `normalize_derived_sequence`.
- `GaspImportAuditTools` in `Build/Tools/Unreal/gasp_import_audit.py`: read-only registry inventory for the GASP import cleanup. Tool: `collect_registry`; the workflow is in `Build/Tools/GaspImportAudit/README.md`.

### Register

Project startup does not load these toolsets. Import them into an editor that has loaded the `SurvivalRpgEditor` module; each module registers its toolset on import:

```python
import sys
sys.path.insert(0, '<repository root>/Build/Tools/Unreal')
import asset_contract_tools
import animation_asset_tools
import ai_asset_tools
```

- In a running editor, enter these lines in the Output Log's Python command line, or run a saved script with the `py "<script path>"` console command.
- At editor launch, pass the script as `-ExecutePythonScript=<script path>` and call `unreal.EditorPythonScripting.set_keep_python_script_alive(True)` in it; otherwise the editor exits after the script.
- Import from the checkout whose project the editor opened. The toolset name follows the file location: `Game.Build.Tools.Unreal.<module>.<Class>` inside the project, `PythonTypes.<Class>_0x<hash>` outside it.
- A repeated import does nothing, and a second registration only logs `already registered`. Restart the editor to load an edited toolset file.
- `unreal-mcp:list_toolsets` shows a new registration immediately; the MCP client does not need to reconnect.

### Call

Describe the toolset, then call its tools with the qualified name:

```json
{
  "toolset_name": "Game.Build.Tools.Unreal.asset_contract_tools.AssetContractTools",
  "tool_name": "reload_assets",
  "arguments": {"asset_paths": ["<package path>"]}
}
```

## Other engine toolsets

`list_toolsets` also returns these toolsets; describe one when a task needs it:

- Editor content: `SceneTools` (levels, actors, outliner folders), `MaterialTools`, `MaterialInstanceTools`, `StaticMeshTools`, `SkeletalMeshTools` (bones, sockets), `TextureTools`, `DataTableTools`, `CurveTableTools`, `StringTableTools`, and `PrimitiveTools`, all under `editor_toolset.toolsets.<module>.<Class>`.
- Editor and project: `EditorToolset.EditorAppToolset` (selection, camera, Content Browser, asset editors, CVars, viewport capture, PIE), `SlateInspectorToolset.SlateInspectorToolset`, `SemanticSearchToolset.SemanticSearchToolset`, `ConfigSettingsToolset.ConfigSettingsToolset`, and `PluginToolset.PluginToolset`. The last two change project settings and plugins; use their write tools only for an approved task.
- Gameplay systems: `state_tree_toolset.toolsets.state_tree.StateTreeTools`, `aimodule_toolset.toolsets.behavior_tree.BehaviorTreeTools`, `DataRegistryToolset.DataRegistryTools`, `WorldConditionsToolset.WorldConditionTools`, `conversation_toolset.toolsets.conversation.ConversationTools`, and `PhysicsToolsets.PhysicsAssetToolset`.
- Effects and animation content: the `NiagaraToolsets` and `PCGToolset` toolsets, the Sequencer and Control Rig toolsets under `animation_toolset.toolsets`, and `DataflowAgent.DataflowAgentToolset`.
- Assistant support: `AIAssistant.AIAssistantToolset` and `ToolsetRegistry.AgentSkillToolset`.

## Capability gaps

If the required operation still cannot be performed after `describe_toolset` and inspection, record the exact missing capability. Only then introduce the smallest reusable editor-only tooling seam that closes that confirmed gap: a tool on the project toolset that owns the area, or a new module in `Build/Tools/Unreal/` for a new area, with any native support in `Source/SurvivalRpgEditor`. Keep it out of runtime modules and do not create one-off tooling for a single content leaf.

- Native editor helpers without a toolset wrapper are not callable through MCP, because `execute_tool_script` cannot import project modules.
- MVVM authoring goes through the engine's `MVVMToolset`; `AssetContractTools` adds function bindings and typed function inputs (see [Widget Blueprint](#widget-blueprint)).
