# GASP locomotion family gap audit

This document closes the source-parity audit from issue #74. It compares the project-owned
`RpgGaspLocomotion` path with the local UE 5.8 Game Animation Sample CMC reference without making
the sample a runtime dependency.

## Evidence snapshot

- SurvivalRpg baseline: merge commit `07e7c2c08c0521f576da5d33c4d9776afa0d94f5`.
- GASP reference: `D:/Repos/GameAnimationSample`, commit
  `a9f560350f058462a4d72325bfe73a1e95ea0205`, `EpicSampleNameHash=1673045636`.
- Both projects declare Unreal Engine 5.8.
- Local content truth: Asset Registry, `CuratedAssetManifest.csv`, the 18-role presentation
  profile, and the runtime resolver/tests named below.
- Current plugin baseline: 216 assets and 165.09 MiB on disk, including 190 `UAnimSequence`
  assets, 19 Pose Search databases, 18 runtime database roles, and 170 explicit
  presentation-profile sequence entries.
- This audit changes no C++, Blueprint, DataAsset, Chooser, Pose Search database, animation,
  PawnData, Experience, GameFeature, input, config, or binary asset.

The on-disk byte figures below are audit evidence for Git LFS review, not a content contract.
Source figures include raw direct member-package sizes and explicitly labelled deduplicated
unique-family totals; they are not recursive dependency closure, LFS pointer size, or the size of
retargeted project content. Unreal may recompress retargeted sequences.

## C++ boundary decision

- **Classification:** source-parity and ownership documentation; no new runtime schema or
  mechanism.
- **Runtime truth:** CharacterMovement owns physical movement and Walk/Run gait; GAS, equipment,
  and the Lyra-derived character lifecycle own gameplay state. Animation consumes read-only
  presentation snapshots.
- **Existing seam:** `FRpgAnimInstanceProxy`, `RpgMotionMatchingRuntime`, the focused turn/jump/
  landing runtimes, `DA_RpgGaspPresentationProfile`, the curated Pose Search databases, PawnData,
  and Experiences.
- **Ownership:** authoritative and worker-thread safety mechanisms stay native; concrete database
  membership and feel stay in Pose Search assets, AnimBP, Choosers, or DataAssets.
- **New native classes:** zero. There is no technical reason for a documentation audit to add one.
- **Asset work:** none. Every remaining family is either already represented, deferred behind a
  real gameplay owner, or rejected from the migration.

## Decision summary

No new animation family is approved for import by this audit. The first-playable has a reproduced
leg/Foot Placement wobble in issue #99 and a separate combat upper-body gap in issue #75; neither
is evidence that a missing Dense/Relaxed locomotion family should be copied into the current
database search space.

- Keep the already adopted Crouch, Walk, Run, and standing turn-in-place baseline.
- Gate every later Sprint asset expansion until after the explicit server-validated Sprint
  lifecycle in issue #62. That issue intentionally uses the ten already curated Sprint sequences
  and imports no additional GASP content.
- Defer additional directional Crouch, Walk, and Run families until a reproducible
  first-playable capture identifies a bounded package. Reject the sample Spin and broad Dense Run
  shape libraries: they add search/tuning cost without a reproduced gap or combat-readability win.
- Reject Slide, Avoidance, and the sample GASP FromTraversal databases from the current migration.
  A future project-owned gameplay feature may audit a bounded subset, but must not import the
  sample state controller, Traversal, Mover, camera, Foley, or Locomotor stack.

Because no remaining package is marked **Adopt now**, this audit creates no new asset follow-up.
The accepted baseline was already delivered by issues #54, #66, #69, #71, #72, #73, and #81;
issues #99, #75, #62, and #55 own the still-open observed or gameplay-backed work.

## Current curated baseline

