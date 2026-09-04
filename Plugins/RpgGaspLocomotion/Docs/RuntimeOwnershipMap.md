# GASP runtime ownership map

This map records how the relevant UE 5.8 GASP CMC Blueprint responsibilities are adapted by
SurvivalRpg. It is the behavior-preserving architecture baseline for issue #81; it does not make
the sample project a runtime dependency.

## Ownership rules

- CharacterMovement, GAS, equipment, and the Lyra-derived character lifecycle own gameplay truth.
- `URpgAnimInstance::InitializeWithAbilitySystem` owns the game-thread ASC/tag-map lifecycle.
  `FRpgAnimInstanceProxy::PreUpdate` collects actor, movement-component, and world data and copies
  the already mirrored gameplay-tag booleans into the immutable proxy snapshot.
- Worker-thread animation consumes only immutable values, already loaded animation assets, the
  game-thread-built presentation/database caches, and the copied locomotion-tuning snapshot. It
  never reads the profile arrays or Pose Search database tags.
- `DA_RpgGaspPresentationProfile` owns the concrete sequence membership, the exact 18-database
  hard-reference set, and locomotion feel. AnimBP, Choosers, and Pose Search assets retain graph
  composition, blending, warping, IK feel, and authored content membership.
- Focused C++ helpers are retained only for engine callbacks, game-thread world queries, immutable
  snapshots, custom AnimNodes, and small deterministic resolvers.

## Source-to-project mapping

