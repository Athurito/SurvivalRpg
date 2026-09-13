# GASP Mover mantle

## Scope and ownership

The existing `RpgGaspMoverExperience` gains the first grounded Mantle slice on
the prepared one-metre blocks. UEFN remains the gameplay mesh. Vault, Hurdle,
airborne catches, taller climbs, ragdoll and the deferred block/walking pose
work are separate steps.

The PlayerState-owned ASC and Experience/PawnData composition stay in place.
`GA_RpgGasp_MoverMantle` is a concrete Blueprint on the existing abstract
`URpgGameplayAbility_Mantle` family. That family shares server target-data
validation and collision queries between CMC and Mover; each movement system
retains its own execution and recovery mechanism. The CMC-facing entry points
remain available.

Repeated ASC initialization for the same owner, avatar and animation instance
refreshes actor information without resetting an already replicated montage.
This prevents a late-joining observer from replaying the same Mantle when pawn
composition finishes after its first montage update. Replacing the avatar or
animation instance still performs the normal initialization.

The server independently queries the obstacle and validates the selected
montage, start time, route and landing support before committing the ability.
Client correlation data does not authorize a ledge position. The Mover runtime
holds a GAS activation-specific traversal lease, ignores only the validated
obstacle component and uses the existing GASP `Traversing` flying mode.
Cancellation, death and a newer movement owner release that lease. Recovery
uses Mover simulation state rather than an actor-only teleport.

`URpgMoverMotionWarpingComponent` and its adapter are the only new native
UObject classes. They run the engine's stock SkewWarp calculation against the
simulation transform and fixed gameplay mesh offset. Per-window value
snapshots, montage position and traversal identity are reconciled with Mover's
NetworkPrediction state. No replacement warping mathematics or second montage
playback system is introduced. Blueprint callbacks and moving-target warping
are outside this first supported contract.

Prediction history retains montage assets, while obstacle references remain
weak and world-owned. Replaying an old frame cannot keep a destroyed obstacle
or a retired PIE world alive.

## Asset mapping

New assets live under
`/Game/SurvivalRpg/Characters/GASP/Mover/RPG/Traversal`:

| Asset | Source and adaptation |
| --- | --- |
| `AC_RpgGasp_MoverTraversalQuery` | Copy of the CMC-owned query-only adapter. Retains GASP traces, ledge queries and pose selection; uses Mover grounded input geometry and six allowed Mantle rows. |
| `GA_RpgGasp_MoverMantle` | Copy of the existing concrete Mantle Blueprint, retaining GAS montage playback and completion/cancellation wiring. |
| `LAS_RpgGasp_MoverTraversal` | Copy of the CMC Mantle ability set, granting the Mover ability with `InputTag.Ability.Traversal`. |
| `Choosers/CHT_RpgGasp_MoverMantleMontages` | Copy of the original project-local Mover traversal chooser, with its embedded PoseSearch databases and six montage references remapped. |
| `Animations/AM_M_Relaxed_Traversal_Mantle_1_0_{stand,walk,run}_F_{Lfoot,Rfoot}` | Six exact montage copies from `Shared/Characters/UEFN_Mannequin/Animations/Traversal/Mantle`. Shared sequences and skeleton are reused. |

The Mover chooser selects the original **Relaxed** clips, which differ from
the CMC **Neutral** clips. Their slots, root motion, two FrontLedge warping
windows, sampling ranges and blend notifies are retained. The chooser owns its
PoseSearch data internally; these six montages have sampling-range notifies,
not external BranchIn database ownership.

Grounded query direction follows velocity above 50 cm/s and otherwise Mover's
target orientation. Local forward speed 0–375 cm/s maps to a 75–300 cm query
reach; capsule radius/half-height are 30/60 cm. These are the source Mover
values. Native eligibility limits execution to the configured grounded Mantle
rows even though the preserved chooser also contains later traversal options.

