# MST-09b: Erntesprung, Durchschlag, Spread and Wandering Effect

Task report for MST-09b in the [mastery map roadmap](mastery-map-roadmap.md). It adds the harvesting runes Erntesprung and Durchschlag, the Spread branch (Ausbreitung) of the [implementation plan](mastery-map-implementation-plan.md), section "Target content catalog": Weite Bahnen, then Gabelung or Tiefer Riss. It also adds the last Aftermath choice, Wandernde Wirkung. The plan's rules apply:
- Propagation is finite.
- No resource takes the same follow-up twice.
- Follow-up work never counts as an own hit.

## User decisions (2026-10-30)

The user accepted the proposals for the runes and the talents and asked to build MST-09b, MST-09c and MST-09d one after another, as three PRs.

- **Erntesprung (Harvest Leap):**
  - Every own hit leaps on one after another to up to two more resources of the same kind within 6 m.
  - Each leap does 30, 45 or 60 % (rank I, II or III) of the hit's work.
  - A leap never strikes a resource twice.
- **Durchschlag (Pierce):** every own hit drives through the struck resource into the next resource behind it, in the swing direction within 4 m, with 50, 75 or 100 % of the hit's work.
- **Spread (Ausbreitung)**, active while Erntesprung or Durchschlag sits in the tool:
  - **Weite Bahnen (Wide Paths, 1 point):** leaps and pierces reach 50 % farther.
  - **Gabelung (Fork, 3 points):** every leap step and every pierce strikes two resources, each with 60 % of the work.
  - **Tiefer Riss (Deep Rift, 3 points):** leaps and pierces do 50 % more work.
  - **Exclusive choice:** Fork and Deep Rift exclude each other.
- **Wandernde Wirkung (Wandering Effect, 3 points):**
  - Needs Brand and Erntesprung in the tool.
  - When a resource burning for the player runs empty, the burn moves on with its remaining time to the nearest resource of the same kind.
  - It becomes the alternative to Entladung (Discharge).
- **All of it is follow-up work:** it counts as no hit and triggers nothing further, and yield and XP go to the player who caused it.
- **How players get the runes:** as for Brand, Echo and Kraftübertragung:
  - selectable in the rune reward;
  - a 2 % drop;
  - a recipe at the Kiln after the first find;
  - the same costs at the Runenschrein.

## C++ boundary decision

- **Classification:** extensions of existing mechanisms; the runes, talents and values are content.
  - **Harvest ability:** two more follow-up effects of own hits.
  - **Burn:** it can move on.
  - **Rune strike:** it can wait longer.
- **Runtime truth:**
  - **Stock:** the stock owner of each resource stays the only stock truth. Leaps, pierces and wandering burns only send requests through its normal commit.
  - **Leap chain:** planned on the server at the hit, from the resources around each step. The strikes carry it out; nothing else keeps state.
  - **Wandering burn:** the burn actor owns its remaining ticks. A wander spawns a new burn with exactly those ticks.
  - **Ranks and talents:** stay with the inventory graph and the skill tree, read live as tunings.
- **Existing seams:**
  - `URpgGameplayAbility_Harvest::CommitSelection` and its tuned values.
  - `ARpgHarvestRuneStrike` and `FRpgHarvestRuneFlow` (MST-09a).
  - `ARpgHarvestBurn` (MST-06b).
  - `FRpgHarvestTargeting::CollectAreaTargets`.
  - The tuning accumulator with the MST-06c source-tool rule and `ERpgSkillTreeRuneMatch::AllRunes`.
