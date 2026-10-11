# GASP-08 – UE5 Manny skeleton pilot

Status: **Ready** (defined 2026-10-11). Not started; no implementation, build, or test has run for this task.

## Goal

Decide with evidence whether SurvivalRpg can standardize its humanoid characters on the UE5 Manny skeleton by baking GASP locomotion offline with the official GASP retargeter. The alternative is the current setup: UEFN stays the hidden gameplay mesh, and a visible character follows it through runtime retargeting.

The pilot answers one question: does an offline-baked GASP clip on Manny keep the quality of the same clip shown through today's runtime retargeting, in data and in Motion Matching?

## Why

The project currently runs two humanoid skeleton worlds:

| Skeleton | Users |
| --- | --- |
| UEFN (`/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin`) | GASP locomotion and traversal. `GetMesh()` of the CMC and Mover pawns; it owns GAS montages, notifies, equipment sockets, and corpse physics. |
| UE5 Manny, Lyra copy (`/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin`) | Rift and wildlife AI placeholders (`SKM_Manny_Simple`), and the Sword & Shield pack in `GF_Combat_Core` (attacks, `06_Dodge`, `07_Roll`). |
| UE5 Manny, GASP copy (`/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UE5_Mannequins/Meshes/SK_Mannequin`) | Target of the official `RTG_UEFN_to_UE5_Mannequin` (via `SKM_Manny`) and the optional runtime-retarget follower ([runtime retargeting](gasp-runtime-retargeting.md)). |

Most marketplace characters, armor, and animation packs target UE5 Manny. Keeping UEFN as the hidden gameplay skeleton has three costs:

- **Double retargeting.** Every combat clip is retargeted Manny → UEFN offline and then back to Manny at runtime.
- **Proportion mismatch.** Grips, sockets, and physics live on a hidden body whose proportions differ from the visible character.
- **Runtime cost.** Each character pays for runtime retargeting, which does not scale to groups of humanoid enemies.

The archived native port ([archive](gasp-native-port-archive.md)) failed partly because it reimplemented GASP logic and retargeting. This pilot changes no GASP logic. It runs Epic's retargeter through the engine's batch workflow and copies Epic's databases and graph unchanged except for the skeleton.

## Scope

In scope, in this order:

1. **Phase A – inventory and skeleton contract.**
   - Compare the two `SK_Mannequin` assets: bone names, hierarchy, retarget modes, sockets, virtual bones, curves, and slots.
   - Choose the pilot target skeleton. Prefer the Lyra copy shared with the AI placeholders and the Sword & Shield pack. If the hierarchies match, record whether compatible-skeleton settings or a re-pointed IK rig serves both.
   - Read back the official retargeter's settings (root, pelvis, chains, IK goals, retarget poses) and use them unchanged. Any deviation goes into a pilot copy with a recorded reason.
   - Identify the Manny mirror data table required by the copied Pose Search schema; GASP mirrors with `MDT_UEFN_Mannequin`.
2. **Phase B – offline bake of the pilot set.** Batch-retarget exactly the 144 sequences referenced by these eight CMC Dense databases:
   - `PSD_Dense_Stand_Idles`
   - `PSD_Dense_Stand_TurnInPlace`
   - `PSD_Dense_Stand_Walk_Loops`, `_Starts`, `_Stops`
   - `PSD_Dense_Stand_Run_Loops`, `_Starts`, `_Stops`

   Output goes to a new folder, `/Game/SurvivalRpg/Characters/GASP/Pilot/Manny/`. Use the engine batch operation, wrapped by the project tool `retarget_animation_batch` where it fits. Pivots, sprint, crouch, jumps, lands, and traversal are excluded.
3. **Phase C – offline quality comparison.** For every baked clip, compare against the same source clip evaluated through the runtime retarget path on the same Manny mesh. Metrics are under acceptance criteria.
4. **Phase D – isolated Motion Matching run.**
   - Copies:
     - pilot schema and the eight databases on the Manny skeleton with a Manny mirror table;
     - a pilot AnimBP copied from `ABP_RpgGasp_CMC`, with a chooser limited to the pilot databases;
     - a pilot pawn, PawnData, and development Experience on a pilot copy of `Lvl_RpgGaspCMC`.
   - Run the pilot pawn side by side with the existing UEFN pawn plus runtime follower on the same scripted route: start, walk and run loops, stop, 90° and 180° turn in place.

Out of scope:

- Existing CMC and Mover pawns, Experiences, AnimBPs, choosers, databases, and the original clips.
- Mover; pivots, sprint, crouch, jump, landing, and traversal; a full bake.
- Removing runtime retargeting.
- The Relaxed-set or movement-tuning decisions from the 2026-10-11 CMC review.
- Runtime C++ classes.
- The character and armor pilot: a marketplace character plus one armor set driven by Leader Pose. It is a separate follow-up and needs an asset the user selects.

## Ownership

| Concern | Owner |
| --- | --- |
| Runtime truth | Unchanged: CMC, GAS, equipment, character lifecycle |
| Baked clips, schema and database copies, pilot AnimBP and chooser, pilot PawnData, Experience, and map | Designer content, authored through the Unreal MCP workflow |
| Native code | None expected. Editor-only tooling only for a confirmed MCP capability gap |
| Verification | Asset contract readback and measurements; no C++ tests for visual leaves |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Preservation | All 144 baked clips keep length, root-motion enablement, curve names and case (including `Phase` and foot contacts), notifies (including Foley), and sync markers, with record counts identical to the source |
| Root speed | Per-loop root speed is measured for source and bake. The ratio is consistent across loops (within ±2 %), and the resulting authored Manny speeds are recorded |
| Foot contact | Horizontal foot-bone speed during contact is reported as median and 95th percentile per clip. Baked clips are no worse than the runtime-retarget output of the same clip on the same mesh |
| Pose Search | The pilot databases build without errors. On the scripted route, the Pose Search debugger shows selections and costs comparable to the UEFN path; differences are reported |
| Performance | Game-thread and animation-thread cost of the pilot pawn versus UEFN plus runtime follower are measured and reported |
| User check | Side-by-side visual acceptance by the user using the repro below |
| Rollback | Deleting the pilot folder and the development Experience restores the prior state. A preservation audit shows existing assets and saves unchanged |

## Exit decision

- **Pass:** propose bounded follow-ups:
  - bake the full CMC closure (Dense, Sparse, traversal, choosers);
  - switch `GetMesh()` to the Manny skeleton and retire the runtime follower for gameplay characters;
  - run the character and armor pilot.
- **Fail:** keep UEFN plus runtime retargeting, and record the failing metrics and reasons here.

## Verification and evidence

Build and test through `Build/Tools/Unreal/ue.py`. Verify the NetworkPrediction and Mover overrides before building. Raw evidence stays under `Saved/`. This report and the PR carry the summary, the commands, and the failed attempts. Update only the `GASP-08` roadmap row and replace the handoff's current state.

## Repro (after implementation)

Open the pilot map without a PIE Experience override and start PIE. Walk and run in all directions, stop, and turn in place 90° and 180°. Compare the pilot pawn with the existing UEFN pawn and its runtime follower: feet, stride, and turn quality.
