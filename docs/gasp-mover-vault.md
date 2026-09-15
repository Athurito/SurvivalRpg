# Grounded GASP Mover Vault

This slice extends `RpgGaspMoverExperience` with grounded Vault through its
existing contextual Space input and concrete `GA_RpgGasp_MoverMantle` asset.
UEFN remains the gameplay mesh. The serialized ability, chooser and query names
are retained; no second input binding, ability family or Experience is introduced.
Mover Hurdle and Ragdoll remain separate integration steps.

## Source and ownership

The original Mover chooser uses the same three **Neutral** Vault montages as CMC.
This differs from Mover Mantle, which uses Relaxed clips. Grounded Vault requires
front and back ledges, no detected back floor, height up to 125 cm and depth up to
59 cm. A same-floor thin barrier normally selects Hurdle; this slice preserves
that distinction rather than changing its action classification.

The three source montages are copied from the project-local shared UEFN
`Animations/Traversal/Vault` folder into
`/Game/SurvivalRpg/Characters/GASP/Mover/RPG/Traversal/Animations/Vault`.
The existing owned Mover chooser selects these copies. Its native eligibility
rows live in `AllowedVaultAnimations` on `AC_RpgGasp_MoverTraversalQuery`.
The source mapping, embedded PoseSearch membership and notify references are
checked by the task's asset audit.

The copied Vault montages retain the original notifies and sampling ranges.
Their three `PoseSearchBranchIn.Database` references are cleared: that field
registers the sample montage in its external shared database, whereas Mover
selects through its chooser's embedded databases and SamplingRange notifies.
An embedded database is a private chooser subobject and cannot become an external
montage reference. This explicit deviation keeps the sample and CMC databases
unchanged while the owned chooser points to the new copies.

| Montage filename | Source speed row (cm/s) | Source handoff (s) |
| --- | --- | --- |
| `AM_M_Neutral_Traversal_Vault_1_0_stand_F_Lfoot` | 0–100 | 0.7294442654 |
| `AM_M_Neutral_Traversal_Vault_1_0_walk_F_Rfoot` | 100–250 | 1.3141385317 |
| `AM_M_Neutral_Traversal_Vault_1_0_run_F_Lfoot` | 250+ | 1.3247057199 |

Concrete montage references, chooser composition and tuning remain assets.
The existing native GAS/query and Mover components own validation, simulation,
prediction and cancellation. No new native UObject class is needed.

## Execution and networking

The shared Vault geometry helpers now accept an Experience-composed pawn while
preserving their CMC entry point. CMC uses its character mesh base; Mover uses the
existing Motion Warping adapter and fixed visual base. Smoothed presentation
transforms never determine authoritative exit geometry.

The authority independently queries both physical obstacle faces, depth,
capsule-clear route and montage-authored exit. The client still proposes only
collider, montage and sampling time. It cannot supply trusted ledge coordinates.
Configured montage eligibility and normal GAS admission remain required.

Mover's immutable traversal request now supports an optional `BackLedge` target
in addition to `FrontLedge`. Standing Vault uses the source translation-only
rear-edge window without a front hand-height offset. Walking and running retain
their source front-target windows. Both target values participate in prediction
history and reconciliation; stock SkewWarp modifier snapshots retain each
window's identity. No new warping mathematics or engine patch is introduced.

Only the validated obstacle is temporarily ignored. Its transform and collision
must remain valid throughout the action, and the exit must remain clear. A
successful source montage handoff preserves momentum and releases traversal into
ordinary Mover Falling; Vault does not require Mantle's support on the obstacle.
Cancellation, death, obstruction and collider loss use existing recovery and
movement ownership rules. Corrected, replaced and terminal traversal states
release obsolete front/rear targets without removing unrelated warp targets.

The previously validated Fixed 50 Hz policy and camera smoothing are unchanged.
The open severe cold-join interpolation limitation is tracked in
[the Fixed tick record](gasp-mover-fixed-tick.md). This slice does not claim to
resolve it or certify packaged builds.

## Test map and acceptance

Use `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover`. The Vault lanes reuse the approved
CMC test-map arrangement: stairs to a raised approach, a thin prepared GASP
barrier and a lower floor behind it. Original project-local GASP materials and
LevelVisuals lighting/exposure remain the presentation baseline. Existing Mantle
lanes and PlayerStarts stay available. The test GameMode disables disk persistence.

