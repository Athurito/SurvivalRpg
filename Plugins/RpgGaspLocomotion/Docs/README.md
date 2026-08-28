# RPG GASP Locomotion

This content-only plugin owns the curated Game Animation Sample Project (GASP) locomotion substrate for SurvivalRpg. It does not select a PawnData, Experience, or Animation Blueprint; the isolated runtime pilot was delivered by issue #54.

The behavior-preserving source-to-project responsibility map and the staged issue #81 extraction
order are recorded in [RuntimeOwnershipMap.md](RuntimeOwnershipMap.md).

The real listen-server acceptance procedure for issues #81, #97, #100, #101, and #103 is recorded
in [the GASP real-network smoke runbook](../../../docs/gasp-network-smoke.md). It uses an actual PIE
network session with native acceleration, analog gait prediction/correction, moving and stationary
late joins, Walk/Run coast late joins, actor-channel relevancy return, and dynamic-base correction
classification on AutonomousProxy and SimulatedProxy views; rendered pose-selection, warping,
Foot Placement, and correction quality remain a separate visual inspection boundary.

## Curated slice

- 190 retargeted `UAnimSequence` assets using `/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin`
- Database membership: 2 stand idle/transition, 8 turn-in-place, 10 crouch, 25 moving walk plus 12 source-exact walk stops, 71 source-exact Sparse run entries split as 11 loops, 34 pivots, 14 starts, and 12 stops, 8 moving sprint plus 2 source-exact sprint stops, 19 airborne jump, 4 Idle-Light landings, 4 curated Idle-Heavy landings, 5 Walk-Light, 4 source-exact Walk-Heavy, 5 Run-Light, and 5 Run-Heavy landings. The prior 41-entry run aggregate remains only as a migration fallback and is excluded from shared normalization/runtime selection.
- Crouch slice: 1 idle, 2 transitions, and 8 directional walk loops. `PSD_Rpg_Crouch` owns the idle, Stand-to-Crouch transition, and 8 walk loops (10 entries); Crouch-to-Stand is the second `PSD_Rpg_Stand_Idle` entry.
- Turn-in-place slice: authored 45, 90, 135, and 180 degree turns in both directions live exclusively in `PSD_Rpg_Stand_TurnInPlace`; `PSD_Rpg_Stand_Idle` retains only neutral idle and Crouch-to-Stand.
- Run slice: four source-exact Sparse databases preserve their physical GASP memberships. The pivot database contains 18 directly referenced neutral pivots, 8 Box pivots, and 8 authored 90/180-degree turns; its source mirror settings provide the omitted counterparts. The plugin still retains all 20 previously imported neutral pivot clips, including the two counterparts not directly referenced by the Sparse database. Dense Diamond, Hourglass, Prism, Spin, and 45/135-degree transition families remain out of scope.
- Stop slice: `PSD_Rpg_Stand_Walk_Stops` owns the source-ordered 12-way RR/RL/LR/LL/F/B walk stops, while `PSD_Rpg_Stand_Sprint_Stops` owns the two F sprint stops. Those clips are removed from the 25-entry Walk and 8-entry Sprint moving pools, so Motion Matching never searches the same Stop sample twice.
- Jump slice: `PSD_Rpg_Jump` owns exactly 18 bounded directional/core start/off clips plus the single looping fall hold. Six exclusive landing databases own Idle/Walk/Run Light/Heavy presentation. Idle-Light preserves its four non-looping B/F/LL/RL clips from issue #66; Idle-Heavy uses the analogous four lead-foot clips, while Walk/Run memberships follow the current GASP Dense databases. Sprint landing assets remain deferred until issue #62 provides a real gameplay-owned Sprint state.
- `PSS_Rpg_Jump` is a specialized GASP-close local schema with the four-sample jump trajectory, foot-relative pose features, paired `FeetVelZ` channels, and pelvis heading. All seven Jump and Landing databases use full-range `[0.0,0.0]` sampling so the post-touchdown landing query can select an authored contact pose.
- `PSS_Rpg_Locomotion` copies GASP `PSS_Default`'s 30 Hz Trajectory + Group channel graph, uses the local Skeleton/mirror table, finalizes to cardinality 30, and uses `NormalizeWithCommonSchema`. The schema requires root, foot_l, foot_r, and pelvis but no curves, so Pose History does not need a Phase curve collector.
- `PSS_Rpg_Stop` is the source-exact local GASP Stop schema: 30 Hz, cardinality 30, four trajectory samples at -0.05/0.0/0.35/0.7 seconds with global weight 5, and the four-channel continuing-pose group with a 0.3 pelvis-heading weight. It replaces only the source skeleton/mirror references with project-local assets and is used by Sprint Stops; Walk and Run Stops keep `PSS_Rpg_Locomotion`, matching GASP.
- 1 project-local mirror table, 3 Pose Search schemas, 19 Pose Search databases, 1 normalization
  set, 1 database chooser, and 1 presentation profile. The profile owns 170 explicit sequence
  categories, the exact 18-database runtime hard-reference set, and cosmetic locomotion tuning.
