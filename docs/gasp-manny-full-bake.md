# GASP-14 – Full CMC bake to Manny

Status: **Ready**, defined 2026-10-17. After parking `GASP-STAB-05`, the user asked to continue with the roadmap and chose `SKM_Manny_Simple` as the bake mesh on 2026-10-17. This report defines the package and becomes its task report.

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

1. Open the development map without a PIE Experience override and start PIE.
2. Walk, run, sprint and crouch in all directions, stop, jump, turn the camera for turn in place, and mantle, vault and hurdle over the obstacles.
3. Compare with the UEFN pawn on `Lvl_RpgGaspCMC`: feet, stride, stops, turns and traversal should match; block shows no block layer on the Manny pawn yet.
