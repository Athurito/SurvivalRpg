# RPG GASP Locomotion

This content-only plugin owns the curated Game Animation Sample Project (GASP) locomotion substrate for SurvivalRpg. It does not select a PawnData, Experience, or Animation Blueprint; that isolated runtime pilot belongs to issue #54.

## Curated slice

- 75 retargeted `UAnimSequence` assets using `/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin`
- Database split: 5 stand idle, 29 walk, 20 run, 10 sprint, and 11 jump/land sequences
- 1 project-local mirror table, 2 Pose Search schemas, 5 Pose Search databases, 1 normalization set, and 1 database chooser
- 85 assets and 71,739,642 bytes (68.4 MiB) on disk after UE 5.8 retargeting and compression

The original source slice was about 90.2 MiB. The smaller project-owned result is expected: Unreal recompressed the retargeted sequences with the project/engine defaults. No padding or unrelated sample content is retained to meet an estimated size range.

## Import contract

- Root motion remains enabled, normalized, force-locked, and locked to the reference pose.
- Source curve names and case are preserved, including `Phase`/`phase` and `MoveData_Speed`/`movedata_speed` variants.
- Native Pose Search sampling, exclusion, transition, and cost notifies are preserved.
- Sample Foley, EarlyTransition, BranchIn database references, and experimental state-machine asset user data are removed.
- The final plugin contains no GASP source skeleton, mesh, IK rig, retargeter, Sample Character, Traversal, Camera, Mover, Locomotor, NetworkPrediction, Foley, Audio, or MetaSound content.

The complete source-to-target mapping and cleanup policy is recorded in `CuratedAssetManifest.csv`.

## Runtime boundary

`CHT_Rpg_LocomotionDatabases` currently contains the five local databases as unfiltered rows. It is an authoring substrate, not the final gameplay-state selector. Issue #54 owns state columns, the isolated Animation Blueprint, PawnData, and Experience integration; issue #55 owns multiplayer verification and cutover.
