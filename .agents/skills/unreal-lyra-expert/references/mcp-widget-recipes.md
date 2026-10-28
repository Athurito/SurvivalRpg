# Widget, CommonUI, and MVVM recipes

Recipes and pitfalls observed in this project's UE 5.8 editor sessions. Read it before authoring Widget Blueprints, MVVM bindings, or CommonUI styles, and before driving UI in PIE. Graph editing rules are in [Blueprint graph pitfalls](mcp-blueprint-graph-pitfalls.md); the visual rules are in `docs/ui-style-guide.md`. Correct this file in the same PR when a recipe changes.

## Contents

- [Workflow](#workflow)
- [Widget tree](#widget-tree)
- [MVVM bindings](#mvvm-bindings)
- [CommonUI](#commonui)
- [Brushes, textures, and text](#brushes-textures-and-text)
- [Driving UI in PIE](#driving-ui-in-pie)

## Workflow

- Show the user screenshots of a restyled screen before rolling a style out to further screens.
- Saving dirty packages can pick up Widget Blueprints that were only read. Check `git status` for unrelated `.uasset` changes before committing.
- After `set_properties` on a Blueprint CDO (`Default__X_C`, also component templates), compile that Blueprint before PIE or saving. Instances copy only properties that differed from the native parent at the last compile, so a new array can come up empty at runtime although the CDO shows it.
- A property change on a button base CDO does not reach placed instances in Widget Blueprints that were already loaded; saving those writes the old value as an override. Set it on every placed instance too: iterate the class with `unreal.ObjectIterator` and take paths that contain `:WidgetTree.` but not `_C:WidgetTree`.
- Saving fails while PIE runs (`save_asset` returns false); stop PIE first.

## Widget tree

- `UMGToolSet.AddWidget` and `RemoveWidget` need full object paths (`<Widget Blueprint>:WidgetTree.<Name>`), not bare widget names. `AddWidget` returns the new slot object for `ObjectTools.set_properties`.
- `ObjectTools.set_properties` takes localizable text as `'NSLOCTEXT("Ns", "Key", "Text")'` strings, and `GetWidgetDescription` shows the result.
- `UMGToolSet.AddWidget` cannot add a second child to a SizeBox or Border. Wrap the existing child with `WrapWidgets`, which returns a list (rename `r[0]["widgetName"]`). The wrapper can be a project widget class; add a Widget Blueprint child class by its generated class path (`<package path>.CUI_X_C`).
- `MoveWidget` applies child indices after the removal and keeps the old slot's size rule, so a Fill button moved into a VerticalBox fills vertically. Move to `-1` (the end) step by step, read the order back with `GetWidgets`, and reset the slot size.
- In Python, setting a SizeBox's `width_override` alone does not set its override flag. Set `bOverride_WidthOverride` (and the height flag) through `ObjectTools.set_properties`.
- A Border's `Padding` is shared with its child slot padding. Set the slot first, then `border.set_editor_property("padding", ...)`.
- `AutoWrapText` wraps at the width of the last arrange. `ForceLayoutPrepass` on a widget that was collapsed measures its text unwrapped, so a card placed and clamped from that size can hang past its area. Give the texts of a fixed-width card `WrapTextAt` (the card's inner width) and place the card again on the tick after it becomes visible (`SetTimerforNextTickbyFunctionName`) (MST-10a).
- A horizontal CommonListView draws a horizontal scrollbar over its entries; collapse it with `ListViewBase|SetScrollbarVisibility` on Construct. A WidgetSwitcher sizes to its active page, so wrap it in a fixed-height SizeBox.

## MVVM bindings

- `MVVMToolset.AddViewModelToWidget` picks the class's first allowed creation type; for a Resolver view model it also inserts the project default resolver. A Resolver context needs both `CreationType=Resolver` and an instanced resolver subobject in `available_view_models`.
- `CreateViewBinding` binds property to property. It inserts `Conv_BoolToSlateVisibility` for bool to Visibility by itself (true Visible, false Collapsed) and rejects FieldNotify function sources such as `GetHealthPercent`.
- A widget-function destination, or a source on the widget's own class, goes through `AssetContractTools.add_view_function_binding`. Its guards accept only `/Game` paths, so GameFeature plugin widgets need the binding built by hand with `MVVMBlueprintViewBinding.import_text`, for example `DestinationPath=(Paths=((BindingReference=(MemberName="Fn",bSelfContext=True),BindingKind=Function)),...,Source=SelfContext)`, or a copy of an existing binding's export text.
- Functions with two parameters cannot be bound. `FLinearColor` to `FSlateColor` has no conversion, so give the view model an `FSlateColor` field. A lazy-texture setter is not bindable either; `URpgLazyImage` exposes a one-argument `SetLazyTexture` for that.
- PropertyPath sources: add the view model, then edit the context through `MVVMEditorSubsystem.get_view(wbp).available_view_models` with `export_text` and `import_text` (`CreationType=PropertyPath`, `ViewModelPropertyPath`, `bOptional`). The source name must not clash with a native property name.
- Retargeting a source to another class: `MVVMEditorSubsystem.reparent_view_model(wbp, name, cls)` changes only the class and creation type. Rewrite the bindings' `MemberParent` paths through `view.get_editor_property('bindings')`, `export_text` with the class path replaced, `import_text` into a fresh `MVVMBlueprintViewBinding`, then `set_editor_property('bindings', ...)`. Clear a leftover instanced resolver the same way on `available_view_models` (`Resolver=None`).
- `rename_view_model` opens a blocking Yes/No/Cancel dialog, and the HTTP reply of that call is lost. Answer it as [Editor sessions and PIE](editor-session-and-pie.md#dirty-packages-and-modals) describes, then read the state again.
- `RpgMvvmWidgetUtils::SetOptionalManualViewModel` logs an error when the widget has no compiled MVVM view. Guard optional hand-offs with `RpgMvvmWidgetUtils::HasViewModelSource`, or every screen test that builds the widget fails.

## CommonUI

- A `CommonButtonStyle` with `bSingleMaterial=True` ignores every state brush and draws `SingleMaterialBrush`; turn it off before restyling the state brushes.
- CommonButton radio tabs: `SetIsSelected(false)` only deselects toggleable buttons; use `CommonButton|ClearSelection`. Style a button in a graph with `CommonButton|Setters|SetStyle :InStyle "<class>"`.
- A CommonTextBlock's text style resets runtime colours when the widget is built. Re-apply selection tints in `EventConstruct` as well as in OnSelected. A CommonTextStyle CDO's `color` is a `LinearColor`; build a `SlateColor` from it, because `import_text` into a `SlateColor` gives magenta.
- `CommonActionWidget` sets its own visibility in `UpdateActionWidget`. `OnInputMethodChanged` fires after that, and `OnInputIconUpdated` fires before it becomes visible. HUD input contexts are applied after the HUD is built, so icons appear later through the Enhanced Input mappings-rebuilt path.
- An activatable widget's `InputMapping` is added on activation and removed outright on deactivation, without reference counting. Two open screens with the same context lose it when the upper one closes, so only root screens carry a shared context. A main menu with `bIsBackHandler` deactivates itself on an unhandled back press.
- A button with `bDisplayInActionBar` and a triggering action shows up in the screen's action bar; without a display name it shows a bare key.
- Action bar and bindings (MST-03c):
  - A CommonButton keeps its triggering-action binding while disabled or collapsed. The router skips it then, but the action bar still lists it.
    - For an action that comes and goes, set the row with `CommonButton|Setters|SetTriggeringInputAction` while it is available and clear it otherwise.
    - A row handle is empty only when both its table and its row are unset. A handle with a table and the row `None` logs "Cannot bind widget ... does not resolve to valid data"; clear it with an argument-free `MakeDataTableRowHandle`.
  - The action bar lists a data-table action on mouse and keyboard even when the row has no keyboard key (`InputTypesExemptFromValidKeyCheck`), as a bare label. Gamepad-only actions need `bDisplayInActionBar` off.
  - `CUI_ApplyButtonBase` overlays its glyph on the label. Narrow buttons need `bHideInputAction`.
  - `CUI_ApplyButtonBase.SetButtonText` only sets the variable its Construct copies into the label. To change the label of a built button, also call `Widget|SetText` on `Class|CUIApplyButtonBase|GetText` (MST-04).
  - A `CommonActionWidget` collapses itself when its row has no key on the current device. Its `OnInputMethodChanged` event can switch a hint row between devices, as `CUI_HudKeyHints` and `CUI_Skills` do.
- GASP pawns use their own input configs (`DA_InputConfig_GaspCMC`, `DA_InputConfig_GaspMover`, `DA_InputConfig_GaspMoverRagdoll`), so a new native hotkey needs an entry in each, not only in `DA_InputConfig`.
- The asset registry misses config references, such as the CommonInput data assets named in `Config/DefaultGame.ini`. Treat those as roots before deleting "unused" assets.

## Brushes, textures, and text

- Slate sizes the Box and Border margins of texture brushes from the texture's surface size, not from the brush `ImageSize`; material brushes do use `ImageSize`. A 2048 px frame with margin 0.16 draws corners of about 300 px whatever `ImageSize` says. Export frame textures at the size they should appear, and set `ImageSize` equal to the texture size.
- An `Image` brush's `ImageSize` is its desired size. A frame texture drawn as Box over a popup needs a small `ImageSize`, or the texture size becomes the popup's minimum size. Set a brush `image_size` with `DeprecateSlateVector2D().import_text("(X=..,Y=..)")`.
- Freshly imported textures report a 32 × 32 placeholder size while they compile; read sizes from the PNG headers.
- Python struct getters can return references into their owner: changing `get_editor_property("font")` of a text style CDO changed the shared style in memory. Copy into a fresh struct, or reload the package afterwards.
- `unreal.Text("...")` stores culture-invariant text. For localizable widget texts use `unreal.ToolsetLibrary.set_object_properties(obj, json.dumps({"Prop": 'NSLOCTEXT("Ns", "Key", "Text")'}))` with a plain namespace and escaped `\r\n` (build the backslash with `chr(92)`), then check that `str(text)` is not the literal.
- `unreal.Key().import_text('(KeyName="X")')` fails silently; use `set_editor_property("key_name", "X")`.
- Avoid font-dependent glyphs such as "▾"; Roboto renders a box.
- DataTable rows: `DataTableFunctionLibrary.export_data_table_to_json_string`, edit the JSON, then `fill_data_table_from_json_string` and save.

## Driving UI in PIE

- Without Python in PIE (MST-02): `SceneTools.find_actors` with `actor_type` `/Script/Engine.PlayerController` searches the running PIE world and returns the local controller's path. `AssetContractTools.set_pie_input_key` with that path and `H` opens the game menu on its Skills tab. While the menu has UI input, game keys no longer arrive; close it with a Slate `Click` on its Back action. Slate `PressKey Escape` ends the whole PIE session, as the editor's stop key. Slate `Click`, `Drag` and `Screenshot` on the PIE window then cover the screen.
- With several PIE players in one process, `find_actors` lists only the first world's controllers. A client's own controller is `/<Map package>/UEDPIE_<n>_<Map>.<Map>:PersistentLevel.<Controller class>_C_0`; pass that path to `set_pie_input_key`, and select its window by the title `[NetMode: Client <n>]` (MST-03b).
- `AssetContractTools.set_pie_view_rotation` accepts only the first PIE world's controller, the host, so a client's view cannot be aimed. Aim the host, or check a client's replicated state with `ObjectTools.get_properties` on its `UEDPIE_<n>` objects. A crafting station opens with F once the view is pitched down about 15 degrees from about 2 m in front of it, until `F Open` shows (MST-07a).
- A Server RPC called from editor Python (`py` console command) on a client PIE world's object never reaches the server; the call returns without effect or error. Test a client's RPC path with a network PIE test step (`.ThenClient`) instead (MST-08a).
- Open screens with `RpgUIScreenBlueprintLibrary.open_ui_screen`; the game menu tag is `UI.Screen.GameMenu`. Close menus with `deactivate_widget()`, because `set_pie_input_key` does not reach CommonUI back actions.
- Find runtime widgets with `unreal.ObjectIterator(cls)`; Python has no WidgetBlueprintLibrary lookup. HUD and menu widgets live under the GameInstance, so their paths lack `UEDPIE_`; filter out `/Game/`, `/Script/`, and `Default__` instead. Click a button with `widget.get_editor_property("Button").on_clicked.broadcast()`.
- Real mouse input goes through `SlateInspectorToolset.SlateInspectorToolset`: `Windows` with action `select`, then `Observe` and `Snapshot` on the window ref give button refs. `Click` takes `button: right`, `Snapshot` with `maxDepth: 80` also lists grid item images, and `Drag(startRef, endRef)` moves items like a mouse. Take a new snapshot after anything that changes grid refs. Slate clicks switch CommonInput back to mouse, which hides gamepad-only tabs, and `Hover` sets hover state but never opens Slate tooltips.
- Switch the input type with `CommonInputSubsystem.set_current_input_type(...)` on the PIE subsystem, found through `unreal.ObjectIterator` (its path contains `CommonLocalPlayer`). Slate inspector `Hover` and keyboard `PressKey` neither change the input type nor reach Enhanced Input.
  - `PressKey` with a gamepad key, such as `Gamepad_FaceButton_Left`, on a focused PIE widget does switch CommonInput to gamepad. It also triggers data-table UI actions bound to that key (MST-03c).
- Console commands without Python (MST-03c):
  - In the main editor window, `Click` and `Type` into the `textbox` that follows the `Cmd` combobox in the status bar, then `PressKey Enter`.
  - While PIE runs, commands with a world argument act on the PIE world, for example `Rpg.Progression.AddSkillXP Skill.Mastery.Harvesting 1500 1` for the second player.
- A Slate snapshot of one PIE window also lists the widgets of the other PIE windows of the same process. Pick refs by their position, not by the first match (MST-03c).
  - It also lists widgets a map or scroll box clipped, at positions outside the window. A `Click` on such a ref lands outside the PIE window (MST-10a).
  - Keep one PIE player's menu open at a time, so the refs belong to that window. In a small client window (about 640 units), clicks into the skill map stopped selecting after a few presses, as if a release were lost; the host window kept working (MST-10b).
- Slate `PressKey` reaches CommonUI's bound actions only while the PIE window is the active window of the desktop; inspector clicks do not activate it. In MST-10a neither F nor a gamepad face button arrived, even in a fresh PIE session, and after a click outside the window F went to the editor, which aligned its camera. Check such a command by clicking its button, and leave its keys to the playtest.
- Synthetic gamepad A: `Click` a harmless text first, which gives the PIE viewport user focus; switch CommonInput to gamepad; open the screen fresh, because a cached page keeps no focus; then `PressKey Gamepad_FaceButton_Bottom`. Enter does not activate buttons this way. Run a control with the feature off.
- `has_any_user_focus` misses focus on a CommonButton's inner SButton; the log lines `LogUIActionRouter: [User 0] Focused desired target X` are reliable focus evidence.
- `EnhancedInputLocalPlayerSubsystem.inject_input_vector_for_action(ia, Vector(1,0,0), [], [])` fires an action like a key; `inject_input_for_action` with a default value does not.
- Python has no `CreateWidget`. `unreal.new_object(cls, outer=pc)` plus `add_to_viewport` works, because `RebuildWidget` initializes the widget.
- Tooltips for screenshots: take the live `URpgInventoryItemTooltipWidget`, call `add_to_viewport` and `set_position_in_viewport(..., True)`, and `remove_from_parent` afterwards. Shift cannot be faked while Unreal is unfocused; call `SetComparisonPinned(true)` on the live tooltip instead. A screenshot's y is the viewport y plus about 36 px of title bar.
- `BlueprintReadOnly` struct fields can be filled with `import_text`, for example an `FRpgInventoryActionFeedbackMessage` from `(Result=InventoryFull)` for the inventory toast's `show_inventory_action_feedback`. The HUD toast is `URpgAbilityFailureToastWidget` with `show_ability_failure(Text)`. To show a toast without a real failure, map a message on the toast CDO temporarily, then revert it, save again, and check `git status`.
- Select a settings tab with `CommonTabListWidgetBase.select_tab_by_id("Controller")` on the instance whose path contains `Transient`.
- Crafting screen: teleport the pawn into the station's access radius, then `pc.call_method("ClientOpenCraftingInteraction", (station,))`.
- World pickups without Python (MST-05):
  - The interact key is F (`IMC_Movement`). A pickup needs the camera aimed at it, and `set_pie_input_key` sends only digital keys.
  - `ObjectTools` can neither read nor set a controller's `ControlRotation` or `Pawn`, nor an inventory's `InventoryList`.
  - Setting a PIE actor's root `RelativeLocation` through `ObjectTools` stores the value but does not move the actor.
  - Walk with timed W, A, S and D holds and read the character's `CollisionCylinder.RelativeLocation`. Leave aimed pickups to the playtest, or check in a map where the item is already in the inventory.
  - To start with a world item in a carry slot, add it for the check to an active starter inventory component's class defaults with `bAssignToEquipment` and the slot, without saving. Starter components run in no fixed order, so the item may still land in the pockets; `Drag` it into the slot. Restore the array afterwards as [Unreal MCP asset authoring](unreal-mcp-asset-authoring.md#create-edit-and-compile-a-blueprint) describes and check `git status` (MST-06a).
- PIE window size: set `NewWindowWidth` and `NewWindowHeight` on `/Script/UnrealEd.Default__LevelEditorPlaySettings` through `ObjectTools.set_properties`; Python does not expose them.
