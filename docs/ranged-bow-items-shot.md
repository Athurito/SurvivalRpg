# RNG-02: Bow, standard arrow and the shot

Task report for RNG-02 in the [ranged combat roadmap](ranged-combat-roadmap.md), PR_LINK. The first bow and the standard arrow are productive items in the new GameFeature `GF_Combat_Ranged`. One native ability task draws, validates and commits a projectile shot exactly once on the server: it launches the RNG-01 projectile, consumes one arrow from the shooter's inventory and pays the release stamina, or does nothing at all. Draw curves, costs and the passive seams are bow content. The primary attack button now holds until it is released. Aim input, camera, aim movement and animations follow in RNG-03, embedding and recovery in RNG-03b/03c, the HUD in RNG-04 and the bow enemy in RNG-05.

## C++ boundary decision

- **Classification:** a reusable native mechanism (the shot task with its validation and resource transaction), native schema (the ammunition fragment, the shot definition on the ranged weapon instance, the shot target data), and designer content (the bow, the arrow, the arrow projectile, `GA_BowShot`, its ability set, the draw curves and the recipes).
- **Runtime truth:**
  - **Release:** the server's task decides whether and how a release shoots. It clamps the client's draw time to the time it observed plus 0.1 s, rejects an aim origin more than 10 m from the avatar, traces the launch from the body to the muzzle and commits. The owning client only predicts its presentation.
  - **Arrows:** the shooter's inventory on `ARpgPlayerState`, including carried bags; never base storage or chests.
  - **Stamina:** the `URpgStaminaSet` attribute on the shooter's ability system.
- **Existing seams:**
  - `URpgRangedWeaponInstance`, the ranged stub from the baseline refresh.
  - `ARpgCombatProjectile` and its launch parameters (RNG-01).
  - The atomic by-definition inventory intents `GetTotalItemCountByDefinition`, `ConsumeItemsByDefinition` and `GrantItemDefinition`.
  - `URpgGameplayAbility::GetTunedValueForSpec` with `URpgAbilityTuningEffectComponent` for passive tuning.
  - The stamina change effect with `SetByCaller` `Data.StaminaDelta` that the sprint uses (`GE_RpgGasp_StaminaDelta`).
  - The RPG ability system's replicated `InputReleased` event and the equipment's slot ability sets.
  - `URpgInputConfig` and `URpgInputComponent::BindAbilityActions` for the primary attack's press and release.
- **Native, and why:**
  - **Prediction and validation:** custom target data, a scoped prediction window, the server's wait for the client's release with a timeout, and the clamp of client data are not available to Blueprint and must hold for every bow, NPC and spell shot.
  - **Exactly-once commit:** validate, spawn deferred, consume one arrow, pay stamina, finish the spawn, or roll everything back. Blueprint cannot order a deferred spawn against the inventory and attribute changes, and a partial commit would duplicate or lose arrows.
  - **Ammunition fragment:** a new item semantic, the projectile an ammunition item launches. Each arrow type configures it on its item definition.
- **New native classes:** two. `URpgAbilityTask_ProjectileShot` and `URpgInventoryFragment_Ammunition`. Their schema is `FRpgProjectileShotDefinition`, `FRpgProjectileShotTuning`, `FRpgProjectileShotTargetData`, `ERpgProjectileAmmunitionPolicy` and `ERpgProjectileShotResult`, with the helpers in `RpgProjectileShot`. `ARpgCombatProjectile` gained a deferred launch (`BeginLaunchFromAbility`, `FinishDeferredLaunch`, `AbortDeferredLaunch`), and `FRpgInputAction` the schema option `bPressOnStartOnly`.
- **Content:** `GA_BowShot` is a Blueprint directly on `URpgGameplayAbility`; it runs the task from the equipped bow and ends on release or cancel. Curves, costs, muzzle and timings are values of `BP_WeaponInstance_Bow`; nothing bow-specific is native.
- **Asset work:** authored through Unreal MCP and headless editor Python, validated by the asset contract tests and the native content test below.

