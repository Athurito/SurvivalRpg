# GASP-STAB-04 – Host-view jitter of remote characters turning in place

Status: **Ready** (defined 2026-10-13). The user found the jitter in the [GASP-12 check](gasp-walk-gait.md#limits) and chose a separate package before `GASP-13`. This report defines the package and is the task report for the implementation PR.

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
| Offset Root Bone | `FAnimNode_OffsetRootBone` overwrites its private `CachedDeltaTime` in every update. Each evaluation counters the whole change of the component transform since the last evaluation, then damps the offset towards the capsule with the last cached delta (Interpolate and Release modes). The node assumes one update per evaluation. |
| Likely cause | With several moves per frame, the node counters several moves of capsule rotation but damps with one move's delta. With no move in a frame, it evaluates again with a stale delta. The visible root therefore lurches between frames. Phase A confirms this; the ORB control above supports it. |
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

Not started.