| GASP CMC source responsibility | Current SurvivalRpg owner | Target owner | Intentional adaptation |
| --- | --- | --- | --- |
| `Update_PropertiesFromCharacter`, `Update_EssentialValues` | `InitializeWithAbilitySystem` for the ASC/tag map; `FRpgAnimInstanceProxy::PreUpdate` for the value snapshot | Existing ASC lifecycle plus proxy snapshot boundary | ASC delegates are initialized on the game thread. `PreUpdate` copies mirrored tag booleans and reads CharacterMovement/world state; no sample character hierarchy is imported. |
| Character acceleration replication and analog trajectory intent | `ARpgCharacter::ShouldReplicateAcceleration`, UE 5.8 `FRepMovement`, and base `UCharacterMovementComponent::UpdateProxyAcceleration` | The same character-scoped engine path | The project opts only RPG characters into UE 5.8's native acceleration payload. Simulated proxies reconstruct both acceleration and `AnalogInputModifier`; there is no parallel custom acceleration property and no project-wide CVar override. |
| Walk/Run prediction and active/coasting proxy reconstruction | `URpgCharacterMovementComponent` `DesiredGait`/`GroundGait`, SavedMove `Custom0`, and `ARpgCharacter::GroundMovementGait` | CharacterMovement truth plus a narrow character replication bridge | Authority/autonomous prediction remains SavedMove-owned. A scoped solve preserves each recorded gait through base-space acceleration quantization; server missing-move extrapolation preserves the last validated gait. Simulated proxies use replicated acceleration for intent and current simulated-only Walk/Run state for both active input and coast. Idle clears at physical stop or when leaving standing ground movement; no pose, Pose History, Motion Matching, or AnimBP state replicates. |
| Controller-facing-to-Free rotation handoff | `ARpgCharacter` rotation policy/revision and reliable owner acknowledgements; `URpgCharacterMovementComponent` timestamped correction and replay | Existing Character authority/lifecycle plus CMC prediction | The owner acknowledges its last pre-Free move. Authority requests a yaw-bearing correction for a strictly newer move and retries until receipt is confirmed. Revision checks reject superseded handoffs; CMC restores capsule yaw before replay. AnimBP steering and turn presentation never author this synchronization. |
| Network correction and semantic-teleport presentation resets | `URpgCharacterMovementComponent` local discontinuity serial, `ARpgCharacter` teleport epoch, and proxy `PreUpdate` | Existing Character/CharacterMovement authority plus one local presentation edge | Autonomous corrections compare the acknowledged move and server correction in the same resolved movement-base/bone frame, including the live pre-/post-replay check. Base/bone changes, absolute corrections, or unusable base data deliberately fall back to a world-space hard-reset classification; UE rejects unresolved relative-base RPCs before the project callback. Ordinary simulated-proxy smoothing never resets history; only semantic teleports or corrections beyond UE's no-smoothing range do. The AnimInstance no longer treats transient `bJustTeleported` as a durable network event. |
| `Update_Trajectory` | Proxy `PreUpdate` orchestrates `PoseSearchGenerateTransformTrajectory`; `RpgPoseSearchTrajectory` owns its sampling constants and validation/correction helpers | Same split: proxy owns generation and snapshot lifetime; focused native helper owns the sampling/correction mechanism | Controller-yaw extrapolation is disabled for the controller-facing project character; raw history stays separate from worker-facing corrected output. |
| `HandleTransformTrajectoryWorldCollisions` | `RpgPoseSearchTrajectory::ResolveWorldCollision` | Focused game-thread trajectory helper | Uses bounded sphere sweeps, CharacterMovement walkability, explicit validity, floor projection, and a pointer-free landing prediction. It never changes movement or touchdown authority. |
| `Update_MotionMatching` | `URpgAnimInstance::UpdateGaspMotionMatching` bridges the AnimNode callback and the immutable database-role cache; `RpgMotionMatchingRuntime` owns pointer-free role selection; `DA_RpgGaspPresentationProfile` owns the exact 18-database hard-reference set | Same callback/runtime/profile split | Profile Role tags are read once while the AnimInstance builds the bidirectional cache on the game thread; whole-legacy mode derives the same cache from reflected slot roles. Worker callbacks use only role values and cached pointers. The project excludes GASP BranchIn, experimental state-machine, Foley, and broad Chooser dependencies. |
| `IsMoving`, `IsStarting`, `IsPivoting`, `Get_TrajectoryTurnAngle` | `RpgMotionMatchingRuntime::IsChooserMoving`, `GetRunPivotMinimumAngle`, and `ResolveDatabaseRoles`; the resolver consumes the GASP-authored future-velocity window, project gait/stance snapshot, and copied profile tuning | Focused value-only Motion Matching selection runtime plus validated designer-owned profile membership and tuning | Project gameplay gait and rotation mode constrain the authored source predicates. The trajectory turn angle is the acceleration-versus-velocity pivot angle, not a turn-in-place rule. Selection remains cosmetic and pointer-free. |
| `Get_MMInterruptMode` | `RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching` plus explicit turn/landing request and playback latches in `URpgAnimInstance::UpdateGaspMotionMatching` | Focused value-only Motion Matching runtime coordinated by the native AnimNode callback | Interrupts also protect physical movement-domain changes and one-shot project turn/landing requests; they never become replicated gameplay state. |
| `Update_MotionMatching_PostSelection` | `URpgAnimInstance::UpdateGaspMotionMatchingPostSelection` bridges selected database metadata; `RpgMotionMatchingRuntime::ResolvePostSelection` owns the pointer-free result and exclusive latch policy | Same callback/value-runtime split | Completed-search metadata is cosmetic only. Unlike source GASP, the project does not apply `Override Motion Matching Blend Settings`; the authored node retains uniform `0.2 s` blending after the FastFeet turn regression. |
| `Update_States`, `Update_Logic` | Proxy movement snapshot plus `RpgTurnInPlaceRuntime`, `RpgJumpRuntime`, and `RpgLandingRuntime`; `URpgAnimInstance` coordinates their reflected and UObject bridges | Gameplay state in CharacterMovement/GAS; focused presentation runtimes and AnimBP/profile tuning | The sample's broad and experimental state controller is not copied. Physical airborne/grounded state always comes from CharacterMovement. |
| `ShouldTurnInPlace` | `RpgTurnInPlaceRuntime` owns pointer-free eligibility, hysteresis, reset edges, request/search policy, watchdog decisions, and synthetic facing; `URpgAnimInstance` retains the reflected facade plus GC-safe asset/Blend Stack bridge; the profile owns feel values | Same native mechanism plus designer-owned profile tuning | Turns remain controller-facing and cosmetic; they never rotate the authoritative actor. GASP's inclusive 50-degree OrientationIntent/root predicate and Chooser 20 cm/s cap are intentionally adapted to project actor-yaw accumulation; the profile defaults preserve the 20/30/10-degree hysteresis and stricter 3 cm/s stationary gate. |
| Jump movement-mode edges and bounded backward Jump Start/Fall continuation | `RpgJumpRuntime` owns pointer-free physical edge interpretation and Continuing-Pose policy; `URpgAnimInstance` retains the reflected phase plus GC-tracked held asset and trait bridge | Same value-runtime/facade split | An upward velocity change while already airborne is not a new phase. The bounded backward hold is a project adaptation for curated non-looping starts and never changes movement authority. |
| `JustLanded_Light`, `JustLanded_Heavy`, `Get_LandVelocity`, `PlayLand`, `PlayMovingLand` | `RpgLandingRuntime` owns pointer-free final-airborne capture, role/fallback, request serials, handoff, and timeout policy; `URpgAnimInstance` retains the reflected facade, exact asset latch, and Blend Stack observation; the profile owns landing database membership and feel | Same native mechanism plus designer-owned profile configuration | Landing begins only on the physical CMC touchdown edge. Prediction may strengthen severity but never starts landing. Idle/Walk/Run Light/Heavy are curated; Sprint landing remains deferred until authoritative Sprint issue #62. |
| `AllowFootPinning`, foot-placement settings helpers | Proxy game-thread traces, `RpgFootPlacement` value helpers, `FAnimNode_RpgFootPlacement` | Existing focused native types/node plus AnimBP tuning | Project-local snapshots make the node worker-safe and preserve moving-base handling; crouch stays opted out by default. |
| `Get_DesiredFacing`, `EnableSteering`, Orientation Warping gates | `URpgAnimInstance::GetGaspBlendStackInputs`, authored curves, and the immutable `FRpgGaspPresentationAssetLookup` | Existing AnimBP/presentation profile seam, with only value inputs supplied natively | `DA_RpgGaspPresentationProfile` explicitly preserves the old 170-sequence presentation domain without package/name reads on workers. Local fall, idle/crouch/TIP, and moving-landing adaptations remain unchanged. |
| Sparse database and state selection | Stable native role enum plus the profile's unordered 18-database hard-reference set and immutable bidirectional cache | Same validated profile/cache seam | Each database has exactly one known `Rpg.MotionMatching.Role.*` identity tag. State tags may remain additive source/provenance metadata, but the profile validator and runtime cache do not require or consult them. Legacy flat bindings remain only as a whole-mode compatibility path. |
| Slide and other optional locomotion families | Not adopted; no runtime owner | No #81 owner; Adopt/Defer/Reject decision belongs to audit issue #74 | #81 does not pre-approve content families or grow the AnimInstance role architecture. |
| `Debug_ExperimentalStateMachine`, full GASP Mover/Traversal stack, sample camera, Foley | Not adopted; no runtime owner | No runtime owner without a dedicated isolated feature evaluation | These sample subsystems are outside the CMC pilot contract and are never incidental dependencies. Curated traversal assets may only enter later through project-owned CMC/GAS/Motion-Warping seams. |
| Sample character/mode composition | Existing `BP_Rpg_Character_GASP`, `DA_PawnData_GASP`, and pilot Experience | Existing Lyra-derived pilot composition remains unchanged | Only the GASP sample character hierarchy is not adopted. #81 does not change PawnData, Experience selection, or the default pawn cutover. |

