# GASP runtime ownership map

This map records how the relevant UE 5.8 GASP CMC Blueprint responsibilities are adapted by
SurvivalRpg. It is the behavior-preserving architecture baseline for issue #81; it does not make
the sample project a runtime dependency.

## Ownership rules

- CharacterMovement, GAS, equipment, and the Lyra-derived character lifecycle own gameplay truth.
- `URpgAnimInstance::InitializeWithAbilitySystem` owns the game-thread ASC/tag-map lifecycle.
  `FRpgAnimInstanceProxy::PreUpdate` collects actor, movement-component, and world data and copies
  the already mirrored gameplay-tag booleans into the immutable proxy snapshot.
- Worker-thread animation consumes only immutable values and already loaded animation assets.
- AnimBP, Choosers, Pose Search databases, and future presentation profiles own concrete asset
  membership, graph composition, blending, warping, IK feel, and presentation tuning.
- Focused C++ helpers are retained only for engine callbacks, game-thread world queries, immutable
  snapshots, custom AnimNodes, and small deterministic resolvers.

## Source-to-project mapping

| GASP CMC source responsibility | Current SurvivalRpg owner | Target owner | Intentional adaptation |
| --- | --- | --- | --- |
| `Update_PropertiesFromCharacter`, `Update_EssentialValues` | `InitializeWithAbilitySystem` for the ASC/tag map; `FRpgAnimInstanceProxy::PreUpdate` for the value snapshot | Existing ASC lifecycle plus proxy snapshot boundary | ASC delegates are initialized on the game thread. `PreUpdate` copies mirrored tag booleans and reads CharacterMovement/world state; no sample character hierarchy is imported. |
| `Update_Trajectory` | Proxy `PreUpdate` orchestrates `PoseSearchGenerateTransformTrajectory`; `RpgPoseSearchTrajectory` owns its sampling constants and validation/correction helpers | Same split: proxy owns generation and snapshot lifetime; focused native helper owns the sampling/correction mechanism | Controller-yaw extrapolation is disabled for the controller-facing project character; raw history stays separate from worker-facing corrected output. |
| `HandleTransformTrajectoryWorldCollisions` | `RpgPoseSearchTrajectory::ResolveWorldCollision` | Focused game-thread trajectory helper | Uses bounded sphere sweeps, CharacterMovement walkability, explicit validity, floor projection, and a pointer-free landing prediction. It never changes movement or touchdown authority. |
| `Update_MotionMatching` | `URpgAnimInstance::UpdateGaspMotionMatching` bridges the AnimNode callback and database pointers; `RpgMotionMatchingRuntime` owns pointer-free role selection | Same split plus designer-owned database/profile configuration | The project keeps a curated role contract and excludes GASP BranchIn, experimental state-machine, Foley, and broad Chooser dependencies. Database externalization remains a later #81 slice. |
| `IsMoving`, `IsStarting`, `IsPivoting`, `Get_TrajectoryTurnAngle` | `RpgMotionMatchingRuntime::IsChooserMoving`, `GetRunPivotMinimumAngle`, and `ResolveDatabaseRoles`; the resolver consumes the GASP-authored future-velocity window and project gait/stance snapshot | Focused value-only Motion Matching selection runtime plus validated designer-owned database/profile membership | Project gameplay gait and rotation mode constrain the authored source predicates. The trajectory turn angle is the acceleration-versus-velocity pivot angle, not a turn-in-place rule. Selection remains cosmetic and pointer-free. |
| `Get_MMInterruptMode` | `RpgMotionMatchingRuntime::ShouldInterruptGroundMotionMatching` plus explicit turn/landing request and playback latches in `URpgAnimInstance::UpdateGaspMotionMatching` | Focused value-only Motion Matching runtime coordinated by the native AnimNode callback | Interrupts also protect physical movement-domain changes and one-shot project turn/landing requests; they never become replicated gameplay state. |
| `Update_MotionMatching_PostSelection` | `URpgAnimInstance::UpdateGaspMotionMatchingPostSelection` bridges selected database metadata; `RpgMotionMatchingRuntime::ResolvePostSelection` owns the pointer-free result and exclusive latch policy | Same callback/value-runtime split | Completed-search metadata is cosmetic only. Unlike source GASP, the project does not apply `Override Motion Matching Blend Settings`; the authored node retains uniform `0.2 s` blending after the FastFeet turn regression. |
| `Update_States`, `Update_Logic` | Proxy movement snapshot plus `RpgTurnInPlaceRuntime`, `RpgJumpRuntime`, and `RpgLandingRuntime`; `URpgAnimInstance` coordinates their reflected and UObject bridges | Gameplay state in CharacterMovement/GAS; focused presentation runtimes and AnimBP/profile tuning | The sample's broad and experimental state controller is not copied. Physical airborne/grounded state always comes from CharacterMovement. |
| `ShouldTurnInPlace` | `RpgTurnInPlaceRuntime` owns pointer-free eligibility, hysteresis, reset edges, request/search policy, timeouts, and synthetic facing; `URpgAnimInstance` retains the reflected facade plus GC-safe asset/Blend Stack bridge | Same split; feel thresholds move to designer configuration in #81 step 4 | Turns remain controller-facing and cosmetic; they never rotate the authoritative actor. GASP's inclusive 50-degree OrientationIntent/root predicate and Chooser 20 cm/s cap are intentionally adapted to project actor-yaw accumulation, 20/30/10-degree hysteresis, and the stricter 3 cm/s stationary gate. |
| Jump movement-mode edges and bounded backward Jump Start/Fall continuation | `RpgJumpRuntime` owns pointer-free physical edge interpretation and Continuing-Pose policy; `URpgAnimInstance` retains the reflected phase plus GC-tracked held asset and trait bridge | Same value-runtime/facade split | An upward velocity change while already airborne is not a new phase. The bounded backward hold is a project adaptation for curated non-looping starts and never changes movement authority. |
| `JustLanded_Light`, `JustLanded_Heavy`, `Get_LandVelocity`, `PlayLand`, `PlayMovingLand` | `RpgLandingRuntime` owns pointer-free final-airborne capture, role/fallback, request serials, handoff, and timeout policy; `URpgAnimInstance` retains database pointers, the reflected facade, exact asset latch, and Blend Stack observation | Same split; database membership and feel move to assets in #81 step 4 | Landing begins only on the physical CMC touchdown edge. Prediction may strengthen severity but never starts landing. Idle/Walk/Run Light/Heavy are curated; Sprint landing remains deferred until authoritative Sprint issue #62. |
| `AllowFootPinning`, foot-placement settings helpers | Proxy game-thread traces, `RpgFootPlacement` value helpers, `FAnimNode_RpgFootPlacement` | Existing focused native types/node plus AnimBP tuning | Project-local snapshots make the node worker-safe and preserve moving-base handling; crouch stays opted out by default. |
| `Get_DesiredFacing`, `EnableSteering`, Orientation Warping gates | `URpgAnimInstance::GetGaspBlendStackInputs`, authored curves, and the immutable `FRpgGaspPresentationAssetLookup` | Existing AnimBP/presentation profile seam, with only value inputs supplied natively | `DA_RpgGaspPresentationProfile` explicitly preserves the old 170-sequence presentation domain without package/name reads on workers. Local fall, idle/crouch/TIP, and moving-landing adaptations remain unchanged. |
| Sparse database and state selection | Fixed native role enum/mapping plus project-local Pose Search assets | Validated designer-owned profile/Chooser with stable native schema only where required | Curated databases remain project-owned; fixed mappings are compatibility state during #81, not the intended final presentation owner. |
| Slide and other optional locomotion families | Not adopted; no runtime owner | No #81 owner; Adopt/Defer/Reject decision belongs to audit issue #74 | #81 does not pre-approve content families or grow the AnimInstance role architecture. |
| `Debug_ExperimentalStateMachine`, full GASP Mover/Traversal stack, sample camera, Foley | Not adopted; no runtime owner | No runtime owner without a dedicated isolated feature evaluation | These sample subsystems are outside the CMC pilot contract and are never incidental dependencies. Curated traversal assets may only enter later through project-owned CMC/GAS/Motion-Warping seams. |
| Sample character/mode composition | Existing `BP_Rpg_Character_GASP`, `DA_PawnData_GASP`, and pilot Experience | Existing Lyra-derived pilot composition remains unchanged | Only the GASP sample character hierarchy is not adopted. #81 does not change PawnData, Experience selection, or the default pawn cutover. |

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
4. Externalize database-to-role mapping and feel tuning into an asset/profile seam; keep the
   AnimInstance as the stable AnimBP facade and engine callback coordinator.

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
  builds a pointer-free movement snapshot, `RpgMotionMatchingRuntime` deterministically resolves
  ordered roles and completed-search policy, and the callback facade alone maps those roles to
  configured database pointers. The runtime performs no actor, component, world, ASC, or asset
  access and creates no parallel gameplay or locomotion state machine.
