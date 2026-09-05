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
python 'D:\Repos\SurvivalRpg\Scripts\Tests\Summarize-GaspMultiProcess.py' 'D:\Repos\SurvivalRpg\Saved\Reviews\GaspMultiProcess\<capture-directory>'
```

Use the **same requested FPS, map, assets, network settings and input schedule** for a before/after
comparison. The script never rebuilds or swaps binaries. Capture the baseline before changing its
runtime or build the diagnostic source against that baseline in an isolated checkout. Missing new
reflected diagnostic fields are written as `unavailable`; no new runtime hook is required. Record
the runtime commit and asset revision with each capture. Existing video alone cannot provide the
new CSV fields retrospectively.

The launcher uses a unique directory per run and never deletes old evidence. It terminates only
the process handles it launched, including after failure. Each process gets a separate UserDir,
shader work directory, editor log and automation report. Standard rendered RHI is used with a
640x480 PIE viewport; optional `-Offscreen` renders without an interactive viewport. Do not substitute
NullRHI for rendered presentation acceptance.

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
and Aim turns of 45/90/180 degrees, then repeated and opposite requests 1.8 seconds after each
followup phase begins. These inputs are fixed in schedule time and do not depend on clip playback;
`tail_followup_triggered` records the second request and `selected_asset_fraction` records the
actual clip position. The phase name alone does not prove a request occurred in a clip's tail.
This is followed by Run-to-Jump with forward input held through landing, another jump
with input released in flight, and another Run-to-Jump with Walk input in flight, stop, crouch and
settle. One intentional 150 ms game-thread hitch is injected during the held-input jump in each
segment on the host and owner; `hitch_injected` marks the injection and actual frame deltas record
its effect. While the late process loads, the host and owner alternate forward/backward runs on
a replicated floor fixture. No gameplay asset is saved. Inputs enter the local Character through
`AddMovementInput`, `Jump`, `Crouch` and controller facing before actor ticks. Authority uses the
existing GAS CombatStrafe/Aim request tags and Character's normal mode replication; no AnimInstance
state is injected. This does not test
Enhanced Input bindings or a concrete predicted combat ability. Packet simulation is 60 ms lag,
10 ms variance and 10% loss in each peer.

Each role writes a CSV row for every replicated pawn and every active BlendStack player. It includes
actual frame delta, UTC ticks, phase, player ID, pawn path, network role, actor position/yaw, mesh and
root-bone yaw, OffsetRoot yaw, trajectory facing at 0 and +0.5 seconds, speed, acceleration, current
gait/rotation, correction/reset counters, MM database role/interrupt/continuing state, TIR state,
optional landing-release state, selected clip/time, elapsed pose-search time, blend clip/time,
play rate and scalar weight. Turn query/accumulated yaw and state elapsed time, jump phase,
landing elapsed time, the existing `contact_l`/`contact_r` curves and FootPlacementAlpha are also
captured. `mm_search_databases` exports the node's actual reflected search list after evaluation;
the selected database role is recorded separately. Join views by player ID and UTC time; UObject
paths are local identities. Per-bone blend-profile presence is `unavailable` because its Engine
getter is not exported; the scalar weight is reconstructed from the exported player state.

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

The automation requires the replicated subject and all three player pawns identified by their
server/owner/late ready-marker IDs, actual movement and
airborne-to-grounded transition, and selected MM clips in every process. Completion markers form a
barrier so one peer does not disconnect before another records acceptance. A successful capture
means these transport and trace prerequisites passed; the CSV and rendered poses still need analysis.
