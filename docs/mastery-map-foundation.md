# MST-01: Mastery map foundation

Task report for MST-01 in the [mastery map roadmap](mastery-map-roadmap.md); design and acceptance contracts are in the [implementation plan](mastery-map-implementation-plan.md). The user started MST on 2026-10-27 after CMB-01 to CMB-07 had merged.

MST-01 delivers the native map mechanism and its data schema. The real skill screen still uses the legacy grid; MST-02 puts the map into `CUI_Skills`.

## Post-CMB baseline refresh

- **Base:** `master` at `29b4dae3` (PR #301 merged). Branch `claude/mst-01-map-foundation`, worktree `game-system-next-steps-568b12`, no editor running, no concurrent MST work.
- **Combat:** the [combat roadmap](combat-roadmap.md) lists CMB-01 to CMB-07 as merged. Its next step, the vertical slice level, is planned separately and does not block MST.
- **Rune foundation:** CMB-07 ([Echo rune](combat-echo-rune.md)) delivered `URpgWeaponRuneDefinition` (one rune with optional effect blocks) and `URpgInventoryFragment_WeaponRunes` (runes engraved on an item definition). Runes are not item instances yet, and there are no sockets. MST-05 extends this foundation. It must not add a parallel rune definition, and it rechecks the contract first.
- **Progression and saves:** skill trees, trade skills and the player save are unchanged since the plan's inspection. The player schema is still 4 (`SkillTreeSchemaVersion`). MST-01 changes no saved data.
- **UI routing:** H still opens `UI.Screen.GameMenu.Skills` through `URpgUIScreenSubsystem`, and `CUI_Skills` hosts `URpgSkillTreeGridWidget`.

## C++ boundary decision

- **Classification:** a reusable native map mechanism (geometry, input, hit testing, spatial focus) plus a small extension of the existing skill tree schema and read models. No content and no gameplay rule.
- **Runtime truth:** unchanged. `URpgSkillTreeComponent` owns learned nodes. The map's camera and selection are local presentation state. The view-model selection stays the selection's truth; the map shows it.
- **Existing seams extended:**
  - `URpgSkillTreeDefinition`: layout mode, grid spacing, free map positions, regions and a default region, with validation.
  - `URpgSkillTreeViewModel` and `URpgSkillTreeNodeViewModel`: map positions, region ids, link identities and positions, region views, map bounds.
  - `URpgUIScreenSubsystem`: a LocalPlayer-scoped view cache.
- **New native types:**
  - `URpgSkillTreeMapWidget` is the reflected UMG wrapper.
  - `SRpgSkillTreeMapPanel` is the internal Slate panel.
  - Plain non-reflected structs: `FRpgSkillTreeMapCamera`, `FRpgSkillTreeMapSettings` and `FRpgUIMapViewState`.
  - Two small reflected helpers: the `ERpgSkillTreeLayoutMode` and `ERpgSkillTreeMapDetail` enums, and the `FRpgSkillTreeMapRegion` and `FRpgSkillTreeRegionView` structs.
  - These match the plan's "two classes, only one reflected": the wrapper is the only reflected class.
- **Designer-owned:** node entries, styling, labels, the detail panel, and presentation by zoom level stay in Widget Blueprints (MST-02). Positions, regions and default focus are DataAsset data.

## Behavior

### Problem

The skill screen laid nodes out on a `SUniformGridPanel` from `Row` and `Column`. It could not zoom or pan, had no spatial focus, and could not lay out a shared harvesting tree with free branch positions.

### Change

- **Schema:**
  - `URpgSkillTreeDefinition::LayoutMode` is `Grid` (default) or `FreePosition`. Existing trees stay on `Grid`; `GridSpacing` (160×160 map units) turns their cells into map positions.
  - `FreePosition` trees place nodes at `FRpgSkillTreeNode::MapPosition`.
  - Nodes may name a `RegionId`. `Regions` frame their nodes with a padding, and `DefaultRegionId` chooses what the map frames first.
  - Validation rejects:
    - duplicate or non-finite map positions on a free tree (duplicate cells only on a grid tree);
    - unknown, unnamed, duplicate or empty regions, and an unknown default region;
    - negative padding and non-positive grid spacing.
- **Read models:**
  - Node view models carry `MapPosition` and `RegionId`.
  - Links name both nodes (`FromNodeTag`, `ToNodeTag`) and carry both map positions. They still come only from authored prerequisites.
  - The tree view model exposes `LayoutMode`, `MapBounds`, `Regions` and `DefaultRegionId`.
- **Camera:** `FRpgSkillTreeMapCamera` is a center plus a zoom.
  - The wheel zooms around the cursor, clamped to `MinZoom` and `MaxZoom`.
  - Resizing and DPI changes keep the centered map point.
  - Fit frames bounds inside a padding.
  - The center stays inside the map bounds.
  - Magnify gestures (trackpad, touch pinch where the platform reports them) use the same zoom.
- **Pointer:**
  - Presses tunnel through `OnPreviewMouseButtonDown`, so entries never receive them; a drag can never click an entry's control.
  - A press becomes a pan after `DragThreshold` (6 units). Left, right and middle all pan.
  - Only a left press and release on the same node without a drag selects it. Nothing on the map learns or refunds.
  - Lost capture, which Slate sends on window deactivation and when the map leaves the visible hierarchy, ends the gesture without a click. So do a move without the held button, a node refresh during a press, `CancelGesture` and `ReleaseSlateResources`. `CancelGesture` releases a capture the map still holds.
- **Hit testing:** the drawn entry's box, at least `MinHitTargetSize` (48) square, preferring the nearest center.
  - Entries scale with the zoom within `MinEntryScale` to `MaxEntryScale`, so they stay readable when zoomed out.
  - Links and region brushes use the same camera, and the panel clips to its bounds.
- **Focus:**
  - Directional navigation picks the nearest node in that direction, preferring the axis; `Next` and `Previous` follow the authored order, so every node is reachable.
  - The navigated node is selected and brought into view with `FocusMargin`.
  - At the map's edge focus escapes to the neighboring widgets.
  - Without a selection, navigation and focus start at the node nearest the view center. A focus brush marks the selection after keyboard or gamepad input.
- **Commands for Blueprints:** `ZoomIn`, `ZoomOut`, `SetZoom`, `FitTree`, `FocusRegion`, `FocusNode`, `PanBy`, `CancelGesture`.
  - `GetDetailLevel` and `OnDetailLevelChanged` report `Overview` or `Detail` around `DetailZoom`, so WBP entries can switch their presentation without rebuilding.
  - `OnNodeSelected` reports map selections.
- **Entries:**
  - One `NodeEntryClass` widget per node, reused by node tag across refreshes.
  - Pan and zoom only re-arrange and repaint; they never recreate entries or view models.
  - Entries get their node view model like `URpgViewModelEntryBox` hands items: `URpgMvvmListEntryWidget` and `URpgMvvmListEntryButton` through `SetEntryItem`, other User Object List Entries through On List Item Object Set.
- **View cache:** `URpgUIScreenSubsystem` keeps camera center, zoom and selected node per tree tag.
  - The map writes it on every view or selection change and restores it for new and pooled screen widgets. It frames the default region only when nothing is remembered.
  - `Deinitialize` clears it. It is never replicated or saved.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings. The first build failed on two test-only compile errors (`TNumericLimits<double>::Quiet_NaN`, which does not exist) and two shadowed `DesiredSize` locals; both were fixed.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.UI.SkillTreeMap SurvivalRpg.Progression.SkillTrees --null-rhi`: **21 of 21 passed**. Two earlier runs failed:
  - The first crashed in the new `Definition.MapLayout` test: `Regions.Add(Regions[0])` hit TArray's alias assertion. The test now copies the element first.
  - The second failed `Widget.Lifecycle` on "Switching back restores the first tree's view": a real bug. Switching trees pushed the new tree's bounds while the old tree was still current, so the clamped camera overwrote the old tree's remembered view. `SyncEntries` now stops saving until the new tree's view is initialized.
- New tests in that run:
  - `SurvivalRpg.UI.SkillTreeMap.Camera`: inverse transforms, cursor anchor at and beyond the zoom limits, pan, fit, bring-into-view, center clamp, resize and DPI round trip.
  - `SurvivalRpg.UI.SkillTreeMap.Navigation`: spatial picks, axis preference, edges, authored order, reachability of every node.
  - `SurvivalRpg.UI.SkillTreeMap.Panel.Input`, on the Slate panel:
    - hit targets and the minimum hit size;
    - click, jitter, drag versus click, other buttons and a second button;
    - lost capture, a release elsewhere, cancel, and a refresh during a press;
    - wheel anchor and limits, navigation and escape, clipping, DPI-scaled geometry.
  - `SurvivalRpg.UI.SkillTreeMap.Widget.Lifecycle`, on the UMG wrapper:
    - entries reused and fed their view models, stable across pan, zoom and refresh;
    - default-region framing on the first tick, the detail level, click-to-select without learning;
    - drag and cancel without a selection change;
    - close and reopen with a new view model, a newly created map, per-tree views, the legacy grid fallback, and the cache cleared on deinitialization.
  - `SurvivalRpg.UI.SkillTreeMap.Content.ExistingTreesFit`: every shipped tree loads on the grid fallback, and fitting it at 720p shows every node.
  - `SurvivalRpg.Progression.SkillTrees.Definition.MapLayout` and `...ViewModel.MapLayoutProjection`: schema, bounds, validation and read-model projection.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.UI SurvivalRpg.Harvesting.Content SurvivalRpg.Save.WorldSave.MemoryRoundTrip --null-rhi`: **63 of 63 passed**.
  - One warning is pre-existing: `SurvivalRpg.UI.Respawn.CommonUIComposition` reports that `CUI_RespawnScreen` "may require a native tick".
  - The first attempt failed `SurvivalRpg.Harvesting.Content.ResourceParityPool` on invalid package summaries. The worktree still had 64 unpulled LFS pointers; after `git lfs pull` the run passed.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- Not run: PIE and network tests (no gameplay, replication or save change), cook, and Unreal MCP asset checks (the server was not running and no asset changed).

## Playtest

None in this task: the map is not on any screen yet, and no gameplay changes. MST-02 adds the in-game check (H, wheel, drag, region focus, selection, close and reopen, multiplayer).

## Limits

- **Not on screen yet:** the map is not in `CUI_Skills`, so it has not been seen rendered. Its look, the focus brush on real entries, and wheel and drag feel in a running game are unverified until MST-02. The Unreal MCP server was not running in this session, so no Widget Blueprint was authored or inspected.
- **No gamepad camera:** there is no analog-stick pan or trigger zoom. Gamepad users navigate node to node and use the zoom commands, which MST-02 binds to CommonUI actions. Controller acceptance is still due before final acceptance, as the plan says.
- **Instant camera:** focus and region shortcuts jump; they do not animate.
- **Touch:** only magnify gestures zoom. One-finger touch pan relies on the platform's mouse emulation.
- **Performance:** not measured. The plan's 200-node synthetic comparison and the idle-menu baseline belong to the map landing in MST-02.
- **Decorative region labels:** not drawn by the native panel. Region titles belong to the screen's WBP (MST-02 or MST-03b).

## Next

MST-02: put `URpgSkillTreeMapWidget` into `CUI_Skills` in place of `GridScroll`/`TreeGrid`, make node clicks select only, add explicit learn and refund actions to the detail panel, bind zoom and region commands to CommonUI actions, wire `CancelGesture` to the screen's deactivation, and verify the in-game map with mouse, keyboard and controller at several resolutions.
