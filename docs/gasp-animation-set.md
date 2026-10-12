# GASP-10 – Animation set and speed parity

Status: **Merged** ([#235](https://github.com/Athurito/SurvivalRpg/pull/235) as `daaab0e2` on 2026-10-12, final head `29f8a2f5`; implemented on `claude/gasp-10-animation-set`, first commit `dfaf3f30`). The user checked it on 2026-10-12 ("sieht gut aus") and asked for the merge; the automated checks pass. The package outline is in the [movement plan](gasp-movement-plan.md#gasp-10-animation-set-and-speed-parity). This report refines it against the current assets and code and is the task report for the implementation PR.

## Goal

Make every CMC target speed fit the animation set the CMC actually plays, so the feet do not slide and heavy movement is never faked by playback rate. Choose one set for the CMC; the choice also fixes which databases `GASP-14` bakes onto Manny.

## Why

The [2026-10-11 review](gasp-movement-plan.md#findings-from-the-2026-10-11-review) found that the CMC runs Relaxed speeds on Neutral clips:
- Run is 375 cm/s against Neutral loops of 500 forward, 350 sideways, and 300 backward. Forward running plays at 0.75× and backpedaling at 1.25×, both outside the 0.85–1.15 play-rate band.
- Sprint is 585 cm/s against a 700 cm/s Neutral loop.
- Crouch walk is 300 cm/s against Neutral crouch loops of 225 / 200 / 180.

Since `GASP-09`, the combat stance strafes and backpedals in every fight, so the directional mismatch is visible far more often than before.

## Current state (checked 2026-10-12 on `master` `3459d43f`)

The editor was not running, so asset findings come from the package name tables, not from opened assets. Phase A confirms them in the editor.

| Area | Finding |
| --- | --- |
| CMC database selection | `ABP_RpgGasp_CMC` evaluates the project-owned `CMC/RPG/Choosers/CHT_PoseSearchDatabases`, which references the `_Dense`, `_Sparse`, and `_ExtremeSparse` sub-choosers. The AnimBP reads the `DDCvar.MMDatabaseLOD` console variable. The Dense databases use the schema `PSS_Default` and the normalization set `PSN_Dense_All`; one database covers all directions per gait and phase, for example `PSD_Dense_Stand_Run_Loops`. |
| Relaxed set | 77 shared databases under `Shared/Characters/UEFN_Mannequin/Animations/MotionMatchingData/Databases/Relaxed`. Walk and run are split by direction (`F`, `B`, `LL`, `LR`, `RL`, `RR`) into loops, starts, pivots, and spins. Sprint, crouch, jumps, lands, `FromTraversal`, and slides are also covered. The set has its own schemas (`PSS_Relaxed_*`) and normalization set `PSN_Relaxed_All`. There are no Sparse or ExtremeSparse variants. |
| Relaxed chooser | The Mover evaluates `Mover/RPG/Choosers/CHT_RpgGasp_Mover_PoseSearchDatabases`, the project copy of the GASP sample's `CHT_PoseSearchDatabases_Relaxed`. In the sample, that chooser belongs to the Mover AnimBP, and the CMC AnimBP uses the Dense/Sparse chooser. The project copy has the context class `ABP_RpgGasp_Mover` and reads `MovementDirection_Recent` together with gait, stance, movement mode, and movement state. |
| AnimBP inputs | `ABP_RpgGasp_Mover` keeps a recent-state history: `Gait_Recent`, `Stance_Recent`, `MovementDirection_Recent`, `MovementDirection_Time`, `MovementDirection_LastStateTime`, and `RecentTimeLimit`. `ABP_RpgGasp_CMC` has `MovementDirection` and `MovementDirectionLastFrame` but none of the recent-state values. Switching the CMC to Relaxed is therefore more than a chooser swap. |
| Additive layers | The CMC AnimBP uses the Neutral `BS1D_Additive_Lean_Run`; the Mover AnimBP uses `BS_Relaxed_Run_Leans`, `BS_Relaxed_Walk_Leans`, and `BS_Relaxed_Lean_Head`. The only aim offset is the Neutral `BS_Neutral_AO_Stand`, which the CMC uses since `GASP-09`; there is no Relaxed aim offset. |
| Speeds | The native default `SprintSpeed` is 585 cm/s (`RpgCharacterMovementComponent.h`). The review measured run 375 and crouch 300. Which Blueprint sets them (`BP_Rpg_Character` or `BP_RpgGasp_CMC`) was not re-read because the editor was not running. The CMC has no walk gait yet (`GASP-12`). |
| Traversal | CMC mantle and hurdle play Neutral montages. The Mover owns Relaxed copies for mantle and hurdle. Vault is Neutral on both variants because the sample has no Relaxed vault ([Mover vault](gasp-mover-vault.md)). |
| Block locomotion | Its linked layer plays its own Sword-and-Shield clips at 157 cm/s, independent of the Motion Matching set ([block locomotion](gasp-block-locomotion.md)). |
| Manny pilot | `GASP-08` baked eight Dense databases (144 clips) into isolated copies under `Characters/GASP/Pilot`; this package does not touch them. |
| `GASP-09` evidence | The stance clips recorded in the [rotation-mode report](gasp-rotation-modes.md) are Neutral. The PIE network test checks `RotationMode` and `WantsToStrafe`, not clip names. |

## Scope

- **Animation set.** Implement the outcome of decision 1 for the CMC only.
- **Relaxed path** (if chosen):
  - A CMC-owned chooser with the context class `ABP_RpgGasp_CMC`, copied from the Mover chooser. It references the shared Relaxed databases read-only.
  - The Mover uses the same databases, so they stay unchanged. A CMC-specific change to a database needs a CMC-owned copy.
  - Port into `ABP_RpgGasp_CMC` the recent-state values the chooser reads, following the Mover AnimBP and staying inside the existing thread-safe update functions.
  - Use the Relaxed lean additives as the Mover AnimBP does. Keep the Neutral aim offset and include it in the user check.
  - Record that the `MMDatabaseLOD` switch no longer applies to the CMC, because the set has a single density.
- **Neutral path** (if chosen):
  - Directional run speeds of 500 / 350 / 300 and a 700 cm/s sprint.
  - This needs a direction-dependent maximum speed in `URpgCharacterMovementComponent`, decided per move like the sprint gate, so prediction, replay, and the server agree.
- **Speeds, either path:**
  - Set run, sprint, and crouch speeds to the measured loop speeds of the chosen set. Crouch is aligned to that set's crouch loops.
  - Record the walk target for `GASP-12` without adding a walk gait.
  - The values stay designer defaults on the CMC pawn; `GASP-11` later moves them into the shared movement profile.
- **Contract test.** Add a stable asset-contract test: the configured run, sprint, and crouch speeds lie within 0.85–1.15 of the chosen set's measured loop root speeds.

## Out of scope

- Acceleration, braking, friction, rotation rates, and air control (`GASP-11`).
- The walk gait and analog threshold (`GASP-12`); this package only records the walk target.
- Presentation polish (`GASP-13`).
- Mover, Mover-Ragdoll, the Manny pilot, and AI characters.
- Changes to the shared Relaxed or Dense databases.
- Traversal montages, unless decision 2 says otherwise.
- The Manny bake (`GASP-14`).
- Any load dependence: load still only selects the dodge ([game vision](game-vision.md)).

## Decisions

The user settled both decisions on 2026-10-12 with "merge, Relaxed, so lassen": the **Relaxed** set, and the traversal montages **stay** Neutral. The options as they were put to the user:

1. **Animation set.**
   - Recommended: **Relaxed.**
     - Run 375 cm/s in every direction and sprint 585 cm/s already match the current speeds.
     - The set is calmer and slower, which suits the heavy, grounded movement direction.
     - Its strafe and backpedal loops per direction serve the `GASP-09` combat stance.
     - The Mover already uses it.
     - Cost: the AnimBP work above, and no Sparse/ExtremeSparse variants.
   - Alternative: **Neutral.**
     - It keeps today's clips, with directional speeds 500 / 350 / 300 and a 700 cm/s sprint.
     - It is faster and more athletic.
     - It needs a direction-dependent speed in the movement component.
2. **Traversal montages with the Relaxed set.**
   - Recommended: keep the accepted Neutral mantle, vault, and hurdle montages on the CMC in this package, and judge the transitions in the user check. Switching mantle and hurdle to the Relaxed copies the Mover already owns would be its own package.
   - Alternative: switch mantle and hurdle within this package; vault stays Neutral either way.

## Ownership

| Responsibility | Location | Reason |
| --- | --- | --- |
| Database selection, recent-state inputs, lean additives | `ABP_RpgGasp_CMC` and CMC-owned choosers | Presentation; Motion Matching selection stays cosmetic and reads only game-thread snapshots |
| Run, sprint, and crouch speeds | Designer defaults on the CMC pawn Blueprint or its movement component | Tuning, not mechanism; `GASP-11` collapses them into one profile |
| Direction-dependent maximum speed (Neutral path only) | C++ in `URpgCharacterMovementComponent` | Per-move prediction, saved-move replay, and server correction |
| Speed-band contract test | C++ editor test next to `RpgGaspCMCIntegrationTests.cpp` | Stable asset contract between movement speeds and authored clips |

File ownership while the package runs:
- `ABP_RpgGasp_CMC` and the choosers under `CMC/RPG/Choosers`, including any new CMC-owned chooser;
- the speed defaults in `BP_RpgGasp_CMC` and, only for those values, `BP_Rpg_Character`;
- `RpgGaspCMCIntegrationTests.cpp` or a new test file next to it;
- `RpgCharacterMovementComponent.*`, only on the Neutral path;
- this report, plus an asset manifest under `docs/assets` if database membership changes.

`GASP-11` to `GASP-13` touch the same pawn and start only after this package merges.

## Phases

1. **A – baseline.**
   - Settle the two decisions with the user.
   - In the editor, confirm the chooser wiring and context classes from the table above, and find which Blueprint owns the run and crouch speeds.
   - Measure loop root speeds of the chosen set, including crouch, and repeat the review's play-rate measurement on UE 5.8.3 as the baseline.
2. **B – AnimBP and choosers.** Switch database selection, inputs, and additives (Relaxed path), or add the directional speed seam (Neutral path).
3. **C – speeds and contract test.** Set the speeds and add the speed-band test.
4. **D – validation.** Run the automation, the PIE probe, and the network checks, then the user check.

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Speed band | Each loop's target speed lies within 0.85–1.15 of its measured root speed: run forward, sideways, and backward; sprint; crouch forward, sideways, and backward. |
| Foot sliding | Starts, loops, stops, pivots, and turns are checked in the PIE probe through selected clip, play rate, and capsule speed, and in the user check. |
| Combat stance | Strafing and backpedaling select directional clips of the chosen set. The `GASP-09` rotation-mode tests pass. |
| Network | Owner, server, simulated proxy, and late joiner pick from the same set. Motion Matching stays cosmetic, and movement corrections do not increase compared with the baseline. |
| Traversal | Mantle, vault, and hurdle automation passes, and the return to locomotion after traversal works. |
| Block | The block locomotion tests pass. |
| Unchanged | Mover, Mover-Ragdoll, the Manny pilot, and AI move as before. The shared databases are unchanged, checked by hash. |
| Performance | Animation-thread cost is measured against the baseline, because the Relaxed set has more databases and no LOD variants. |

## Verification

- `python Build/Tools/Unreal/ue.py build` after any C++ change.
- Focused automation through `ue.py test`:
  - `SurvivalRpg.GASP.CMC`, including `SurvivalRpg.GASP.CMC.Sprint`;
  - `SurvivalRpg.Movement.CMC`, including the rotation-mode tests;
  - `SurvivalRpg.Combat.BlockLocomotion`;
  - `SurvivalRpg.GASP.Mantle`, `SurvivalRpg.GASP.Vault`, and `SurvivalRpg.GASP.Hurdle`;
  - `SurvivalRpg.GASP.Mover`, `SurvivalRpg.GASP.RuntimeRetarget`, and `SurvivalRpg.GASP.VariantComparison` as regressions;
  - the new speed-band test.
- A PIE probe on `Lvl_RpgGaspCMC` that records the selected database, clip, and speed ratio per direction and gait at 30, 60, and 120 FPS.
- A user check on `Lvl_RpgGaspCMC`: run, stop, and turn holstered; strafe, backpedal, and sprint with a weapon; crouch-walk in every direction; and mantle, vault, and hurdle back into running.

## Repro (after implementation)

1. Start PIE on `Lvl_RpgGaspCMC`.
2. Holster the weapon. Run, stop, and turn: the feet do not slide.
3. Draw a weapon. Strafe and backpedal: the character plays directional clips at matching speed. Sprint forward.
4. Crouch and walk in every direction.
5. Mantle, vault, and hurdle, then keep running.

## Phase A findings

Read in the editor on 2026-10-12; the [manifest](assets/gasp-10-animation-set.json) holds the measured values.

| Area | Finding |
| --- | --- |
| Speed owners | `BP_RpgGasp_CMC` sets `MaxWalkSpeed` 375. `MaxWalkSpeedCrouched` 300 was inherited from `BP_Rpg_Character`. `SprintSpeed` 585 is the native default. |
| Database selection | Confirmed as in the table above: `Update_MotionMatching` evaluated `CHT_PoseSearchDatabases` with the AnimBP as context, and its `MMDatabaseLOD` column picked Dense, Sparse, or ExtremeSparse. |
| Movement direction on the CMC | `Update_MovementDirection` compares the trajectory's future velocity with the `OrientationIntent` snapshot. It returns F in `OrientToMovement` or while sprinting, applies thresholds with hysteresis, and resolves left and right through `MovementDirectionBias`; the default `LeftFootForward` gives LL and RL. `Update_Logic` called it only with `UseExperimentalStateMachine`, so in normal play the direction stayed F. Only the experimental state machine read it. |
| Movement direction on the Mover | The Mover pawn computes the direction in `Get_MovementDirectionAndOffset` with the same thresholds and returns F, B, LL, or RL, the four values the Relaxed chooser has rows for. The Mover AnimBP debounces it into `MovementDirection_Recent` through its `UpdateStateValues` macro with a 0.1 s limit. |
| Loop root speeds | Relaxed: run 375 and walk 165 cm/s in every direction (F, B, LL, LR, RL, RR), sprint 585. Neutral Dense: run 500 F, 350 sideways, 300 B; sprint 700. |
| Crouch | The Relaxed crouch databases contain the Neutral crouch loops: 225 F, 200 sideways, 180 B. One crouch speed fits all three only between 191 and 207 cm/s. |
| Baseline play rate | PIE probe at 60 FPS, capsule speed over the selected loop's root speed: forward 0.75, backward 1.25, sideways 1.07, crouch 1.33. |

## Implementation

- **Chooser.** `CMC/RPG/Choosers/CHT_RpgGasp_CMC_PoseSearchDatabases` is a copy of the Mover chooser. Its context class is remapped to `ABP_RpgGasp_CMC`. It references the shared Relaxed databases read-only; no database changed.
- **AnimBP** (`ABP_RpgGasp_CMC`):
  - `Update_MotionMatching` evaluates the new chooser.
  - `Update_Logic` runs `Update_MovementDirection` on every update, then tracks the recent direction like the Mover's `UpdateStateValues`. A direction change resets `MovementDirection_Time`. After 0.1 s in one direction, `MovementDirection_Recent` takes the value. `Update_TargetRotation` stays behind `UseExperimentalStateMachine`.
  - New variables `MovementDirection_Recent` and `MovementDirection_Time`, category States.
  - The logic is inline in the existing thread-safe `Update_Logic`, because Python cannot mark a new Blueprint function thread safe.
  - The `MovementDirectionBias` default stays `LeftFootForward`, which already yields the LL and RL values the chooser expects.
- **Speeds.** `BP_RpgGasp_CMC` sets `MaxWalkSpeedCrouched` to 200 cm/s. Run 375 and sprint 585 already match. The walk target for `GASP-12` is 165 cm/s.
- **Cleanup.** The four Neutral CMC choosers `CHT_PoseSearchDatabases`, `_Dense`, `_Sparse`, and `_ExtremeSparse` under `CMC/RPG/Choosers` had no referencer left and were removed. The shared Dense databases and choosers stay, because the GASP sample copies and the Manny pilot use them.
- **Tests.**
  - New `SurvivalRpg.GASP.CMC.SpeedParity`: run, sprint, and crouch speeds of `BP_RpgGasp_CMC` must lie within 0.85–1.15 of nine reference loops, run and crouch in four directions and sprint forward. Each loop must be reachable through the CMC chooser's databases.
  - The CMC PIE tests' Motion Matching check now also requires the selected database to come from the CMC chooser. It runs on owner, listen server, and late-joined simulated proxy.
- No runtime C++ changed.

## Results

| Area | Result |
| --- | --- |
| Speed band | **Met.** Run 375 against 375 in all four directions (1.00), sprint 585 against 585 (1.00), crouch 200 against 225 / 200 / 180 (0.89 / 1.00 / 1.11). `SpeedParity` checks this. |
| Foot sliding | **Met in the probe and accepted by the user.** At 30, 60, and ~113 FPS (the 120 cap) every loop played at 1.00 of its authored speed, and crouch forward at 0.89. Starts, stops, turns, and pivots now come from the Relaxed databases. Every frame had a selection. |
| Combat stance | **Met.** Forward, backward, right, and left selected `Run_Start`/`Run_Loop` F, B, RL, and LL; diagonals selected LL or RL with orientation warping. Sprint selected the Relaxed sprint start and loop. The rotation-mode tests pass. |
| Network | **Met.** `SurvivalRpg.GASP.CMC` passes 10/10 with the stricter selection check: owner, listen server, and late-joined simulated proxy select databases from the CMC chooser. With `p.NetShowCorrections 1` the stance test logs 0 corrections and the four sprint tests 22, 24, 4, and 4, the range of `GASP-09`. Motion Matching stays cosmetic; no movement value except crouch speed changed. |
| Traversal | **Automation met.** Mantle 12/12, Vault 8/8, Hurdle 11/11. The return to locomotion through the Relaxed `FromTraversal` databases is part of the user check. |
| Block | **Met.** `SurvivalRpg.Combat.BlockLocomotion` 16/16. |
| Unchanged | **Met.** `SurvivalRpg.GASP.Mover` 67/67 (the known Vault timing case passed this time), `RuntimeRetarget` 2/2, `VariantComparison` 3/3. Git shows no change under `Shared`, `Mover`, or `Pilot`, so the shared databases are byte-identical. AI characters use other AnimBPs. The Manny pilot inherits the crouch speed; see the deviations. |
| Performance | **Measured: about +0.23 ms worker time per character.** CSV profile of the probe route at 60 FPS, two runs of 3,660 frames per variant. The baseline switched the old Neutral chooser back in memory, without saving. Game-thread animation, median: 0.32 / 0.32 ms Neutral against 0.31 / 0.32 ms Relaxed. Animation worker time (parallel evaluation, total), median: 0.60 / 0.60 ms against 0.83 / 0.83 ms; p95 0.74 against 1.04–1.07 ms. |

### Automation

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors and 0 warnings.
- `SurvivalRpg.GASP.CMC.SpeedParity` passed. A mutation check confirmed it detects regressions: with a Neutral run loop and 1.5× the crouch speed, it failed on 6 checks with the measured speeds (500, 225, 180, 200).
- Regression, one suite at a time:

| Filter | Result |
| --- | --- |
| Native and asset filters of [GASP-09](gasp-rotation-modes.md#automation) plus `SpeedParity` | 114/114 passed |
| `SurvivalRpg.GASP.CMC`, with `p.NetShowCorrections 1` | 10/10 passed |
| `SurvivalRpg.Combat.BlockLocomotion` | 16/16 passed |
| `SurvivalRpg.GASP.Mantle` | 12/12 passed |
| `SurvivalRpg.GASP.Vault` | 8/8 passed |
| `SurvivalRpg.GASP.Hurdle` | 11/11 passed |
| `SurvivalRpg.GASP.RuntimeRetarget` | 2/2 passed |
| `SurvivalRpg.GASP.VariantComparison` | 3/3 passed |
| `SurvivalRpg.Network` | 19/19 passed |
| `SurvivalRpg.GASP.Mover` | 67/67 passed |
| `SurvivalRpg.GASP.CMC` after the stricter selection check | 10/10 passed |

A mutation check of the stricter check pointed it at the wrong chooser: the owner step "Owner accelerates and its animation snapshot tracks real movement" timed out, as intended.

### Deviations from the definition

- **Lean additives.** The Mover builds its Relaxed leans in a separate animation layer with three blendspaces and its own update logic. Porting that is AnimGraph presentation work and moves to `GASP-13`. The CMC keeps the Neutral `BS1D_Additive_Lean_Run` on the Relaxed poses; the user check judges it.
- **Removed choosers.** The definition did not name the removal of the four unused Neutral CMC choosers; see Implementation.
- **Manny pilot.** `BP_RpgGasp08_CMC_Manny` derives from `BP_RpgGasp_CMC` and inherits the new crouch speed, as it already inherited run and sprint. Its own databases are unchanged.

### Failed attempts

- **Graph DSL.** Rewriting `Update_Logic` through `write_graph_dsl` failed: the parser splits node type IDs at the parentheses in the category `State Machine (Experimental)`. The graph stayed unchanged; node-level edits through the Blueprint toolset replaced it.
- **Direction bias.** The first edit set `MovementDirectionBias` to `RightFootForward`, assuming it selects LL and RL. It selects LR and RR, which the chooser has no rows for, so sideways movement had no selection in the probe. The default was restored.

## Limits

- **User check:** on 2026-10-12 the user found the result good ("sieht gut aus") and asked for the merge. The reply did not itemize leans, diagonal strafing, crouch, or the return from traversal.
- **Diagonals** use the LL and RL clips with orientation warping. The databases also contain LR and RR offset loops, which neither this chooser nor the Mover sample's selects.
- **Free movement** keeps the CMC rule that the direction is F in `OrientToMovement`. Directional pivots and spins (B, LL, RL) are therefore used only in the combat stance; the Mover also uses them in free movement.
- **Crouch** plays the Neutral crouch clips, as the Relaxed set does.
- **No database LOD:** the `MMDatabaseLOD` switch no longer applies to the CMC. The Relaxed set costs about 0.23 ms more animation worker time per character; this matters if GASP pawns are later used for large groups.
- **Experimental state machine:** it is off by default and now receives the movement direction on every update; it remains untested.
- No walk gait until `GASP-12`; traversal montages stay Neutral (decision 2).
