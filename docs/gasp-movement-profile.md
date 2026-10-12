# GASP-11 – Shared movement profile

Status: **Ready** (defined 2026-10-12). The package outline is in the [movement plan](gasp-movement-plan.md#gasp-11-shared-movement-profile). This report refines it against the current code and assets and is the task report for the implementation PR.

## Goal

One designer-tuned profile asset sets how the CMC GASP pawn accelerates, brakes, turns, falls, and jumps. Prediction-safe overrides of `URpgCharacterMovementComponent` apply it. The profile replaces today's scattered defaults and makes movement heavier, with no load dependence.

## Why

- The [2026-10-11 review](gasp-movement-plan.md#findings-from-the-2026-10-11-review) measured much snappier starts and stops than the original: 0.24 s to run speed and a 0.125 s / 18.5 cm stop, against about 0.6 s and 0.25 s / 62 cm.
- The values come from four places: the native constructor, `BP_Rpg_Character`, `BP_RpgGasp_CMC`, and component properties added by `GASP-07` and `GASP-09`. There is no single place to tune the feel.
- GASP gets its feel from Blueprint code that rewrites the movement values every tick: it lowers acceleration during sprint build-up, brakes harder without input, and lowers friction while sprinting. The [design rules](gasp-movement-plan.md#design-rules) exclude that pattern.

## Current state (checked 2026-10-12 on `master` `1282e872`)

The values are class defaults read in the running editor. The GASP column is the per-tick behavior of the original `SandboxCharacter_CMC`, as recorded by the review.

| Value | Native `ARpgCharacter` (AI) | `BP_Rpg_Character` (baseline) | `BP_RpgGasp_CMC` | GASP original |
| --- | --- | --- | --- | --- |
| Run / sprint / crouch (cm/s) | 600 / – / 300 | 600 / – / 300 | 375 / 585 / 200 | 500 / 700 / 225 forward; run and crouch slower sideways and backward |
| `MaxAcceleration` (cm/s²) | 2400 | 1500 | 1500 | 800; tapers to 300 during sprint |
| Braking, no input / with input (cm/s²) | 1400 | 2000 | 2000 | 2000 / 500 |
| Braking friction | ground friction × 1 | separate 6 × 1 | separate 6 × 1 | factor 0, none |
| Ground friction | 8 | 8 | 8 | 5; 3 while sprinting |
| Falling deceleration / air control | 0 / 0.05 | 1500 / 0.35 | 1500 / 0.35 | 0 / 0.25 |
| Jump Z (cm/s) | 420 | 500 | 500 | 500 |
| Yaw | camera, 720°/s | orient to movement, 500°/s | rotation modes: instant on the ground, 200°/s in the air | instant on the ground, 200°/s in the air |

| Area | Finding |
| --- | --- |
| Existing seams | `GetMaxSpeed` picks the sprint speed of the GAS lease and caps it while blocking. `PhysicsRotation` applies `GroundRotationRate` and `AirRotationRate` per move. `RpgAbilityTask_Sprint` copies the component's `SprintSpeed` into the lease at activation. Sprint and block replay from saved-move history. |
| Inheritance | `BP_RpgGasp_MantleCharacter` and the three `GASP-08` pilot pawns inherit from `BP_RpgGasp_CMC`. `BP_Rpg_Character` is the baseline pawn of `DA_PawnData` and `DA_PawnData_Baseline`. The AI's `BP_BaseEnemy` derives from the native `ARpgAICharacter` and uses the native defaults. |
| Trajectory | `ABP_RpgGasp_CMC` builds its Motion Matching trajectory with `PoseSearchGenerateTrajectory` on the game thread, because `Update_CVarDrivenVariables` sets `UseThreadSafeUpdateAnimation` to false every frame. The library calls `GetMaxSpeed()` and `GetMaxBrakingDeceleration()` virtually, but reads `GroundFriction`, `BrakingFriction`, `BrakingFrictionFactor`, and `bUseSeparateBrakingFriction` as fields (`PoseSearchTrajectoryLibrary.cpp`). It turns the prediction with its own `RotateTowardsMovementSpeed`, not with the capsule's turn rate. |
| Load | `URpgEquipmentLoadoutComponent` derives the load tier: Medium from 13 kg, Heavy from 23 kg. Movement reads no load state. |
| Tests that read these values | `SurvivalRpg.GASP.CMC.SpeedParity` reads the speeds from the pawn defaults. `SurvivalRpg.Movement.CMC.RotationModes.*` set the rotation rates. `SurvivalRpg.Movement.CMC.Sprint.*` and `SurvivalRpg.GASP.CMC.Sprint` cover the sprint speed. |

## Scope

- **Native schema.** Add `URpgMovementProfile`, a `UDataAsset` in `Source/SurvivalRpg/Core/Character`, with documented units and ranges. It holds:
  - speeds for run, sprint, and crouch; all directions share one speed because the Relaxed loops do, and walk joins in `GASP-12`;
  - acceleration, tapering to a lower value as speed rises from run to sprint speed;
  - braking deceleration without and with input, and the braking friction factor;
  - ground friction, tapering toward a sprint value;
  - free-movement turn rates on the ground, while sprinting, and in the air; the combat stance keeps turning instantly (`GASP-09` decision 3);
  - air control, falling deceleration, and jump Z velocity.
- **Application** in `URpgCharacterMovementComponent`:
  - A designer-set profile reference. Without one, the component behaves as today, so the baseline pawn and AI are unchanged.
  - Values that depend on the move come from overrides that read only the move's own state: velocity, input acceleration, and the effective sprint gait. They cover the acceleration taper, braking with or without input, the friction taper, and the turn rate per gait. Saved-move replay and the server compute the same values, and nothing rewrites settings per tick.
  - Values the engine reads as fields (for example `JumpZVelocity`, `AirControl`, `MaxWalkSpeedCrouched`) are taken from the profile once when the component initializes, on every role.
  - The friction fields that the trajectory reads must match the profile. Phase A decides whether they stay at the profile's base value or are mirrored on the game thread, as `GASP-09` does for the rotation flags.
  - The sprint lease takes its speed from the profile. GAS keeps owning sprint authorization and stamina.
- **One source.**
  - `SprintSpeed`, `GroundRotationRate`, and `AirRotationRate` move from the component into the profile; only the GASP pawn uses them.
  - The sprint gates `MinimumSprintInput` and `CombatStanceSprintAngle` stay on the component, because they are gameplay rules rather than feel.
  - `BP_RpgGasp_CMC` loses its overrides of the values the profile owns.
- **Designer asset.** One profile asset under `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/`, referenced by `BP_RpgGasp_CMC`; its children inherit it. Start values are below; the user check tunes them.
- **Tests.**
  - Native tests for the profile math (taper endpoints, braking with and without input, friction taper, turn rate per gait) and for saved-move replay with a profile.
  - An asset contract that `BP_RpgGasp_CMC` references a valid profile. `SpeedParity` reads its speeds from the profile.
  - PIE measurements of start, stop, sprint exit, and jump against the values the profile predicts, including load changes.

### Start values

From the plan, Relaxed set assumed. Rows marked with a decision follow its outcome.

| Value | `BP_RpgGasp_CMC` today | Start |
| --- | --- | --- |
| Run / sprint / crouch | 375 / 585 / 200 | unchanged |
| Acceleration | 1500 | 750, tapering to 350 between run and sprint speed |
| Braking without / with input | 2000 / 2000 | 1300 / 500 |
| Braking friction | separate 6 × 1 | factor 0 |
| Ground friction | 8 | 4, tapering to 3 while sprinting |
| Free-movement turn: ground / sprint | instant / instant | decision 2 |
| Air turn rate | 200°/s | 150°/s |
| Air control / falling deceleration | 0.35 / 1500 | 0.15 / 0 |
| Jump Z | 500 | decision 1 |

Predicted on flat ground at these values:

| Motion | Today | Start values |
| --- | --- | --- |
| Start to 95 % run speed | 0.24 s, measured | 0.47 s |
| Stop from run | 0.125 s / 18.5 cm, measured | 0.29 s / 54 cm |
| Stop from sprint | – | 0.45 s / 132 cm |
| Sprint release to run speed (input held) | – | 0.42 s |
| Jump apex | 128 cm | 103 cm at 450 cm/s, 128 cm at 500 cm/s |

## Out of scope

- The walk gait and analog threshold (`GASP-12`).
- Presentation (`GASP-13`): Offset Root Bone, steering, camera lag, and Foley.
- New speeds or directional speeds; `GASP-10` set the speeds, and this package only moves them.
- Mover, Mover-Ragdoll, the baseline pawn `BP_Rpg_Character`, and AI characters; the native constructor defaults stay as their values.
- Sprint stamina cost and regeneration (GAS effects), and the dodge ([#124](https://github.com/Athurito/SurvivalRpg/issues/124)).
- Traversal montages and root-motion abilities, which do not use acceleration or braking.
- Any load dependence: load still only selects the dodge ([game vision](game-vision.md)).

## Decisions to settle at the start

The implementing chat settles these with the user before Phase B and records the answers here.

1. **Jump height.**
   - Recommended: **450 cm/s**, an apex of about 1.03 m instead of 1.28 m today and in GASP. The jump feels calmer and more grounded; mantle and vault cover higher obstacles.
   - Alternative: keep **500 cm/s**.
2. **Turning in free movement.**
   - Recommended: **stay instant on the ground**, as GASP and `GASP-09` do.
     - The profile still holds the rates, so they can be tuned later.
     - The width of a running turn comes from ground friction, which bends the velocity toward the input; it does not come from the capsule's turn rate.
     - The visible turn gets heavier in `GASP-13` through Offset Root Bone.
     - A slow capsule would drift from the Motion Matching trajectory, which turns with its own speed.
   - Alternative: **finite rates** from the plan, 400°/s on the ground and 220°/s while sprinting.
     - The character visibly lags behind its input in turns.
     - The trajectory's `RotateTowardsMovementSpeed` in `ABP_RpgGasp_CMC` must then match, and the probe must check foot sliding in turns.

## Ownership

| Responsibility | Location | Reason |
| --- | --- | --- |
| Profile schema, ranges, validation | C++ `URpgMovementProfile` | Stable designer-facing contract, read on the movement hot path by owner, server, and replay |
| Applying the profile | C++ overrides in `URpgCharacterMovementComponent` | Prediction, saved-move replay, and server correction |
| Concrete values | Profile DataAsset referenced by `BP_RpgGasp_CMC` | Tuning, not mechanism |
| Sprint authorization and stamina | GAS, unchanged since `GASP-07` | Gameplay-owned |
| Trajectory turn speed | `ABP_RpgGasp_CMC`, only if decision 2 picks finite rates | Presentation |
| Tests | Native tests next to the component tests; asset and PIE tests in `SurvivalRpgEditor/Private/Network` | Stable seams and asset contracts |

No replication is added. The profile is a class default, so owner, server, simulated proxies, and late joiners use the same asset.

File ownership while the package runs:
- `RpgCharacterMovementComponent.*`, the new `RpgMovementProfile.*`, and `RpgAbilityTask_Sprint.*` (only the source of the sprint speed);
- the CMC movement tests `RpgCharacterMovementComponentTests.cpp`, `RpgCmcSprintTests.cpp`, and `RpgCmcRotationModeTests.cpp`;
- the movement defaults of `BP_RpgGasp_CMC` and the new profile asset;
- `RpgGaspCMCSpeedParityTests.cpp` and the CMC PIE tests;
- the trajectory settings of `ABP_RpgGasp_CMC`, only if decision 2 picks finite rates;
- this report.

`GASP-12` and `GASP-13` touch the same pawn and start only after this package merges.

## Phases

1. **A – baseline.**
   - Settle the two decisions with the user.
   - On UE 5.8.3, measure with a PIE probe at 30, 60, and 120 FPS:
     - start to 95 % of run and sprint speed;
     - stop time and distance from run and from sprint;
     - sprint release back to run speed, a 180° turn, the jump apex, and air drift;
     - corrections as in `GASP-10`.
   - Confirm every reader of the moved values, including the trajectory fields.
2. **B – schema and application.** Add the native schema, the overrides, and the sprint speed source, with native tests and a build.
3. **C – asset.** Create and assign the profile, remove the duplicate Blueprint overrides, and update the contract tests.
4. **D – validation.** Run the automation, the PIE probe against the predicted values, and the network and load checks, then the user check with tuning.

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| One source | The CMC GASP pawn and its children take every value listed in the scope from one profile. A contract test fails without a valid profile. The baseline pawn and AI keep their values. |
| Start and stop | On the owner at 30, 60, and 120 FPS, these match the values the profile predicts within one frame and 5 % of the distance: time to 95 % run speed, stop time and distance from run and from sprint, and the sprint release. |
| Turning | Free movement turns as decision 2 records. The combat stance turns instantly. Attack facing and block alignment are unchanged; the `GASP-09` tests pass. |
| Air and jump | The jump apex lies within 5 % of JumpZ² / 2g. Air control and falling deceleration follow the profile. |
| Speed band | `SpeedParity` passes with the speeds read from the profile. |
| Network | Owner and server agree under saved-move replay and forced corrections (`p.NetForceClientAdjustmentPercent`). With `p.NetShowCorrections 1`, corrections do not rise above the `GASP-10` baseline. Simulated proxies and late joiners move and animate the same. |
| Load | Equipping across 13 kg and 23 kg, idle and while moving, changes no effective movement value and no measured time. |
| Animation | Every frame has a Motion Matching selection. Starts, stops, and the sprint release select start and stop clips without new foot sliding, in the probe and in the user check. |
| Regression | Sprint, rotation-mode, block-locomotion, mantle, vault, and hurdle tests pass. Mover is unchanged. |

## Verification

- `python Build/Tools/Unreal/ue.py build` after the C++ changes.
- Focused automation through `ue.py test`:
  - `SurvivalRpg.Movement.CMC` and `SurvivalRpg.Movement.Acceleration`;
  - `SurvivalRpg.GASP.CMC`, including `Sprint` and `SpeedParity`;
  - `SurvivalRpg.Combat.Block.Prediction` and `SurvivalRpg.Combat.BlockLocomotion`;
  - `SurvivalRpg.GASP.Mantle`, `SurvivalRpg.GASP.Vault`, `SurvivalRpg.GASP.Hurdle`, and `SurvivalRpg.Network`;
  - `SurvivalRpg.GASP.Mover`, `SurvivalRpg.GASP.RuntimeRetarget`, and `SurvivalRpg.GASP.VariantComparison` as regressions;
  - the new profile tests.
- The Phase A PIE probe, repeated with the profile, and network runs with `p.NetShowCorrections 1` and forced client adjustments.
- A user check on `Lvl_RpgGaspCMC` and `Lvl_RpgGaspMantle`, with tuning by feel.

## Repro (after implementation)

1. Start PIE on `Lvl_RpgGaspCMC` and holster the weapon.
2. Run and let go. The character needs about half a second to reach running speed and slides about half a meter to a stop.
3. Sprint, then release sprint while still moving. The character slows smoothly back to running speed.
4. Jump and steer in the air.
5. Draw the weapon, then strafe and stop.
6. Equip heavy items until the load tier changes. Movement stays the same.
7. On `Lvl_RpgGaspMantle`, mantle, vault, and hurdle, then keep running.

## Results

Not started.
