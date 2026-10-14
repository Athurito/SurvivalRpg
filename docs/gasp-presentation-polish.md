# GASP-13 – Presentation polish and standing turn in place

Status: **PR open** in [#247](https://github.com/Athurito/SurvivalRpg/pull/247) on `claude/gasp-13-presentation-polish` (defined 2026-10-14 in [#246](https://github.com/Athurito/SurvivalRpg/pull/246), implemented the same day); the user check is pending. The package outline is in the [movement plan](gasp-movement-plan.md#gasp-13-presentation-polish). On 2026-10-14 the user added the standing turn in place in the combat stance, found in [GASP-STAB-04](gasp-host-view-turn-jitter.md#limits), and moved armor Foley out of this package. This report refines the outline against the current code and assets and is the task report for the implementation PR.

## Goal

- A CMC character standing in the combat stance turns its feet and hips with the camera through turn-in-place steps, on every role.
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

File ownership while the package runs:
- `ABP_RpgGasp_CMC` and the choosers under `CMC/RPG/Choosers`, plus new CMC-owned copies under `CMC/RPG` that Phase B or C needs;
- `DA_PawnData_GaspCMC`, `DA_PawnData_GaspMantle` (camera field only) and a new CMC camera mode;
- `Source/SurvivalRpg/Animation/RpgAnimInstance.*` with its tests, only for a snapshot input from Phase A;
- a new CMC turn-in-place PIE test and `RpgGaspCMCHostViewTests.cpp`;
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
2. Stand still and turn the camera slowly, then quickly, about 90° and 180° both ways. The feet step around with the camera, and the upper body leads. Watch the same turn in the other window.
3. Run, stop, start, reverse (W↔S, A↔D) and run curves, armed and holstered. The feet look heavier and calmer, with fewer spins and no sliding.
4. Run and stop: the camera trails slightly behind the character. Turning the camera has no lag.
5. Turn with block held: unchanged.
6. Mantle on `Lvl_RpgGaspMantle`: the camera handoff still looks as approved.

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

### Deviations from the definition

- The cause differs from the definition's hypothesis; see [Phase A](#phase-a--baseline-and-cause).
- Phase B needed a second content change, the interrupt mode, and a crouch-chooser row; both are in the owned files.
- Three planned tuning values were measured and not applied; see [Phase C](#phase-c--tuning).
- The original GASP ran neither scripted nor by hand; the project copy's graphs were compared.
- The Mover pawn is no control for the standing turn.
- The definition missed the Relaxed lean port that `GASP-10` moved to `GASP-13`; it is not done, see [Limits](#limits).

### Failed attempts

- The first interrupt edit created a call node bound to the sample AnimBP's `ShouldTurnInPlace` and broke the AnimBP's compile until the node was rebound; nothing was saved in that state.
- The first spin route ran into a wall and did not measure the walk snaps; the route now runs out and back.
- The first cost runs started PIE before the CMC map had loaded and measured the baseline pawn; they were discarded and repeated.

### Limits

- **User check:** pending; see [Repro](#repro-after-implementation).
- **Proxy turn selection:** clients that watch another player see that player's standing turn pick a new turn clip almost every frame (see [Phase B](#phase-b--standing-turn-in-place)). The root still follows; how visible the switching is needs the user's look. A fix would smooth the proxy's view yaw for the trajectory, a native change; it needs the user's decision and its own package.
- **Residual angle:** after a continuous turn the root stays up to about 50° behind the capsule, GASP's trigger angle; the aim offset covers it. The definition's 45° holds only after snaps.
- **Tuning outside the ranges:** the Offset Root Bone rotation half-life (0.1), the Steering target time (0.4) and the spin gate stay unchanged for the measured reasons in [Phase C](#phase-c--tuning). The user's check decides whether they stay.
- **Relaxed leans:** `GASP-10` moved porting the Mover's Relaxed lean layer to `GASP-13`. The definition missed it, so this package does not include it; the CMC keeps the Neutral run lean.
- **Camera asset:** `CM_RpgGasp_Traversal` also serves both Mover PawnData. A CMC-only camera value needs a CMC-owned child mode.
- **Foot-slide metric:** the planted-foot measure uses foot height only and counts some swing frames near the floor. It compares runs of the same route; it is not an absolute slide value.
- **Original GASP:** compared through the project copy's graphs only, not run.
- **Network conditions:** single-process PIE without latency, jitter or loss; those stay with `GASP-VAL-02`. Proxy selection may change with real update rates.
- **Mover:** unchanged and not checked for a standing turn, because its capsule does not turn with the camera while standing.
