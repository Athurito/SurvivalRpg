# Mover simulation rate and visual smoothing

The Mover Mantle pilot exposed slow remote movement when an external client ran
at roughly 200–250 render FPS and its listen host at roughly 20 FPS. Limiting
only that client to 60 FPS removed the reported symptom in the user's A/B test.

In the inspected UE 5.8.2 NetworkPrediction Independent backend, each controlling
client engine frame produces a simulation command. Authority consumes at most
six remote commands per engine frame (`TRemoteIndependentTickService`), while
the configured six-command receive window can discard older unconsumed commands
(`TIndependentTickReplicator_Server::NetRecv`). A sufficiently fast client can
therefore advance much less remote simulation time per wall-clock second on a
slow host. Increasing animation speed or interpolation buffering does not fix
that simulation-time mismatch.

## Ownership and changes

- NetworkPrediction configuration selects Fixed ticking at 50 simulation steps
  per second with fixed visual smoothing enabled. Render FPS remains independent;
  there is no production `t.MaxFPS` override. Simulated proxies remain interpolated
  with their existing 100 ms target buffer.
  UE 5.8.2 truncates the simulation timestep to integer milliseconds while
  scheduling against the fractional real timestep. An initial 60 Hz probe
  measured the resulting 4% shared time loss (16 / 16.667 ms). At 50 Hz both
  clocks use exactly 20 ms. This adds about 3.3 ms of simulation granularity
  relative to nominal 60 Hz; presentation continues at the render rate.
- The existing camera-assist interface accepts an optional presentation pivot.
  `ARpgMoverPawn` reconstructs the smoothed actor origin from Mover's primary
  visual component and its authored base transform, retaining the pawn eye
  offset. `URpgCameraMode` consumes it before existing camera damping and collision.
  Gameplay view location, collision and animated bone transforms retain their
  existing responsibilities. CMC uses its existing fallback.
- Ordinary GAS root-motion extraction uses the fixed mesh base against the
  historical simulation transform. Both authored root-motion processing delegates
  remain active. Render-only smoothing must not rotate or offset a replayed
  attack's movement. Mantle already uses its captured simulation/base transform.
- The RPG Mover pawn retains a pending jump edge until `ProduceInput` copies it
  into a simulation command. The sample cleared it after one render frame, which
  can fall entirely between two Fixed steps. The existing GAS traversal priority,
  initial jump fallback and held retry remain in the Blueprint.
- GASP AnimBP graphs, montage speed, map presentation and Experience composition
  are unchanged. The jump handoff only changes the project RPG pawn copy.

The native changes implement engine-facing prediction and presentation seams;
animation content and camera tuning remain designer-owned assets. No Engine
plugin files are modified. This changes the project's NetworkPrediction default,
including other consumers of that default; it does not change CMC's backend.

## Validation

Validation for this follow-up is recorded separately from the earlier Independent
PIE results in `Saved/GaspMoverFixedTick20260914`. The existing correction cases
now verify the actual Fixed world policy and smoothing before injecting their
prediction error; replay, active warp, collision ownership and terminal-state
checks remain in place. A separate movement-conversion test checks that changing
only the smoothed mesh transform cannot change historical GAS movement.

The Win64 Development Editor build passed. The native root-motion conversion and
three camera-damping tests passed in the freshly restarted editor. The short
Space regression also passes after the Blueprint handoff fix, without extending
its original two-render-frame key pulse. The two source/foundation Mover pawns
retain their original SHA-256 hashes.

Correction checks use the unchanged local Fixed input head, normalizing only the
replicated server-frame offset. Mantle cases additionally observe an actual Mover
rollback callback. Their one-off 50 cm owner error clears local floor/base caches
as the stock teleport effect does, so based movement cannot erase the test error
before the authority corrects it. Active warp and post-handoff cases pass with
their identity, montage, collider and terminal-state checks intact.

The consolidated result is **34/34 passing cases**, including Mover input/camera,
combat, lifecycle, 15 Mantle scenarios, native state/root-motion tests and existing
CMC, melee and retargeting regressions. The final Editor build succeeded in
12.56 seconds. All seven existing save-file hashes remain unchanged, with no new
saves. The changed Blueprint compiled with warnings treated as errors and was
loaded by fresh editor processes before the final gameplay checks.

The initial 50 Hz batch completed 34 cases with 28 passes and six failures. After
the input/fixture changes, 23 of 24 affected Mover cases passed; the final two
lifecycle cases then passed with the corrected owner-death observation. It permits
only the first actual rollback to the authority's stationary death position,
within 1 cm and 0.75 seconds, while retaining zero-motion, terminal-state and
5 cm drift checks. The native serialization/lifetime case passed in a fresh
editor before any PIE map cycles. Its earlier global-GC failure involved an
initialized LandscapeSubsystem from an earlier editor map; no runtime fix for
that editor-map cleanup is claimed. Reports retain earlier failures and warnings;
`final-validation.json` records the latest executed result and its source report
for every selected case. The earlier 60 Hz exploratory failures are retained
separately and are not counted as 50 Hz validation.

Separate-process uncooked `-game` probes used real Enhanced Input, task-only
render caps, unique profile IDs and the persistence-disabled Mover test map.
Measured frame rates, rather than requested caps, were recorded:

| Host / owner / observer render FPS | Authority / owner / observer speed | Owner rollbacks / resimulations | Final position difference |
| --- | --- | --- | --- |
| 20 / 161 / — | approximately 375 / 375 / — cm/s | 0 / 0 | 0 cm |
| 10 / 140 / 60 | 375.44 / 374.40 / 374.69 cm/s in the unobstructed return interval | 0 / 0 | authority 0 cm; observer 0.0047 cm |

All peers reported 20 ms simulation steps and `bUseFixedFrameRate=false`.
The owner's visual and camera advanced on every sampled moving render frame,
including frames on which the collision capsule did not advance. The three-peer
route also hit the stationary player capsules; its whole-route average is not
used as a steady-running measurement. Evidence is in
`probe_run_fixed50_h20_o240/analysis.json` and
`probe_run_fixed50_h10_o240_observer/clean-return-segment.json` under the validation
directory. These loopback measurements do not cover packaged builds or WAN loss.

### Original engine limitation: a severe cold-join stall

The separately versioned UE 5.8.2 correction and its September 18 measurements
are documented in [Mover network recovery](gasp-mover-network-recovery.md).
The evidence below describes the original unpatched Fixed-tick implementation.

The uncooked cold observer join spent 18.38 seconds in asset loading, texture and
skinned-asset compilation and PoseSearch derived-data work. This stalled network
dispatch and exceeded Fixed interpolation history capacity; the probe's first
successful pawn discovery/binding/sample took about 11 ms after that load.
UE 5.8.2 raised the ensure at
`NetworkPredictionService_Interpolate.inl:131`; that path does not reset the
interpolation clock. `NetworkPredictionWorldManager.cpp:380–447` advances Fixed
interpolation normally or pauses when starved, without an over-buffer recovery.
The observer subsequently retained a visible delay despite matching movement
speed and eventually converging. The probe therefore does not establish low
observer presentation latency, and this loading-stall case remains open.

Interpolated proxies are retained. Switching them to forward prediction would
require additional project GAS root-motion prediction support: the current
bridge deliberately excludes simulated proxies. No private engine clock is
mutated and no Engine plugin is patched by this change.
