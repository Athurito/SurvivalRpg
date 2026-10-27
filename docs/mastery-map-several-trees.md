# MST-10a: Several trees on one map

Task report for MST-10a in the [mastery map roadmap](mastery-map-roadmap.md); the design is in the [implementation plan](mastery-map-implementation-plan.md), section "One screen for every tree". It builds on [MST-03c](mastery-map-single-map.md).

## User decision (2026-10-27)

After the first look at MST-03c the user wanted every skill group on the one map, like the clusters of Path of Exile's atlas, instead of a harvesting screen. MST-10 was split:
- **MST-10a (this task):** the map for several trees, each with its own budget. It is proven with the Harvesting tree and a synthetic second tree.
- **MST-10b:** the Combat tree as the first real second cluster.

## C++ boundary decision

- **Classification:** an extension of the existing map mechanism, view models and tree schema, as the plan allows for MST-10. No new class, manager or progression component.
- **Runtime truth:** unchanged. `URpgSkillTreeComponent` owns learned nodes per `TreeTag`, and points stay derived per tree, so learning in one tree never spends another's points. Camera, selection and the remembered view are local presentation state.
- **Native schema:** `URpgSkillTreeDefinition::MapOrigin` places a tree's cluster on the shared map. Node positions, grid cells, regions and bounds are relative to it. Validation rejects a non-finite origin.
- **Native mechanism:**
  - `URpgSkillTreeMapWidget::SetSkillTrees` shows several trees. Node identity is tree plus `NodeTag` (`FNodeKey`), because node tags and region ids only need to be unique per tree.
  - The map keeps one selection: selecting a node clears the other trees' selections, and a selection made elsewhere wins as the newest.
  - New Blueprint calls: `FocusTree`, `FocusTreeRegion`, `FocusTreeNode`, `GetTreeNodeEntry`, `GetTreeEntry`, `GetSkillTrees` and `GetSelectedNode`. `FitTree` frames every tree. The single-tag calls use the first tree that has the tag.
  - **Tree entries:** `TreeEntryClass` creates one widget per tree. `SRpgSkillTreeMapPanel` keeps them in a second child list (`FCombinedChildren`) and places them centered just below the tree's lowest drawn node, outside hit testing and navigation.
  - **View cache:** `URpgUIScreenSubsystem` keys remembered views by an `FName` instead of a gameplay tag. The map uses the shown tree tags, joined with `+`, and `FRpgUIMapViewState::SelectedGroupTag` stores the selected node's tree.
  - **View models:**
    - `URpgSkillProgressionViewModel` observes every tree view model, not only the selected one.
    - Its new `SelectedNode` is the node selected in any tree, and `SelectedSkillTree` follows that node's tree.
    - `URpgSkillTreeViewModel::GetSelectedNode` exposes a tree's selection natively.
- **Assets (Unreal MCP):**
  - `CUI_SkillTreeBudgetEntry` (new, `CommonUserWidget`): one tree's name, level, free and spent points and Reset tree, filled by `F_ShowTree`.
  - `CUI_SkillTreeClusterLabel` (new): the tree entry on the map with name, level and free points.
    - It began as a copy of `CUI_SkillTreeNode`, so it keeps that widget's User Object List Entry interface.
    - It refreshes itself whenever the map hands it its tree view model again.
  - `CUI_Skills`:
    - `TreeBudgets` row in the top bar.
    - `F_ShowSkillTrees`, `F_EnsureTreeBudgetEntries`, `F_RefreshTreeBudgets` and `F_CollectShortcuts`.
    - Shortcuts across trees, kept in `ShortcutTreeTags`, `ShortcutRegionIds` and `ShortcutLabels`.
    - `F_RecheckCardNextTick`.
    - Removed: `TreeTitleBox`, `PointsText`, `ResetButton` with its event, and `F_ShowSkillTree`.

## Behavior

### Problem

The skill screen showed exactly one tree: `CUI_Skills` passed the progression view model's selected tree to the map, and the top bar showed that tree's budget. A second tree with its own points, such as Combat, would have needed tree switching again.

### Change