## Behavior

### Problem

The bow and arrows existed only as sandbox placeholders (`ID_Placeholder_Bow`, `ID_Placeholder_Arrow`) without equipment, ammunition or an ability. `URpgRangedWeaponInstance` was an empty stub, and nothing could draw, validate or pay a shot.

### Change

- **Shot task (`URpgAbilityTask_ProjectileShot`):**
  - `Start Projectile Shot From Weapon` reads the shot definition of the ability's source, the equipped `URpgRangedWeaponInstance`. `Start Projectile Shot` takes a definition directly, for spells, scripted and NPC shots.
  - The draw starts with the task. `On Nocked` fires after the minimum nocking time. A release before it ends with `EarlyRelease` and costs nothing.
  - The locally controlled machine releases on the ability's input release or `Release Shot`. A remote client sends its draw time, camera origin, aim direction and a shot id as target data in a scoped prediction window, then predicts `On Released` without a projectile. The listen-server host and server-only activations resolve directly.
  - The server waits at most `MaxHoldSeconds` (60 s) for the release, then cancels with `TimedOut`. A second release is ignored.
  - The draw fraction is the clamped draw time over the full draw time. The bow's curves turn it into damage (`SetByCaller.Damage`) and launch speed; `StaggerDamage` is fixed.
  - Every end without a projectile publishes `On Cancelled` with its reason (`NoAmmunition`, `InsufficientStamina`, `Exhausted`, `InvalidShot`, `SpawnFailed`, `TimedOut`, `Interrupted`) and is logged at Verbose to `LogRpgAbilitySystem`.
- **Validation and geometry (server):** the client's draw counts at most 0.1 s longer than the server observed it. The aim origin must lie within 10 m of the avatar. The camera's aim traces up to `MaxAimDistance` to find the aim point. The arrow launches from the bow's muzzle offset toward that point; a trace from the body to the muzzle on the projectile channel finds walls in between, and an obstructed shot starts 2 cm in front of the wall, so it hits the wall instead of passing through it.
- **Commit, exactly once (server):**
  1. Check: arrows in the shooter's inventory (`GetTotalItemCountByDefinition`) and stamina for the release cost.
  2. Spawn the arrow projectile deferred.
  3. Consume one arrow (`ConsumeItemsByDefinition`).
  4. Pay the release stamina through the stamina change effect.
  5. Finish the spawn.
  Any failure rolls back the steps before it (arrow granted back, stamina refunded, deferred projectile destroyed) and publishes no projectile. Ammunition removed during the draw, too little stamina and a projectile that destroys itself on spawn pay nothing. An arrow that cannot be granted back is logged as a warning.
- **Hold drain:** with `HoldStaminaPerSecond` above zero, the server drains stamina from the nock on. At zero stamina the bow lowers with `Exhausted`, never fires, suppresses the input until it is released, and keeps the drain already paid.
- **Tuning:** draw time, release cost and hold drain resolve once per activation through `GetTunedValue` with `Ability.Tuning.Ranged.DrawTime`, `Ability.Tuning.Ranged.ReleaseCost` and `Ability.Tuning.Ranged.HoldDrain`, for the ability id `Ability.Ranged.BowShot`. A passive that changes during a draw applies to the next one. Costs need a valid stamina change effect; a shot without one stays free, also when a passive adds a cost. `Ability.Tuning.Ranged.AimMovement` (RNG-03) and `Ability.Tuning.Ranged.BreakChance` (RNG-03b) are reserved.
- **Ammunition policy:** `ShooterInventory` reads only an inventory on the ability system's owner when that owner is a player state. `Unlimited` must be chosen explicitly by the starting ability, for NPCs; missing inventory never falls back to it.
- **Ranged weapon instance:** carries the `ShotDefinition`, cannot block and has no melee attacks. Data validation reports an invalid shot definition.
- **Primary attack input:** `IA_PrimaryAttack` had a Pressed trigger. Enhanced Input then completes the action one tick after the press while the button is still down, so every bow draw ended as an early release, and held tool swings ran only once. The trigger is gone, and the new input config option `bPressOnStartOnly` (`FRpgInputAction`) binds the primary attack's press to `Started`: a click is still one melee press, and the release comes when the button is released. Held axe and pickaxe swings now repeat as the harvesting rework describes.
- **Dev command:** `Rpg.Inventory.GrantItem <ItemDefinitionClassPath> [Count] [equip]` grants items to the host's inventory and, with `equip`, equips the first one in the main hand (server only, non-shipping builds).