- **Native:**
  - **`FRpgHarvestRuneFlow` rules:**
    - `PlanLeaps` plans the chain: each step takes the nearest resources around the previous one that no step struck yet, and the chain ends after the given jumps or when nothing is left.
    - `PickPierceTargets` takes the nearest resources ahead of the struck one, inside a cone around the hit's direction.
    - `PickFollowUpTargets` takes a list of excluded resources (`FRpgHarvestResourceId`).
  - **`ARpgHarvestRuneStrike::Launch`:** an extra delay, so the later steps of a leap land one after another.
  - **`ARpgHarvestBurn`:**
    - `FRpgHarvestBurnParams::WanderRadius` and `WanderChannel`.
    - When a burn's resource runs empty, by the burn or any hit, and ticks are left, a new burn of the same class takes over those ticks on the nearest other resource the tool can work that does not burn for the player yet.
    - Every tick counts, so the wandering always ends.
  - **`URpgGameplayAbility_Harvest`:**
    - **Classes:** `LeapStrikeClass` and `PierceStrikeClass`.
    - **Designer values:** `LeapRadius` (600 cm), `LeapJumps` (2), `LeapStepSeconds` (0.2 s), `PierceDepth` (400 cm) and `PierceConeDegrees` (35°).
    - **Tuned values:** `LeapWork`, `PierceWork`, `SpreadReach`, `SpreadSplit` and `bBurnWander`, from new tunings.
    - **Logic:** `SendSpreadStrikes` sends the leaps and the pierce of the first struck resource after an accepted own hit. `CollectFollowUpCandidates` is the shared query of echoes, transfers, leaps and pierces.
  - **Tunings:** `Ability.Tuning.Harvest.LeapWork`, `.PierceWork`, `.SpreadReach`, `.SpreadSplit` and `.BurnWander`.
  - **New native classes:** none.
- **Assets (Unreal MCP), in `GF_Harvesting_Magic`:**
  - **`ID_Rune_Erntesprung`:** "Erntesprung", icon `T_UI_Item_SpellFocus`. Ranks add `LeapWork` 0.3, 0.45 and 0.6. Brand's upgrade costs and salvage outputs, and base recipe `DA_Recipe_Rune_Erntesprung`.
  - **`ID_Rune_Durchschlag`:** "Durchschlag", icon `T_UI_Item_RiftResidue`. Ranks add `PierceWork` 0.5, 0.75 and 1.0. Base recipe `DA_Recipe_Rune_Durchschlag`.
  - **Recipes:** both cost 4 ore and 2 charcoal, are locked until found, and sit in `DA_RecipeSet_Kiln`.
  - **Strike classes:** duplicated from the Echo strike, with the spark burst template.
    - `BP_HarvestStrike_Erntesprung`: ProcTag `Rune.Erntesprung`, 0.3 s delay, green light.
    - `BP_HarvestStrike_Durchschlag`: ProcTag `Rune.Durchschlag`, 0.1 s, red light.
  - **Swings:** `GA_Harvest_AxeChop` and `GA_Harvest_PickaxeStrike` use both strike classes.
  - **Reward:** `DA_Reward_HarvestingRuneChoice` offers all five harvesting runes.
  - **Loot:** `LT_IronVein` and `LT_DeadPine` drop each new rune at 2 %.
  - **`DA_SkillTree_Harvesting`:**
    - **Region Spread:** Wide Paths (1 point), Fork and Deep Rift (3 points each, group `SpreadChoice`). All three need `Rune.Erntesprung` or `Rune.Durchschlag` (any).
    - **Tunings:**
      - Wide Paths: SpreadReach ×1.5.
      - Fork: SpreadSplit +0.6.
      - Deep Rift: LeapWork and PierceWork ×1.5.
    - **Region Aftermath:**
      - Wandering Effect (3 points, after Lasting Power, BurnWander +1) needs `Rune.Brand` and `Rune.Erntesprung` (all).
      - Discharge and Wandering Effect share the new group `AftermathChoice`; Discharge moved left to make room.
  - **`BP_HarvestingRuneStarterInventory`:** the development starter also gives Erntesprung and Durchschlag (only if missing).
  - **Tags:** `Rune.Erntesprung`, `Rune.Durchschlag`, `Recipe.Rune.Erntesprung`, `Recipe.Rune.Durchschlag`, `SkillTree.Node.Harvesting.Spread.WidePaths`, `.Fork`, `.DeepRift` and `SkillTree.Node.Harvesting.Aftermath.Wander`.

