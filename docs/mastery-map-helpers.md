# MST-09c: Totenarbeiter and the Helper branch

Task report for MST-09c in the [mastery map roadmap](mastery-map-roadmap.md). It adds the harvesting rune Totenarbeiter and the Helper branch (Helfer) of the [implementation plan](mastery-map-implementation-plan.md), section "Target content catalog": Gemeinsamer Hieb, then Rotte or Werkmeister. The plan's rules apply:
- Own direct hits raise the workers.
- Helpers do not summon helpers.
- Beneficiary and rewards stay with the triggering player.
- Triggers are bounded.

## User decisions (2026-10-30)

The user accepted the proposal together with MST-09b, and asked to build MST-09b, MST-09c and MST-09d one after another.

- **Totenarbeiter (Dead Worker):**
  - Every fifth own hit raises a dead worker at the struck resource.
  - For 5 s it works resources of the same kind within 6 m, about every 1.2 s.
  - Each strike does 40, 60 or 80 % (rank I, II or III) of the hit's work.
- **Limits:**
  - A player has at most one worker, or one pack, at a time.
  - The worker keeps working when the player switches tools, and disappears when the player dies.
  - Its strikes are not the player's hits: no echo, no burn, no new workers.
  - Yield and XP go to the player, in one delivery.
- **Helper (Helfer)**, active while Totenarbeiter sits in the tool:
  - **Gemeinsamer Hieb (Shared Blow, 1 point):** workers strike 25 % harder.
  - **Rotte (Pack, 3 points):** two workers rise instead of one, each with 60 % of the work.
  - **Werkmeister (Master Worker, 3 points):** the worker rises every third hit and stays 50 % longer.
  - **Exclusive choice:** Pack and Master Worker exclude each other.
- **How players get the rune:** as for the other harvesting runes (rune reward, 2 % drop, Kiln recipe after the first find, Runenschrein).

## C++ boundary decision

- **Classification:** an extension of the existing swarm and harvest ability; the rune, talents, values and presentation are content.
- **Runtime truth:**
  - **Workers:** the existing `ARpgHarvestSwarm` owns them, with its server-authoritative creatures, reservations, lifetime, summoner death handling and one reward delivery for the beneficiary.
  - **Worker charge:** server-only state of the harvest ability instance, like the echo charge.
  - **One crew per player:** found in the world by beneficiary and the swarm's `ProcTag`, so it holds across both tools of a player.
  - **Ranks and talents:** stay with the inventory graph and the skill tree, read live as tunings.
- **Existing seams:**
  - The swarm of the retired Grave Swarm power (MST-03b removed the power; its native swarm and creature presentation stayed). The plan asked to reuse its lifecycle and beneficiary handling after inspecting compatibility: its strikes commit through `IRpgHarvestableTarget` as follow-up work, never through the harvest ability, so workers cannot trigger echoes, burns, leaps or workers.
  - `URpgGameplayAbility_Harvest::CommitSelection` and `CollectFollowUpCandidates` (MST-09b).
  - `FRpgHarvestEchoCharge` (MST-09a).
- **Native:**
  - **`ARpgHarvestSwarm`:**
    - `ProcTag`, carried in the provenance of every strike.
    - `FindActiveSwarm` returns a player's unfinished swarm of a proc tag.
  - **`URpgGameplayAbility_Harvest`:**
    - **Class:** `WorkerSwarmClass`.
    - **Designer values:** `WorkerHitsPerSummon` (5), `WorkerChargeTimeoutSeconds` (8 s), `WorkerRadius` (600 cm), `WorkerMaxTargets` (6) and `Worker`, the swarm parameters.
    - **`Worker` defaults:** one creature, 500 cm/s, 0.5 s emerge, 1.2 s between strikes, 5 s lifetime, no line of sight needed.
    - **Tuned values:** `WorkerWork`, `WorkerHits`, and `Worker.CreatureCount` and `Worker.MaxLifetimeSeconds`.
    - **Logic:** after an accepted own hit, the charge counts only while the player has no crew. When it completes, `RaiseWorkers` spawns the swarm at the struck point with the resources of the tool around it. Each strike does `WorkerWork` times the hit's own work.
  - **Tunings:** `Ability.Tuning.Harvest.WorkerWork`, `.WorkerCount` (1 to 4), `.WorkerHits` and `.WorkerLifetime`.
  - **New native classes:** none.
- **Assets (Unreal MCP), in `GF_Harvesting_Magic`:**
  - **`ID_Rune_Totenarbeiter`:** "Totenarbeiter", icon `T_UI_Item_Bones`. Ranks add `WorkerWork` 0.4, 0.6 and 0.8. Brand's upgrade costs and salvage outputs, and base recipe `DA_Recipe_Rune_Totenarbeiter` (4 ore, 2 charcoal, locked until found, in `DA_RecipeSet_Kiln`).
  - **`BP_HarvestWorker_Totenarbeiter`:** a new Blueprint of `ARpgHarvestSwarm` with ProcTag `Rune.Totenarbeiter` and the existing grave wisp `BP_HarvestSwarmCreature_Grave` as its creature.
  - **Swings:** `GA_Harvest_AxeChop` and `GA_Harvest_PickaxeStrike` raise it.
  - **Reward, loot and starter:** the rune choice reward, the 2 % vein and pine drops and the development starter include the rune.
  - **`DA_SkillTree_Harvesting`, region Helpers:**
    - Shared Blow (1 point, WorkerWork ×1.25).
    - Pack (3 points, WorkerCount +1 and WorkerWork ×0.6).
    - Master Worker (3 points, WorkerHits set to 3, WorkerLifetime ×1.5).
    - Pack and Master Worker share group `HelperChoice`; all three need `Rune.Totenarbeiter`.
  - **Tags:** `Rune.Totenarbeiter`, `Recipe.Rune.Totenarbeiter` and `SkillTree.Node.Harvesting.Helpers.SharedBlow`, `.Pack` and `.MasterWorker`.