- **One map, several clusters:** the Skills tab shows every known tree at its `MapOrigin`. Links, regions and the background follow all of them. Fit frames every tree, and a single tree still opens on its default region.
- **Top bar:**
  - One budget entry per tree: name, level, free and spent points, and Reset tree for that tree.
  - **Shortcuts:** one button per tree, which frames its cluster, followed by the regions of every tree that has more than one. The row shows from the second shortcut on.
- **Cluster labels:** each tree shows its name, level and free points centered below its lowest node, at the entries' scale. A framed tree keeps the label in view.
- **Selection and card:**
  - One selection across all trees. The card follows the selected node, and Learn and Refund spend the node's own tree's points.
  - When points are missing, the card names that tree's mastery, for example "Mining levels earn more", instead of the fixed word Harvesting.
- **Navigation:** arrow keys and the gamepad move from one cluster to the next; Next and Previous follow the trees in order.
- **Reopen:** camera and selection come back with the selected node's tree. A different set of trees, for example once Combat exists, gets its own remembered view.
- **Card fix (from MST-03c):** reopened with a selected node near the bottom, the card hung up to 100 units past the map.
  - Its wrapped texts were measured before they knew their width.
  - Name, description and requirement now wrap at a fixed 284 units, the card's inner width.
  - The card is placed once more on the tick after it becomes visible.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings (first build and after the label change).
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.UI.SkillTreeMap SurvivalRpg.Progression.SkillTrees --null-rhi`: **25 of 25 passed** on the first native version.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.UI SurvivalRpg.Progression SurvivalRpg.Harvesting.Content SurvivalRpg.Save.WorldSave.MemoryRoundTrip --null-rhi`: **89 of 89 passed** after the label change, and again **89 of 89 passed** on the final assets. The one warning is the existing `CUI_RespawnScreen` "may require a native tick".
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.SkillTreePIE`: **1 of 1 passed**, with the usual net GUID notes.
- `python Build/Tools/Unreal/ue.py cook Lvl_HarvestPickaxe`: passed, 0 errors, 3 known warnings (Steam disabled, no GameplayCueNotifyPaths, `CUI_RespawnScreen` tick). `CUI_Skills`, `CUI_SkillTreeBudgetEntry` and `CUI_SkillTreeClusterLabel` are in the cooked output.
- **New and extended tests:**
  - `SurvivalRpg.UI.SkillTreeMap.Widget.SeveralTrees` (new), with the map tree and a synthetic second tree that reuses its node tags and region id at another origin:
    - Each tree gets its own entries and tree entry, and the second tree sits at its origin.
    - Several trees open on the whole map, and each tree entry sits just below its tree's lowest node.
    - A click selects in one tree and clears the other; a selection made outside the map is the newest.
    - Navigation crosses from one cluster to the next.
    - Learning spends only that tree's points, even for the same node tag.
    - Fit, `FocusTree` and `FocusTreeRegion` work across the trees and keep equal region ids apart.
    - The view is remembered under the set of trees with the selected node's tree. Reopening restores it, another set frames itself, and a removed tree takes its entries along.
  - `SurvivalRpg.UI.SkillTreeMap.Widget.SyntheticMapPerformance` (new): the plan's comparison of the fifteen-node size against 200 nodes, four clusters of 50, measured on tick, prepass and arrange.

    | Nodes | Open | Pan/zoom frame | Refresh of every tree |
    | --- | --- | --- | --- |
    | 15 | 0.21 ms | 0.003 ms | 0.02 ms |
    | 200 | 2.53 ms (1.95 ms in the first run) | 0.032 ms | 0.23 ms |

    Pan, zoom and refresh keep every entry and node view model in both maps. No virtualization is needed.
  - `SurvivalRpg.UI.SkillTreeMap.Content.ExistingTreesFit`: shipped trees must not overlap, and every node must be visible when all trees are framed together.
  - `SurvivalRpg.Progression.SkillTrees.Definition.MapLayout`: `MapOrigin` moves positions, regions, bounds and grid cells, and a non-finite origin is rejected.
  - `SurvivalRpg.Progression.SkillTrees.ViewModel.ProgressionOverview`:
    - Every tree notifies the overview, which reports the selected node.
    - A selection in a second tree is the newer one, and the remaining selection comes back when it clears.
    - Without a selection the picked tree stays selected.
- **Agent PIE check** in `Lvl_ThirdPerson` with its dev save, driven by the Slate inspector, `set_pie_input_key` and the editor's console box.
  - **Second tree:** an unsaved copy of `DA_SkillTree_Harvesting` named "Check tree", with tree tag `SkillTree.Tree`, points from `Skill.Gathering.Mining` and `MapOrigin` (900, 0). It was deleted afterwards and never written to disk.
  - **Standalone, 1280 x 720:**
    - Both clusters, both budget entries, the shortcuts Check tree and Harvesting, and both cluster labels appear. The top bar fits with about 30 units to spare.
    - Selecting Cleaving Force in one cluster and then in the other moves the card and clears the first selection. The second card reads "You have 0 free. Mining levels earn more."
    - The Check tree shortcut frames that cluster.
    - Learn by click: Harvesting 4 free, 1 spent; Check tree stays at 0 and 0. Refund by click restores 5 free, 0 spent.
    - **Reopen:** after selecting Follow Through in the Check tree, Back and H restore that cluster's view and its selection, with the card inside the map.
  - **Listen server with two players:**
    - The client received Harvesting XP (level 5) and learned Cleaving Force by click, reaching 3 free and 1 spent; the Check tree stayed at 0.
    - The host's screen still showed 5 free, 0 spent and an unlearned Cleaving Force.
    - The client refunded afterwards.
  - The editor's play settings were restored to the user's listen server with two clients at 1600 x 900.
- **Failed attempts and fixes:**
  - **Tree label:** placed below the tree's bounds, it was cut off when a tree was framed. It now sits just below the lowest node.
  - **Label wrap:** "Check tree" wrapped into two lines; the label's texts no longer wrap.
  - **Card after reopen:** the existing MST-03c overflow above; fixed with fixed wrap widths and the second placement.
  - **Keys in PIE:** F, R and the gamepad face buttons sent by the Slate inspector did not reach CommonUI in this session, also in a fresh PIE window.
    - After a click on an off-screen snapshot ref, F even reached the editor and aligned its camera.
    - Learn and Refund were checked by clicking their buttons instead. Their key bindings are unchanged since MST-03c, which verified them, and the action bar showed "F Learn" and "R Refund" whenever available.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.

## Playtest

Only the Harvesting tree ships, so the second cluster arrives with Combat in MST-10b.

1. **One cluster:** in `Lvl_HarvestPickaxe`, press H.
   - Below the Harvesting tree's lowest nodes, its label shows name, level and free points.
   - The top bar has one budget entry with Reset tree. No shortcut row appears for a single tree with one region.
   - Fit frames the tree.
2. **Card after reopen:** select Follow Through, press Esc, then H. The card is back beside the node and fully inside the map.
3. **Missing points:** with no free points, select Cleaving Force. The card says "Harvesting levels earn more."
4. **Multiplayer:** in a two-player listen server, the client learns a node. The host's budget entry and tree stay unchanged.

## Limits

- **One real tree:** the second cluster was a synthetic tree in automation and a temporary copy in PIE. Combat follows in MST-10b and will reset the remembered view once, because the set of trees changes.
- **Keys not re-checked:** F/R and the gamepad face buttons did not reach the PIE window in this session; mouse clicks were checked instead.
- **Top bar width:**
  - Two budget entries, two shortcuts, the hints and the zoom buttons fit at 1280 x 720 with little room.
  - More trees, long names or several regions per tree will need a narrower budget entry or a second row.
  - PIE client windows (about 640 units) still overflow, as in MST-03c.
- **Shortcut rebuild:** the shortcut buttons are rebuilt only when their count changes; trees do not change at runtime today.
- **Performance measurement:** the synthetic run uses empty native entries and measures tick, prepass and arrange, not paint or the real Widget Blueprint entries.
- **Not localizable:** the budget, label and card lines are built from strings, like the MST-03c texts they extend.

## Next

MST-10b: the Combat tree with kill-based `Skill.Mastery.Combat` and the talents Kampfrhythmus, Konter and Bollwerk on the melee weapons. MST-04 follows.
