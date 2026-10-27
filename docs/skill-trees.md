# Skill trees

Weapons and tools carry skill trees, as in New World:
- The equipped weapon decides which skills the player has.
- The tree's mastery skill earns the points.
- Learned active abilities are placed on Q/E/R inside the tree.

The system is generic. Harvesting tools use it first (HARV-09, see
[harvesting-roadmap.md](harvesting-roadmap.md)); combat weapons can use it
without code changes. Since MST-03b, axe and pickaxe share one harvesting tree of passive talents
([mastery-map-shared-harvesting.md](mastery-map-shared-harvesting.md)); its nodes place nothing on Q/E/R.

The design goal is extensibility: new trees, nodes, abilities and upgrades
are data. Code is only needed when an ability gains a new tunable value.

## Ownership

| Part | Owner | Notes |
| --- | --- | --- |
| Tree layout, nodes, costs, grants, tunings | `URpgSkillTreeDefinition` (DataAsset) | Static designer data, primary asset type `RpgSkillTreeDefinition`. |
| Which item carries which tree | `URpgInventoryFragment_SkillTree` on the item definition | Every item referencing the tree shares its progress. |
| Learned nodes, Q/E/R assignment per tree | `URpgSkillTreeComponent` on the PlayerState | Server-authoritative, replicated to the owner only, saved in the host save (player schema 4). |
| Points | Derived from the tree's `MasterySkillTag` level in `URpgTradeSkillProgressionComponent` | Never stored. |
| Abilities, effects and tags of learned nodes | `URpgEquipmentManagerComponent` | Granted while the weapon is in use; the source object is the weapon's equipment instance. |
| Q/E/R binding | `URpgWeaponAbilityLoadoutComponent` | Order: player selection, then the tree of the weapon in use, then ability set defaults. |
| Numeric upgrades | `URpgGameplayAbility::GetTunedValue` | Resolved on the server and the owning client. |
| Presentation | `URpgSkillProgressionViewModel`, `URpgSkillTreeViewModel`, `URpgSkillTreeNodeViewModel`, `URpgTradeSkillViewModel`, `URpgSkillTreeGridWidget`, `URpgSkillTreeMapWidget`, and the CommonUI screen `CUI_Skills` | Read-only; commands send the component's requests. |
| Map camera and selection | `URpgSkillTreeMapWidget`, remembered per tree in `URpgUIScreenSubsystem` | Local to the player; never replicated or saved. |

## Runtime rules

- **Points:**
  - A tree earns `Level - 1` points of its mastery skill, or the value of its `PointsByLevel` curve.
  - Earned points are capped at `MaxPoints`.
  - The available points are the earned points minus the cost of the learned nodes.
- **Learning:** a node can be learned when all of these hold (`ERpgSkillTreeUnlockResult` reports the first one that fails):
  - It is not learned yet.
  - No node of its `ExclusiveGroup` is learned.
  - Every prerequisite is learned.
  - At least `RequiredPointsInTree` points are already spent in the tree.
  - Enough points are available for its `Cost`.
- **Requests:** clients call `RequestUnlockNode`, `RequestSwapNode`, `RequestRefundNode`, `RequestResetTree` and `RequestAssignSlot`. The server validates them exactly like its own calls; a rejected request changes nothing.
  - A rejected unlock, swap or refund reaches the owner through `OnRequestRejected` with its node and an `ERpgSkillTreeRequestRejection` reason (MST-04).
- **Swap:** `SwapNode` replaces the learned node of a node's `ExclusiveGroup` with that node in one change, so grants, slots, replication and the save never see both or neither ([choice swap](mastery-map-choice-swap.md)).
  - The replaced node's points count as free, so a swap works with no free point.
  - The other purchases keep their order, and the new node joins the end.
  - It is rejected while another learned purchase would no longer be learnable without the replaced node (`ChoiceStillNeeded`), or when the new node does not fit the freed and free points.
  - Without a learned alternative a swap is an unlock. `EvaluateSwap` reports the result and the replaced node without changing anything.
- **Q/E/R slots:**
  - A newly learned node places its abilities on the first free slot of its tree.
  - Assigning an ability that already occupies another slot swaps the two slots.
  - Only abilities of learned nodes can be assigned.
- **Refund:** `RefundNode` forgets one learned node for free, refunds its cost and takes its abilities off Q/E/R. The other purchases keep their order.
  - It is rejected while another learned node lists the node as a prerequisite (`RequiredByNode`). Refund that node first, as in New World.
  - It is also rejected while a later purchase needs the node's points for a point gate (`PointsStillNeeded`).
  - `EvaluateRefund` reports the result without changing anything.
