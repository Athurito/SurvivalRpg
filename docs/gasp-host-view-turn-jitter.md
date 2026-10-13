# GASP-STAB-04 – Host-view jitter of remote characters turning in place

Status: **In progress** on `claude/gasp-stab-04-host-view-jitter` (defined 2026-10-13 in [#243](https://github.com/Athurito/SurvivalRpg/pull/243), started the same day). The user found the jitter in the [GASP-12 check](gasp-walk-gait.md#limits) and chose a separate package before `GASP-13`. This report defines the package and is the task report for the implementation PR.

## Goal

On a listen server, the host sees a client's CMC character turn in place as smoothly as the other clients do. The fix changes only how the host presents remote characters. Movement, the server's gameplay timing, the owner's view and the other clients' views stay as they are.

## Why

- A client in the combat stance who only turns the camera while standing jitters in the host's view. In the client's own view and on other clients, the same turn is smooth. Turning the host's character shows nothing on the clients.
- Listen servers are the expected co-op setup, so the host sees this whenever a companion aims or looks around.

## Current state (checked 2026-10-13 on `master` `8a2ec4a9`)

Read in the engine source (UE 5.8.3) and the project code; the numbers come from the GASP-12 user-check probe.

| Area | Finding |
| --- | --- |
| Measured symptom | Three-player listen-server PIE at 60 FPS without latency; client A stands armed in the combat stance and turns the camera at 90, 180 and 45 °/s. A's pelvis reversed its yaw direction in 15, 41 and 10 of about 170 frames on the server (`master` `d0bddda7`; 43, 59 and 11 on the GASP-12 branch). On the owner it reversed 0 to 1 times, on the other client 0 to 2 times. |
| Controls | With `a.animnode.offsetrootbone.enable 0` the server showed 0 reversals. Turning off listen-server smoothing (`p.NetEnableListenServerSmoothing 0`) did not help (32, 65 and 4). The Offset Root Bone CVar exists only in builds with animation debugging, so it is a diagnostic, not a fix. |
| Server pose ticking | `ACharacter::UpdateMeshAllowAutonomousTickPose` sets the mesh's `bOnlyAllowAutonomousTickPose` for a remotely controlled character that replicates movement. The mesh then skips its own pose tick, and the CMC ticks the pose once per received move with that move's delta: after physics in `MoveAutonomous`, before physics while root motion plays. Queued animation events dispatch right after each move. |
| Existing guard | `URpgAnimInstance::CanRunParallelWork` runs these per-move graph updates immediately on a listen server, so every move's delta reaches the graph ([listen-server timing](listen-server-animation-timing.md), tests `SurvivalRpg.Animation.ListenServer`). `ABP_Unarmed` and the GASP AnimBPs share this base. |
| Pose evaluation | The mesh evaluates the graph in its own tick (`RefreshBoneTransforms`), at most once per frame, however many moves arrived. A server frame therefore sees zero, one or several graph updates per evaluation. |
| Offset Root Bone | `FAnimNode_OffsetRootBone` overwrites its private `CachedDeltaTime` in every update. Each evaluation takes the component transform the anim proxy cached, keeps the root where it was in the world, and in Interpolate and Release modes damps the offset towards the capsule with the last cached delta. The node assumes one update per evaluation. |
| Likely cause (hypothesis) | With several moves per frame, the node counters several moves of capsule rotation but damps with one move's delta. With no move in a frame, it evaluates again with a stale delta. The visible root therefore lurches between frames. Phase A confirms this; the ORB control above supports it. **Phase A disproved this hypothesis; see [Cause](#cause).** |
| CMC AnimBP | `ABP_RpgGasp_CMC` keeps the sample's Offset Root Bone path with project additions for block (`Get_OffsetRootRotationMode`, [block foot turn](gasp-block-foot-turn.md)). Its rotation half-life is 0.1; `GASP-13` plans 0.15 to 0.2. |
| Mover | The Mover pawn is not an `ACharacter` and ticks its pose per frame on the server. It is a control, not part of the fix. |

## Scope

1. **Phase A – baseline and cause, without code changes.**
   - Repeat the turn probe on `master` and record per server frame:
     - the number of pose ticks;
     - the deltas of the moves and of the frame;
     - the change of the component yaw;
     - the root and pelvis yaw;
     - the selected animation.
   - Measure at 45, 90 and 180 °/s with the client at 30, 60 and 120 FPS. These rates change how many moves arrive per server frame.
   - Measure the same pawn as owner, on the server and as a proxy on a second client.
   - Extend the matrix to see whether the host view also jitters elsewhere:
     - walking and running in the combat stance while turning;
     - curves in free movement;
     - starts and stops.
   - Read the Offset Root Bone setup in `ABP_RpgGasp_CMC`: modes per state, half-lives, maximum errors and evaluation mode.
   - Controls:
     - Offset Root Bone off;
     - the Mover pawn on a listen server;
     - the original GASP CMC character on a listen server in the reference project, if it runs there.
   - Write down the mechanism and the predicted effect of the fix before building it.
2. **Phase B – fix.** The smallest change that gives Offset Root Bone a consistent view on a listen server's remote characters, under these constraints:
   - The server keeps ticking the pose per received move. Montage time, notifies, root-motion consumption and event dispatch keep their per-move timing.
   - Owners, other clients, dedicated servers and standalone games keep their current path; the change is gated like the existing guard (listen server, authority, remote autonomous owner).
   - Offset Root Bone tuning stays with `GASP-13`. No half-life, mode or movement value changes.
   - No engine source change and no change to the Mover or baseline graphs.

   Directions for Phase A to weigh, with the evidence:
   - evaluate the remote character's graph after each move's update on the listen server;
   - a project-owned Offset Root Bone variant that damps with the summed delta since its last evaluation (the engine node keeps its delta private);
   - present the remote character once per frame with the summed delta while gameplay events keep their per-move timing, only if Phase A shows that notifies and root motion stay intact.
3. **Phase C – tests.**
   - A listen-server PIE test that turns a client in place in the combat stance and checks that the root and pelvis yaw keep turning in one direction on the server, as on the proxy.
   - A native test next to `SurvivalRpg.Animation.ListenServer` if the fix is a native seam.
4. **Phase D – validation.** The Phase A probe with the fix, the regression suites, a multiplayer probe and the user check.

## Out of scope

- Offset Root Bone tuning, Steering, Foley and camera (`GASP-13`).
- Dedicated-server presentation; a dedicated server renders nothing.
- Latency, jitter and loss (`GASP-VAL-02`).
- Other host-view artifacts that Phase A finds but that Offset Root Bone does not cause; they become findings.
- The Mover pawn and the baseline pawn, beyond regression checks.

## Decisions

None is needed before Phase A. The implementing chat asks the user before building if:
- the only working fix changes what the host sees compared with the other clients (for example different Offset Root Bone settings for remote characters); or
- the fix costs more than 0.1 ms of game-thread time per remote character on the listen server.

## Ownership

| Responsibility | Location | Reason |
| --- | --- | --- |
| When the server ticks remote poses | Engine CMC, unchanged | Gameplay timing of montages, notifies and root motion |
| Presentation timing on the listen server | C++ `URpgAnimInstance` or a narrow native seam beside it | Engine-facing scheduling, like the existing guard; worker-thread safety |
| Offset Root Bone path and tuning | `ABP_RpgGasp_CMC` | Designer-owned graph; tuning belongs to `GASP-13` |
| A project node variant, only if Phase A picks it | Native AnimNode with its editor node | An engine-facing custom node is native by the GASP rules |
| Tests | `RpgAnimInstanceTests.cpp` for the native seam; a CMC PIE test in `SurvivalRpgEditor/Private/Network` | Stable seam and listen-server contract |

File ownership while the package runs:
- `Source/SurvivalRpg/Animation/RpgAnimInstance.*` and its tests and test types;
- a new native node or seam in `Source/SurvivalRpg/Animation`, only if Phase B needs one;
- `ABP_RpgGasp_CMC`, only to swap in such a node without retuning it;
- a new CMC listen-server turn test in `SurvivalRpgEditor/Private/Network`;
- [listen-server timing](listen-server-animation-timing.md), this report and a manifest under `docs/assets`.

`GASP-13` changes the same AnimBP and its Offset Root Bone tuning; it starts after this package merges.

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Host view | Standing in the combat stance and turning at 45, 90 and 180 °/s, with the client at 30, 60 and 120 FPS, the root and pelvis on the server reverse no more often than on the proxy in the same run (0 to 2 per run in the baseline). The user sees no jitter in the host view. |
| Same presentation | On the server, the root's lag behind the capsule and the turn-in-place clips stay within the range the proxy shows in the same run. |
| Other motion | Walking, running, curves, starts and stops on the server are not worse than in Phase A. |
| Unchanged roles | Owner and proxy measurements match Phase A within run-to-run noise. Dedicated-server and standalone paths are untouched by construction and named in the PR. |
| Gameplay timing | The CMC sprint, mantle, vault and hurdle PIE tests, block prediction, block locomotion and `SurvivalRpg.Animation.ListenServer` pass. The remote melee test keeps its known time-dependent rate. |
| Cost | Game-thread animation time per remote character on the listen server is measured before and after; anything above 0.1 ms needs the user's approval. |
| Content | No Offset Root Bone tuning, movement value, Mover or baseline graph changes. |

## Verification

- `python Build/Tools/Unreal/ue.py build` after C++ changes.
- Focused automation through `ue.py test`:
  - `SurvivalRpg.Animation.ListenServer` and the new turn test;
  - `SurvivalRpg.Movement.CMC` and `SurvivalRpg.GASP.CMC`;
  - `SurvivalRpg.Combat.Block.Prediction` and `SurvivalRpg.Combat.BlockLocomotion`;
  - `SurvivalRpg.GASP.Mantle`, `SurvivalRpg.GASP.Vault`, `SurvivalRpg.GASP.Hurdle` and `SurvivalRpg.Network`;
  - `SurvivalRpg.GASP.Mover` and `SurvivalRpg.GASP.VariantComparison` as regressions.
- The Phase A probe repeated with the fix: three players, the turning pawn recorded as owner, on the server and as a proxy, plus a late joiner.
- A user check on `Lvl_RpgGaspCMC` with two players, as host and as client.

## Repro (after implementation)

1. Set PIE to Play As Listen Server with two players and open `Lvl_RpgGaspCMC`. The GASP test maps spawn armed, so both characters stand in the combat stance.
2. In the client window, stand still and turn the camera slowly, then quickly, in both directions.
3. In the host window, the client's character turns in place smoothly, with the same turn steps the client sees. Repeat with the host turning while the client watches; nothing changes there.
4. Walk and run in circles as the client; the host view looks the same as before.

## Results

Commits on `claude/gasp-stab-04-host-view-jitter`: `cb8adae2` (start), `20aef8e6` (fix), `b97a8d67` (tests), and the commits listed in the PR. Raw probe data are in `Saved/GaspStab04` of the worktree `musing-ishizaka-7bac20`; the [manifest](assets/gasp-stab-04-host-view-turn.json) holds the summaries.

### Probe

- Single-process listen-server PIE on `Lvl_RpgGaspCMC` with three players, without latency. `t.MaxFPS` 30, 60 or 120 applies to every world of the process.
- Client A stands armed in the combat stance. The probe turns its control rotation, as mouse look does, at 45, 90, 180 and −90 °/s for 3 s each, with 1 s holds between.
- After every frame, A's pawn is recorded as owner, on the server, and as a simulated proxy on client B.
- **Direction changes:** consecutive frame steps of the root bone's world yaw with opposite signs, both above 0.05°.
- **Roughness:** the mean change of that per-frame step, in degrees per frame.
- The first 10 frames of each window are skipped. Windows have 80 frames at 30 FPS and 165 to 195 frames otherwise.
- The hip and shoulder lines, measured between the thigh and upper-arm sockets, behave like the root. Euler yaw of the pelvis and spine bones is ill-conditioned on this skeleton and is not used.

### Phase A – baseline

On `master` code, the server's root changed direction in these windows; the owner and the proxy showed 0 in every window:

| FPS | 45 °/s | 90 °/s | 180 °/s | −90 °/s | Roughness (°/frame) |
| --- | --- | --- | --- | --- | --- |
| 30 | 0 | 14 | 16 | 7 | 0.04–0.11 |
| 60 | 15 | 49 | 90 | 21 | 0.05–0.29 |
| 120 | 190 | 188 | 186 | 191 | 1.08–4.21 |

Controls at the same rates (server direction changes, 45/90/180/−90 °/s):

| Control | 60 FPS | 120 FPS |
| --- | --- | --- |
| Offset Root Bone off (`a.animnode.offsetrootbone.enable 0`) | 0/0/0/0 | – |
| Listen-server smoothing off (`p.NetEnableListenServerSmoothing 0`) | 37/75/103/83 | 5/3/7/3 |
| Smoothing off and `a.ParallelAnimEvaluation 0` | 28/58/73/70 | – |
| Rotation smoothing time 0 only (temporary class default) | 46/94/100/79 | 8/5/10/9 |

Other observations:

- **Moves per frame:** at 60 FPS, the server applied one move per frame in 163 of 164 frames and still jittered. Several updates per evaluation are therefore not the cause.
- **Moving:** at 60 FPS, the server's root yaw while running and turning was close to the owner's (4 direction changes against 0 at most). Root translation, starts and stops matched the owner. At 120 FPS, the root yaw changed direction 55 to 59 times against 0 while running and turning in curves.
- **Feet in a standing stance turn:** on every role, Motion Matching kept `M_Relaxed_Stand_Idle_Loop` for whole turns of up to 540°. The root and hip line stayed at the same world yaw, and the shoulders varied by about ±15°. See [Limits](#limits).

### Cause

The [definition's hypothesis](#current-state-checked-2026-10-13-on-master-8a2ec4a9) was wrong. Two effects add up:

1. **The pose caches the mesh from before the move.**
   - `UCharacterMovementComponent::ServerMove_PerformMovement` wraps `PlayerController::UpdateRotation` and `MoveAutonomous` in a deferred `FScopedCapsuleMovementUpdate`.
   - The capsule turns at once, but its child mesh follows only when the scope ends. `MoveAutonomous` ticks the pose inside the scope.
   - `UAnimInstance::PreUpdateAnimation` therefore caches the mesh's component transform from before the move in the anim proxy. `FAnimInstanceProxy::PreEvaluateAnimation` re-caches that transform only while root motion plays.
   - Offset Root Bone evaluates against the cached transform and holds the root one move behind the rendered mesh. Each change in move length shows as jitter.
   - Evidence, with smoothing off: in every frame, the server's root yaw step equalled the capsule's yaw step in that frame minus the step in the previous frame, to three decimals. At the same time, the AnimBP's `CharacterTransform`, which reads the actor, was current.
2. **Listen-server rotation smoothing moves the mesh after the pose cached it.**
   - `SmoothCorrection` runs after the pose tick, and the CMC tick then decays a mesh rotation offset over 0.033 s.
   - Offset Root Bone holds the root against the cached transform, so the root follows the smoothing offset.
   - At 120 FPS the client sends moves every second frame, which turns the offset into a sawtooth.
   - At 30 FPS a 33 ms frame exceeds 0.033 s, the rotation snaps, and little jitter remains.

The controls agree with both effects:
- Smoothing off removes effect 2. At 120 FPS, effect 1 alone is small, because the moves there are of equal length.
- Rotation smoothing off alone makes 60 FPS worse: the smoothed mesh had partly hidden effect 1.

### Phase B – fix

- **Native:** `URpgAnimInstance::PreUpdateAnimation` calls `GetSkelMeshComponent()->UpdateComponentToWorld()` before the engine caches the transform.
  - It does this only while `IsRemoteAutonomousPoseTick` holds: listen server, authority, remote autonomous owner, and the mesh inside its movement-owned autonomous pose tick. `CanRunParallelWork` uses the same scope.
  - The mesh then takes the transform the deferred scope would give it a moment later. The scope's own update finds nothing left to change.
  - A `URpgAnimInstance::MoveMeshWithCapsule` CPU trace scope makes the cost visible in Unreal Insights.
- **Data:** in `BP_RpgGasp_CMC`, CharacterMovement's `ListenServerNetworkSimulatedSmoothRotationTime` changes from 0.033 to 0.
  - Offset Root Bone already smooths the visible turn in every rotation mode.
  - The mantle pawn and the three `GASP-08` pilot pawns inherit the value.
  - Location smoothing stays at 0.04 s. The `ARpgCharacter` default stays 0.033 for pawns without Offset Root Bone.
- Neither part is enough alone. With only the native fix and default smoothing, the server showed 49/84/79/69 direction changes at 60 FPS and 175 to 184 at 120 FPS.

With both parts, the turn probe gave:

| FPS | Server direction changes, all four rates | Server roughness | Owner and proxy |
| --- | --- | --- | --- |
| 30 | 0 | 0.00 | 0 |
| 60 | 0 | 0.00 | 0 |
| 120 | 0 | 0.00 | 0 |

- **Moving at 60 FPS:** the server's root yaw and translation equal the owner's, while running and turning, in curves, at starts and at stops.
- **Moving at 120 FPS:** root yaw direction changes while running and turning fell from 55 to 59 to 0.
- **What remains at 120 FPS:** with both parts, at 120 FPS the hip and shoulder lines and the root translation still step at the client's 60 Hz send rate: hip roughness 1.1 against 0.36 on the owner while running in curves, and shoulders 0.3 to 1.0 °/frame against below 0.12 while turning in place. These steps do not go back and forth. They do not depend on Offset Root Bone; see [Limits](#limits).

### Phase C – tests

- **`SurvivalRpg.Animation.ListenServer.CachesMeshTransformAfterDeferredMove`** (native):
  - ticks a remote autonomous pose inside a deferred `FScopedMovementUpdate` and checks that the proxy caches the moved mesh;
  - an engine-baseline control, using the identical proxy, still caches the transform from before the move;
  - outside the pose tick, and as client, dedicated server or standalone, the engine's cached transform stays unchanged.
- **`SurvivalRpg.GASP.CMC.HostView`** (PIE):
  - listen server with two clients; the armed owner turns in place at 90 and 180 °/s;
  - the host's root yaw may change direction and step no more than the proxy's;
  - a late joiner then turns at 90 °/s with the same check.
- **Mutation checks:** both tests failed without the native fix (14 direction changes on the host against 0 on the proxy; roughness 0.112 against 0.000). The PIE test also failed with the native fix but the asset's smoothing value restored to 0.033 (90 against 0; 2.077 against 0.000). Both mutations were reverted.

### Phase D – validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed after every change.
- **Regression** on the fix (`20aef8e6`) with the tests of `b97a8d67`:

  | Suite | Result |
  | --- | --- |
  | Native, `--null-rhi`: Movement, Equipment, Harvesting, AbilitySystem, block lifecycle and prediction, Dodge, ability and weapon contracts, BlockLocomotion validation, CMC asset, speed and sprint contracts, Mantle asset composition, Input, UI.Input | 135/135 |
  | `SurvivalRpg.Animation` | 4/4 |
  | `SurvivalRpg.GASP.CMC`, including `HostView`, `Sprint` and `SpeedParity` | 12/12 |
  | `SurvivalRpg.Combat.BlockLocomotion` | 15/16 |
  | `SurvivalRpg.GASP.Mantle` | 13/13 |
  | `SurvivalRpg.GASP.Vault` | 9/9 |
  | `SurvivalRpg.GASP.Hurdle` | 12/12 |
  | `SurvivalRpg.GASP.RuntimeRetarget` | 2/2 |
  | `SurvivalRpg.GASP.VariantComparison` | 3/3 |
  | `SurvivalRpg.Network` | 19/19 |
  | `SurvivalRpg.GASP.Mover` | 67/67 |

  - The one failure was `MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease`. It timed out in the run and passed 1 of 2 times alone.
  - It is the known time-dependent Mover case from the open findings. The Mover pawn is an `AModularPawn` and never takes the new code path.
- **After the trace scope and the late-join stage:** `SurvivalRpg.Animation.ListenServer` and `SurvivalRpg.GASP.CMC.HostView` passed 5/5 again.
  - The host showed 0 direction changes and 0.000 roughness for the first client at 90 and 180 °/s and for the late joiner at 90 °/s, like the owner and the proxy.
- **Cost:** a `Trace.File cpu` capture of a 60 FPS three-player turn probe, exported with `UnrealInsights -NoUI`.
  - `URpgAnimInstance::MoveMeshWithCapsule` ran 4,700 times: 0.009 ms on average, 0.003 ms median, 0.234 ms at most.
  - With about one received move per frame, that is about 0.009 ms per remote character per frame, far below the 0.1 ms limit.
- **Controls with the fix:**
  - **Mover pawn:** turning with block held on `Lvl_RpgGaspMover`, the host, the owner and the proxy matched (0 direction changes, roughness 0.05 to 0.30 on every role). The Mover path never had the problem.
  - **CMC pawn, block held, 60 FPS:** the host matched the owner (0 direction changes).
  - **CMC pawn, block held, 120 FPS:** 0 direction changes, but the root steps at the 60 Hz move rate (roughness 1.1 to 4.5 against 0.1 to 0.3 on the owner).
  - **Same, rotation smoothing temporarily restored:** the block turn jittered again (15 to 20 direction changes at 60 FPS, 64 to 78 at 120 FPS). Turning rotation smoothing off therefore does not make blocking turns worse.
- **Original GASP:** not run. The reference project enables neither Python nor MCP, and a scripted listen-server probe there would need plugin changes in an external checkout. The mechanism lives in engine CMC and animation code that every CMC character shares.
- **User check:** pending.

### Deviations from the definition

- The cause differs from the definition's hypothesis; see [Cause](#cause).
- **Asset change:** the fix changes `BP_RpgGasp_CMC`, which the definition did not list in the file ownership. The setting is designer-owned pawn data on the blueprint that owns the Offset Root Bone AnimBP. `ABP_RpgGasp_CMC` did not change.
- **Trace scope:** a CPU trace scope was added to measure the cost.
- **Probe order:** the controls ran before the fix was built. The prediction that both parts are needed came from the control table above.
- **Original GASP:** the control in the reference project did not run; see [Phase D](#phase-d--validation).
- **Late joiner:** covered by the PIE test, not by the Python probe.

### Failed attempts

- Overriding `ClientNetSendMoveDeltaTime` with the `set` console command had no measurable effect on the move rate, and those runs are not used. `GameNetworkManager`'s properties are not readable from Python.
- The definition-time metric used the Euler yaw of the pelvis. On this skeleton that yaw is ill-conditioned and showed reversals on every role, so the probe switched to the root bone and to lines between sockets.

### Limits

- **User check:** pending; see [Repro](#repro-after-implementation).
- **Late join:** the PIE test checks a late joiner's turn. The Python probe ran without one.
- **60 Hz steps:**
  - A host above 60 FPS still shows remote characters' poses and root translation in 60 Hz steps. The server ticks those poses only when moves arrive, and clients send at most every 0.0166 s by default.
  - It is most visible when the root follows the capsule, for example turning with block held at 120 FPS: roughness 1.1 to 4.5 °/frame against 0.1 to 0.3 on the owner, without direction changes. Rotation smoothing hid part of this before, at the cost of jitter.
  - Offset Root Bone does not cause this. It needs its own package if it is visible, for example a different client send rate on listen servers or interpolated presentation.
- **Standing stance turns:** on every role, a standing combat-stance character does not turn its feet or hips with the camera, even over 540° of turning.
  - Motion Matching keeps the idle loop.
  - Offset Root Bone in Accumulate mode, without a maximum rotation error, holds the root in the world.
  - This predates the package and is not host-specific. It belongs to `GASP-13` or its own package after a user check.
- **Dedicated servers:** they keep the engine behavior; their remote poses stay one move behind their capsule. Nothing renders there, but server-side bone or socket queries on remote characters see the old transform.
- **Other bases:** the native fix applies only to animation bases derived from `URpgAnimInstance`. The GASP sample copy `SandboxCharacter_CMC` keeps the engine behavior.
- **Network conditions:** single-process PIE without latency, with the same frame rate in every world. Different client and host frame rates, latency and loss stay with `GASP-VAL-02`.
