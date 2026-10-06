# Skill trees

Weapons and tools carry skill trees, as in New World:
- The equipped weapon decides which skills the player has.
- The tree's mastery skill earns the points.
- Learned active abilities are placed on Q/E/R inside the tree.

The system is generic. Harvesting tools use it first (HARV-09, see
[harvesting-roadmap.md](harvesting-roadmap.md)); combat weapons can use it
without code changes.

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
| Presentation | `URpgSkillProgressionViewModel`, `URpgSkillTreeViewModel`, `URpgSkillTreeNodeViewModel`, `URpgTradeSkillViewModel`, `URpgSkillTreeGridWidget`, and the CommonUI screen `CUI_Skills` | Read-only; commands send the component's requests. |

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
- **Requests:** clients call `RequestUnlockNode`, `RequestResetTree` and `RequestAssignSlot`. The server validates them exactly like its own calls; a rejected request changes nothing.
- **Q/E/R slots:**
  - A newly learned node places its abilities on the first free slot of its tree.
  - Assigning an ability that already occupies another slot swaps the two slots.
  - Only abilities of learned nodes can be assigned.
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

| Part | Role |
| --- | --- |
| `URpgSkillProgressionViewModel` | Character level, trade skills with XP, one tree view model per known tree, the selected tree, free points. `OnProgressionChanged` also fires when the selected tree changes, so a screen refreshes from one event. |
| `URpgSkillTreeViewModel` | Points, rows with their gates, prerequisite links, Q/E/R slots, the selected node, and the commands learn, reset, select and assign. |
| `URpgSkillTreeNodeViewModel` | One node: name, text, icon, kind, cell, cost, state (`Unlocked`, `Unlockable`, `Unaffordable`, `Locked`, `Excluded`), slot. |
| `URpgSkillTreeGridWidget` | Places one entry per node from `Row` and `Column`, draws the links and darkens locked rows. Entries implement User Object List Entry. |
| `CUI_Skills`, `CUI_SkillTreeNode`, `CUI_TradeSkillEntry`, `CUI_SkillTreeListEntry` | Layout and style in `/Game/SurvivalRpg/UI/Menus/GameMenu/GameMenu/Skills`. |

The view models observe the replicated state and rebuild at most once per frame. Trade skill names come from
`DisplayName` in the trade skill config, or the last part of the skill tag.

## Current trees

| Tree | Points | Nodes |
| --- | --- | --- |
| `DA_SkillTree_Axe` (`GF_Harvesting_Magic`) | Logging | Death Wave (row 0, column 1) chaining into Wide Wave and Long Reach; Grave Swarm (row 0, column 5) forking into the exclusive forms Swarm Brood and Grave Detonation; the loose passives Keen Edge and Quick Recovery in column 3 |
| `DA_SkillTree_Pickaxe` (`GF_Harvesting_Magic`) | Mining | Rift Grip (row 0, column 2) forking into the exclusive forms Wide Rift and Deep Grip; the loose passives Steady Hands and Quick Recovery beside it |

The upgrades and forms only carry tunings. The harvest abilities read the tags under `Ability.Tuning.Harvest`
(area radius, reach, target count, sections, swarm creatures, rest and strike radius, cooldown); the table is in
[harvesting-roadmap.md](harvesting-roadmap.md#harvest-tunings).

## How to extend

### Add a tree for a new weapon or tool

1. Create a `URpgSkillTreeDefinition` DataAsset in the feature that owns the weapon:
   - Set `TreeTag` (`SkillTree.Tree.<Weapon>`) and `MasterySkillTag`.
   - Set `MaxPoints`, and optionally a `PointsByLevel` curve and `BranchNames`.
2. Add `URpgInventoryFragment_SkillTree` to the weapon's item definition and point it at the tree. Change `ActiveInSlots` only for off-hand items.
3. Make sure the asset manager scans the folder (`DefaultGame.ini` for `/Game`, the GameFeatureData for a plugin).
4. Run data validation. The automation test `SurvivalRpg.Progression.SkillTrees.Content.AllTreesValid` checks every tree.

No code, widget or save change is needed; the skill screen lists the tree and draws it.

### Add a node

Add an entry to `Nodes`:
- `NodeTag`: `SkillTree.Node.<Weapon>.<Name>`. Never rename it once shipped; saves use it.
- `Row` and `Column` for the grid, and `KindTag` for the style (`SkillTree.NodeKind.Active`, `Passive`, `Upgrade`, `Ultimate`).
  - Put exclusive alternatives side by side in one row below their prerequisite, so the links fork. Chain upgrades that stack straight down, each requiring the one above, so no link passes behind another node.
  - A loose passive has no prerequisites.
  - Keep `RequiredPointsInTree` at 0 except for capstones such as an ultimate, so players choose freely and only chains gate nodes.
- `Cost`, `RequiredPointsInTree`, `Prerequisites` and optionally `ExclusiveGroup`.
- Any combination of grants:
  - **Active ability:** an ability set whose entry has an `AbilityIdTag` and no Q/E/R input tag. The tree places it on Q/E/R.
  - **Passive value:** a GameplayEffect in the node's ability set.
  - **Flag for other systems:** `GrantedTags`.
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
- reset and slots
- weapon grants
- tunings
- Q/E/R through the weapon loadout
- save and restore

- the view models: projection, commands, coalesced refresh and the overview

`SurvivalRpg.Save.WorldSave.MemoryRoundTrip` covers the save fields.
`SurvivalRpg.Harvesting.Content.AxeSkillTreeContract` and `PickaxeSkillTreeContract` check the chain item →
fragment → tree → node → ability set → ability for the tool trees.
`SurvivalRpg.Harvesting.Tuning.*` checks that harvest tunings change preview and commit alike, are captured when
execution starts, and shape the swarm; `SurvivalRpg.Harvesting.Swarm.StrikeRadiusTakesEachTargetOnce` checks the
strike radius.