- 216 assets after UE 5.8 retargeting, compression, and presentation-profile authoring

Unreal recompresses retargeted sequences with the project/engine defaults, so on-disk size is not part of the content contract. No padding or unrelated sample content is retained to meet an estimated size range.

## Import contract

- Issue #73 was re-audited directly against the current local UE 5.8 GameAnimationSample checkout at commit `a9f560350f058462a4d72325bfe73a1e95ea0205` (`EpicSampleNameHash=1673045636`). That checkout has no upstream remote, so this local commit plus the curated manifest provide provenance, not a runtime dependency. Its newer Ragdoll system is intentionally outside this landing slice.

- Root motion remains enabled, normalized, force-locked, and locked to the reference pose.
- Source curve names and case are preserved, including `Phase`/`phase` and `MoveData_Speed`/`movedata_speed` variants.
- Native Pose Search sampling, exclusion, transition, and cost notifies are preserved.
- Sample Foley, EarlyTransition, BranchIn database references, and experimental state-machine asset user data are removed.
- Sparse database entry metadata is copied from GASP except `BranchInId`, which is reset because the corresponding notify states are removed. Ordinary full-range database search is the explicit local replacement; no dangling source synchronization dependency is retained.
- Landing databases use `PSS_Rpg_Jump`, `PSN_Rpg_Locomotion`, base bias `0`, looping bias `-0.005`, exclusion `[0,-0.3]`, and extrapolation `[-100,100]`. Continuing-pose bias is `-0.15` for both Idle pools, `-0.01` for Walk Light/Heavy, `-0.10` for Run Light, and `-0.01` for Run Heavy. Every curated entry is enabled, disables reselection, remains unmirrored-only/non-looping, and uses full-range sampling.
- Walk Stops retain GASP's `ContinuingPoseCostBias=-0.01` without the source `Stops` tag; Sprint Stops retain `ContinuingPoseCostBias=-0.2` and the exact source `Stops` tag. Both also carry their project-owned role/state tags. All 14 Stop entries are enabled, disable reselection, remain unmirrored-only/non-looping, and use full-range `[0.0,0.0]` sampling.
- The historical flat chooser remains an archival comparison source for the moving Walk/Sprint pools and legacy aggregate Run database; it deliberately does not reference the runtime Walk, Run, or Sprint Stop databases and never owns runtime selection. `RpgMotionMatchingRuntime` owns the pointer-free native selector with fixed Idle/Walk/Run/Sprint role shapes of 1/2/4/2; `URpgAnimInstance` remains the stable callback facade and maps roles through an immutable cache built from the presentation profile.
- `PSD_Rpg_Stand_TurnInPlace` retains the source `TurnInPlace` tag alongside its project-owned role/state tags, uses `BaseCostBias=-0.2` and `ContinuingPoseCostBias=-0.05`, and keeps all eight entries unmirrored and non-looping. Every entry uses `SamplingRange=[0.0,0.01]`, which deliberately indexes exactly the authored `t=0` pose at the 30 Hz schema rate so controller-facing turns cannot enter after their root-yaw section.
- `DA_RpgGaspPresentationProfile` replaces runtime package/name classification with 170 explicit sequence categories: 124 GroundMoving, 16 JumpStart, 2 BackwardJumpStart, 1 AirborneFall, and 27 Landing. It deliberately includes six legacy-only Run clips outside the current Sparse runtime databases so the old folder classifier's domain remains exact. The same profile now hard-references the exact 18 runtime databases and owns Motion Matching, turn, jump, landing, blend, and warping presentation values. The pilot AnimBP hard-references the profile, making it the deterministic database load/cook root; each AnimInstance builds immutable trait and bidirectional database-role caches and copies tuning on the game thread before parallel updates. Physical Walk/Run gait, speed caps, response, and Free yaw belong to the movement profile on `DA_PawnData_GASP`, not this presentation asset.
- The final plugin contains no GASP source skeleton, mesh, IK rig, retargeter, Sample Character, Traversal, Camera, Mover, Locomotor, NetworkPrediction, Foley, Audio, or MetaSound content.