### Content

| Asset | Role |
| --- | --- |
| `GF_Combat_Ranged` (GameFeature) | Depends on `GameplayAbilities` and `GF_Combat_Core`; registered by default and enabled in `SurvivalRpg.uproject`. Active in the Experiences `RpgBaseline`, `RpgGaspCMC`, `RpgGaspMantle`, `RpgInventoryStorage` and `RpgPrototype`; `GF_Dev_Sandbox` lists it as a plugin dependency. The Mover Experiences stay unchanged. |
| `Items/ID_Bow` | Weapon, not stackable, never dropped on death, 1 x 3 cells, `T_UI_Item_Bow` icon; equips through `ED_Bow`. No combat skill tree. |
| `Equipment/Bow/ED_Bow` | `BP_WeaponInstance_Bow`, main hand only, both hands, load 0; grants `AS_Bow` in the main hand; spawns `BP_BowActor` at `Grip_Shield_L` (placeholder grip until RNG-03). |
| `Equipment/Bow/BP_WeaponInstance_Bow` | Parent `URpgRangedWeaponInstance`, `Weapon.Family.Bow`, cannot block. Shot: `ID_Arrow_Standard`, nock 0.3 s, full draw 1.0 s, `CF_Bow_DamageByDraw` (8 to 30) and `CF_Bow_LaunchSpeedByDraw` (2000 to 6000 cm/s), stagger 15, release cost 8 stamina, no hold drain, `GE_RpgGasp_StaminaDelta`, muzzle (60, 0, 50) cm, aim 100 m, server wait 60 s. These are starter values for playtesting. |
| `Equipment/Bow/BP_BowActor` | The bow mesh in the hand. |
| `GAS/Abilities/GA_BowShot` | Instanced per actor, locally predicted, on input triggered. Runs `Start Projectile Shot From Weapon` with the shooter's inventory and ends on release or cancel. |
| `GAS/AbilitySets/AS_Bow` | `GA_BowShot` on `InputTag.Weapon.Primary` with the ability id `Ability.Ranged.BowShot`. |
| `Items/ID_Arrow_Standard` | Stackable to 99, not a material, 1 x 1 cell, `T_UI_Item_Arrows` icon; the ammunition fragment launches `BP_Projectile_Arrow`. |
| `Projectiles/BP_Projectile_Arrow` | Parent `ARpgCombatProjectile`: the arrow mesh under `VisualRoot`, `GE_Damage_Instant`, hit cue `GameplayCue.Combat.Hit`, 5000 cm/s default speed, gravity 1, radius 2 cm, 8 s flight, 2 s linger, recoverable. |
| `Equipment/Bow/SkeletonArmy` | The SkeletonArmy archer's bow and arrow meshes, materials and downsized textures ([third-party notices](third-party-notices.md)). |
| `GF_Combat_Core/Input/IA_PrimaryAttack`, `DA_InputConfig_Combat` | No trigger on the action; `bPressOnStartOnly` on its config entry |
| `GF_Dev_Sandbox` recipes | `DA_Recipe_Parity_Bow` makes `ID_Bow` (12 wood, 2 ore) and `DA_Recipe_Parity_Arrows` 20 `ID_Arrow_Standard` (3 wood, 1 ore), with their costs unchanged. The placeholders are deleted without a save migration. |

