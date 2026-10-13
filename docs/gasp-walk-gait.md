# GASP-12 – Walk gait and analog input

Status: **In progress** on `claude/gasp-12-walk-gait` (defined 2026-10-13 in [#240](https://github.com/Athurito/SurvivalRpg/pull/240), started the same day). The package outline is in the [movement plan](gasp-movement-plan.md#gasp-12-walk-gait-and-analog-input). This report refines it against the current code and assets and is the task report for the implementation PR.

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

Not started.
