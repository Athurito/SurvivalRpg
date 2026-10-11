# GASP-08 – UE5 Manny skeleton pilot

Status: **PR open** ([#228](https://github.com/Athurito/SurvivalRpg/pull/228), draft; implemented 2026-10-11 on `claude/gasp-08-manny-skeleton-pilot`). Phases A–D are done, and the automated evidence supports **Pass**. The user's side-by-side visual check is still open, and the exit decision waits for it. Results, failed attempts, and limits are under [Results](#results); the asset manifest is [gasp-08-manny-pilot.json](assets/gasp-08-manny-pilot.json).

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
   - Inventory gameplay montages by skeleton. As of 2026-10-11:
     - the sword attack (`AM_BasicSword_Attack01`), the pickaxe harvest swing, skinning, death, and hit-reaction montages reference the Lyra `SK_Mannequin`;
     - the block montages reference UEFN.
     - No skeleton declares a compatible skeleton.
     - Record how the Manny-skeleton montages resolve on the UEFN pawn today, and what a Manny gameplay mesh would change for them.
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

## Coordination with open issues

Checked on 2026-10-11 against all open issues and pull requests.

| Item | Overlap | Rule while `GASP-08` is open |
| --- | --- | --- |
| [#124](https://github.com/Athurito/SurvivalRpg/issues/124) load-dependent dodge | Its roll and sidestep sources (`07_Roll`, `06_Dodge`) are Lyra-Manny clips. Retargeting them to UEFN now would be throwaway work if the pilot passes. | Skeleton-independent parts may proceed: grant, input, cost, blockers, tier snapshot contract, tests, and a placeholder montage. Author the final tier montages after the exit decision. Medium is a shorter roll ([game vision](game-vision.md)). |
| [#46](https://github.com/Athurito/SurvivalRpg/issues/46) footstep surface variants | Foley notifies on the GASP clips are copied by the bake. | Put surface selection in the Foley notify Blueprint or the audio bank, which survives a bake. Do not edit notifies on individual source clips until the exit decision. |
| [#26](https://github.com/Athurito/SurvivalRpg/issues/26) harvesting loop | New cast and harvest montages. | Author them on the Lyra `SK_Mannequin`, like the existing harvest swing. Do not retarget them to UEFN before the exit decision. |
| [#31](https://github.com/Athurito/SurvivalRpg/issues/31) revive and bleedout validation | Validation runs on the current pawns. | No conflict. Keep its test sessions off the pilot map and pilot Experience. |
| [#169](https://github.com/Athurito/SurvivalRpg/issues/169) skill evaluations | None at runtime. | No conflict. |

Issues #25, #58, #84, #86, #87, #88 and open PR #219 (UI, runes, architecture tracking) do not touch animation, skeletons, or GASP assets.

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

## Results

All numbers come from runs in this task on UE 5.8.3 in the editor of the producing checkout. Raw evidence is in `Saved/Gasp08` and `Saved/Profiling/CSV` of that checkout and is not versioned. The [manifest](assets/gasp-08-manny-pilot.json) lists every mapping, metric, and target hash.

### Acceptance status

| Area | Result |
| --- | --- |
| Preservation | **Met.** All 144 bakes match their source in length, frame count, root-motion flags (enabled, forced root lock, reference-pose lock), rate scale, curve names and case (`contact_l`, `contact_r`, `enable_warping`, `movedata_speed`, `phase`, …), curve keys and values, and all 1,295 notifies, including Foley. Neither source nor bake has sync markers. |
| Root speed | **Met.** The bake-to-source ratio is exactly 1.0000 for all 24 loops and all 134 clips that move more than 20 cm. The authored Manny speeds therefore equal the UEFN speeds, for example walk F 198.3, walk B 148.8, run F 493.3, run B 296.1, and run LL 345.1 cm/s. |
| Foot contact | **Met in aggregate; strict per-clip reading misses by ≤4.2 cm/s in 9 clips.** At identical 60 Hz sample times, horizontal foot speed during contact has a median of per-clip medians of 10.50 (source), 10.78 (bake), and 10.37 cm/s (runtime follower). The median of per-clip p95 values is 99.69, 99.58, and 99.71 cm/s. Per clip, the bake's p95 exceeds the runtime's by more than 1 cm/s in 9 clips (maximum +4.2 cm/s, +2.3 %, `M_Neutral_Run_Reface_Start_F_L_180`) and is lower by more than 1 cm/s in 4 clips. For the ball bone, the bake is worse in 5 clips and better in 9. |
| Pose Search | **Met; differences explained.** The pilot databases build and serve searches, and no Pose Search error occurs in the route sessions. Against a UEFN control with the same eight databases, the best match agrees in 98.9 % of route frames. Against the existing UEFN path it agrees in 90.6 %; the difference comes from the reduced database and normalization set, not from the bake (see Phase D). |
| Performance | **Measured.** The Manny pawn costs 0.31 ms game-thread animation and 0.50 ms animation-worker time (median), against 0.47 and 0.69–0.71 ms for UEFN plus runtime follower: one skeletal mesh tick instead of two. |
| User check | **Open.** It needs the side-by-side visual check described under [Repro](#repro). |
| Rollback | **Met.** No asset outside the pilot folders references any of the 208 pilot packages. The only changed existing asset is the Lyra `SK_Mannequin` (two added blend profiles, approved by the user). The existing save file is unchanged, and so are the source maps. |

### Phase A – inventory and skeleton contract

- **Skeleton choice:** the Lyra `SK_Mannequin`. The Lyra and GASP copies have the same 161 bones in the same order with the same parents. Their local and world reference poses match exactly (maximum difference 0.0 cm), and so do the translation retarget modes (145 AnimationScaled, 6 Skeleton, 1 Animation).
  - The Lyra copy additionally has six gameplay sockets (`HandGrip_R/L` and the staff and axe sockets), the `Arms` slot, and Lyra curve metadata. It has five blend profiles; the GASP copy has thirteen.
  - The Lyra copy has 1,074 referencers (Sword & Shield pack, AI placeholders, gameplay montages); the GASP copy has 63, mostly its own rig and pose assets.
- **Bake mesh:** the Lyra folder has only `SKM_Manny_Simple` (89 bones, without corrective, `weapon_l` and `weapon_r` bones). The bake targets it; corrective bones in a later full mesh need their post-process AnimBP.
- **Both skeletons with one rig:** `IK_UE5_Mannequin_Retarget` and `IK_UEFN_Mannequin` are compatible with both Manny meshes, so the official retargeter drives either target unchanged. A compatible-skeleton setting is not needed at runtime; it only affects editor filtering.
- **Official retargeter readback:**
  - The source is `IK_UEFN_Mannequin` and the target `IK_UE5_Mannequin_Retarget`.
  - The op stack is Pelvis Motion (global Z offset −1 cm), Retarget FK Chains (Root chain GloballyScaled, clavicles OneToOne), Blend to Source (legs 1.0 with pelvis offset), Offset Goals, Speed Plant IK Goals (no speed curves configured), Run IK Rig, Pole Vector Alignment (disabled) and Remap Curves (copy all).
  - The target retarget pose raises the root by 0.3 cm. There is no root-motion or pin-bone op.
- **Mirror table:** the copied schemas have no mirror table, and no pilot database entry is mirrored. The AnimBP's Blend Stack node is the only reference to `MDT_UEFN_Mannequin`. The pilot adds `MDT_RpgGasp08_Manny` with the same `_l`/`_r` rules: 161 rows, 146 of them swapped pairs, identical to the UEFN table on every shared bone. The engine's row generation is not reachable from Python, so the rows were written from those rules.
- **Proportions:** the pelvis is at 93.12 cm (UEFN) and 95.90 cm (Manny), the head at 152.0 and 162.6 cm.
- **Montages by skeleton:** 145 montages in total.
  - 128 use UEFN: GASP traversal plus the five `GF_Combat_Core` block montages.
  - 11 use the Lyra `SK_Mannequin`: sword attack and combo, the four Sword & Shield block montages, death, hit reaction, harvest swing, skinning, and pistol fire.
  - 6 use the MoverExamples copy.
- **How Manny montages play on the UEFN pawn today:** `UAnimInstance::Montage_PlayInternal` does not check skeleton compatibility. Evaluation remaps bones by name through `FSkeletonRemappingRegistry`, using the target skeleton's translation retarget modes. UEFN-only bones (`attach`, `props_root`, `prop_01`, `poi`, two virtual bones) keep their pose, and Manny-only bones are dropped.
- **What a Manny gameplay mesh would change:** the 11 Lyra montages would play natively. The UEFN block and traversal montages would be remapped in the other direction or would need their own bake.

### Phase B – offline bake

`retarget_animation_batch` builds its own automatic rigs and cannot run the official retargeter. The bake therefore calls the engine's `IKRetargetBatchOperation.run_batch_retarget` directly, from `SKM_UEFN_Mannequin` to the Lyra `SKM_Manny_Simple`. It does not include referenced assets and never overwrites. The 144 clips take about 6 s and occupy 157 MB, against 193 MB for the sources.

Failed attempts, kept as evidence:

1. **Probe bake with the official retargeter (one clip, not saved):** the IK bones stay in the reference pose. In the source they coincide exactly with `foot_*` and `hand_*` (verified in the reference pose and two animation frames). The copied AnimBP reads those bones in Leg IK and Foot Placement.
2. **First full bake (official retargeter plus a Pin Bones op):** the bake evaluates sources with root motion in the pose. The GloballyScaled root and the scaled pelvis therefore drift away from the blend-to-source feet by 3.3 % per metre. In `Run_Loop_F`, the pelvis ends 40 cm ahead and planted feet drag up to 27 cm. Foot-contact p95 rose to 252 cm/s (source 140) and ball p95 to 190 cm/s (source 57). The root-speed ratio was a constant 1.0331.

The final pilot copy `RTG_UEFN_to_UE5_Mannequin_PilotBake` keeps every official setting and differs in three recorded points (T3D diff checked):

| Op | Change | Reason |
| --- | --- | --- |
| Pin IK Bones (added) | `ik_foot_root`/`ik_hand_root` from `root`, `ik_foot_l/r` from `foot_l/r`, `ik_hand_l/r` from `hand_l/r`, `ik_hand_gun` from `hand_r`; target skeleton, global position and rotation | Keeps the IK bones on the FK bones as in the source |
| Retarget FK Chains, Root chain | GloballyScaled → Absolute | Removes the root drift of root-motion-in-pose evaluation |
| Pelvis Motion | BlendToAbsoluteOffset (1,1,0) | Removes the horizontal pelvis drift; vertical motion stays scaled as at runtime |

Absolute horizontal pelvis motion keeps Manny's own reference pelvis position (Y +2.28 cm; UEFN −0.71 cm). The baked pelvis therefore sits a constant 3.09 cm (mean) further forward over the feet than the runtime follower's, which scales the UEFN pelvis position.

### Phase C – offline and runtime comparison

- **Runtime reference:** `BP_RpgGasp_MantleCharacter` in `Lvl_RpgGaspMantle` applied the transient-use profile `DA_RpgGasp08_RuntimeRetarget_Manny` (Lyra `SKM_Manny_Simple`, official retargeter, `ABP_RpgGasp_RuntimeRetarget`). The gameplay mesh then played each of the 144 source clips in single-node mode at 60 FPS while a post-tick callback recorded the follower: 29,552 samples.
- **Alignment check:** the captured gameplay pose, transformed by the clip's root motion, matches the offline evaluation within 0.16 cm.
- **Metric:** horizontal speed of the foot bone (and the ball bone) in root-motion world space. Contact means the source `contact_l`/`contact_r` curve is at least 0.5 at both ends of a sample interval. Source, bake, and runtime are evaluated at the same playback times.
- **Earlier contact definition (≥0.99), discarded:** at 30 Hz it left only 2–3 contact frames per run loop.

### Phase D – isolated Motion Matching run

- **New assets:**
  - Manny copies of the three schemas, the eight databases, and an eight-database normalization set; the mirror table.
  - Baked copies of the AimOffset and run-lean blendspaces with their 18 additive poses (additive type and reference pose preserved).
  - `ABP_RpgGasp08_CMC_Manny`, with references remapped to the Manny skeleton, the pilot blend profiles, mirror table, blendspaces, and choosers, and to its own class.
- **Blend profiles:** the copied AnimBP needs `FastFeet+Root_Weight` and `FastHead_Weight`, which exist only on the UEFN skeleton. With the user's approval they were copied 1:1 to the Lyra `SK_Mannequin` (18 and 59 entries, same mode).
- **Database rows:** in `CHT_PoseSearchDatabases_Dense_Manny`, every row that points at a database outside the pilot is disabled: pivots, spin transitions, from-traversal, lands, sprint, crouch, and in-air. Exactly the eight pilot databases stay active.
- **Experimental state machine path:** it is off (`UseExperimentalStateMachine=False`, no setter). Its chooser was copied only to give it the pilot class as context; its UEFN animations stay referenced. `StrafeOffsetCurveContainer`, a curve-only UEFN sequence, also stays referenced.
- **Chooser context pin:** the Evaluate Chooser node names its context pin after the context class. After the class remap the old, linked pin was orphaned on reload. Self was relinked to the new pin with the graph tools.
- **Variants:** each has its own pawn (a child of `BP_RpgGasp_CMC`), PawnData, development Experience (`/Game/SurvivalRpg/System/Experiences/Gasp08Pilot`, the scanned Experience root), and map:
  - **Manny:** baked clips with its own Motion Matching on `SKM_Manny_Simple`.
  - **UEFN follower:** the existing UEFN AnimBP with the runtime Manny follower.
  - **UEFN control:** the UEFN AnimBP copy limited to the same eight databases with an eight-database normalization set, plus the follower.
- **Maps:** at the user's request, the maps start from the approved GASP presentation of `Lvl_RpgGaspMantle` (GASP floor and grid, LevelVisuals lighting). The traversal obstacles and two of the three PlayerStarts are removed, so every variant starts from the same place on open floor.
- **Scripted route:** after a 2 s settle: forward 3 s, stop, right 2 s, stop, back 2 s, stop, left 2 s, stop, then control yaw +90° and +180°. Input is synthetic through `RpgEditorPlaytestTools`, at 60 FPS. Per frame the run records position, speed, the AnimBP's `CurrentSelectedAnim` and `CurrentSelectedDatabase`, and a fresh `PoseSearchLibrary.MotionMatch` over the AnimBP's `ValidDatabases` with the `PoseHistory` tag. That search has no continuing-pose bias, so its cost approximates the node's cost.

| Comparison | Best-match agreement | Notes |
| --- | --- | --- |
| Manny vs UEFN control (same 8 databases) | 98.9 % of 1,590 frames | Final positions differ by at most 0.6 cm. Per-step median costs are within 0.00–0.05, except after the right stop (0.392 vs 0.286) and the back stop (0.072 vs 0.025). |
| Manny vs existing UEFN path | 90.6 % | After the right stop the existing path plays `Stand_Turn_090_L`; the pilot keeps the stop. The following backward start then refaces from 90° instead of 180°. Absolute costs are lower on the existing path because `PSN_Dense_All` normalizes over all Dense databases. |

Performance during the same route (editor PIE, one pawn, 60 FPS, 1,561 frames, median per frame):

| Variant | Game-thread animation | Animation worker | Skeletal mesh ticks |
| --- | --- | --- | --- |
| Manny bake, own Motion Matching | 0.309 ms | 0.497 ms | 1 |
| UEFN control + runtime follower | 0.474 ms | 0.685 ms | 2 |
| Existing UEFN + runtime follower | 0.470 ms | 0.708 ms | 2 |

### Tooling

Two confirmed gaps got editor-only tools on `AnimationAssetTools`, each with a native helper in `URpgAnimationAssetTools`:

- `copy_skeleton_blend_profiles`: neither the engine toolsets nor Python can author skeleton blend profiles.
- `set_chooser_disabled_rows`: `DisabledRows` is not reachable from Python, and `configure_chooser` rejects choosers with embedded tables.

Both refuse partial or overwriting edits and never save. `SurvivalRpgEditor` Win64 Development built after each addition (23 s and 21 s, 0 errors, 0 warnings).

### Limits

- **Pending:** the visual check is the user's, and the exit decision waits for it.
- **Bake mesh:** `SKM_Manny_Simple` has no corrective bones. A production Manny or marketplace mesh needs its post-process AnimBP.
- **Coverage:** the pilot covers only standing idles, turn in place, and walk and run starts, loops, and stops. Pivots, spin transitions, sprint, crouch, jumps, lands, and traversal have no pilot databases; when the chooser would select them, the pilot plays without that set.
- **Normalization:** the pilot normalizes over eight databases, unlike production. The UEFN control isolates that effect.
- **Costs:** they come from a fresh `MotionMatch` call, not from the node's internal search with continuing-pose bias. The Pose Search Rewind Debugger view is part of the user check.
- **Turn in place on the route:** the control-yaw snaps (+90°, +180°) produced no turn-in-place animation in any variant. Turn in place was observed after the backward stop (`Stand_Turn_180_R`) in all three variants, and after the right stop only on the existing path.
- **Pelvis:** the baked pelvis sits a constant 3.09 cm further forward than the runtime follower's; feet are unaffected.
- **Foot contact:** in a strict per-clip reading, 9 of 144 clips have a p95 up to 4.2 cm/s (2.3 %) above the runtime follower; 4 are better.
- **Performance:** it was measured in editor PIE with one pawn, not cooked or packaged and not with groups of characters.
- **Untested:** multiplayer, montages, equipment sockets, death, and ragdoll on a Manny gameplay mesh were out of scope and are untested.
- **Editor side effects:**
  - On load, the editor marked three existing `PSD_SM_CMC_*` databases dirty without any edit from this task. They were never saved, and their autosave restore data was set aside as `Saved/Autosaves/PackageRestoreData.gasp08-discarded.json`.
  - Two editor restarts were forced. One was for the new tools; one was for a blocking compile-error dialog that the orphaned chooser pin caused before it was fixed.

### Rollback

Delete `/Game/SurvivalRpg/Characters/GASP/Pilot` and `/Game/SurvivalRpg/System/Experiences/Gasp08Pilot`, then revert `Content/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin.uasset`. The two tools are generic and may stay.

### Proposed exit decision

**Pass, pending the user check.** The bake keeps every authored contract, needs three recorded retargeter changes, and matches the runtime follower's foot contact in aggregate. Motion Matching on Manny selects the same animations as UEFN with the same database set, at about two thirds of the animation cost. If the user check passes, the follow-ups below apply. The next one should first bake the full CMC closure, with the missing databases and the production normalization set.

## Repro

1. Open `/Game/SurvivalRpg/Characters/GASP/Pilot/Manny/Maps/Lvl_RpgGasp08_Manny` without a PIE Experience override and start PIE. Walk and run in all directions with WASD, stop, and turn the camera for turn in place.
2. Repeat on `Lvl_RpgGasp08_UefnFollower`, the existing UEFN pawn with its runtime Manny follower on the same mesh. `Lvl_RpgGasp08_UefnControl` shows the UEFN path with the same eight databases.
3. Compare feet, stride, stops, and turns. Optionally set `a.AnimNode.MotionMatching.DebugDrawInfo 1` or open the Rewind Debugger's Pose Search view.

Expected: the Manny pilot moves like the follower variants, without visible foot sliding or broken legs. Sprint, crouch, jumps, and direction reversals that need pivots are outside the pilot set.