### Decisions for later tasks

- **Skill tree:** the bow carries no `DA_SkillTree_Combat`. That tree's Battle Rhythm and similar talents tune `Ability.Tuning.Attack.Damage` for every ability of a weapon that carries it; ranged passives use the `Ability.Tuning.Ranged.*` tags with an Archery tree later.
- **Aim input (RNG-03):** bind the bow's held aim to `InputTag.Weapon.Block` (right mouse button, `IA_Block`). The bow cannot block, so the input is free while it is equipped, and `InputTag.Weapon.Secondary` has no mapping.
- **Attack tag:** `GA_BowShot` does not carry `Ability.Type.Attack`, because the AI's reactions to attacks would start at draw begin. RNG-05 decides how enemies read a drawn bow.
- **Predicted arrow (RNG-03):** the owner predicts only `On Released`. A predicted cosmetic arrow and a client notice for a rejected release are RNG-03 presentation; the shot id already travels with the release and the projectile.
- **Grip:** the bow hangs at `Grip_Shield_L` until RNG-03 sets the bow grip and its animations.
- **NPC ammunition:** RNG-05 starts the task with `Unlimited` for the bow enemy as an explicit content choice.

## Validation (2026-11-02)

Final runs in the task worktree, on `master` `ed787f37` plus this branch:

- **Review:** three independent review agents read the diff, one each for network authority and prediction, the resource transaction and lifecycle, and architecture and tests. Each finding was checked against the code and, for input, the engine source before fixing:
  - **Primary attack input (high, confirmed):** `IA_PrimaryAttack`'s Pressed trigger ended every real bow draw one tick after the press. The manual PIE check before the review had shown exactly that early release, wrongly blamed on the PIE key tool. Fixed as described under Behavior and guarded by `PrimaryInputHoldsUntilRelease`.
  - **Draw tolerance (medium, confirmed):** 0.25 s of tolerance left a modified client only 0.05 s of real nock time against the bow's 0.3 s. Lowered to 0.1 s; latency cancels out of the server's measurement.
  - **Commit order (low, confirmed):** the release stamina was paid before the arrow was consumed, unlike the agreed order, so a refused consumption still dipped stamina. Now arrow first, then stamina.
  - **Unchecked rollback (low, confirmed):** a failed grant-back of the arrow was silent. It is logged now; placement stays a limit.
  - **Tuned costs without a stamina effect (low, confirmed):** a passive could add a cost that the shot could not pay, making every release invalid. Costs now stay zero without a valid stamina effect.
  - **Bindings after an end (low, confirmed):** with a zero nocking time, an `On Nocked` handler that ended the shot left `Activate` binding on a finished task. The task now binds first and stops once it ended.
  - **Tests (medium and low, confirmed):** the ability-id targeting of ranged tunings, hold-drain tuning, the duplicate-release guard with a still-active ability, a cancel followed by a late release and the server timeout had no test, and the arrow contract froze gravity 1.0. The tests below add them; the contract now requires only falling arrows.
  - **Kept as limits:** the remote hold drain until the release arrives, and the missing aim-direction check (see Limits).
  - **Process (confirmed):** the third-party notice and this report were not committed yet when the review ran.
