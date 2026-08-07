# RPG GASP Locomotion

This content-only plugin owns the curated Game Animation Sample Project (GASP) locomotion substrate for SurvivalRpg. It does not select a PawnData, Experience, or Animation Blueprint; that isolated runtime pilot belongs to issue #54.

## Curated slice

- 95 retargeted `UAnimSequence` assets using `/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin`
- Standing/airborne database split: 2 stand idle/transition, 8 turn-in-place, 29 walk, 25 run, 10 sprint, and 11 jump/land entries
- Crouch slice: 1 idle, 2 transitions, and 8 directional walk loops. `PSD_Rpg_Crouch` owns the idle, Stand-to-Crouch transition, and 8 walk loops (10 entries); Crouch-to-Stand is the second `PSD_Rpg_Stand_Idle` entry.
- Turn-in-place slice: authored 45, 90, 135, and 180 degree turns in both directions live exclusively in `PSD_Rpg_Stand_TurnInPlace`; `PSD_Rpg_Stand_Idle` retains only neutral idle and Crouch-to-Stand.
- Run slice: the five added BL, BR, FL, FR, and LL loops complete the curated eight-way directional core without importing the dense GASP movement library.
- 1 project-local mirror table, 2 Pose Search schemas, 7 Pose Search databases, 1 normalization set, and 1 database chooser
- 107 assets after UE 5.8 retargeting and compression

Unreal recompresses retargeted sequences with the project/engine defaults, so on-disk size is not part of the content contract. No padding or unrelated sample content is retained to meet an estimated size range.

## Import contract

- Root motion remains enabled, normalized, force-locked, and locked to the reference pose.
- Source curve names and case are preserved, including `Phase`/`phase` and `MoveData_Speed`/`movedata_speed` variants.
- Native Pose Search sampling, exclusion, transition, and cost notifies are preserved.
- Sample Foley, EarlyTransition, BranchIn database references, and experimental state-machine asset user data are removed.
- The final plugin contains no GASP source skeleton, mesh, IK rig, retargeter, Sample Character, Traversal, Camera, Mover, Locomotor, NetworkPrediction, Foley, Audio, or MetaSound content.

The complete source-to-target mapping and cleanup policy is recorded in `CuratedAssetManifest.csv`.

## Runtime boundary

`CHT_Rpg_LocomotionDatabases` keeps the five standing/airborne gait databases as unfiltered rows. It is an authoring substrate, not the final gameplay-state selector. The runtime selects `PSD_Rpg_Crouch` and `PSD_Rpg_Stand_TurnInPlace` directly through their dedicated AnimInstance properties, with selector priority Airborne > Crouch > Turn-in-place > Gait. Issue #54 owns the isolated Animation Blueprint, PawnData, and Experience integration; issue #55 owns multiplayer verification and cutover.
