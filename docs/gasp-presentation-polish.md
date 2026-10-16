# GASP-13 – Presentation polish and standing turn in place

Status: **PR open** in [#247](https://github.com/Athurito/SurvivalRpg/pull/247) on `claude/gasp-13-presentation-polish` (defined 2026-10-14 in [#246](https://github.com/Athurito/SurvivalRpg/pull/246), implemented the same day). The first user check on 2026-10-15 found the standing turn too jerky; the fix is in [User check 1](#user-check-1-2026-10-15--turn-smoothness). The second check on 2026-10-16 still found it odd, and the user chose that a standing stance keeps its facing, as in Souls-likes and Valheim; see [User check 2](#user-check-2-2026-10-16--the-standing-stance-keeps-its-facing). The third check found blocking after a camera turn to turn twice; the fix is in [User check 3](#user-check-3-2026-10-16--blocking-turned-twice). In the fourth check the user still saw a problem when block is released, deferred it to later and asked for the merge; see [Limits](#limits). The package outline is in the [movement plan](gasp-movement-plan.md#gasp-13-presentation-polish). On 2026-10-14 the user added the standing turn in place in the combat stance, found in [GASP-STAB-04](gasp-host-view-turn-jitter.md#limits), and moved armor Foley out of this package. This report refines the outline against the current code and assets and is the task report for the implementation PR.

## Goal

- A CMC character standing in the combat stance turns its feet and hips with the camera through turn-in-place steps, on every role. Since user check 2 the standing stance keeps its facing instead; the steps turn the feet when blocking, attacking or starting to move turns the capsule.
- Movement looks heavier and calmer: steadier pose selection, slower procedural corrections, fewer spin transitions, and a camera that trails the character slightly.
- Nothing about gameplay movement changes: speeds, acceleration, braking, rotation rates, jump and the capsule's facing stay as `GASP-11` and `GASP-12` left them.

## Why

- A standing armed character keeps its feet planted while the camera, and with it the capsule, turns. After a half turn it faces away from where it aims, and co-op partners see the same.
- The movement plan asks for heavy, grounded dark-fantasy movement. `GASP-09` to `GASP-12` set the gameplay side; the presentation still uses the sample's athletic tuning.

## Current state (checked 2026-10-14 on `master` `0cb467e2`)

Read from `ABP_RpgGasp_CMC` (text export from `GASP-STAB-04`; the AnimBP last changed in `GASP-10`), the assets in a headless editor run and the engine headers (UE 5.8.3).

| Area | Finding |
| --- | --- |
| Standing stance turn | Measured in `GASP-STAB-04` on owner, listen server and proxy: Motion Matching kept `M_Relaxed_Stand_Idle_Loop` for whole turns of up to 540°. The root and the hip line stayed at the same world yaw, and the shoulders varied by about ±15°. In the stance the capsule follows the camera at once (`GASP-09`, `GASP-11`). |
| Turn-in-place content | The Relaxed set has `M_Relaxed_Stand_Turn_045/090/135/180_L/R` and crouch turns of 90° and 180°, in `PSD_Relaxed_Stand_TurnInPlace` (schema `PSS_Relaxed_StandTurn`) and `PSD_Relaxed_Crouch_TurnInPlace`. The CMC chooser `CHT_RpgGasp_CMC_PoseSearchDatabases` references both. |
| Likely cause (hypothesis) | The reverted `GASP-06` attempt recorded on 2026-09-27, on the Neutral set, that CMC Motion Matching selected `M_Neutral_Stand_Turn_090_R` and `_045_R` while turning ([historical report](gasp-block-foot-turn.md)). Since `GASP-10` the CMC evaluates a copy of the Mover's Relaxed chooser. The Mover drives locomotion through the experimental state machine (`LocomotionSetup = 1`), the CMC through Motion Matching (`LocomotionSetup = 0`). The turn rows of the copied chooser, or the inputs they read, may therefore never match on the CMC. Phase A confirms or rejects this. |
| Offset Root Bone | `RotationMode` Accumulate, with the project's block exception in `Get_OffsetRootRotationMode`; `TranslationHalflife` 0.2, `RotationHalfLife` 0.1 (engine default 0.2), `MaxTranslationError` 30, no maximum rotation error. In Accumulate mode the root keeps its world yaw until an animation turns it. The plan proposes a rotation half-life of 0.15 to 0.2. |
| Motion Matching node | `PoseReselectHistory` 0.3 (engine default), `BlendTime` 0.5 with the `FastFeet+Root_Weight` blend profile, play rate 0.85 to 1.15. The plan proposes a reselect history of 0.4 to 0.5. |
| Steering | The Motion Matching blend stack has two Steering nodes: `ProceduralTargetTime` 0.4 (`AnimatedTargetTime` 2.0, off below 10 cm/s) and 1,000,000, which effectively disables the procedural correction. The state-machine blend stack (0.2 and 100,000) is not used by the CMC. The plan proposes 0.5 to 0.6. |
| Spin transitions | Eight Relaxed spin databases (walk and run, directions F, B, LL and RL) with base cost bias 0 and continuing-pose bias −0.3. The CMC chooser, the Mover chooser, the sample chooser and `PSN_Relaxed_All` share them, so changing them changes the Mover. |
| Camera | `DA_PawnData_GaspCMC` uses `CM_ThirdPerson` with `PositionDampingFactor` 0, which means no lag. The baseline, the default PawnData and the `GASP-08` pilots share that mode. `CM_RpgGasp_Traversal` (20 rad/s, the GASP free-camera value approved for mantle) serves the Mantle PawnData, which the Mantle and inventory-storage maps use, and both Mover PawnData. The CMC pawn therefore trails on `Lvl_RpgGaspMantle` but not on `Lvl_RpgGaspCMC`. The damping is the cosmetic critical spring in `URpgCameraMode_ThirdPerson` (tests `SurvivalRpg.Camera.PivotDamping`). |
| Foley | The CMC pawn plays Foley through `GaspFoleyEvents` and `DA_RpgGaspCMCFoley` with sneaker, hand, body and combat sounds. The project has no armor sounds and no armor yet. |

## Scope

1. **Phase A – baseline and cause, without changes.**
   - Standing stance turn:
     - Repeat the `GASP-STAB-04` probe with three players: turns at 45, 90 and 180 °/s and single 90° and 180° flicks, standing and crouched, armed in the stance, at 60 FPS.
     - Record on owner, server and proxy: the chooser's databases, the selected clip, the trajectory's future facing against the root, and the root and hip yaw against the capsule.
     - Controls, in memory and unsaved: the CMC with the shared Dense chooser (the path before `GASP-10`), and the Mover pawn doing the same turn.
     - Control by hand: the original GASP CMC character in the reference project, if it runs there.
     - Write down the cause and the predicted fix before changing anything.
   - Tuning baseline:
     - A route in the stance and in free movement: starts, stops, 90° and 180° direction changes, strafe reversals (W↔S, A↔D) and curves.
     - Record the root offset, spin-database selections, pose changes per second, a planted-foot slide metric and the camera pivot's lag, at 60 FPS, and at 30 and 120 FPS for the camera.
2. **Phase B – standing turn in place.**
   - The smallest change that lets Motion Matching select the turn clips in the stance, in the chooser rows, their inputs or the AnimBP's turn path.
   - The capsule keeps turning at once; the aim offset covers the remaining angle, as in the original GASP.
   - Native code only if Phase A shows a missing game-thread input in the `URpgAnimInstance` snapshot.
3. **Phase C – tuning, one value at a time, each measured against the Phase A route.**
   - Offset Root Bone rotation half-life (0.15 to 0.2).
   - Motion Matching `PoseReselectHistory` (0.4 to 0.5).
   - Steering `ProceduralTargetTime` on the active node (0.5 to 0.6).
   - A cost bias against spin transitions that only the CMC sees, for example through CMC-owned database copies or chooser rows; the shared databases stay unchanged.
   - Camera lag: both CMC PawnData get the damped camera. If the user wants a value other than the approved 20 rad/s, both move to a CMC-owned child camera mode, so the Mover keeps `CM_RpgGasp_Traversal`.
4. **Phase D – tests and validation.**
   - A listen-server PIE test with a late joiner for the stance turn in place.
   - The Phase A probe and route repeated, the regression suites, a cost measurement and the user check.

## Out of scope

- Armor Foley: moves to `GASP-16` or its own audio package once armor and sounds exist (user choice 2026-10-14).
- Gameplay movement values, the movement profile and the capsule's rotation in both modes.
- New feel work on the Mover, the baseline pawn and the shared databases.
- GASP's GameplayCameras runtime; the existing native damping seam stays.
- The 60 Hz steps a host above 60 FPS shows on remote characters, and the stale transform on dedicated servers (`GASP-STAB-04` findings).
- Latency, jitter and loss (`GASP-VAL-02`), and the Manny bake (`GASP-14`).

## Decisions

Made by the user on 2026-10-14:
1. The standing turn in place in the combat stance belongs to this package and comes before the tuning.
2. Armor Foley leaves this package.

Made by the user on 2026-10-16, after user check 2:
3. A combat stance that stands still keeps its facing while the camera turns, as in Souls-likes and Valheim, and turns when it moves, blocks or attacks. The user chose this from options that named it a change to the `GASP-09` rotation rule with a native change, outside this definition.

The implementing chat proposes tuning values inside the ranges above; the user's check sets the final values. It asks the user before building if:
- the turn-in-place fix needs a change to shared databases, Mover assets or a native change beyond a `URpgAnimInstance` snapshot input;
- the feet can follow only through behavior the original GASP lacks, for example a maximum rotation error instead of turn clips;
- a tuning value would leave its range.

## Ownership

| Responsibility | Location | Reason |
| --- | --- | --- |
| Turn-in-place selection, Offset Root Bone, Motion Matching and Steering tuning | `ABP_RpgGasp_CMC`, CMC choosers | Designer-owned presentation; selection stays cosmetic |
| Spin cost bias | CMC-owned chooser rows or database copies under `CMC/RPG` | The shared databases also serve the Mover |
| Camera lag | Camera field of the CMC PawnData; a CMC-owned child of `CM_RpgGasp_Traversal` only if the value changes | Native damping seam exists; tuning is data |
| Game-thread inputs for the AnimBP | `URpgAnimInstance`, only if Phase A finds a missing input | Thread-safe snapshot |
| Tests | CMC PIE tests in `SurvivalRpgEditor/Private/Network` | Turn-in-place contract across roles, not tuning values |
| Standing stance facing and attack alignment (user check 2) | `URpgCharacterMovementComponent`, lease number in `URpgAbilitySystemComponent` | Movement authority, saved-move replay and server agreement |

File ownership while the package runs:
- `ABP_RpgGasp_CMC` and the choosers under `CMC/RPG/Choosers`, plus new CMC-owned copies under `CMC/RPG` that Phase B or C needs;
- `DA_PawnData_GaspCMC`, `DA_PawnData_GaspMantle` (camera field only) and a new CMC camera mode;
- `Source/SurvivalRpg/Animation/RpgAnimInstance.*` with its tests, only for a snapshot input from Phase A;
- a new CMC turn-in-place PIE test and `RpgGaspCMCHostViewTests.cpp`;
- after user check 2: the stance rotation in `URpgCharacterMovementComponent`, the facing-commitment getter in `URpgAbilitySystemComponent`, their native tests, and the cancellation checks in the mantle and vault PIE tests;
- this report and a manifest under `docs/assets`.

Not owned: the shared Relaxed and Dense databases and normalization sets, the Mover assets, `CM_ThirdPerson`, `CM_RpgGasp_Traversal`, the movement values in `BP_RpgGasp_CMC` and `DA_RpgGasp_MovementProfile`, and the Foley assets.

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Standing turn | Standing armed in the stance and turning at 45, 90 and 180 °/s or in 90° and 180° flicks, Motion Matching selects `M_Relaxed_Stand_Turn_*` clips. Within 1.5 s after the camera stops, the root is within 45° of the capsule, the smallest turn clip, unless the Mover or the original GASP settles wider in the same probe. The same holds crouched with the crouch turns. |
| Roles | Owner, listen server, proxy and late joiner show the turn steps. The `GASP-STAB-04` host-view test passes. |
| Unchanged turns | Free movement standing still turns no body with the camera, and turning with block held keeps its root behavior. |
| Tuning | Final values come from the user check, inside the ranges above unless the user approves. Planted-foot slide on the Phase A route is not worse than in Phase A. The CMC selects fewer spin databases on the direction-change route than in Phase A. |
| Camera | The CMC camera trails moves and stops at 30, 60 and 120 FPS and resets after teleports and respawn; view rotation stays immediate. |
| Gameplay unchanged | No movement value changes; `SurvivalRpg.GASP.CMC.SpeedParity` and the movement-profile tests pass. |
| Mover unchanged | Shared databases, Mover choosers, the Mover AnimBP and `CM_RpgGasp_Traversal` are byte-identical; the Mover regression passes. |
| Cost | Game-thread and worker animation time per character is measured before and after and reported. |

## Verification

- `python Build/Tools/Unreal/ue.py build` only after C++ changes.
- Focused automation through `ue.py test`:
  - `SurvivalRpg.GASP.CMC` with the new turn-in-place test, and `SurvivalRpg.Movement.CMC`;
  - `SurvivalRpg.Animation.ListenServer` and `SurvivalRpg.Camera`;
  - `SurvivalRpg.Combat.Block.Prediction` and `SurvivalRpg.Combat.BlockLocomotion`;
  - `SurvivalRpg.GASP.Mantle`, `SurvivalRpg.GASP.Vault`, `SurvivalRpg.GASP.Hurdle` and `SurvivalRpg.Network`;
  - `SurvivalRpg.GASP.Mover` and `SurvivalRpg.GASP.VariantComparison` as regressions.
- AnimBP compile and asset validation; the reference closure has no dependency on the GASP sample checkout.
- The Phase A probe and route repeated, with three players and a late joiner.
- A user check on `Lvl_RpgGaspCMC` with two players, as host and as client, plus a mantle on `Lvl_RpgGaspMantle`.

## Repro (after implementation)

1. Set PIE to Play As Listen Server with two players and open `Lvl_RpgGaspCMC`. Both characters spawn armed in the combat stance.
2. Stand still and turn the camera around the character. The body keeps its facing; head and shoulders look along the camera up to about 115° and fade back beyond. Watch the same in the other window: the other player's body stays still too.
3. Turn the camera away, then hold block: the body turns to the camera at once, the feet step around. Release block and turn the camera again: the body stays.
4. Turn the camera away, then attack: the body turns quickly into the attack (180° in about 0.1 s) and the swing goes toward the camera.
5. Turn the camera away, then tap a movement key: the character turns and steps off toward the camera direction.
6. Run, stop, start, reverse (W↔S, A↔D) and run curves, armed and holstered. The feet look heavier and calmer, with fewer spins and no sliding. While moving armed, the character faces the camera and strafes as before.
7. Run and stop: the camera trails slightly behind the character. Turning the camera has no lag.
8. Mantle on `Lvl_RpgGaspMantle`: the camera handoff still looks as approved.

## Results

Commits on `claude/gasp-13-presentation-polish`: `33d04741` (start), `a9e4f9ba` (turn in place), `c89bdfe7` (tuning), `6941f26c` (test), `014480b7` (narrower interrupt), and the commits listed in the PR. Raw probe data are in `Saved/Gasp13` of the worktree `musing-ishizaka-7bac20`; the [manifest](assets/gasp-13-presentation-polish.json) holds the summaries.

### Probe

- Listen-server PIE with three players on `Lvl_RpgGaspCMC` at 60 FPS without latency. Client A acts; the probe records A as owner, on the listen server and as a simulated proxy on client B.
- **Turn:** standing armed in the stance, the camera turns at 45, 90 and 180 °/s for 3 s each with 1.5 s holds, then snaps by 90°, 180° and −90°, then the same crouched.
- **Route:** starts, stops, W↔S and A↔D reversals and a curve in the stance, then the same holstered in free movement.
- **Spin route:** running and walking in the stance, the camera snaps by 180° while the pawn keeps its world direction, which asks for a spin transition.
- Metrics:
  - the root's yaw against the capsule, with the hip line between the thigh sockets as a check;
  - the selected clip and database (`CurrentSelectedAnim`, `CurrentSelectedDatabase`) and `ShouldTurnInPlace`;
  - planted-foot slide: the horizontal speed of a foot while it stays within 3 cm of its lowest height in a step;
  - the camera's horizontal distance to the pawn.
- A repeat of the same configuration gave the same slide within 0.5 cm/s, so the differences below are not run-to-run noise.

### Phase A – baseline and cause

- **Standing turn:** on every role, `ShouldTurnInPlace` was false on all frames, Motion Matching stayed on `M_Relaxed_Stand_Idle_Loop` (crouched on the crouch idle), and the root turned 0° over 540° of camera turn; it lagged the capsule by up to 179°. The hip line followed the root.
- **Cause 1, the gate:** `ShouldTurnInPlace` is the GASP function unchanged. It offers the turn-in-place database only while `WantsToAim` is set or in the frame after a stop. `BP_RpgGasp_CMC` reports the combat stance as `WantsToStrafe` (`GASP-09`) and `WantsToAim` as a constant `false`. The project copy of the original `SandboxCharacter_CMC` sets `WantsToAim` only from its aim input; its strafing without aiming gates turning the same way, so the original shows the same behavior there. The stops on the route did play `Stand_Turn_090_R` and `_045_R`, so the turn database itself works.
- **Hypothesis rejected:** the shared Dense chooser that the CMC used before `GASP-10` gates its turn row on `ShouldTurnInPlace` as well. The turn clips recorded on 2026-09-27 came from the reverted `GASP-06` attempt, which allowed turning while the upper-body slot played.
- **Mover control:** the Mover pawn does not turn its capsule with the camera while standing (root and capsule both stayed), so it is no control for this turn.
- **Original GASP control:** not run in the reference project; the project copy's graphs were compared instead.

### Phase B – standing turn in place

- **Gate:** `ShouldTurnInPlace` in `ABP_RpgGasp_CMC` also accepts the strafe rotation mode, the combat stance. `WantsToAim` stays false, so direction thresholds that read it do not change.
- **Cause 2, found with the gate open:** `ShouldTurnInPlace` was true on 110 to 157 frames per turn, but Motion Matching still kept the idle loop. Standing still, `Get_MMInterruptMode` returns `DoNotInterrupt`, so the continuing idle pose stays a candidate. The Dense turn database beats it with a base cost bias of −0.2 against +0.1 on the idles; the Relaxed one has 0 on both, so the idle loop won.
- **Interrupt:** `Get_MMInterruptMode` now also returns `InterruptOnDatabaseChange` while `ShouldTurnInPlace` holds standing still and the current database is not tagged `TurnInPlace`. Motion Matching then drops the idle pose once, when the chooser switches to the turn database, and never interrupts a playing turn clip.
- **Crouch:** the CMC chooser's crouch-idle table offered the crouch idles together with the crouch turns. Its idle row now requires `ShouldTurnInPlace` to be false, as the stand-idle row already did.
- Shared databases, the Mover and its chooser are unchanged.

Standing turn after both changes (root lag behind the capsule in degrees at the end of each hold; owner / server / proxy):

| Step | Before | After |
| --- | --- | --- |
| 45 °/s for 3 s, then 1.5 s hold | 136 / 136 / 136 | 47 / 47 / 47 |
| 90 °/s | 46 / 46 / 46 | 46 / 46 / 50 |
| 180 °/s | 134 / 134 / 134 | 36 / 45 / 49 |
| Snap 90° | 44 / 44 / 44 | 0 / 0 / 0 |
| Snap 180° | 136 / 136 / 136 | 0 / 0 / 0 |
| Snap −90° | 46 / 46 / 46 | 0 / 0 / 21 |
| Crouched, 90 °/s then hold | 43 / 43 / 43 | 0 / 0 / 48 |
| Crouched, snap 180° | 137 / 137 / 137 | 0 / 0 / 41 |

- During continuous turns the root follows in 45° to 180° turn clips; the capsule stays up to about 50° ahead, then a clip starts. After holds the root stays up to 49.7° behind, below GASP's 50° trigger; the aim offset covers that angle.
- Each role selects its own clips, so a proxy may settle on a different step than the owner.
- **Proxy selection:** a simulated proxy switches between turn clips far more often than the owner. Clip switches per 3-s turn (owner / server / proxy): 2 / 2 / 24 at 45 °/s, 10 / 10 / 71 at 90 °/s, 28 / 22 / 95 at 180 °/s, crouched 16 / 15 / 91.
  - Cause: the engine's trajectory generator predicts the facing from `GetViewRotation()`. On a proxy that is the actor yaw, which a 60-FPS client receives about every second frame, so the predicted turn rate alternates between zero and double. Motion Matching picks a different turn clip almost every frame and drops it for the idle when the offset dips below 50°.
  - The listen server sees the remote client like the owner does, because it ticks the pose per received move. Clients that watch another player, including the host's character, see the proxy selection.
  - Narrowing the interrupt to idle poses (`014480b7`) did not change the switch counts, so the interrupt is not the cause. A fix would smooth the proxy's view yaw for the trajectory, a native change the definition leaves to the user.
- **Moving unchanged:** on the route, before and after Phase B, owner slide 32.0 / 31.3 cm/s (p95 194 / 181), mean root lag 12.9° / 12.8°, the same clips, and no spins.

### Phase C – tuning

Each value was measured against the state without it on the spin route (owner; server and proxy agree). The steering and spin trials ran with `PoseReselectHistory` 0.4; the spin trial also with steering 0.5, against a run with steering 0.5 and the original spin gate.

| Change | Result | Kept |
| --- | --- | --- |
| `PoseReselectHistory` 0.3 → 0.4 | Spin route slide 32.6 against 31.9 cm/s (p95 189 / 178), 0.63 clip changes per second in both, the same spins | Yes |
| Steering `ProceduralTargetTime` 0.4 → 0.5 | The run snap that plays a pivot and a backward loop slid 139 instead of 75 cm/s (p95 836 / 254), with 106° instead of 90° mean root lag | No, 0.4 |
| Spins only from 300 cm/s instead of 150 | No walk spins, but the walk snaps then play `Walk_Loop_B` and slide 150 and 168 instead of 93 and 85 cm/s (p95 546 and 633 against 338 and 315) | No |
| Offset Root Bone rotation half-life 0.1 → 0.15–0.2 | Not applied: the engine damps rotation with it only in Release and Interpolate modes. The CMC uses Accumulate and switches to Release only while a `DefaultSlot` montage plays, so the value would only slow how attacks and other montages align the body. | No, 0.1 |
| Camera: `DA_PawnData_GaspCMC` uses `CM_RpgGasp_Traversal` (20 rad/s) | The camera trails starts by up to 37 cm (295.7 → 332.5 cm to the pawn) and settles after stops, identically at 30, 60 and 120 FPS; view rotation stays immediate | Yes |

- The spin transitions are the only clips that cover a 180° facing change while the pawn keeps moving. The CMC therefore keeps them; the shared spin databases stay unchanged.
- The rejected values are outside the definition's ranges and need the user's check.

### Phase D – tests

- New `SurvivalRpg.GASP.CMC.HostView…StandingStanceTurnFollowsTheCameraOnEveryRole`: listen server with two clients and a late joiner. The owner's camera snaps by 180° and −90°, then by 180° after a third client joined. After 2.5 s the root must be within 60° of the capsule on the owner, the server, the proxy and the late joiner. Measured lags: 0.0 to 26.8°.
- **Mutation:** with the AnimBP and chooser from before `a9e4f9ba`, the test fails with 180.0° on every role.
- The `GASP-STAB-04` host-view test still passes: server root flips 0, roughness 0.19 to 0.53 against 0.83 to 1.59 °/frame on the proxy.

### Phase D – validation

- **Build:** `ue.py build` passed (0 errors, 0 warnings) after the test change; the other changes are assets.
- **Regression** on the final assets, each suite on its own through `ue.py test`:

  | Suite | Result |
  | --- | --- |
  | Native (movement, equipment, harvesting, abilities, block, dodge, combat content, CMC and mantle assets, speed parity, sprint, input, UI input, camera) | 138 / 138 |
  | `SurvivalRpg.Animation` | 4 / 4 |
  | `SurvivalRpg.GASP.CMC` (with both host-view tests) | 13 / 13 |
  | `SurvivalRpg.Combat.BlockLocomotion` | 16 / 16 |
  | `SurvivalRpg.GASP.Mantle` | 13 / 13 |
  | `SurvivalRpg.GASP.Vault` | 9 / 9 |
  | `SurvivalRpg.GASP.Hurdle` | 12 / 12 |
  | `SurvivalRpg.GASP.RuntimeRetarget` | 2 / 2 |
  | `SurvivalRpg.GASP.VariantComparison` | 3 / 3 |
  | `SurvivalRpg.Network` | 19 / 19 |
  | `SurvivalRpg.GASP.Mover` | 66 / 67 |

  The Mover failure is the known timing-sensitive fixture `GaspMoverVaultPIE.EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection` (`GASP-VAL-03`, [5.8.3 report](gasp-engine-5-8-3.md)) with its usual precondition message; of two isolated reruns, one failed and one passed. This package changes no Mover asset.
- **Cost:** CSV profile of the turn probe (three PIE worlds, nine animated characters, 60 FPS), two runs per variant with the `master` assets and the final assets. Game-thread animation median 1.57 / 1.60 ms before and 1.62 / 1.53 ms after, unchanged within run-to-run spread. Animation worker time (parallel evaluation, total) median 5.87 / 5.84 ms before and 6.00 / 5.94 ms after: about +0.1 ms for nine characters, roughly 0.01 ms per character. A first pair of runs measured the baseline pawn because the map had not finished loading; it is set aside in `Saved/Gasp13/invalid`.
- **Multiplayer:** the probe recorded owner, listen server and proxy for the turn, route and spin route; the new PIE test adds a late joiner.

### User check 1 (2026-10-15) – turn smoothness

The user found the standing turn too jerky, and fast turns "skipped" the steps. The turn probe gained a fast mode (360 °/s for 2 s, a 720 °/s whip of 360°, −360 °/s for 1 s, then 30 and 120 °/s), and the analysis new metrics per role:
- **dead stops:** the root turns faster than 150 °/s and is below 20 °/s within three frames;
- **bursts:** the root starts turning above 100 °/s after being below 20 °/s;
- **stall:** the share of frames in which the root stands still while trailing by more than 25°;
- **lead:** how far the root runs ahead of the capsule in the turn direction.

Findings on the committed state:
- **Cause 3, aborted steps:** a turn clip starts at 50°. Once the root came within 50° of the capsule, `ShouldTurnInPlace` turned false and the chooser offered only the idle again. Motion Matching then swapped the turn clip for the idle loop although the clip still turned the root at about 350 °/s, so the root stopped dead within 50 ms. During continuous turns this repeated four to five times per second: 13 dead stops in 3 s at 180 °/s on the owner and the proxy.
- **Stuck after fast turns:** an aborted step left the root up to 38° behind once the camera stopped, below the 50° trigger, so it stayed there: 33° after the 720 °/s whip and 38° after the −360 °/s turn on the owner.
- **Clip speed:** the Relaxed turn clips turn fast by design. 90° clips do 90% of their rotation in 0.35 s, 180° clips in 0.4 s, peaking at 330 and 500 °/s. The rest of each clip, up to 2.17 s, settles the feet. The clips are unchanged.
- **Upper body:** the aim offset is active in the stance and follows the full offset (44° at 45° behind), but `BS_Neutral_AO_Stand` turns the shoulder line only about 10° of it; most goes to neck and head. From behind, the body looks frozen until a step starts.

Fix, in `ABP_RpgGasp_CMC` only:
- A new variable `TurnInPlaceSide` (−1, 0 or +1, category States) holds the side of the root offset while a step runs. `Update_States`, which runs once per update on the worker thread, sets it from `ShouldTurnInPlace` and the sign of the offset, and to 0 otherwise.
- `ShouldTurnInPlace` is now `(|offset| >= 50 OR offset × TurnInPlaceSide >= 10) AND` the unchanged aim, stop and strafe conditions. A started step keeps the turn database until the root is within 10° on the side it started from. A root that passed the capsule does not reopen the gate, which needs 50° again.

Turn probe at 60 FPS, before / after (owner / server / proxy; after: two runs):

| Measure | Before | After |
| --- | --- | --- |
| Dead stops in 3 s at 90 °/s | 5 / 5 / 8 | 3 / 3 / 3 and 4 / 4 / 4 |
| Dead stops in 3 s at 180 °/s | 13 / 11 / 13 | 4 / 4 / 3 and 4 / 4 / 4 |
| Bursts in 3 s at 180 °/s | 14 / 11 / 13 | 5 / 5 / 4 and 5 / 5 / 4 |
| Mean root lag at 180 °/s | 42 / 38 / 58° | 25 / 25 / 31° and 24 / 24 / 27° |
| Clip switches in 3 s at 180 °/s | 28 / 22 / 95 | 13 / 13 / 73 and 13 / 15 / 65 |
| Root lag after the 720 °/s whip | 33 / 33 / 4° | 5 / 6 / 5° |
| Root lag after −360 °/s for 1 s | 38 / 0 / 46° | 9 / 0 / 0° |
| Mean root lag at 360 °/s | 46 / 46 / 67° | 26 / 26 / 53° |

- Slow turns are unchanged: at 30 and 45 °/s the root still waits until 50°, then takes one step (stall 0.9 before and after).
- The route probe is unchanged: owner slide 32.3 against 32.4 cm/s, 0.64 against 0.68 clip changes per second, no spins.
- **Tests:** `RemoteStanceTurnInPlaceStaysSteadyOnTheHost` now also allows at most three dead stops per second on every role in each 1.5-s window at 90 and 180 °/s. `StandingStanceTurnFollowsTheCameraOnEveryRole` adds a 450° sweep at 720 °/s before the late join and a −450° sweep after it; then every role's root must be within 50° of the capsule. Measured with the fix: at most 2.0 dead stops per second in three runs, and 0 to 34° after the sweeps in four runs (the server trails most). With the AnimBP from before the fix, the steady-turn test failed in three of three runs: 4.64 dead stops per second on the proxy at 90 °/s, 3.30 on the owner at 180 °/s, and 3.31 on the proxy at 90 °/s.

Regression after the fix, each suite on its own through `ue.py test` after `ue.py build` (0 errors, 0 warnings):

| Suite | Result |
| --- | --- |
| Native (as above) | 138 / 138 |
| `SurvivalRpg.Animation` | 4 / 4 |
| `SurvivalRpg.GASP.CMC` (with both host-view tests) | 13 / 13 |
| `SurvivalRpg.Combat.BlockLocomotion` | 15 / 16 |
| `SurvivalRpg.GASP.Mantle` | 13 / 13 |
| `SurvivalRpg.GASP.Vault` | 9 / 9 |
| `SurvivalRpg.GASP.Hurdle` | 12 / 12 |
| `SurvivalRpg.GASP.RuntimeRetarget` | 2 / 2 |
| `SurvivalRpg.GASP.VariantComparison` | 3 / 3 |
| `SurvivalRpg.Network` | 19 / 19 |
| `SurvivalRpg.GASP.Mover` | 65 / 67 |

- **BlockLocomotion:** `MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease` timed out waiting for the release-crossing replay. Alone it failed twice more and then passed three times in a row; with the AnimBP from before the fix it passed three of three runs in between. It runs the Mover pawn, and only CMC assets and the Manny pilot reference `ABP_RpgGasp_CMC`, so the failures are attributed to timing, not to this change.
- **Mover:** the known `GASP-VAL-03` fixture `EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection` failed with its usual precondition message, alone as well. `LateJoinReconstructsTheCurrentVaultState` failed in the suite and passed alone, as in `GASP-12`.

Failed attempts and rejected values:
- **Hysteresis without a side:** holding the gate at 10° while the current database carried the `TurnInPlace` tag. A turn clip that overshot the capsule reopened the gate on the other side, and at 90 °/s the root ran up to 41° ahead of the camera on owner and server (9 to 13% of frames more than 15° ahead, against 1% and at most 17° with the side).
- **Release at 5° or 20°** instead of 10°: no clear gain; 20° left the owner 48° behind after the −360 °/s turn.
- **Trigger at 35°** instead of 50°: smaller lag, but more dead stops at 180 °/s, and the threshold also applies to GASP's turn after a stop outside the stance. Not kept.
- **Lead assertion in the test:** the owner's lead at 90 °/s reached 24° in one run, too close to the 27 to 41° of the rejected variant to separate them reliably. On the listen server the sampled capsule trails the pose of a remote pawn by about 50° in both variants, because the server poses it per received move. The test logs the lead but does not assert it.
- Creating the `ShouldTurnInPlace` call and the `CharacterProperties` getter in `Update_States` bound them to the sample AnimBP again; both were replaced with nodes bound to `ABP_RpgGasp_CMC` (`declaring_class`) before compiling.

### User check 2 (2026-10-16) – the standing stance keeps its facing

The user still found the standing turn odd and asked how other games handle it. The comparison given:
- Dark Souls, Elden Ring and Monster Hunter do not turn a standing character with the camera; movement, lock-on or an attack turns it.
- Valheim and Enshrouded do not turn it either; attacking, blocking or aiming turns it toward the camera.
- New World turns the body with the camera at once and smoothly, with small shuffle steps.
- Shooters, Lyra and GASP's aiming twist the upper body clearly toward the aim and let the legs catch up in steps.

The CMC mixed New World's rule with GASP's shooter steps, whose aim offset turns the shoulders only about 10° at 45°. The user chose the Souls and Valheim behavior (decision 3).

Change, native because it is movement authority with saved-move replay:
- `URpgCharacterMovementComponent` keeps the stance's facing while it stands still on the ground: no input, under 5 cm/s and no block. Moving, blocking and falling still face the camera at once; free movement is unchanged.
- The rotation flags that the Pose Search trajectory reads report no turn while the stance holds, so Motion Matching predicts none.
- An ability's new steering facing commitment, such as an attack or harvesting windup, first turns the character toward the camera at `CombatStanceAlignRate` until it faces it, then steers at its own rate as in `GASP-09`. The saved move records whether it aligned, so replay repeats it; the latch uses the lease number, which `URpgAbilitySystemComponent::GetFacingCommitment` now also returns. A holding commitment, like the dodge, keeps its facing.
- `bCombatStanceFacesCameraWhileStanding` (default false) restores the `GASP-09` stance per character class.
- `ABP_RpgGasp_CMC` is unchanged: `OrientationIntent` comes from the actor rotation, so a holding stance starts no turn-in-place step. The aim offset reads `GetBaseAimRotation` and still turns head and shoulders toward the camera; `Enable_AO` fades it out beyond 115° while standing (GASP's value). Measured shoulder twist: 25° at 94°, 27° at 107°, then back to rest once the offset passes 115°.

Probe `stand` (60 FPS, owner / server / proxy):

| Step | Result |
| --- | --- |
| Camera turns 270° at 90 °/s while standing | Capsule and root unchanged on all roles; no turn clip |
| Block held after the camera turned 89° away | Capsule turns at once; the root follows with `Stand_Turn_090_L` and `_045_L` to 0° behind |
| Walk 0.6 s after a 120° camera snap | Capsule turns at once; `Run_Reface_Start_090_R` turns the root; 0° behind after the stop |
| Attack after a 150° camera snap | At 1,800 °/s capsule and root turn in 0.08 s on every role (the proxy in network steps) and end at 0° |
| Camera turns 360° at 180 °/s after the attack | Capsule and root unchanged again |

- **Attack alignment, first attempt:** turning at once made the owner's and the server's root jump 150° in one frame, because the attack montage poses the root on the capsule; the proxy blended. At 1,080 °/s the turn took 0.15 s on owner and server and about 0.25 s on the proxy, without a jump.
- **Align rate:** the sword attacks open their hit window, which holds the facing, 0.16 s after the start (0.107 s at play rate 1.5). 1,080 °/s would leave a 180° attack up to 65° short. The default is 1,800 °/s: 180° in 0.1 s.

Tests:
- New `SurvivalRpg.Movement.CMC.RotationModes.StandingStanceKeepsFacingUntilItMovesBlocksOrActs`: standing keeps the facing and reports no trajectory turn; moving, blocking and a steering commitment turn it; the commitment aligns at its rate, then steers; replay repeats the alignment; the designer option restores the old stance.
- The existing rotation-mode and profile tests move or align where they assumed a standing stance that faces the camera.
- New `SurvivalRpg.GASP.CMC.HostView…StandingStanceKeepsFacingUntilItMovesOnEveryRole`: camera snaps of 120° and −150° leave capsule and root at 0.0° on owner, server, proxy and late joiner; a 0.4-s walk then turns every role to within 0.5° of the camera with the root at most 7.8° behind.
- The turn-in-place host-view tests set `bCombatStanceFacesCameraWhileStanding` on every role, so the camera alone drives the capsule as blocking does; they pass unchanged (renamed `CameraFacingStanceTurnInPlaceFollowsOnEveryRole`).

Validation after the change, each suite on its own through `ue.py test` after `ue.py build` (0 errors, 0 warnings):

| Suite | Result |
| --- | --- |
| Native (as above, with the new rotation-mode test) | 139 / 139 |
| `SurvivalRpg.Animation` | 4 / 4 |
| `SurvivalRpg.GASP.CMC` (three host-view tests) | 14 / 14 |
| `SurvivalRpg.Combat.BlockLocomotion` | 16 / 16 |
| `SurvivalRpg.GASP.Mantle` | 13 / 13 after the test change below |
| `SurvivalRpg.GASP.Vault` | 9 / 9 after the test change below |
| `SurvivalRpg.GASP.Hurdle` | 12 / 12 |
| `SurvivalRpg.GASP.RuntimeRetarget` | 2 / 2 |
| `SurvivalRpg.GASP.VariantComparison` | 3 / 3 |
| `SurvivalRpg.Network` (with the remote melee and combat cases) | 19 / 19 |
| `SurvivalRpg.GASP.Mover` | 65 / 67 |

- Hurdle, RuntimeRetarget, VariantComparison and Mover ran with an align rate of 1,080 °/s; native, CMC, Mantle, Vault and Network ran again after the change to 1,800 °/s.
- **Mover:** the known `GASP-VAL-03` fixture `EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection` and `LateJoinReconstructsTheCurrentVaultState`; the latter passed alone. Neither runs the CMC pawn.
- **Mantle and Vault tests:** both cancellation cases expected a standing armed pawn to face the camera again by itself. They now take a step along the view, which turns it through the ordinary policy, and the mantle case steps forward again before the fixture teleports the pawn to the entry. A server teleport with `ClientSetRotation` alone no longer turns a standing armed owner, because rotation-mode characters ignore `FaceRotation`; before, the stance hid that by facing the camera.
- **Mutation:** with `bCombatStanceFacesCameraWhileStanding` defaulting to true, the new native test fails on the held facing and the trajectory flags, and the new host-view test fails with 120° of capsule turn on owner, server and proxy.

### User check 3 (2026-10-16) – blocking turned twice

The user found the standing stance good, but blocking "triggered twice". A block probe (block while facing the camera, then block after a 120° camera snap; owner, server, proxy) showed the cause:
- Since user check 2, block turns the capsule to the camera at once. The block layer `ALI_RpgBlockLocomotion` (`GASP-06`) sits after Offset Root Bone in the AnimGraph and turns the visible body itself. Below it, the base pose also started a turn-in-place step (`Stand_Turn_135_R`, then `_045_R`).
- While the block layer is fully in, the base pose is not evaluated: the Offset Root Bone state froze at 46° (owner) to 59° (proxy) behind the capsule.
- On release the base resumed with that offset and stepped again (`Stand_Turn_045_L`), ending 10.6° off the capsule on every role. Blocking while already facing the camera showed nothing of this.

Fix, in `ABP_RpgGasp_CMC` only:
- `ShouldTurnInPlace` is false while the block layer is active (`bBlockLocomotionActive` from `URpgAnimInstance`), in the update after it ends, and while a `DefaultSlot` montage is active (`IsSlotActive`, thread safe). Under these the base is hidden and Offset Root Bone releases it.
- Two state variables, `BlockLocomotionActive_ThisFrame` and `BlockLocomotionActive_LastFrame`, set once per update in `Update_States`, carry the previous update's block state: a proxy receives the block's end one update before its end montage counts as active.
- `Get_OffsetRootRotationMode` returns `Interpolate` while the block layer is active (Release stays for `DefaultSlot` montages), so the base root follows the capsule with the 0.1-s half-life instead of keeping the offset. Release would turn the root with the capsule in the same frame: a probe showed a 120° jump at the block press.
- `Get_MMInterruptMode` re-selects in the update after the block ends, so a turn clip that a proxy started in the frame between the capsule turn and the block state cannot resume.

Block probe after the fix: the body turns once (root 90° to 0° over about 0.15 s on owner and server), no base turn clip plays under the block, and release leaves the root at 0.0° with no step on every role, in three proxy runs with −120°. The `stand` probe shows the release clean as well.

Test: `StandingStanceKeepsFacingUntilItMovesOnEveryRole` now turns the camera −120°, holds block through the ordinary right mouse button for 2.5 s, releases it and checks every role 2.5 s later: facing the camera, root on the capsule within 5°, and no root turn after the release above 5°. With the fix it passed three runs at 0.0° on every role (an earlier run without the interrupt rule failed on the proxy with 23°). With the AnimBP from before the fix it failed twice with 10.5° to 10.8° on owner, server and proxy.

Regression after the fix, each suite on its own through `ue.py test` after `ue.py build` (0 errors, 0 warnings): native 139 / 139, `SurvivalRpg.Animation` 4 / 4, `SurvivalRpg.GASP.CMC` 14 / 14, `SurvivalRpg.Combat.BlockLocomotion` 16 / 16, `SurvivalRpg.GASP.Mantle` 13 / 13, `SurvivalRpg.GASP.Vault` 9 / 9, `SurvivalRpg.GASP.Hurdle` 12 / 12, `SurvivalRpg.GASP.RuntimeRetarget` 2 / 2, `SurvivalRpg.GASP.VariantComparison` 3 / 3, `SurvivalRpg.Network` 19 / 19. The Mover suite was not repeated; the change is in the CMC AnimBP, which the Mover pawn does not use.

### Deviations from the definition

- The cause differs from the definition's hypothesis; see [Phase A](#phase-a--baseline-and-cause).
- Phase B needed a second content change, the interrupt mode, and a crouch-chooser row; both are in the owned files.
- The first user check needed a third: a directional hold for the turn gate with a new AnimBP variable, also in `ABP_RpgGasp_CMC`.
- The second user check changed the capsule's rotation in the combat stance, which the definition excluded, with a native change in `URpgCharacterMovementComponent` and `URpgAbilitySystemComponent` (decision 3).
- Three planned tuning values were measured and not applied; see [Phase C](#phase-c--tuning).
- The original GASP ran neither scripted nor by hand; the project copy's graphs were compared.
- The Mover pawn is no control for the standing turn.
- The definition missed the Relaxed lean port that `GASP-10` moved to `GASP-13`; it is not done, see [Limits](#limits).

### Failed attempts

- The first interrupt edit created a call node bound to the sample AnimBP's `ShouldTurnInPlace` and broke the AnimBP's compile until the node was rebound; nothing was saved in that state.
- The first spin route ran into a wall and did not measure the walk snaps; the route now runs out and back.
- The first cost runs started PIE before the CMC map had loaded and measured the baseline pawn; they were discarded and repeated.

### Limits

- **User check:** the first check (2026-10-15) found the turn too jerky ([User check 1](#user-check-1-2026-10-15--turn-smoothness)); the second (2026-10-16) led to the standing stance keeping its facing ([User check 2](#user-check-2-2026-10-16--the-standing-stance-keeps-its-facing)); the third found blocking turned twice ([User check 3](#user-check-3-2026-10-16--blocking-turned-twice)). In the fourth (2026-10-16) the user asked for the merge.
- **Block release (user check 4, deferred):** the user still sees a problem when block is released and deferred it to later. It was not analyzed further. A lead is the base pose below `ALI_RpgBlockLocomotion`, which is not evaluated while the layer is fully in and resumes on release ([User check 3](#user-check-3-2026-10-16--blocking-turned-twice)).
- **Open decisions:** the user has not decided on the proxy turn-clip switching, the tuning values left unchanged in Phase C, or where the Relaxed lean port goes; they stay open findings.
- **Step cadence:** turning while blocking, and with `bCombatStanceFacesCameraWhileStanding`, still steps with the fast Relaxed clips (180° in 0.4 s): a slow turn waits until 50°, a continuous turn steps about every 0.6 s.
- **After fast sweeps:** with the capsule following the camera, the root can meet the capsule while the camera still turns and then stay up to 50° behind, GASP's trigger; the PIE test measured up to 34° on the listen server.
- **Aim offset on proxies:** other clients see no head or shoulder turn toward a remote player's camera, because a proxy's base aim yaw is its actor yaw; the owner and the listen server see it. This predates `GASP-13`.
- **Attack alignment:** at 1,800 °/s the shortest sword windup (0.107 s at play rate 1.5) covers 193°, so every angle finishes turning before the hit window holds the facing. A faster attack or a lower rate would cut large turns short.
- **Block turn:** blocking turns the capsule, and with it the block direction, at once; the feet catch up over about half a second.
- **Block press on a proxy:** a proxy can receive the capsule turn one update before the block state; the base then starts a turn clip for that update under the incoming block layer. The interrupt rule drops it on release; it is not visible in the probes.
- **Block layer and base pose:** the base pose below `ALI_RpgBlockLocomotion` is not evaluated while the layer is fully in, so its state freezes. The fix keeps it out of the release; a layer that keeps updating its base pose would make the guards unnecessary, which is combat content outside this package.
- **Server teleports:** a server teleport with a rotation and `ClientSetRotation` does not turn a standing armed owner's capsule, which keeps its own facing until it moves, blocks or attacks; the server then holds the teleport facing. Respawn and travel spawn a new pawn and are not affected. Systems that teleport a living pawn with a facing, like future portals, need an explicit owner-side facing.
- **Proxy turn selection:** clients that watch another player see that player's standing turn pick a new turn clip almost every frame (see [Phase B](#phase-b--standing-turn-in-place)). The root still follows; how visible the switching is needs the user's look. A fix would smooth the proxy's view yaw for the trajectory, a native change; it needs the user's decision and its own package.
- **Residual angle:** after a continuous turn the root stays up to about 50° behind the capsule, GASP's trigger angle; the aim offset covers it. The definition's 45° holds only after snaps.
- **Tuning outside the ranges:** the Offset Root Bone rotation half-life (0.1), the Steering target time (0.4) and the spin gate stay unchanged for the measured reasons in [Phase C](#phase-c--tuning). The user's check decides whether they stay.
- **Relaxed leans:** `GASP-10` moved porting the Mover's Relaxed lean layer to `GASP-13`. The definition missed it, so this package does not include it; the CMC keeps the Neutral run lean.
- **Camera asset:** `CM_RpgGasp_Traversal` also serves both Mover PawnData. A CMC-only camera value needs a CMC-owned child mode.
- **Foot-slide metric:** the planted-foot measure uses foot height only and counts some swing frames near the floor. It compares runs of the same route; it is not an absolute slide value.
- **Original GASP:** compared through the project copy's graphs only, not run.
- **Network conditions:** single-process PIE without latency, jitter or loss; those stay with `GASP-VAL-02`. Proxy selection may change with real update rates.
- **Mover:** unchanged and not checked for a standing turn, because its capsule does not turn with the camera while standing.