## Network transform-space contract

| Data or consumer | Authoritative or presentation frame | Contract |
| --- | --- | --- |
| Velocity, acceleration, `AnalogInputModifier`, MovementMode, floor/base state, and correction distance | CharacterMovement and the collision capsule | These values remain gameplay/network truth. Owner corrections compare the server position with the saved client move at the same acknowledged timestamp and, for the same resolved dynamic base and bone, in base-relative space. The documented fallback is world space when those frames cannot be shared; smoothed mesh state never feeds movement authority. |
| Free-mode rotation correction | Authority capsule yaw at the corrected CMC move timestamp | The owner receives yaw through the normal CMC response and restores it before replaying newer moves. The handoff acknowledges a temporal boundary, not a client-authored yaw; no OnRep-only rotation snap or cosmetic turn result becomes authority. |
| Local owner, standalone, and ordinary authority presentation | Actor/capsule transform | Their presentation snapshot uses the actor transform because it is not a network-smoothed remote view. |
| SimulatedProxy and listen-server remote AutonomousProxy presentation | Skeletal-mesh component transform with the authored base translation and rotation offsets removed | One reconstructed presentation frame supplies snapshot location/yaw, local velocity/acceleration, Aim and locomotion angles, Pose Search trajectory history, and turn-in-place facing. This prevents capsule correction and mesh smoothing from being applied as two independent visual turns. |
| Foot Placement | Smoothed mesh component plus game-thread floor/base traces | Foot locks and trace snapshots stay in the same visual frame as the rendered mesh. A shared semantic-discontinuity pulse resets their history; ordinary network smoothing does not. |