The complete source-to-target mapping and cleanup policy is recorded in `CuratedAssetManifest.csv`.

## Runtime boundary

`DA_PawnData_GASP` opts into the project-local `FRpgCharacterMovementProfile`; the prototype PawnData explicitly remains opt-out, so its Blueprint-authored CharacterMovement values and the independently selectable Prototype Experience are unchanged. For standing, uncrouched ground movement, `URpgCharacterMovementComponent` resolves the physical input before both ground physics and SavedMove capture: magnitudes at or below `0.10` become zero, while `0.11`, `0.25`, `0.50`, and every other value above the deadzone keep their original analog magnitude without renormalization, apart from CharacterMovement's native `NetQuantize10` precision. Crouching, falling, and custom movement modes retain their legacy input response.

The prediction-owned desired gait enters Run at input `>= 0.70`, retains Run at `>= 0.65`, and exits to Walk only below `0.65`. One CharacterMovement SavedMove `Custom0` flag stores that gait and restores it before authoritative server movement and client replay, so the selected 200/500 forward cap follows the same state through prediction, correction, and move combining. Simulated proxies do not receive the owner's SavedMove flag; they reconstruct active-input gait from replicated acceleration. During inputless Walk/Run coast, the authority publishes one current gait classification with `COND_SimulatedOnly`; a new or newly relevant proxy consumes it before local gait history and therefore preserves Run even below the Walk cap. The value clears to Idle at physical stop and never carries AnimBP, pose, Motion Matching, or movement history. A proxy first observed with active input directly inside the `0.65`-to-`0.70` retention band remains a separate ambiguity outside issue #101.

The profile retains GASP's forward/side/back source values and pure direction mapping as a validated contract, but #99 deliberately activates only the 200/500 forward caps: predicted rotation-request tags are not yet part of CharacterMovement saved moves, so making physical speed depend on controller-facing mode would create correction risk. `URpgAnimInstance` consumes the prepared CMC gait and never reclassifies Walk/Run from a cosmetic presentation threshold. Sprint and prediction-safe directional strafe caps remain separate gameplay slices.

Above the physical deadzone, SurvivalRpg forwards raw stick magnitude into CharacterMovement. Its pilot therefore behaves like GASP's variable-speed Walk/Run option: keyboard input reaches Run 500 exactly, while partial gamepad Walk remains analog-scaled (for example, 0.5 input with Walk 200 and `MinAnalogWalkSpeed=150` resolves to 150 cm/s). Enhanced Input may apply an additional UX filter, but it is not the movement invariant. GASP's default fixed-speed single-gait input normalizes non-zero stick input; that normalization remains a separate gameplay choice and is not silently folded into #100's SavedMove gait contract.

`CHT_Rpg_LocomotionDatabases` keeps the five historical standing/airborne rows as unfiltered archival references; the dedicated Walk/Sprint/Run Stop pools and all six landing databases are deliberately excluded. It remains a comparison and authoring source, not a gameplay-state selector or runtime dependency.

The production chooser is the focused pointer-free `RpgMotionMatchingRuntime`. It consumes value-only movement snapshots and first resolves stable `ERpgMotionMatchingDatabaseRole` values; only afterward does the `URpgAnimInstance` callback facade map those roles through the immutable profile cache. Ground roles preserve the fixed Idle/Walk/Run/Sprint shapes of 1/2/4/2, overlapping stop boundaries, the explicit Sprint-stop gait requirement, start/loop/pivot ordering, and domain-level interrupt behavior without making asset pointers part of chooser decisions. Profile-mode Database Role tags are inspected only while that bidirectional cache is built on the game thread; whole-legacy mode builds the same cache from the reflected slot roles. Worker callbacks never read the profile, database arrays, legacy slots, tags, packages, or paths.