- **Red run first:** `PrimaryInputHoldsUntilRelease` failed on the unchanged input assets (press once per click false, a trigger present); the new native shot tests passed on the fixed code.
- **Build:** `ue.py build` passed, 0 errors, 0 warnings.
- **Native and asset tests:** `ue.py test SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.Ranged SurvivalRpg.Harvesting SurvivalRpg.Input SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.WeaponBlockAssetContract SurvivalRpg.Combat.WeaponAttackAssetContract SurvivalRpg.Combat.StarterEquipmentAssetContract SurvivalRpg.Combat.Block.Lifecycle SurvivalRpg.Combat.HitFeedback SurvivalRpg.Combat.Dodge SurvivalRpg.Combat.ReactionProfile SurvivalRpg.Combat.Runes SurvivalRpg.Combat.SwingTrace SurvivalRpg.Combat.AbilityContent SurvivalRpg.Combat.HyperArmor SurvivalRpg.AbilitySystem SurvivalRpg.Inventory SurvivalRpg.Crafting SurvivalRpg.Equipment --null-rhi` passed 420 / 420. Four inventory tests warn exactly as in October's runs (request-id collisions, Mover example components, controller action rows). The new tests:
  - `ProjectileShot.EarlyReleaseCancelsWithoutCost`: a release before the nocking time launches nothing and pays nothing.
  - `ProjectileShot.DrawCurvesSetDamageAndSpeed`: through the input release, half the draw gives the curves' half values for speed, damage and stagger; one arrow and one release cost; a long draw is a full draw from the muzzle.
  - `ProjectileShot.AmmunitionIsCheckedAtDrawAndRelease`: no draw without arrows; an arrow removed during the draw cancels the release without cost.
  - `ProjectileShot.InsufficientStaminaPaysNothing`, `SpawnFailureRefundsEverything`: no projectile, arrow or stamina is lost.
  - `ProjectileShot.DuplicateReleaseShootsOnce`: with the ability still active after its shot, repeated release messages launch one projectile and pay once; an overstated draw is clamped to the observed draw plus 0.1 s.
  - `ProjectileShot.CancelledDrawIgnoresALateRelease`: a cancel ends the draw, and a release arriving afterwards shoots and pays nothing.
  - `ProjectileShot.TuningIsFixedPerActivation`: a transient passive halving cost and draw time and adding hold drain, gained during a draw, applies to the next shot only.
  - `ProjectileShot.ShotWithoutStaminaEffectStaysFree`: passives add no cost to a shot without a stamina effect.
  - `ProjectileShot.ArrowsInACarriedBagShoot`: arrows only in a carried bag shoot and are consumed from the bag.
  - `ProjectileShot.LaunchObstructionStopsTheShotAtTheWall`: a wall between body and muzzle moves the launch in front of the wall, and a camera more than 10 m from the shooter is refused.
  - `ProjectileShot.HoldDrainExhaustionLowersTheBow`: the drain stops at zero stamina and lowers the bow without a shot, keeping the paid drain.
  - `ProjectileShot.UnlimitedAmmunitionIsAnExplicitPolicy`: an NPC-owned ability system has no shooter inventory; only the explicit policy shoots without arrows.
  - `ProjectileShot.BowContentShootsTheStandardArrow`: `AS_Bow` grants `GA_BowShot` with `BP_WeaponInstance_Bow` as source; through the input tags a tap costs nothing, a full draw launches `BP_Projectile_Arrow` at the curve's top speed and damage and pays one arrow and 8 stamina, and a passive for `Ability.Ranged.BowShot` halves the next cost while one for another ability id changes nothing.
  - `Ranged.BowAssetContract`, `Ranged.ArrowAssetContract`, `Ranged.PrimaryInputHoldsUntilRelease`, and `Harvesting.Content.ResourceParity` with the new recipe outputs.
- **PIE network tests:** `ue.py test SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.Network.CombatImpactCuesPIE SurvivalRpg.Network.KillXPCuePIE --expect-tests 12 --ue-arg=-ini:Engine:[ConsoleVariables]:TestFramework.CQTest.CommandTimeout.Network=90` passed 12 / 12, listen server with two clients:
  - `ClientShotLaunchesOneArrowAndPaysOnce`: a client draws 0.7 s and releases; the server launches one arrow, the shooter loses one arrow and 10 stamina once, the other players nothing, and every client sees the arrow.
  - `ServerTimesOutAHeldShotWithoutCost`: a client that never releases is stopped by the server after the 2 s hold limit, on both machines, without a projectile or cost.
  - The RNG-01 projectile tests and the melee, impact-cue and kill-XP tests are unchanged. Warnings are the baseline `LogNetPackageMap` and `AdvancedFriendsInterfaceLog` messages. One run of `TwoRemoteAttackersHitOneTargetAndEachHitPlaysOnceEverywhere` also logged a `SwingTraceWindow` warning (a montage ended inside its attack window); the ten earlier runs and an immediate rerun of `CombatImpactCuesPIE` (1 / 1) had none, and that test drives the ability system directly, which this task does not change.
