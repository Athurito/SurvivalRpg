# RNG-06: Integrated acceptance of the first bow

Task report for RNG-06 in the [ranged combat roadmap](ranged-combat-roadmap.md), [PR #372](https://github.com/Athurito/SurvivalRpg/pull/372).

This task closes the first bow delivery. It holds:
- the acceptance matrix for every contract the roadmap lists, with the evidence for each;
- the checks this task added for gaps the earlier tasks left;
- the cook and the full regression;
- the tuning sheet;
- one consolidated playtest for the user's check of RNG-01 to RNG-06.

No gameplay value changed: every number is still the roadmap's starter value, and the user's feel check decides the final ones.

## C++ boundary decision

- **Classification:** validation only. Tests, a cook and documentation; no runtime mechanism or content changes, apart from one test-access `friend` in `ARpgGameModeBase`, as its other world-save tests have.
- **New native classes:** none. The new PIE steps reuse the existing fixtures (`ARpgNetworkAutomationArrowBody`, `ARpgNetworkAutomationRecoveryWall`, `ARpgCombatNetworkFloorFixture`).

## Acceptance matrix

### Focused contracts

| Roadmap contract | Evidence | Status |
| --- | --- | --- |
| Nocking cancellation, partial and full draw, repeated shots, recovery timing, every interruption route | `ProjectileShot.EarlyReleaseCancelsWithoutCost`, `DrawCurvesSetDamageAndSpeed`, `CancelledDrawIgnoresALateRelease`, `HoldDrainExhaustionLowersTheBow`, `PresentationTagsFollowTheShot` (the release state is set at the release), `Ranged.Aim.InterruptionsCancelAndNeedFreshInput`, `Ranged.Aim.StaggerRemovalDeathAndAvatarChangeEndTheStance`; PIE `RangedAimPIE.DodgeInterruptsTheDrawAndNeedsAFreshPress` | Automated, except the release presentation's 0.5 s duration and its reset by a new draw |
| Aim ownership of primary and secondary input, camera cleanup, fresh-input suppression, passives across activations, avatar and equipment rebinding | `Ranged.Aim.TokensHoldTheCombinedStance` (the camera follows the remaining owner), `Ranged.Aim.InterruptionsCancelAndNeedFreshInput` (a cancelled hold needs a fresh press), `Ranged.Aim.PressDuringABlockingStateWaits` (a press during a dodge waits), `Ranged.Aim.MovementTuningIsFixedPerActivation`, `ProjectileShot.TuningIsFixedPerActivation`, `UI.RangedCombat.*` (HUD rebinding) | Automated |
| Last arrow, ammunition removed while drawing, insufficient stamina, spawn failure, duplicate release, no partial commits | `ProjectileShot.AmmunitionIsCheckedAtDrawAndRelease`, `InsufficientStaminaPaysNothing`, `SpawnFailureRefundsEverything`, `DuplicateReleaseShootsOnce`, `ArrowsInACarriedBagShoot`; PIE `CombatProjectilePIE.ClientShotLaunchesOneArrowAndPaysOnce`, `ServerTimesOutAHeldShotWithoutCost` | Automated |
| Impact on world and characters, shield block, cheaper Perfect Block without reflection or remote stagger, dodge immunity | `Projectile.WorldImpact`, `ImpactAppliesOnce`, `ShieldBlock` (a perfect block takes no damage and never staggers the distant shooter), `DirectionalBlock` (block outcomes by direction), `FlightSweep` (a dodging target lets the projectile through) | Automated, except the perfect block's lower stamina cost: the fixture's block costs are 0 |
| Fireball carrier contract: flight system and loop everywhere, stop and fade, one authoritative impact cue | PIE `CombatProjectilePIE.ReplicatedFlightHitsOnceWithOneImpactCuePerMachine` (every client sees the flight, the stop holds, one impact cue per machine, the projectile goes), `PredictedAbilityLaunchesWithoutEquipment`, `PointBlankAndOrphanedProjectilesStillHitOnce` | Replication, stop and cues automated; the flight effect, its loop and the fade were seen only in RNG-01's manual fireball run |
| Player hits leave nothing (co-op hits, downed players, death, respawn) | `Projectile.Recovery.CharactersKeepNothing`: players have no host, so every hit on a player keeps nothing, whatever their state | Automated by construction; co-op, downed and respawn not run separately |
| Consumed-on-impact ammunition: no break roll, embedding, pickup, credit or NPC token | `Projectile.Recovery.ConsumedOrUnpaidLeavesNothing`, `ProjectileShot.UnlimitedAmmunitionIsAnExplicitPolicy` | Automated |
| Grouping on AI characters and world hosts, full and partial collection, merged deadline, world cap, no lost or duplicated quantity | `Projectile.Recovery.GroupingAndWorldCap`, `CollectExpireAndLoseHost`, `AICharacterGroupsBeyondVisibleMaximum`, `AICharacterKeepsArrowsAsCorpseLoot` | Automated |
| Launch state after weapon switch, shooter death, avatar replacement | `Projectile.LaunchState`; PIE `PointBlankAndOrphanedProjectilesStillHitOnce` | Automated |
| One break roll, fixed retry outcome, lifetime expiry, missing hosts, **late-join attachment reconstruction** | `Projectile.Recovery.BreakRollAndSurfaces`, `WorldImpactEmbedsOnce`, `CollectExpireAndLoseHost`; **new:** PIE late joiner sees the arrow embedded in its copy of the moved wall | Automated |
| Embedded arrows **following live animation and local ragdolls**, the killing arrow, corpse hits | `AICharacterKeepsArrowsAsCorpseLoot` (record before the damage), `CorpseTakesArrowsUntilItCompletes` (hits on a dying body and a corpse); **new:** PIE late joiner rebuilds both shafts on its skeleton, and each machine's shaft stays on its bone while a test-forced physics simulation of that machine's fixture body falls | Attachment and a forced ragdoll automated; the AI corpse ragdoll and live animation were not observed in a running game, which the user's playtest covers |
| Full inventory, two concurrent looters, partial stacks, rearrangement, expiry during transfer, failed death-loot retries | `CollectExpireAndLoseHost` (a second looter gets the remainder of a group), `AICharacterKeepsArrowsAsCorpseLoot` (the loot table's own arrows taken first); PIE `IntactArrowEmbedsEverywhere...` (two looters of a world pickup in one frame) | Partial: a full inventory, rearrangement inside a corpse, expiry during a transfer, two looters of a corpse and a failed death-loot retry with queued arrows are not tested |
| NPC fire beyond the recovery budget: misses, breaks, shooter death, AI and loadout refreshes, no refill | `ProjectileShot.UnlimitedAmmunitionIsAnExplicitPolicy` (budget, a new life, a failed release, a bigger budget never raises it); PIE `RangedEnemyPIE.BowEnemyShootsThePlayerThroughTheSharedBowShot` (five tokens, then another hit without a sixth) | Partial: tokens are spent at the release, so misses and breaks follow by construction; an arrow in flight when its shooter dies, target changes, disengagement and a real loadout re-equip are not exercised |
| Normal persistence of collected arrows; **no temporary arrow state in world saves** | **new:** `Projectile.Recovery.PickupsStayOutOfWorldSaves` (the world-save capture takes the chest and no pickup); collected arrows are ordinary inventory items (`SurvivalRpg.Save.*`) | Automated |
| No flight-path tube, line, arc, guide, impact marker or ballistic crosshair correction | No such component or widget exists. `UI.RangedCombat.HudAssets` checks the HUD widgets, and the RNG-04 captures showed only the crosshair and draw bar. | Code and asset contract; user's visual check |

### Integration and playtest

| Roadmap item | Evidence | Status |
| --- | --- | --- |
| One player's and one enemy's bow use the same mechanism, without a player controller for the AI | `Ranged.EnemyBowAssetContract` (identical ability sets); PIE `RangedEnemyPIE` (a real `BP_RiftGrunt_Bow` shoots through `GA_BowShot`) | Automated |
| Aim, draw, release, line-of-fire obstruction, **target loss or death during the draw**, spacing, retreat and repositioning | PIE `RangedAimPIE`, `RangedEnemyPIE` (a wall blocks the shot). **New:** `RangedEnemyPIE.DeathDuringTheDrawShootsNothing`: an archer dying mid-draw launches nothing, and a target's death mid-draw lowers that draw unreleased. RNG-05's sandbox PIE saw the approach, circling and the retreat under close pressure. | Death automated; losing the line of fire or the target mid-draw and repositioning around cover are not tested |
| Listen server with two clients, latency, **late join**, simultaneous shots, point-blank impacts; remote draw, release, aim posture, embedded arrows, **ragdoll attachments** | Every PIE test runs a listen server, two clients and 60 ms lag. `PointBlankAndOrphanedProjectilesStillHitOnce`, `RangedAimPIE.RemoteAimDrawAndReleaseShowEverywhereAndCapMovement`, plus the new late-join steps (embedded pickup, shafts) and ragdoll steps above. | Partial: simultaneous ranged shots and a late joiner seeing a drawing player's posture are not tested; posture quality is the user's check |
| Regression: melee combo, block, dodge, equipment, inventory, attribution, GASP montages and layers, death and corpse, Mover foundations | See [Validation](#validation-2026-11-02) | Automated |
| Compile affected assets, parentage, skeletons, dependency closure; cook the test map | Asset contracts (`BowAssetContract`, `ArrowAssetContract`, `EnemyBowAssetContract`, `UI.RangedCombat.HudAssets`); cook of `Lvl_CombatSandbox` | Automated |
| Reproduction steps for every observable task | [Playtest](#playtest-the-users-check) below | Done |

## Tuning sheet

All values are the roadmap's starter values or the earlier tasks' starting points, unchanged by this task. The feel check below decides them.

| Value | Now | Authored in | Source |
| --- | --- | --- | --- |
| Draw: nock / full draw | 0.3 s / 1.0 s | `BP_WeaponInstance_Bow` | RNG-02 starter |
| Damage by draw | 8 to 30 | `CF_Bow_DamageByDraw` | RNG-02 starter |
| Launch speed by draw | 2000 to 6000 cm/s | `CF_Bow_LaunchSpeedByDraw` | RNG-02 starter |
| Stagger damage | 15 at full draw | `BP_WeaponInstance_Bow` | RNG-02 starter |
| Release stamina / hold drain | 8 / 0 | `BP_WeaponInstance_Bow` | RNG-02 starter |
| Aim movement cap, camera | 180 cm/s, `CM_Bow_Aim` (220 cm back, 75 cm right, FOV 70) | `BP_WeaponInstance_Bow` | RNG-03 starter |
| Break chance | 0.3 | `BP_Projectile_Arrow` | Roadmap starter |
| Recovery lifetime, pickups per host, group radius, world cap | 600 s, 10, 100 cm, 100 | arrow recovery data, `URpgEmbeddedProjectileSubsystem` | Roadmap starter |
| Release presentation | 0.5 s | `ReleasePresentationSeconds` on the bow's shot definition (native default) | RNG-03 starter |
| Visible arrows per AI character | 10 | arrow recovery data | Roadmap starter |
| NPC recovery budget | 5 per life | `BP_WeaponInstance_EnemyBow` | Roadmap starter |
| NPC bow damage, speed, draw | as the player bow; no stamina | `BP_WeaponInstance_EnemyBow` (child of the player bow) | RNG-05 |
| Archer hold before release | 0.4 to 0.8 s | `ST_SkeletonArcher` | RNG-05 |
| Archer bands | shoot 8 to 13 m, approach beyond 16 m, evade inside 4.5 m | `ST_SkeletonArcher` | RNG-05 |
| Archer timings | circle 1.5 to 3 s, reposition 1 to 2 s, line-of-fire grace 1 s, shot timeout 6 s | `ST_SkeletonArcher` | RNG-05 |
| Archer awareness | finds targets within 30 m, drops them beyond 40 m | `ST_SkeletonArcher` (Combat Control) | CMB-06 values |

Four full-draw arrows down a fresh player with 100 health who stands still. From its tree, the archer shoots about every 3 to 5 s: about 1 s of draw, 0.4 to 0.8 s of hold and 1.5 to 3 s of circling.

## Validation (2026-11-02)

- **Build:** `python Build/Tools/Unreal/ue.py build` passed.
- **New tests:**
  - `SurvivalRpg.Combat.Projectile.Recovery.PickupsStayOutOfWorldSaves`:
    - a chest and the shipped `BP_EmbeddedPickup_Arrow` run their BeginPlay;
    - the chest gets a persistent id and the pickup none;
    - the real `ARpgGameModeBase::CaptureWorldContainers` takes only the chest.
  - `CombatProjectilePIE.IntactArrowEmbedsEverywhereFollowsItsWallAndIsCollectedOnce`: a late joiner, connected through CQTest's `ThenClientJoins`, shows the arrow embedded in its copy of the moved wall. The two original clients still collect it once.
  - `CombatProjectilePIE.ArrowsInABodyShowOnEveryClientsSkeletonAndLeaveWithTheirLoot`:
    - a late joiner rebuilds both shafts on its own skeleton;
    - after the loot, every machine first checks its shaft against the replicated group, from world transforms;
    - each machine then lets its own body fall as a forced ragdoll, and the shaft moves at least 50 cm with its bone and keeps its location and rotation on it.
  - `RangedEnemyPIE.DeathDuringTheDrawShootsNothing`:
    - an archer killed at 80 % of its draw launches no arrow, does no damage and releases nothing for 2 s;
    - when a second archer's target dies mid-draw, it ends that draw unreleased.
  - `RangedEnemyPIE.BowEnemyShootsThePlayerThroughTheSharedBowShot` now shares its setup with the death test.
- **Cook:** `python Build/Tools/Unreal/ue.py cook Lvl_CombatSandbox` passed in 840 s with 0 errors. Its 2 warnings are the Steam subsystem disabled by the command line and a tick note on `CUI_RespawnScreen`, both outside the bow.
- **Final regression** on `f7ea53ab`, with the 90 s network command timeout:
  - native suites with `--null-rhi`: `SurvivalRpg.AbilitySystem`, `Inventory`, `AI`, `Interaction`, `Equipment`, `Loot`, `Save`, `Input`, `UI`, `Crafting`, `Harvesting` and `Progression`, 511/511;
  - `SurvivalRpg.Combat` with rendering, 103/103;
  - `SurvivalRpg.Network`, 48/48;
  - `SurvivalRpg.GASP` with `np.ForceReconcile 0` and `t.MaxFPS 30`, 119/119 (the CMC and Mover locomotion, traversal, block and weapon-hold regression).
- **After the review hardening:**
  - `SurvivalRpg.Combat.Projectile.Recovery`, 10/10;
  - `SurvivalRpg.Network.CombatProjectilePIE` and `RangedEnemyPIE`, 9/9.
- **Review:** two review agents, one on test soundness and one on the accuracy of this matrix. Each finding was checked against the code.
  - **Fixed:**
    - the ragdoll check restated the attachment, so it now measures from world transforms against the replicated group and requires the shaft itself to move;
    - the archer death phase now kills near full draw and checks the release flag and the death;
    - the save test now uses the shipped pickup Blueprint;
    - several matrix rows overclaimed and are now marked partial with what is missing;
    - the tuning sheet lacked the group radius, the release presentation time and the archer's timings and awareness;
    - several playtest steps were wrong: grouping, damage range, shield, NPC arrows, and how the second player gets a bow.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

## Playtest: the user's check

Use `GF_Dev_Sandbox/Maps/Lvl_CombatSandbox` with PIE in standalone, and in the console:
- `Rpg.Inventory.GrantItem /Game/SurvivalRpg/Inventory/Items/Test/ID_TestBackpack.ID_TestBackpack_C 1 equip`
- `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C 1 equip`
- `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C 20`

1. **Aim and HUD (RNG-03, RNG-04):** hold the right mouse button. **Expected:**
   - the camera moves over the shoulder and you walk slower;
   - the crosshair shows;
   - the ammunition counter reads 20 next to the arrow icon.
2. **Draw and release (RNG-02 to RNG-04):** hold the left button. **Expected:** the bow draws and the strength bar fills for about a second. A tap shorter than about 0.3 s shoots nothing and costs nothing. A release shoots one arrow, costs 8 stamina and lowers the counter by one.
3. **Arrow feel:** shoot at different draws. **Expected:**
   - a full draw flies flat and fast;
   - a short draw drops sooner and hits weaker; the damage curve runs from 8 to 30, and the shortest shot leaves at the 0.3 s nock, already above the curve's start;
   - there is no flight line, arc or impact marker anywhere.
4. **Recovery in the world (RNG-03b):** shoot walls and the floor. **Expected:**
   - about seven in ten arrows stay embedded where they hit, and the rest break;
   - each can be picked up once;
   - after ten pickups in one wall or floor piece, further arrows within 1 m join the nearest one.
5. **Arrows in enemies (RNG-03c):** shoot the skeleton warrior. **Expected:**
   - intact arrows stay on the body part they hit and move with it;
   - after its death they stay in the ragdoll, and its corpse loot lists them;
   - taking them removes the shafts, oldest first.
6. **The archer (RNG-05):** walk toward the archer in the northeast. **Expected:**
   - it notices you within 30 m, runs closer, stops, draws visibly, holds briefly and shoots;
   - a sidestep during the hold dodges the arrow;
   - between shots it circles; behind cover it repositions; when you rush it, it runs back and then shoots.
7. **Archer arrows:** dodge its arrows. **Expected:** of its first five released arrows, those that miss you can embed or drop and be picked up; from the sixth on, none leave anything. Arrows that hit you never leave anything.
8. **Melee and shields:** sword combo, block, perfect block and dodge against the warrior, as before. With a sword and shield, block the archer's arrows. **Expected:** an intact blocked arrow drops to the ground, a broken one vanishes.
9. **Multiplayer:** listen server with two players. `Rpg.Inventory.GrantItem` gives items only to the host. As in RNG-02, the host drops bow and arrows from the inventory (Tab), and the client picks them up with F and equips the bow. Both players then repeat steps 2 to 7. **Expected:**
   - each sees the other player's aim pose, draw and release, and the same arrows fly, land and stick on both screens;
   - only one player can take each arrow;
   - arrows in an enemy stay on its body parts on both screens, also in the ragdoll.
10. **Late join:** enable *Allow late joining* under Editor Preferences, Level Editor, Play, Multiplayer Options. Start a listen server with one player, shoot arrows into a wall and into the warrior, then add a client with the PIE toolbar's *Add Another Client*. **Expected:** the new player sees the same embedded arrows.

**Feel questions for the user:** draw time, damage and arrow speed, stamina cost, the aim camera, break chance and recovery, and the archer's damage, cadence, hold time and distances. Any change is a value in the tuning sheet.

## Limits

- **No feel tuning was done.** Every value is a starter value, and feel tuning needs the user's hands on the bow. The tuning sheet lists each lever and where it is authored.
- **Partial or untested contracts:** the matrix marks them. In short:
  - the perfect block's lower stamina cost;
  - flight effect, loop and fade (manual in RNG-01 only);
  - the AI corpse ragdoll and live animation (the test forces a ragdoll on a fixture body);
  - a full inventory;
  - rearrangement inside a corpse;
  - expiry during a transfer;
  - two looters of a corpse;
  - a failed death-loot retry with queued arrows;
  - an arrow in flight when its NPC shooter dies;
  - target changes, disengagement and a real loadout re-equip for the NPC budget;
  - losing the line of fire or the target mid-draw, and repositioning around cover;
  - simultaneous ranged shots;
  - a late joiner seeing a drawing player's posture;
  - the release presentation's duration.
- **Downed players stay targets:** a player at 0 health is first downed, and the AI attack driver drops only dead targets. So the archer, like the CMB-06 warrior, keeps attacking a downed player. Whether enemies should spare downed players for a revive is a design question outside this roadmap.
- **Visual quality** is the user's check: the aim and draw postures, the archer's pose on the sword-and-shield locomotion, arrow meshes in bodies and walls, the HUD, and that no flight guide exists.
- **Test runs:** a single rendered process running every native suite crashed the GPU driver at the same test twice. The regression was split as [Validation](#validation-2026-11-02) shows.
- **Mover:** the bow presentation stays CMC/Manny only, as the roadmap scopes it.
