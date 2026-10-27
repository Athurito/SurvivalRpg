# MST-03c: Single-map skill screen

Task report for MST-03c in the [mastery map roadmap](mastery-map-roadmap.md); design and acceptance contracts are in the [implementation plan](mastery-map-implementation-plan.md), section "One screen for every tree". It builds on [MST-02](mastery-map-skill-screen.md) and [MST-03b](mastery-map-shared-harvesting.md).

## C++ boundary decision

- **Classification:** designer content on the MST-01/02 map, plus the one native event the plan allows. No new class.
- **Runtime truth:** unchanged. `URpgSkillTreeComponent` owns learned nodes; Learn, Refund and Reset tree send the existing validated requests. Camera, selection and the card's position stay local presentation state.
- **Native change:** the selected node's position for the detail card.
  - The panel's existing `OnViewChanged` fires only when the target camera changes. It stays silent on the frames of a glide and when the map is resized, so a card that follows its node would need a per-frame Blueprint tick.
  - `SRpgSkillTreeMapPanel` now reports the selected node's drawn box from its tick, only when the box, the selection or the viewport size changed.
  - `URpgSkillTreeMapWidget::OnSelectedNodeMoved(bHasSelection, NodePosition, NodeSize, MapSize)` forwards it to Blueprints in the map's local space.
- **Assets (Unreal MCP):**
  - `CUI_Skills`: one page with the map below a top bar, and a canvas layer over the map that holds the detail card.
    - Removed: the professions and tree lists, the character block, the Q/E/R bar with Assign Ability, the detail column, the hint text and the unused tree description.
    - New graph functions: `F_PlaceDetailCard`, `F_RefreshDetailCard`, `F_RefreshRegionShortcuts`, `F_HandleRegionShortcutClicked`, `F_SetCardAction` and `F_SyncDeviceHints`.
    - Rewritten: `F_Refresh` and `F_RefreshNodeActions`.
    - Removed: `F_SlotLabel`, `F_AssignToNextSlot` and `F_RefreshSlots`.
  - `DT_RpgUIActions_SkillTree` (new): the screen's bound UI actions.
    - Learn: F or the gamepad's left face button.
    - Refund: R or the top face button.
    - Zoom out, zoom in and fit: left trigger, right trigger and right stick press, gamepad only.
  - `CUI_SkillButtonStyle` (new): a CommonButton style with the shared `MI_UI_Frame_Button_*` frames. The main menu style drew the buttons as white boxes on this screen.
  - `CUI_SkillTreeListEntry` was deleted: only the removed tree list used it. `CUI_TradeSkillEntry` stays for the Character tab's professions.
- **Character tab:** `CUI_Character` already shows the character values and the professions list, so it needed no change.

## Behavior

### Problem

The Skills tab split the screen into three columns: professions and trees on the left, the map with a Q/E/R bar in the middle, and a fixed detail column on the right. After MST-03b only one tree exists and no node is an active ability, so the columns took space from the map.

### Change

- **One map:** the map fills the tab below the menu's tab bar.
- **Top bar:**
  - **Budget:** the tree's name and its mastery level, the points available and spent, and Reset tree beside them.
  - **Region shortcuts:** shown once the tree has more than one region; each frames its region.
  - **Hints:** on mouse and keyboard, left button Select, right button Drag to move, and wheel Zoom. On a gamepad they become LT RT Zoom and RS Fit. The row switches with the input device.
  - **Zoom and fit:** the `-`, `+` and Fit buttons, moved out of the map's corner where they could cover a label.
- **Detail card:**
  - Selecting a node opens one fixed-width card (340 units) beside it, on the right when there is room and otherwise on the left.
  - The card stays inside the map with a 12-unit margin. It follows its node on every frame of a pan, zoom, glide or resize.
  - It shows icon, name, kind, description, state, cost and what is missing (points, the talent before it, or the excluding choice), with Learn and Refund.
  - The card does not scale with the map, so it stays readable at every zoom.
- **Keyboard and gamepad:**
  - Learn and Refund are bound actions, so F/R or the X/Y face buttons work while focus stays on the map.
  - The menu's action bar lists each one only while it is available, for example "F Learn" or "Y Refund".
  - Zoom and fit have gamepad bindings: LT, RT and the right stick press.