| Local family | Manifest sequences | Runtime database membership | Runtime role/state | On-disk sequence payload |
| --- | ---: | --- | --- | ---: |
| Crouch | 10, plus Crouch-to-Stand in `Stand.Idle` | `PSD_Rpg_Crouch`: 1 idle, Stand-to-Crouch, 8 loops; Crouch-to-Stand is in `PSD_Rpg_Stand_Idle` | `Crouch`; live from CMC crouch stance | 10.03 MiB, plus the Stand.Idle transition |
| Walk | 37 | `PSD_Rpg_Stand_Walk`: 25 moving; `PSD_Rpg_Stand_Walk_Stops`: 12 | `StandWalk`, `StandWalkStops`; live | 41.58 MiB |
| Run | 77 | 11 loops, 34 pivots, 14 starts, 12 stops; 6 legacy-only sequences are excluded from runtime databases | Four `StandRun*` roles; live | 64.71 MiB |
| Sprint | 10 | `PSD_Rpg_Stand_Sprint`: 8 moving; `PSD_Rpg_Stand_Sprint_Stops`: 2 | Two `StandSprint*` roles; dormant | 10.87 MiB |
| Standing turn in place | 8 | `PSD_Rpg_Stand_TurnInPlace`: 8 | `StandTurnInPlace`; live | 4.29 MiB |

The manifest contains 190 rows and is the exact source-to-target sequence map. The content
contract verifies the database entry counts against the Asset Registry. The presentation profile
hard-references exactly the 18 live database roles; the archival aggregate Run database is not a
runtime or normalization dependency.

## Four-state gap matrix

The four middle columns deliberately distinguish physical content from database membership,
runtime selection, and gameplay state. A `Partial` value is not treated as permission to import
the rest of a sample family.

| Family under audit | Curated asset present | Member of local database | Selectable in the current pilot | Required gameplay/movement state | Visible first-playable problem | Decision |
| --- | --- | --- | --- | --- | --- | --- |
| Crouch idle, transition, and directional loops | Yes: 11 related sequences | Yes: 10 in `PSD_Rpg_Crouch`; exit transition in Stand Idle | Yes, through the single Crouch role | Yes: CMC crouch stance and project input | No missing-family report | **Retain; already adopted** |
| Crouch starts, stops, and pivots | No; 54 Sparse members are absent | No dedicated roles or databases | No | Crouch exists, but no bounded start/stop/pivot selection contract | None reproduced; importing the missing 82.46 MiB raw Sparse set would not address a measured gap | **Defer** |
| Crouch turn in place | No; source Dense TIP has 8 members / 9.00 MiB raw | No dedicated role or database | No | No crouch-TIP request/search policy; the local TIP runtime intentionally resets while crouched | No reproduced crouch-facing gap | **Defer** |
| Walk loops, bounded starts/pivots, gait transitions, and stops | Yes: the curated 37-sequence Sparse-oriented subset | Yes: moving aggregate 25 plus Stops 12 | Yes | Yes: prediction-owned CMC Walk gait | The remaining leg wobble is tracked by #99, not proven as a membership gap | **Retain; already adopted** |
| Additional source-selected Walk starts/pivots/turns and wider gait transitions | Partial: 30 of 63 Source-Sparse members plus seven project additions are present; 33 Source-Sparse members are absent | No additional source-family databases | No additional family selection | Walk state exists; no new authoritative state is required | No reproducible missing-pose capture; the missing Source-Sparse payload is 55.00 MiB raw | **Defer** |
| Walk SpinTransition database | Partial: 1 of 8 source Reface members is local | Yes: that member is in `PSD_Rpg_Stand_Walk` | Yes as an ordinary Walk/Start candidate; no dedicated Spin selector | No local `ShouldSpinTransition` presentation predicate | No reproduced gap; a second role would duplicate source membership and complicate weapon-facing selection | **Reject dedicated SpinTransition semantics/role** |
| Sparse Run loops, pivots, starts, and stops | Yes: 77 sequences; 71 are live database members | Yes: exact 11/34/14/12 split | Yes | Yes: prediction-owned CMC Run gait, SavedMove restoration, and simulated-proxy coast reconstruction | The remaining leg wobble is tracked by #99 | **Retain; already adopted** |
| Additional source-selected Run 135 directional subset | No; all 71 Source-Sparse members are already local, while the four 135 clips belong to Dense Pivots | No additional family database | No additional selection | Run state exists; this would be presentation-only | No reproducible missing-direction capture | **Defer** |
| Dense Run Diamond, Hourglass, Prism, and full-shape libraries | No | No | No | Run state exists; the shapes are presentation choices, not replicated gameplay states | No reproduced gap; very large content/search footprint would increase tuning entropy | **Reject** |
| Run SpinTransition database | Yes: all 8 source Reface members are local | Yes: all 8 are in `PSD_Rpg_Stand_Run_Starts` | Yes as ordinary Start candidates; no dedicated Spin selector | No local `ShouldSpinTransition` presentation predicate | No reproduced gap; a second role would duplicate membership and can obscure combat facing | **Reject dedicated SpinTransition semantics/role** |
| Raw Walk/Run 45-degree and Spin clips | No | No; the audited source assets themselves also have zero referencers | No | None | No audited Asset Registry referencer or source selector selects them; importing them would invent a project-only problem and contract | **Reject** |
| Curated Sprint moving and stops | Yes: 8 moving plus 2 stops | Yes: two runtime-role databases and profile hard references | No: the selector branch is unreachable from current input/gameplay | No explicit Sprint intent/state; the movement resolver never infers Sprint and coast replication rejects it | Sprint is unavailable rather than visually incomplete | **Defer to #62** |
| Additional Sprint starts, pivots, and turns | Ten of 19 Source-Sparse members are local; nine members / 11.28 MiB raw are absent | No additional local family | No | Gated until after the explicit Sprint lifecycle | No live state exists to validate extra content | **Defer**; #62 imports no new assets |
| Slide | No | No | No | No GAS ability, CMC custom mode, prediction, or cancel contract | The action does not exist in the first playable | **Reject** from this migration |
| Avoidance animation set | No | No | No | No project gameplay or movement semantic; camera penetration avoidance is unrelated | No action or state exists; the 12 source assets have no audited source referencer | **Reject** |
| GASP FromTraversal Walk/Run/Jump databases | No | No | No | No project-owned traversal handoff state/ability | No project traversal action exists | **Reject** as a sample family; re-evaluate only inside a project-owned traversal feature |

