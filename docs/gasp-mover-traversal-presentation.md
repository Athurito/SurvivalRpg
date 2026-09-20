# Mover traversal presentation clock

## Problem and ownership

The September 20 recording showed the listen host's Vault roll before the
obstacle on the observing client, although the host's own view was correct.
A separate-process reproduction measured a median **122.45 ms** montage lead
at the same displayed obstacle-relative positions (maximum **122.95 ms**).
The authority crossed correctly. This was a presentation timing error, not a
different traversal animation speed or incorrect authoritative warp target.

Ordinary GAS montage replication starts and corrects a remote montage when its
replication arrives. Mover's simulated-proxy movement uses the buffered
NetworkPrediction presentation state. Those two clocks differed by roughly the
movement interpolation buffer. The existing tests checked capsule traversal and
handoff but did not require the visible montage to match the displayed phase.

This correction belongs to the existing native GAS/Mover bridge. Experience
composition, query allowlists, montage selection, source notifies and tuning
remain designer-owned assets. The NetworkPrediction override, Fixed 50 Hz
configuration, imported assets and baseline CMC behavior are not changed here.

## Presentation contract

- On simulated Mover proxies, traversal montage membership comes from the
  composed query's Mantle/Vault/Hurdle allowlists. Receipt of those montages no
  longer immediately starts, seeks or stops them through ordinary GAS OnRep.
- The finalized traversal identity and montage position drive one cosmetic
  montage instance on the gameplay mesh. Position interpolates only between
  snapshots of the same active play and asset. At an inactive-to-active
  boundary, presentation adopts the destination command as stock Mover already
  does for movement mode and layered moves, advancing from the authored montage
  start phase. This also works for missing frames synthesized between packets.
  Warp caches and targets are copied as whole snapshots, never interpolated.
- The existing Mover component reapplies the last finalized phase before the
  mesh tick, including frames when interpolation has no new packet. A stopped
  movement presentation therefore cannot leave the animation freely advancing.
  When ASC avatar binding completes after finalization, it immediately consumes
  that same snapshot through the normal guards instead of waiting for another
  movement frame.
- Normal animation ticking consumes forward notify intervals. An explicit
  starting position plus the one-argument forced target avoids UE's repeated
  forced-from branching substeps. Frozen intervals retain active notify states;
  backward presentation corrections do not replay historical forward intervals.
  Automatic blend-out is disabled only on the controlled instance because a
  forced catch-up interval has a synthetic rate; source notifies and authority
  terminal state own its stop. Cleanup restores the asset's original setting on
  that instance without modifying the asset.
- Compact terminal history retains the authority's actual stopped position and
  effective blend settings, including profile/curve lifetime. The final interval
  can finish through ordinary animation ticking; an authoritative blend is a
  fallback when the conditional source notify does not stop locally. The
  source's standing/walking/running blend times are not replaced by a generic
  asset default.
- Ordinary montage replacement keeps GAS behavior and blocks buffered traversal
  resurrection. Authority's GAS play token correlates a later traversal receipt
  with its movement snapshot; full traversal identity still distinguishes
  repeated plays of the same asset. Cleanup stops only the owned instance.
- Authority and autonomous-owner playback, physical validation, prediction,
  movement extraction and gameplay reconciliation retain their existing paths.
  Cosmetic receipt/blend metadata does not decide movement reconciliation.

The inspected GASP assets use Standard blending. This slice preserves that
contract; it does not implement generic inertialization-mode replication.

## Verification

Evidence lives in `Saved/GaspMoverProxyPose20260920`. The separate-process probe
uses the saved Mover map and real Enhanced Input movement/Space actions. It
compares montage phase when each peer displays the same position, allowing
network delay. It does not compare two unrelated simultaneous wall-clock poses.
The map has disk persistence disabled.

The initial revision passed the new listen-host PIE check but failed the
separate-process probe: the two-argument forced-position overload revisited a
branching point and triggered premature automatic blend-out. A separate 350 ms
incoming-packet pause also proved free animation advancement while movement
was frozen. Both failed observations are retained alongside later runs; they
must not be represented as a pristine all-green validation batch.

