# MST-09a: Echo, Kraftübertragung and Rune Flow

Task report for MST-09a in the [mastery map roadmap](mastery-map-roadmap.md), [PR #325](https://github.com/Athurito/SurvivalRpg/pull/325). It adds the harvesting runes Echo and Kraftübertragung and the Rune Flow branch (Runenfluss) of the [implementation plan](mastery-map-implementation-plan.md), section "Target content catalog": Runenresonanz, then Teilung or Verdichtung. The plan's rules apply:
- Own direct hits drive Echo.
- Follow-up work carries provenance and a finite chain budget.
- Only actual overkill enters Kraftübertragung.

## User decisions (2026-10-29)

The user accepted the proposals for the runes and the talents, and asked for one PR.

- **Echo:**
  - Every third own hit of a tool with Echo echoes 0.3 s later on the same resource, with 50, 75 or 100 % (rank I, II or III) of that hit's work.
  - The charge expires after 5 s without a hit, or when the tool is put away.
  - An echo does not count as a hit and triggers nothing further.
- **Kraftübertragung:**
  - When an own hit empties a resource with actual overkill, 50, 75 or 100 % of the overkill carries over to the nearest resource of the same kind within 8 m (tree to tree, vein to vein).
  - This happens once per hit, and carried work is not carried on.
- **Both runes:** yield and XP always go to the player, and the yield per section stays the same.
- **How players get them:** as for Brand:
  - selectable in the rune reward;
  - a 2 % drop;
  - a recipe at the Kiln after the first find;
  - the same costs at the Runenschrein.
- **Rune Flow** (works only while Echo or Kraftübertragung sits in the tool):
  - **Runenresonanz (Rune Resonance, 1 point):** Echo and Kraftübertragung work 25 % stronger.
  - **Teilung (Division, 3 points):** echoes and transfers also strike a second resource of the same kind within 8 m. Each strike does 60 % of the work.
  - **Verdichtung (Condensation, 3 points):** the echo already comes every second hit, and transfers carry 50 % more overkill.
  - **Exclusive choice:** Teilung and Verdichtung exclude each other.

## C++ boundary decision

- **Classification:** two extensions of existing mechanisms; the runes, talents and values are content.
  - **Harvest ability:** two follow-up effects of own hits.
  - **Harvesting work:** one reusable follow-up work source, a delayed strike, next to the burn and the swarm.
- **Runtime truth:**
  - **Stock:** the resource's stock owner (actor node or instance stock) stays the only stock truth; strikes only send requests through its normal commit.
  - **Echo charge:** server-only state of the harvest ability instance, so it ends when the tool's abilities are removed.
  - **Strike actor:** owns its request and delay, server-only. It replicates only its existence, for the presentation.
  - **Ranks and talents:** stay with the inventory graph and the skill tree, read live as tunings.
- **Existing seams:**
  - `URpgGameplayAbility_Harvest::CommitSelection` and its tuned values.
  - The ability tuning accumulator (Set, then Add, then Multiply) over rune ranks and learned nodes, with the MST-06c source-tool rule.
  - `FRpgHarvestProvenance::MakeSecondary`, and `FRpgHarvestResult::OverkillWork` and `IsAcceptedDirectHit` from MST-03a.
  - `FRpgHarvestTargeting::CollectAreaTargets`.
  - The burn's spawn and commit pattern (MST-06b).
- **Native:**
  - **`ARpgHarvestRuneStrike`:** one delayed follow-up hit on one resource.
    - **Timing:** `Launch` spawns it on the server. After `DelaySeconds` it commits secondary work of the triggering action once, for the triggering player, with `ProcTag`.
    - **Stale resources:** it takes the revision at launch, so a resource emptied or respawned meanwhile takes nothing.
    - **Protection:** a strike on another resource is an area request, so protected resources refuse it.
    - **Presentation:** the actor replicates without movement and stays `LingerSeconds` for its presentation.
  - **`FRpgHarvestEchoCharge`:** counts accepted own hits with a timeout.
  - **`FRpgHarvestRuneFlow`:**
    - `PickFollowUpTargets` takes the nearest resources a follow-up would harvest, never the struck one.
    - `TransferSections` turns actual overkill into sections and gives zero without overkill.
  - **`URpgGameplayAbility_Harvest`:**
    - **Classes:** `EchoStrikeClass` and `TransferStrikeClass`.
    - **Designer values:** `EchoHitsPerEcho` (3), `EchoChargeTimeoutSeconds` (5 s) and `RuneFlowRadius` (800 cm).
    - **Tuned values:** `EchoWork`, `EchoHits`, `TransferWork` and `RuneFlowSplit`, from new tunings.
    - **Logic:** `SendRuneFlowStrikes` places the strikes.
      - An echo strikes its own resource while it has work left, plus one more resource when split.
      - A transfer strikes one other resource, two when split.
      - Two strikes each do `RuneFlowSplit` of the work; a single strike does all of it.
  - **Tunings:** `Ability.Tuning.Harvest.EchoWork`, `.EchoHits`, `.TransferWork` and `.RuneFlowSplit`.
  - **New native classes:** one, the strike actor. Like the burn, it needs server timing that outlives the ability activation, and replicated presence for its presentation. No GameplayAbility leaf.
- **Assets (Unreal MCP), in `GF_Harvesting_Magic`:**
  - **`ID_Rune_Echo`:** "Echo", icon `T_UI_Item_RiftCrystal`. Ranks add `EchoWork` 0.5, 0.75 and 1.0. Brand's upgrade costs and salvage outputs, and base recipe `DA_Recipe_Rune_Echo`.
  - **`ID_Rune_Kraftuebertrag`:** "Kraftübertragung", icon `T_UI_Item_RiftCore`. Ranks add `TransferWork` 0.5, 0.75 and 1.0. Base recipe `DA_Recipe_Rune_Kraftuebertrag`.
  - **Recipes:** both cost 4 ore and 2 charcoal, are locked until found, and sit in `DA_RecipeSet_Kiln`.
  - **Strike classes:** `BP_HarvestStrike_Echo` (ProcTag `Rune.Echo`, 0.3 s delay, blue-violet light) and `BP_HarvestStrike_Kraftuebertrag` (`Rune.Kraftuebertrag`, 0.2 s, gold light), both with the spark burst template `NS_FX_Tpl_SparkBurst`.
  - **Swings:** `GA_Harvest_AxeChop` and `GA_Harvest_PickaxeStrike` use both strike classes.
  - **Reward:** `DA_Reward_HarvestingRuneChoice` (Harvesting level 3 and the Root Grove portal) offers Brand, Echo and Kraftübertragung.
  - **Loot:** `LT_IronVein` and `LT_DeadPine` drop each new rune at 2 %, next to Brand.
  - **`DA_SkillTree_Harvesting`:** the region Rune Flow with Rune Resonance (1 point), Division and Condensation (3 points each, group `RuneFlowChoice`).
    - **Rune requirement:** all three need `Rune.Echo` or `Rune.Kraftuebertrag` (any).
    - **Tunings:**
      - Resonance: EchoWork and TransferWork ×1.25.
      - Division: RuneFlowSplit +0.6.
      - Condensation: EchoHits set to 2, TransferWork ×1.5.
  - **`BP_HarvestingRuneStarterInventory`:** the development starter also gives Echo and Kraftübertragung (only if missing), as it gives Brand.
  - **Tags:** `Rune.Echo`, `Rune.Kraftuebertrag`, `Recipe.Rune.Echo`, `Recipe.Rune.Kraftuebertrag` and `SkillTree.Node.Harvesting.RuneFlow.Resonance`, `.Split` and `.Condense`.

## Behavior

### Problem

Brand was the only harvesting rune, and the Rune Flow branch of the plan did not exist.

### Change

- **Echo in a tool:** every third hit on a resource is followed 0.3 s later by a spark burst on the same resource. The burst works it once more with half the hit's work at rank I.
  - Breaks of more than 5 s, or putting the tool away, start the count over.
  - The echo never adds yield; its sections reward you with yield and XP once.
- **Kraftübertragung in a tool:** a hit that empties a resource and has force left over carries that overkill to the nearest resource of the same kind within 8 m, half of it at rank I.
  - A hit that empties a resource exactly carries nothing.
  - Carried work does not carry on.
- **Rune Flow talents:** they can be learned without the runes and sleep until Echo or Kraftübertragung sits in the tool in hand.
  - **Rune Resonance:** both runes work a quarter stronger.
  - **Division:** echoes and transfers also strike the second nearest resource of the same kind, each with 60 % of the work. A single target takes all of it.
  - **Condensation:** the echo comes every second hit, and transfers carry half again as much.
- **Getting the runes:**
  - Choose them at Harvesting level 3 or after closing the Root Grove portal.
  - Find them in veins and dead pines (2 % each).
  - Forge them at the Kiln after the first find.
  - Upgrade and salvage them at the Runenschrein like Brand.
- **Co-op:** echoes, transfers and their rewards belong to the player whose hit caused them. Every player sees the spark bursts.

## Validation

Run on 2026-10-29 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Harvesting.Work.EchoChargeCountsOwnHits`:** the third hit echoes and the count starts over; a hit after the timeout starts a new count; two hits, and one hit, echo when tuned so; no count echoes nothing.
  - **`SurvivalRpg.Harvesting.Work.RuneStrikeWorksLaterForItsPlayer`:**
    - No strike without work. The strike waits for its delay, then does its work as secondary work of the triggering action.
    - A whole-section strike rewards its player with yield and XP once.
    - A strike on a resource emptied meanwhile takes and rewards nothing; the total yield equals the stock.
  - **`SurvivalRpg.Harvesting.Work.RuneFlowCarriesOnlyActualOverkill`:**
    - Overkill converts to sections by share. Exact completion, refusals and a zero share carry nothing.
    - Follow-up targets are the nearest other resources a strike would harvest, never the struck instance; another instance of the same component counts.
  - **`SurvivalRpg.Harvesting.Ability.EchoRepeatsEveryThirdOwnHit`,** with real swings of a tree-tool ability:
    - Two own hits do not echo, and the third echoes 50 units on the struck resource after the delay, without extra yield.
    - With every-hit and split tunings, the echo does 30 units on the struck resource and 30 on its neighbor.
  - **`SurvivalRpg.Harvesting.Ability.TransferCarriesOverkillToTheNextResource`:**
    - A partial and an exactly emptying hit carry nothing.
    - Three sections on a one-section resource carry two sections to the nearest neighbor and none to a farther one. The carried sections reward the harvester and are reported as follow-up work.
    - Split, the two nearest neighbors take 1.2 sections each.
  - **`SurvivalRpg.Harvesting.Content.RuneFlowContract`:**
    - Both runes reach rank III, every rank works and is no weaker than the one below, and ranks II and III cost.
    - Their locked base recipes forge them and sit in the Kiln set; the rune choice offers them; the pickaxe sockets take them; veins and pines drop them rarely.
    - Both swings echo and transfer with strikes that name the runes.
    - The Rune Flow branch has three talents that need Echo or Kraftübertragung, and one choice group.
- **Failed attempts and fixes:**
  - **Missing include:** the first build failed because `RpgHarvestRuneStrike.cpp` lacked the player state include, the same pitfall as the burn in MST-06b.
  - **Wrong namespace:** the ability tests first used the tuning helpers from the wrong namespace.
  - **Abstract test object:** the rules test first created abstract `UObject` receivers.
  - **Pawn without root:** the transfer test first moved the test pawn, which has no root component, so it did not move. The cases now sit one after another on the view ray, and each struck resource is removed once its case is done.
  - **Rank tunings:** `set_properties` refused to shorten each rank's tuning list while changing its elements, and still applied the other members of the call. The ranks were set in two calls; this is recorded in the skill reference.
  - **Item compile:** the duplicated items failed to compile until their recipes output them; this is recorded as well.
- **Full test run:** `ue.py test SurvivalRpg.Portals SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **493 of 493 passed**, 17 with the known warnings.
- **Network tests:** `ue.py test SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.SkillTreePIE`: **12 of 12 passed**, only `LogNetPackageMap` warnings.
- **Cook:** `ue.py cook Lvl_HarvestPickaxe`: passed, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **Skill sync:** `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players:
  - **Inventory:** the host's pockets held Brand, Echo and Kraftübertragung from the starter, each with its icon.
  - **Skill map:** it showed the region Rune Flow with its own shortcut: Rune Resonance ("Needs 1 pt") above Division and Condensation ("Locked"), joined by links.
  - **Not checked in PIE:** swings with Echo or Kraftübertragung socketed. Aiming a swing at a vein and socketing by drag with the PIE tools would have needed a temporary level setup. The native ability tests cover the gameplay with real swings and real strike actors.

## Playtest

1. **Runes:** in `Lvl_HarvestPickaxe`, open the inventory (Tab). Your pockets hold Brand, Echo and Kraftübertragung; their tooltips describe them.
2. **Echo:** pick up the pickaxe, put it into Weapon I, and drag Echo into one of its sockets. Strike an iron vein three times; after the third hit a blue spark burst flashes on the vein, and its stock drops a little further.
3. **Kraftübertragung:** socket Kraftübertragung instead (or as well) and empty a vein that has only a little left with a strong hit. A golden spark burst flashes on the nearest other vein within 8 m, which loses stock; its ore lands in your pockets.
4. **Rune Flow:** press H, choose Rune Flow. Learn Rune Resonance and then Division or Condensation; they sleep while the pickaxe in hand has neither rune.
   - **Division:** echoes and transfers also flash on a second vein.
   - **Condensation:** the echo comes every second hit.
5. **Kiln and shrine:** carrying the runes teaches the Kiln their recipes (4 ore, 2 charcoal). The Runenschrein upgrades and salvages them like Brand.
6. **Multiplayer:** in a two-player listen server, the client's echoes and transfers reward only the client, and the host sees their spark bursts.

## Limits

- **Swings in the agent check:** the agent PIE check did not swing with the new runes; the native ability tests cover that gameplay.
- **Development starter:** the starter now gives all three harvesting runes to every player of the harvesting feature, as it gave Brand.
- **Presentation:** the presentation is a placeholder spark burst with a colored light; it marks the struck resource, not a path from the source.
- **Tuning:** the values (50 to 100 %, 8 m, 3 hits, 5 s, 60 %, ×1.25, ×1.5) are first tuning in assets.
- **Discharge:** Brand's Discharge stays a plain follow-up talent until Wandernde Wirkung arrives in MST-09b.

## Next

MST-09b: Erntesprung, Durchschlag, Ausbreitung and the remaining Nachwirkung choices.
