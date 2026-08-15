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
| `Update_MotionMatching` | `URpgAnimInstance::UpdateGaspMotionMatching`, native role resolver, AnimBP database defaults | Thin native node callback plus designer-owned database/profile configuration | The project keeps a curated role contract and excludes GASP BranchIn, experimental state-machine, Foley, and broad Chooser dependencies. Database externalization remains a later #81 slice. |
| `IsMoving`, `IsStarting`, `IsPivoting` | `IsGroundMotionMatchingChooserMoving` and `ResolveMotionMatchingDatabaseRoles`; the latter consumes the GASP-authored future-velocity window and project gait/stance snapshot | Focused value-only Motion Matching selection runtime plus validated designer-owned database/profile membership | Project gameplay gait and rotation mode constrain the authored source predicates. Selection remains cosmetic and reads only the proxy snapshot. |
| `Get_MMInterruptMode` | `ShouldInterruptGroundMotionMatching` plus explicit turn/landing request and playback latches in `UpdateGaspMotionMatching` | Focused value-only Motion Matching runtime coordinated by the native AnimNode callback | Interrupts also protect physical movement-domain changes and one-shot project turn/landing requests; they never become replicated gameplay state. |
| `Update_MotionMatching_PostSelection` | `URpgAnimInstance::UpdateGaspMotionMatchingPostSelection` and request latches | Thin engine callback bridge plus focused value-only runtimes | Completed-search metadata is cosmetic only. Unlike source GASP, the project does not apply `Override Motion Matching Blend Settings`; the authored node retains uniform `0.2 s` blending after the FastFeet turn regression. |
| `Update_States`, `Update_Logic` | Proxy movement snapshot plus native turn/jump/landing presentation lifecycles | Gameplay state in CharacterMovement/GAS; focused presentation runtimes and AnimBP/profile tuning | The sample's broad and experimental state controller is not copied. Physical airborne/grounded state always comes from CharacterMovement. |
| `ShouldTurnInPlace`, `Get_TrajectoryTurnAngle` | `URpgAnimInstance` turn-in-place resolver, synthetic trajectory, latch, and watchdog | Focused turn-in-place runtime; feel thresholds in designer configuration | Turns remain controller-facing and cosmetic; they never rotate the authoritative actor. Extraction is pending in #81. |
| `JustLanded_Light`, `JustLanded_Heavy`, `Get_LandVelocity`, `PlayLand`, `PlayMovingLand` | Proxy pre-touchdown snapshot plus `URpgAnimInstance` landing resolver/latch/watchdog | Focused landing runtime; database membership and feel in assets | Landing begins only on the physical CMC touchdown edge. Idle/Walk/Run Light/Heavy are curated; Sprint landing remains deferred until authoritative Sprint issue #62. |
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
   runtimes without changing serialized AnimBP names or behavior.
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