A subsequent three-process run exposed a separate 40–59 ms startup gap: stock
Mover already displayed the incoming Traversing movement while the project
state waited for the last endpoint. The onset policy above corrects that gap.
The analyzer's second version also excludes authority samples whose bracket
still contains Walking, which is not a valid already-active traversal reference.
Excluded points remain in its output; the genuine startup failure remained
failed after that measurement correction.

The normal-network probe after the onset correction used three separate
processes at 60 FPS (`probe_run_onset_host_run`). Both
observers passed all 16 obstacle-relative comparisons: median phase difference
was **-19.72 ms**, with a maximum absolute difference of **20.41 ms**, versus
the original **+122.45 ms** median lead. Both crossed, fell and landed. There
were no ensures, fatal errors or Mover time-refund warnings in that run.

The final binding-build repeat (`probe_run_binding_host_run`) passed **16/16**
comparisons on one observer and all **15 numerically comparable** crossings on
the other. The remaining crossing straddles Walking without a montage and the
first Traversing sample, so the analyzer cannot interpolate a pose there and
retains an overall **failed** result. No result was suppressed or rerun to turn
that report green. Measured median phase difference was **-30.36 ms**, maximum
absolute difference **30.73 ms**, within the fixed-step and observed sampling
tolerance. Both observers crossed, fell and landed without an ensure, fatal
error or time-refund warning. The exact onset boundary is a measurement limit
of that separate-process sample; the native presentation/onset tests pass.
Raw finalization and frame samples contain the montage in the first Traversing
frame on both clients, with no active interval missing its montage.

With a deliberate **350 ms** incoming-packet pause, **17 frozen-movement frame
pairs** held the montage phase exactly. That loss-recovery run is **not fully
passing**: at one measured plane, the recovered pose differed by **-70.99 ms**,
outside the sampling tolerance. The resumed transform and montage phase follow
the same reconstructed interval, but NetworkPrediction fills the missing
movement samples along a straight segment across a curved authoritative path.
Holding animation during starvation is verified; exact obstacle/pose agreement
through this packet-loss recovery remains a limitation.

Win64 Development Editor and Game builds succeeded (`build-*-binding.log`).
The broad regression run passed **37/41** tests. It exposed the first-frame
late-join gap for Mantle and Vault, the stale-cache correction witness below,
and one Mantle terminal-reason disagreement. After the binding and witness
corrections, the targeted run passed **15/15** tests, including both late joins,
listen-host running Vault, pending/active equipment replacement and replay,
correction witnesses, lifecycle and native snapshot contracts.

`final-validation.json` retains every attempt and reports the latest result for
each of **42 distinct tests**. All latest results pass; this is not a single
clean 42-test batch. In particular, the earlier Mantle terminal correction saw
predicted `Finished` versus authoritative `Cancelled`. The unchanged assertion
passed on rerun when both sides used `Cancelled`; that does not establish a fix
for the differing-reason condition. Identity, montage and warp-history checks
passed in the original failed observation.

Two native test-harness details were corrected while retaining their contracts.
GC lifetime checks now use the editor's standard `GARBAGE_COLLECTION_KEEPFLAGS`,
with explicit checks that the transient test objects are not standalone; this
prevents collecting unrelated loaded map packages between PIE tests. The
no-forward-step correction witness uses the actual unchanged world prediction
head and fixed step. Liaison frame/time values remain diagnostics because their
per-instance cache can refresh during rollback without a new forward tick.
Dedicated cases reject real local advancement, invalid snapshots and step
changes.

This work does not close the previously documented active-warp correction
observation gap, Block cleanup ensure, intermittent post-respawn Falling state,
or cooked/WAN validation. See
[`gasp-mover-network-recovery.md`](gasp-mover-network-recovery.md).

## Manual check

Open `Lvl_RpgGaspMover` with a listen server and a client in separate processes.
Run the host toward a prepared Vault barrier and press Space; watch the host
from the client. The roll, hand contact and crossing should correspond to the
displayed obstacle position. Repeat standing, running and at an angle, then
swap which player traverses. Also check Mantle, interruption and an equipment
attack followed by another traversal. Ordinary interpolation delay can remain;
the pose and displayed movement must agree with each other.
