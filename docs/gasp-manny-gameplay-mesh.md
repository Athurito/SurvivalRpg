# GASP-15 – Manny as the CMC gameplay mesh

Status: **Ready**, defined 2026-10-17. After merging `GASP-14`, the user asked to start `GASP-15` on 2026-10-17 and chose its scope (see [Decisions](#decisions)). This report defines the package and becomes its task report.

## Goal

The CMC gameplay pawns play the Lyra UE5 Manny skeleton natively: `GetMesh()` is the GASP-look `SKM_Manny_Simple` with the `GASP-14` Manny AnimBP, traversal uses the Manny traversal chain, and the runtime retarget follower is gone from them. Combat, equipment, block, hit reactions, death and ragdoll work on Manny on every network role. The Mover stays on UEFN with its follower. The `GASP-08` pilot and the `GASP-14` development chain are removed.

## Why

`GASP-14` baked everything the CMC plays onto Manny and showed the same selection and traversal on a development pawn ([report](gasp-manny-full-bake.md)). The production CMC pawns still play UEFN:
- the gameplay montages (sword attack and combo, harvest swing, hit reaction, death) are Manny clips that the engine remaps bone by name onto UEFN at runtime ([pilot](gasp-manny-skeleton-pilot.md#phase-a--inventory-and-skeleton-contract));
- the Mantle pawn carries an optional runtime follower for a Manny presentation mesh.

On Manny the gameplay montages play natively, and one skeleton tick replaces mesh plus follower.

## Current state (checked 2026-10-17 on `master` `f8e95aa4`)

| Area | State |
| --- | --- |
| CMC pawns | `CMC/RPG/BP_RpgGasp_CMC` (Experience `RpgGaspCMCExperience`, map `Lvl_RpgGaspCMC`) and its child `CMC/RPG/Traversal/BP_RpgGasp_MantleCharacter`. The child is used by `RpgGaspMantleExperience` (`Lvl_RpgGaspMantle`, `GF_Dev_Sandbox` `Lvl_ResourceParity`) and `RpgInventoryStorageExperience`. Both play `SKM_UEFN_Mannequin` with `ABP_RpgGasp_CMC` and `PA_UEFN_Mannequin`; the UEFN mesh also runs the post-process AnimBP `ABP_UEFN_Mannequin_PostProcess`. |
| PawnData | `DA_PawnData_GaspCMC` (no follower) and `DA_PawnData_GaspMantle` (follower profile `DA_RpgGasp_RuntimeRetarget`, traversal ability set). |
| Traversal | `AC_RpgGasp_TraversalQuery`, `GA_RpgGasp_Mantle` and its ability set use the UEFN montages. `GASP-14` made Manny copies (`AC_RpgGasp_TraversalQuery_Manny`, `GA_RpgGasp_Mantle_Manny`, `LAS_RpgGasp_Mantle_Manny`). |
| Mover | `BP_RpgGasp_Mover` and `BP_RpgGasp_MoverRagdoll` play UEFN; `DA_PawnData_GaspMover` carries the follower profile `DA_RpgGaspMover_RuntimeRetarget`. |
| Gameplay montages | `GF_Combat_Core` and `GF_Harvesting_Magic` montages for attack, combo, harvest, hit reaction and death use the Lyra `SK_Mannequin`. |
| Block | The shield (`BP_WeaponInstance_BasicShield`) has one `FRpgWeaponBlockDefinition`: the linked layer class `ABP_Block_SwordShield` and the start, end, hit and parry montages. These are a UEFN set (35 sequences, `BS_BlockWalk`, 5 montages) derived from Sword & Shield Manny clips ([block locomotion](gasp-block-locomotion.md)). `URpgEquipmentManagerComponent` links exactly that class on the gameplay mesh, and a linked layer must match the mesh skeleton. |
| Equipment sockets | Sword, two-handed sword, pickaxe and axe attach to `hand_r`, the shield to `hand_l`; both bones exist on both skeletons. The grip offsets were tuned on UEFN. |
| Death and ragdoll | `SKM_Manny_Simple` uses `PA_Mannequin`. The CMC death and corpse path runs on the gameplay mesh's physics asset. |
| Manny assets | `GASP-14` under `Characters/GASP/Manny`: baked set, Motion Matching copies, `ABP_RpgGasp_CMC_Manny`, traversal chain, and the development chain (`Dev/BP_RpgGasp14_CMC_Manny`, `Dev/DA_PawnData_Gasp14_Manny`, `RpgGasp14MannyExperience`, `Lvl_RpgGasp14_Manny`). The `GASP-08` pilot is under `Characters/GASP/Pilot` (208 packages, 157 MB) with three Experiences and maps. |
| Tests with UEFN or CMC paths | CMC integration, speed parity, sprint assets, mantle, vault, hurdle, runtime retarget (follower on the Mantle PawnData), block locomotion, `SurvivalRpg.GASP.Manny.DevPawn`. |
| Cost | `GASP-14` measured the Manny pawn at about 0.08 ms more animation-worker time than the UEFN pawn. Insights puts it in the Motion Matching node's update and blend stack, not in the database search or decompression. |

## Scope

1. **Phase A – inventory and contracts.**
   - List every asset and test that names the UEFN CMC chain, the follower on the Mantle PawnData, the pilot or the `GASP-14` development chain.
   - Compare the hand bones and the weapon grips of UEFN and Manny, and the physics bodies of `PA_UEFN_Mannequin` and `PA_Mannequin`.
   - Inventory the shield's block set, the authoring steps recorded in [block locomotion](gasp-block-locomotion.md) (root mapping, rate scales, notifies, sections), and their Sword & Shield sources.
   - Record a baseline on `master`: the UEFN Mantle pawn with and without its follower target, route cost, montage playback on owner, server and proxy.
2. **Phase B – block presentation per skeleton (native).** `FRpgWeaponBlockDefinition` gains optional presentation variants keyed by skeleton. Each variant holds the linked layer class and the start, end, hit, parry and guard-break montages. The existing fields stay the default. `URpgEquipmentManagerComponent` binds the variant that matches the gameplay mesh's skeleton, and the block and stagger abilities play the matching montages. Unit tests cover the selection and the fallback at this seam.
3. **Phase C – Manny block content.** A Manny block set from the original Sword & Shield clips with the recorded authoring steps reapplied; a bake of the UEFN set is the fallback if a step cannot be reproduced. This includes a Manny data child of the block layer, its chooser and blend space, five montages, and the shield's Manny variant. The Mover keeps the UEFN set.
4. **Phase D – switch the CMC pawns.**
   - `BP_RpgGasp_CMC`: `SKM_Manny_Simple_GaspLook` with `ABP_RpgGasp_CMC_Manny` and `PA_Mannequin`.
   - `BP_RpgGasp_MantleCharacter`: the Manny traversal component.
   - `DA_PawnData_GaspMantle`: the Manny traversal ability set and no follower profile.
   - Weapon grips: only if Phase A measures a difference, a socket of the same name on both skeletons carries the offset, so the equipment definitions stay skeleton-free and the Mover keeps its grip.
   - The UEFN CMC chain (AnimBP, choosers, traversal component, ability) stays in place unused; its removal is a later package.
5. **Phase E – cleanup and tests.**
   - Remove the `GASP-08` pilot (folder, Experiences, maps) and the `GASP-14` development chain (pawn, PawnData, Experience, map, test). Remove nothing that production, the Mover or a remaining test references.
   - Update the tests to the Manny contracts. Move the runtime retarget tests to the Mover pawn.
6. **Phase F – validation.**
   - Regression and network checks.
   - A cost comparison of the Manny CMC pawn against the UEFN pawn with follower target on `master`. It includes a Motion Matching probe of pose jumps and blend-stack entries for the open `GASP-14` cost finding.
   - The user check.

## Out of scope

- The Mover and Mover ragdoll (UEFN with follower), the shared UEFN databases and Mover assets.
- Removing the unused UEFN CMC chain, the runtime retarget mechanism or `SKM_UEFN_Mannequin`.
- A full Manny mesh with corrective bones, other characters and armor (`GASP-16`), and the dodge montages (#124).
- `GASP-STAB-05` (block release), which stays parked.

## Decisions

- 2026-10-17 (user): start `GASP-15`.
- 2026-10-17 (user): only the CMC pawns switch to Manny. The Mover stays on UEFN with its follower, and block presentation becomes selectable per skeleton so that the shield works on both.
- 2026-10-17 (user): remove the `GASP-08` pilot. The `GASP-14` development chain is absorbed into production, and its development assets are removed.
- Earlier, still in force: the mesh is `SKM_Manny_Simple` (`GASP-14`). The Lyra `interaction` bone carries the traversal ledge point, and `SK_Mannequin` gets no bone or hierarchy change (`GASP-14`).

## C++ boundary

- **Classification:** per-skeleton block presentation is a schema extension of a reusable native mechanism. Everything else is designer content and asset composition.
- **Runtime truth:**
  - The server owns equipped state and block activation, as today.
  - Each machine binds the linked layer on its own gameplay mesh. The block and stagger abilities choose montages from the avatar's mesh skeleton, so prediction and the server play the same choice.
- **Existing seams:** `FRpgWeaponBlockDefinition` on `URpgWeaponInstance`, `URpgEquipmentManagerComponent::RefreshBlockLocomotionLayer`, `URpgGameplayAbility_Block`, `URpgGameplayAbility_Stagger`.
- **Ownership:**
  - C++: the variant struct and the resolution by skeleton.
  - Assets: the Manny block layer child, chooser, blend space, montages and the shield's variant entry.
  - Through the Unreal MCP workflow: pawn, PawnData, sockets and cleanup.
- **New native classes:** none; one new `USTRUCT` for the variant. The linked-layer API requires exact skeleton identity, which Blueprint data alone cannot select per mesh.
- **Asset work:** Unreal MCP authoring, compile and save, asset-registry closure checks, PIE probes.

## Ownership

| Concern | Owner |
| --- | --- |
| Equipped state, block activation, damage | Unchanged: equipment components and GAS on the server |
| Block presentation variant | Native schema on the weapon block definition; content per variant in assets |
| Gameplay mesh, AnimBP, physics asset, sockets, traversal chain, PawnData | Designer content through the Unreal MCP workflow |
| Removal of pilot and development chain | Asset deletion with a registry check that nothing remaining references it |
| Verification | Native unit tests for the variant seam; PIE network tests and probes; no C++ test per visual leaf |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Mesh | Both CMC pawns have `SKM_Manny_Simple_GaspLook` with `ABP_RpgGasp_CMC_Manny` as `GetMesh()` and no runtime follower. The Mover pawns are unchanged. |
| Locomotion and traversal | The route matches the `GASP-14` development pawn in at least 99 % of frames. Mantle, vault and hurdle play the same Manny montages. |
| Combat | Sword attack and combo, harvest swing, hit reaction and death play natively on Manny on owner, server and proxy. Attack windows and traces hit as on `master`. |
| Block | Holding block on the Manny CMC pawn shows the Manny block layer and montages. The Mover pawn keeps its UEFN block. Block locomotion tests pass for both. |
| Weapons | Weapon grips on Manny match UEFN within 1 cm and 2° relative to the hand, or the difference is shown to the user and accepted. |
| Death and ragdoll | Death, corpse ragdoll on `PA_Mannequin`, downed and respawn behave as on `master`, including on a proxy and a late joiner. |
| Network | Owner, listen server, proxy and late joiner see the Manny mesh, equipped weapons, the block layer and montages. |
| Performance | Game-thread and animation-worker cost of the Manny CMC pawn against the UEFN pawn with follower target on `master`. The Motion Matching probe explains or bounds the `GASP-14` difference. |
| Cleanup | The pilot and the development chain are gone, and no remaining asset or test references them. The removed LFS volume is reported. |
| Regression | Native, animation, CMC, block, traversal, runtime retarget (Mover), network and Mover suites pass apart from documented timing cases. |
| User check | The user plays the CMC maps on Manny, including combat, block, death and a multiplayer step. |

## Verification

Build and test through `Build/Tools/Unreal/ue.py`; regressions as in `GASP-14`. Raw evidence goes to `Saved/Gasp15`; this report, the PR and the manifest `docs/assets/gasp-15-manny-gameplay-mesh.json` carry the summary.

## Repro (after implementation)

1. Open `Lvl_RpgGaspMantle` without a PIE Experience override and start PIE.
2. Walk, run, sprint, crouch, jump and traverse. Equip sword and shield: attack, combo, hold block while walking, release, take a hit, and die.
3. Repeat on `Lvl_RpgGaspCMC` and as a listen server with two clients, one of which joins late.
4. Compare with the Mover on `Lvl_RpgGaspMover`: it still plays UEFN, and block works there too.