Controller-facing turn presentation is similarly split. `RpgTurnInPlaceRuntime` owns the pointer-free actor-yaw accumulator, collection hysteresis, stability and recovery windows, one-shot request/search policy, reset edges, watchdog decisions, and the facing-only synthetic trajectory. The profile owns the cosmetic values and preserves the 20/30/10-degree and 3 cm/s defaults; `URpgAnimInstance` remains the stable Blueprint-property and AnimNode callback facade and coordinates the cached turn database, GC-tracked selected asset, and Blend Stack playback observation. This deliberately adapts GASP's simpler inclusive 50-degree OrientationIntent-versus-root predicate and 20 cm/s Chooser cap to the project's replicated capsule-facing policy; it never rotates authoritative gameplay state.

Jump and landing presentation use the same boundary. `RpgJumpRuntime` interprets pointer-free CharacterMovement phase edges and owns the bounded backward Jump Start/Fall Continuing-Pose policy. `RpgLandingRuntime` owns final-airborne capture, Light/Heavy and Stand/Walk/Run role resolution, same-gait Heavy-to-Light fallback, the single bounded stationary-to-moving handoff, request serials, and selection/playback timeout policy. The profile owns database membership and cosmetic values; `URpgAnimInstance` remains the stable reflected facade and coordinates cached pointers, GC-tracked selected/held assets, presentation-trait lookup, PostSelection latches, and Blend Stack observations. Neither runtime changes movement, replication, PawnData, Experiences, or gameplay authority.

Step 4 changes only the source of static presentation configuration. CharacterMovement/GAS authority,
physical movement edges, replicated state, serial/latch/reset policy, validation gates, and native
watchdog/fail-safe mechanisms remain native and behavior-preserving. The copied profile values tune
their cosmetic boundaries and durations but do not own those mechanisms.

Exactly 18 runtime databases participate in this role contract. Each carries exactly one matching
`Rpg.MotionMatching.Role.*` tag. The profile stores them as an unordered hard-reference set, validates
one unique database for every non-`None` role, and builds both role-to-pointer and pointer-to-role
maps. The current databases retain these state tags as descriptive/source metadata:

- Grounded: `StandIdle`, `StandWalk`, `StandWalkStops`, `StandRunLoops`, `StandRunPivots`, `StandRunStarts`, `StandRunStops`, `StandSprint`, and `StandSprintStops` use `Rpg.MotionMatching.State.Grounded`.
- Crouch: `Crouch` uses `Rpg.MotionMatching.State.Crouching`.
- Turn in place: `StandTurnInPlace` uses `Rpg.MotionMatching.State.TurnInPlace`.
- Airborne: `Jump` uses `Rpg.MotionMatching.State.Airborne`.
- Landing: `StandLightLanding`, `StandHeavyLanding`, `WalkLightLanding`, `WalkHeavyLanding`, `RunLightLanding`, and `RunHeavyLanding` use `Rpg.MotionMatching.State.Landing`.

`Rpg.MotionMatching.State.*` tags are no longer required or consulted by profile validation or
runtime selection; they may remain as additive metadata and for legacy asset-validation
compatibility. Other source behavior tags also remain additive rather than becoming role
identifiers: the Run Pivot database retains `Pivots`, Sprint Stops retain `Stops`, and Turn in Place
retains `TurnInPlace`. The legacy aggregate `PSD_Rpg_Stand_Run` is excluded from the 18-role runtime
contract and from shared normalization.

Configuration switches only as a whole. A non-empty profile database array selects profile mode;
all 18 roles, presentation coverage, and tuning must validate together, and an invalid or partial
profile fails closed without borrowing individual legacy pointers. An absent or empty profile
database array selects whole-legacy mode, preserving all historical AnimBP database bindings and
the legacy Heavy-landing threshold on top of native tuning defaults. Those 18 bindings are copied
into the same immutable cache as one complete set; an already invalid null, duplicate, or partial
legacy CDO now fails closed instead of publishing a partial worker-thread cache. This keeps existing
AnimBP serialization reversible without allowing an ambiguous mixed runtime configuration.

