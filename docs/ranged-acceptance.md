# RNG-06: Integrated acceptance of the first bow

Task report for RNG-06 in the [ranged combat roadmap](ranged-combat-roadmap.md), [PR #372](https://github.com/Athurito/SurvivalRpg/pull/372).

This task closes the first bow delivery. It holds:
- the acceptance matrix for every contract the roadmap lists, with the evidence for each;
- the checks this task added for gaps the earlier tasks left;
- the cook and the full regression;
- the tuning sheet;
- one consolidated playtest for the user's check of RNG-01 to RNG-06.

No gameplay value changed: every number is still the roadmap's starter value, and the user's feel check decides the final ones. The [validation fixes](#validation-fixes-2026-11-03) close the medium findings of an independent validation afterwards.

## C++ boundary decision

- **Classification:** validation only. Tests, a cook and documentation; no runtime mechanism or content changes, apart from one test-access `friend` in `ARpgGameModeBase`, as its other world-save tests have.
- **New native classes:** none. The new PIE steps reuse the existing fixtures (`ARpgNetworkAutomationArrowBody`, `ARpgNetworkAutomationRecoveryWall`, `ARpgCombatNetworkFloorFixture`).

## Acceptance matrix

### Focused contracts

| Roadmap contract | Evidence | Status |
| --- | --- | --- |
| Nocking cancellation, partial and full draw, repeated shots, recovery timing, every interruption route | `ProjectileShot.EarlyReleaseCancelsWithoutCost`, `DrawCurvesSetDamageAndSpeed`, `CancelledDrawIgnoresALateRelease`, `HoldDrainExhaustionLowersTheBow`, `PresentationTagsFollowTheShot` (the release state is set at the release), `Ranged.Aim.InterruptionsCancelAndNeedFreshInput`, `Ranged.Aim.StaggerRemovalDeathAndAvatarChangeEndTheStance`, `ProjectileShot.BowShotPressedInTheAirWaitsForItsStance` (added by the [validation fixes](#validation-fixes-2026-11-03): a shot pressed in the air draws nothing until its stance begins on landing); PIE `RangedAimPIE.DodgeInterruptsTheDrawAndNeedsAFreshPress` | Automated, except the release presentation's 0.5 s duration and its reset by a new draw. Before the validation fixes, a shot pressed in the air fired without the stance (F40) |
| Aim ownership of primary and secondary input, camera cleanup, fresh-input suppression, passives across activations, avatar and equipment rebinding | `Ranged.Aim.TokensHoldTheCombinedStance` (the camera follows the remaining owner), `Ranged.Aim.InterruptionsCancelAndNeedFreshInput` (a cancelled hold needs a fresh press), `Ranged.Aim.PressDuringABlockingStateWaits` (a press during a dodge waits), `Ranged.Aim.MovementTuningIsFixedPerActivation`, `ProjectileShot.TuningIsFixedPerActivation`, `UI.RangedCombat.*` (the HUD follows equipping, a weapon refresh and unequipping, and unbinds) | Automated, except the HUD's rebinding to a new avatar after a respawn: no test binds the view model to a second ability system or avatar (RNG-04 limit) |
| Last arrow, ammunition removed while drawing, insufficient stamina, spawn failure, duplicate release, no partial commits | `ProjectileShot.AmmunitionIsCheckedAtDrawAndRelease`, `InsufficientStaminaPaysNothing`, `SpawnFailureRefundsEverything`, `DuplicateReleaseShootsOnce`, `ArrowsInACarriedBagShoot`; PIE `CombatProjectilePIE.ClientShotLaunchesOneArrowAndPaysOnce`, `ServerTimesOutAHeldShotWithoutCost` | Automated |
| Impact on world and characters, shield block, cheaper Perfect Block without reflection or remote stagger, dodge immunity | `Projectile.WorldImpact`, `ImpactAppliesOnce`, `ShieldBlock` (a perfect block takes no damage and never staggers the distant shooter), `DirectionalBlock` (block outcomes by direction), `FlightSweep` (a dodging target lets the projectile through) | Automated, except the perfect block's lower stamina cost: the fixture's block costs are 0 |
| Fireball carrier contract: flight system and loop everywhere, stop and fade, one authoritative impact cue | PIE `CombatProjectilePIE.ReplicatedFlightHitsOnceWithOneImpactCuePerMachine` (every client sees the flight, the stop holds, one impact cue per machine, the projectile goes), `PredictedAbilityLaunchesWithoutEquipment`, `PointBlankAndOrphanedProjectilesStillHitOnce` | Replication, stop and cues automated; the flight effect, its loop and the fade were seen only in RNG-01's manual fireball run |
| Player hits leave nothing (co-op hits, downed players, death, respawn) | `Projectile.Recovery.CharactersKeepNothing`: players have no host, so every hit on a player keeps nothing, whatever their state | Automated by construction; co-op, downed and respawn not run separately |
| Consumed-on-impact ammunition: no break roll, embedding, pickup, credit or NPC token | `Projectile.Recovery.ConsumedOrUnpaidLeavesNothing`, `ProjectileShot.UnlimitedAmmunitionIsAnExplicitPolicy` | Automated |
| Grouping on AI characters and world hosts, full and partial collection, merged deadline, world cap, no lost or duplicated quantity | `Projectile.Recovery.GroupingAndWorldCap`, `CollectExpireAndLoseHost`, `AICharacterGroupsBeyondVisibleMaximum`, `AICharacterKeepsArrowsAsCorpseLoot` | Automated |
| Launch state after weapon switch, shooter death, avatar replacement | `Projectile.LaunchState` (stat changes after the launch, shooter death); PIE `PointBlankAndOrphanedProjectilesStillHitOnce` (orphaned projectiles) | Partial: shooter death and later stat changes are automated. A weapon switch and an avatar replacement during the flight hold by construction, because the launch keeps its own snapshot, but no test performs them |
| One break roll, fixed retry outcome, lifetime expiry, missing hosts, **late-join attachment reconstruction** | `Projectile.Recovery.BreakRollAndSurfaces`, `WorldImpactEmbedsOnce`, `CollectExpireAndLoseHost`; **new:** PIE late joiner sees the arrow embedded in its copy of the moved wall | Automated |
| Embedded arrows **following live animation and local ragdolls**, the killing arrow, corpse hits | `AICharacterKeepsArrowsAsCorpseLoot` (record before the damage), `CorpseTakesArrowsUntilItCompletes` (hits on a dying body and a corpse); **new:** PIE late joiner rebuilds both shafts on its skeleton, and each machine's shaft stays on its bone while a test-forced physics simulation of that machine's fixture body falls | Attachment and a forced ragdoll automated; the AI corpse ragdoll and live animation were not observed in a running game, which the user's playtest covers |
| Full inventory, two concurrent looters, partial stacks, rearrangement, expiry during transfer, failed death-loot retries | `CollectExpireAndLoseHost` (a looter with a full grid and one partial stack takes only what fits, the rest stays in the group, and a second looter takes it), `AICharacterKeepsArrowsAsCorpseLoot` (the loot table's own arrows taken first); PIE `IntactArrowEmbedsEverywhere...` (two looters of a world pickup in one frame) | Partial: a looter without any free space, rearrangement inside a corpse, expiry during a transfer, two looters of a corpse and a failed death-loot retry with queued arrows are not tested |
| NPC fire beyond the recovery budget: misses, breaks, shooter death, AI and loadout refreshes, no refill | `ProjectileShot.UnlimitedAmmunitionIsAnExplicitPolicy` (budget, a new life, a failed release, a bigger budget never raises it); PIE `RangedEnemyPIE.BowEnemyShootsThePlayerThroughTheSharedBowShot` (five tokens, then another hit without a sixth) | Partial: tokens are spent at the release, so misses and breaks follow by construction; an arrow in flight when its shooter dies, disengagement and a real loadout re-equip are not exercised. `RangedEnemyPIE.ANewTargetDuringTheDrawGetsItsOwnHold` exercises a target change mid-draw, but not against the budget |
| Normal persistence of collected arrows; **no temporary arrow state in world saves** | **new:** `Projectile.Recovery.PickupsStayOutOfWorldSaves` (the world-save capture takes the chest and no pickup); collected arrows are ordinary inventory items (`SurvivalRpg.Save.*`) | Automated |
| No flight-path tube, line, arc, guide, impact marker or ballistic crosshair correction | No such component or widget exists in the code or the bow assets. The RNG-04 captures and the independent validation's PIE showed only the crosshair and the draw bar. `UI.RangedCombat.HudAssets` checks the HUD widgets but asserts nothing about a trajectory display. | Not automated: code inspection and captures; user's visual check |

### Integration and playtest

| Roadmap item | Evidence | Status |
| --- | --- | --- |
| One player's and one enemy's bow use the same mechanism, without a player controller for the AI | `Ranged.EnemyBowAssetContract` (identical ability sets, so the enemy bow grants `GA_BowShot`); PIE `RangedEnemyPIE` (a real `BP_RiftGrunt_Bow` shoots the player with the enemy bow; the test does not check which ability class released the shot) | Automated |
| Aim, draw, release, line-of-fire obstruction, **target loss or death during the draw**, spacing, retreat and repositioning | PIE `RangedAimPIE`, `RangedEnemyPIE` (a wall blocks the shot). **New:** `RangedEnemyPIE.DeathDuringTheDrawShootsNothing`: an archer dying mid-draw launches nothing, and a target's death mid-draw lowers that draw unreleased. The [validation fixes](#validation-fixes-2026-11-03) add `RangedEnemyPIE.ANewTargetDuringTheDrawGetsItsOwnHold` (a covered target keeps the archer holding; when that target dies, the full draw turns to a player in reach with a new hold) and `AI.AttackDriver.APlayersHitTargetsThePlayersPawn` (a player's hit draws the enemy's attention). RNG-05's sandbox PIE saw the approach, circling and the retreat under close pressure. | Death, a short loss of the line of fire and a target change mid-draw automated; repositioning around cover is not tested |
| Listen server with two clients, latency, **late join**, simultaneous shots, point-blank impacts; remote draw, release, aim posture, embedded arrows, **ragdoll attachments** | Every PIE test runs a listen server, two clients and 60 ms lag. `PointBlankAndOrphanedProjectilesStillHitOnce`, `RangedAimPIE.RemoteAimDrawAndReleaseShowEverywhereAndCapMovement`, plus the new late-join steps (embedded pickup, shafts) and ragdoll steps above. | Partial: simultaneous ranged shots and a late joiner seeing a drawing player's posture are not tested; posture quality is the user's check |
| Regression: melee combo, block, dodge, equipment, inventory, attribution, GASP montages and layers, death and corpse, Mover foundations | See [Validation](#validation-2026-11-02) | Automated |
| Compile affected assets, parentage, skeletons, dependency closure; cook the test map | Asset contracts (`BowAssetContract`, `ArrowAssetContract`, `EnemyBowAssetContract`, `UI.RangedCombat.HudAssets`); cook of `Lvl_CombatSandbox` | Partial: compilation, parentage, skeletons and the cook are automated. The dependency closure of `GF_Combat_Ranged` (no `GF_Dev_Sandbox` or test-only references, Core never depending on ranged) is declared in its `.uplugin`, but no test checks it |
| Reproduction steps for every observable task | [Playtest](#playtest-the-users-check) below | Done |
| Owner-predicted cosmetic arrow, reconciled by shot identity ([roadmap](ranged-combat-roadmap.md#shot-commit-and-networking)) | Never built: RNG-01 to RNG-03 passed it on and deferred it. The owner sees its arrow only when the server's projectile replicates, about one round trip after the release. | Open; recorded as a [later extension](ranged-combat-roadmap.md#later-extensions) |

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

## Validation fixes (2026-11-03)

An independent validation of RNG-01 to RNG-06 on `fbf92894` rated the delivery "passed with restrictions": no high defects, five medium findings (F40, F57, F58, V1, F36), 45 low findings and the never-built predicted arrow. This follow-up, [PR #__PR__](https://github.com/Athurito/SurvivalRpg/pull/__PR__) on `master` `32d961cc` (with PR #373's bow grips), fixes the five medium findings, corrects the matrix rows that overclaimed (F02, F09, F11, F12, F79, F101) and the stale roadmap prose (F75), and records the predicted arrow as an open later extension. The low findings stay open unless a fix touched them.

### C++ boundary decision

- **Classification:** fixes inside existing native mechanisms (the shot task, the ability system's aim tokens, the AI attack driver, the ranged StateTree task, the projectile's recovery placement) and one Widget Blueprint change (`CUI_InteractionPrompt`). No new class, asset or replicated field.
- **Native, and why:** the shot's draw start, the driver's target choice and the drop host are authority, prediction and lifecycle rules of native mechanisms. The prompt's target name is presentation, so it lives in the Widget Blueprint's existing hook; the native presenter keeps its compact default.

### Behavior

- **F40, a shot pressed in the air (RNG-03):** a bow shot pressed after a jump, a fall or a vault drew, nocked and fired without the aim stance, at full speed, and paid an arrow and stamina. The aim task already waited for the landing, but the shot task did not know about it.
  - Now a projectile shot of an ability tagged `Ability.Type.RangedAim`, such as `GA_BowShot`, starts its draw only when that activation's own aim token begins (`URpgAbilitySystemComponent::IsRangedAimHeldBy`, `OnRangedAimBegan`). Owner and server each decide this from their own movement, so the draw starts at the landing on both.
  - A release before the stance began ends the shot as an early release: no arrow, no stamina, no projectile, on owner and server.
  - This follows the existing rule of the aim task: a press during a jump waits for the landing and counts as the fresh input. A shot ability without that tag, such as a spell, still draws at once.
- **F57, players never drew an enemy's attention (RNG-05, inherited from CMB-06):** a player's damage names the player state as instigator, because the player's ability system lives there, and the AI attack driver only accepts targets with a health component. Hits by players therefore never retargeted an enemy.
  - `URpgAIAttackDriverComponent::HandleHealthChanged` now resolves a player state or controller instigator to its pawn first, as the hit reactions already do.
  - Both the StateTree's last-attacker policy (with its 4 s switch cooldown) and the driver's own retaliation now target the player who hit.
  - `IsValidCombatTarget` stays strict, so a player state never becomes a combat target.
- **F58, the archer's hold carried over a target switch (RNG-05):** the `Ranged Attack` task rolled its 0.4 to 0.8 s hold only on entering, so a new target mid-draw got the rest of the old hold, or none after the old target briefly took cover, and the shot left before the new target could read it. The task now remembers the target its hold was rolled for and rolls a new hold, with a fresh line-of-fire grace, when the combat target changes.
- **V1, the collect prompt showed no name or quantity (RNG-03b):** `ARpgEmbeddedProjectilePickup` set the target text "Arrow (N)", but the HUD showed only "F Collect".
  - **Cause:** `URpgInteractionPromptWidget` clears and collapses its optional `TargetTextBlock` on every refresh ("intentionally one line"), and `CUI_InteractionPrompt` placed no such widget and had no graph. So no prompt in the game showed its target text: doors, chests, storage, revive targets, loot and pickups showed only their action.
  - **Fix:** `CUI_InteractionPrompt` now places a `TargetTextBlock` (`CUI_TextStyle_BodyMuted`, 10 px after the action), and its `On Prompt Presentation Changed` calls `F_ShowTargetText` while the prompt is Ready, which shows `PromptData.TargetText` when it is not empty. Every interaction that names a target now shows it, such as "F Collect Arrow (3)" or "F Open Storage". The native presenter and the gameplay side are unchanged.
- **F36, a dropped arrow followed hosts clients cannot resolve (RNG-03b, latent):** the drop branch attached the pickup to any moving ground, without the check the embed branch uses. On a moving ground that no client can resolve, the server's pickup followed the ground while the clients' copies stayed behind.
  - The drop now uses the same check, `ARpgEmbeddedProjectilePickup::CanEmbedIn`, for moving ground. On a moving ground that no client can resolve, the pickup has no host and stays where it fell, on every machine.
  - A resolvable moving ground still carries the pickup along.
  - Ground that never moves always hosts the pickup, for grouping, even when the ground has no owner actor, as level BSP does.

### Matrix and roadmap corrections

- F02: no test checks that a flight-path display is absent; the row now says so.
- F09: weapon switch and avatar replacement during the flight hold by construction, but no test performs them.
- F11: a full grid with a partial stack is tested; only a looter without free space is not.
- F12: no test checks the dependency closure of `GF_Combat_Ranged`.
- F79: no test rebinds the HUD to a new avatar after a respawn.
- F101: the PIE test does not check which ability released the archer's shot; the asset contract does.
- F75: the roadmap's prose no longer calls the tasks Deferred or the plan "not a report of implemented gameplay".
- The predicted cosmetic arrow is a matrix row, a limit and an open roadmap extension.

### Validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed, 0 errors, 0 warnings.
- **New tests:**
  - `SurvivalRpg.Combat.ProjectileShot.BowShotPressedInTheAirWaitsForItsStance`: the real `GA_BowShot` from `AS_Bow` on a falling character. A press in the air keeps the ability but neither aims nor draws; a release in the air after a full draw's time shoots nothing and costs nothing; a press held through the landing begins stance and draw at the landing (draw progress 0), and the release shoots one arrow and pays once.
  - `SurvivalRpg.AI.AttackDriver.APlayersHitTargetsThePlayersPawn`: a real `ARpgBasePlayerState` with team 2 and an `ARpgCharacter` pawn. Its hit makes the pawn, never the player state, the target, under the StateTree's last-attacker policy and in the driver's own retaliation; the owner's own hit keeps the target.
  - `SurvivalRpg.Network.RangedEnemyPIE.ANewTargetDuringTheDrawGetsItsOwnHold`: listen server, two clients, 60 ms lag. The archer holds a full draw at the target; a wall then covers the target for 0.82 s, longer than the longest hold; the target dies; the archer turns the full draw to the other player in reach (13.5 m), releases nothing at the switch, holds again for at least 0.3 s and hits that player.
  - `SurvivalRpg.Combat.Projectile.Recovery.DropFollowsOnlyResolvableHosts`: an arrow coming down on an unresolvable moving block drops onto it without a host or attachment and stays when the block moves; an arrow off an unresolvable door onto a resolvable moving lift rides the lift.
- **Review:** an independent review agent read the source diff against the engine code. It found no high-severity defect and confirmed several points:
  - fresh activation keys per activation;
  - safe delegate removal during the broadcast;
  - the binding order;
  - that no shot waits forever;
  - that each new test fails without its fix.

  It raised three points, all fixed and checked:
  - **Drops on static ground without an owner stopped grouping (medium-low, confirmed):** the first F36 fix also took the host from static ground that `CanEmbedIn` rejects, such as level BSP. Such drops then never grouped and filled the world cap. Only moving ground is checked now. `DropFollowsOnlyResolvableHosts` adds an owner-less static floor that keeps hosting; with the old rule that case fails.
  - **A synchronous end before the aim token is stored (low, latent):** a shot that ends its activation while its stance begins would leave the token, and the authority's `State.Ranged.Aiming`, set. The aim task now releases a token whose task already finished.
  - **The F58 PIE case's margin (low):** the wall now spawns in the full-draw frame and the target dies in the same check that measures the wait. The PIE tests also limit each simulated frame to 1/60 s, so a hitch cannot skip the grace.
- **Relaxed presentation freeze:** `SurvivalRpg.Interaction.HudAssets.Presentation` required the focus prompt to have no Blueprint graph and no `TargetTextBlock`. That is the kind of freeze AGENTS.md rules out, and V1 needs both. The test now only checks that an authored target name is a CommonTextBlock, the type of the presenter's binding.
- **Mutation check:** with each runtime fix reverted (the stance wait, the instigator resolution, the hold reroll, the drop check) and rebuilt, each new test failed:
  - the airborne shot drew and launched;
  - the player's hit kept the old target;
  - the archer released the spent hold at the switch ("The switch does not release the spent hold at once");
  - the pickup attached to the unresolvable block and moved with it.

  With the fixes restored, all four pass.
- **Focused runs before the review fixes:** `ue.py test SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.Ranged SurvivalRpg.Combat.Projectile SurvivalRpg.AI.AttackDriver --null-rhi`, 50/50. `ue.py test SurvivalRpg.Network.RangedEnemyPIE`, 3/3 (baseline warnings, plus the known teardown `SpawnActor failed` warning).
- **Full regression on the final build (`bc843caf`; later commits change only documentation), one Unreal process at a time, MCP server off, 90 s network command timeout:**
  - native ranged and combat suites with `--null-rhi` (`SurvivalRpg.Combat.Projectile`, `ProjectileShot`, `Ranged`, `UI.RangedCombat`, block, hit feedback, dodge, reaction profile, runes, swing trace, ability content, hyper armor and the weapon, starter equipment and socket asset contracts), 85/85;
  - native regression with `--null-rhi` (`SurvivalRpg.AbilitySystem`, `Inventory`, `AI`, `Interaction`, `Equipment`, `Loot`, `Save`, `Input`, `UI`, `Crafting`, `Harvesting`, `Progression`), 512/512;
  - `SurvivalRpg.Combat` with rendering, 105/105;
  - `SurvivalRpg.Network`, 49/49, including all `CombatProjectilePIE`, `RangedAimPIE` and `RangedEnemyPIE` cases;
  - `SurvivalRpg.GASP` with `np.ForceReconcile 0` and `t.MaxFPS 30`, 114/119 in the full run. The five failures are Mover traversal fixtures, which use neither the bow nor the changed code:
    - `Mover.Mantle...AuthorityCancellationDuringAutoBlendRemainsCancelled`, `VariantComparison...RagdollVariantRunningMantlePreservesMoverTraversal`, `...RagdollVariantRunningVaultPreservesMoverTraversal` and `Vault.GaspVaultAuthoredMapPIE.NegativeAngledVaultAlignsWithoutTurningTheView` timed out;
    - `Mover.Vault...EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection` missed its known VAL-03 overlap window.
    - Another session's editor ran on the same GPU during that run. Rerun alone on the same build, all five passed (5/5). Two of them are documented as timing-dependent (VAL-03 and the ragdoll-variant vault in `docs/gasp-integration-handoff.md`).
  
  All warnings are the known baseline:
  - inventory: controller action rows, request-id collisions and the Mover example components;
  - network: `LogNetPackageMap`, `AdvancedFriendsInterfaceLog`, the late-join arrow body's socket lookups before its mesh, and one teardown `SpawnActor failed`;
  - the `CUI_RespawnScreen` tick note;
  - one engine HTTP timeout in `ReactionProfile`;
  - GASP: `LogNetworkPrediction` rollback notes, out-of-date pose assets, Pose Search skips and the Mover pawn's runtime retarget profile.
- **Before the review fixes,** the same native, Combat and Network suites passed on `65660faf`, except one failure. The native regression then failed `SurvivalRpg.Interaction.HudAssets.Presentation`, which froze the old prompt and is now relaxed as described above.
- **Two-player listen-server PIE (manual, own editor, on `65660faf`; the later review fixes change only the drop host on static ground, a guard for an aim token whose activation already ended, and tests):** `Lvl_CombatSandbox`, a host and one client in one process, real inputs through `RpgEditorPlaytestTools.set_pie_input_key`; bow, backpack and 20 arrows granted through the server inventory, and god mode on both players. The warrior was frozen.
  - **F40:** each shooter was lifted 9 m on the server.
    - **Host, release in the air:** pressed while falling and released 0.5 s later, still falling. Neither stance nor draw on the server, 20 → 20 arrows, no projectile on server or client.
    - **Host, held through the landing:** no draw in the air; the draw starts in the landing frame; one projectile on both machines; 20 → 19.
    - **Client:** the same two trials gave the same results: nothing in the air, 20 → 20; the draw from the landing frame, one projectile on server and client, 20 → 19.
  - **V1:** the host 2.5 m from its embedded arrow. The live `CUI_InteractionPrompt` shows action "Collect" and target "Arrow (1)", both visible; the capture shows "F COLLECT Arrow (1)".
  - **F57 and F58:** host 10 m in front of the archer, client 14 m to its side.
    1. The archer acquired the host, then drew at it.
    2. After the 4 s switch cooldown, 1.16 s into a draw (the full draw takes 1.0 s), damage from the client's ability system hit the archer. Its context names the client's player state as instigator, as the client's arrows do.
    3. The archer switched to the client at once, held its full draw again for 0.58 s and released straight at the client: the cosine between the shot and the direction to the client was 1.0, and 0.73 toward the host.
    4. 4.5 s later the host shot the archer with a real arrow; the target switched from the client to the host. The host's count fell 19 → 18, and the shaft stuck at `spine_05`.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed. The `unreal-lyra-expert` references now record `Widget|SetText(Text)` as the TextBlock's call (`Class|Text|SetText` is its property setter) and the prompt widget's target-text hook.

### Limits

- **Owner and server decide the landing each from their own movement.** In the rare case that a release reaches the server before the move in which the owner landed, the server cancels a shot the owner predicted as released. It costs nothing and the owner's bow lowers. A release that close to the landing is already shorter than the 0.3 s nock.
- **A shot whose stance never begins never draws.** If the aim task of an `Ability.Type.RangedAim` shot cannot acquire its token, the shot waits until its release, which cancels it without cost. No current content does that, and shots without the tag are unaffected.
- **The second player's hits draw the attention at most once every 4 s,** the StateTree's switch cooldown. Hits inside the cooldown keep the current target.
- **F58's PIE case changes the target through a death:** a switch through a hit is covered by the driver test and the manual PIE, not by an automated PIE test.
- **V1 changes every interaction prompt.** Doors, chests, storage, revive and loot now also show their target name next to the action. The compact nearby indicator, the only other prompt widget, is unchanged. The target text has no automated presentation test, because widget content is checked visually (see the `unreal-lyra-expert` testing rules).
- **The graph has no comment boxes:** the 5.8 tools cannot create them, so the function name `F_ShowTargetText` carries the explanation.
- **Still open:** the 45 low findings of the validation report, apart from the matrix rows corrected above; the predicted cosmetic arrow; and the user's feel check.

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
   - Jump and press the left button in the air. **Expected:** nothing draws while you fly; released before the landing, nothing flies and nothing is paid. Held through the landing, the stance and the draw begin when you land.
3. **Arrow feel:** shoot at different draws. **Expected:**
   - a full draw flies flat and fast;
   - a short draw drops sooner and hits weaker; the damage curve runs from 8 to 30, and the shortest shot leaves at the 0.3 s nock, already above the curve's start;
   - there is no flight line, arc or impact marker anywhere.
4. **Recovery in the world (RNG-03b):** shoot walls and the floor. **Expected:**
   - about seven in ten arrows stay embedded where they hit, and the rest break;
   - each can be picked up once, and its prompt reads "F Collect Arrow (1)", with the group's quantity in the brackets;
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
   - arrows in an enemy stay on its body parts on both screens, also in the ragdoll;
   - when the archer shoots at one player, the other player can hit it and draw its attention, at most once every 4 s. If that happens mid-draw, the archer turns its bow to the new target and holds again visibly before it shoots.
10. **Late join:** enable *Allow late joining* under Editor Preferences, Level Editor, Play, Multiplayer Options. Start a listen server with one player, shoot arrows into a wall and into the warrior, then add a client with the PIE toolbar's *Add Another Client*. **Expected:** the new player sees the same embedded arrows.

**Feel questions for the user:** draw time, damage and arrow speed, stamina cost, the aim camera, break chance and recovery, and the archer's damage, cadence, hold time and distances. Any change is a value in the tuning sheet.

## Limits

- **No feel tuning was done.** Every value is a starter value, and feel tuning needs the user's hands on the bow. The tuning sheet lists each lever and where it is authored.
- **Partial or untested contracts:** the matrix marks them. In short:
  - the perfect block's lower stamina cost;
  - flight effect, loop and fade (manual in RNG-01 only);
  - the AI corpse ragdoll and live animation (the test forces a ragdoll on a fixture body);
  - a looter without any free space (a full grid with a partial stack is tested);
  - rearrangement inside a corpse;
  - expiry during a transfer;
  - two looters of a corpse;
  - a failed death-loot retry with queued arrows;
  - an arrow in flight when its NPC shooter dies;
  - disengagement and a real loadout re-equip for the NPC budget;
  - repositioning around cover;
  - a weapon switch or avatar replacement while a projectile flies;
  - the HUD's rebinding after a respawn;
  - the dependency closure of `GF_Combat_Ranged`;
  - simultaneous ranged shots;
  - a late joiner seeing a drawing player's posture;
  - the release presentation's duration.
- **No predicted arrow:** the roadmap asks for an owner-predicted cosmetic arrow, reconciled by shot identity. It was never built; the owner sees its arrow when the server's projectile replicates. It is an open [later extension](ranged-combat-roadmap.md#later-extensions).
- **Downed players stay targets:** a player at 0 health is first downed, and the AI attack driver drops only dead targets. So the archer, like the CMB-06 warrior, keeps attacking a downed player. Whether enemies should spare downed players for a revive is a design question outside this roadmap.
- **Visual quality** is the user's check: the aim and draw postures, the archer's pose on the sword-and-shield locomotion, arrow meshes in bodies and walls, the HUD, and that no flight guide exists.
- **Test runs:** a single rendered process running every native suite crashed the GPU driver at the same test twice. The regression was split as [Validation](#validation-2026-11-02) shows.
- **Mover:** the bow presentation stays CMC/Manny only, as the roadmap scopes it.
