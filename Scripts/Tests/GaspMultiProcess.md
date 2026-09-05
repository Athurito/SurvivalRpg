# GASP multi-process diagnostics

Build the Development Editor target first. Do not run this alongside another build, content-authoring
editor, or acceptance suite. The harness launches exactly three independent OS processes: a listen
host and one initial client, then a second client during movement. Each process owns one PIE world;
gameplay replication crosses the IP socket rather than sharing a CQTest world. This is **editor
multi-process evidence**, not packaged or Steam acceptance.

```powershell
& 'D:\Repos\SurvivalRpg\Scripts\Tests\Run-GaspMultiProcess.ps1' -Label before -Fps 60
& 'D:\Repos\SurvivalRpg\Scripts\Tests\Run-GaspMultiProcess.ps1' -Label after -Fps 15,30,60,120
& 'D:\Repos\SurvivalRpg\Scripts\Tests\Run-GaspMultiProcess.ps1' -Label visual -Fps 60 -CaptureScreenshots
& 'D:\Repos\SurvivalRpg\Scripts\Tests\Run-GaspMultiProcess.ps1' -Label response -Fps 60 -PacketLag 0 -PacketLagVariance 0 -PacketLoss 0 -NoHitch -Offscreen
& 'D:\Repos\SurvivalRpg\Scripts\Tests\Run-GaspMultiProcess.ps1' -Label reduced_response -Fps 15,30,60,120 -PacketLag 0 -PacketLagVariance 0 -PacketLoss 0 -NoHitch -Offscreen -ReducedRendering
python 'D:\Repos\SurvivalRpg\Scripts\Tests\Summarize-GaspMultiProcess.py' 'D:\Repos\SurvivalRpg\Saved\Reviews\GaspMultiProcess\<capture-directory>'
python -B 'D:\Repos\SurvivalRpg\Scripts\Tests\Check-GaspResponse.py' 'D:\Repos\SurvivalRpg\Saved\Reviews\GaspMultiProcess\<capture-directory>' --require-animation-counters
```

Use the **same requested FPS, map, assets, network settings and input schedule** for a before/after
comparison. The script never rebuilds or swaps binaries. Capture the baseline before changing its
runtime or build the diagnostic source against that baseline in an isolated checkout. Missing new
reflected diagnostic fields are written as `unavailable`; no new runtime hook is required. Record
the runtime commit and asset revision with each capture. Existing video alone cannot provide the
new CSV fields retrospectively.

`Check-GaspResponse.py` checks observed clip direction and size, response deadlines, settled root
facing, sustained Idle/Walk output and repeated server root motion without a new animation update.
It writes `response-check.json` and `.md` and exits nonzero for missing or failed required evidence.
Use `--require-animation-counters` for final captures; omit it only when inspecting older baselines
without those columns. Zero repeated evaluations means that case was not exercised, not that a
replay bug was excluded. The checker accepts the exact authored standing-recovery Idle asset and
does not require only the neutral Idle loop. It still cannot approve final skinning or foot polish.

The opposite-input response contract uses the **new actor target transformed by a proven mesh
basis**, with wrapped yaw differences across -180/180 degrees. The actor-to-mesh yaw basis is
inferred from at least three terminal observations spanning at least 75% of the 0.4-second settle
window. A preceding observation can provide the span at sparse frame rates; this does not extend
any response or root-convergence deadline. Actor yaw and relative mesh yaw must remain within
0.1 degree, and any earlier nearly constant plateau (at least three samples over 0.1 second,
actor/relative yaw varying at most 0.01 degree) must agree with that basis within 0.1 degree.
Conflicting or insufficient basis evidence fails the counter-response check. No mannequin-specific
rotation offset is assumed.

The target is the actor yaw on the observed counter-input frame plus this basis. The mesh may
still be rotating toward that target under network smoothing; its intermediate yaw is not the
requested target. Latency still starts at the actor-target change, and actor yaw must stay within
one degree of that request until the response/convergence. At or above native `TurnActivationThreshold`
(30 degrees), a newly selected counter-turn clip must have the correct direction within the
existing response budget: `max(0.3 seconds, blend time + two measured frames)`. A same-clip response
must visibly restart playback. Below 30 degrees, but at or above native `TurnCancelThreshold`
(10 degrees), the runtime also permits recovery instead of another authored turn. The checker
accepts that route only when all of these observations hold:

