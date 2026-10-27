# MST-03a: Quantized harvest work

Task report for MST-03a in the [mastery map roadmap](mastery-map-roadmap.md); design and acceptance contracts are in the [implementation plan](mastery-map-implementation-plan.md), section "Harvest work before percentage-based gameplay".

MST-03a gives the harvesting stock a work model, so that later talents and runes can add a fraction of a section without changing the yield. No content uses fractional work yet; MST-03b adds the first source, Spaltkraft in the shared harvesting tree.

## Baseline

- **Base:** `master` at `a888a078` (MST-02 merged as [PR #304](https://github.com/Athurito/SurvivalRpg/pull/304)), branch `claude/mst-03a-harvest-work`. No editor ran during the native work.
- **Stock before MST-03a:** `FRpgHarvestStockRules::Evaluate` took whole `RequestedSections` plus the weak-point bonus sections, clamped to the remaining sections. Every extracted section rolled the loot table and awarded XP once. `HarvestPower` scales the yield of loot rows and stays separate from work.
- **Representations:** the three stock owners are `URpgHarvestableComponent` (actor nodes), `URpgHarvestInstanceStockComponent` with `URpgHarvestableInstancesComponent` (PCG instances, stock shards on the GameState), and the legacy `URpgHarvestableInstancedMeshComponent` (bushes and sandbox, one section, session only). `URpgHarvestPersistenceComponent` saves the first two.
- **Request builders:** the harvest ability, its chains (root groves), the grave swarm, and the manual harvest of legacy instances.

## C++ boundary decision

- **Classification:** an extension of the existing native harvesting mechanism: stock rules, the request and result schema, stock replication and the saved stock. No new class, component or manager.
- **Runtime truth:** unchanged owners. Actor nodes keep their stock in `FRpgHarvestNodeState`, instances in their stock shard entry, legacy instances in their FastArray entry. Each one now also stores the residual work of its next section.
- **Native:** quantization, evaluation, the commit order of reward, stock and XP, replication, persistence and provenance. These are authority and persistence invariants that every representation must share.
- **Designer-owned, later:** how much work a talent, rune or tool adds (`WorkScale`) and which effects react to own hits. MST-03a adds no content and no tuning.

## Behavior

### Problem

Stock could only be extracted in whole sections. A talent adding a quarter section, a burn tick or an echo fraction had no way to work a resource without either changing its yield or rounding the effect away. Nothing recorded whether work came from the harvester's own hit or from follow-up work, or how much of a hit was left over.

### Change

- **Work units:** work is counted in hundredths of a section (`RpgHarvestWork::UnitsPerSection` = 100).
  - A request brings `RequestedSections × WorkScale` sections of work, rounded to whole units and capped at 64 sections.
  - `WorkScale` defaults to 1, must lie above 0 and at most 16, and is designer-facing for later talents and runes. A request whose work rounds to zero is invalid.
- **Residual progress:** work that does not complete a section stays on the resource as `SectionWork` (0 to 99).
  - A section completes once the work on it reaches 100 units. It is then rewarded and awards XP exactly once, as before.
  - A hit that completes no section is still a successful harvest, with zero sections, an empty delivery and no XP.
- **Results:** `FRpgHarvestResult` reports:
  - `WorkApplied`: the work the resource accepted.
  - `OverkillWork`: the work beyond what the resource had left. It is positive only on the depleting hit.
  - `RemainingWork` and `SectionWork`: the work left after the result, and the residual progress on the next section.
- **Weak points:** a weak-point hit adds its bonus sections as work from the same stock, as before.
- **Finishing:** `bFinishResource` applies exactly the remaining work and reports no overkill. A chain box now finishes its other trees with it instead of requesting 16 sections, so a felled grove never manufactures overkill.
- **Provenance:** every request carries `FRpgHarvestProvenance`, and the result copies it.
  - **Source:** `Direct` for the harvester's own action, or `Secondary` for follow-up work.
  - **Root action:** `RootActionId` comes from `FRpgHarvestProvenance::MakeRootActionId`.
  - **Proc:** an optional `ProcTag` names the effect behind secondary work.
  - **Own hits:** `IsAcceptedDirectHit` marks accepted direct work, also without a completed section. Later Echo-like effects use it, but it grants nothing for an unearned section.
- **Who sets provenance:**
  - The harvest ability stamps one root id per execution, shared by its targets, chains, swarm and stride pulses.
  - Chains and the swarm turn the request into secondary work of that root.
  - The manual harvest of legacy instances stamps its own root id.
  - Previews have no root id.
- **Commit order:** a commit evaluates again against the stock as it is at that moment on the server.
  - With a completed section, it delivers the reward first. A failed delivery leaves sections and residual work untouched, as before.
  - It then writes sections and residual work together, and awards XP for the completed sections.
- **Replication:**
  - Actor nodes replicate `SectionWork` in `FRpgHarvestNodeState`.
  - Instances replicate it in their stock shard entry. Partial work creates the sparse entry of an untouched instance.
  - Legacy instances replicate it in their FastArray entry and stay active while partly worked.
  - Clients therefore preview the same crossing the server commits, and late joiners receive it with the rest of the stock.
- **Saves:** `FRpgHarvestSavedStock::SectionWork` saves the residual work of actor nodes and instances. Residual work alone makes a resource non-pristine, so it is recorded. The harvest save schema stays 1. The new field is a tagged `SaveGame` property, so older dev saves load it as 0; no migration is needed, per the user's standing preference.
- **Interface change:** `URpgHarvestInstanceStockComponent::ExtractSections` became `CommitWork`, with the residual work as a parameter. The new name breaks every old caller at compile time, so no float argument can slide into the new integer parameter.

### Decision against the plan: partial work keeps the revision

The plan says positive partial work should advance the revision "so concurrent/stale requests cannot reuse it". MST-03a keeps the revision as the depletion and respawn boundary instead:

- **Same safety:** every commit already evaluates against the current stock on the server's game thread. Two concurrent requests both commit and their work adds up; neither can reuse work the other consumed. The tests cover this for actor nodes, and the PIE test for two hits on one node.
- **Gameplay cost:** a revision change on every partial hit would make each co-op partner's swing, a swarm's planned assignments and a manual-harvest option stale whenever anyone else worked the same resource.
- **Current behavior:** partial whole sections never advanced the revision either.

### Cost

- **Memory:** the field fits existing padding. Measured after the change, a stock shard entry is 40 bytes and `FRpgHarvestNodeState` 12 bytes; by their member layout, both sizes are unchanged.
- **Network:** one byte per changed entry or node update. Untouched instances still cost nothing; a merely worked instance costs one sparse entry, as a harvested section did before.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Harvesting SurvivalRpg.Interaction.Harvesting SurvivalRpg.Save.WorldSave --null-rhi`: **86 of 86 passed**, without warnings.
- New tests:
  - `SurvivalRpg.Harvesting.Work.StockRulesQuantizeWorkAndOverkill` covers:
    - quantization and invalid work scales;
    - partial work, residual crossings and several sections at once;
    - true overkill, and finishing without overkill;
    - weak-point work;
    - provenance and unique root ids;
    - unchanged rejections.
  - `...Work.NodeResidualKeepsTotalYield` covers:
    - concurrent requests on one stock;
    - fourteen 0.3-section swings emptying four sections with the defined yield and XP once per section;
    - the overkill of the last swing, and a revision that only depletion advances;
    - restoring residual work.
  - `...Work.FailedDeliveryKeepsStockAndWork`: a failed delivery keeps sections and residual work, and the retry completes from the kept work.
  - `...Work.InstancesMatchNodesAndSaveResidualWork`: the same swings give identical results on an instance and an actor node. Residual work alone exports, restores and continues.
  - `...Work.ResidualWorkSurvivesReload`: the persistence saves residual work alone, restores it into a reloaded map, and a later request continues from it.
  - `SurvivalRpg.Interaction.Harvesting.HISM.ResidualWorkKeepsInstanceActive`: legacy instances keep residual work per instance, stay active and keep their revision until the completing swing.
- Extended tests:
  - `SurvivalRpg.Harvesting.Ability.CommitOnceAndCancelYieldsNothing`: each ability execution commits as a direct hit with its own root id.
  - `SurvivalRpg.Harvesting.Chain.ChainFollowsAreaHarvestRules`: chained trees are secondary work of the trigger's root and report no overkill.
  - The network PIE tests `ActorNodeSectionsReplicateAndLateJoin` and `InstanceStockReplicatesThroughGameStateAndLateJoins`: residual work reaches the first client and a late joiner. For the actor node, the depleting hit reports its overkill and both clients see the residual work cleared.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.LootHarvestPIE`: **8 of 8 passed**. It covers actor nodes, instance stock, the swarm, the stride and co-op load, each with a late joiner. Its warnings are the usual net GUID notes of the temporary PIE map.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.HarvestCuePIE`: **2 of 2 passed**. A predicted harvest commit still reaches the harvester and its peers, and target cues reach every machine.
- **Own PIE check:** not run interactively. In `Lvl_HarvestPickaxe` the player must pick up and equip the pickaxe and aim at a vein. The available editor tools cannot drive that reliably without the scripted Python tool that was refused in MST-02. The editor was started for it and closed again without changes. The network PIE tests above run the real harvest abilities with clients and late joiners instead.

## Playtest

No content uses fractional work yet, so there is nothing new to see. Harvesting should behave exactly as before:

1. In `Lvl_HarvestPickaxe`, mine the iron vein with the pickaxe. Every swing takes one section with its usual yield, and weak-point hits still take more.
2. Fell trees with the axe and Death Wave. Root groves still fall together, and the grave swarm still clears its targets.
3. **Multiplayer:** in a listen-server session with two players, both mine the same vein. Its stock goes down for both, and the rewards reach whoever struck.

## Limits

- **No work source yet:** nothing sets `WorkScale` below or above 1 in content. Spaltkraft (MST-03b) is the first. Abilities need a tuning seam for it then; MST-03a adds none.
- **No presentation of partial progress:** a partly worked section looks the same as an untouched one. Presentation events fire for partial work with unchanged remaining sections.
- **Provenance consumers:** nothing reads provenance yet. Proc limits, chain budgets and the Echo trigger follow in MST-09a and later. The swarm's secondary provenance is set in code but no test asserts it.
- **Threshold talents:** a finishing talent such as Zertruemmern can use `bFinishResource`, but none exists yet.
- **Corpses:** skinning corpses keep their own reservation flow outside the section stock and have no work model.
- **Legacy instances** keep residual work only for the session, like the rest of their state.
- **No manual PIE session:** harvesting in the real test map with two players is left to the user's batch playtest. That playtest has no new behavior to see; it checks for regressions.

## Next

MST-03b: the shared harvesting mastery and tree with the first functional Wucht nodes, starting with Spaltkraft as the first source of fractional work.