- **Native type count:** zero new `UObject` classes. Three non-reflected value structs plus one
  bounded role-list alias and focused namespace functions move the existing selector,
  ground-domain interrupt, landing-role classification, and PostSelection policy out of
  `URpgAnimInstance`.
- **Replication and persistence:** none added or changed. Selection and latch results remain local,
  derived cosmetic presentation state; no network role, authority input, replicated property, or
  saved state is introduced.
- **Designer/editor work:** no Blueprint, DataAsset, Pose Search database, manifest, or content
  asset changes. Every reflected database property name, default, graph callback, and authored
  tuning value remains compatible. Database-to-role mapping and feel externalization stay in
  issue #81 step 4, so no MCP/editor authoring is required for this slice.
- **Stable tests:** `SurvivalRpg.Animation.MotionMatching.GroundDatabaseResolver` directly owns the
  extracted role ordering, boundaries, pivot, interrupt, landing-role, and PostSelection value
  contracts while retaining coverage of the AnimInstance database-pointer bridge. The jump,
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
- **Designer/editor work:** no Blueprint, DataAsset, Pose Search database, manifest, PawnData, or
  Experience changes. The database pointer, GC-tracked selected asset, completed-search callback,
  and Blend Stack playback observation remain in `URpgAnimInstance`; threshold externalization is
  still issue #81 step 4, so no MCP/editor authoring is required.
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
- **Designer/editor work:** no Blueprint, DataAsset, Pose Search database, presentation profile,
  manifest, PawnData, Experience, or content asset changes. The six landing database pointers,
  Heavy threshold, immutable presentation lookup, GC-tracked selected/held assets, PostSelection
  latch, and Blend Stack playback observation remain in `URpgAnimInstance`. No MCP/editor authoring
  is required; database/tuning externalization remains issue #81 step 4.
- **Stable tests:** `SurvivalRpg.Animation.Jump.Runtime.LandingSelection` directly owns finite
  capture, epoch/prediction, inclusive 3/700 boundaries, role/fallback, handoff, serial, and watchdog
  policy. `SurvivalRpg.Animation.Jump.Runtime.PhaseAndProceduralGates` owns physical phase edges,
  backward-start/fall continuation, callback facade, exact asset latches, GC cleanup, and procedural
  presentation gates. Trajectory, pilot asset/reflection, and the complete `SurvivalRpg.Animation`
  filter remain adjacent regression gates.