- The selected old turn is replaced by a non-turn pose in Recovering or Inactive within the same
  response budget; the state label alone is insufficient.
- From that release through convergence, selection stays non-turn and the actor target stays
  within one degree. Root yaw makes more than 0.1 degree of progress toward the fixed new target, with
  no measured backward step larger than 0.1 degree; the first release-frame step is included.
- The actual root gap reaches at most 10 degrees within
  `max(TurnRecoveryDuration, blend time) + two measured frames` after release. The native recovery
  default is 0.15 seconds and the pilot blend contract is 0.2 seconds. Missing samples, an old turn
  that remains selected, wrong-direction motion, and failure to converge all fail acceptance.

The existing `opposite_turn_selection_response` JSON check names the chosen response contract and
retains release/convergence CSV lines, timings, directed root progress, backward steps, target drift
and selection continuity. It also retains the inferred basis, supporting CSV span, stable-window
conflicts and fixed target yaw. Per-frame diagnostic `root_gap_degrees` still describes the currently
presented mesh; `actual_gap_at_request` and recovery `converged_target_gap_degrees` use the fixed
actor target. Defaults come from `RpgGaspLocomotionConfig.h`; use
`--turn-activation-degrees` and `--turn-recovery-seconds` only when the capture used other authored
tuning. This does not widen the independent settled-root window, landing, or replay contracts.

The launcher uses a unique directory per run and never deletes old evidence. It terminates only
the process handles it launched, including after failure. Each process gets a separate UserDir,
shader work directory, editor log and automation report. Standard rendered RHI is used with a
640x480 PIE viewport; optional `-Offscreen` renders without an interactive viewport. Do not substitute
NullRHI for rendered presentation acceptance.

`-ReducedRendering` optionally lowers render cost in all three processes with these transient
console commands, queued before Automation through the existing `-ExecCmds` argument:
`sg.ShadowQuality 0`, `sg.GlobalIlluminationQuality 0`, `sg.ReflectionQuality 0`,
`sg.PostProcessQuality 0`, and `r.ScreenPercentage 50`. `run.json` records
`reduced_rendering.enabled` and the exact `reduced_rendering.commands` list sent to each process;
without the switch that list is empty. The launcher does not save project graphics settings.
The four groups and their low-quality callbacks are defined in UE 5.8 `Scalability.cpp` and
`BaseScalability.ini`; `LegacyScreenPercentageDriver.cpp` defines the screen-percentage CVar.
Engine `ParseExecCommands.cpp` splits the queued graphics commands by comma, preserving the
existing Automation command's semicolon queue.

This option changes rendering quality to help collect animation-response evidence under load.
It does not change view-distance, texture or effects groups, skeletal LOD/update skipping,
animation timing, frame caps, or response thresholds. Compare captures using the same rendering
option. It cannot establish visual polish or a performance benchmark, and the measured CSV frame
deltas remain decisive: an observer running at 8 FPS does not prove a requested 15/30/60/120-FPS
case. The command list records what was submitted, not a readback of every effective renderer
setting; screen percentage can also be overridden by the Engine's dynamic-resolution policy.

The native opt-in filter is `SurvivalRpg.Network.GaspMultiProcess.DiagnosticRole`. It requires
`-GaspProcessRole=server|owner|late`, `-GaspTraceDir=<project Saved or Intermediate directory>`,
`-GaspTraceFPS=15|30|60|120`, and `-GaspTracePort=<port>`. A broad test invocation without a role
skips it. Server startup precedes the owner. `late.request` launches the observer; `resume.txt`
repeats the input sequence after that observer actually possesses its replicated pawn.
The summarizer uses the player ID in `owner.ready` to compare that same character across peers;
`--all-pawns` includes the host, late observer and unrelated AI. Read captures after the run has
finished. The CSV writer remains open with read sharing, but final flush/close and completion
markers determine whether evidence is complete.