## Source package evidence

The source comparison uses direct Pose Search database membership, not folder-name inference. The
existing adopted rows are traceable sequence-by-sequence through
[CuratedAssetManifest.csv](CuratedAssetManifest.csv).

### Sparse baseline comparison

| Source family | Exact UE 5.8 source databases: direct entries / raw member packages | Unique direct members / raw payload | Schema, normalization, database tags | Local outcome |
| --- | --- | ---: | --- | --- |
| Crouch | `PSD_Sparse_Crouch_Walk_Starts`: 14 / 19.02 MiB; `PSD_Sparse_Crouch_Walk_Stops`: 12 / 21.65 MiB; `PSD_Sparse_Crouch_Walk_Pivots`: 28 / 41.78 MiB; `PSD_Sparse_Crouch_Walk_Loops`: 8 / 8.87 MiB | 62 / 91.33 MiB | `PSS_Default`, `PSN_Sparse_All`; only Stops tagged `Stops` | Local `PSD_Rpg_Crouch` keeps the eight source loops plus project idle/entry. The 54 absent Source-Sparse members / 82.46 MiB raw are deferred. |
| Walk | `PSD_Sparse_Stand_Walk_Starts`: 14 / 22.94 MiB; `PSD_Sparse_Stand_Walk_Stops`: 12 / 20.34 MiB; `PSD_Sparse_Stand_Walk_Pivots`: 26 / 42.80 MiB; `PSD_Sparse_Stand_Walk_Loops`: 11 / 13.16 MiB | 63 / 99.24 MiB | `PSS_Default`, `PSN_Sparse_All`; no database tag | Local moving aggregate plus Stops contain 30 source members and seven project additions. The 33 absent source members / 55.00 MiB raw are deferred. |
| Run | `PSD_Sparse_Stand_Run_Starts`: 14 / 16.91 MiB; `PSD_Sparse_Stand_Run_Stops`: 12 / 18.45 MiB; `PSD_Sparse_Stand_Run_Pivots`: 34 / 38.22 MiB; `PSD_Sparse_Stand_Run_Loops`: 11 / 8.64 MiB | 71 / 82.22 MiB | `PSS_Default`, `PSN_Sparse_All`; only Pivots tagged `Pivots` | The four local runtime databases preserve the same 14/12/34/11 direct split; six extra local legacy sequences are excluded from selection. |
| Sprint | `PSD_Sparse_Stand_Sprint_Starts`: 6 / 7.30 MiB; `PSD_Sparse_Stand_Sprint_Stops`: 2 / 3.47 MiB; `PSD_Sparse_Stand_Sprint_Pivots`: 4 / 5.92 MiB; `PSD_Sparse_Stand_Sprint_Loops`: 7 / 8.79 MiB | 19 / 25.48 MiB | `PSS_Default` except Stops use `PSS_Stop`; `PSN_Sparse_All`; Loops/Pivots/Stops carry matching tags | Ten source members are curated; the nine absent members / 11.28 MiB raw remain gated until after #62. |

