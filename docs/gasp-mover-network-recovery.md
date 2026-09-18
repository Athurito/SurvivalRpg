# Mover spawn selection and interpolation recovery

The September 18 listen-server recording exposed two separate problems: an occupied
PIE camera start was reused, and a client's displayed server pawn could retain a
large delay after a loading stall. Animation play rate is unchanged by this fix.

## Spawn selection

`ARpgGameModeBase` now checks occupancy before preferring a PIE camera start and
selects a free authored player start when one exists. Native collision continues
to use Unreal's existing overlap query. For the project Mover pawn, whose capsule
is constructed in Blueprint and absent from the class default object, the query
resolves the actual inherited root capsule template, its transform, scale and
collision responses. No capsule dimensions are duplicated in gameplay code.

The Mover map already contains three starts 600 cm apart. This change does not
move them or alter pawn spawn policy, checkpoint respawn, or retry behavior.
If all starts are occupied, or a rootless Blueprint uses a different collision
shape, the existing Engine fallback still applies. This is not a general spawn
reservation or occupied-checkpoint redesign.

The Falling/None warning is Mover's guard against repeatedly returning an entire
simulation substep without progressing. Overlapping bodies are a plausible trigger
in the reported case; the warning alone does not prove the cause. `None` in this
message is not evidence of a second configured movement mode.

## Fixed interpolation recovery

The original client log contains a 10.91-second network-tick stall during loading,
followed by a NetworkPrediction interpolation history ensure. A separate-process
regression reproduced persistent presentation delay after deliberate client stalls:

| Measurement window | Unpatched presentation delay |
| --- | --- |
| Before a deliberate stall | 420 ms |
| After a 2.3-second client stall | 1,040 ms |
| After a later 5-second client stall | 535 ms |

With the correction, the displayed pawn lag measured 130 ms before a deliberate
stall, 125 ms after 160 ms and 2.3-second stalls, 130 ms after a 5-second stall,
and 130 ms after stopping and restarting movement. All five windows had median
position fitting error below 1.4 cm. The configured 100 ms receive buffer is not
zero-latency rendering: interpolation spans the preceding frame, with transport
and rendering adding a small delay. Neither process logged an interpolation
ensure or Mover time-refund warning in the patched run.

These values match the client's displayed server-pawn XY trajectory against the
authoritative trajectory using local UTC timestamps. Median fitting error was
below 1.6 cm. They are measured residual delays, not the stall lengths. Both peers
used variable-rate simulation rendering with a task-only 60 FPS cap; the project
retains its 50 Hz NetworkPrediction cadence.

The separate, pinned UE 5.8.2 correction is maintained in
[`Build/Patches/NetworkPrediction`](../Build/Patches/NetworkPrediction/README.md).
It restores the presentation clock to its configured buffer before interpolation
services access overwritten history and prevents the same hitch delta from
immediately consuming that buffer. It does not change simulation time, input,
rollback, animation speed or public Engine ABI.

The preparation script generates ignored project-local plugin copies from the
user's own verified Engine installation. NetworkPrediction contains the patch;
Mover, ChaosMover and MoverExamples are unchanged dependency copies required by
Unreal's module hierarchy. No complete Engine/sample plugin source or content is
added to version control, and the installed Engine remains untouched.

## Mover owner traversal handoff

The gameplay regression also exposed an independent normal-end ordering race.
During a late join, the server's reliable GAS end message reached the owning
client when its montage was at 0.7230 seconds, before the authored 0.7294-second
handoff. Cleanup stopped that exact montage, classified its endpoint as incomplete
and queued a Cancelled traversal transition. Instrumentation of the actual
simulation input/output confirmed that it discarded 391.47 cm/s of forward
velocity; this was not just a delayed fixture observation.

The existing early-remote-end guard now also covers the locally controlled
autonomous Mover pawn. Its own still-playing montage reaches the original source
notify before normal cleanup. True gameplay cancellation, invalid geometry,
death and the existing duration timeout retain their immediate cleanup paths.
CMC's owner path and montage timing are unchanged. Temporary diagnostic code was
removed after recording the failing transition; the shared test fixture retains
the terminal phase in its existing handoff log.

## Validation record

Evidence for this follow-up is under `Saved/GaspMoverNetFix20260918`.
The unpatched run completed with one client interpolation ensure and no Mover
time-refund warnings. It happened to allocate different authored starts; the
deterministic spawn-selection tests exercise the occupied camera-start case.

Both the Win64 Development Editor and Win64 Development Game builds passed,
including all four local plugin copies. The actual host loaded its
NetworkPrediction DLL from the project plugin path. All four focused native tests
passed: the clock regression and three spawn-selection tests. The separate-process
run also confirmed distinct, non-overlapping player spawn positions.

A missing staging marker correctly blocked a build; restoring it allowed the
final build. Installer regressions passed five cases; three symlink cases were
skipped because Windows refused to create their fixtures. All seven existing save
files remained unchanged, with no new save files.

The broader gameplay run passed 35 of 39 tests. A targeted repeat of its four
failures passed three: montage replay after correction, active Mantle correction,
and active Vault correction. The additional failing diagnostic run of
`LateJoinReconstructsTheCurrentVaultState` established the normal-end race above.
The final run after its correction passed 27 of 29 cases (with logged warnings),
including all 13 Vault cases and 14 of 15 Mantle cases. The Vault late-join owner
now applied Finished with 385.76 cm/s forward velocity instead of Cancelled with
zero velocity. The two failures are detailed below. Across all follow-up runs,
41 of 43 distinct tests have a successful latest result; that is not a single
all-green batch. Every earlier failed attempt is retained in the validation ledger.

Two separate findings remain in the final regression run:

- `FixedCorrectionPreservesActiveWarpAndCollider` timed out waiting for the
  correction observer. Normal Motion Warping removed the injected 50 cm error
  over the next two simulation frames before a network-dispatch observation
  captured a position correction. All three roles completed and landed. Ten
  rollback callbacks occurred, but the captured evidence does not establish
  that one crossed the injected active-warp frame. This remains a validation
  gap, not proof of a broken production correction or a passed contract.
- The CMC movement/late-join/equipment case reached PIE teardown and hit an
  existing `URpgGameplayAbility_Block::ClearBlockState` ensure: equipment ability
  removal tried to restore `BlockAngleDegrees` after its attribute set was
  removed. These combat/lifecycle files are unchanged from `master`. A separate
  follow-up should make this cleanup idempotent across feature/attribute removal
  order and test that lifecycle boundary.

The earlier intermittent Falling/zero-velocity state after respawn is also not
explained or proven fixed by the currently passing lifecycle tests. The PR stays
a draft pending these findings and user visual validation. No packaged/WAN claim
is made by the uncooked loopback probe.