| Clip | Allowed speed (cm/s) | End/handoff (montage seconds) |
| --- | --- | --- |
| Standing, left foot | 0–100 | 2.0; movement input permits 1.320531 |
| Standing, right foot | 0–100 | 2.0; movement input permits 1.324321 |
| Walking, either foot | 100–250 | 1.331763 |
| Running, either foot | 250+ | 1.068310 |

The original blend notify only stops the montage using the
`FastFeet_InstantRoot` blend profile over 0.3 seconds. It does not own collision
or Mover movement modes. Standing uses its original movement-input condition;
walking and running force the authored early blend. Playback rate remains 1.

## Composition and input

`DA_PawnData_GaspMover` grants the new ability set alongside the existing
default set and reuses the approved `CM_RpgGasp_Traversal` camera. The working
Mover pawn supplies the query component and specialized MotionWarping component.
The dormant sample traversal execution paths remain disconnected.

The existing Blueprint Space binding first calls the PawnGameplay traversal
hook. If the initial press is not consumed, the original Mover jump/uncrouch
branch runs once. Nonzero held input retries contextual traversal; release
does not activate it. No second native movement input binding is added.

## Playtest

Open `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover` without a PIE Experience
override. Its isolated GameMode disables disk persistence. The three original
GASP blocks span X=-400 to 0, centred at Y=-600, 0 and 600, with their top at
Z=100. The approved grid materials and LevelVisuals setup are retained.
The death test zone moves to (-900, -1300, 120), outside all three approach lanes.

Test Space near a block while standing, walking and running, including a
diagonal approach. The body should align to the ledge without forcing view yaw.
Hold forward through the transition and check that motion continues off the
animation. On open ground, Space should still jump; while crouched it should
first uncrouch. Repeat with a listen-server host and a remote client, observing
both directions.

## Validation

The SurvivalRpgEditor Win64 Development target built successfully with UE
5.8.2 (CL 56702186). Working evidence is kept in
`Saved/GaspMoverMantle20260913`.

The asset audit freshly loaded the authored packages, compiled the Blueprints
and compared full montage/notify exports, nested chooser data and the preserved
query trace graphs against their sources. All six montage copies matched after
path normalization; the dependency closure remained project-local or used the
existing Engine/plugin dependencies.

Runtime results are consolidated per case in `final-validation.json`, with the
source report retained for each result: **67/67 cases passed** (2 native state
tests, 15 Mover Mantle PIE cases and 50 existing regression cases), including
the focused reruns described below. Coverage includes real Space input,
standing/walking/running Mantle, diagonal body alignment, listen-host and remote
ownership, late join, server rejection, cancellation, death, collider loss,
and actual Independent prediction corrections during warping and after handoff.
Existing CMC traversal, camera, retargeting, Mover combat and respawn cases are
included in the regression selection.

The proxy angle check uses montage time from the same NetworkPrediction frame
as the observed body transform. The lifecycle proxy check also allows its
buffered position to reach the fixed authoritative death position before
measuring stationary drift, while retaining the arrival deadline and terminal
state checks. Authority and owner death checks retain their original boundary.
An existing CMC Hurdle case timed out before its Space input in the batch run
and passed an unchanged isolated rerun; viewport focus/input loss remains a
possible fixture cause.
The existing attack/death/late-join respawn case also timed out once on the
simultaneous movement/velocity check after all roles had already composed their
new healthy pawn. Its unchanged isolated rerun passed in 10.09 seconds. That
batch failure did not record the individual current positions/velocities, so
its exact cause remains unresolved; no runtime fix is claimed for it.

The authored Mover map was also opened in PIE for a visual composition check,
then returned to idle with clean packages. SHA-256 comparison against the
pre-change baseline checked 7,624 existing content/plugin files: 7,621 were
unchanged and only the intended Mover pawn, PawnData and test map differed.
All seven existing save files remained unchanged, with no new saves created.

These are editor automation checks. Packaged builds, separate-process network
play and packet-loss testing are not certified by this run. PIE reports retain
temporary-world NetGUID, unavailable voice-interface and startup buffering
warnings rather than suppressing them.