### Remaining source-package evidence

These are direct members of the named source databases, not a recommended import list. Family
totals below are deduplicated; Walk and Run each have eight SpinTransition entries that duplicate
Reface sequences already in their Starts database.

| Candidate family | Exact audited source database or folder | Direct entries / raw member packages | Audit outcome |
| --- | --- | ---: | --- |
| Dense Crouch direction/TIP | `PSD_Dense_Crouch_Walk_Starts` (28); `PSD_Dense_Crouch_Walk_Stops` (20); `PSD_Dense_Crouch_Walk_Pivots` (131); `PSD_Dense_Crouch_TurnInPlace` (8) | 187 entries / 187 unique / 271.88 MiB | Defer bounded directional evidence; no broad import. |
| Dense Walk direction/Spin | `PSD_Dense_Stand_Walk_Starts` (28); `PSD_Dense_Stand_Walk_Loops` (18); `PSD_Dense_Stand_Walk_Pivots` (132); `PSD_Dense_Stand_Walk_SpinTransition` (8) | 186 entries / 178 unique / 281.77 MiB | Defer bounded direction; reject the eight-member SpinTransition DB, whose members duplicate Starts. |
| Dense Run shape/Spin | `PSD_Dense_Stand_Run_Starts` (28); `PSD_Dense_Stand_Run_Loops` (20); `PSD_Dense_Stand_Run_Pivots` (136); `PSD_Dense_Stand_Run_SpinTransition` (8) | 192 entries / 184 unique / 186.24 MiB | Defer only a captured Run-135 subset; reject broad shape and duplicate SpinTransition sets. |
| Dense Sprint expansion | `PSD_Dense_Stand_Sprint_Starts` (10); `PSD_Dense_Stand_Sprint_Loops` (7); `PSD_Dense_Stand_Sprint_Pivots` (12) | 29 entries / 29 unique / 37.50 MiB | Defer until after #62; landing databases remain outside this family audit. |
| Relaxed Slide | `PSD_Relaxed_Slide_FeetOut`; `PSD_Relaxed_Slide_KneesOut`; `PSD_Relaxed_Slide_FeetOut_ExitToCrouchIdle`; `PSD_Relaxed_Slide_FeetOut_ExitToCrouchWalk`; `PSD_Relaxed_Slide_FeetOut_ExitToRun`; `PSD_Relaxed_Slide_FeetOut_ExitToSprint`; `PSD_Relaxed_Slide_FeetOut_ExitToStandIdle`; `PSD_Relaxed_Slide_FeetOut_ExitToWalk` | 20 entries / 20 unique / 38.88 MiB | Reject from the current migration; this is Relaxed/Mover content, not the source CMC path. |
| Avoidance | `/Game/Characters/UEFN_Mannequin/Animations/Avoidance/*` | 12 sequences / about 14.78 MiB; zero audited source referencers | Reject; no source or project runtime owner was found. |
| Dense FromTraversal | `PSD_Dense_Stand_Walk_FromTraversal`; `PSD_Dense_Stand_Run_FromTraversal`; `PSD_Dense_Jumps_FromTraversal` | 6 + 6 + 2 / about 16.57 MiB | Reject as a sample family; Relaxed FromTraversal was not object-dumped and no completeness claim is made for it. |

The Dense Pivot members explain why those databases are not a single bounded fix: Crouch contains
20 basic Pivot, 32 Box, 32 Diamond, 31 Hourglass, and 16 Turn entries; Walk contains 20 basic
Pivot, 32 Box, 32 Diamond, 32 Hourglass, 8 Prism, and 8 Turn entries; Run contains 20 basic Pivot,
32 Box, 32 Diamond, 32 Hourglass, 8 Prism, and 12 Turn entries. The four Run-135 members alone are
4.94 MiB raw. Sprint contains four Diamond and eight Turn members. Separately, four raw Walk-45
clips / 7.03 MiB, two raw Walk-Spin clips / 3.15 MiB, four raw Run-45 clips / 3.75 MiB, and one raw
Run-Spin clip / 0.89 MiB all have zero audited source referencers.