## Slice 1 boundary and verification contract

- **Authority and lifecycle:** CharacterMovement remains authoritative. On each eligible game-thread
  `PreUpdate`, the proxy generates the raw trajectory and, when enabled, invokes the bounded
  world-collision helper. It then publishes only corrected trajectory and pointer-free prediction
  values to animation consumers. Worker updates perform no world query.
- **Native type count:** zero new `UObject` classes. The existing reflected settings and prediction
  structs keep their names and defaults but move to a focused header; one non-reflected value result
  and namespace functions isolate the engine-facing world-query mechanism for deterministic tests.
- **Replication and persistence:** none added or changed. Trajectory correction and landing
  prediction are locally derived cosmetic presentation state; they neither replicate nor save and
  never start a physical landing.
- **Designer/editor work:** no Blueprint, DataAsset, Pose Search database, manifest, or content asset
  changes. Collision settings stay on the existing AnimBP-facing properties. No MCP/editor authoring
  is required for this slice.
- **Stable tests:** `SurvivalRpg.Animation.Trajectory.CollisionAndTimeToLand` owns sampling,
  collision, walkability, bounded-query, and landing-prediction semantics;
  `SurvivalRpg.Animation.MotionMatching.GroundDatabaseResolver` protects the shared sampling
  contract; `SurvivalRpg.Animation.Gasp.PilotAssetContract` protects reflected defaults and the
  existing AnimBP bindings.

## Issue #81 extraction order

1. Extract GASP sampling constants plus collision, prediction, and value helpers from
   `URpgAnimInstance` while preserving reflected settings, proxy values, semantics, and tests.
   `PoseSearchGenerateTransformTrajectory` orchestration and snapshot lifetime remain in proxy
   `PreUpdate`.
2. Replace package-name presentation classification with explicit, validated designer-owned
   membership that is safe to consume during worker updates. This slice is implemented by
   `URpgGaspPresentationProfile` plus its immutable game-thread-built lookup; six legacy-only Run
   sequences remain explicit members to preserve the previous classifier exactly.
3. Split Motion Matching role resolution, turn-in-place, and jump/landing into focused value-only
   runtimes without changing serialized AnimBP names or behavior. Slice 3a now owns pointer-free
   role selection, ground-domain interruption, landing-role classification, and PostSelection
   policy in `RpgMotionMatchingRuntime`. Slice 3b moves the pointer-free turn-in-place lifecycle and
   synthetic facing policy to `RpgTurnInPlaceRuntime`. Slice 3c moves physical jump edges and the
   bounded backward-start/fall continuation policy to `RpgJumpRuntime`; slice 3d moves final-airborne
   capture, landing role/fallback, handoff, serial, and timeout policy to `RpgLandingRuntime`.
