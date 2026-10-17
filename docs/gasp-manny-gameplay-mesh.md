# GASP-15 – Manny as the CMC gameplay mesh

Status: **PR open** ([#254](https://github.com/Athurito/SurvivalRpg/pull/254)), implemented 2026-10-17 on `claude/gasp-15-manny-gameplay-mesh`; the user check is in progress ([User check 1](#user-check-1-2026-10-17--block-turning)). Defined 2026-10-17 (definition PR [#253](https://github.com/Athurito/SurvivalRpg/pull/253)): after merging `GASP-14`, the user asked to start `GASP-15` and chose its scope (see [Decisions](#decisions)). Results are under [Results](#results).

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
- 2026-10-17 (user): create no legacy asset types such as `BlendSpace1D`. The block walk and the Manny additive lean use standard Blend Spaces.
- 2026-10-17 (user): the standing-turn host-view test checks the documented criterion, root within 60° of the capsule, instead of the distance travelled since the flick. The larger remaining offset on remote Manny roles stays an open finding (see [Phase F](#phase-f--validation)).
- 2026-10-17 (user, user check 1): the block turning findings, which predate `GASP-15`, go to a separate stabilization package (`GASP-STAB-06`); `GASP-15` stays unchanged.
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

## Results

All numbers come from runs in this task on UE 5.8.3 in the editor of the producing checkout (worktree `musing-ishizaka-7bac20`). Raw evidence is in `Saved/Gasp15` and `Saved/ToolRuns` of that checkout and is not versioned. The [manifest](assets/gasp-15-manny-gameplay-mesh.json) lists the added, changed and removed packages with their hashes.

### Acceptance status

| Area | Result |
| --- | --- |
| Mesh | **Met.** `BP_RpgGasp_CMC` and `BP_RpgGasp_MantleCharacter` use `SKM_Manny_Simple_GaspLook` with `ABP_RpgGasp_CMC_Manny` and no follower component or profile; the Mover pawns are unchanged. |
| Locomotion and traversal | **Met.** The route selects the same database as the `GASP-14` development pawn in 99.9 % of frames, with the same mantle, vault and hurdle montages and end positions. |
| Combat | **Met.** `RemoteMovementLateJoinAndEquipmentMontage` passes on Manny: the attack montage and its attack windows on owner and server, and death, downed and respawn for every role. Sword, combo, harvest, hit and death montages are Manny assets and now play without bone remapping. |
| Block | **Met.** The CMC pawn links `ABP_Block_SwordShield_Manny` and plays the Manny block montages; the Mover keeps the UEFN set. `SurvivalRpg.Combat.BlockLocomotion` passes 16/16 on both hosts, including layer swap, reactions, stagger and late join. |
| Weapons | **Met as far as measurable.** The hand bones share their axes (index and little finger within 1°), so the unchanged attach transforms place the weapons the same way relative to the hand bone. How they sit in Manny's hand geometry is part of the user check. |
| Death and ragdoll | **Met.** On `PA_Mannequin` the corpse comes to rest after about 1.9 s with the pelvis 16.5 cm above the floor, comparable to UEFN (14.3 cm after 1.5 s). The automated death and respawn path passes on every role. |
| Network | **Met.** The CMC, host-view, sprint, block, traversal and network suites pass with owner, listen server, proxy and late joiners on Manny. |
| Performance | **Measured, no reliable difference.** In one editor session on the locomotion route, the Manny pawn costs 0.82–0.83 ms animation-worker time, against 0.94 ms for the same pawn switched to UEFN at runtime. `GASP-14` had measured the opposite (+0.08 ms for Manny), and one pawn varied by up to 0.17 ms between sessions. Game-thread animation is 0.32–0.35 ms for both. |
| Cleanup | **Met.** 214 files (186.1 MB of LFS) are removed with no remaining referencer; 47 files (19.4 MB) are added. |
| Regression | **Met** after the host-view test fix (Phase F). |
| User check | **In progress.** The first findings concern block turning, predate `GASP-15` and moved to `GASP-STAB-06` ([User check 1](#user-check-1-2026-10-17--block-turning)). |

### Phase A – inventory and contracts

- **Removal set.** 213 packages: the 206 of the `GASP-08` pilot folder, its three Experiences, and the `GASP-14` development pawn, PawnData, Experience and map. No asset outside the set referenced any of them.
- **Grips.** The UEFN and Manny hand bones share their axis convention. In hand space, the index and little finger point within 1° of each other and the middle finger within 6°; only the short thumb base differs (19°). The same attach transform therefore gives the same grip relative to the hand bone. Whether the weapon sits in Manny's hand geometry is part of the user check.
- **Physics.** `SKM_Manny_Simple` uses `PA_Mannequin`, which the baseline pawn and the AI placeholders already ragdoll with. The CMC corpse path simulates the gameplay mesh's physics asset below its ragdoll bone, so it needs no change.
- **Follower.** Both runtime retarget profiles (CMC mantle and Mover) have no target mesh, so no follower was visible in production. On the CMC only the unused profile reference and the inert `RpgRuntimeRetargetComponent` on the mantle pawn remain to be removed.
- **Block set.** The UEFN block set was derived from the Sword & Shield Manny clips with recorded steps: root mapping, a `BlockRootYaw` curve, root lock, `FootContacts` markers, and per-loop `RateScale` for 157 cm/s ([block locomotion](gasp-block-locomotion.md)). The authoring script is not versioned, so the definition's fallback applies: the UEFN set is baked back with the `GASP-14` retargeter, which preserved curves, markers, rate scales and root flags exactly.

### Phase B – block presentation per skeleton

- `FRpgWeaponBlockPresentation` (new `USTRUCT`) holds a skeleton, the linked layer class and the start, loop, end, hit, perfect-block and guard-break montages. `FRpgWeaponBlockDefinition::SkeletonPresentations` lists them, and `ResolveForSkeleton` returns a copy where a matching entry replaces the whole presentation; the existing fields stay the default.
- `URpgEquipmentManagerComponent::RefreshBlockLocomotionLayer` links the layer resolved for the gameplay mesh's skeleton, and `URpgGameplayAbility_Block` resolves the avatar's gameplay mesh skeleton when it snapshots the definition. Prediction and the server read the same mesh.
- Data validation rejects a presentation without a skeleton, a skeleton listed twice, a layer that targets another skeleton, and a montage on another skeleton. The existing error text still names `BlockDefinition.BlockLocomotionLayer`.
- `ConfigureBlockLocomotionForTests` writes the layer into the presentation the test pawn resolves, so a later restore is symmetric.
- New native tests: `SurvivalRpg.Equipment.BlockPresentation.ResolvesBySkeleton` and `...ValidationRequiresUniqueMatchingSkeletons`.

### Phase C – Manny block content

- **Bake.** The 35 UEFN block sequences and `BS_BlockWalk` went through the engine batch retarget with `RTG_UEFN_to_Manny_Bake` into `GF_Combat_Core/Animations/BlockLocomotion/SwordShield/Manny`. The five montages are duplicates pointed at the baked clips.
  - The contract audit keeps all 41 pairs: length, frames, root-motion flags, rate scale, additive settings, curves (including `BlockRootYaw`), notifies, `FootContacts` markers, montage segments and blend times, and blend-space samples.
  - The horizontal root distance ratio is exactly 1.0 for all 25 clips that move, so the 157 cm/s rate scales stay valid.
- **No legacy assets.** The batch copied `BS_BlockWalk` as a `BlendSpace1D`. At the user's request (2026-10-17: no legacy asset types), it is a standard `BlendSpace`:
  - the same horizontal axis (Direction −180..180, 8 cells, wrapped) and settings;
  - an unused vertical axis 0..1;
  - every sample at Y = 0 and Y = 1, so the result does not depend on Y.

  The `GASP-14` Manny AnimBP's additive lean `BS1D_Additive_Lean_Run` is replaced the same way by `BS_Additive_Lean_Run`.
- **Layer.** `ABP_RpgBlockLocomotion_Manny` is a copy of the shared block parent on `SK_Mannequin`. The engine binds a child Anim Blueprint only to its parent's skeleton unless that parent is a template, and the template flag cannot be set through Python or the project toolset. `ABP_Block_SwordShield_Manny` is its data child, with a Manny chooser over the baked clips. The CDO's `PackChooser` points at it, because asset reference remapping does not reach Blueprint defaults.
- **Shield.** `BP_WeaponInstance_BasicShield` has one presentation for `SK_Mannequin`:
  - the Manny layer;
  - the Manny start, end, hit, parry and guard-break montages;
  - the Sword & Shield loop montage, which is a Manny asset already.

  The Mover resolves the default, the UEFN set.
- **Test variant.** `ABP_Block_TestVariant_Manny` with its own Manny chooser gives the layer lifecycle test a second Manny profile.

### Phase D – switch the CMC pawns

- `BP_RpgGasp_CMC` uses `SKM_Manny_Simple_GaspLook` with `ABP_RpgGasp_CMC_Manny` and the mesh's own materials; the UEFN material override is gone. `BP_RpgGasp_MantleCharacter` inherits that mesh and uses `AC_RpgGasp_TraversalQuery_Manny` under the same component name.
- `DA_PawnData_GaspMantle` grants `LAS_RpgGasp_Mantle_Manny` and names no runtime retarget profile. The mantle pawn no longer has a `RpgRuntimeRetargetComponent`.
- **Route.** On `Lvl_RpgGaspMantle`, the switched production pawn selects the same database as the `GASP-14` development pawn in 99.9 % of frames. Mantle, vault and hurdle play the same Manny montages and end at the same positions.
- **First PIE attempt.** It hung behind the editor's "blueprints have errors" dialog. The shield had last been compiled while its new Manny layer child still pointed at the UEFN chooser. After the child was fixed and the editor restarted, the shield compiles cleanly.

### Phase E – cleanup and tests

- **Removed:** the 213 packages above and `SurvivalRpg.GASP.Manny.DevPawn`, whose coverage the CMC suite now gives the production pawn.
  - The Experience manager kept two Experiences loaded and refused their editor deletion. They and the development map were removed with `git rm` after closing the editor, once the registry showed no referencer.
- **Tests on Manny contracts:**
  - CMC integration: gameplay mesh, AnimBP and database chooser.
  - Speed parity: AnimBP, chooser and reference loops in the baked set.
  - Mantle, vault and hurdle: `GA_RpgGasp_Mantle_Manny`.
  - Block locomotion: per host, the source mesh, the resolved presentation and the second test profile.
- **Runtime retarget.** The follower no longer exists on any CMC pawn. Its PIE test, `RemotePoseLateJoinTraversalEquipmentAndFallback`, needs an `ARpgCharacter` and the CMC mantle, so it was retired. The Mover suites already cover the follower with equipment, attacks, late join, death, respawn and ragdoll. `SurvivalRpg.GASP.RuntimeRetarget.OptionalProfileComposition` now checks the new composition: the Mover keeps its optional profile without a target, and both CMC PawnData compose neither a profile nor the component.

### Phase F – validation

- **Build:** `ue.py build` passed (0 errors, 0 warnings) after each code change.
- **Regression** on the final assets, each suite on its own through `ue.py test`:

  | Suite | Result |
  | --- | --- |
  | Native (movement, equipment incl. the block presentation tests, harvesting, abilities, block, dodge, combat content and contracts, CMC and mantle assets, speed parity, sprint, runtime retarget composition, input, UI input, camera) | 143 / 143 |
  | `SurvivalRpg.Animation` | 4 / 4 |
  | `SurvivalRpg.GASP.CMC` | 13 / 14, then 14 / 14 after the host-view fix |
  | `SurvivalRpg.Combat.BlockLocomotion` | 16 / 16 |
  | `SurvivalRpg.GASP.Mantle` | 13 / 13 |
  | `SurvivalRpg.GASP.Vault` | 9 / 9 |
  | `SurvivalRpg.GASP.Hurdle` | 12 / 12 |
  | `SurvivalRpg.GASP.VariantComparison` | 3 / 3 |
  | `SurvivalRpg.Network` | 19 / 19 |
  | `SurvivalRpg.GASP.Mover` (`np.ForceReconcile 0`, `t.MaxFPS 30`) | 67 / 67 |

- **Host-view turn.** `CameraFacingStanceTurnInPlaceFollowsOnEveryRole` failed once in the regression and passed twice on its own.
  - Remote roles end a standing turn further behind the capsule on Manny than on UEFN:

    | Role | UEFN, 7 earlier runs | Manny, 3 runs |
    | --- | --- | --- |
    | Listen server, median / p90 / max | 1.0° / 26° / 33° | 13.4° / 40.6° / 90° |
    | Proxy, p90 | 7.7° | 41.2° |

  - The owner ends at 0° on both.
  - When the listen server ended a 180° flick 40.6° behind, the next −90° flick left it 49.4° from the capsule, below the 50° turn-in-place threshold. So the root rightly stayed, but the test, which measured the travel since the flick, failed.
  - The user chose to check the criterion that the `GASP-13` report states: the root within 60° of the capsule (50° after a sweep). The travel is still logged.
  - After the fix: three host-view runs 3/3, CMC suite 14/14. The largest remaining offsets are 18.6° (server) and 44.3° (proxy). A root that does not turn still fails.
- **Cost probe.** Insights traces of one route per pawn:
  - Manny's extra time sits in the Motion Matching node's update (385 against 299 µs per frame exclusive) and in pose evaluation (981 against 881 µs). The database searches themselves cost the same.
  - Both evaluate the same number of sequence poses (12,431 against 12,440). Each Manny evaluation costs about 25–35 % more.
  - Ruled out as causes:
    - more blending or more clip changes;
    - larger or more animated data (the bakes are 22 % smaller compressed and keep fewer animated tracks);
    - search parameters, which are identical in all 77 databases;
    - the mesh (UEFN even adds a post-process AnimBP);
    - translation retarget modes (UEFN uses the costlier `OrientAndScale` on 75 bones).
  - A structural difference that remains is the skeleton size: 161 bones in the Lyra skeleton against 91, 72 of them not in `SKM_Manny_Simple`. Because a same-session locomotion comparison then measured Manny 0.11 ms cheaper, no reliable per-pawn difference is established.

### User check 1 (2026-10-17) – block turning

The user found three things while blocking in a listen-server session:
- turning the camera away and then pressing block is instant on one screen, while another screen drags the legs around;
- turning in place with block held looks good, but jitters slightly while the camera still turns;
- the body sometimes starts turning one way and then switches to the other.

A scripted listen-server probe recorded the acting player on its own screen, the listen server or the other client, and a second client. Every case ran with Manny and with the same pawns switched to UEFN at runtime; the block layer and montages follow the skeleton. All three behave the same on UEFN, so they predate `GASP-15`:

| Finding | Measured | Cause |
| --- | --- | --- |
| Block after a camera turn | Own screen and listen server: the body reaches the capsule in 0.11–0.15 s without a step. Remote copies: a 90° block turn clip over 0.85–1.2 s with two direction changes of the root (Manny 0.86–0.9 s, UEFN 0.85–0.96 s). | The capsule turns in the frame the block layer starts. Where the mesh follows at once, the layer sees no turn and the base root catches up through `Interpolate` (0.1-s half-life). A remote copy smooths its mesh rotation over several frames. The layer starts during that smoothing, counts the rest (about 80°) as turn debt and plays a turn clip. |
| Jitter while turning with block held | Every right turn clip (`Turn_Block_90_R`, `_180_R`) first turns the hips 13.7° and the shoulders 8.6° against the turn for about 0.1 s; left clips do not. Continuous turning steps stop-and-go, a 90° clip about every 1.4 s at 90°/s. Root motion itself is smooth on every role. | The right turn clips themselves (same on both skeletons and roles); the cadence comes from the layer's debt threshold and clip lengths. |
| Starts one way, then the other | Besides the twist above: a 180° camera flick is ambiguous, and owner, listen server and remote copies pick different sides; a remote copy follows the direction of its mesh smoothing. Once, a remote copy started a right turn from a 4° leftover debt and switched to the left 0.25 s later. | Turn direction from the sign of a debt near ±180°, and turn starts from smoothing leftovers on remote copies. |

The user moved all three into a separate stabilization package, `GASP-STAB-06` (definition [#255](https://github.com/Athurito/SurvivalRpg/pull/255), planned), and chose the target for a block press after a camera turn: body and shield face the block direction at once on every screen, as on the acting player's own screen now. `GASP-15` stays unchanged. Raw evidence: `Saved/Gasp15/UserCheck`.

### Limits

- **Remote standing turns:** remote Manny roles end standing turns up to 41–44° behind the capsule (UEFN up to 33°). That is below the turn-in-place threshold, and the aim offset covers it, but a remote player's feet can visibly trail its body. The owner is unaffected. Tuning the remote turn selection is left for a later step.
- **Cost:** no reliable difference with one pawn in editor PIE; groups of characters and cooked builds are not measured (`GASP-16`, `GASP-VAL-02`).
- **Block parent copy:** the block layer parent exists twice (UEFN for the Mover, Manny for the CMC). A change to the block logic, for example when `GASP-STAB-05` resumes, must be made in both until the Mover moves or a template parent replaces them. A template needs an editor tool for `bIsTemplate`, which the project toolset cannot set yet. The abstract parents also trigger an engine ensure when the editor renders their thumbnail on save; it does not affect play.
- **Block set quality:** the Manny block clips are a bake of the UEFN set, which itself came from the Manny Sword & Shield clips. All authored data is preserved, but the poses went through two retargets.
- **Runtime retarget coverage:** the CMC traversal test with a follower is retired. The follower during a traversal and the missing-source probe are no longer tested; the Mover suites cover the rest.
- **Unused UEFN CMC chain:** `ABP_RpgGasp_CMC`, its choosers, `AC_RpgGasp_TraversalQuery`, `GA_RpgGasp_Mantle`, its ability set and `DA_RpgGasp_RuntimeRetarget` stay in place unused; their removal is a later package.
- **Not checked by the user yet:** the weapon grips in Manny's hand geometry, block and combat feel on Manny, and a multiplayer session.