- **Unchanged from MST-02:** clicks only select; a drag pans and never selects, learns or refunds. H, Back, focus, semantic zoom and the restored view and selection work as before, and the card comes back with the selection.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.UI SurvivalRpg.Progression SurvivalRpg.Harvesting.Content SurvivalRpg.Save.WorldSave.MemoryRoundTrip --null-rhi`: **87 of 87 passed**. The one warning is the existing `CUI_RespawnScreen` "may require a native tick".
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.SkillTreePIE`: **1 of 1 passed**, with the usual net GUID notes.
- `python Build/Tools/Unreal/ue.py cook Lvl_HarvestPickaxe`: passed, 0 errors, 3 known warnings (Steam disabled, no GameplayCueNotifyPaths, `CUI_RespawnScreen` tick). `DT_RpgUIActions_SkillTree`, `CUI_SkillButtonStyle` and `CUI_Skills` are in the cooked output; `CUI_SkillTreeListEntry` was not, because nothing referenced it any more.
- New and extended tests:
  - `SurvivalRpg.UI.SkillTreeMap.Widget.Lifecycle`, new case: the selected node's box is reported once per change. It moves with a pan and a resize, reports no box when the selection clears, and reports the node again when it is selected again.
  - `SurvivalRpg.UI.SkillTreeMap.Content.SkillScreenActions` (new):
    - Learn and Refund have keyboard and gamepad keys, and zoom and fit gamepad keys.
    - Every row has a display name.
    - No key collides with Back, Accept, the tab keys or another skill screen action.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, driven by the Slate inspector, `set_pie_input_key` and the editor's console box:
  - **Standalone, 1600 x 900:**
    - H opens the full-tab map with the top bar. The action bar shows only Back.
    - A click on Cleaving Force opens the card beside it with "Not enough points" and both actions disabled.
    - A drag pans the map, keeps the selection, and the card follows its node.
    - Fit frames the tree again. Back and H restore view, selection and card.
  - **Learning by key:** `Rpg.Progression.AddSkillXP Skill.Mastery.Harvesting 1500` raised Harvesting to level 5.
    - The card switched to Learnable, and the action bar showed "F Learn".
    - F learned Cleaving Force with focus on the map: 3 points available and 1 spent, Reset tree enabled, and "R Refund" in the action bar.
    - In a new session, the gamepad's X learned it and Y refunded it. The hints and the action bar switched to gamepad glyphs.
  - **Region shortcuts:** with a second region added in memory and never saved, Impact and Choices appeared, and Choices framed Shatter and Follow Through. The tree was restored afterwards and its file reverted with `git checkout`.
  - **Listen server with two players in one process:**
    - The client opened Skills, received the XP and learned Cleaving Force with F.
    - The host's screen still showed level 1, 0 points and Cleaving Force not learned, with its own card.
- **Failed attempts and fixes:**
  - **Button style:** `CUI_ApplyButtonBase` with the main menu style drew white boxes. `CUI_SkillButtonStyle` fixes that.
  - **Action bar:** disabled CommonButtons keep their action binding, so the bar listed Learn and Refund without a selection. `F_SetCardAction` now sets the row only while the action is available.
    - A row handle with the table set and the row `None` logged "Cannot bind widget". The handle is now cleared entirely.
  - **Keyless actions:** the action bar lists data-table actions on mouse and keyboard even without a key. Zoom and fit therefore stay out of it.
  - **Glyph overlap:** the buttons drew their gamepad glyph over the label. The buttons hide it, and the top bar's hint row shows the zoom and fit keys.
  - **Teardown error:** closing PIE logged "Accessed None", because the view model fires once more without a tree. `F_Refresh` now refreshes the card, actions and shortcuts only with a valid tree.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.

## Playtest

1. **Map:** in `Lvl_HarvestPickaxe`, press H. The Skills tab is one map under a top bar: Harvesting with its level, points, Reset tree, the mouse hints and `-`, `+`, Fit. Professions and character level are on the Character tab.
2. **Card:** click Cleaving Force. A card opens beside it with its effect, state and cost, and Learn and Refund. Drag the map and zoom with the wheel; the card follows the node and stays on screen, at the same size.
3. **Learn by key:** with points (Harvesting level 2 or more), press F while the card shows a learnable node. The action bar lists "F Learn" only then. R refunds a learned node.
4. **Gamepad:** with a controller, move between nodes with the stick or d-pad. X learns, Y refunds, LT and RT zoom, and the right stick press fits. The top bar's hints switch to the controller.
5. **Reopen:** press Esc and then H. The view, the selected node and its card come back.
6. **Multiplayer:** in a two-player listen-server session, the client learns a node. The host's tree, points and card stay unchanged.

## Limits

- **Small windows:** in a 640-unit-wide PIE client window, the top bar runs past the right edge. 1600 x 900 fits; 1440p, ultrawide and other DPI scales were not checked.
- **Card coverage:** at the fitted overview the card can cover a neighboring node, for example Follow Through. When its node leaves the view, the card waits at the map's edge.
- **No way to clear the selection** on the map: the card stays open until another node is selected.
- **No keyboard zoom or fit keys:** the wheel and the top bar's buttons cover mouse and keyboard.
- **Not localizable:** the card's lines (state, cost and requirement) are built from strings, like the MST-02 texts they replace. The display names of rows, buttons and hints are localizable.
- **Region shortcuts** were only seen with a temporary second region; the shipped tree has one region, so they stay hidden.
- **Gamepad input** was synthesized with Slate key presses. A real controller and stick navigation remain for the playtest.
- **Graph comments:** the 5.8 Blueprint tools cannot author comment boxes, so the functions' names carry the explanation.

## Next

MST-04: atomic replacement of exclusive choices, with its pending and rejection feedback on this card.