4. Externalize database-to-role mapping and feel tuning into the existing presentation profile;
   keep the AnimInstance as the stable AnimBP facade and engine callback coordinator. This step is
   implemented by the exact 18-database hard-reference set, role-tag-built immutable cache, and
   copied `FRpgGaspLocomotionTuning` snapshot.

Each step must build independently and keep the pilot Experience reversible. New locomotion
families, Sprint authority, combat polish, and default PawnData cutover are separate issues.

## Slice 2 boundary and verification contract

- **Authority and lifecycle:** the presentation profile is static cosmetic configuration. Each
  AnimInstance resolves sequence metadata and builds its asset-to-trait lookup only in
  `NativeInitializeAnimation`; worker callbacks perform pointer lookups only. CharacterMovement,
  GAS, montage suppression, touchdown, and movement authority are unchanged.
- **Native type count:** one `UDataAsset` schema is added because the complete membership is
  designer-owned content that must remain editable without recompiling C++. The supporting entry,
  validation result, and lookup are value types; no manager or replicated runtime class is added.
- **Replication and persistence:** none added or changed. The hard profile reference and its
  sequence references provide deterministic load/cook ownership; the derived cache is local,
  transient, immutable after initialization, and never saved or replicated.
- **Designer/editor work:** one `DA_RpgGaspPresentationProfile` asset contains exactly 170 unique
  mappings: 124 GroundMoving, 16 JumpStart, 2 BackwardJumpStart, 1 AirborneFall, and 27 Landing.
  The existing pilot AnimBP holds the only new binding. Existing 18 database bindings, serialized
  property names, graph composition, thresholds, PawnData, and Experience remain untouched.
- **Stable tests:** `SurvivalRpg.Animation.Gasp.PresentationProfileValidation` protects structural
  validation and derived traits; `SurvivalRpg.Animation.Jump.Runtime.PhaseAndProceduralGates`
  proves package/name independence and immutable lookup behavior; the GASP content and pilot
  contracts protect the exact 170-member set, profile binding, native DataValidation, and direct
  cook dependency.

## Slice 3a boundary and verification contract

- **Authority and lifecycle:** CharacterMovement remains authoritative. The AnimInstance callback
  built a pointer-free movement snapshot, `RpgMotionMatchingRuntime` deterministically resolved
  ordered roles and completed-search policy, and the callback facade alone mapped those roles to
  configured database pointers in this slice. The runtime performs no actor, component, world,
  ASC, or asset access and creates no parallel gameplay or locomotion state machine.
- **Native type count:** zero new `UObject` classes. Three non-reflected value structs plus one
  bounded role-list alias and focused namespace functions move the existing selector,
  ground-domain interrupt, landing-role classification, and PostSelection policy out of
  `URpgAnimInstance`.
- **Replication and persistence:** none added or changed. Selection and latch results remain local,
  derived cosmetic presentation state; no network role, authority input, replicated property, or
  saved state is introduced.
- **Designer/editor work:** this slice changed no Blueprint, DataAsset, Pose Search database,
  manifest, or content asset. Every reflected database property name, default, graph callback, and
  authored tuning value remained compatible; the later step 4 now owns database-to-role mapping
  and feel through `DA_RpgGaspPresentationProfile`.
- **Stable tests:** `SurvivalRpg.Animation.MotionMatching.GroundDatabaseResolver` directly owns the
  extracted role ordering, boundaries, pivot, interrupt, landing-role, and PostSelection value
  contracts while retaining coverage of the AnimInstance role-to-database bridge. The jump,
  landing, turn-in-place, and pilot asset contracts guard adjacent lifecycle and serialization
  behavior; the complete `SurvivalRpg.Animation` filter is the regression gate.

## Slice 3b boundary and verification contract

- **Authority and lifecycle:** actor rotation, CharacterMovement, GAS tags, montage state, and jump
  state remain authoritative outside this cosmetic runtime. `URpgAnimInstance` assembles immutable
  proxy/config/latch observations; `RpgTurnInPlaceRuntime` resolves only value state, Offset Root
  policy, reset pulses, and synthetic facing. It never rotates the actor or reads an Actor,
  Component, World, ASC, database, animation asset, or Blend Stack node.