## Behavior

### Problem

Brand, Echo and Kraftübertragung were the only harvesting runes. The Spread branch did not exist, and Brand's Discharge had no alternative.

### Change

- **Erntesprung in a tool:** every hit sends a green spark burst to the nearest other resource of the same kind within 6 m, and a moment later one more on from there. Each does 30 % of the hit's work at rank I. A leap never comes back to a resource this hit already struck.
- **Durchschlag in a tool:** every hit sends a red spark burst into the next resource behind the struck one, in the direction of the swing within 4 m, with half the hit's work at rank I. Resources beside the struck one or behind the player stay untouched.
- **Spread talents:** they can be learned without the runes and sleep until Erntesprung or Durchschlag sits in the tool in hand.
  - **Wide Paths:** leaps and pierces reach half again as far, and so do wandering burns.
  - **Fork:** every leap step and pierce strikes two resources, each with 60 % of the work.
  - **Deep Rift:** leaps and pierces do half again as much work.
- **Wandering Effect:** with Brand and Erntesprung in the tool, a burn whose resource runs empty moves on to the nearest resource of the same kind within the leap's reach. It burns there for its remaining time. Discharge and Wandering Effect are now an exclusive choice; a learned Discharge stays learned.
- **Getting the runes:**
  - Choose them at Harvesting level 3 or after closing the Root Grove portal.
  - Find them in veins and dead pines (2 % each).
  - Forge them at the Kiln after the first find.
  - Upgrade and salvage them at the Runenschrein.
- **Co-op:** leaps, pierces, wandering burns and their rewards belong to the player whose hit caused them, and every player sees the spark bursts and burns.

## Validation

Run on 2026-10-30 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Harvesting.Work.SpreadStrikesEachResourceOnce`:**
    - Two leaps go to the nearest resource, then on from there.
    - Eight allowed jumps end once nothing new is in reach; the struck resource and every leaped one are struck once.
    - A forked step takes the two nearest resources.
    - A pierce takes only resources ahead inside its cone. A wider cone takes the one beside, and a hit without horizontal direction pierces nothing.
  - **`SurvivalRpg.Harvesting.Ability.LeapStrikesEachResourceOnce`,** with real swings of a tree-tool ability:
    - A hit leaps 50 units to the nearest resource after the strike delay, and 50 more one step later from there.
    - Nothing comes back to the struck resource, nothing reaches resources beyond the reach, and the leaps add no yield of their own.
    - Forked, A and B take 30 units each and C 30 more.
  - **`SurvivalRpg.Harvesting.Ability.PierceStrikesTheResourceBehind`:**
    - A hit drives a whole section into the resource 3 m behind the struck one, and the section rewards the harvester.
    - Resources beside it, behind the harvester and beyond 4 m stay untouched.
    - Forked, the two resources behind take 60 units each.
  - **`SurvivalRpg.Harvesting.Ability.BurnWandersOnWhenItsResourceRunsEmpty`:**
    - Without the talent, a burn goes out with its resource.
    - With it, the burn moves on to the neighbor with exactly its two remaining ticks, for the same player.
    - Those two ticks complete one section, the burn then goes out, and a resource beyond the reach stays untouched.
    - Every completed section rewards the igniting player once.
  - **`SurvivalRpg.Harvesting.Content.SpreadContract`:**
    - Both runes reach rank III, every rank works and is no weaker than the one below, and ranks II and III cost.
    - Their locked base recipes forge them and sit in the Kiln set.
    - The rune choice offers them, the pickaxe sockets take them, and veins and pines drop them rarely.
    - Both swings leap and pierce with strikes that name the runes.
    - The Spread branch has three talents that need Erntesprung or Durchschlag, and one choice group.
    - Wandering Effect needs Brand and Erntesprung, follows Lasting Power and shares Discharge's choice group.
  - **Refactor:** `Harvesting.Content.RuneFlowContract` now uses the same per-rune and per-swing checks as the Spread contract.
- **Failed attempt:** the first rules test excluded the struck resource with item `INDEX_NONE`. `FHitResult` starts with item 0, so the chain took the struck resource first. The test now uses item 0, as the candidates do.
- **Asset authoring:** the first tree edit changed Discharge and appended nodes in one `set_properties` call, which the array rule refuses. It was split into two calls, as the skill reference describes.
- **Icon:** Erntesprung first used `T_UI_Item_RiftFragment`, which is Brand's icon; the PIE check showed two identical runes in the pockets. It now uses `T_UI_Item_SpellFocus`.
- **Full test run:** `ue.py test SurvivalRpg.Portals SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **498 of 498 passed**, 17 with the known warnings.
- **Network tests:** `ue.py test SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.SkillTreePIE`: **12 of 12 passed**, only `LogNetPackageMap` warnings.
- **Cook:** `ue.py cook Lvl_HarvestPickaxe`: passed, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **Skill sync:** `python Build/Tools/AgentSkills/sync.py --check`: consistent. The test pitfalls reference records that a default `FHitResult` names item 0.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players:
  - **Inventory:** the host's pockets held Brand, Echo, Kraftübertragung, Erntesprung and Durchschlag from the starter.
  - **Skill map:** the shortcut Spread showed Wide Paths ("Needs 1 pt") above Fork and Deep Rift ("Locked"). Aftermath showed Lasting Power above Discharge and Wandering Effect.
  - **Not checked in PIE:** swings with the new runes socketed, as in MST-09a. The native ability tests cover leaps, pierces and wandering burns with real swings.