- **Reset:** `ResetTree` is free. It forgets every node, clears the slots and refunds every point.
- **In use:** a weapon is in use while it is equipped in one of the fragment's `ActiveInSlots` (main hand by default).
  - While it is in use, the equipment manager grants each learned node's `AbilitySet` (abilities, passive effects) and `GrantedTags` (loose tags, replicated).
  - Unequipping, switching weapons or resetting the tree removes them.
  - Learned nodes stay learned, so switching back restores everything.
- **Tunings:** `GetTunedValue(TuningTag, Base)` collects the learned entries of the tree of the ability's weapon that match the tag and either target the ability's id or have no id.
  - The result is `(last Set or Base + sum of Add) * product of Multiply`.
  - Abilities that must not change mid-action read their values once when they start.
- **Saves:** states are keyed by tags only.
  - On restore, a registered tree replays its purchases in order against the current skill levels.
  - Nodes that no longer exist are dropped and refunded.
  - A tree whose purchases no longer fit is reset.
  - States of trees that are not registered are kept unchanged.
- **Registration:** at BeginPlay the component registers every tree the asset manager knows, and the equipment manager registers the tree of every weapon it equips.
  - Trees under `/Game/SurvivalRpg` are scanned by `DefaultGame.ini`.
  - A Game Feature adds its own folder to the primary asset types of its GameFeatureData.
  - The skill screen registers the asset manager's trees again when it opens, so trees of Game Features registered later appear too.

## Skill screen

H opens the game menu on its Skills tab: `IA_UI_Skills` → `InputTag.UI.Skills` →
`UI.Screen.GameMenu` with a payload whose screen tag is `UI.Screen.GameMenu.Skills`.
The menu's `TabScreenTags` map such tags to its tabs.
The Skills tab is one mastery map that fills the tab (MST-03c, [single-map skill screen](mastery-map-single-map.md);
the map itself is MST-02, [skill screen on the map](mastery-map-skill-screen.md)). Every known tree is a cluster of
that map at its `MapOrigin`, with its own points, as in Path of Exile's atlas (MST-10a,
[several trees on one map](mastery-map-several-trees.md)). Character level and professions are on the Character tab.
- **Top bar:** one budget entry per tree (`CUI_SkillTreeBudgetEntry`: name, level, free and spent points, **Reset tree**), the shortcuts, input hints for the current device, and zoom out, zoom in and fit.
  - **Shortcuts:** one per tree, which frames its cluster, followed by the regions of every tree that has more than one. They show from the second shortcut on.
- **Clusters:** below each tree's lowest node the map shows `CUI_SkillTreeClusterLabel` with the tree's name, level and free points. Fit frames every tree.
- **Card:** clicking a node only selects it and opens one fixed-size card beside it, which follows the node while the map moves and stays inside the map. The map has one selection over all trees. **Learn** and **Refund** on the card send the node view model's `RequestUnlock` and `RequestRefund`, so they spend the points of the node's own tree.
  - For an excluded node that can replace its learned alternative, the card says "Switch possible" and names that alternative, and Learn reads **Switch** and sends `RequestSwap`.
  - While a request waits for the server, the card's commands are disabled and it says so. A rejection shows "Not changed" with its reason until the selection moves or the node gets a new request.
- **Bound actions:** `DT_RpgUIActions_SkillTree`. Learn is F or the left face button and Refund R or the top face button; they work while focus stays on the map, and the menu's action bar lists each while it is available. Zoom and fit add gamepad bindings: the triggers and the right stick press.
- **Q/E/R:** the screen has no Q/E/R bar. Assigning a learned active ability belongs on its card once a tree has active abilities; the assignment rules are unchanged.

