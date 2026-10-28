# Blueprint graph pitfalls

Behavior of `BlueprintTools` (`read_graph_dsl`, `write_graph_dsl`, `create_node`, `connect_pins`) observed in this project's UE 5.8 editor sessions. Read it before writing or rewriting a Blueprint, Widget Blueprint, AnimBP, or GameplayCue notify graph. When a tool behaves differently, trust `describe_toolset` and the running editor, and correct this file in the same PR.

## Contents

- [Readable graphs](#readable-graphs)
- [Choose the edit style](#choose-the-edit-style)
- [Node type ids](#node-type-ids)
- [Arguments and pins](#arguments-and-pins)
- [Control flow](#control-flow)
- [Delegates and events](#delegates-and-events)
- [AnimBP graphs](#animbp-graphs)
- [Python access](#python-access)

## Readable graphs

Blueprints are designer-owned content: a person opens them in the editor and must follow them without asking the agent that wrote them. Every graph you create or change ends in this state:

- **Structure.** The event graph only dispatches: each event calls named functions or custom events, one per logical step, such as `Trigger`, `Extend`, `ApplyDamage`, and `Retract`. Move a step with its own inputs or outputs, or more than a handful of nodes, into a function with a descriptive name; use a macro only for exec-flow helpers. Prefer local variables and the DSL's `bind` over long wires across the graph, and split a graph that no longer fits on one screen.
- **Layout.** `write_graph_dsl` places nodes in a row, and `create_node` puts each node where you say. After writing, call `BlueprintTools.arrange_nodes` with the new and changed nodes: it lays them out left to right by exec and data flow, keeps producers left of their consumers, avoids overlaps, and anchors them to the untouched rest of the graph. Do not pass untouched nodes, because rearranging an author's existing layout destroys it and bloats the review; move single nodes with `set_node_position`. Exec wires run left to right without crossing back behind nodes; where they would, move the step into a function.
- **Comments.** Each logical block gets a short English comment that says what it does and why, following the documentation defaults in `AGENTS.md`; skip comments that only repeat a node's title. The 5.8 `BlueprintTools` cannot create comment boxes, and a node's comment bubble (`NodeComment`, `bCommentBubbleVisible`) is not an editable property. `unreal.ToolsetLibrary.set_object_properties` may still set the bubble; that is not verified yet, so read the value back and record the result here. Until comments can be authored, let function, event, and variable names carry the explanation, add variable tooltips with `AssetContractTools.set_blueprint_variable_tooltips`, and say in the report which comments are still missing.
- **Verification.** Read positions and connections back with `get_node_infos`. To review the picture, open the asset with `EditorAppToolset.OpenEditorForAsset` and capture the editor with `CaptureEditorImage`, never the desktop. Layout and comments are presentation, so do not assert them in C++ automation tests.

## Choose the edit style

- `write_graph_dsl` does not delete the nodes it replaces. Old component-bound events and disconnected nodes stay behind, still wired to the entry node's data pins; they compile against removed functions and keep hard package references.
  - In an event graph the events are replaced, but stale nodes remain.
  - In a function graph the DSL appends instead of replacing. Write each function once, or delete every node except FunctionEntry and FunctionResult first.
  - After a rewrite, delete every node the entry no longer reaches through exec pins, plus pure nodes that fed only those: `get_connected_subgraph` and `delete_node`, with paths from `find_nodes` (empty title) or names from `export_asset`. `get_connected_subgraph` returns node infos: pass each one's `node` to `delete_node`, which silently does nothing with the whole info (MST-03c). `remove_function_graph` drops a whole function. Run `unreal.SystemLibrary.collect_garbage()` before saving, or the deleted nodes still pin their dependencies.
- `read_graph_dsl` is lossy. It prints wrong categories, repeats multi-output nodes, shows a merging exec path (an `Is Not Valid` branch that joins the main flow) as empty, hides `K2Node_PropertyAccess` paths, and omits a commutative operator's third and later pins. Never round-trip such a graph through `write_graph_dsl`. For example, `Class|RpgSkillProgressionViewModel|GetSelectedNode` reads back as `SkillTree|Map|GetSelectedNode`, and writing that id binds the map widget's function instead (MST-06c).
- Prefer surgical edits in existing graphs: `create_node`, `break_pins`, `connect_pins`, and `set_pin_value`. To add behavior to a function you cannot round-trip, write it as a new function and call it right after an existing call node: `break_pins` between that node's `then` and its successor, `create_node` with `CallFunction|F<Name>`, and two `connect_pins` (MST-06c). Verify the wiring with `find_nodes` and `get_node_infos` (connected pins) instead of re-reading the DSL. Key pins by `pin_id.export_text()`, because `str(pin_id)` contains the struct address and differs per call. Node positions are `unreal.IntPoint`.
- Reading graphs (`read_graph_dsl`, `list_functions`) or recompiling a parent can mark Blueprints dirty; discard that as [Editor sessions and PIE](editor-session-and-pie.md#dirty-packages-and-modals) describes.
- Blueprint compile errors show in `Saved/Logs/SurvivalRpg.log` and `LogsToolset.GetLogEntries`, not in the compile reply. A Blueprint that fails to compile blocks the next scripted PIE start with a modal, so compile and check the status before `StartPIE`.

## Node type ids

- `find_node_types` needs `graph`, `type_id_filter`, and `context_pins: []`. It serves a stale action database: widgets added or renamed in the session do not show up, even after compile, save, or reload. `write_graph_dsl` still resolves `Variables|<WBP>|Get<Widget>` by name, so write it anyway.
- Own variables are `Variables|Default|GetX` or `Variables|<Category>|GetX`, a widget's variables `Variables|<WBP>|GetX`, and component getters `Variables|Default|Get<ComponentName>`. A native parent's component property reads `Variables|StaticMeshActor|GetStaticMeshComponent` in the actor's own graph and `Class|StaticMeshActor|GetStaticMeshComponent :self actor` from another Blueprint, such as a cue notify. Bool getters drop the `b` (`Variables|Burn|GetBurning`), also on a native view model's properties (`Class|RpgSkillTreeNodeViewModel|GetCanSwap` for `bCanSwap`). Own functions are `CallFunction|F<Name>` with underscores dropped, for example `CallFunction|FSetupGamepadGlyphs`.
- Short ids can resolve to another class: `Appearance|SetBrushfromMaterial` became a `Border` node, `Rendering|Material|SetVectorParameterValue` the material parameter collection version, `Audio|Components|Audio|FadeOut` the SynthComponent function, and `Appearance|SetBrushFromTexture` a two-pin variant. Use class-qualified ids such as `Class|Image|SetBrush`, the `...ByInfo` nodes with `MakeMaterialParameterInfo` for dynamic material instances, or `create_node` with the declaring class, for example `unreal.AudioComponent.static_class()`.
- A function added in this session is callable from its own Blueprint as `CallFunction|F<Name>` only after a compile. From another Blueprint it needs an editor restart. The id then drops underscores in the class name too: `Class|CUISkillTreeNode|FSetShowDetails` for `CUI_SkillTreeNode.F_SetShowDetails` (MST-02).
- A switch on a native enum is `Utilities|FlowControl|Switch|SwitchonE<Enum>`, for example `SwitchonERpgSkillTreeRequestStatus` (MST-04).
- `Math|Vector|vector-vector`, as `read_graph_dsl` prints it, cannot be created; write `(- a b)`.
- `write_graph_dsl` cannot parse type ids whose category contains parentheses, such as GASP's `State Machine (Experimental)`; edit those graphs node by node.
- `read_graph_dsl` may label a self call `Class|<OtherBP>|Func` when another loaded Blueprint has a function of that name. The node is still self-bound; check the package dependencies instead of the label.
- `Class|InputMappingContext|GetMappings` reads the deprecated, empty `Mappings` array. Use `Utilities|Struct|BreakInputMappingContextMappingData (Class|InputMappingContext|GetDefaultKeyMappings ctx)`.

## Arguments and pins

- Positional arguments bind to `self` first, and a literal that does not fit `self` can be dropped silently. Pass keywords, such as `(Actor|SetLifeSpan :InLifespan 3)`, `(Class|Image|SetBrush :self img :Brush b)`, or `(CallFunction|FSyncHint :Hint x :Key y)`, and read literal values back with `get_pin_value`.
- Property-setter nodes list `self` last; pass both by keyword. `Variables|Behavior|SetVisibility` is the widget's own setter; for another widget use `Widget|SetVisibility :self w :InVisibility "Collapsed"`.
- By-ref wildcard pins drop literals (the Item of `Utilities|Array|SetArrayElem`); feed an expression such as `(not false)`.
- `add_function_param` with a single value: omit `container_type`; `"NONE"` is rejected.
- `connect_pins` keys pins by direction and index. An event node's exec output is index 1, after `OutputDelegate` at 0, for both `K2Node_Event` and component-bound events.
- An inherited function with a return value, such as `BP_GetDesiredFocusTarget`, gets entry and result wired by `add_function_graph`, but `write_graph_dsl` rejects it (`AddEvent|BP_... does not exist`). Create the getter with `create_node` and connect it to the result's `ReturnValue` pin.
- Bind multiple outputs with `(bind (a b c) (Node ...))`, ordered as the out parameters, then ReturnValue. A function that returns a value needs `add_function_param` with `input_param: false` before the DSL `return` works. An impure own function used only as a return value is pruned; `bind` it first.
- Pure getters evaluate where they are used. Cache a "previous" value in a member variable before setting it.
- `write_graph_dsl` cannot create FormatText argument pins; only `Format` exists at creation.
- Struct break pins carry GUID suffixes such as `OrientationIntent_49_...`; match them by prefix.
- UObject Blueprints, such as GameplayCue notifies, need `:WorldContextObject` on `SpawnActorFromClass` and `GetPlayerCameraManager`. A static notify's world is the cue target's world.

## Control flow

- `Utilities|IsValid` is the exec macro. As a plain call or inside an `if` condition, its outputs stay unconnected and the flow stops silently. Write `(Utilities|IsValid x (:"Is Valid" ...) (:"Is Not Valid"))` at the end of a flow, with both continuations.
- `(if cond a b)` runs both statements in the True branch; the else branch is `(if cond a (else b))`.
- Setting `primary_actor_tick` from Python left an actor without ticks; use a looping `SetTimerbyFunctionName` instead.

## Delegates and events

- Bind widget delegates with `UMGToolSet.BindToEventProperty`, passing `propertyName` and `propertyClass`: `/Script/UMG.Button` for `OnClicked`, a ListView's `BP_OnItemClicked`, or a child widget's generated class for its Blueprint dispatcher. The DSL then names the event `(event OnClicked(ResetButton) ...)`.
- A CommonButton's `OnButtonBaseClicked` (`propertyClass` `/Script/CommonUI.CommonButtonBase`) gets the same `OnClicked(<Button>)` title as a UButton event, with a `Button` parameter. Before deleting the old events of a widget, tell them apart by their delegate class, not by the title (MST-03c).
- `EventDispatchers|BindEventtoOnButtonBaseClicked`, from the DSL or `create_node`, binds to `CommonButtonGroupBase`'s dispatcher of the same name. Call `retarget_node_class` from `/Script/CommonUI.CommonButtonGroupBase` to `/Script/CommonUI.CommonButtonBase`, then wire the button, a `CreateEvent` and `set_create_event_function` by hand (MST-03c).
- Engine panel calls keep their own UFUNCTION categories: `HorizontalBox.AddChildToHorizontalBox` is `Widget|AddChildtoHorizontalBox` in the DSL, but `VerticalBox.AddChildToVerticalBox` is `Panel|AddChildtoVerticalBox`. When the DSL reports that an id does not exist, look up the `Category` in the engine header (MST-07b).
- `BindEventto...` nodes leave their delegate pin open, which fails the compile. Write a `(event Custom|Handler ...)` or an `EventDispatchers|CreateEvent` node, connect its output delegate pin to the bind node's `Delegate` pin with `connect_pins` (`output_pin`, `input_pin`), then call `set_create_event_function`. Dispatcher binds read `Default|BindEventtoX :self obj :Delegate (EventDispatchers|CreateEvent)`.
- Event-shaped BlueprintNativeEvent overrides go into the EventGraph as `(event Category|EventName (Param) ...)`; adding them as a function graph is refused. `GameplayCueNotify_Burst.OnBurst` is a function override (`add_function_graph`, not `add_event`), while `GameplayCueNotify_Looping` uses event-graph events (`AddEvent|EventOnLoopingStart`, `EventOnRemoval`).
- List entries: `AssetContractTools.implement_blueprint_interface` with `/Script/UMG.UserObjectListEntry`, then `add_event OnListItemObjectSet`, written as `(event ObjectListEntry|EventOnListItemObjectSet (ListItemObject) ...)`.
- New member variables of a UObject Blueprint resolved as `Variables|Default|Get...` only after an editor restart. `get_node_type_pins` leaves a node in the graph; delete it afterwards.

## AnimBP graphs

- `create_node` for a self function whose name also exists in another AnimBP, such as `MovementAnalysis|ShouldTurninPlace`, can bind to that other class: a `self` pin appears, and the compile fails with "This blueprint (self) is not a ...". Delete the node and pass `declaring_class` as the AnimBP's generated class (`<path>.<Name>_C`). Variable getters can bind the same way; prefer the `Variables|<Category>|Get...` id and check that the new node has no input pins.
- Plain Python cannot mark a Blueprint function thread safe; use `AnimationAssetTools.set_local_function_thread_safety`, or put the logic inline in an existing thread-safe function.
- Nested chooser tables: read and write `ColumnsStructs` as JSON with `unreal.ToolsetLibrary.get_object_properties` and `set_object_properties` on `find_object(root, 'Table Name')`, then call `AnimationAssetTools.set_chooser_disabled_rows` with unchanged flags to recompile the root.
- `AssetContractTools.export_asset` writes UTF-16 T3D; decode it before searching. The export shows chooser rows and columns and property-access paths that the Python API hides.

## Python access

- In `exec_python`, `from editor_toolset.toolsets.blueprint import BlueprintTools as BT, ContainerType` gives `write_graph_dsl`, `add_variable(..., ContainerType.ARRAY)`, `add_object_variable(bp, name, Cls.static_class())`, `add_function_graph`, and `add_function_param`. Class arguments are `X.static_class()`.
- Class-reference variables: `BlueprintEditorLibrary.add_member_variable(bp, name, BlueprintEditorLibrary.get_class_reference_type(cls))`.
- `AssetContractTools.set_blueprint_variable_tooltips` accepts only `/Game` paths; for GameFeature plugin Blueprints use `AnimationAssetTools.set_variable_tooltip`.
- `DeactivateImmediate` on a Niagara component is not Blueprint-callable in 5.8; hide it with `Rendering|SetVisibility` and call `Deactivate`.