## Playtest

1. **Runes:** in `Lvl_HarvestPickaxe`, open the inventory (Tab). Your pockets hold Brand, Echo, Kraftübertragung, Erntesprung and Durchschlag.
2. **Erntesprung:** put the pickaxe into Weapon I and drag Erntesprung into a socket. Strike an iron vein that has neighbors: a green spark burst flashes on the nearest other vein, then on one more, and both lose a little stock.
3. **Durchschlag:** socket Durchschlag instead. Strike a vein with another vein right behind it, seen from you: a red spark burst flashes on the one behind.
4. **Spread:** press H, choose Spread. Learn Wide Paths, then Fork (two bursts per step) or Deep Rift (stronger bursts). The talents sleep while the pickaxe in hand has neither rune.
5. **Wandering Effect:** socket Brand and Erntesprung, learn Lasting Power and Wandering Effect in Aftermath. Let a vein with little stock burn empty: its glow moves on to the nearest vein and keeps working it for the rest of its time.
6. **Kiln and shrine:** carrying the runes teaches the Kiln their recipes (4 ore, 2 charcoal). The Runenschrein upgrades and salvages them.
7. **Multiplayer:** in a two-player listen server, the client's leaps, pierces and wandering burns reward only the client, and the host sees their bursts and glows.

## Limits

- **Presentation:** the leap and pierce are placeholder spark bursts with a colored light on the struck resource; they show no path from the source.
- **Tuning:** the values (30 to 60 %, 50 to 100 %, 6 m, 4 m, 35°, two jumps, 60 %, ×1.5) are first tuning in assets.
- **Development starter:** the starter now gives all five harvesting runes to every player of the harvesting feature, as it gave Brand.
- **Swings in the agent check:** the agent PIE check did not swing with the new runes; the native ability tests cover that gameplay.
- **Wander timing:** a burn notices that its resource ran empty at its next tick, so a wandering burn moves on up to one second after the resource emptied.

## Next

MST-09c: Totenarbeiter and the Helper branch (Gemeinsamer Hieb, Rotte, Werkmeister).
