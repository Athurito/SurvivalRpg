# GASP grounded Hurdle integration

This slice extends the existing CMC traversal Experience with original grounded Hurdle selection and animation. Space still uses the existing contextual traversal Ability. The UEFN gameplay mesh, source montage tempo, approved camera damping, existing Mantle/Vault content and map presentation remain in place. Additional visible skeletons and Mover remain separate steps.

## Source content and ownership

GASP action 1 is Hurdle: front and back ledges, a detected back floor, obstacle depth up to 59 cm and a back ledge at least 50 cm above that floor. The selected grounded rows allow obstacle heights up to 125 cm. Vault remains action 2 with no nearby back floor; Mantle remains action 3. The authority independently validates physical geometry and walkable support.

Ten Neutral montages are copied from `/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Traversal/Hurdle` to `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/Animations/Hurdle`, keeping their filenames. Each name has prefix `AM_M_Neutral_Traversal_Hurdle_1_0_`:

| Filename suffix | Depth (cm) | Speed (cm/s) | Pose-search entry (s) | Source blend-out begins (s) |
| --- | --- | --- | --- | --- |
| stand_F_V2_Lfoot | 0–25 | 0–100 | 0–0.07 | 1.017674, with movement input |
| walk_F_Lfoot | 0–25 | 100–250 | 0–0.73 | 1.836459 |
| walk_F_Rfoot | 0–25 | 100–250 | 0–0.84 | 2.000074 |
| run_F_Lfoot | 0–25 | 250+ | 0–0.49 | 1.122898 |
| run_F_Rfoot | 0–25 | 250+ | 0–0.48 | 1.065964 |
| stand_F_V2_Rfoot | 25–60 | 0–100 | 0–0.12 | 0.959538, with movement input |
| walk_F_V2_Lfoot | 25–60 | 100–250 | 0–0.50 | 1.599901 |
| walk_F_V2_Rfoot | 25–60 | 100–250 | 0–0.50 | 1.536383 |
| run_F_V2_Lfoot | 25–60 | 25+ | 0–0.66 | 1.395191 |
| run_F_V2_Rfoot | 25–60 | 25+ | 0–0.67 | 1.432122 |

The wider running rows intentionally preserve the source's 25 cm/s lower bound. Their overlap with walking rows is resolved by the original PoseSearch chooser. The outer chooser still limits physical depth to 59 cm. Native unbounded speed rows use the existing finite 100,000 cm/s convention.

The owned chooser and traversal PoseSearch database select these copies. Sequence, skeleton, root motion, slots, complete notify settings and warp modifiers remain source-authored; only the BranchIn database reference points to the existing owned database. The source/foundation packages remain unchanged. `AllowedHurdleAnimations` records the ten eligibility rows on the existing query Blueprint; it defaults to empty. New depth and conditional handoff fields preserve the previous Mantle/Vault defaults.

## Warping and handoff

FrontLedge retains the source hand offset and facing alignment. BackLedge is installed only for montages that use it. BackFloor uses the original `Distance_From_Ledge` curve through Unreal's AnimationWarping library, including animation-segment curves. Its horizontal position is the back edge plus its outward normal multiplied by the absolute difference between the curve values at the first BackLedge and BackFloor window ends. Without a BackLedge window, that first distance remains zero. BackFloor height comes from the queried floor; targets account for the existing gameplay mesh/capsule feet offset.

The wider right-foot walking montage contains an original 3.277 ms overlap between BackLedge and BackFloor warp windows. This authored overlap is retained. Front-facing alignment finishes before the translation-only landing phase; the view direction remains independent.

Original montage notifies own animation handoff. Walking and running force their source blend-out; standing waits for movement input inside its conditional window or reaches the natural montage end. Hurdle returns to ordinary CMC walking on the validated floor behind the obstacle. Native authority, collision/rotation ownership, cancellation and support-lifecycle checks extend the existing traversal seams.

Activation and ongoing clearance checks both place their check capsule on the measured support plane. The source animation's small vertical root residual is separately bounded against that plane; it must not lower the clearance capsule into an otherwise valid floor. The support component must remain registered, walkable, query-enabled and at its accepted transform throughout traversal.

Environmental failures and the existing traversal timeout use GAS's dedicated cancellation path. In this engine version, marking an ordinary `EndAbility` as locally cancelled still replicates a normal end to the other peer. Dedicated cancellation preserves the reason on the owning client and server, including synchronous montage interruption callbacks. Internal safety failures retain their forced cleanup behavior.

## Test map

Open `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle` with its saved Experience. Three prepared original GASP cubes stand in the free eastern area, beyond the existing Mantle platforms: 100 cm high, 400 cm wide and 20, 40 or 59 cm deep. They have the tag `Rpg.TraversalTest.Hurdle` and front faces at X=1500, with lane centers Y=-1200, -400 and 400. Approach their front faces from the west and press Space. Test standing, walking, running and angled approaches, with both the listen-server host and a remote client.

The floor is at the same height before and after these barriers. All 28 previous actors, saved WorldSettings and PlayerStarts are preserved. The original GASP grid materials and LevelVisuals lighting/exposure remain the map's presentation reference.

## Validation

Evidence is collected under `Saved/GaspHurdle20260912`. The source manifest records exact chooser rows, original warp flags, handoff conditions and graph links. The asset audit checks source/copy parity, unchanged query and chooser logic, exact BranchIn membership, preserved database rows and project-local dependencies. Preservation hashes cover all 7,600 pre-existing Content/Plugins files and seven save files.

The final SurvivalRpgEditor Win64 Development build passed on Unreal 5.8.2. All 37 tests passed in one 332.92-second run: eleven Hurdle cases and the 26 existing Mantle, Vault, CMC, camera and combat regressions. Tests use real mapped input in isolated listen-server PIE worlds. Hurdle-specific coverage includes standing natural and conditional exits, analog walking, running, both approach angles, host, maximum depth, blocked authority landing, explicit cancellation and support loss during traversal. Existing lifecycle tests cover late join, latency, death/respawn and collider changes. No packaged-build or dedicated-server validation is implied.

The final asset audit passed for 35 roots and 1,616 dependency packages, all ten source/copy montage pairs, unchanged executable query/chooser logic, all 28 selected owned BranchIn records and 24 unaffected database rows. Preservation hashes checked all 7,600 pre-existing Content/Plugins files: only the query, owned chooser/database and test map changed. The other 7,596 files and all seven saves remained unchanged. The ten Hurdle montage packages are the only new content.

`validation-summary.json`, `all-final-results.json`, `build-final.log`, `final-editor.log`, `hurdle-asset-audit.json` and `preservation-final.json` retain final evidence in `Saved/GaspHurdle20260912`. Earlier diagnostic failures remain separately recorded. Existing voice-interface, temporary-world NetGUID and respawn-widget warnings remain visible in test results.