- **Native type count:** zero new `UObject` classes. Three non-reflected value structs, one search
  enum, constants, and namespace functions move eligibility, yaw quantization, collection and
  recovery hysteresis, hard-reset edges, request serial policy, timeouts, and trajectory facing out
  of `URpgAnimInstance`.
- **Replication and persistence:** none added or changed. The existing flat transient Blueprint
  properties keep their names, types, and defaults as the stable read facade. The request state is
  locally derived and cosmetic; it is neither replicated nor saved.
- **Designer/editor work:** this slice changed no Blueprint, DataAsset, Pose Search database,
  manifest, PawnData, or Experience. The database pointer, GC-tracked selected asset,
  completed-search callback, and Blend Stack playback observation remained in `URpgAnimInstance`;
  the later step 4 now supplies its thresholds from the copied profile tuning.
- **Stable tests:** `SurvivalRpg.Animation.TurnInPlace.AngleAndTrajectory` directly owns the pure
  quantization, wrap-safe yaw, authored durations, and synthetic-sample contract.
  `SurvivalRpg.Animation.TurnInPlace.StateMachine` retains the complete facade integration contract
  for eligibility, hysteresis, serials, search priority, GC-safe selection, playback observation,
  reset edges, timeouts, and 20/60/120-FPS stability. The pilot asset contract and complete
  `SurvivalRpg.Animation` filter remain the serialization and regression gates.

## Slices 3c and 3d boundary and verification contract

- **Authority and lifecycle:** CharacterMovement remains the sole physical airborne/grounded truth.
  `RpgJumpRuntime` interprets those immutable edges and owns only the bounded backward-start/fall
  Continuing-Pose policy. `RpgLandingRuntime` captures final-airborne values on game-thread
  `PreUpdate` and resolves cosmetic landing roles, fallback, one bounded handoff, request serials,
  and timeouts on the animation update thread. Prediction never emits a touchdown transition.
  `URpgAnimInstance` alone coordinates phase intents and preserves the callback priority TIR,
  Landing, backward/fall continuation, then normal locomotion.
- **Native type count:** zero new `UObject` classes. The two focused headers contain only
  non-reflected enums, snapshots, state/results, constants, and namespace functions. The existing
  flat reflected `JumpPhase`, landing snapshot/debug fields, database properties, and callback
  signatures keep their names, types, flags, and defaults.
- **Replication and persistence:** none added or changed. Capture and lifecycle state remain local,
  transient, pointer-free cosmetic values derived from each role's existing CharacterMovement
  snapshot. No replicated property, save data, RPC, authority path, or parallel gameplay state
  machine is introduced.
- **Designer/editor work:** these slices changed no Blueprint, DataAsset, Pose Search database,
  presentation profile, manifest, PawnData, Experience, or content asset. The six landing database
  pointers, Heavy threshold, immutable presentation lookup, GC-tracked selected/held assets,
  PostSelection latch, and Blend Stack playback observation remained in `URpgAnimInstance`; the
  later step 4 now supplies database membership and feel through the profile seam.
- **Stable tests:** `SurvivalRpg.Animation.Jump.Runtime.LandingSelection` directly owns finite
  capture, epoch/prediction, inclusive 3/700 boundaries, role/fallback, handoff, serial, and watchdog
  policy. `SurvivalRpg.Animation.Jump.Runtime.PhaseAndProceduralGates` owns physical phase edges,
  backward-start/fall continuation, callback facade, exact asset latches, GC cleanup, and procedural
  presentation gates. Trajectory, pilot asset/reflection, and the complete `SurvivalRpg.Animation`
  filter remain adjacent regression gates.

## Slice 4 boundary and verification contract

