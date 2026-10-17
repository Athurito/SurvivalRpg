# GASP-14 – Full CMC bake to Manny

Status: **PR open** ([#251](https://github.com/Athurito/SurvivalRpg/pull/251)), implemented 2026-10-17 on `claude/gasp-14-full-cmc-bake`; the user check is pending. Defined 2026-10-17 ([#250](https://github.com/Athurito/SurvivalRpg/pull/250)): after parking `GASP-STAB-05`, the user asked to continue with the roadmap and chose `SKM_Manny_Simple` as the bake mesh. Results are under [Results](#results).

## Goal

Every animation the CMC pawn plays in locomotion and traversal exists on the Lyra UE5 Manny skeleton as a project-owned offline bake, with Manny copies of the Motion Matching data and the CMC AnimBP. A development pawn runs the full set on `SKM_Manny_Simple` side by side with today's UEFN pawn. The gameplay pawns stay on UEFN; switching `GetMesh()` is `GASP-15`.

## Why

`GASP-08` passed on 2026-10-11: offline-baked Dense clips on Manny kept every authored contract and matched the runtime follower ([pilot](gasp-manny-skeleton-pilot.md)). Since `GASP-10` the CMC plays the Relaxed set, so the full bake covers that set instead of Dense and Sparse. Manny is the skeleton of the Sword & Shield pack, the AI placeholders and most marketplace characters, and one skeleton removes the double retargeting and the runtime follower cost (`GASP-15`, `GASP-16`).

## Current state (checked 2026-10-17 on `master` `cf40597f`)

Asset registry, recursive from the CMC chooser and traversal assets:

| Area | Assets |
| --- | --- |
| Locomotion | `CMC/RPG/Choosers/CHT_RpgGasp_CMC_PoseSearchDatabases` references 71 shared Relaxed databases (`Shared/.../MotionMatchingData/Databases/Relaxed`), which use 460 UEFN sequences (732 MB of source packages). Schemas `PSS_Relaxed_*`, normalization set `PSN_Relaxed_All`. |
| Traversal | `CMC/RPG/Traversal/Choosers/CHT_RpgGasp_MantleMontages` and `CMC/RPG/Traversal/Databases/PSD_RpgGasp_Traversal`: 34 UEFN montages with 32 further sequences (47 MB), shared with the Mover's traversal choosers. |
| AnimBP inputs | `ABP_RpgGasp_CMC` also uses `BS_Neutral_AO_Stand`, `BS1D_Additive_Lean_Run` with their additive poses, `MDT_UEFN_Mannequin` and the UEFN post-process AnimBP. |
| Pilot | `GASP-08` assets under `Characters/GASP/Pilot`: the bake retargeter copy `RTG_UEFN_to_UE5_Mannequin_PilotBake` (three recorded changes), `MDT_RpgGasp08_Manny`, two blend profiles on the Lyra `SK_Mannequin`, and the pilot pawn, Experience and maps. |
| Block | The block layer's data children (`GF_Combat_Core`) target UEFN; the shield's block set is retargeted from Manny sources. |

The pilot baked 144 clips into 157 MB. The full bake is expected near 650 MB of new LFS content.

## Scope

1. **Phase A – inventory.** Export the exact bake set: every sequence, blend space and montage reachable from the CMC chooser's 71 databases, the CMC traversal chooser and database, and the AnimBP's blend spaces. Record per source: length, frame count, root-motion flags, curve names, notify count and classes, sync markers, and the LFS hash. Confirm that no row of the CMC chooser reaches a database outside the set.
2. **Phase B – bake.**
   - Copy the pilot bake retargeter and mirror table into a production folder, `/Game/SurvivalRpg/Characters/GASP/Manny/`, unchanged. The pilot stays until `GASP-15` decides its removal.
   - Bake the set with the engine's batch retarget from `SKM_UEFN_Mannequin` to the Lyra `SKM_Manny_Simple`, as in the pilot: no overwrite, references remapped inside the output only.
   - Montages keep their sections, slots, notifies and blend settings and point at the baked sequences.
3. **Phase C – Manny copies.**
   - Schemas, the 71 databases, `PSN_Relaxed_All`, the CMC chooser, the traversal chooser and database, the AimOffset and lean blend spaces with their poses.
   - `ABP_RpgGasp_CMC_Manny`, copied from `ABP_RpgGasp_CMC` with references remapped to the Manny copies and the Manny mirror table. It keeps every `GASP-09` to `GASP-13` behavior.
4. **Phase D – development pawn and checks.**
   - A development pawn (a child of `BP_RpgGasp_CMC` on the GASP-look Manny mesh with the Manny AnimBP), its PawnData, a development Experience and a map copied from the GASP presentation, as in the pilot.
   - A scripted route side by side with the UEFN pawn: idle, walk and run starts, loops, stops and pivots in all four directions, spins, sprint, crouch, jump and land, turn in place, mantle, vault and hurdle.
   - A 2-player listen-server probe on the development map: owner, server and proxy select databases from the Manny chooser; a late joiner as well.

## Out of scope

- Switching `GetMesh()` of the CMC or Mover pawns, sockets, physics asset, ragdoll and gameplay montages (`GASP-15`).
- The Mover, the Mover ragdoll variant and the shared UEFN databases; they stay unchanged.
- The block layer on Manny: the development pawn has no Manny block data child, so block shows no block layer there (`GASP-15`).
- The Dense, Sparse and ExtremeSparse sets, the smart-object bench and climb animations, which the CMC does not play.
- Removing the pilot or runtime retargeting.
- Runtime C++ classes.

## Decisions

- 2026-10-17 (user): bake mesh `SKM_Manny_Simple` (Lyra copy), as in the pilot; a later character with corrective bones gets its own post-process AnimBP or bake.
- 2026-10-17 (user): `GASP-STAB-05` is parked (draft [#249](https://github.com/Athurito/SurvivalRpg/pull/249)); the roadmap continues with `GASP-14`.
- 2026-10-17 (user): GASP's traversal data reads the UEFN-only bone `attach`. The user first approved adding it to the Lyra `SK_Mannequin`. When the late-join test showed that this changes the skeleton GUID for all 1,813 assets on `SK_Mannequin` (see [Phase D](#phase-d--development-pawn-and-checks)), the user chose to carry the attach motion on Lyra's existing `interaction` bone instead.

## Ownership

| Concern | Owner |
| --- | --- |
| Runtime truth | Unchanged: CMC, GAS, equipment, character lifecycle |
| Baked clips, montages, Motion Matching copies, Manny AnimBP, development pawn, PawnData, Experience and map | Designer content through the Unreal MCP workflow; a scripted bake through the engine batch operation as in the pilot |
| Native code | None expected; editor-only tooling only for a confirmed MCP capability gap |
| Verification | Preservation audit and measurements, Pose Search build, route and network probes; no C++ tests per visual leaf |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Preservation | Every baked clip keeps length, frame count, root-motion flags, rate scale, curve names, case, keys and values, notifies (classes, times, Foley), and sync markers, with counts identical to the source. Every montage keeps sections, slots and notifies. |
| Root speed | The bake-to-source root speed ratio is 1.000 ± 0.002 for every clip that moves more than 20 cm. |
| Foot contact | In aggregate (median of per-clip medians and of per-clip p95), contact foot speed stays within 2 % of the source; per-clip outliers are listed. |
| Pose Search | All Manny databases build without errors. On the route, the Manny pawn's selected database matches the UEFN pawn's in at least 95 % of frames; every difference is explained. |
| Traversal | Mantle, vault and hurdle complete on the Manny development pawn with the same montage choice as on UEFN. |
| Network | Owner, listen server and proxy (and a late joiner) select databases from the Manny chooser on the development map. |
| Performance | Game-thread and animation-worker cost of the Manny pawn against the UEFN pawn, measured on the route. |
| Unchanged | No existing asset changes except as recorded; the shared UEFN databases and Mover assets stay byte-identical. |
| Size | The added LFS volume is reported. |
| User check | The user compares the Manny development pawn with the UEFN pawn on the development map. |

## Verification

Build and test through `Build/Tools/Unreal/ue.py`; regressions as in `GASP-13` (CMC, traversal, block, Mover, network). Raw evidence goes to `Saved/Gasp14`; this report, the PR and the manifest `docs/assets/gasp-14-manny-full-bake.json` carry the summary.

## Repro (after implementation)

1. Open `/Game/SurvivalRpg/Characters/GASP/Manny/Maps/Lvl_RpgGasp14_Manny` without a PIE Experience override and start PIE. The map's Experience `RpgGasp14MannyExperience` spawns the Manny development pawn.
2. Walk, run, sprint and crouch in all directions, stop, jump, turn the camera for turn in place, and mantle, vault and hurdle over the obstacles. Right after the traversal data changed, the first traversal can fall back to a jump once while the index rebuilds (UEFN behaves the same); press again.
3. Compare with the UEFN pawn on `Lvl_RpgGaspMantle`, the same map with the UEFN traversal pawn: feet, stride, stops, turns and traversal should match. Block shows no block layer on the Manny pawn.
4. Multiplayer: set PIE to a listen server with two clients and repeat a few starts, stops and a mantle; the other windows should show the same motion.

## Results

All numbers come from runs in this task on UE 5.8.3 in the editor of the producing checkout (worktree `musing-ishizaka-7bac20`). Raw evidence is in `Saved/Gasp14`, `Saved/Profiling/CSV` and `Saved/ToolRuns` of that checkout and is not versioned. The [manifest](assets/gasp-14-manny-full-bake.json) lists the mappings, copies, metrics and target hashes.

### Acceptance status

| Area | Result |
| --- | --- |
| Preservation | **Met.** All 556 pairs (522 sequences and blend spaces, 34 montages) keep length, frame count, root-motion flags, rate scale, additive settings, curve names, key counts and values, notifies (class and time), sync markers, the montages' slot segments (pointing at the baked sequences) and blend times, and the blend-space samples. Two contact curves differ in key count: the bake dropped a duplicate source key at the same time (`M_Relaxed_Run_Turn_R_180_Lfoot` `contact_r`, `M_Relaxed_Walk_Stop_B_Lfoot` `contact_l`). |
| Root speed | **Met.** The bake-to-source ratio is exactly 1.00000 for all 477 clips that move more than 20 cm. |
| Foot contact | **Met.** Over 405 clips with contact, the median of per-clip medians is 12.89 cm/s for source and bake (0.0 %), and the median of per-clip p95 values is 94.53 and 95.76 cm/s (+1.3 %). Per clip, the bake's p95 is more than 1 cm/s higher in 12 clips (at most +5.6 cm/s, +5.5 %, `M_Relaxed_Run_Reface_Start_B_090_L_Rfoot`) and more than 1 cm/s lower in 34. |
| Pose Search | **Met.** All 77 Manny databases, the traversal database and the three databases embedded in the traversal chooser build. On the scripted route, the Manny pawn selects the same database as the UEFN pawn in 97.7 % of frames: 99.9 % outside traversal and 94.3 % in the traversal segments, where the vault starts at a different montage time (below). Two UEFN runs agree in 99.9 %, and the two final Manny runs in 99.7 % (96.5 to 99.8 % across earlier runs). |
| Traversal | **Met.** Mantle, vault and hurdle play the same Manny montage as on UEFN (`Mantle_1_0_run_F_Lfoot`, `Vault_1_0_run_F_Lfoot`, `Hurdle_1_0_run_F_V2_Rfoot`) and end at the same positions. The vault starts at 0.217 s instead of 0.350 s into the montage. |
| Network | **Met.** In a listen server with two clients, owner, server and proxy select only Manny databases and the same mantle montage. The new test `SurvivalRpg.GASP.Manny.DevPawn` adds a client that joins after the pawn moved and stopped. |
| Performance | **Measured, open.** In a fresh editor session the Manny pawn costs 0.33–0.35 ms game-thread animation and 0.92–0.93 ms animation-worker time (median), against 0.34 and 0.84 ms for the UEFN pawn: about 0.08 ms (10 %) more worker time. The cause is not isolated (see [Limits](#limits)). |
| Unchanged | **Met.** Outside the new folders, the branch changes only the Lyra `SK_Mannequin`: one added blend profile and no bone change. The shared UEFN databases, the Mover assets and the CMC gameplay pawns are untouched. |
| Size | 662 new packages under `Characters/GASP/Manny` (691 MB in LFS) and the 11 KB development Experience. |
| User check | **Pending.** |

### Phase A – inventory

- The CMC chooser `CHT_RpgGasp_CMC_PoseSearchDatabases` references 71 Relaxed databases and otherwise only enums and the AnimBP, so no row reaches a database outside the set. Its databases use 460 UEFN sequences.
- The full bake set is 548 UEFN assets: 512 sequences, the two AnimBP blend spaces (`BS_Neutral_AO_Stand`, `BS1D_Additive_Lean_Run`) and 34 traversal montages. Names are unique and every asset is on the UEFN skeleton.
- `PSN_Relaxed_All` also lists six databases that the chooser never selects. They were copied too, with their 8 clips baked, so that the Manny normalization matches the UEFN one.

### Phase B – bake

- **Retargeter.** `RTG_UEFN_to_Manny_Bake` is the `GASP-08` pilot copy with one further change: Pelvis Motion `BlendToAbsoluteOffset` goes from (1,1,0) to (1,1,1). The pilot made only the horizontal pelvis motion absolute. Its set had no vertical root motion; the jumps and traversal clips here do. In the first full bake, the scaled vertical pelvis left the root: catch clips ended up to 12 cm high at the pelvis and 6 cm at the feet, and vault and jump starts 47 cm low. After the change and a full rebake, the pelvis sits a constant 2.08 cm above the source relative to the root (Manny's own reference height). Over all 502 regular clips, the feet lie between −3.66 and +0.77 cm of the source relative to the root.
- **Output.** 522 sequences and blend spaces and 34 montages are under `Characters/GASP/Manny/Animations`, mirroring the source folders; the CMC traversal montages are under `Animations/Traversal/CMC`. The engine batch retarget remapped references inside the output only, and the montages point at the baked sequences.
- The mirror table `MDT_RpgGasp_Manny` is the pilot table, unchanged.

### Phase C – Manny copies

- 14 schemas, the 77 databases, `PSN_Relaxed_All`, the CMC database chooser and the experimental chooser `CHT_CMCCharacterAnimations` (kept only as AnimBP context), the CMC traversal database and chooser with its embedded databases, and `ABP_RpgGasp_CMC_Manny` with the Manny mirror table.
- The traversal montages blend out with the blend profile `FastFeet_InstantRoot`, which the Lyra `SK_Mannequin` lacked. It was copied by bone name from `SK_UEFN_Mannequin`. This is the only change to an existing asset.
- After the class remap, the AnimBP's chooser-evaluation context pin was orphaned. Reconnecting `Self` to the new pin fixed the compile.

### Phase D – development pawn and checks

- **Development chain.**
  - `Dev/BP_RpgGasp14_CMC_Manny` copies `BP_RpgGasp_MantleCharacter` with `Meshes/SKM_Manny_Simple_GaspLook` (the pilot's GASP-look mesh) and the Manny AnimBP.
  - Its traversal query component `AC_RpgGasp_TraversalQuery_Manny` lists the Manny montages, `GA_RpgGasp_Mantle_Manny` and `LAS_RpgGasp_Mantle_Manny` grant traversal, and `DA_PawnData_Gasp14_Manny` has no runtime retarget.
  - The Experience `RpgGasp14MannyExperience` and the map `Lvl_RpgGasp14_Manny` (a copy of `Lvl_RpgGaspMantle`) complete the chain.
- **The `attach` bone.** GASP's traversal data uses a UEFN-only bone `attach`, which carries the ledge point through each traversal clip.
  - The schemas `PSS_Traversal` and `PSS_Traversal_Chooser` sample it in two channels (their queries take the ledge from the AnimBP interface and do not read the bone).
  - The montages' Skew Warp modifiers use it as warp point. `URpgGameplayAbility_Mantle` computes the landing from it and rejects the traversal if the mesh lacks the bone.
  - Without it, the Manny schemas were invalid and every traversal fell back to a jump.
- **First fix, reverted.** With the user's approval, `attach` was added to `SK_Mannequin` and the source track copied into the 59 baked jump and traversal clips that carry it, and traversal worked. The new late-join test then timed out:
  - Adding a bone regenerates the skeleton GUID. Every animation asset still saved with the old GUID, 1,813 assets including 962 in the combat pack, is re-conformed to the skeleton on each load.
  - The first PIE start stalled for 30 s to over 60 s, every UEFN database rebuilt its index, and the engine warned about non-deterministic builds.
  - Through the Manny aim offset's preview pose, which still pointed at the UEFN idle, the engine even conformed that UEFN clip onto `SK_Mannequin` in memory.
- **Final fix.** The user chose Lyra's `interaction` bone, a child of `root` like `attach` and unanimated in every baked traversal clip. Neither the bone change nor the attach-track clips were pushed; the branch history was rebuilt without them.
  - `SK_Mannequin` is back to its `GASP-08` state plus the blend profile, and the GaspLook mesh was duplicated again from the pilot mesh.
  - The 59 clips carry the source `attach` track 1:1 on `interaction`; the world positions match exactly (0.0 cm).
  - The two Manny traversal schemas sample `interaction`, and the 51 Skew Warp modifiers in the 34 Manny montages warp to `interaction`.
  - The Manny aim offset previews the baked idle.
  - With this, editor start and PIE re-conform no asset, every database index comes from the cache, and the late-join test's owner is ready after 0.3 s instead of timing out.
- **Route.** One scripted route in standalone PIE at 60 FPS: idle, walk and run starts, stops and pivots in four directions, turns of 90° and 180°, sprint, crouch, jump, and mantle, vault and hurdle. The results are in the acceptance table.
  - The final `interaction` assets select the same databases as the earlier `attach` assets in 99.8 % of frames.
  - A traversal right after a schema reload falls back to a jump once while the index builds lazily. This happened on UEFN too and is engine behavior, not a bake difference. In a fresh editor session with a cached index, all three traversals succeed on the first attempt.
- **Network.** A listen-server probe with two clients recorded owner, server and proxy. In `SurvivalRpg.GASP.Manny.DevPawn`, the owner walks, and the listen server sees the moving pawn select a Manny database. After it stops, a second client joins and sees the pawn as a simulated proxy with a Manny database selected.
- **Performance.** `csvprofile` over the route in standalone PIE with one pawn at 60 FPS, two runs per pawn alternating in one fresh session:

  | Pawn | Game-thread animation | Animation worker |
  | --- | --- | --- |
  | UEFN (`BP_RpgGasp_MantleCharacter`) | 0.336 / 0.338 ms | 0.838 / 0.843 ms |
  | Manny (`BP_RpgGasp14_CMC_Manny`) | 0.345 / 0.330 ms | 0.927 / 0.920 ms |

  Two earlier sessions are set aside:
  - The first used the interim `attach` mesh and measured Manny at 0.75–0.76 ms and UEFN at 0.82–0.83 ms worker time.
  - The second ran with verbose animation logging, which inflated both.

### Validation

- **Build:** `ue.py build` passed (0 errors, 0 warnings) after the test change.
- **New test:** `SurvivalRpg.GASP.Manny.DevPawn` 1/1. It first timed out at the owner step (60 s) with the `attach` skeleton, which led to the finding above. A diagnostic run then crashed on a missing null check for the late joiner's pawn, which is now fixed.
- **Regression** on the final assets, each suite on its own through `ue.py test`:

  | Suite | Result |
  | --- | --- |
  | Native (movement, equipment, harvesting, abilities, block, dodge, combat content, CMC and mantle assets, speed parity, sprint, input, UI input, camera) | 139 / 139 |
  | `SurvivalRpg.Animation` | 4 / 4 |
  | `SurvivalRpg.GASP.CMC` | 14 / 14 |
  | `SurvivalRpg.GASP.Manny` (new) | 1 / 1 |
  | `SurvivalRpg.Combat.BlockLocomotion` | 15 / 16 |
  | `SurvivalRpg.GASP.Mantle` | 13 / 13 |
  | `SurvivalRpg.GASP.Vault` | 9 / 9 |
  | `SurvivalRpg.GASP.Hurdle` | 12 / 12 |
  | `SurvivalRpg.GASP.RuntimeRetarget` | 2 / 2 |
  | `SurvivalRpg.GASP.VariantComparison` | 2 / 3 |
  | `SurvivalRpg.Network` | 19 / 19 |
  | `SurvivalRpg.GASP.Mover` (`np.ForceReconcile 0`, `t.MaxFPS 30`) | 66 / 67 |

  All three failures are Mover cases. None of them loads a Manny asset, and this branch changes no Mover asset or runtime code. Each was rerun on its own:
  - `BlockLocomotionPIE.MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease` timed out waiting for the fixed replay. It is the known timing case from `GASP-ENG-01` (1 of 3 isolated runs passed there). Isolated: 1 of 2 passed.
  - `GaspVariantComparisonPIE.RagdollVariantRunningVaultPreservesMoverTraversal` timed out. Owner and server finish the vault, but the observing proxy never records the short fall after the vault handoff (`releasedFalling=0`, `landedBeyond=0`). Isolated: 4 of 5 passed. It had passed in all 9 earlier recorded runs, so it is new as a timing case and is listed under [Limits](#limits).
  - `GaspMoverRagdollPIE.CancelledGetupCannotCompleteTheNextRagdollEpisode` failed on physics warnings ("Failed to find bone data for neck_01"). It failed the same way in the `GASP-12` regression on 2026-10-13. Isolated: 2 of 2 passed.

### Limits

- **Performance:** the Manny pawn costs about 0.08 ms more animation-worker time than the UEFN pawn in the same session, and the cause is not isolated. Compression settings match in 103 of 103 sampled clips, and track counts are nearly equal (89 against 88 bone tracks). Sessions also differed by up to 0.17 ms. Measured in editor PIE with one pawn, not cooked and not with groups of characters. `GASP-15` replaces UEFN plus runtime follower, which cost 0.69 ms more than the Manny pilot in `GASP-08`, so the comparison that decides it is still open.
- **`interaction` carries the traversal ledge point** in the 59 Manny jump and traversal clips. Anything attached to `interaction` follows it during those clips. `GASP-15` must keep the bone free or move the role.
- **The `SK_Mannequin` GUID must stay stable.** Any later bone or hierarchy change to it re-conforms about 1,800 assets on every load until all are resaved.
- **Lazy traversal index:** after the traversal data changes, the first traversal can fall back to a jump once while the index rebuilds. This also happens on UEFN; a fresh session with a cached index is not affected.
- **Mover timing case:** `RagdollVariantRunningVaultPreservesMoverTraversal` failed in 2 of 6 runs on this branch (once in the regression, once isolated), against none of 9 earlier recorded runs. The observing proxy misses the short fall after the vault handoff. The test loads no Manny asset, and this branch changes no Mover asset or code; the cause is not analyzed.
- **Outside references:** these remain, as in the sources:
  - 86 baked clips reference the experimental UEFN state-machine databases (`PSD_SM_CMC_*`).
  - The Manny copy of the experimental chooser `CHT_CMCCharacterAnimations` references 387 UEFN clips.
  - The Manny traversal component references the Mover chooser `CHT_TraversalMontages_Mover`.
  - The experimental path is off, but loading the Manny AnimBP still loads these UEFN assets.
- **Mesh:** `SKM_Manny_Simple` has no corrective bones (decision above). The development pawn has no block layer, and gameplay montages, sockets, death and ragdoll on Manny are untested (`GASP-15`).
- **Preservation timing:** the contract audit ran before the `interaction` edits, which change only that bone's keys and the warp point name.