## Behavior

### Problem

The plan's sixth rune and its Helper branch did not exist.

### Change

- **Totenarbeiter in a tool:** every fifth hit raises a grave wisp at the struck resource.
  - For 5 s it flies from resource to resource of the same kind within 6 m, about once a second.
  - Each strike works a resource with 40 % of the hit's work at rank I.
  - Its yield and XP reach the player in one delivery when it is gone.
- **One crew:** hits while the worker works do not count towards the next one. The worker stays when the player puts the tool away, and vanishes when the player dies.
- **Helper talents:** they can be learned without the rune and sleep until Totenarbeiter sits in the tool in hand.
  - **Shared Blow:** workers strike a quarter harder.
  - **Pack:** two wisps rise, each with 60 % of the work.
  - **Master Worker:** the wisp already rises every third hit and stays 7.5 s.
- **Getting the rune:**
  - Choose it at Harvesting level 3 or after closing the Root Grove portal.
  - Find it in veins and dead pines (2 %).
  - Forge it at the Kiln after the first find.
  - Upgrade and salvage it at the Runenschrein.
- **Co-op:** each player's workers work for that player only, and every player sees all wisps.

## Validation

Run on 2026-10-30 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Harvesting.Ability.WorkersRiseEveryFewHitsForTheirPlayer`,** with real swings of a tree-tool ability and a real swarm:
    - No worker rises without worker work, nor after one counted hit; the second counted hit raises one worker for the hitting player.
    - A hit while it works raises no second crew.
    - The worker works the struck resource. Its rewards wait until it finishes, and then every completed section has rewarded the player once.
    - A resource beyond 6 m stays untouched, and worker strikes are follow-up work, never the player's own hits.
    - The hit during the work did not count: one more hit raises nothing, and the next raises the next crew. With Pack it raises two workers, whose sections reward the player once as well.
  - **`SurvivalRpg.Harvesting.Content.HelperContract`:**
    - The rune reaches rank III, every rank works and is no weaker than the one below, and ranks II and III cost.
    - Its locked base recipe forges it and sits in the Kiln set.
    - The rune choice offers it, the pickaxe sockets take it, and veins and pines drop it rarely.
    - Both swings raise workers whose proc tag names the rune.
    - The Helper branch has three talents that need Totenarbeiter, and one choice group.
- **Full test run:** `ue.py test SurvivalRpg.Portals SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **500 of 500 passed**, 17 with the known warnings. The existing `Harvesting.Swarm.*` tests pass with the swarm's new proc tag.
- **Network tests:** `ue.py test SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.SkillTreePIE`: **12 of 12 passed**, only `LogNetPackageMap` warnings.
- **Cook:** `ue.py cook Lvl_HarvestPickaxe`: passed, 0 errors, the 3 known warnings.
- **Failed attempts:** none; the new tests passed on their first run.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players:
  - **Inventory:** the host's pockets held all six harvesting runes from the starter, each with its own icon.
  - **Skill map:** the shortcut Helpers showed Shared Blow ("Needs 1 pt") above Pack and Master Worker ("Locked").
  - **Not checked in PIE:** swings with Totenarbeiter socketed. The native ability test covers the workers with real swings and a real swarm.

## Playtest

1. **Rune:** in `Lvl_HarvestPickaxe`, open the inventory (Tab). Your pockets hold all six harvesting runes, Totenarbeiter with a bone icon.
2. **Workers:** put the pickaxe into Weapon I and drag Totenarbeiter into a socket. Strike an iron vein five times: a pale wisp rises at the vein and strikes it, and nearby veins, about once a second for 5 s. When it is gone, its ore lands in your pockets at once.
3. **One crew:** keep striking while the wisp works; no second wisp rises, and the count starts again once it is gone.
4. **Helpers:** press H, choose Helpers. Learn Shared Blow, then Pack (two wisps) or Master Worker (a wisp every third hit, staying longer). The talents sleep while the pickaxe in hand has no Totenarbeiter.
5. **Kiln and shrine:** carrying the rune teaches the Kiln its recipe (4 ore, 2 charcoal). The Runenschrein upgrades and salvages it.
6. **Multiplayer:** in a two-player listen server, the client's wisp rewards only the client, and the host sees it fly and strike.

## Limits

- **Swings in the agent check:** the agent PIE check did not swing with Totenarbeiter; the native ability test covers the workers.
- **Presentation:** the worker is the grave wisp of the retired Grave Swarm, without a strike burst of its own.
- **Tuning:** the values (every fifth hit, 5 s, 1.2 s, 6 m, 40 to 80 %, ×1.25, two at 60 %, every third hit, ×1.5) are first tuning in assets.
- **Reservations:** a worker strike reserves a whole section of its resource while it is under way. Other workers then pick another resource, even though the strike does less than a section of work. This only affects a pack.

## Next

MST-09d: full catalog acceptance, legacy cleanup, input, performance and network regression, and cook reachability.
