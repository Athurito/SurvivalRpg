# GASP-12 – Walk gait and analog input

Status: **PR open** in [#241](https://github.com/Athurito/SurvivalRpg/pull/241) on `claude/gasp-12-walk-gait` (defined 2026-10-13 in [#240](https://github.com/Athurito/SurvivalRpg/pull/240), implemented the same day). The user check is pending. The package outline is in the [movement plan](gasp-movement-plan.md#gasp-12-walk-gait-and-analog-input). This report refines it against the current code and assets and is the task report for the implementation PR.

## Goal

The CMC GASP pawn gets a third gait, Walk, at the speed of the Relaxed walk loops. A toggle selects it, and so does a lightly pushed stick (decision 2). The movement component owns one effective gait (Walk, Run, or Sprint). Movement, the AnimBP, and simulated proxies all read it.

## Why

- The CMC pawn knows only Run and Sprint. The [2026-10-11 review](gasp-movement-plan.md#findings-from-the-2026-10-11-review) found that partial stick input stays in the Run gait.
- Partial stick input scales the running speed, so the run loops play slower than authored and outside the 0.85–1.15 play-rate band. The CMC vault and hurdle tests use exactly this to approach obstacles at walking speed.
- The CMC chooser already contains the Relaxed walk databases, but nothing selects them, because the pawn never reports Walk.

## Current state (checked 2026-10-13 on `master` `8beb88b1`)

Read in the running editor and in the code.

| Area | Finding |
| --- | --- |
| Gait for the AnimBP | `BP_RpgGasp_CMC.Get_PropertiesForAnimation` reports `Gait` Sprint while `IsSprinting()` is true and Run otherwise. `WantsToWalk` is always false; nothing produces Walk. |
| Speed | `GetMaxSpeed` returns the GAS sprint speed while sprint is effective and otherwise `MaxWalkSpeed`, which the profile sets to `RunSpeed` 375. The block cap limits both. |
| Analog input | `URpgPawnGameplayComponent::Input_Move` passes the stick value to `AddMovementInput`, after a radial dead zone of 0.2 on `Gamepad_Left2D`. The stock CMC scales the maximum speed by the input strength, so speed follows the stick continuously down to `MinAnalogWalkSpeed` (20 cm/s). The sprint gate requires an input strength of at least `MinimumSprintInput` (0.7). |
| Prediction and replication | The sprint wish travels in `FLAG_Custom_0`; saved moves keep the sprint authorization. Only the effective sprint state replicates, to simulated proxies (`bIsSprinting`). |
| Proxy speed | Simulated proxies never pass the sprint gate, so their `GetMaxSpeed` is the run speed in every gait. The Pose Search trajectory calls `GetMaxSpeed()` and also uses the replicated acceleration (`ShouldReplicateAcceleration`). Phase A confirms what a sprinting proxy's trajectory predicts; this is from reading the code. |
| Chooser | `CHT_RpgGasp_CMC_PoseSearchDatabases` selects its nested `Stand Walks` chooser for `Gait` Walk. That chooser holds Relaxed walk starts, loops, pivots, and spins in F, B, LL, and RL, plus turns, `FromTraversal`, light and heavy lands, and the slide exit to walk. `Stand Runs` also reads `Gait_LastFrame`. |
| Loop speeds | The Relaxed walk loops move at 165 cm/s in every direction (`GASP-10`). |
| Input | The CMC pawns share `DA_InputConfig_GaspCMC`. `BP_Rpg_PlayerController` adds the shared `IMC_Movement` for every pawn; there, Ctrl toggles crouch, and the controller suppresses crouch while Ctrl is still held from a menu. Sprint is Shift and the gamepad's left shoulder in `IMC_RpgGasp_CMC`. C, Caps Lock, and the gamepad's right shoulder are free in gameplay; the right shoulder only switches tabs in menus. |
| Mover | The Mover pawn walks with Ctrl or the gamepad's right shoulder through the shared `IA_Walk` and crouches with C, as in GASP. It is not part of this package. |
| Traversal | The mantle pawn validates traversal entries by approach speed, in bands of 0–100, 100–250, and from 250 cm/s, with a network tolerance of 75 cm/s. Walking at 165 cm/s lies in the walking band. `SurvivalRpg.GASP.Vault` and `SurvivalRpg.GASP.Hurdle` approach with a half-pushed stick. |

## Original GASP

Read from the hash-verified export of `SandboxCharacter_CMC` (2026-09-28):

- `IA_Walk` on Ctrl and the gamepad's right shoulder toggles `WantsToWalk`.
- `GetDesiredGait` returns:
  - Sprint while `CanSprint` holds and the input is full;
  - otherwise Walk while `WantsToWalk` is set;
  - otherwise Run with full input and Walk without.
- `MovementStickMode` combines fixed or variable speed with a single gait or walk and run by stick deflection. Fixed speed normalizes the stick, so speed depends only on the gait. `AnalogWalk/RunThreshold` is 0.7.
- The sample defaults to **Fixed Speed – Single Gait**: the stick always runs, and walking needs the toggle. The function's comment warns that choosing the gait by stick deflection can disturb Motion Matching selection.
- `UpdateMovement_PreCMC` writes the gait's speed into `MaxWalkSpeed` every tick, which the [design rules](gasp-movement-plan.md#design-rules) exclude.

## Scope

- **Effective gait.** `URpgCharacterMovementComponent` resolves one gait per move:
  - Sprint wherever the sprint gate allows it today.
  - Otherwise Walk while the walk wish is set or the stick is below the threshold (decision 2).
  - Otherwise Run.
- **Prediction.**
  - The walk wish is a toggle that the owner sends in a compressed flag, like the sprint wish; saved moves and replay keep it. Walking needs no authorization or cost, so the server accepts the flag.
  - The stick's strength comes from the move's own acceleration before any scaling, so owner, replay, and server derive the same gait. The sprint gate reads the same value.
  - Partial input moves at the gait's full speed: the stick's strength only selects the gait, and neither acceleration nor maximum speed scales with it. Steady movement therefore always plays the loops at their authored speed.
  - Crouch keeps its own speed and ignores the walk wish. The block cap (157 cm/s) stays below the walk speed. Root motion and traversal keep their rules.
  - All of this applies only with a movement profile. Without one, as on the baseline pawn and AI, nothing changes.
- **Speed.** The profile gains `WalkSpeed`, 165 cm/s. `GetMaxSpeed` returns the speed of the effective gait, and the block cap still applies.
- **Read model and replication.**
  - A `BlueprintPure` gait getter replaces `IsSprinting()` as the AnimBP's source. `IsSprinting()` stays for GAS and the tests.
  - One replicated effective gait replaces `bIsSprinting` for simulated proxies, so late joiners receive it with the initial state.
  - On simulated proxies, `GetMaxSpeed` returns the replicated gait's speed, so their trajectory predicts walking and sprinting at the right speed.
- **Input.**
  - A native `InputTag.Walk` and a toggle handler in `URpgPawnGameplayComponent`, next to crouch.
  - A CMC input action on Caps Lock (decision 1) and the gamepad's right shoulder, mapped in `IMC_RpgGasp_CMC` and listed in `DA_InputConfig_GaspCMC`.
  - The toggle persists through sprint, crouch, block, and traversal; holding sprint overrides it. A new pawn starts running.
- **Blueprint.** `Get_PropertiesForAnimation` maps the component's gait onto `E_Gait` and reports `WantsToWalk`. The AnimBP and the chooser need no change.
- **Tests.**
  - Native tests:
    - gait resolution: the toggle, the threshold, the sprint override, crouch, and block;
    - saved-move replay of the walk wish;
    - the server's stick gait from a received acceleration;
    - full gait speed at partial input;
    - the profile's walk speed.
  - `SpeedParity` adds the walk loops (F, B, LL, RL) and requires them to be reachable through the chooser.
  - PIE tests: the walk toggle and partial stick on owner, listen server, simulated proxy, and late joiner, with corrections; vault and hurdle from a walk.

### Start values and predictions

| Value | Start |
| --- | --- |
| Walk speed | 165 cm/s, play rate 1.00 on the Relaxed walk loops |
| Stick threshold | 0.7 after the 0.2 dead zone, about three quarters of the stick's travel; the existing `MinimumSprintInput`, shared with the sprint gate |

Predicted on flat ground with the `GASP-11` profile:

| Motion | Prediction |
| --- | --- |
| Start to 95 % walk speed | 0.21 s |
| Stop from walk | 0.13 s / 10 cm |
| Walk to 95 % run speed (toggle off) | 0.26 s |
| Run to walk speed (toggle on, input held) | 0.42 s |
| Partly pushed stick | walks at 165 cm/s; today a matching fraction of 375 cm/s on the run loops |

## Out of scope

- Presentation (`GASP-13`): walk leans, Foley, and camera.
- Run, sprint, and crouch speeds, acceleration, braking, and turning (`GASP-11`).
- Mover, Mover-Ragdoll, the baseline pawn `BP_Rpg_Character`, and AI characters.
- Walking by holding a key, and speed steps between the gaits.
- Sprint stamina rules. Walking costs nothing and regenerates stamina like running.
- Stealth or noise effects of walking.
- Traversal montage selection, which stays speed-based.
- Any load dependence: every load tier can walk, at the same speed.

## Decisions to settle at the start

The user settled both on 2026-10-13 ("beide empfohlen"): the walk toggle is on **Caps Lock** with crouch staying on Ctrl, and a stick **below the threshold walks**.

1. **Keyboard key for the walk toggle.**
   - Recommended: **Caps Lock**, with crouch staying on Ctrl.
     - Crouch keeps its key and its menu safeguard.
     - The shared `IMC_Movement`, and with it the baseline pawn, stays unchanged.
     - Caps Lock is a common walk/run toggle in RPGs.
   - Alternative: **Ctrl walks and crouch moves to C**, as in GASP and the project's Mover.
     - Both variants share one layout.
     - The shared `IMC_Movement` changes for every pawn, and the menu safeguard moves from crouching to walking.
   - Either way the gamepad toggles walking with the right shoulder, as in GASP.
2. **Partly pushed stick.**
   - Recommended: **walk below the threshold**, GASP's Fixed Speed – Walk/Run. Below 0.7 the character walks at 165 cm/s; from 0.7 it runs at 375 cm/s. Speed depends only on the gait, so the loops always play at their authored speed.
   - Alternative: **the stick always runs**, GASP's default Fixed Speed – Single Gait. Any deflection runs at 375 cm/s, and walking needs the toggle. GASP calls this the safest option for Motion Matching.
   - Today's continuous speed is not offered, because it leaves the play-rate band.

## Ownership

| Responsibility | Location | Reason |
| --- | --- | --- |
| Gait resolution, walk wish, gait speeds, replication | C++ `URpgCharacterMovementComponent` | Prediction, saved-move replay, the server's derivation, and proxy state |
| Walk speed | `URpgMovementProfile` field, value in `DA_RpgGasp_MovementProfile` | Tuning on the shared profile |
| Stick threshold | `MinimumSprintInput` on the component | A gameplay rule shared with the sprint gate; `GASP-11` kept the sprint gates on the component |
| Walk toggle | Native `InputTag.Walk` and handler in `URpgPawnGameplayComponent`; input action, mapping, and input-config entry as assets | The same native binding seam as crouch; keys are designer content |
| Gait for the AnimBP | `BP_RpgGasp_CMC.Get_PropertiesForAnimation` | Presentation mapping onto the GASP enum |
| Database selection | The existing CMC chooser, unchanged | Content |
| Tests | Native tests next to the CMC tests; asset and PIE tests in `SurvivalRpgEditor/Private/Network` | Stable seams and asset contracts |

File ownership while the package runs:
- `RpgCharacterMovementComponent.*`, `RpgMovementProfile.*`, `RpgPawnGameplayComponent.*`, and the gameplay tag declaration;
- `BP_RpgGasp_CMC`, `DA_RpgGasp_MovementProfile`, `DA_InputConfig_GaspCMC`, `IMC_RpgGasp_CMC`, and the new walk input action;
- the CMC movement and profile tests, `RpgGaspCMCSpeedParityTests.cpp`, the CMC PIE tests, and the CMC vault and hurdle tests;
- this report.

`GASP-13` touches the same pawn and starts only after this package merges.

## Phases

1. **A – baseline.**
   - Settle the two decisions with the user.
   - On UE 5.8.3, measure in PIE at 30, 60, and 120 FPS:
     - speed, play rate, and selected database at stick strengths 0.3, 0.5, 0.7, and 1.0;
     - run–sprint transitions as the reference for the new gait changes;
     - corrections as in `GASP-11`.
   - In a listen-server PIE, record what a sprinting proxy's trajectory predicts, and find every reader of the sprint state.
2. **B – native.** Gait resolution, the walk wish flag, gait speeds, the profile's walk speed, the replicated gait, and the input tag and handler, with native tests and a build.
3. **C – assets.** Walk speed in the profile; the input action, its mappings, and its input-config entry; `Get_PropertiesForAnimation`; and the contract tests.
4. **D – validation.** Automation, the PIE probe against the predictions, a multiplayer probe with three players and a late joiner, and the user check.

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Gait selection | The toggle selects Walk, and so does a stick below the threshold. Holding sprint overrides Walk, and releasing it returns to Walk while the toggle is on. Crouch and block keep their speeds. |
| Speed band | Walking plays the Relaxed walk loops within 0.85–1.15 in F, B, LL, and RL. `SpeedParity` checks walk next to run, sprint, and crouch. In steady movement, no stick position plays a loop outside the band. |
| Transitions | Walk start, stop, walk to run, and run to walk match the predictions within one frame at 30, 60, and 120 FPS. Every frame has a Motion Matching selection, and walk starts and stops select walk clips. A stick held at the threshold does not switch back and forth between the gaits. |
| Network | Owner, listen server, simulated proxy, and late joiner show the same gait, at most about a frame apart, as in the `GASP-11` multiplayer probe. Saved-move replay and forced corrections keep the walk wish, and the server derives the stick gait from the received acceleration. With `p.NetShowCorrections 1`, corrections do not rise above the `GASP-11` baseline. A walking proxy's trajectory predicts walking speed. |
| Traversal | Mantle, vault, and hurdle work from a walk, with the toggle and with a partly pushed stick, and return to walking. |
| Load | Gaits and their speeds do not depend on the load tier. |
| Regression | Sprint, rotation-mode, profile, block-locomotion, mantle, vault, and hurdle tests pass. The baseline pawn, AI, and Mover are unchanged. |

## Verification

- `python Build/Tools/Unreal/ue.py build` after the C++ changes.
- Focused automation through `ue.py test`:
  - `SurvivalRpg.Movement.CMC`, including the profile, sprint, rotation-mode, and new gait tests;
  - `SurvivalRpg.GASP.CMC`, including `SpeedParity` and the sprint network tests;
  - `SurvivalRpg.Combat.Block.Prediction` and `SurvivalRpg.Combat.BlockLocomotion`;
  - `SurvivalRpg.GASP.Mantle`, `SurvivalRpg.GASP.Vault`, `SurvivalRpg.GASP.Hurdle`, and `SurvivalRpg.Network`;
  - `SurvivalRpg.GASP.Mover`, `SurvivalRpg.GASP.RuntimeRetarget`, and `SurvivalRpg.GASP.VariantComparison` as regressions.
- The Phase A PIE probe, repeated with the change, and network runs with `p.NetShowCorrections 1` and forced client adjustments.
- A multiplayer probe: listen server with three players, recording the walking pawn as owner, on the server, and as a proxy on the other client, plus a late joiner.
- A user check on `Lvl_RpgGaspCMC` and `Lvl_RpgGaspMantle`, alone and with two players.

## Repro (after implementation)

1. Start PIE on `Lvl_RpgGaspCMC` and holster the weapon by pressing its action-bar slot again.
2. Press Caps Lock, or the gamepad's right shoulder, and move: the character walks. Press it again: it runs.
3. On a gamepad, push the stick slightly, then fully: walk, then run.
4. While walking, hold sprint: the character sprints. Release sprint: it walks again.
5. Draw the weapon and walk in every direction.
6. On `Lvl_RpgGaspMantle`, walk into the mantle, vault, and hurdle obstacles.
7. Repeat steps 2 to 4 in PIE with two or three players and watch the other character: it walks and runs at the same moments.

## Results

Implemented on 2026-10-13 on `claude/gasp-12-walk-gait`.

### Baseline (Phase A)

Measured on `master` `d0bddda7` with UE 5.8.3 before any change. The [manifest](assets/gasp-12-walk-gait.json) holds every value.

- **Partial stick.** At input strengths of 0.3, 0.5, 0.65, 0.75, and 1.0 after the dead zone, the pawn ran at 112.5, 187.5, 243.8, 281.2, and 375 cm/s, always in the Run gait and on Run clips. The run loops therefore played at 0.30 to 0.75 of their authored speed. The values were the same at 30, 60, and 120 FPS. With sprint held, a half-pushed stick kept running at 187.5 cm/s.
- **Proxy trajectory.** In a listen-server PIE with three players, a sprinting simulated proxy moved at 585 cm/s. Its `GetMaxSpeed` and its trajectory's future speed were 375 cm/s, while owner and server predicted 585. This confirms the finding from reading the code.
- **Sprint state readers.** `IsSprinting` is read by the sprint stamina task, the AnimBP mapping, and the tests. `RefreshSprintState` is called when the GAS lease ends.
- **Corrections.** The runtime code had not changed since `GASP-11` (only documentation was merged), so the `GASP-11` counts serve as the baseline: 0 for the stance test, and 20, 20, 0, and 0 for the four sprint tests.

### Implementation

- **Movement component** (`URpgCharacterMovementComponent`):
  - `ERpgMovementGait` (Walk, Run, Sprint) is the effective gait of a move. A replicated `Gait` replaces `bIsSprinting` for simulated proxies and late joiners. `GetGait()` is the AnimBP's read model, and `IsSprinting()` stays.
  - `ToggleWalk()` flips the owner's walk wish. `ControlledCharacterMove` reads the stick's strength on the owner: below `MinimumSprintInput` (0.7) the stick walks, and a running stick falls back to walking only below 0.65 (`WalkInputHysteresis`). Released input keeps the last choice, so stops keep their gait.
  - With a profile, `ScaleInputAcceleration` normalizes every input to full acceleration, as GASP's fixed-speed stick modes do. Each gait therefore moves at its own fixed speed.
  - Saved moves carry the walk toggle in `FLAG_Custom_1` and the stick's walk choice in `FLAG_Custom_2`. Replay restores both, the server reads both from the received move, and moves with different choices never combine.
  - `GetMaxSpeed` returns the sprint speed, else the profile's `WalkSpeed` while the move walks, else the run or crouch speed; the block cap still applies. Walking applies on the ground and while falling, never crouched. A weak stick cannot sprint; the walk toggle yields to sprint.
  - On simulated proxies, `GetMaxSpeed` returns the speed of the replicated gait, so the Pose Search trajectory predicts walking and sprinting correctly.
- **Profile.** `URpgMovementProfile::WalkSpeed` is 165 cm/s and must lie below `RunSpeed`. `DA_RpgGasp_MovementProfile` keeps the class default, so the asset did not change.
- **Input.**
  - The native tag `InputTag.Walk` is bound in `URpgPawnGameplayComponent::Input_Walk`, which toggles walking and ignores the press while a menu owns input.
  - `ARpgPlayerController::IsMenuInputActive` now holds the CommonUI menu check that crouch already used.
  - `IMC_RpgGasp_CMC` maps the shared `IA_Walk` to Caps Lock and the gamepad's right shoulder, and `DA_InputConfig_GaspCMC` binds it to `InputTag.Walk`. Crouch stays on Ctrl.
- **Blueprint.** `BP_RpgGasp_CMC.Get_PropertiesForAnimation` reports `Gait` as Sprint while sprinting, otherwise Walk when `GetGait()` is Walk, otherwise Run, and `WantsToWalk` from `IsWalkToggled()`. The AnimBP and its chooser did not change; the chooser already selects the Relaxed walk databases for Walk.
- **Tests.**
  - Five native tests `SurvivalRpg.Movement.CMC.Gait.*` cover:
    - toggle and stick timings at 30, 60, and 120 FPS against the profile;
    - hysteresis, sprint, crouch, and block priority;
    - saved moves and replay;
    - the server's reading of received flags;
    - the proxy's speed per replicated gait.
  - The profile test also rejects a walk speed at or above run speed.
  - `SpeedParity` adds the walk loops F, B, LL, and RL.
  - The CMC sprint PIE test checks the effective gait in every observation window and adds `WalkToggleAndWeakStickWalkOnEveryRoleAndLateJoin`. It drives the real Caps Lock and the mapped left stick through a late join, a sprint override, the toggle off, a weak stick with sprint held, and a full stick.
  - The mantle, vault, and hurdle PIE tests each add a walk-toggle approach.
  - `URpgEditorPlaytestTools::SetPIEInputAxis` sends analog stick values to a PIE controller for probes.

### PIE probe

Free movement on `Lvl_RpgGaspCMC`, one player, after the change. Strength is the input after the 0.2 dead zone.

| Stick strength | 0.3 | 0.5 | 0.65 | 0.75 | 1.0 |
| --- | --- | --- | --- | --- | --- |
| Before: speed, gait | 112.5, Run | 187.5, Run | 243.8, Run | 281.2, Run | 375, Run |
| Before: play rate on the run loops | 0.30 | 0.50 | 0.65 | 0.75 | 1.00 |
| After: speed, gait | 165, Walk | 165, Walk | 165, Walk | 375, Run | 375, Run |
| After: play rate | 1.00 | 1.00 | 1.00 | 1.00 | 1.00 |

- Steady values are identical at 30, 60, and 120 FPS. Walking selects the Relaxed walk databases.
- With sprint held, a half-pushed stick walks at 165 cm/s instead of running at 187.5. A full stick sprints at 585.

| Motion | 30 / 60 / 120 FPS | Prediction |
| --- | --- | --- |
| Toggle: start to 95 % walk speed | 0.233 / 0.217 / 0.217 s | 0.209 s |
| Weak stick: start to 95 % walk speed | 0.233 / 0.217 / 0.217 s | 0.209 s |
| Stop from walk | 0.133 s, 10.6 cm / 0.133 s, 10.5 cm / 0.120 s, 10.4 cm | 0.127 s, 10.5 cm |
| Toggle off: walk to 95 % run speed | 0.267 / 0.267 / 0.261 s | 0.255 s |
| Toggle on while running: run to walk speed | 0.433 / 0.433 / 0.422 s | 0.420 s |

- Every time lies within one frame of the prediction, and every distance within a few millimetres.
- Every frame has a Motion Matching selection.
- Walk starts select `Walk_Start` or `Walk_Reface_Start` and stops `Walk_Stop`. Switching from run to walk, and back to the toggled walk after sprinting, selects `Transition_Run_to_Walk`. Turning the toggle off while walking selects `Run_Start`.

### Multiplayer probe

Listen-server PIE with three players at 60 FPS, without latency. Client A walked in the combat stance; A's pawn was recorded as the owner on A, as the authority on the server, and as a simulated proxy on client B.

| Motion | Owner (A) | Server | Proxy (B) |
| --- | --- | --- | --- |
| Half-pushed stick: start to 95 % walk speed | 0.217 s | 0.234 s | 0.234 s |
| Toggle: start to 95 % walk speed, backward | 0.217 s | 0.233 s | 0.252 s |
| Toggle off: to 95 % run speed | 0.257 s | 0.273 s | 0.273 s |
| Run to 95 % sprint speed | 0.321 s | 0.337 s | 0.337 s |
| Gait switch after the input | 0.017 s | 0.033 s | 0.033 s |
| `GetMaxSpeed` and trajectory future speed while sprinting | 585 | 585 | 585 (before: 375) |

- All roles walk at 165 cm/s with the same walk clips. Roles disagreed on the gait in 6 of 1391 ticks: one frame at each of three switches.
- The proxy stayed within 0.0 cm of the owner at the median, 0.4 cm at p95, and 1.2 cm at most. The server stayed within 2.8 cm at the median and 11.5 cm at most.
- Every frame on every role has a Motion Matching selection.

### Automation

Every suite ran on its own through `ue.py test`; the runs are listed in order.

| Filter | Result |
| --- | --- |
| `SurvivalRpg.Movement` with `--null-rhi`, during development | 24 run, 23 passed. The server test ran an unpossessed pawn, whose walking physics the CMC skips without a controller. With `bRunPhysicsWithNoController` set in the test, `SurvivalRpg.Movement.CMC` passed 21/21. |
| `SurvivalRpg.GASP.CMC` with `p.NetShowCorrections 1` | 11/11 passed (10 before plus the new gait scenario, which includes `SpeedParity`) |
| `SurvivalRpg.GASP.CMC` with `p.NetForceClientAdjustmentPercent 0.1` | 11/11 passed |
| Native and asset filters: `SurvivalRpg.Movement`, `Equipment`, `Harvesting`, `AbilitySystem`, the combat contracts, the CMC, sprint, and mantle asset tests, `SurvivalRpg.Input`, and `SurvivalRpg.UI.Input` | 135/135 passed |
| `SurvivalRpg.Combat.BlockLocomotion` | 16/16 passed |
| `SurvivalRpg.GASP.Mantle` | Crashed in the shared run during the cleanup of the existing `NegativeAngledApproachAlignsBodyWithoutTurningView`: an engine assertion in `FSceneViewport::Destroy` on the render thread, as once in `GASP-09`. Rerun alone: 13/13 passed, including the new walk-toggle approach. |
| `SurvivalRpg.GASP.Vault` | 9/9 passed, including the new walk-toggle approach |
| `SurvivalRpg.GASP.Hurdle` | 12/12 passed, including the new walk-toggle approach |
| `SurvivalRpg.GASP.RuntimeRetarget` / `VariantComparison` | 2/2, 3/3 passed |
| `SurvivalRpg.Network` | 19/19 passed |
| `SurvivalRpg.GASP.Mover` | 65/67. `Ragdoll...CancelledGetupCannotCompleteTheNextRagdollEpisode` failed on unexpected ragdoll bone-data warnings, and `Vault...LateJoinReconstructsTheCurrentVaultState` on a late-join condition. Both passed when rerun alone. The Mover pawn does not use the changed component, and no Mover asset changed. |

- **Mutation checks.** One build disabled the input normalization, the hysteresis band, and the proxy gait speed at once. `SurvivalRpg.Movement.CMC.Gait` then failed in exactly the three matching tests:
  - the weak stick did not reach walking speed within 3 s instead of 0.21 s;
  - a running stick inside the band walked;
  - the proxy predicted 375 instead of 165 and 585 cm/s.

  With the code restored, 5/5 passed.
- **Corrections.** Counted per test from `Error for` lines with `p.NetShowCorrections 1`, as in `GASP-11`:
  - the two `GaspCMCExperiencePIE` tests log 0;
  - the four sprint tests log 22, 16, 0, and 0, against 20, 20, 0, and 0 in `GASP-11`;
  - the new gait scenario logs 4.
- **Gait scenario.** Every observation window matched on owner, server, and proxy: walk 165, sprint 585, and run 375 cm/s, with the expected gait and no stamina use outside sprint. The proxy joined late while the toggle was on and received Walk.

### Acceptance

| Area | Result |
| --- | --- |
| Gait selection | **Met.** The toggle and a stick below the threshold walk; sprint overrides the toggle, and releasing it walks again; crouch and block keep their speeds. Native tests and the PIE scenario check it with real keys and the mapped stick. |
| Speed band | **Met.** Walking plays the walk loops at 1.00; `SpeedParity` checks walk F, B, LL, and RL next to run, sprint, and crouch. With fixed gait speeds, every stick position plays its gait's loop at the authored speed. |
| Transitions | **Met.** Walk start, stop, walk to run, and run to walk lie within one frame of the prediction at 30, 60, and 120 FPS, natively and in PIE. Every frame has a selection, and walk starts and stops select walk clips. A stick between 0.65 and 0.7 keeps its gait; see the native hysteresis test. |
| Network | **Met.** Owner, server, proxy, and late joiner agree within about a frame. Replay, the server's flag reading, and forced corrections keep the walk wish. Correction counts stayed in the `GASP-10`/`GASP-11` range. Walking and sprinting proxies predict their trajectory with their gait's speed. |
| Traversal | **Met in automation.** Mantle, vault, and hurdle pass from a walk-toggle approach in the walking band, checked on owner, server, and proxy. Vault and hurdle also pass from a half-pushed stick, which now approaches at 165 cm/s. The return to walking after landing is part of the user check. |
| Load | **Met by construction.** Walk speed and gait selection read no load state; the `GASP-11` load test still passes. |
| Regression | **Met.** Every suite passes. The Mantle crash and the two Mover failures of the shared run passed when rerun alone. |

### Deviations

- **Stick strength on the server.** The definition derived the stick's walk choice on the server from the received acceleration.
  - Instead, the owner normalizes the input and sends its walk choice in a second flag (`FLAG_Custom_2`), as GASP normalizes the stick in its fixed-speed modes.
  - Otherwise the replicated acceleration and the Pose Search trajectory would keep the partial strength on every role. The trajectory would then predict a slower start than the fixed-speed movement performs.
  - Walking is never an advantage, so the server can accept the choice.
- **Hysteresis.** Phase A was to decide whether a band is needed. A stick at the threshold would otherwise flip between two target speeds every frame. So the owner-side choice uses a 0.05 band, `WalkInputHysteresis`.
- **Input action.** The CMC reuses the shared GASP `IA_Walk` (Boolean, Pressed trigger) that the Mover maps to Ctrl, instead of a new CMC action. Only `IMC_RpgGasp_CMC` maps it, so the keys do not collide.
- **Player controller.** `ARpgPlayerController` gained `IsMenuInputActive`, the existing CommonUI check that crouch used, so that the walk toggle ignores presses while a menu owns input. The definition did not list this file.
- **Mantle test.** The definition named vault and hurdle; the mantle PIE test also gained a walk-toggle approach.
- **Editor tool.** `URpgEditorPlaytestTools::SetPIEInputAxis` is new, for analog input in PIE probes.

### Failed attempts

- **Probe route.** The first stick probe ran every stick step forward and stopped at the level wall during the sprint step. The route now alternates direction.
- **PIE player count.** The first baseline ran with three PIE players, because the editor setting was set that way, so the 120 FPS run reached only about 60 FPS. The single-player probes were repeated with one player; the setting was restored to three players afterwards.
- **Multiplayer probe.** The first multiplayer baseline sprinted backward in the combat stance. The stance refuses that, as `GASP-09` requires, so the route now sprints forward.
- **Blueprint graph.**
  - Rewriting `Get_PropertiesForAnimation` through `write_graph_dsl` failed: the DSL read back from the graph does not round-trip (`GetVelocity` on the component). The graph was unchanged.
  - A select node indexed by `ERpgMovementGait` kept only two option pins, because the node edit does not rebuild the node, and the Blueprint failed to compile.
  - The final graph uses an enum equality with a bool select instead.
- **Native server test.** The first version unpossessed the pawn; the CMC skips walking physics without a controller, so the test failed until it enabled `bRunPhysicsWithNoController`.

### Limits

- **User check:** started on 2026-10-13, not yet accepted.
- **Listen-server view (found in the user check; already on `master` `d0bddda7`):**
  - When a client in the combat stance only turns the camera while standing, the host sees that client's character jitter.
  - A three-player probe measured the pelvis reversing direction in 10 to 65 of about 170 frames at 45 to 180°/s on the server, against 0 to 2 on the owner and on the other client. `master` without this package shows the same pattern.
  - With Offset Root Bone disabled (`a.animnode.offsetrootbone.enable 0`) the reversals disappear; disabling listen-server smoothing does not help.
  - The likely cause is the node's interaction with the server ticking a remote client's animation per received move (`bOnlyAllowAutonomousTickPose`). Not fixed in this package.
- **Network conditions:** the multiplayer probe and PIE tests ran locally without latency, jitter, or loss; that stays with `GASP-VAL-02`.
- **Hardware:** stick input came from simulated `Gamepad_LeftY` values through the real mapping and dead zone, not from a physical gamepad. Real stick noise near 0.7 is covered only by the 0.05 band.
- **Crouch:** a crouching pawn reports Run to the AnimBP even with the toggle on; the crouch databases ignore the gait.
- **Presentation:** the CMC keeps the Neutral run lean on walking poses; the Relaxed walk leans stay with `GASP-13`.
- **Shared regression run:** the Mantle suite crashed once in the engine (`FSceneViewport::Destroy` on the render thread), and two Mover cases failed once; all passed when rerun alone.