- **Authority and lifecycle:** `DA_RpgGaspPresentationProfile` is static cosmetic configuration.
  During `NativeInitializeAnimation`, the AnimInstance validates it, resolves each database's Role
  tag, builds immutable role-to-pointer and pointer-to-role caches, and copies the tuning struct.
  Parallel animation updates read only those caches and value snapshots. CharacterMovement, GAS,
  montage suppression, touchdown authority, request serials, latch/reset policy, and native
  fail-safe mechanisms are unchanged.
- **Configuration mode:** a non-empty profile database array selects the whole-profile path. All 18
  non-`None` roles, presentation coverage, and tuning must validate together; invalid or partial
  profile configuration fails closed and never mixes individual legacy bindings into the cache.
  An absent or empty profile database array selects the whole-legacy path, retaining all historical
  database bindings and the legacy Heavy-landing threshold with native tuning defaults. The legacy
  bindings are cached only when all 18 are non-null and unique; an already validation-invalid partial
  legacy CDO fails closed rather than exposing a partial worker-thread cache.
- **Native type count:** zero new `UObject` classes. The existing profile gains the hard-reference
  array and one reflected value-only tuning struct; non-reflected lookup/config helpers build and
  serve the immutable cache. The stable native role enum remains the runtime schema.
- **Replication and persistence:** none added or changed. Profile data is immutable static content;
  derived caches and tuning snapshots are local, transient, unsaved, and unreplicated. No profile
  value becomes authoritative gameplay state.
- **Designer/editor work:** the only Step 4 content delta is the existing
  `DA_RpgGaspPresentationProfile`, which now hard-references exactly 18 unique runtime databases and
  therefore serves as their deterministic load/cook root while retaining its 170 sequence
  memberships. No AnimBP, PawnData, Experience, Pose Search database, animation, chooser, schema,
  normalization, manifest, or plugin-asset-count change is part of this step.
- **Tag contract:** exactly one known `Rpg.MotionMatching.Role.*` tag identifies every database and
  is consumed only during the game-thread cache build. `Rpg.MotionMatching.State.*` tags are no
  longer part of profile validation or runtime selection; existing tags may remain as additive
  metadata and legacy asset-validation compatibility.
- **Stable tests:** the presentation-profile validation contract protects exact role coverage,
  hard references, presentation coverage, tuning validity, and fail-closed cache construction.
  Motion Matching, turn, jump, landing, pilot asset, and complete `SurvivalRpg.Animation` contracts
  continue to protect defaults, boundary behavior, serialization compatibility, and the unchanged
  authority/safety mechanisms.

## Issues #81 and #97 real-network acceptance boundary

- **Authority and lifecycle:** the editor-only acceptance test drives the existing Experience,
  PawnData, CharacterMovement, ASC, and AnimInstance paths. Issue #97 keeps gameplay authority in
  CharacterMovement, replaces the project-specific acceleration payload with UE 5.8
  `FRepMovement`, and adds only a semantic teleport epoch plus local presentation-reset serial.
- **Topology:** the CQTest starts a listen host and one PIE client, late-joins a second client
  while the original subject is moving, then late-joins a third client while that subject is
  stationary. The subject is observed as Authority, AutonomousProxy, and SimulatedProxy.
- **Network profile:** the test uses `PktLag=60` and `PktLagVariance=10`; configured packet loss,
  reordering, and duplication remain zero.
- **Stable test:**
  `SurvivalRpg.Network.GaspPilotPIE.ReplicationLateJoinCorrectionAndDefaultSlotMontage` protects
  native acceleration and analog-input reconstruction at 25/50/100%, moving/stationary late join,
  start/stop/reversal, a 90-degree facing change activating and completing the TIR lifecycle,
  stance, jump and landing state, grounded Foot Placement snapshots, role-correct owner correction
  and semantic-teleport resets, ASC `DefaultSlot` montage plumbing, authoritative root motion, and
  stable post-montage convergence.
- **Runtime/content delta:** zero new production runtime classes and zero content assets. Existing
  Character, CharacterMovement, and AnimInstance seams own the #97 behavior. The test and its
  replicated movement-base fixture remain confined to `SurvivalRpgEditor` and reuse the existing
  CQTest PIE seam.