Approach from standing, walking and running, press Space, and repeat at an angle
from either side. The character should align its body without forcing view yaw,
cross the back edge, leave traversal while falling and land fully beyond the
barrier. Held movement should continue through the handoff. Repeat with a listen
host, remote owner and observer; open-ground Space and existing Mantle remain
regression checks.

## Validation record

Evidence is retained under `Saved/GaspMoverVault20260915`. The Win64 Development
Editor build succeeded; the final contract build took 11.19 seconds. The asset
audit freshly reloads the new packages, compiles the query with warnings treated
as errors, compares complete montage exports and chooser references, and checks
2,282 project-local dependency packages. The original map package is unchanged.

The selected coverage comprises three native history/root-motion tests, thirteen
Mover Vault scenarios, forty-one existing regressions, and one occupied-respawn
case. `final-validation.json` records the latest executed result per test and
retains every earlier attempt. All 58 latest results passed; these results are
not one pristine all-green run and do not clear the open regression below.
The Vault cases exercise standing/walking/running, both approach angles, host
and remote ownership, a blocked exit, cancellation, death, collider loss, late
join, and real Fixed prediction corrections during and after warping. The shared
fixture preserves the fifteen existing Mantle test names and contracts.

Initial fixture failures were corrected at the observation boundary: handoff is
measured from an applied terminal NP state rather than the earlier GAS lease
release, and a very late Vault proxy must visibly advance to the same terminal
identity rather than invent an additional 100 ms after the source clip ends.
Mantle retains its original 100-ms progress check. The death observer admits at
most one proven authoritative re-anchor under the existing strict lifecycle
contract; the successful targeted Vault run needed no such exception.

The preservation audit checks all 7,634 pre-existing content/plugin files. Only
the intended query and chooser are modified; the other 7,632 files and all seven
save files remain unchanged. The three copied montages are the only new content
packages. No spawn policy, gameplay map, engine plugin or Network Prediction
configuration is changed.

## Open regression: intermittent movement after respawn

The existing `WeaponDamageCancelsAttackStopsMovementAndRespawnsForLateJoin` test
timed out three times at its movement-after-respawn gate. The new pawn had a
healthy ASC, equipment and valid forward input on authority, owner and observer,
but remained at its spawn position in Falling with zero velocity. Subsequent
unchanged runs passed; a cause and a fix have **not** been established.

The additional `OccupiedRespawnStartAllowsMovementAndCombat` case makes a real
late joiner occupy the stored start, verifies a blocking capsule overlap before
the normal respawn RPC, and then checks movement and combat on every role. It
passes with the unchanged GASP `AlwaysSpawn` setting. Consequently an occupied
start alone does not reproduce the failure, and the proposed spawn-policy change
was not applied. This test does not assert the exact first simulation frame.

Per-role input, capsule, movement and overlap diagnostics remain in the lifecycle
fixture. The PR stays **draft** while the intermittent regression is unresolved;
later passing runs are not treated as proof of a fix or merge readiness.

## Separate-process observation and open limitation

An uncooked three-process test drove the real movement and Space actions from
the saved PlayerStart through the original stairs to a standing Vault. Median
render rates were 20 FPS on the listen host, 187 on the owner and 60 on the
observer. Every role crossed the rear edge, entered Falling with forward
momentum and landed on the lower floor. The owner received two natural
corrections: approximately 0.33 cm on entry and 6.31 cm after handoff, with no
later repeated correction. Final owner/authority position difference was
0.0083 cm. All probe processes shut down gracefully.

Cold startup also reproduced the existing Fixed interpolation-history ensure
after 10.56-second owner and 14.29-second observer stalls during uncooked mesh
and PoseSearch loading. Both stalls preceded the input run. The observer's
movement handoff lagged authority by approximately 0.689 seconds; its montage
finished before buffered traversal movement began. Consequently this run proves
the functional crossing and handoff under unequal render rates, **not smooth
proxy presentation**. The known cold-load recovery problem remains open and
must be checked in the planned cooked/packaged multiplayer validation. No
interpolation clock, engine plugin or graphics FPS limit was changed to mask it.

The raw logs retain uncooked editor-Python plugin startup diagnostics as well
as the interpolation ensures. `probe_run_fixed50_vault_h20_o240/analysis.json`
and its per-role traces distinguish these from the successful scoped probe.
