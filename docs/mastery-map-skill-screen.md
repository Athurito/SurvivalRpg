# MST-02: Skill screen on the mastery map

Task report for MST-02 in the [mastery map roadmap](mastery-map-roadmap.md); design and acceptance contracts are in the [implementation plan](mastery-map-implementation-plan.md). It builds on [MST-01](mastery-map-foundation.md).

## C++ boundary decision

- **Classification:** designer content on the MST-01 mechanism, plus changes to that mechanism found while integrating it and asked for by the user. No new native class.
- **Runtime truth:** unchanged. `URpgSkillTreeComponent` owns learned nodes. Learn and Refund send the existing validated requests through the node view model. The map's camera, selection and detail level stay local presentation state.
- **Assets (Unreal MCP):**
  - `CUI_Skills` hosts `URpgSkillTreeMapWidget` (`TreeMap`) in place of `GridScroll`/`TreeGrid`.
  - It gains Learn and Refund buttons in the detail panel and zoom out, zoom in and fit buttons over the map.
  - Its graphs gain these helpers: `F_ShowSkillTree`, `F_ApplyMapDetail`, `F_ShowNodeDetails`, `F_LearnSelected`, `F_RefundSelected` and `F_RefreshNodeActions`.
  - It overrides `BP_GetDesiredFocusTarget` and cancels map gestures in `On Deactivated`.
  - `CUI_SkillTreeNode` loses learn-on-click and the right-click refund, its button is no longer focusable, and `F_SetShowDetails` shows or hides name and status.
- **Native changes:**
  - **Clipping:** `URpgSkillTreeMapWidget` defaults its `Clipping` to Clip to Bounds. UMG pushes `UWidget::Clipping` onto the Slate panel, which overrode the panel's own clipping with Inherit. Nodes and links then drew over the neighboring panels.
  - **Fit:** a requested fit now follows the viewport size until anything else moves the camera. In the menu, the map's first tick still saw the whole window, so the tree was framed about 50 % too large.
  - **Focus:** focus set directly, for example when the screen activates, no longer selects a node. Only navigating into the map does.
  - **Clicks:** a click or navigation no longer makes the wrapper bring the node into view. The first PIE check showed a click on a top node panning the map.
  - **Glide and background:** the user asked for a Path of Exile-like map one dives into, so the view now glides and the background art moves with it.
    - `CameraBlendTime` (0.12 s) glides zoom, fit, focus and region framing with an exponential approach. The wheel keeps the point under the cursor fixed on every frame, and a drag still follows the pointer and stops a running glide.
    - `BackgroundBrush` and `BackgroundScale` draw map art centered on the tree that pans and zooms with it. `CUI_Skills` moved its ritual-tree sigil there from a static overlay image.

## Behavior

### Problem

H opened the skill screen on the old grid: no zoom or pan, learning on click, refunding on right-click.

### Change

- **Map:** the tree sits on the map.
  - The wheel zooms around the cursor and a drag with any button pans.
  - `-`, `+` and `Fit` zoom and frame without a wheel, and arrow keys or the gamepad move the selection from node to node.
  - The map clips to its panel. Opening a tree frames it, or its default region, in the space the screen gives it.
- **Selecting and learning:** a click only selects a node, and the detail panel shows it.
  - **Learn** and **Refund** in the detail panel send the server requests. They are enabled from the node's `bCanUnlock` and `bCanRefund`.
  - **Assign Ability**, Q/E/R and **Reset tree** are unchanged.