The 48-second input schedule runs once before and once after late join: stationary CombatStrafe
and Aim turns of 45/90/180 degrees, then repeated and opposite requests 0.45 seconds after each
followup phase begins. These inputs are fixed in schedule time and do not depend on clip playback;
`tail_followup_triggered` records the second request and `selected_asset_fraction` records the
actual clip position. Verify the prior row is Active with a selected turn; the phase name alone
does not prove in-flight coverage. The older 1.8-second schedule mostly requested turns after Idle.
This is followed by Run-to-Jump with forward input held through landing, another jump
with Stop requested 0.1 seconds after physical moving touchdown, and another with Walk requested
at the same contact age, then stop, crouch and settle. Verify the trace actually selected a Run
landing before each input change; a rejected jump is not valid coverage. One intentional 150 ms
game-thread hitch is injected during the held-input jump in each
segment on the host and owner; `hitch_injected` marks the injection and actual frame deltas record
its effect. While the late process loads, the host and owner alternate forward/backward runs on
a replicated floor fixture. No gameplay asset is saved. Inputs enter the local Character through
`AddMovementInput`, `Jump`, `Crouch` and controller facing before actor ticks. Authority uses the
existing GAS CombatStrafe/Aim request tags and Character's normal mode replication; no AnimInstance
state is injected. This does not test
Enhanced Input bindings or a concrete predicted combat ability. Packet simulation is 60 ms lag,
10 ms variance and 10% loss in each peer by default. `-PacketLag`, `-PacketLagVariance`,
`-PacketLoss`, and `-NoHitch` isolate animation response from network conditions. Their actual
values are recorded in run.json; vary one condition at a time when investigating missing jumps.

Each role writes a CSV row for every replicated pawn and every active BlendStack player. It includes
actual frame delta, UTC ticks, phase, player ID, pawn path, network role, actor position/yaw, mesh and
root-bone yaw, OffsetRoot yaw, trajectory facing at 0 and +0.5 seconds, speed, acceleration, current
gait/rotation, correction/reset counters, MM database role/interrupt/continuing state, TIR state,
optional landing-release state, selected clip/time, elapsed pose-search time, blend clip/time,
play rate and scalar weight. Turn query/accumulated yaw and state elapsed time, jump phase,
landing elapsed time, the existing `contact_l`/`contact_r` curves and FootPlacementAlpha are also
captured. `mm_search_databases` exports the node's actual reflected search list after evaluation;
the selected database role is recorded separately. `tir_root_feedback` confirms whether this
update used evaluated root-facing rather than the legacy actor-yaw fallback. Join views by player ID and UTC time; UObject
paths are local identities. Per-bone blend-profile presence is `unavailable` because its Engine
getter is not exported; the scalar weight is reconstructed from the exported player state.
`anim_update_counter`, `bone_revision` and `engine_frame` distinguish a new animation update from
a repeated rendered bone evaluation; replay checks compare unchanged asset time and actor facing.

Optional `-CaptureScreenshots` passes `-GaspCaptureScreenshots` to each role. It requests at most
12 images per role and segment around turn and jump/landing phases, with phase, clip/time and
viewport metadata beside each image in `Screens`. The local camera follows the subject through
the existing Character camera; projected head/foot bounds are checked before requesting a shot.
This does not prove the subject is unobstructed or a pose is correct; inspect the rendered files.
Skipped projections and missing outputs are reported. Keep screenshots off for the FPS matrix
to avoid screenshot I/O affecting that measurement, and use a separate fixed-rate visual run.

Graph and pose reads occur only after `HandleExistingParallelEvaluationTask(true, true)` completes.
That synchronization is intentional diagnostic overhead: FPS is a requested cap, and the CSV's
measured frame deltas determine what actually ran. These captures are not uninstrumented performance
benchmarks. Blend scalar weights follow the Engine BlendStack recurrence; per-bone blend profiles
can differ. Root/trajectory yaw separation, clip changes and non-unit play rates require contextual
review and are not automatically defects.

The launcher's editor-startup readiness threshold is 5 FPS. With two peers already rendering,
the late editor can remain below the Engine's default 10-FPS startup threshold before PIE even
starts. This setting only lets automation start; gameplay frame caps and measured response checks
are unchanged. A slow completed capture cannot establish a sustained higher frame rate.

The automation requires the replicated subject and all three player pawns identified by their
server/owner/late ready-marker IDs, actual movement and
airborne-to-grounded transition, and selected MM clips in every process. Completion markers form a
barrier so one peer does not disconnect before another records acceptance. A successful capture
means these transport and trace prerequisites passed; the CSV and rendered poses still need analysis.