The read-only Asset Registry/object dump captured direct database membership, raw member-package
bytes, configured schema/normalization/tags, selected sequence notify/curve metadata, and direct
dependency identities. It did **not** calculate recursive dependency closure, dependency sizes,
actual Git LFS pointer payload, or retargeted/cooked project size. No source or project asset was
saved. A future accepted package must repeat the dump against the then-current reference commit and
set an explicit asset cap; the cap for every deferred/rejected package in this PR is **zero
imported assets and zero Git LFS bytes**.

### Metadata and direct dependency evidence

- The audited Dense Crouch/Walk/Run/Sprint databases use `PSS_Default` and `PSN_Dense_All`.
  `PSD_Dense_Jumps_FromTraversal` instead uses `PSS_Jump`. Slide main databases use
  `PSS_Relaxed_Slide`, Slide exits use `PSS_Relaxed_SlideExit`, and both use
  `PSN_Relaxed_All`.
- Relevant source notifies include `PoseSearchExcludeFromDatabase`, `PoseSearchBranchIn`,
  `PoseSearchBlockTransition`, `PoseSearchOverrideContinuingPoseCostBias`,
  `PoseSearchModifyCost`, Foley events, and `BP_NotifyState_EarlyTransition_C`. EarlyTransition
  occurs on four audited Sparse Run members, ten Dense Run members, and one Slide member.
  FromTraversal also carries Handplant, Jump, Land, and ScuffWall presentation events.
- The animation-library API returned no float-curve tracks for the audited database members.
  Binary name tables contain names such as `Phase`, `movedata_speed`, `Distance`, `Turn`, and
  `RootMotion` on some packages; those names are provenance evidence, not proof of active curve
  tracks. `PSS_Rpg_Locomotion` does not require a curve channel.
- The root source `CHT_PoseSearchDatabases` references `SandboxCharacter_CMC_ABP` and its
  Dense/Sparse/ExtremeSparse sub-Choosers, but not the Relaxed Chooser. Dense/Sparse Choosers also
  depend directly on `E_Gait`, `E_MovementMode`, `E_MovementState`, `E_Stance`, and their database
  packages. The Sparse Chooser deliberately references a few Dense databases, including Crouch
  TIP, Walk/Run SpinTransition, and FromTraversal; a `Sparse` label alone is therefore not an
  import boundary.
- `CHT_PoseSearchDatabases_Relaxed` references `SandboxCharacter_Mover_ABP`,
  `E_MovementDirection`, and the Relaxed/Slide databases. `BP_MovementMode_Slide` depends on Mover
  custom inputs and slide transitions and is referenced by sample Mover/Ragdoll characters and
  levels. Slide is consequently not a missing CMC family.
- Direct sequence dependencies include the source UEFN mannequin skeleton, sample compression,
  and often sample Foley/EarlyTransition Blueprint classes. Avoidance is the opposite case: all 12
  sequence assets have zero audited Asset Registry referencers. Neither dependency shape is
  allowed to become part of the project runtime closure accidentally.

## Decision impact and future owner

Every row below has the same impact in this audit PR: zero imported assets, zero new cook roots,
zero runtime/search-space or performance delta, and no new gameplay test surface. The future gate
states what must exist before that zero cap may change.