- **Evidence boundary:** reflected state and authoritative movement are automated. Rendered pose
  selection, warping, IK quality, gameplay-notify behavior, packaged multi-process networking,
  combat/death/ragdoll behavior, performance, and default cutover remain separate gates. See
  [the network smoke runbook](../../../docs/gasp-network-smoke.md).

## Issue #101 coast boundary and subsequent network review changes

The original #101 slice introduced inputless-only `GroundCoastGait`. Subsequent network review
supersedes that transport with `GroundMovementGait` for active input and coast, and adds the
SavedMove/base-rounding and Free-rotation handoff fixes. The current contract below includes those
later changes; they are not attributed to the original #101 slice.

- **Authoritative truth:** CharacterMovement resolves Walk/Run from prediction-owned intent while
  input is active and retains its physical coast classification after release. `ARpgCharacter`
  stores the current standing `Idle/Walk/Run` classification in both cases.
- **Prediction:** `Custom0` records the owner's Run decision. A scoped `MoveAutonomous` solve
  validates it with only the bounded Quantize10 allowance, preserving the decision through
  server movement and client replay. Server `ForcePositionUpdate` scopes the same validation
  around its last accepted gait while extrapolating missing moves; reused acceleration does not
  become a fresh input decision.
- **Replication:** `GroundMovementGait` uses `COND_SimulatedOnly` plus RepNotify. The movement
  component consumes Walk/Run only for a standing, uncrouched GASP profile, during active input
  and coast; stop, non-standing movement, and profile opt-out clear the authority transport to Idle.
- **Lifecycle:** a newly created or newly relevant proxy consumes current server gait before local
  history or residual-speed fallback, preserving active Run inside the 0.65–0.70 retention band
  and Run coast below the Walk cap. The hint
  survives PawnData/profile application ordering and clears deterministically at physical stop.
- **Rotation handoff:** Character authority advances an owner-only revision on policy/possession
  changes. After actually applying Free, the owner reliably acknowledges its last pre-Free
  movement timestamp. CMC requests a normal correction for a strictly newer move with
  `bForceClientUpdate`, includes authority yaw through `ShouldCorrectRotation`, and restores it
  before replay. The request survives unreliable response loss until the owner reliably confirms
  receipt; new revisions cancel superseded requests. No movement input is needed after release.
- **Presentation boundary:** the simulated-proxy gait bridge carries no AnimBP, Pose Search,
  Motion Matching, SavedMove, montage, or movement history. The existing owner-to-server SavedMove
  stream still carries `Custom0`. `URpgAnimInstance` consumes the prepared CMC gait through its
  worker-thread-safe snapshot; replicated rotation and CMC corrections remain native gameplay seams.
- **Experience boundary:** no content asset changes are required. `DA_PawnData_GASP` opts into the
  mechanism; the prototype PawnData remains opt-out and both Experiences remain independently
  selectable.
- **Stable test:**
  `SurvivalRpg.Network.GaspPilotPIE.GroundCoastLateJoinAndRelevancyReturn` protects Walk and Run
  late join, Run coast below the Walk cap, real relevancy loss/recreation/return, role parity, and
  deterministic stop clearing in a rendered listen-server PIE session.
- **Subsequent review regressions:**
  `SurvivalRpg.Network.GaspPilotPIE.ActiveRunLateJoinAndRelevancyReturn` covers current Run state
  inside the retention band; `RotatedBasePreservesSavedRunAtExitThreshold` in the same suite
  covers base-space rounding; `RotationModeExitConvergesAfterInputRelease` covers delayed mode
  replication, idle yaw convergence and receipt confirmation. Native
  `SurvivalRpg.Character.Movement.SavedMovePrediction` also covers forced server extrapolation,
  and `SurvivalRpg.Character.RotationMode.HandoffTimestampBoundary` covers timestamp/reset bounds
  and an actual yaw-only CMC correction response with no positional error. These describe test
  scope; run results belong with the tested revision and report in the network smoke runbook.
