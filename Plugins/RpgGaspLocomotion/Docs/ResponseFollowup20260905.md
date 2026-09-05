# GASP response follow-up, 2026-09-05

Tracking: [PR #122](https://github.com/Athurito/SurvivalRpg/pull/122),
[#55](https://github.com/Athurito/SurvivalRpg/issues/55),
[#99](https://github.com/Athurito/SurvivalRpg/issues/99).

The user still observed buggy rapid Combat turns and running legs after a jump after
`7f815acf`. The earlier 39 native and 11 PIE passes validated bounded mechanisms, not those
visible outcomes. The old second turn input arrived 1.8 seconds after the first, usually
after Idle; Stop/Walk inputs changed in flight rather than during a Run landing.
The earlier review must not be read as acceptance of these complaints.

## Reproduced causes

| Finding | Evidence and correction |
| --- | --- |
| Turn heading counted twice | Pose History recovers the current root offset to its reference rotation over 0.3 s. Heading already sees the outstanding root-to-target angle through this recovery. Adding the synthetic trajectory yaw ramp asks for that rotation a second time. The baseline repeatedly selects `Turn_180_R` for Query +90. With evaluated root feedback, future stationary facing uses the desired mesh-world orientation; Pose History contributes the recovery once. |
| Immediate counterturn searches in the previous pose's wrong basis | The collector receives trajectory during Update, but component-space bone history during Evaluate. Applying a new mesh facing to the old root pose changes its reconstructed world orientation. A first candidate still chose `045_R` for a -99.38 degree actual gap: old mesh 135 minus root 99.38 stores -35.62 in component space; replacing mesh with 0 reconstructs -35.62 and requests +35.62. Proxy PostEvaluate now captures the component basis paired with history. The next trajectory keeps this basis at t=0 and uses the new target for future samples, yielding the actual -99.38 degree request without waiting for another evaluation. |
| Active turns discard new goals | The selected-turn mode copied further component rotation into the simulated root, while the actor-yaw accumulator kept growing. No latched request could retarget; recovery then discarded accumulated yaw. Actual evaluated root-facing now defines the outstanding angle. Opposite changed goals can replace an obsolete turn; additional same-direction input survives to the authored marker and can start a follow-up without an intermediate Idle selection. An unchanged goal enters ordinary recovery instead of generating further clips from blend-induced root remainder. |
| Half-turn signs disagree across peers | In the reduced-rendering 60-FPS capture, Owner/Server query -180 while Late queries +180; all select the same 180_R and consume the same root rotation. A signed-query gate gave only Late an extra 045_R. A per-request direction now latches the first clear evaluated root progress for half-turns, retaining it across yaw wrap for counter-input and changed-goal follow-up. Both representations of an unchanged half-turn enter recovery at the marker. |
| Server evaluations replay the same root-motion delta | Remote autonomous server meshes receive animation updates from CMC moves but can refresh their bones again without a new move. SequencePlayer then emits the same `DeltaTimeRecord` as root-motion attributes, and OffsetRoot consumes it again. In `response_pair_fixed-20260905-190540-a29b31e2`, server rows 20367/20372 retain Update 4914 and asset time 0.192986 while bone revision/frame advance and the root rotates another 4.765 degrees. Nine such active-turn repetitions are observed after late join, despite the broad turn checks passing. The server proxy now retains the complete evaluated pose, curves and attributes for the same update, initialization, main graph root and mesh/skeleton, remapping changed required bones without repeated procedural evaluation. |
| Multiple server updates lose intermediate root-motion deltas | CMC can process several moves before the next bone evaluation. Each sequence tick overwrites `DeltaTimeRecord`, so a later evaluation consumes only the last move's root delta. In `response_final_matrix-20260905-192346-33204843/fps-60`, the replay cache prevents all 838 repeated evaluations from advancing the root, but two server turns still converge too late. At 18.335 s the whole interval in the unchanged compressed root track contains 9.9086 degrees; the observed 3.3869 degrees matches only its final 1/60 s (3.3854 degrees). The server consequently needs an extra 45-degree clip. |
| Bone-cache refresh can replay a completed update | The first cache implementation treated a changed required-bones counter as permission to evaluate again. A real SequencePlayer/OffsetRoot regression reproduces another 17.429663 degrees at unchanged asset time 0.05 and Update 0 after a bone recache. The retained baseline is `GaspResponseRecacheBaseline20260905`; 50 other tests pass. No LOD change was measured in the gameplay captures, so this is a native seam regression rather than an attributed video cause. |
| Stop clips continue stepping after CMC stops | Post-touchdown input release immediately replaces RunLanding with RunStop, but the Stop then stays selected at speed zero, with further foot-contact changes. Logical Idle began on input release, so physical standstill creates no later domain interrupt. Stop continuing-pose competition is now released at the existing horizontal stillness tolerance, retaining the normal BlendStack crossfade to Idle. |

The landing timer itself is not changed again. In the new baseline, releasing input 0.1 s
after a selected Run landing immediately selects Stop; Walk similarly selects Run-to-Walk.
The long held-input landing did not reproduce running in place. Stop continuation does.

## GASP and ownership

The comparison is still `D:/Repos/GameAnimationSample` at `a9f56035`; its CMC AnimBP has local
modifications recorded in the earlier review. No claim of pristine Epic parity is made.
UE 5.8.1 source establishes the math:

- `PoseSearchFeatureChannel_Trajectory.cpp` builds root-relative Heading channels.
- `PoseSearchContext.cpp` computes sample rotation relative to origin rotation.
- `PoseSearchHistory.cpp` combines trajectory-world facing with component-space root facing
  and recovers the latter towards the reference root. Beyond the recovery horizon the old
  ramp therefore yields `root gap + extra trajectory rotation`.
- The collector's documented t=0 sample represents the previous simulation frame. Its
  component basis is retained until the next evaluation, including multiple server-move
  updates. Future samples supply the current desired mesh-world facing. The schema's
  +0.35/+0.7 s heading channels then see just the actual root gap, including nonidentity mesh
  bases and yaw wrap. At exactly 180 degrees the endpoint heading cannot distinguish the
  left and right half-turn; a specific side is not asserted from the endpoint alone.
- Curated 45/90/135/180 root tracks have their named magnitudes. No new retargeting defect
  is established, and no retargeted asset is changed in this follow-up.

The Stop-at-standstill rule is an intentional responsive RPG adaptation. Local GASP also
permits continuing Stop poses while logical Idle remains unchanged. This change does not
alter CMC acceleration, braking, inventory weight or gear rules.

Idle acceptance uses the actual authored database membership, not just folder names.
`PSD_Rpg_Stand_Idle` contains `M_Neutral_Stand_Idle_Loop` and
`M_Neutral_Transition_Crouch_to_Stand`; the latter can recover from a bent landing pose.
The read-only export `Saved/Reviews/gasp-idle-contract20260905.t3d` and compressed-pose
samples in the sibling JSON confirm that contract. At the sampled times the transition has
zero root translation and both contact curves are 1; the pelvis rises from 43.63 cm to 90.40 cm by 0.7 s and
92.12 cm by 1.0 s. It is a one-way standing recovery, not continued locomotion. The checker
now includes that exact existing asset; it does not permit arbitrary crouch or Stop clips.
This corrects eight folder-classification false failures in the paired-history capture.

After a material opposite target change of at least 30 degrees, a remaining opposite root
gap below the existing 30-degree activation threshold intentionally recovers instead of
launching another turn. A target change below 30 degrees does not itself cancel an active
turn immediately. The checker accepts recovery only after the old turn
exits within the unchanged response budget, the root moves towards the stable new target,
and the gap reaches 10 degrees within the recovery/blend duration plus two measured frames.
Larger counter-input still requires a correctly directed turn clip. This resolves the
cache-only 30-case's -29.52-degree counter-input classification; it does not waive its seven
remaining settling failures. Synthetic negative cases reject wrong-way motion, stale clips,
late release/convergence and an unstable goal.

Counter-input timing also distinguishes gameplay target from mesh network smoothing. On the
late observer, the actor can already have its new facing while the mesh is still smoothing
from the old one. Counter direction is therefore checked against that new actor target plus
a measured stable mesh basis, with unchanged timing budgets. An unclear basis fails the
check. This corrects two false failures in the reduced-rendering 60-case: both left counter
clips actually arrive within 216–217 ms, although the old immediate mesh angle suggested
the opposite direction. No runtime smoothing is disabled for this measurement.

Reusable mechanisms stay native: the AnimInstance binds its unique Offset Root node on
initialization; Proxy PostEvaluate snapshots its world rotation and corresponding component
basis together. Game-thread PreUpdate publishes the paired values after the engine join;
it never combines new update-only root-node state with old evaluated history.
A preceding MM traversal under the root provider and traversal
counters guard that value. Initialization, irrelevance and movement discontinuities fall
back safely. Worker updates receive only value snapshots. Existing profile thresholds and
per-clip recovery markers remain designer-owned. No new manager, ability or asset is introduced.

The server evaluation cache applies only to the main AnimInstance of an opted-in GASP mesh
using montage-only gameplay root motion on a remote autonomous server Character, with
`bOnlyAllowAutonomousTickPose`. Both PreUpdate and PreEvaluate refresh its
game-thread scope. The worker cache key includes the main graph root, update/initialization
counters, and mesh/skeleton identity. Scope, owner, lifecycle and presentation
discontinuities invalidate it. Owner and simulated-proxy clocks remain unchanged; this does not
advance animation on render time or change CMC/montage root-motion consumption.

A required-bones refresh within the same update remaps the held pose instead of evaluating
again. This follows UE's `FBlendStackAnimPlayer::RestorePoseContext`: known bones retain their
local transforms through skeleton indices, newly required bones use the current local reference
transform until a real update, curves remain held, and mesh-indexed attributes remap into the
current compact indices. The original cache survives full/reduced/full mappings in one update.
This preserves the animation clock; it does not promise visually identical newly added LOD bones.

Completed CMC animation updates also evaluate the graph immediately in that same narrow server
scope, before a later move can overwrite the sequence's extraction interval. The engine's
game-thread `PostUpdate` hook runs after the worker join and slot-buffer flip. It fills the
pose cache and captures the evaluated root/component pair without writing mesh bones, dispatching
notifies or calling `PostEvaluate`/`ClearObjects` inside the still-running montage post-update.
The later normal bone refresh consumes the cache. This adds pose evaluation per actual remote
move, not per render frame; scalability remains part of the cutover performance gate.

## Reproduction and acceptance

The revised launcher sends second yaw input after 0.45 s and changes Stop/Walk only after
physical moving touchdown. Verify actual clips rather than relying on phase names. Network
simulation and the hitch can now be disabled independently to isolate presentation response.

Baseline: `Saved/Reviews/GaspMultiProcess/response_baseline-20260905-180624-90c852f7/fps-60`.
Runtime `7f815acf`, unchanged content, revised inputs; three rendered editor processes with
listen server, owner and late observer, requested cap 60, zero simulated lag/loss, no hitch.
Owner: +90 request at 8.094 s selects `Turn_180_R`; additional +90 at 8.462 s does not requeue
before recovery. Opposite input at 11.462 s leaves the right-turn clip selected until 12.178 s.
RunLanding at 35.032 s changes to RunStop on release at 35.132 s, but Stop still has full
scalar contribution after physical speed reaches zero at 35.399 s. Across recorded peers,
Stop remains selected for 1.28 to at least 1.60 s after standstill; actor translation is zero,
the search list contains only Idle, and foot-contact curves continue changing.

The first candidate (`response_fixed-20260905-183125-19dee498/fps-60`) corrected nominal
turn magnitudes and every observed Stop/Walk response, but still failed 11 turn checks.
It exposed the history-basis counterturn defect above and was not accepted as fixed.
The later lifecycle correction also handles an exactly 180-degree changed goal whose
signed target delta is ambiguous while the evaluated root gap already has a clear side.

The cache-only matrix is retained as failed intermediate evidence, despite its `final_matrix`
directory label. At caps 15 and 30, all 65 Stop/Walk assertions per capture pass. Several turn
settling checks still fail; the 30-case includes four sustained server residuals of 17–36
degrees with multiple updates per evaluation. Five 15-case failures cross below 10 degrees
one frame after the asserted settling window; no coalesced turn updates are observed there.
Its 120-case fails before late-client PIE startup and is not an animation acceptance run.
The intended 5-FPS editor-startup override lacked section brackets, so UE silently ignored it
and still waited for 10 FPS. The launcher now uses the engine parser's required `[section]:key`
syntax. Earlier run metadata records the intended value, not proof that it took effect.

The per-move-evaluation capture `response_every_move-20260905-194752-5ab8c5cc/fps-60` has
65/65 Stop/Walk checks and no replay in 421 repeated server evaluations. In its 90-degree
turn, 18 observed multi-update intervals still produce the same root remainder at recovery:
Owner 26.55034 degrees, server 26.55070 degrees. Both select 090R then Idle; neither needs
the former extra 045R. The same-direction follow-up also selects 090R, 135R, then Idle on
both peers. This supports the intermediate-delta correction in an actual network session.
However, six of 382 checks remain red: four first samples in the settling window exceed
10 degrees before crossing below on the next sample; one late-client follow-up settles
later; one has only 279 ms of the required 300 ms observation coverage. Actual frame rates
are Owner 26.82, server 29.77, late client 8.12 FPS despite a 60 cap. This capture cannot
establish 15/30/60/120-FPS response. Its startup log now confirms the 5-FPS readiness setting.

Final native build: `Saved/Logs/GaspResponseHalfTurnBuild20260905.log`, successful.
After consolidating the unpublished commits, the clean-working-tree Unity build also succeeds:
`Saved/Logs/GaspResponseCleanUnityBuild20260905.log` (97.72 s). It retains the existing
`GetMovementBase` deprecation warning in the rendered PIE fixture; no source change was needed.
`Saved/Automation/GaspResponseHalfTurnNative20260905/index.json`: 53 passed, including five
root-feedback, three Stop-playback and six real Engine server-evaluation regressions;
two existing asset tests passed with warnings. No failures or skipped tests.
The server-evaluation tests use real SequencePlayer and OffsetRoot nodes, demonstrate duplicate
root motion in an uncached control, and verify held bones, curves, attributes and root facing,
next-update progress, scope isolation and initialization/root invalidation. Bone recache and
changed compact-bone mappings hold/remap poses without another root delta; the former
17.429663-degree native replay is now exactly zero.
The multiple-move case compares two and four real updates within the same engine frame against
rendering after every move, including full pose/curve/attribute equality and the montage-only
extraction policy: locomotion attributes do not enter authoritative extracted root motion.
That native batch test does not play an active root-motion montage; DefaultSlot playback
and combat cancellation are separate rendered PIE coverage.
Half-turn regressions cover both query signs and both physical directions at 15/30/60/120
frame steps: unchanged targets recover at the marker, additional 60/90-degree input requests
the remaining angle, counter-input replaces an obsolete clip, and overshoot cannot reverse the
latched direction or create an endless sequence of requests.
The editor test unity regrouping exposed an existing ambiguous
`PrototypeExperiencePath`; its use in the indicator fixture is now namespace-qualified.
The GASP AnimBP compiles against this build and the existing presentation profile loads;
`Saved/Reviews/gasp-response-half-turn-asset-validation20260905.json` records success with no saved
packages. Existing asset contracts ran in the native suite. No retargeted clip or content asset
changes in this follow-up. Source hashes used for the final captures are retained in
`Saved/Reviews/gasp-response-half-turn-source-manifest20260905.json`.

`Saved/Automation/GaspResponseHalfTurnPIE20260905/index.json`: all 11 rendered PIE tests pass
with warnings, including correction/relevancy, late join, the DefaultSlot montage, combat-profile
reactivation, moving/rotated bases, real remote melee damage/cancellation and Prototype override.
The final isolated response matrix is
`Saved/Reviews/GaspMultiProcess/response_verified_matrix-20260905-203403-3b53f278`.
It uses three rendered processes, reduced rendering, zero simulated lag/loss and no injected
hitch. `Check-GaspResponse.py --require-animation-counters` writes each completed case's
`response-check.json` and `.md`. Requested caps are not measured frame rates:

| Requested cap | Actual FPS Owner / Server / Late | Response checks | Stop/Walk checks | Repeated server evaluations replaying root motion |
| --- | --- | --- | --- | --- |
| 15 | 12.98 / 12.99 / 12.98 | 374/382 | 65/65 | 0/3 |
| 30 | 23.96 / 23.94 / 24.04 | 382/382 | 65/65 | 0/100 |
| 60 | 46.03 / 47.05 / 46.27 | 382/382 | 65/65 | 0/540 |

In all three cases, Stop releases to a valid Idle candidate in the first observed CMC
standstill frame. Idle holds the scalar majority after 75–93 ms, compared with the baseline's
1.28 to at least 1.60 seconds before even selecting Idle. Every observed counter-input gets
the correctly directed clip within 0–78 ms. All 30 nominal half-turns across these cases
have one 180-degree clip followed by recovery/Idle, with no unsolicited 45/90/135-degree turn.

The eight failed checks in the requested-15 case remain failures. They are confined to the
fixed last-0.4-second root-convergence window: seven cross under 10 degrees one measured
frame later; one late-observer half-turn needs two frames. All end Inactive within 0.83–1.54
degrees of the target. This does not reproduce the former stale Stop, wrong counterclip or
extra-turn defects, but it does not establish the requested low-FPS timing acceptance either.
No checker threshold or presentation tuning was relaxed to hide the remaining delay.

The requested-120 case is incomplete. The late editor's Asset Registry scan takes 164.619 s
wall time for 11,542 uncached files (`late/Editor.log`, line 2450). The subsequent editor
frame-rate readiness check succeeds after 5 s at 39.32 FPS; it is not the failed gate.
Late joins successfully at 20:49:22 UTC and the repeated sequence starts at 20:49:24.098.
The server then reaches its global 300-second test deadline during `after_late/aim_stationary_180`
at 20:49:45 (`server/Automation/index.json`: test duration 300.362396 s). Only about 21 s of
the second sequence are present, so its later counter-input and landing cases are missing.
Measured pre-join Owner/Server rates are 98.35/98.12 FPS and partial post-join
Owner/Server/Late rates are 91.03/90.32/88.81 FPS. This is neither complete acceptance nor
evidence of sustained 120-FPS rendering; the timeout does not itself establish a gameplay defect.

The separate network/visual capture is
`Saved/Reviews/GaspMultiProcess/response_network_visual-20260905-205027-6c6719e2/fps-60`.
All three processes complete with the final runtime, cap 60, reduced rendering, per-peer
60 +/-10 ms packet lag, 10% packet loss and the intentional 150 ms hitch. Network conditions
and the hitch coexist in this stress run; the zero-netem matrix isolates response without
them, but this run alone cannot attribute every discrepancy to one stressor.
The checker reports 378/382, with every Owner/Server check passing and four Late failures.
Actual overall FPS are 52.40/52.81/41.34; after-join action phases run at 46.11/46.62/40.79.
The server has zero root replay in 1,358 qualifying repeated evaluations. The two observed
180-degree Late turns each play only 180_R and recover without another turn; the fixed
settling window nevertheless begins at 21.087/14.200 degrees and ends at 1.314/0.964 degrees.
These remain timing failures, including network/smoothing delay, not nominal-side failures.

Stop/Walk checks are 63/65. The two red Walk checks are both the Late observer's
`after_late/run_land_walk`: Ground Walk selection takes 1.376 s and scalar majority 1.470 s
from the first observed Walk gait. Its trace shows RunLanding at 40.189 s, Airborne at
40.206, WalkLanding at 40.233, Airborne again at 40.265–40.295, and another WalkLanding at
40.324. The capsule Z alternates between approximately 10092.140 and 10092.150 cm, without a
new jump arc; history resets stay at 2. Proxy snapshots consume CMC movement mode directly.
Thus the apparent recontacts create new landing requests before the original Run-to-Walk
transition can retain its context. The final new Walk landing opens Ground search at 40.630 s,
as specified, then continues winning until Ground Walk at 41.609 s. This is a separate
simulated-proxy contact-continuity finding, not a measured failure of the 0.3-second release
within one continuous landing. Both response checks remain red, and the capture does not
trace the raw replicated movement-mode byte or packet that caused each edge.
The concrete trace and next diagnostic contract are tracked in
[#126](https://github.com/Athurito/SurvivalRpg/issues/126).

All six Owner and server jumps and all three Late-observed jumps have actual airborne arcs
in this stress run; the missing-jump finding #125 does not recur here and is not closed.
The explicit hitch is recorded before/after Late on Owner and host, followed by measured
159–160 ms frames. Late is not deliberately slept. Additional 0.4-second maximum frames
occur with rendering/screenshot load and are not substituted for proof of the injected hitch.

All 60 requested PNGs are present. Nine were visually inspected: Owner/Server
`s0_04` (counterturn) and `s0_10` (post-landing Stop phase), Late `s1_04`, `s1_10`, `s1_11`,
Owner `s1_08` and Server `s1_09`. Their actual viewport is 640x448. The images show rendered
turn/recovery, jumping and landing poses with sword/shield; the inspected poses do not show
gross leg elongation. They are static samples with reduced rendering, an invisible collision
floor and some feet at/cut by the lower edge, so they do not establish contact, foot slip,
temporal smoothness or a complete retargeting/medieval-style approval.
The diagnostic completion marker alone is never animation acceptance. Clip selection,
actual root response and scalar contributions are checked separately; scalar weights
do not prove per-bone skinning, terrain contact or final visual polish.

## Remaining scope

- [#55](https://github.com/Athurito/SurvivalRpg/issues/55) retains actual frame-rate and
  performance acceptance, including sustained 120 FPS and low-FPS convergence timing.
  Native fixed-step tests cannot replace rendered evidence at the intended frame rate.
- [#99](https://github.com/Athurito/SurvivalRpg/issues/99) retains same-build Prototype/GASP
  visual approval, final per-bone skinning, foot slip and terrain IK.
- [#125](https://github.com/Athurito/SurvivalRpg/issues/125) tracks predicted jumps missing
  on the server under loss. No transport fix or blanket multiplayer clearance follows.
- [#126](https://github.com/Athurito/SurvivalRpg/issues/126) tracks near-zero-height proxy
  recontacts restarting Landing and delaying the observed Run-to-Walk response under loss.
  A fix must preserve real repeat jumps, ledge falls, moving bases and relevance transitions.
- [#123](https://github.com/Athurito/SurvivalRpg/issues/123) retains medieval stance,
  sword/shield footwork and further stationary-steering/presentation work. The basic
  response defects above are handled in this PR, not deferred to that style feature.
- [#124](https://github.com/Athurito/SurvivalRpg/issues/124) retains equipped-gear combat
  weight, Light roll/Heavy sidestep and predicted activation/profile changes. Ordinary
  inventory stays weightless; general load-dependent running inertia remains outside scope.
