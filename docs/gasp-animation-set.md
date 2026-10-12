# GASP-10 – Animation set and speed parity

Status: **Ready** (defined 2026-10-12). The package outline is in the [movement plan](gasp-movement-plan.md#gasp-10-animation-set-and-speed-parity). This report refines it against the current assets and code and is the task report for the implementation PR.

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

## Decisions to settle at the start

The implementing chat settles these with the user before Phase B and records the answers here.

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

## Results

Not started.