- **Hints:** they now explain selecting, dragging and zooming.
- **Semantic zoom:** below `DetailZoom` (0.75) the nodes hide their name and cost; above it they show them again.
- **Reopening:** camera and selection come back per tree when the menu is closed and reopened. Switching between the axe and pickaxe trees keeps a view for each. Tree switching through the list stays until MST-03b.
- **Focus:** the screen gives keyboard and gamepad focus to the map. Focus leaves it at the map's edges for the neighboring panels.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings, after each native change. The editor was closed for every build.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.UI SurvivalRpg.Progression.SkillTrees SurvivalRpg.Harvesting.Content SurvivalRpg.Save.WorldSave.MemoryRoundTrip --null-rhi`: **80 of 80 passed** on the final assets. The one warning is the existing `CUI_RespawnScreen` "may require a native tick".
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.SkillTreePIE`: **1 of 1 passed**. Its warnings are the usual net GUID notes of the temporary PIE map.
- New and extended tests:
  - `SurvivalRpg.UI.SkillTreeMap.Content.SkillScreenHostsMap`: `CUI_Skills` loads as an activatable widget and hosts one map whose node entries are User Object List Entries. Names and layout stay unfrozen.
  - `...Panel.Input`, new cases:
    - direct focus selects nothing, focus by navigation selects the nearest node;
    - a fit follows the layout until the camera moves;
    - the wheel glides while the point under the cursor stays put, settles on the target, and a drag stops it.
  - `...Widget.Lifecycle`, new cases: the UMG map keeps clipping, and a click on an edge node leaves the camera alone.
- Agent PIE check on `Lvl_HarvestPickaxe` without Python, through `set_pie_input_key` and the Slate inspector:
  - **Standalone, 1600 x 900:**
    - H opens the Skills tab. The axe tree is framed and clipped, with the sigil behind it.
    - A click selects Death Wave without moving the map; Learn stays disabled at 0 points.
    - A drag moves the map by the drag distance and selects nothing, even when it ends on another node.
    - `-` reaches the overview level, which hides names and costs; `+` zooms in.
    - Back closes the menu, and H reopens it with the same view and selection. Axe and pickaxe keep separate views.
  - **Listen server with two clients in one process:**
    - On Client 1, H opens the menu with the map framed and clipped. The small client window starts at the overview level.
    - `+` reaches the detail level, and a click selects Quick Recovery.
    - The host's menu has its own view, framed and with nothing selected.
- Failed attempts and corrections:
  - **First PIE view:** nodes drew over the side panels, and the tree was framed for the whole window. Both were fixed natively; tests now cover them.
  - **Click panning:** a click on a top node panned the map. Fixed.
  - **Exec toolset refused:** launching the editor with a scratch `exec_python` toolset was refused as a remote-code-execution surface, so the check used only existing tools.
  - **Escape:** Slate `Escape` ends the PIE session, so the menu was closed with its Back action instead.
  - **Graph tooling:** functions created in this session were callable from another Blueprint only after an editor restart.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent after the reference updates.

## Playtest

1. Open any gameplay map, for example `Lvl_HarvestPickaxe`, and press H. The Skills tab shows the axe tree on a map with the ritual tree behind it.
2. **Zoom and pan:** the wheel zooms smoothly toward the cursor, and the background zooms along. Drag with the left, right or middle button: the map follows. Releasing over a node does not select it.
3. **Select:** click a node. It gets the gold ring, the detail panel shows it, and the map does not move.
4. **Learn:** with points (Logging 2 or more), **Learn** learns the node, and **Refund** returns its points. Assign Ability and Q/E/R work as before.
5. **Zoom levels:** zoom out until names and costs disappear, then zoom in again. Use `-`, `+` and `Fit`.
6. **Reopen:** close with Esc or Back and press H again; view and selection are back. Switch to Pickaxe and back to Axe; each keeps its own view.
7. **Keyboard and gamepad:** arrow keys or the gamepad move the selection from node to node and bring it into view. At the map's edges focus moves on to the panels.
8. **Multiplayer:** in a listen-server session with two players, repeat steps 1 to 6 on the client. The host's view stays independent, and a node learned on the client shows as learned only for that client.

## Limits

- **Not seen in PIE:** wheel zoom and glides are covered by automated tests only, because the Slate inspector cannot scroll. Keyboard and gamepad navigation were not exercised either. Controller acceptance stays due before the final MST acceptance.
- **Learning:** no node was learned in PIE, because the test map starts at level 1 with 0 points. The server path is covered by `SurvivalRpg.Network.SkillTreePIE`.
- **Resolutions:** checked at 1600 x 900 and in a small client window, not at 1440p, ultrawide or other DPI scales.
- **Performance:** not measured. The plan's real versus 200-node comparison follows when the shared tree exists (MST-03b).
- **Graph comments:** the 5.8 Blueprint tools cannot author comment boxes. The new functions carry descriptive names instead.
- **Small issues:**
  - An unaffordable node's hint still says "Learn the nodes leading to it first.", an existing `F_Refresh` text.
  - The zoom buttons can cover a label at the map's lower right until the map is panned.

## Next

MST-03a: quantized harvest work, residual progress and overkill in the existing stock and commit mechanism. It is independent of the UI and comes before the shared harvesting tree in MST-03b.