| Deferred or rejected package | First-playable/combat value today | Future target owner and validation gate |
| --- | --- | --- |
| Additional Crouch starts/stops/pivots | No reproduced missing-pose case; broad crouch content is lower priority than #99 and #75 | Bounded AnimBP/runtime predicate plus DataAsset/database role after a captured gap; rerun content/profile/resolver tests and multiplayer locomotion acceptance if selection changes |
| Crouch turn in place | No reproduced stance-facing gap | Explicit stance-aware TIP policy in the focused runtime plus a bounded database/profile role before content import |
| Additional source-selected Walk/Run directional transitions, including Run 135 | Possible polish only; no captured hole after the current bounded set | Existing proxy/runtime predicate plus a bounded Pose Search database/DataAsset change; first capture the failure, then audit search cost, Foot Placement, combat facing, simulated proxies, and late join |
| Walk/Run SpinTransition and broad Dense Run shapes | No proven value; spins can obscure weapon facing and the Dense sets add large search/tuning cost | No target owner in the current migration; a future proposal would need an explicit presentation predicate, bounded database, asset cap, and combat-readability acceptance |
| Raw Walk/Run 45-degree and Spin clips | No value in the audited source path because each asset has zero source referencers | No target owner; rejected unless a later project feature independently defines and validates a use case |
| Additional Sprint content | Cannot be evaluated while Sprint is unreachable | Issue #62 first owns server validation, prediction/SavedMove, simulated proxies, late join, and input; only a later bounded DataAsset/database slice may expand the existing ten assets |
| Slide | No first-playable action exists | Separate GAS ability plus authoritative/predicted CMC movement mode and Motion-Warping/cancel contract before AnimBP, Chooser, DataAsset, or database work |
| Avoidance | No defined gameplay action, movement semantic, or source runtime referencer | No target owner; a future movement feature must first define authority, prediction, collision semantics, and a worker-safe snapshot |
| FromTraversal | No project traversal action exists | Separate project-owned GAS/CMC/Motion-Warping handoff, montage/root-motion and late-join contract before any bounded presentation database is considered |

## Metadata and dependency policy

- Accepted sequences must preserve root-motion flags, sampling ranges, exclusion/transition/cost
  notifies, mirroring, schema channels, normalization, tags, and source curve spelling when those
  values participate in the selected database contract.
- Sample Foley, EarlyTransition, BranchIn synchronization, experimental state-machine user data,
  sample character, camera, Mover, Traversal, Locomotor, NetworkPrediction, and generic retarget
  content remain excluded.
- `PSS_Rpg_Locomotion` remains the default schema for the current ground families;
  `PSD_Rpg_Stand_Sprint_Stops` deliberately uses `PSS_Rpg_Stop`. A future family may not silently
  pull in Dense/Relaxed normalization or a different schema graph.
- Database Role tags are read only while the AnimInstance builds its immutable cache on the game
  thread. Worker updates consume pointer-free roles, cached pointers, and copied tuning.
- The current `GroundMoving` profile category owns the accepted Walk/Run/Sprint sequence domain.
  New package/name classification on worker threads is forbidden.
- A future accepted asset slice must report direct animation count, recursive hard/soft/management
  closure, on-disk/LFS payload, cook roots, schema/normalization changes, and excluded dependency
  cleanup before import.

## Source Blueprint/function ownership delta

[RuntimeOwnershipMap.md](RuntimeOwnershipMap.md) remains canonical for authority, threading, proxy
snapshots, Motion Matching, turn, jump, landing, ragdoll, and lifecycle ownership. This #74-only
delta records the source functions that decide the remaining family gaps; it does not create a
second runtime map.
The canonical map already covers `Update_MotionMatching`, the `CHT_PoseSearchDatabases*`
selection, `Get_MMInterruptMode`, `Update_MotionMatching_PostSelection`, `Update_States`,
`Update_Logic`, and `ShouldTurnInPlace`.

