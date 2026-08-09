# RPG GASP Locomotion

This content-only plugin owns the curated Game Animation Sample Project (GASP) locomotion substrate for SurvivalRpg. It does not select a PawnData, Experience, or Animation Blueprint; the isolated runtime pilot was delivered by issue #54.

## Curated slice

- 107 retargeted `UAnimSequence` assets using `/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin`
- Database membership: 2 stand idle/transition, 8 turn-in-place, 10 crouch, 29 walk, 25 run, 10 sprint, 19 airborne jump, and 4 stand-light landing entries
- Crouch slice: 1 idle, 2 transitions, and 8 directional walk loops. `PSD_Rpg_Crouch` owns the idle, Stand-to-Crouch transition, and 8 walk loops (10 entries); Crouch-to-Stand is the second `PSD_Rpg_Stand_Idle` entry.
- Turn-in-place slice: authored 45, 90, 135, and 180 degree turns in both directions live exclusively in `PSD_Rpg_Stand_TurnInPlace`; `PSD_Rpg_Stand_Idle` retains only neutral idle and Crouch-to-Stand.
- Run slice: the five added BL, BR, FL, FR, and LL loops complete the curated eight-way directional core without importing the dense GASP movement library.
- Jump slice: `PSD_Rpg_Jump` owns exactly 18 bounded directional/core start/off clips plus the single looping fall hold. `PSD_Rpg_Stand_Idle_Lands_Light` owns four non-looping B/F/LL/RL stand-light landings. Moving and heavy landing libraries remain out of scope.
- `PSS_Rpg_Jump` is a specialized GASP-close local schema with the four-sample jump trajectory, foot-relative pose features, paired `FeetVelZ` channels, and pelvis heading. Both jump-phase databases use full-range `[0.0,0.0]` sampling so the post-touchdown landing query can select an authored contact pose.
- 1 project-local mirror table, 2 Pose Search schemas, 8 Pose Search databases, 1 normalization set, and 1 database chooser
- 120 assets after UE 5.8 retargeting and compression

Unreal recompresses retargeted sequences with the project/engine defaults, so on-disk size is not part of the content contract. No padding or unrelated sample content is retained to meet an estimated size range.

## Import contract

- Root motion remains enabled, normalized, force-locked, and locked to the reference pose.
- Source curve names and case are preserved, including `Phase`/`phase` and `MoveData_Speed`/`movedata_speed` variants.
- Native Pose Search sampling, exclusion, transition, and cost notifies are preserved.
- Sample Foley, EarlyTransition, BranchIn database references, and experimental state-machine asset user data are removed.
- `PSD_Rpg_Stand_TurnInPlace` carries only the `TurnInPlace` tag, uses `BaseCostBias=-0.2` and `ContinuingPoseCostBias=-0.05`, and keeps all eight entries unmirrored and non-looping. Every entry uses `SamplingRange=[0.0,0.01]`, which deliberately indexes exactly the authored `t=0` pose at the 30 Hz schema rate so controller-facing turns cannot enter after their root-yaw section.
- The final plugin contains no GASP source skeleton, mesh, IK rig, retargeter, Sample Character, Traversal, Camera, Mover, Locomotor, NetworkPrediction, Foley, Audio, or MetaSound content.

The complete source-to-target mapping and cleanup policy is recorded in `CuratedAssetManifest.csv`.

## Runtime boundary

`CHT_Rpg_LocomotionDatabases` keeps the five standing/airborne gait databases as unfiltered rows; the dedicated light-landing database is deliberately excluded. It is an authoring substrate, not the final gameplay-state selector. Runtime selects crouch, turn-in-place, and the bounded post-touchdown landing database through dedicated AnimInstance properties. Issue #54 delivered the isolated Animation Blueprint, PawnData, and Experience integration; issue #66 adds directional jump/landing phase ownership without importing GASP Chooser, State Controller, Traversal, Foley, or dense landing content.
