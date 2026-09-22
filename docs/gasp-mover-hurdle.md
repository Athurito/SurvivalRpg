# GASP-01: Grounded Mover Hurdle

Review: [PR #144](https://github.com/Athurito/SurvivalRpg/pull/144), merged on
22 September 2026 after the user's visual acceptance and explicit authorization.
Confirmed merge commit: `aa4447d69d3187dec3592913a1f683b5c91c0a7b`.

This slice extends the existing `RpgGaspMoverExperience`, contextual Space input,
query and `GA_RpgGasp_MoverMantle`. Hurdle crosses a thin obstacle with validated
ground behind it and returns to ordinary Walking. Vault still requires the
absence of that back floor; Mantle keeps its existing destination contract.

## Source and ownership

The original Mover chooser uses ten grounded **Relaxed** Hurdle montages; the CMC chooser
uses Neutral variants with different sampling and handoff times. Only the ten
grounded Relaxed montages are copied to
`/Game/SurvivalRpg/Characters/GASP/Mover/RPG/Traversal/Animations/Hurdle`.
The owned chooser replaces their result, cooked-result and embedded PoseSearch
references (30 replacements). Shared animation sequences, curves, notifies,
skeleton and source selection logic remain intact. Unlike the previous Neutral
Vault copies, these source clips have no BranchIn database references to remove.

Every filename starts with `AM_M_Relaxed_Traversal_Hurdle_1_0_`:

| Suffix | Depth cm | Speed cm/s | Sampling end s | Source handoff s |
| --- | --- | --- | --- | --- |
| stand_F_V2_Lfoot | 0–25 | 0–100 | 0.033333 | 1.017674 with input; 1.5 natural end |
| walk_F_Lfoot | 0–25 | 100–250 | 0.830306 | 1.936244 |
| walk_F_Rfoot | 0–25 | 100–250 | 0.973484 | 2.168770 |
| run_F_Lfoot | 0–25 | 250+ | 0.713020 | 1.383317 |
| run_F_Rfoot | 0–25 | 250+ | 0.657152 | 1.437855 |
| stand_F_V2_Rfoot | 25–60 | 0–100 | 0.033333 | 1.017674 with input; 1.5 natural end |
| walk_F_V2_Lfoot | 25–60 | 100–250 | 0.692728 | 1.735257 |
| walk_F_V2_Rfoot | 25–60 | 100–250 | 0.717240 | 1.671265 |
| run_F_V2_Lfoot | 25–60 | 25+ | 0.502439 | 1.215341 |
| run_F_V2_Rfoot | 25–60 | 25+ | 0.538969 | 1.239338 |

All sampling intervals start at zero. The outer chooser limits physical depth
to 59 cm and the back ledge to at least 50 cm above its floor; the Hurdle
subchooser caps obstacle height at 125 cm. Its three airborne Catch rows remain
outside this grounded integration.
The wider running rows retain the original 25 cm/s threshold and overlapping
selection. Notify trigger times are read from the live montage; this table is
rounded to six decimal places.

GAS and Mover retain activation, independent authority queries, prediction,
collision ownership and cleanup. The existing native Hurdle geometry functions
now accept the composed pawn while keeping CMC entry points. Mover uses its
fixed visual base and Motion Warping adapter, never the smoothed visible mesh,
for gameplay geometry. The existing Blueprint query owns eligibility data;
Chooser and montage assets own concrete selection and presentation. No new
native runtime class or ability family is introduced.

## Simulation and cleanup

The immutable Mover traversal request now carries an optional `BackFloor` target
and a weak reference to its accepted landing support and transform. They join
serialization, reconciliation and historical frame replay. Stock SkewWarp
window snapshots retain authored identity and overlapping windows. The existing
`Distance_From_Ledge` curve calculation supplies the BackFloor offset.

The authority validates both the natural and conditional source endpoints and
the route between them. Clearance capsules sit on the measured support plane;
a small vertical root-motion residual cannot push the check capsule into the
floor. Live validation and replay reject changed, removed or nonblocking
support. History does not keep world-owned support alive.

Successful completion uses the existing Mover end path and a fresh ordinary
floor acquisition. Cancellation, death, correction and replacement release only
the owned FrontLedge/BackLedge/BackFloor targets and obstacle collision lease.
The source standing tail may have zero velocity at the input handoff; ordinary
Walking then accelerates from the held movement input.

Natural standing completion uses the exact montage instance's noninterrupted
engine end callback. UE can finish the blend slightly before the authored last
frame (observed 1.498 versus 1.500 seconds), so position alone incorrectly marked
a successful landing `Cancelled`. The existing GAS task delegate is forwarded
once after recording the reason. Final warp, capsule clearance and fixed support
remain mandatory; conditional stops retain their time checks. A remote normal-end
RPC cannot preempt this local natural-completion callback. This narrow Hurdle fix
does not resolve the separate historical `GASP-NET-02` finding.

Late join exposed a separate binding gap: ASC avatar initialization could start
the montage from the prior finalized snapshot while PawnExtension still returned
no ASC. The next finalized snapshot and pre-mesh refresh therefore missed that
already-bound ASC; a later binding callback queued the current animation range
after the mesh had ticked. The first displayed frame lagged by 43.6–48.6 ms.
Mover retains the explicitly bound presentation ASC weakly during this interval
and validates its avatar on every use. PawnExtension remains the normal lookup;
the existing animation tick consumes the unchanged notify range. This binding
fix adds no direct pose seek and does not relax the phase threshold.

## Map and validation

Use `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover`. Its existing Hurdle lanes at
X=1500 and Y=-1200/-400/400 are 100 cm high and 20/40/59 cm deep. The saved map,
approved GASP blocks/materials, LevelVisuals and persistence-disabled test
GameMode are reused unchanged.

Validation uses implementation commit
`b78edf5b` (Hurdle foundation: `c8961b53`). Local evidence lives in
`Saved/GaspMoverHurdle20260922`; these ignored files are not supplied by Git.
UE 5.8.2 Win64 Development Editor and Game builds both succeeded with the final
binding fix (`build-editor-03.log`, `build-game-03.log`).

- All 15 `SurvivalRpg.GASP.Mover.Hurdle.*` PIE cases and four
  `SurvivalRpg.Network.Mover.Traversal.*` native contracts pass on this commit,
  in two batches of 2 and 17 (`critical-final-04-results.json`,
  `other-hurdle-final-04-results.json`). They include actual Fixed rollbacks
  during BackFloor warping and after handoff, authority rejection, cancellation,
  death, support loss, conditional input, host/owner/observer phase and late join.
- The final late-join case has 27 measured observer phase samples with maximum
  clock difference rounded to 0.0000 seconds; natural standing has 55 samples.
  All successful Hurdles require an applied `Finished` state with the exact
  authority identity as well as a fully supported capsule beyond the rear edge.
- All 28 affected regression tests pass (`regression-final-04-results.json`):
  eleven CMC Hurdle cases, five Mover Mantle, six Mover Vault, two equipment/
  optional-retargeting cases, CMC Mantle/Vault handoffs and two native smoothing/
  interpolation contracts. That is **47 distinct passing tests in three batches**
  of 2, 17 and 28 on the final implementation, not one combined 47-test run.
- Both separate-process probes below pass. The user accepted the visual review
  on 22 September 2026. Packaged/WAN validation has not been performed.

Three uncooked `UnrealEditor -game` processes use ordinary loopback transport
and the existing Enhanced Input actions. Target frame limits are 60/120/60;
actual median frame rates are reported separately.

| Probe / driver | Compared planes per remote client | Maximum absolute phase difference, client 1 / client 2 | Median FPS, host / client 1 / client 2 |
| --- | --- | --- | --- |
| `owner-stand-01`, client 1, 20 cm lane | 8/8 eligible planes each | 44.45 / 20.32 ms | 60 / 86.9 / 60 |
| `host-run-01`, host, 40 cm lane | 15/15 each; one onset bracket excluded | 34.41 / 34.34 ms | 60 / 56.0 / 60 |

All three roles cross while Traversing and return to grounded Walking with the
full capsule beyond the rear edge; held running input continues after handoff.
All six process instances exit gracefully with code 0. No ensure, fatal or
time-refund signatures occur in their logs. Reports and raw samples live under
`probe_run_<label>/analysis.json` and the adjacent per-role files.

These comparisons measure visible montage phase at the same spatial planes,
with tolerance of one 20 ms Fixed step plus both measured sampling brackets.
They do not prove zero phase error or bone/hand/foot contact across the entire
clip. The initial standing plane X=1470 falls below the existing 0.03-second
phase gate; all eight qualifying planes are compared, with no further exclusions.
For host-run, X=1350 is explicitly excluded because its authority sampling
bracket crosses Walking to Traversing; all 15 established traversal planes are
retained. Floor evidence here is grounded state and capsule feet relative to the
known common floor; the native PIE cases separately verify support identity.
There is no injected packet loss in these probes. Natural rollback counts are
not used as a substitute for the two explicit Fixed-correction tests.

Earlier failed results are preserved (`smoke-01`, `hurdle-01`, `hurdle-final-02`
and `latejoin-diagnostic-03` result files). They led to the natural-end and binding
fixes above and three fixture corrections: Relaxed Hurdle uses queued notifies,
the standing input handoff can have zero source velocity, and exact instance
time must be sampled in the GAS end callback after active montage lookup ends.
Source timing thresholds, real correction witnesses and observer phase limits
were retained; successful Hurdles gained the explicit `Finished` assertion.

The final test reports contain 391 warning entries (voice interface, transient
PIE NetGUIDs, shared Manny PoseAsset age, console lookups, rollback boundaries
and deliberately invalid retarget-profile fallback). NetworkPrediction recovery
diagnostics remain visible. There are no Error/Fatal lines or ensure/assert/time-
refund signatures during the requested test interval. Two `LogAutomationTest:
Condition failed` startup errors precede engine initialization and the tests;
their cause is unproven and the full editor session is not claimed error-free.
`final-validation-summary.json` records the exact scopes, warnings and witnesses.

The final preservation comparison rehashes all 7,637 preexisting tracked content
and plugin files. Only the owned query and chooser differ; exactly ten Hurdle
assets are added. All ten tracked maps and all seven save files remain byte
identical, including a repeat after all PIE and separate-process runs
(`preservation-after-tests.json`).

The current local imported originals still match the foundation snapshot hashes.
Fresh mounted-source exports are compared against that approved mapping and the
new copies: all ten montages and the original chooser match the approved source
exports, and all ten owned copies retain the original full reflected contract
after path remapping. The owned chooser has exactly the 30 expected reference
replacements; prior Mantle/Vault eligibility is unchanged.

The first raw query export comparison failed because compilation removed 431
transient/compiler objects and regenerated eight hidden, unlinked empty pin IDs.
That failed report is retained. A separate membership-aware comparison verifies
all 281 authored objects (root, three graphs and 277 nodes), their properties,
defaults and links; each changed pin ID occurs once and has no references.
All active link endpoints resolve. This proves authored graph/property/link
parity, not VM bytecode identity. It explains the specific export differences
rather than excluding Blueprint graph changes. The fresh hard
and soft dependency closure contains 2,292 packages and no `/Game` reference
outside `/Game/SurvivalRpg`.

The external `D:/Repos/GameAnimationSample` package bytes differ;
that checkout is neither a runtime dependency nor assumed to have identical
current semantics without a new external export.

The stock UE 5.8 MCP toolsets lack complete reflected exports, reference remaps
through owned chooser subobjects, precise montage event inspection and clean
package reloads. Optional editor-only adapters in
`Build/Tools/Unreal/asset_contract_tools.py` expose these existing engine/project
APIs. Standard MCP asset, object and Blueprint tools perform duplication,
configuration, compilation and saving. The adapter has no runtime content and
registers only when explicitly loaded; project startup does not enable it.

Existing unrelated limitations remain in the [roadmap](gasp-integration-roadmap.md)
under GASP-02. This slice does not close historical lifecycle or network findings
merely because another run succeeds.

After editor validation in `Lvl_RpgGaspMover`, the user accepted the visible result
and authorized merging on 22 September 2026: “schaut gut aus kann gemerged werden”.
The exact manual role, gait and obstacle coverage was not individually recorded;
the automated host/owner/observer evidence above remains a separate validation.
Only documentation changed after tested runtime commit `b78edf5b`; the builds and
automated tests were not rerun for this acceptance update or the subsequent
documentation-only merge-status update. PR #144 is confirmed merged.
