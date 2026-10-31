# MST-06b: Brand's burn

> **To be replaced (2026-10-31):** HRW-05 of the [harvesting rework roadmap](harvesting-rework-roadmap.md) redesigns the harvesting runes and rune-gated talents, and the mastery map roadmap marks this playtest superseded. Until HRW-05 merges, this report still describes the running code.

Task report for MST-06b in the [mastery map roadmap](mastery-map-roadmap.md), [PR #317](https://github.com/Athurito/SurvivalRpg/pull/317). The user split MST-06 and accepted the Brand design on 2026-10-28; both decisions are in the [MST-06a report](mastery-map-socket-ui.md#user-decisions-2026-10-28). This task delivers the rune's own effect. The talents Anhaltende Macht and Entladung follow in MST-06c.

## C++ boundary decision

- **Classification:**
  - An extension of the ability tuning mechanism, so that rune ranks tune their tool's abilities.
  - An extension of the harvesting work mechanism by one follow-up work source, the burn.
  - Brand's values, the burn's presentation and the swings that light it are content.
- **Runtime truth:**
  - The resource's stock owner (actor node or instance stock) stays the only stock truth; the burn only sends requests.
  - The burn actor owns its remaining time and its beneficiary, server-only. It replicates only its existence, for the presentation.
  - Ranks and sockets stay with the inventory graph from MST-05; tunings are read live from it.
- **Native:**
  - **Rank tunings:**
    - `FRpgRuneRank::AbilityTunings` reuses `FRpgSkillTreeAbilityTuning`.
    - `URpgSkillTreeComponent::ResolveAbilityTuningForSpec` folds the learned node tunings of the spec's source item and then the current-rank tunings of the runes socketed in that item. The new `FRpgAbilityTuningAccumulator` combines them.
    - A rune therefore tunes only the abilities of its own tool. This is the first seam of the source-tool evaluation that MST-06c extends to talents.
  - **Tunings:** `Ability.Tuning.Harvest.BurnWork` (work per second as a fraction of the hit's work) and `.BurnDuration` (seconds), both 0 by default.
  - **Burn:** `ARpgHarvestBurn`, the second follow-up work actor after the swarm.
    - `Light` spawns one burn per player and resource. A further hit of the same player keeps the running burn unchanged; refreshing it is Anhaltende Macht's job in MST-06c.
    - Every tick commits secondary work of the igniting action, with `ProcTag` and the igniting player as harvester, through the resource's normal commit.
    - It goes out when its time is up, the resource is empty, respawned or protected, or the player leaves.
  - **Ability:** `URpgGameplayAbility_Harvest::BurnClass`.
    - After a successful direct hit that leaves work on the resource, the server lights that burn.
    - Its work per second is the tuned fraction times the hit's own work (requested sections times work scale, without follow-up or weak-point bonus).
- **Content (Unreal MCP):**
  - `ID_Rune_Brand`: ranks I to III add 0.10, 0.15 and 0.20 burn work and 4 s burn time. The rank texts and the tooltip describe the burn.
  - `BP_HarvestBurn_Brand`: `ProcTag` `Rune.Brand`, a 1 s tick, and as placeholder presentation the ember template `NS_FX_Tpl_EmberDrift` with a warm point light.
  - `GA_Harvest_AxeChop` and `GA_Harvest_PickaxeStrike` use it as `BurnClass`.

## Behavior

### Problem

A socketed rune granted only its tag. Brand changed nothing in harvesting, and runes had no way to tune the tool they sit in.

### Change

- **Brand burns:** a pickaxe or axe with Brand in a socket sets every resource it works burning, as long as stock is left.
  - At rank I the burn does 10 % of the hit's work per second for 4 s, so 40 % of a hit in total; rank II and III do 15 and 20 %.
  - The burn uses the resource's normal stock. It never adds yield.
  - Every section it completes rewards the player who lit it and awards XP once.
- **One burn per player and resource:** hitting a burning resource again keeps the running burn. Two players each keep their own burn on a shared resource, and each is rewarded for the sections their burn completes.
- **Visible to everyone:** the burn actor replicates, so both players see the embers and the warm glow on the burning resource.
- **Without Brand:** nothing changes. The tunings default to 0, and a tool without Brand lights nothing.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
  - **Failed attempt:** the first build failed because `RpgHarvestBurn.cpp` lacked the player state include, so the resolved beneficiary did not convert to `AActor`.
- **New tests:**
  - `SurvivalRpg.Runes.Tunings.SocketedRuneTunesItsTool`:
    - A socketed rank tunes the abilities of its tool and never those of another tool.
    - Rank II replaces rank I's tunings, and an ability-specific tuning applies only to that ability.
    - An unsocketed rune, and a spec without source equipment, keep the base value.
  - `SurvivalRpg.Harvesting.Work.BurnWorksTheResourceForItsIgniter`:
    - No burn without work or time, and one burn per player and resource.
    - 30 units per second, as secondary work of the igniting action.
    - The fourth second completes a section, which rewards the igniting player with yield and XP once, and the burn then goes out.
    - A burn that empties its resource goes out with it.
  - `SurvivalRpg.Harvesting.Work.BurnsOfTwoPlayersShareOneResource`:
    - Two players burn one node side by side.
    - Together they complete exactly three sections, with the yield and XP of three; each is rewarded.
  - `SurvivalRpg.Harvesting.Content.RuneSocketContract` now also checks:
    - Every Brand rank burns, and no rank burns weaker than the one below.
    - Axe and pickaxe swings light a burn that names Brand.
- **Failed attempt in the tests:** the burn ticks first came one second late. Stepping a test world's timers exactly onto a deadline fires the timer one step later, as [GAS and network test pitfalls](../.agents/skills/unreal-lyra-expert/references/gas-and-network-test-pitfalls.md#timers-in-test-worlds) already describes; the burn tests step 1.01 s.
- `ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage --null-rhi`: **453 of 453 passed**. The 16 with warnings are the known RequestId collisions, UI asset logs and PIE locomotion messages.
- `ue.py test SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.InventoryReorderPIE`: **13 of 13 passed**, with only `LogNetPackageMap` warnings.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players.
  - **Setup for the check only, reverted and not committed:** three iron veins in front of the player starts, so that a straight swing reaches one without mouse aim, and the starter also gave the pickaxe.
  - **Host:**
    - Socketed Brand into the pickaxe in Weapon I and struck the vein. The server spawned `BP_HarvestBurn_Brand`.
    - The struck vein glowed orange with rising embers.
    - With the strikes, the burn emptied the vein, and the burn went out.
  - **Client:**
    - Moved its pickaxe into Weapon I, socketed its own Brand and struck its vein.
    - The server lit a new burn for that hit.
    - The client saw the glow and embers on its vein and received the ore of its own hit.

## Playtest

1. **Socket Brand:** in `Lvl_HarvestPickaxe`, pick up the pickaxe or the axe, put it into Weapon I, and drag the Brand rune into one of the sockets below it. The rune's tooltip describes the burn.
2. **Burn:** strike an iron vein or a dead pine. The struck resource glows warm and sheds embers for about 4 s, and its stock goes down a little further after the hit. Ore or wood from completed sections lands in your inventory.
3. **Without Brand:** take the rune out and strike again. Nothing burns.
4. **Multiplayer:** in a two-player listen server, the client sockets its own Brand and strikes a resource. Both players see that resource burn, and the client gets the rewards of the sections its burn completes.

## Limits

- **Placeholder look:** the burn shows the ember template and a warm point light, no flames, and it vanishes at once when it goes out. A proper burn effect belongs to the VFX roadmap.
- **Weak rank I:** at rank I a burn adds 40 % of one hit over 4 s, so it often completes no section by itself. The values are tunings on `ID_Rune_Brand`.
- **Refresh and discharge:** a further hit keeps the running burn unchanged. Anhaltende Macht (longer, refreshing burns) and Entladung (release at once) come with MST-06c.
- **Area harvests:** only the swings' direct hits light burns. Stride pulses, chains and the swarm never do.
- **No network test for the burn:** the replicated burn was checked in the agent's PIE session. The automated network tests cover the unchanged harvesting paths.
- **Dead pines not seen by hand:** the native tests and the PIE check burn actor nodes (iron veins). Dead pines are PCG instances; their burn uses the same commit through the instance stock, but no test or PIE session struck one.

## Next

MST-06c:
- the source-tool activation evaluator for talents;
- Anhaltende Macht and Entladung in the Nachwirkung branch;
- the learned-but-inactive state on the skill screen;
- the rune bar with highlights.
