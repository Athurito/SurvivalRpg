# RNG-03b: Arrow break roll, embedded world arrows and recovery

Task report for RNG-03b in the [ranged combat roadmap](ranged-combat-roadmap.md), [PR #368](https://github.com/Athurito/SurvivalRpg/pull/368). An intact standard arrow now stays in the world: the server rolls once at a world impact or a block whether it broke; an intact arrow embeds in the hit surface, or drops to the ground near a shield or a surface it cannot embed in. Players collect it with the ordinary Collect interaction. Characters never keep arrows. Arrows embedded in AI characters, corpse recovery and the NPC recovery budget follow in RNG-03c.

## C++ boundary decision

- **Classification:** reusable native mechanism (the exactly-once break roll and recovery, pickup placement, grouping, the world cap, expiry and nonpersistence), native schema (`FRpgProjectileRecoveryDefinition`, `FRpgProjectileSurfaceRecoveryRule`, the recovery fields of `FRpgProjectileLaunchParams`) and designer content (the arrow pickup Blueprint and the arrow's recovery tuning).
- **Runtime truth:**
  - **Impact:** the server's projectile resolves its impact once (RNG-01) and, inside that resolution, the break roll and the recovery once. Nothing later rolls again.
  - **Quantity:** the pickup's replicated loot inventory (the `ARpgDroppedInventoryActor` foundation) is the only owner of recovered arrows. Collection, partial collection and concurrent looters go through its atomic transfer path.
  - **Order:** `URpgEmbeddedProjectileSubsystem` only knows which pickups exist and which host they belong to; it owns no inventory, recovery or combat state.
- **Existing seams:** `ARpgCombatProjectile` (`ImpactRecovery`, `ResolveImpact`); `FRpgProjectileLaunchParams`; `RpgProjectileShot::ResolveTuning` and `CommitShot`; `ARpgDroppedInventoryActor` with its Collect interaction and `CollectRootItemsBatch`; `URpgInventoryContainerComponent` (transfer policy, crafting access, world-save registration); the tuning tag `Ability.Tuning.Ranged.BreakChance` reserved in RNG-02.
- **Native, and why:** the roll, the placement and the recovery are server authority inside the projectile's impact; the pickup's container configuration must be set before its `BeginPlay` decides world-save registration; grouping and the cap must stay exactly-once in quantity. Blueprint cannot run inside the projectile's impact resolution or configure the native container subobject in time.
- **New native classes:** two, as planned in the roadmap: `ARpgEmbeddedProjectilePickup` and `URpgEmbeddedProjectileSubsystem`.
- **Content:** `BP_EmbeddedPickup_Arrow` (mesh and transform only) and the recovery data on `BP_Projectile_Arrow`. No native content leaf.
- **Asset work:** created and configured through Unreal MCP (`BlueprintTools.create`, `ObjectTools.set_properties`, `compile_blueprint`, `save_assets`); validated by the asset contract in `ProjectileShot.BowContentShootsTheStandardArrow`.

## Behavior

### Problem

Every arrow was gone after its flight (RNG-02). The roadmap requires intact arrows to stay visibly in the world and be recoverable, bounded so long fights cannot flood the world, without arrows on players and without persisting temporary arrows.

### Change

- **Policies:** a `Recoverable` projectile launched with a `RecoveryItemDefinition` returns its ammunition; `ConsumedOnImpact` projectiles (magic arrows, spells) never roll and leave nothing. Only the native shot commit sets the recovery item, when it consumed an arrow from the shooter's inventory; the field is not exposed to Blueprint, and `Unlimited` shots return nothing, so no launch creates ammunition.
- **Break roll:** once per impact on the server. The chance is the arrow class's `BaseBreakChance` (authored 0.3 on the standard arrow; the native default 0 never breaks), tuned once per activation by `Ability.Tuning.Ranged.BreakChance` and carried on the launch, then scaled by the hit surface's rule (`SurfaceRules`: physical material, chance factor, whether it can embed). A broken arrow grants nothing.
- **Embedded:** an intact arrow at a world impact becomes an `ARpgEmbeddedProjectilePickup` at the contact along its flight, `EmbedDepth` (4 cm) inside the surface. On a movable host it attaches to the hit component (and bone), so it follows the host; the engine's attachment replication rebuilds that on every client and for late joiners. It embeds only in hosts every client can resolve (a replicated or stably named actor and component) and never in another collectable such as a loot bag or another pickup. If the host goes, the pickup stays at its last world transform until its own deadline.
- **Dropped:** an intact arrow stopped by a shield (block or perfect block), on a surface whose rule forbids embedding, or on a host it may not embed in, falls from 30 cm in front of and above the contact onto the ground below (up to `MaxDropHeight`, 5 m) and lies there. The fall uses the projectile channel, so triggers, interaction shapes and pickups do not catch it, and it passes characters and collectables. It never stays on the blocker; a floor it could not embed in is its own ground. Without ground in reach it is lost. Lying on a moving ground that no client can resolve, it stays where it fell instead of following that ground on the server alone; since the [validation fixes](ranged-acceptance.md#validation-fixes-2026-11-03) the drop uses the embed's check (F36).
- **Player-hit rule:** a projectile that stops on a character (hit, ally hit), or on any pawn such as an NPC corpse whose ability system is already gone, keeps nothing: no attachment, pickup or recovery credit. RNG-03c adds recovery on AI characters through their corpses.
- **Manual drops:** a player's manual drop no longer merges into nearby loot that refuses deposits; beside recovered arrows it gets its own pile (before this fix, such a drop failed).
- **Flight presentation:** when the flight stops, the projectile now hides its meshes unless it stopped in the world with nothing to recover. Arrows no longer float at a hit character or in mid-air while the projectile lingers, and the pickup takes over an intact arrow without a double mesh. FX and audio still fade as in RNG-01.
- **Grouping:** a hit component holds at most `MaxSeparatePickupsPerHost` (10) separate pickups. A further intact arrow then joins the nearest pickup of the same ammunition on that host within `GroupRadius` (1 m), raising its stack without a new actor; the group keeps the later of both deadlines. Without a group in range it gets its own pickup.
- **World cap:** at most `MaxWorldArrowPickups` (100, project config on the subsystem) pickups exist per world. A new pickup beyond it expires the pickup closest to its deadline early, exactly like its normal expiry.
- **Collection:** the pickup is an `ARpgDroppedInventoryActor`: the Collect interaction moves what fits through `CollectRootItemsBatch`, a full inventory leaves the rest in the pickup (and opens its loot window), and the emptied pickup goes. The prompt reads "Collect" and the arrow's item name with the group's quantity, such as "Arrow (3)". Until the [validation fixes](ranged-acceptance.md#validation-fixes-2026-11-03) the HUD showed only "Collect": `CUI_InteractionPrompt` had no target text (V1).
- **Lifetime and persistence:** every pickup expires at its recovery deadline (`RecoveryLifetimeSeconds`, 10 minutes) with what is left. Its container is withdraw-only and closed to crafting, so it never registers for world saves or base crafting scans. Collected arrows are ordinary inventory items and persist with the player.

### Content

| Asset | Content |
| --- | --- |
| `GF_Combat_Ranged/Projectiles/BP_EmbeddedPickup_Arrow` | New, parent `ARpgEmbeddedProjectilePickup`. `DisplayMesh` shows `SM_Skeleton_Archer_Arrow` with the same relative transform as the projectile (-62 cm X, pitch -90), so its tip lies at the actor origin and the shaft along -X (measured bounds X -62.5 to 0.04 cm). Query-only collision for interaction focus. |
| `GF_Combat_Ranged/Projectiles/BP_Projectile_Arrow` | `Recovery`: pickup class `BP_EmbeddedPickup_Arrow` and break chance 0.3 saved in the asset; lifetime 600 s, 10 pickups per host, 100 cm group radius and 500 cm drop height equal the native defaults, which are the roadmap's starter values. No surface rules: the project has no surface physical materials yet. |

## Validation (2026-11-02)

- **Review:** two review agents worked on separate lenses: (1) authority, replication, lifecycle and exactly-once; (2) contract coverage, architecture boundary and tests. I checked each finding against the code and engine source. Confirmed and fixed (commit `86bd64a7`):
  - A manual drop near recovered arrows failed: the merge picked the withdraw-only pickup and spawned no fallback pile.
  - An NPC corpse, whose ability system is uninitialized on unpossess, counted as world geometry, so arrows embedded in the ragdoll and outlived the corpse.
  - The drop trace queried object types, so it landed on triggers and other pickups. It also ignored the hit surface, so an intact arrow on a non-embeddable floor was lost, and it started at contact height below sloped ground.
  - Arrows could embed in loot bags and other collectables and float once those were collected. Moving hosts that clients cannot resolve made clients diverge.
  - `RecoveryItemDefinition` was Blueprint-writable, so a Blueprint launch could create ammunition.
  - The 0.3 break chance lived only in the native default.
  - Weak tests: a vacuous projectile check in the PIE test, no second looter there, and no tests for mesh hiding, the no-ground loss, a trigger volume or a pawn without an ability system.

  Kept as limits: cleanup on feature deactivation, the cap order, and the untested paths listed under Limits.
- **Build:** `python Build/Tools/Unreal/ue.py build` passed (0 errors, 0 warnings).
- **Native tests** (`--null-rhi`): `SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.Ranged SurvivalRpg.Inventory.Drop` passed 42 / 42 after the fixes, including:
  - **Recovery tests:** `Projectile.Recovery.WorldImpactEmbedsOnce`, `BreakRollAndSurfaces`, `CharactersKeepNothing`, `ConsumedOrUnpaidLeavesNothing`, `GroupingAndWorldCap`, `CollectExpireAndLoseHost`.
  - **Manual drop:** the new `Inventory.Drop.SkipsWithdrawOnlyLootNearby`.
  - **Tuning and policy:** extended `ProjectileShot.TuningIsFixedPerActivation` (break chance tuned per activation) and `UnlimitedAmmunitionIsAnExplicitPolicy` (recovery item only for consumed arrows).
  - **Asset contract:** `BowContentShootsTheStandardArrow` checks that the arrow is recoverable, has a content pickup, authors a break chance and returns the consumed arrow.
- **Native regression:** `SurvivalRpg.Combat SurvivalRpg.AbilitySystem SurvivalRpg.Inventory SurvivalRpg.Crafting SurvivalRpg.Equipment SurvivalRpg.Harvesting SurvivalRpg.Input SurvivalRpg.Interaction SurvivalRpg.Save SurvivalRpg.Base` passed 513 / 513. The same 16 tests warned as in the run before the fixes (GASP, Mover and inventory UI baseline).
- **PIE network tests** (listen server, 2 clients, 60 ms lag): `CombatProjectilePIE` passed 6 / 6, including the new `IntactArrowEmbedsEverywhereFollowsItsWallAndIsCollectedOnce`:
  - every client's projectile reports the arrow embedded;
  - the pickup is attached to the replicated wall on server and clients and follows it when it moves;
  - one remote player collects the arrow, and the other gets nothing in the same frame;
  - the pickup disappears everywhere, and the collector's own client sees the arrow in its inventory.
- **PIE regression:** `RangedAimPIE CombatRemoteMeleePIE CombatImpactCuesPIE KillXPCuePIE` passed 9 / 9 with baseline NetGUID warnings only.
- **Editor check:** a spawned `BP_EmbeddedPickup_Arrow` measured the mesh from X -62.5 to 0.04 cm around its origin, with query-only collision.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

## Playtest

1. Open `Lvl_RpgGaspCMC` and start PIE in standalone.
2. In the console, run:
   - `Rpg.Inventory.GrantItem /Game/SurvivalRpg/Inventory/Items/Test/ID_TestBackpack.ID_TestBackpack_C 1 equip`
   - `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C 1 equip`
   - `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C 20`
3. Shoot ten arrows into a wall or the ground. Expected: about seven stay in the surface (break chance 30 %); broken ones vanish at the impact.
4. Walk to an embedded arrow. Expected: the prompt "F Collect Arrow (1)"; collecting adds one arrow to the inventory and removes the shaft.
5. Shoot more than ten arrows into the same wall section within 1 m. Expected: after ten shafts, further arrows join the nearest one; its prompt shows the larger count.
6. Multiplayer: start PIE as a listen server with two players, give both the bow and arrows, and let one shoot into a wall. Expected: both see the same embedded arrows; only one player can collect each, and the count in the other player's prompt drops accordingly.

## Limits

- **AI characters and corpses:** no arrow stays in an NPC or its corpse yet; corpse recovery, post-death hits and the NPC recovery budget are RNG-03c.
- **Feature deactivation:** there is no explicit cleanup when `GF_Combat_Ranged` deactivates; pickups end with their world or at their deadline.
- **Cap order:** the world cap expires the pickup closest to its deadline. While every arrow has the same lifetime that is the oldest pickup; a group that a later arrow joined counts as newer.
- **Surfaces:** no surface rules are authored, because the project has no surface physical materials yet; every surface embeds with the arrow's base chance.
- **Hidden flight meshes:** a stopped projectile now hides its meshes unless it stopped in the world with nothing to recover. This changes the RNG-01 stop presentation described in [ranged-projectile-foundation.md](ranged-projectile-foundation.md); FX and audio still fade as there.
- **Not tested directly:**
  - attachment reconstruction for a late joiner (the PIE test's clients join before the shot; the engine's attachment replication covers late joiners);
  - host loss and expiry seen on clients;
  - collection through the interaction ability's range and validation (the tests call the same `CollectRootItemsBatch` the Collect ability uses);
  - a world-save round trip; nonpersistence is proven through the container configuration instead.
- **No manual PIE:** this task has no manual PIE session with screenshots. The pickup's geometry was measured in the editor, and the network test proves the embedding on both clients. The visual check of embedded arrows belongs to RNG-06.
- **Loot window:** a partial collection opens the pickup's loot window with the remainder, as for every dropped inventory.
- **Tuning:** all values are starter tuning for RNG-06.