The Motion Matching node binds `UpdateGaspMotionMatchingPostSelection` through `OnMotionMatchingStateUpdatedFunction`. The callback facade resolves the selected database role, then `RpgMotionMatchingRuntime::ResolvePostSelection` records whether Pose Search continued the existing pose, preserves the pending interrupt mode, and computes the exclusive turn-in-place or exact active-landing latch for a newly selected matching result. Unlike source GASP, this hook does not override the authored uniform `0.2 s` node blend setting. Issue #54 delivered the isolated Animation Blueprint, PawnData, and Experience integration; issue #66 added directional jump/Idle-Light ownership; issue #73 extended that same serial/latch/watchdog lifecycle to Idle/Walk/Run Light/Heavy; issue #81 step 4 externalizes the database map and feel without importing the GASP Chooser, State Controller, Traversal, Foley, Sprint state, or Ragdoll system.

`FRpgLandingSelectionSnapshot` freezes horizontal velocity/direction, raw move intent, last grounded gait, measured downward speed, and the final valid trajectory contact for the physical Airborne-to-Grounded edge. `RpgLandingRuntime` preserves the profile's inclusive physical 3 cm/s Idle default and GASP's inclusive 700 cm/s Heavy default. Predicted impact speed may compensate a sparse low-FPS sample, but prediction never starts landing early. Missing Heavy content falls back to Light for the same gait; missing Light returns immediately to normal locomotion. Walk/Run landing roles keep authored `Enable_Warping` and Steering for the frozen request, while both Idle roles remain Reset-Root-only. Airborne input may capture the desired Walk/Run gait, but raw input alone cannot select a moving landing at zero horizontal speed. On the physical touchdown edge, a finite live speed inside the same Idle band rebases stale airborne Walk/Run momentum to the matching Stand severity before database fallback. Once the horizontal chooser becomes Moving inside the profile's default 0.3-second landing window, the request performs one severity-preserving Idle-to-Walk/Run landing handoff; later exits and speed-only mismatches return to normal locomotion. Completed, cancelled, and outgoing landing samples keep database-change interruption and Reset Root so neither their Continuing Pose nor root offset can hold the mesh in place. The snapshot and selection are cosmetic, pointer-free, unreplicated, and never own fall damage, stagger, rolls, or CharacterMovement.

The Step 4 asset delta is only the existing `DA_RpgGaspPresentationProfile`. No AnimBP, PawnData,
Experience, Pose Search database, animation, schema, chooser, normalization, manifest, or plugin asset
count changes are part of this externalization.

Pose Search trajectory collision is also project-owned. `FRpgAnimInstanceProxy::PreUpdate` keeps the raw kinematic history separate from the worker-facing corrected trajectory, applies gravity, and resolves at most the 15 generated future samples against simple Visibility collision. The defaults preserve GASP's 0.01 cm floor offset and 150 cm search height, while a small sphere sweep plus CharacterMovement walkability rejects walls and non-walkable slopes. Valid sweep contacts are projected along gravity onto the hit plane, so the later sample keeps its authored horizontal time and remains above flat floors, ramps, and steps. All world queries remain on the game thread; the AnimGraph receives only the corrected value trajectory.

The associated `FRpgTrajectoryLandingPrediction` is cosmetic and pointer-free: a valid snapshot contains the first walkable world-space contact, its normalized normal, and finite `TimeToLand`; no hit uses `bIsValid=false` and `TimeToLand=-1`. It is recomputed every update and invalidated on touchdown, missing owner/movement/world state, unsupported movement modes, malformed trajectories, teleports, large relocations, and owner/network-role changes. The prediction may inform airborne Pose Search and later moving/heavy landing selection, but it never starts a landing request, changes CharacterMovement, or becomes replicated gameplay truth. This deliberately tightens GASP's experimental helper, whose no-hit result is the trajectory horizon and whose collision result has no validity, point, normal, or walkability contract.