- **Manual PIE, `Lvl_ThirdPerson`, standalone:** with the bow equipped through `Rpg.Inventory.GrantItem`, a 1.3 s hold of the left mouse button through `set_pie_input_key` drew the bow and released one `BP_Projectile_Arrow` at full draw on release. With the sword, a 3 s hold started one attack. With the axe, a 3.5 s hold started nine swings and none after the release.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

## Playtest

1. Open `Lvl_ThirdPerson` and start PIE as a single player. In the console run `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C 1 equip`, then `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C 20`. The bow appears at the left hand (placeholder grip) and the inventory shows 20 arrows.
2. Hold the left mouse button for about one second and release: an arrow flies along the view and drops with gravity, the inventory shows 19 arrows and stamina drops by 8. A quick click under 0.3 s shoots nothing and costs nothing. Without arrows, pressing does nothing.
3. **Input regression:** equip the sword with `Rpg.Inventory.GrantItem /GF_Combat_Core/Items/Weapons/ID_BasicSword.ID_BasicSword_C 1 equip` and hold the left mouse button: one attack, as before; clicks still chain the combo. Equip the axe (`/GF_Harvesting_Magic/Items/Tools/ID_Tool_Axe.ID_Tool_Axe_C`) and hold the button at a tree: it keeps chopping until you release.
4. **Multiplayer:** set Play to 2 players, *Play As Listen Server*, and run both commands of step 1 in the host's window. Open the host's inventory (Tab) and drop a few arrows and, after unequipping it, the bow. The client picks them up with F and equips the bow from its inventory. The client holds and releases: the host sees the same arrow fly, the client's arrow count drops by one and its stamina by 8 once.
5. There is no aim camera, crosshair or draw animation yet (RNG-03, RNG-04).

## Limits

- **Held input over the network not checked by hand:** the held left mouse button was checked in standalone PIE (bow, sword, axe). The listen-server test releases the client's shot through `Release Shot`; the input path to it is the same `InputReleased` event, but no PIE run held the button on a client.
- **No aim plausibility for remote shooters:** the server checks the aim origin's distance and the launch obstruction, not whether the aim direction matches the shooter's facing. A modified client can shoot in any direction from its own muzzle. RNG-03's aim stance ties the facing to the aim and is the place for that check.
- **Draw tolerance:** a client that overstates its draw gains up to 0.1 s of draw and can release up to 0.1 s before the nocking time.
- **Remote hold drain:** the server drains until the release arrives, about one trip longer than a remote client held. With little stamina it can lower the bow with `Exhausted` after the client already predicted the release. The starter bow has no hold drain.
- **Rollback on a failed spawn:** only a projectile that destroys itself while spawning reaches it. The returned arrow is a new item in the first free cell, possibly outside the bag it came from; if the inventory cannot take it, the arrow is lost and a warning is logged. The release stamina is paid and refunded in the same frame, so stamina listeners such as the regeneration delay still see the dip.
- **Not covered by a test:** the `Interrupted` result during hold drain and the exhaustion path over the network.
- **Presentation (RNG-03, RNG-04):** no predicted arrow, no client notice for a rejected release, no aim camera, aim movement or draw animations, no crosshair or ammunition display. The bow hangs at the shield grip.
- **Legacy replication only:** Iris is inactive in this project; the shot's target data was exercised only with its legacy `NetSerialize`.
- **Starter tuning:** the bow's draw, damage, speed and cost values are starting points for RNG-06.
- **No user check yet:** the user validates the whole ranged roadmap at the end (RNG-06).