| Exact GASP CMC source function or group | Current SurvivalRpg owner | Target owner for an accepted future slice | Deliberate deviation |
| --- | --- | --- | --- |
| `GetDesiredGait`, `CalculateMaxSpeed`, `CalculateMaxAcceleration`, `CalculateBrakingDeceleration`, `CalculateBrakingFriction`, `CalculateGroundFriction`, `CanSprint`, `GetMovementInputScaleValue` | CharacterMovement, PawnData movement profile, and the current predicted Walk/Run gait contracts | Issue #62 owns any future Sprint input, server validation, prediction, and reconstruction; other gait physics remain in CharacterMovement | Animation never infers Sprint or copies sample movement authority. |
| `Get_Gait`, `Update_MovementDirection`, `Get_MovementDirectionThresholds` | `FRpgAnimInstanceProxy` snapshots plus focused `RpgMotionMatchingRuntime` value contracts and presentation-profile tuning | The same focused runtime/profile seam, extended only after a reproduced directional gap | No Actor/Component/ASC/World reads or package-name classification on workers. |
| `IsStarting`, `IsPivoting`, `Get_TrajectoryTurnAngle` | Existing pointer-free `RpgMotionMatchingRuntime` predicates over the proxy snapshot | The same focused runtime predicate plus DataAsset/database role if a bounded family is accepted | The source Start rule compares future speed against current speed plus 100 and excludes the current `Pivots` database; project tuning remains explicit rather than package-derived. |
| `ShouldSpinTransition` | No dedicated local owner; the retained Reface clips are ordinary Walk/Run Start candidates | None in the current migration | Dedicated Walk/Run SpinTransition semantics and roles are rejected; a selected pose does not invent state. |
| `Get_DynamicPlayRate` | Existing presentation profile and worker-safe runtime values | The same DataAsset/runtime tuning seam | A future family must not copy source package-name logic. |
| `OnStateEntry_SlideLoop`, `OnStateEntry_TransitionToSlide`, `SetBlendStackAnimFromChooser` | No local owner | Dedicated GAS ability plus predicted CMC/Motion-Warping state first, then bounded AnimBP/Chooser/DataAsset/database presentation | Sample Slide state/controller logic is not imported with animations. |
| `JustTraversed`, `Get_PropertiesForTraversal` | No local owner | Project-owned traversal ability, authoritative CMC/Motion-Warping handoff, then a worker-safe snapshot and bounded presentation database | Sample Traversal/Mover state is not a runtime dependency. |

[CuratedAssetManifest.csv](CuratedAssetManifest.csv) remains canonical for source-to-target
sequence identity.
`ContentContract`, `PresentationProfileValidation`, and the ground resolver remain canonical for
Asset Registry-to-manifest-to-profile-to-runtime consistency. This document owns only the family
gap and Adopt/Defer/Reject decisions.

## Authority, replication, threading, and validation contract

- Crouch, Walk, and Run continue to use CharacterMovement truth. Simulated proxies and late joiners
  reconstruct the inputs already covered by the movement/network contracts; animation state is not
  replicated independently.
- Sprint remains unavailable until issue #62 supplies predicted/server-validated state and a
  simulated-proxy/late-join contract.
- Slide or traversal would require server validation, prediction/correction, cancellation, montage
  and root-motion handoff, and late-join behavior before animation presentation is added.
- Avoidance cannot become an animation-only collision or authority mechanism.
- Actor, movement, ASC, component, and world reads stay on the game thread. Worker code consumes
  immutable snapshots and prebuilt caches only.
- The prototype and GASP PawnData/Experience paths remain independently selectable. This audit
  does not change either Experience, PawnData, AnimBP, or the default selection.

## Verification and follow-up order

The stable contracts for this snapshot are:

- `SurvivalRpg.Animation.Gasp.ContentContract`
- `SurvivalRpg.Animation.Gasp.PilotAssetContract`
- `SurvivalRpg.Animation.Gasp.PresentationProfileValidation`
- `SurvivalRpg.Animation.MotionMatching.GroundDatabaseResolver`
- `SurvivalRpg.Character.Movement.ProfileAndGait`
- `SurvivalRpg.Character.Movement.SavedMovePrediction`
- `SurvivalRpg.Character.Movement.GroundCoastReplicationContract`

For this documentation snapshot, all 18 tests under `SurvivalRpg.Animation` and all three under
`SurvivalRpg.Character.Movement` passed. The existing real-network acceptance contract
`SurvivalRpg.Network.GaspPilotPIE.ReplicationLateJoinCorrectionAndDefaultSlotMontage` remains the
listen-server/two-client/late-join gate; it was not rerun because this audit changes no runtime or
asset.

Issue order after this audit:

1. Continue with #75 as the next active slice for the combat/weapon upper-body presentation
   contract.
2. Keep #99 open as the observed Walk/Run leg and Foot Placement defect; it is not a database-
   membership follow-up and must not be hidden by widening the search databases.
3. #62 may add the explicit Sprint gameplay lifecycle independently using existing content only.
4. #55 remains the real multiplayer/GAS/cutover gate after those presentation and gameplay seams
   are accepted.

Any future **Adopt now** follow-up must name an asset cap and source provenance, authoritative
runtime owner, C++/Blueprint/DataAsset/Chooser boundary, rollback path, cook/LFS audit, focused
automation, and a real listen-server/two-client/late-join acceptance when runtime selection changes.