| Part | Role |
| --- | --- |
| `URpgSkillProgressionViewModel` | Character level, trade skills with XP, one tree view model per known tree, the node selected in any tree with its tree, free points. `OnProgressionChanged` also fires when any tree view model changes, so a screen refreshes from one event. |
| `URpgSkillTreeViewModel` | Points, rows with their gates, prerequisite links, Q/E/R slots, the selected node, the open requests and the owner's rejections, and the commands learn, swap, reset, select and assign. |
| `URpgSkillTreeNodeViewModel` | One node: name, text, icon, kind, cell, cost, state (`Unlocked`, `Unlockable`, `Unaffordable`, `Locked`, `Excluded`), slot, `bCanRefund`, `bCanSwap` with the node it replaces, and `RequestStatus` (`Idle`, `Pending`, `Rejected`) with its `Rejection`; commands learn, swap, refund and select. |
| `URpgSkillTreeGridWidget` | Places one entry per node from `Row` and `Column`, draws the links and darkens locked rows. Entries implement User Object List Entry. No screen uses it since MST-02. |
| `URpgSkillTreeMapWidget` | Zoomable, pannable map ([below](#mastery-map)) that `CUI_Skills` shows every tree on. |
| `CUI_Skills`, `CUI_SkillTreeNode`, `CUI_SkillTreeBudgetEntry`, `CUI_SkillTreeClusterLabel`, `CUI_SkillButtonStyle` | Layout and style in `/Game/SurvivalRpg/UI/Menus/GameMenu/GameMenu/Skills`. `CUI_TradeSkillEntry` there lists the professions on the Character tab. |

### Mastery map

`URpgSkillTreeMapWidget` (MST-01, [mastery map foundation](mastery-map-foundation.md)) shows one or several trees on a clipped map:
- `SetSkillTrees` shows several trees as clusters; each node is identified by its tree and its node tag, so node tags and region ids only need to be unique per tree (MST-10a).
- An optional `TreeEntryClass` widget per tree sits centered below the tree's lowest node and receives the tree view model.
- The wheel zooms around the cursor; a drag with any mouse button pans after a few units.
- Zoom, fit and focus glide over `CameraBlendTime`, and `BackgroundBrush` art pans and zooms with the tree.
- A left click without a drag selects a node; nothing on the map learns or refunds.
- Arrow keys and the gamepad move the selection to the nearest node in that direction and bring it into view; Tab follows the authored order.
- Blueprints get zoom, fit, tree, region and node focus, pan and cancel commands, and `Overview`/`Detail` from `DetailZoom`.
- The map keeps one selection: selecting a node clears the other trees' selections.
- Camera and selection, with the selected node's tree, come back when the screen reopens; the view is remembered per set of shown trees.
- `OnSelectedNodeMoved` reports the selected node's drawn box in the map's local space whenever it moves, so a card can follow it without a tick (MST-03c).
- Entries are presentation only and never receive presses; `URpgMvvmListEntryWidget` entries get their node view model as their entry item.

The view models observe the replicated state and rebuild at most once per frame. Trade skill names come from
`DisplayName` in the trade skill config, or the last part of the skill tag.

## Current trees

| Tree | Carried by | Points | Nodes |
| --- | --- | --- | --- |
| `DA_SkillTree_Combat` (`GF_Combat_Core`) | Sword and two-handed sword | `Skill.Mastery.Combat` (shown as Combat), earned by kills through the kill XP action | Free-position map left of Harvesting with the region Discipline: the entry Battle Rhythm (1 point, weapon attacks +10 % damage through `Ability.Tuning.Attack.Damage`) forking into the exclusive choices Riposte (3 points, a perfect block deals 50 % more stagger damage and restores 50 % more stamina) and Bulwark (3 points, blocking costs 25 % less stamina and stops 15 % more damage); [report](mastery-map-combat-tree.md) |
| `DA_SkillTree_Harvesting` (`GF_Harvesting_Magic`) | Axe and pickaxe | `Skill.Mastery.Harvesting` (shown as Harvesting) | Free-position map with the region Impact: the entry Cleaving Force (1 point, every hit +25 % work) forking into the exclusive choices Shatter (3 points, weak-point bonus work x2) and Follow Through (3 points, the next hit on the same resource within 3 s after a weak-point hit does +50 % work) |

Riposte and Bulwark grant infinite GameplayEffects on `URpgDefenseSet`; the other talents only carry tunings. Weapon attacks read
`Ability.Tuning.Attack.Damage`. The harvest abilities read the tags under `Ability.Tuning.Harvest`
(area radius, reach, target count, sections, swarm creatures, rest and strike radius, cooldown, and the work values
Work, WeakPointWork, FollowUpWork and FollowUpWindow); the table is in
[harvesting-roadmap.md](harvesting-roadmap.md#harvest-tunings).

MST-03b retired the former `DA_SkillTree_Axe` and `DA_SkillTree_Pickaxe` with their active powers (Rift Grip,
Death Wave, Grave Swarm, their forms and Striding Wave), as the user decided on 2026-10-27. Saved states of those
trees stay unregistered: they grant nothing, requests for them are rejected, and their points are not refunded.

## How to extend

### Add a tree for a new weapon or tool

1. Create a `URpgSkillTreeDefinition` DataAsset in the feature that owns the weapon:
   - Set `TreeTag` (`SkillTree.Tree.<Weapon>`) and `MasterySkillTag`.
   - Set `MaxPoints`, and optionally a `PointsByLevel` curve and `BranchNames`.
   - Set `MapOrigin` so the tree's cluster does not overlap another tree on the shared map. Node positions are relative to it.
2. Add `URpgInventoryFragment_SkillTree` to the weapon's item definition and point it at the tree. Change `ActiveInSlots` only for off-hand items.
3. Make sure the asset manager scans the folder (`DefaultGame.ini` for `/Game`, the GameFeatureData for a plugin).
4. Run data validation. The automation test `SurvivalRpg.Progression.SkillTrees.Content.AllTreesValid` checks every tree.

No code, widget or save change is needed; the skill screen lists the tree and draws it.

### Add a node

Add an entry to `Nodes`:
- `NodeTag`: `SkillTree.Node.<Weapon>.<Name>`. Never rename it once shipped; saves use it.
- `Row` and `Column` for the grid, and `KindTag` for the style (`SkillTree.NodeKind.Active`, `Passive`, `Upgrade`, `Ultimate`).
  - On a tree whose `LayoutMode` is `FreePosition`, `MapPosition` places the node instead (map units: slate units at zoom 1, Y down). `GridSpacing` turns a `Grid` tree's cells into map positions.
  - `RegionId` puts the node into one of the tree's `Regions`. A region frames its nodes with its `Padding`, and `DefaultRegionId` chooses what the map frames first.
  - Put exclusive alternatives side by side in one row below their prerequisite, so the links fork. Chain upgrades that stack straight down, each requiring the one above, so no link passes behind another node.
  - A loose passive has no prerequisites.
  - Keep `RequiredPointsInTree` at 0 except for capstones such as an ultimate, so players choose freely and only chains gate nodes.
- `Cost`, `RequiredPointsInTree`, `Prerequisites` and optionally `ExclusiveGroup`.
- Any combination of grants:
  - **Active ability:** an ability set whose entry has an `AbilityIdTag` and no Q/E/R input tag. The tree places it on Q/E/R.
  - **Passive value:** a GameplayEffect in the node's ability set.
  - **Flag for other systems:** `GrantedTags`. For example, a form tag below `Harvest.Form` can turn on a harvest ability's yield conversion.
  - **Change to an existing value:** an `AbilityTunings` entry with an existing tuning tag.

### Add a tunable value

1. Add a tag under `Ability.Tuning` (for example `Ability.Tuning.Harvest.AreaRadius`) to `Config/DefaultGameplayTags.ini`.
2. Read it where the ability uses the value:
   - In C++: `GetTunedValue(Tag, Base)`, or `GetTunedValueForSpec(Spec, ActorInfo, Tag, Base)` for non-instanced code such as previews.
   - In Blueprint: `Get Tuned Value`.
3. Document the tag on the ability so designers know it exists.

## Tests

`SurvivalRpg.Progression.SkillTrees.*` covers:
- validation
- points
- unlock rules
- refunds of single nodes, reset and slots
- swaps of exclusive choices with their grants, and the owner's rejection reports
- weapon grants
- tunings
- Q/E/R through the weapon loadout
- save and restore

- the view models: projection, commands, coalesced refresh, the overview, and swap and rejection feedback
- the map layout: grid fallback, free positions, regions and their projection

`SurvivalRpg.UI.SkillTreeMap.*` covers the map's camera, spatial navigation, pointer gestures, widget lifecycle with
the remembered view, several trees with one selection and their own points, a 200-node synthetic map, and that every
shipped tree fits on the map, alone and with the others, without overlapping another tree.

`SurvivalRpg.Save.WorldSave.MemoryRoundTrip` covers the save fields.
`SurvivalRpg.Harvesting.Content.SharedHarvestingTreeContract` checks that axe and pickaxe carry the same tree, that its
talents grant no abilities and form an entry with exclusive choices, and that the tool resources award the shared
mastery XP. `AxeToolContract` and `PickaxeToolContract` check the tools' swings.
`SurvivalRpg.Harvesting.Tuning.*` checks that harvest tunings change preview and commit alike, are captured when
execution starts, shape the swarm, and shape the work of own hits (`WorkTalentsShapeOwnHits`); `SurvivalRpg.Harvesting.Swarm.StrikeRadiusTakesEachTargetOnce` checks the
strike radius. `SurvivalRpg.Harvesting.Stride.*` checks the strides of
ultimates such as Striding Wave.
