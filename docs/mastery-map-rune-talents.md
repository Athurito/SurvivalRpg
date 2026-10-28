# MST-06c: Rune-bound talents

Task report for MST-06c in the [mastery map roadmap](mastery-map-roadmap.md), [PR #318](https://github.com/Athurito/SurvivalRpg/pull/318). It adds the source-tool activation rule of the [implementation plan](mastery-map-implementation-plan.md), section "Shared harvesting progression", and Brand's talents Anhaltende Macht (Lasting Power) and Entladung (Discharge), as the user accepted on 2026-10-28 ([MST-06a report](mastery-map-socket-ui.md#user-decisions-2026-10-28)). It builds on Brand's burn from [MST-06b](mastery-map-brand-burn.md).

## Scope change

The rune bar with highlights on the skill screen moves to a new step, MST-06d, so this PR stays bounded. The talents and their sleeping state are complete without it: the node and its card say when a rune is missing.

## C++ boundary decision

- **Classification:**
  - An extension of the skill tree schema and its one activation rule.
  - An extension of the burn mechanism by two talent behaviors.
  - The talents, their values and the screen texts are content.
- **Runtime truth:**
  - `URpgSkillTreeComponent` still owns what is learned. A rune never changes ownership, points or saves.
  - Whether a learned node works is derived on demand from the source item's sockets. Nothing new is replicated or saved.
- **Native:**
  - **Schema:** `FRpgSkillTreeNode::RequiredRunes` and `RuneMatch` (any or all), and `FRpgSkillTreeNode::IsActiveWithRunes`.
  - **One rule:** `URpgSkillTreeComponent::IsNodeActiveForItem` over `URpgInventoryFragment_Rune::GetSocketedRuneTags`. Three places use it:
    - The equipment manager grants a learned node only while it is active on the equipped item.
    - `ResolveAbilityTuningForSpec` folds a node's tunings only while it is active on the spec's source item.
    - The skill tree view model marks a learned node `bIsDormant` while the tool of its tree in hand lacks its runes.
  - **View model:** `URpgSkillTreeNodeViewModel` gains `RequiredRunes`, `bNeedsRunes`, `bIsDormant` and `RuneRequirementText`. `URpgSkillTreeComponent::FindActiveItemOfTree` finds the tool in hand.
  - **Burn:**
    - `ARpgHarvestBurn::Light` takes `FRpgHarvestBurnParams`. A further hit on a resource the player burns restarts the burn (`bRefresh`), or releases its remaining work at once times a multiplier and puts it out (`DischargeMultiplier`, which wins).
    - Burns count their ticks, so the remaining work is exact.
  - **Tunings:** `Ability.Tuning.Harvest.BurnRefresh` and `.BurnDischarge`, both 0 by default.
- **Assets (Unreal MCP):**
  - **`DA_SkillTree_Harvesting`:** the region Aftermath with two talents, both requiring `Rune.Brand`:
    - **Lasting Power (1 point):** burn time ×1.5, and further hits restart the burn.
    - **Discharge (3 points, after Lasting Power):** a further hit releases the remaining burn with ×1.5 work.
  - **`CUI_SkillTreeNode`:** the new function `F_ApplyDormancy` dims a sleeping node and labels it "Sleeping - rune missing". `OnListItemObjectSet` calls it after `F_Refresh`.
  - **`CUI_Skills`:** the new function `F_ApplyRuneRequirement`, called after `F_ApplyRequestFeedback`. The card names the rune requirement and says "Learned, sleeping" while the node sleeps.
  - **Tags:** `SkillTree.Node.Harvesting.Aftermath.LastingPower` and `.Discharge`.
- **Discharge without a choice group:** the plan pairs Discharge with Wandering Effect (Wandernde Wirkung), which needs Erntesprung (MST-09b). Until that alternative exists, Discharge is a plain follow-up talent, not a one-member choice.

## Behavior

### Problem

Brand burned, but the Aftermath talents did not exist. Talents had no way to depend on the runes in a tool, so a rune talent would either always work or never.

### Change

- **Learn first:** Lasting Power and Discharge can be learned without Brand.
- **Sleeping:** while the tool in hand has no Brand in its sockets, the talents sleep. The node is dimmed and labeled "Sleeping - rune missing", and the card says "Learned, sleeping" and names the missing rune.
- **Waking:** socketing Brand into the tool in hand wakes them without a new purchase. Taking it out puts them to sleep again, and their points stay spent.
- **Only the right tool:** a talent works only through the tool that carries Brand. Brand in another tool does not wake it.
- **Lasting Power:** burns last 6 s instead of 4 s, and every further hit on a burning resource restarts its burn.
- **Discharge:** a further hit on a resource you burn releases the rest of the burn at once, with half again its work, and puts the burn out. The next hit lights a new burn.
- **Co-op:** both stay per player. A partner's burn on the same resource is not refreshed or discharged by your hits.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - `SurvivalRpg.Runes.Talents.RuneBoundNodesSleepWithoutTheirRune`, with a real equipment manager, player inventory and skill trees:
    - A rune-bound talent can be learned without its rune.
    - Without the rune it adds no tuning and no grant, and the view model shows it sleeping.
    - With the rune socketed it tunes the tool's abilities and is granted once.
    - Removing the rune puts it to sleep without a refund, and socketing again wakes it.
    - With no tool in hand it shows as sleeping.
  - `SurvivalRpg.Harvesting.Work.BurnRefreshesAndDischarges`:
    - A refreshing hit restarts the remaining ticks; six ticks complete one section.
    - A discharging hit releases three ticks times 1.5 at once, as secondary work, and leaves no burn.
    - Every section rewards the igniting player once.
- **Changed tests:**
  - The two MST-06b burn tests use `FRpgHarvestBurnParams`.
  - `SurvivalRpg.Harvesting.Content.SharedHarvestingTreeContract`:
    - Now checks one entry per branch, and that every other talent builds on its branch's entry.
    - Choice groups must have at least two members.
    - The Aftermath talents need Brand.
  - **Failed attempts:**
    - The new test first failed: without a controller, the test player inventory has no layout and no Pockets, so it now uses the default storage grid.
    - The contract then failed on the second entry talent and on Discharge's one-member choice group; both led to the changes above.
- `ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage --null-rhi`: **455 of 455 passed**; the 16 with warnings are the known ones.
- `ue.py test SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.InventoryReorderPIE`: **13 of 13 passed**, only `LogNetPackageMap` warnings.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players. For the check only, the starter also gave a pickaxe in hand; this was reverted and not committed. Mastery XP came from `Rpg.Progression.AddSkillXP`.
  - **Map:** the Aftermath branch appears beside Impact, with its own region shortcut.
  - **Host:**
    - Lasting Power's card said "Learnable" and "Needs Brand in the tool's sockets".
    - After Learn, 1 point was spent. The node dimmed to "Sleeping - rune missing", and the card said "Learned, sleeping".
    - With Brand socketed into the pickaxe in hand, reopening the screen showed "Learned", with the same 1 point spent.
  - **Client:** the host's purchase did not touch the client's points. The client learned Lasting Power through its own request, and it showed "Learned, sleeping".

## Playtest

1. **Points:** in `Lvl_HarvestPickaxe`, mine or chop until Harvesting reaches level 2 or higher, or run `Rpg.Progression.AddSkillXP Skill.Mastery.Harvesting 400` on the host.
2. **Learn without Brand:** press H and choose the Aftermath shortcut. Learn Lasting Power. With a tool in hand that has no Brand, the node is dim and says "Sleeping - rune missing".
3. **Wake it:** socket Brand into the axe or pickaxe in Weapon I and open H again. Lasting Power shows "Learned", with no point spent again.
4. **Lasting Power:** strike a vein twice within a few seconds. The burn keeps glowing longer than 4 s.
5. **Discharge:** learn Discharge, strike a vein, and strike it again while it glows. The glow goes out, and the vein loses a larger chunk at once.
6. **Multiplayer:** in a two-player listen server, each player learns and wakes the talents only for themself.

## Limits

- **Rune bar and highlights:** these move to MST-06d. Without them, the skill screen shows a rune only through the node's sleeping state and the card text.
- **Refresh while open:** the sleeping state is computed when the skill screen opens; socketing cannot happen while it is open.
- **Discharge has no alternative yet:** Wandering Effect waits for Erntesprung (MST-09b).
- **Not seen by hand:** the refresh and discharge in a running game are covered by the native burn test, not by the agent's PIE session.
- **Rune names:** the card names runes from their tags (`Rune.Brand` reads "Brand"), not from localized item names.

## Next

MST-06d: the rune bar on the skill screen, with the tool in hand and its three sockets above the map, and highlights of a rune's talents.
