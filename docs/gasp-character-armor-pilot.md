# GASP-16 – Character and armor pilot

Status: **In progress** since 2026-10-18. Phases A, B and C are done; next is Phase D (Paladin content). On 2026-10-18 the user changed the design to Lyra Character Parts (see [Decisions](#decisions)). Defined in [#262](https://github.com/Athurito/SurvivalRpg/pull/262) after the user added character packs to the asset library and chose the pilot character and the character-editor scope. This report is the task report.

## Goal

A marketplace player character runs on the Manny gameplay skeleton from `GASP-14`/`GASP-15`: the Polyphoria MetaHuman Paladin with one modular armor set.

The project adopts Lyra's Character Parts:
- The animated `GetMesh()` stays the Manny gameplay mesh. It becomes invisible while a body part is present.
- The visible Paladin body is a character part. It copies the gameplay mesh's pose and drives its corrective bones with the UE5 mannequin post-process.
- The armor pieces are character parts that equipment adds and removes. They follow the body by Leader Pose and inherit its corrective bones. Through their cosmetic tags they hide the body regions they cover.

This works on every network role, including late join, death and ragdoll. Knight_01 runs as a comparison and fallback. The MetaHuman route to a later character editor stays open, and the Character Parts controller component becomes its seam.

## Why

Since `GASP-15`, the CMC pawns play the plain Lyra mannequin `SKM_Manny_Simple_GaspLook`. The project has no player character and no armor visuals: the only armor item, `ID_BasicHelmet`, grants an ability set but shows nothing. `docs/game-vision.md` section 12 asks for a third-person character with a simple item and equipment structure. Armor that the player can see is part of that structure.

## Current state (checked 2026-10-18 on `master` `18c7c006`)

| Area | State |
| --- | --- |
| CMC pawns | `BP_RpgGasp_CMC` and `BP_RpgGasp_MantleCharacter` play `SKM_Manny_Simple_GaspLook` with `ABP_RpgGasp_CMC_Manny` and `PA_Mannequin` on the Lyra `SK_Mannequin`, without a runtime follower. Block selects its presentation by the gameplay mesh's skeleton (`FRpgWeaponBlockDefinition::SkeletonPresentations`). |
| Mover pawns | Stay on `SKM_UEFN_Mannequin` with an optional runtime retarget follower (`URpgRuntimeRetargetComponent`). |
| `SK_Mannequin` | No new bones or hierarchy changes: a new skeleton GUID re-conforms about 1,800 assets on every load. |
| Equipment visuals | `URpgEquipmentDefinition::ActorsToSpawn` makes the server spawn replicated actors and attach them to `GetMesh()` at a per-slot socket. Weapons use this today. |
| Armor | Equipment slots `Head`, `Chest`, `Hands`, `Legs` and `Feet` exist. `ID_BasicHelmet` / `ED_BasicHelmet` in `GF_Combat_Core` grant `DA_HelmetAbilitySet` and spawn no actor. No armor mesh exists in the project. |
| Asset library | `D:/Repos/Assets/Assets_3DAssets` (MCP port 8001). Packs added 2026-10-18 are listed below. Their skeletons were read from the package name tables and still need checking in the editor. |

| Pack | Content | Skeleton |
| --- | --- | --- |
| `Polyphoria/Paladin` | Modular armor: helmets (two variants), chest, bracers, pants, boots, and two combined meshes. Also a sword and two shields as static meshes, and physics assets for the skirt and chest. `MH_Creator` holds a MetaHuman Character asset (`MHC_paladin`) with outfit, cloth and wardrobe assets. About 32 MB of meshes and 302 MB of textures; `MH_Creator` adds 264 MB. | Armor on `SKEL_metahuman_base`. Its bone names include every Manny bone, plus MetaHuman corrective, finger, toe and `_dyn` bones and face curves. |
| `Polyphoria/Base` | Male body with head (`SK_ma_meta_body_head_01`), hands mesh, skin, eye and hair materials, and body mask textures. The pack's sample characters `BP_UE5_ma_paladin` and `_full` drive them with a Manny AnimBP. | A copy of the UE5 Manny skeleton (`SKEL_Mannequin_UE5`). |
| `Knight_01` | Five full knight meshes, about 35 modular parts (including body variants for metal gloves and half armor), physics assets, UE4 variants. 142 MB of UE5 meshes and 751 MB of textures. | A copy of the UE5 Manny skeleton (`SKM_Manny_Skeleton`) with the same bone names. |
| Enemy packs | `SkeletonCharacter` (Manny-compatible), `SkeletonArmy`, `BLACKMOON_BOSS_PACK1`, `YI_NPC` (UE4 skeletons), `SkeletonProps`. | Not part of `GASP-16`. |

## Scope

1. **Phase A – inventory and fit (asset project, no repository change).** Open `Assets_3DAssets` and measure:
   - Bone lengths: compare the reference poses of `SK_ma_meta_body_head_01`, the armor's `SKEL_metahuman_base` meshes and Knight_01 with `SKM_Manny_Simple` for each limb segment, hip height and hand and foot positions.
   - Skeleton fit: list the bones of each mesh that `SK_Mannequin` lacks, and the armor vertices weighted to MetaHuman-only bones. With Leader Pose, those bones stay in their reference pose.
   - How the pack hides the body: the body mask textures and material parameters, and how the sample BPs apply them.
   - Parts driven by physics or cloth (skirt, cape), level-of-detail meshes, material and texture budget.
   - Each pack's licence for `docs/third-party-notices.md`.

   Decision rule:
   - If the Paladin body and armor fit Manny within 2 cm per segment and the extra bones carry no visible weight, continue with Leader Pose.
   - Otherwise stop after Phase A. Present the fit, the cost of the runtime retarget path (which keeps the Paladin's proportions) and the Knight_01 fallback to the user before Phase B.
2. **Phase B – migrate the curated subset.**
   - Migrate the Paladin base body, hands, the materials and textures they need, the body masks, and one armor set: one helmet, chest, bracers, pants and boots.
   - Migrate one full Knight_01 mesh with its materials.
   - Cap the source data of every migrated texture at 2048 pixels (user decision), so the repository stores 2K data.
   - Skeletons:
     - The body and hands move to the UE5 mannequin skeleton that `ABP_Manny_PostProcess` targets (`Characters/GASP/Shared/Characters/UE5_Mannequins/Meshes/SK_Mannequin`), but only if that adds no bone.
     - Otherwise they keep `SKEL_metahuman_base` and get a copy of the post-process on that skeleton.
     - The armor keeps `SKEL_metahuman_base`.
     - The Lyra `SK_Mannequin` gets no bone.
   - Leave out demo content, `MH_Creator`, weapons and UE4 variants.
   - Record the licence (Fab Standard License) in `docs/third-party-notices.md`, plus the added LFS volume.
3. **Phase C – Character Parts (native, ported from Lyra).**
   - Port `ULyraPawnComponent_CharacterParts`, `ULyraControllerComponent_CharacterParts`, the part types and the body-style selection as `URpg...` types, close to 1:1. Also port `ALyraTaggedActor` as the base for part actors, because `IGameplayTagAssetInterface` cannot be implemented in Blueprint.
   - Keep Lyra's behaviour:
     - a FastArray list of parts on the pawn, changed only by the authority;
     - each non-dedicated machine spawns the part actors locally as child actor components on `GetMesh()`;
     - cosmetic tags are collected from the parts;
     - the body-style rules pick the gameplay mesh from those tags;
     - the controller component re-applies its parts when it possesses a new pawn.
   - Leave out the cosmetic cheats, developer settings and animation-layer selection.
   - Bridge to equipment: `URpgEquipmentDefinition` gains a list of character parts. `URpgEquipmentInstance` adds them to the pawn's parts component on equip (authority only), keeps the handles, and removes them on unequip and on teardown.
   - Native tests at the seams:
     - part list add and remove with handles;
     - collected tags and body-style selection;
     - equip and unequip adding and removing parts.
   - A PIE network test checks that a proxy and a late joiner spawn the same parts.
4. **Phase D – Paladin content.**
   - An invisible variant of the gameplay mesh, `SKM_Manny_Simple_GaspLook` with an invisible material (Lyra's `SKM_Manny_Invis` pattern). The pawn's body-style rules select it while a body tag is present. Without a body part the pawn looks as it does today.
   - The body part, a Blueprint on the tagged actor:
     - It carries the Paladin body and hands. A copy-pose AnimBP takes the pose from the attach parent, and the mesh's post-process drives the correctives.
     - It listens to the parts component's change event. It sets the skin mask textures for the collected `Cover` tags and hides the hands while gloves are present.
   - Armor part Blueprints bind Leader Pose to the body part's mesh, or to `GetMesh()` while no body part exists, and rebind on every parts change. Each piece carries its `Cover` tag.
   - Five Paladin armor items for `Head`, `Chest`, `Hands`, `Legs` and `Feet`, with equipment definitions that list their part. The existing `ID_BasicHelmet` gets the Paladin helmet part.
   - A pilot Experience on a test map copied from the approved `Lvl_RpgGaspMantle` presentation. It adds the pawn parts component to the CMC Mantle pawn and a controller parts component with the Paladin body, as Lyra's `B_PickRandomCharacter` does. The production Experiences stay unchanged.
   - Load weights and other armor balance values stay unchanged unless the user sets them.
5. **Phase E – Knight_01 comparison.**
   - Swap the controller's body part to a Knight_01 part in the same PIE session.
   - Compare fit, animation quality and cost with the Paladin.
   - If the Paladin fails in Phase D, Knight_01 becomes the pilot character after the user agrees.
6. **Phase F – validation and the user check.**
   - Network: equip and unequip on owner, listen server, proxy and late joiner.
   - Combat, block, hit reaction, death, corpse ragdoll and respawn with full armor.
   - Cost with one and four armored characters against Manny.
   - The user check. The user then decides whether the production CMC pawns adopt the Paladin.

## Out of scope

- A player-facing character editor, and enabling the MetaHuman plugins or `MH_Creator` assets. That is a later package (user decision 2026-10-18).
- Armor Foley and all other sound (user decision 2026-10-18).
- Switching the production CMC pawns or the Mover pawns. The Mover keeps UEFN and gets no parts component, so it shows no armor.
- Enemy characters and their retargeting (`SkeletonArmy`, `BLACKMOON`, `YI_NPC`, `SkeletonCharacter`, Paragon).
- Paladin weapons and shields as weapon items, armor stats, armor balance and new load values.
- Female body variants, hair and beard customization, and cosmetic animation-layer selection.

## Decisions

- 2026-10-18 (user): merge the definition and start Phase A.
- 2026-10-18 (user): the Polyphoria MetaHuman Paladin is the pilot player character, with Knight_01 as a comparison.
- 2026-10-18 (user): a character editor comes later. `GASP-16` builds one fixed Paladin and keeps the MetaHuman route open.
- 2026-10-18 (user): sound, including armor Foley, comes later because things may still change. This reverses the 2026-10-14 move of armor Foley into `GASP-16`.
- 2026-10-18 (user): adopt Lyra's Character Parts instead of armor actors spawned through `ActorsToSpawn`. The visible body copies the pose and drives its corrective bones; the armor follows the body by Leader Pose. This changes the C++ boundary below.
- 2026-10-18 (user): cap migrated textures at 2K.
- 2026-10-18 (user): the packs are under the Fab Standard License. The user believes most were bought with the Professional tier, although the listings show "Standard".
- Earlier, still in force: Manny (`SK_Mannequin`) is the gameplay and animation skeleton (`GASP-14`/`GASP-15`), and `SK_Mannequin` gets no bone or hierarchy change.

## C++ boundary

- **Classification:**
  - Character Parts is a reusable native mechanism: replicated state, authority-only changes, and the lifecycle of locally spawned part actors.
  - The equipment link is a schema extension of the equipment definition.
  - Everything Paladin-specific is designer content.
- **Runtime truth:**
  - Equipped armor is server-owned equipment state, as today.
  - The pawn's part list is server-owned and replicated as a FastArray; the controller's part list is server-owned.
  - Part actors, Leader Pose, the copy pose, correctives and body masks are cosmetic. Each non-dedicated machine derives them from the replicated list.
- **Existing seams:**
  - `URpgEquipmentDefinition` and `URpgEquipmentInstance` (equip and unequip lifecycle);
  - `UPawnComponent` / `UControllerComponent` from ModularGameplay;
  - Experience `AddComponents` actions;
  - `URpgPawnExtensionComponent::FindGameplayMesh`;
  - `FRpgWeaponBlockDefinition::SkeletonPresentations` (unchanged, because `GetMesh()` stays on `SK_Mannequin`);
  - `ABP_Manny_PostProcess` and `CR_Mannequin_Procedural` from the GASP content.
- **Ownership:**
  - C++: the parts components, the part types, the body-style selection, the tagged actor base, the equipment part list and its lifecycle.
  - Blueprint and assets: the part actors, invisible mesh, copy-pose AnimBP, items, equipment definitions, the controller parts Blueprint, and the pilot Experience and map.
- **New native classes:** three, ported from Lyra:
  - `URpgPawnComponent_CharacterParts`;
  - `URpgControllerComponent_CharacterParts`;
  - `ARpgTaggedActor`.

  The part types and the body-style selection come along as structs. Justification:
  - replication via FastArray and authority-only mutation;
  - client-local spawn and teardown lifecycle;
  - `IGameplayTagAssetInterface`, which Blueprint cannot implement.
- **Asset work:** migration with the asset project's MCP (port 8001), authoring through the Unreal MCP workflow (port 8000), compile and save, asset-registry closure checks, PIE probes.

## Ownership

| Concern | Owner |
| --- | --- |
| Equipped armor, slots, grants | Unchanged: equipment components on the server |
| Which parts a pawn wears | Native parts components; equipment adds armor parts, the controller adds the body |
| Part visuals, Leader Pose, copy pose, correctives | Part actor Blueprints and AnimBPs; cosmetic, per machine |
| Body masking under armor | Body part Blueprint from the collected `Cover` tags; cosmetic, per machine |
| Invisible gameplay mesh selection | Body-style rules on the pawn parts component |
| Pilot Experience, map, items | Designer content through the Unreal MCP workflow |
| Migrated third-party content | Curated subset, licence entry in `docs/third-party-notices.md`, manifest under `docs/assets` |
| Verification | Native tests at the parts and equipment seams, a PIE network test, asset contract checks; no C++ test per visual leaf |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Fit | Phase A reports bone-length differences, extra-bone weights and the chosen binding (done). |
| Character | The pilot pawn shows the Paladin body and plays locomotion, traversal, attacks, block, hit reaction and death natively on Manny animations. Weapon grips match Manny within 1 cm and 2°, or the user accepts the difference. |
| Armor | Each of the five pieces equips and unequips through the inventory, follows the body without visible lag, deforms with the body's correctives, and hides the body regions it covers. Removing one piece reveals only its own regions. |
| Network | Owner, listen server, proxy and late joiner see the same parts, armor and body masks after equip, unequip and respawn. A dedicated server spawns no part actors. |
| Death and ragdoll | Body and armor follow the corpse ragdoll and are gone or restored correctly after respawn. |
| Mover and production | The Mover and the production Experiences are unchanged; a pawn without a body part looks as today. |
| Comparison | Knight_01 is compared with the Paladin in the same session on fit, animation and cost. |
| Performance | Game-thread, render-thread and animation-worker cost and draw calls for one and four armored characters against Manny, in the same editor run. |
| Content | Only the curated subset is migrated, with textures at 2K or less. No remaining reference into the asset library or to demo content. Licence and LFS volume are recorded. |
| Regression | Native (including the new parts tests), animation, CMC, block, traversal, equipment, network and Mover suites pass apart from documented timing cases. |
| User check | The user plays the pilot map alone and in a listen-server session, equips and removes armor, fights and dies. |

## Verification

Build and test through `Build/Tools/Unreal/ue.py`. Run regressions as in `GASP-15`, plus the equipment and inventory suites. Raw evidence goes to `Saved/Gasp16`. This report, the PR and the manifest `docs/assets/gasp-16-character-armor-pilot.json` carry the summary.

## Repro (after implementation)

1. Open the pilot map and start PIE with its Experience.
2. Move, sprint, crouch, jump and traverse. Equip sword and shield: attack, block while walking and take a hit.
3. Equip the five Paladin armor pieces one by one, then remove them in a different order. Watch the body under each piece.
4. Die and respawn with full armor.
5. Repeat as a listen server with two clients, one of which joins late with armor already equipped on the others.

## Results

### Phase A – inventory and fit (2026-10-18)

Measured in the asset project `Assets_3DAssets` (UE 5.8.3, MCP port 8001) with a scratch measurement toolset that saves nothing. Raw evidence, scripts and images are in `Saved/Gasp16/PhaseA`. The repository is unchanged apart from this report.

**Proportions: an exact fit.**
- These meshes have the same component-space reference positions as the template `SKM_Manny_Simple` (0.0 cm at pelvis, spine, neck, head, clavicles, arms, hands, legs and feet):
  - the Paladin body `SK_ma_meta_body_head_01`;
  - the hands `SK_ma_meta_body_hands`;
  - every Paladin armor mesh;
  - the full Knight_01 meshes.
- The MetaHuman base body `SK_tal_nrw_base` deviates by at most 0.06 cm.
- Only Knight_01's `ball` bones sit 3.2 cm further back (foot to ball 13.8 cm instead of 16.8 cm). Under Leader Pose the toe of the Knight's boot stretches by that amount.
- Leader Pose therefore keeps every proportion. The runtime retarget path is not needed.

**Extra bones: the Paladin uses corrective bones heavily; Knight_01 does not.**

| Mesh | Bones | Extra bones vs. Lyra `SK_Mannequin` | Vertices mostly weighted to extra bones |
| --- | --- | --- | --- |
| Paladin body | 150 | 70 (UE5 Manny corrective and twist-corrective bones); lacks the `ik_*` and `interaction` bones | 116 of 7,726 (clavicle) |
| Paladin hands | 161 | 72 | 150 of 2,524 (wrist) |
| Chest `chest_heavy_05_c` | 109 | 66 | 9,061 of 13,256 (shoulder `upperarm_out/fwd/bck`, `upperarm_twistCor`) |
| Bracers `bracers_heavy_04_c` | 71 | 14 | 377 of 6,526 (wrist) |
| Pants `pants_01_e_v2` | 50 | 30 | 865 of 1,501 (`calf_twistCor`, `thigh_twistCor`) |
| Boots `shoe_heavy_04_e` | 20 | 6 | 2,768 of 5,648 (`calf_twistCor_02` at weight 1.0) |
| Helmet `helm_paladin_a_V2` | 161 | 72 | 0 |
| Knight_01 full, body, helmet | 161 | 72 | 0 |

- In UE 5.8 a Leader Pose follower bone that the leader lacks keeps its reference transform relative to its animated parent (`SkeletalRender.cpp`, `UpdateRefToLocalMatrices`). So these bones follow rigidly, and the armor loses its corrective deformation but does not tear.
- Polyphoria's own sample, `BP_Character`, drives the body with a Manny AnimBP and binds an armor part by `SetLeaderPoseComponent`. It does not drive the correctives either.
- Visual probe:
  - Setup: a Manny leader in idle, jog, two punches and a jump, with the Paladin set and Knight_01 following by Leader Pose (images in `Saved/Gasp16/PhaseA/shots*`).
  - Result: no tearing and no visible gaps at shoulders, elbows, hips or knees. The chest's tabard is a Chaos cloth asset that does not simulate in the editor probe.
- Driving the correctives would need a post-process AnimBP (copy pose plus corrective rig) on every follower. That is not part of the pilot.

**Skeletons.**
- The body's 150 bones are a subset of `SKEL_metahuman_base`. Body and armor can share that one skeleton, so no bone is added to `SK_Mannequin`.
- The hands and Knight_01 additionally carry the `ik_*`, `interaction`, `weapon_*` and `center_of_mass` bones. These are harmless for followers.

**Hiding the body.**
- The skin material `M_Skin_master` has the mask texture parameters `Hat Mask`, `UpperBody Mask`, `Hands Mask`, `LowerBody Mask`, `Feet Mask` and `Socks Mask`. They are white, so nothing is hidden by default.
- The pack ships matching masks in `Base/Textures/Metahuman_body_mask`: `T_mh_mask_headline`, `T_mh_mask_chest_full`, `T_mh_mask_suit_upper`, `T_mh_body_mask_upperbody_long_arms`, `T_mh_body_mask_gloves`, `T_mh_mask_legs` and `T_mh_body_mask_shoes`.
- These map one to one onto the `Head`, `Chest`, `Hands`, `Legs` and `Feet` slots. The probe hid the body with dynamic material instances and showed no skin poking through.
- Some parts also carry morph targets for this, such as `gloves_equipped` on the chest and `boot_high_equipped` on the pants.

**Content and budget.**

| Subset | Packages | On disk | Textures | Skeletal meshes | LODs |
| --- | --- | --- | --- | --- | --- |
| Paladin pilot: body, hands, helmet, chest, bracers, pants, boots, masks | 92 | 236 MB | 55, 223 MB | 7 | 1 each; about 38,000 vertices for the full outfit against 48,705 for `SKM_Manny_Simple` LOD 0 |
| Knight_01 `SKM_Knight_01_Full_01` | 23 | 316 MB | 16, 288 MB | 1 (95,078 vertices) | 1 |

- Both closures stay inside their pack, with no reference to demo content or other packs.
- The user chose a 2K texture cap. Licence: Fab Standard License (see [Decisions](#decisions)).

**Decision rule.**
- The proportions pass.
- The extra bones carry weight, which the rule does not allow. Their effect under Leader Pose was not visible in the probe.
- Follow-up check:
  - Every extra bone of the five armor pieces also exists in the Paladin body.
  - The body's bones equal the full UE5 `SKM_Manny`, without its `ik_*`, `interaction`, `weapon_*` and `center_of_mass` bones.
  - So a body that copies the pose and runs the UE5 mannequin post-process (Pose Driver and `CR_Mannequin_Procedural`) drives the correctives, and the armor inherits them by Leader Pose.
  - This is Lyra's own pattern. Its `B_Manny` is a character part with the full `SKM_Manny`, the copy-pose AnimBP `ABP_Mannequin_CopyPose`, and the post-process `ABP_Manny_PostProcess` on the mesh.
- Following the rule, the task stopped after Phase A for the user's decision. The user chose the Lyra Character Parts design with a corrective-driven body (see [Decisions](#decisions)).

### Phase C – Character Parts (2026-10-18, done before Phase B's content work)

The native port was built first, so that one editor build served both phases. Commit `e15edba5`.

- **New native classes:** three, all under `Source/SurvivalRpg/Cosmetics`:
  - `URpgPawnComponent_CharacterParts`;
  - `URpgControllerComponent_CharacterParts`;
  - `ARpgTaggedActor`.

  New structs:
  - the part types `FRpgCharacterPart` and `FRpgCharacterPartHandle`, plus the collision mode;
  - the FastArray list;
  - the body-style selection `FRpgAnimBodyStyleSelectionSet`.

  They were ported from Lyra close to 1:1, without the cosmetic cheats, developer settings and animation-layer selection.
- **Deviations from Lyra:**
  - An unconfigured body style returns no mesh and leaves the gameplay mesh alone. Lyra would set a null mesh.
  - The swap uses `SetSkeletalMesh(Mesh, false)`, so the running AnimBP instance and its linked layers (block) survive a body part that arrives after equip. Lyra re-initialises the pose.
- **Equipment bridge:**
  - `URpgEquipmentDefinition::CharacterParts`: data validation rejects a missing or replicating part class.
  - `URpgEquipmentInstance::AddCharacterParts` and `RemoveCharacterParts`, called by the equipment list next to `SpawnEquipmentActors` and `DestroyEquipmentActors` on the authority.
  - A pawn without a parts component wears nothing. This is how the Mover and the production pawns stay unchanged.
- **Tags:**
  - `Cosmetic.Body`;
  - `Cosmetic.Cover.Head`, `.UpperBody`, `.Hands`, `.Legs` and `.Feet`. `UpperBody` replaces the planned `Chest` because the skin material's mask is called `UpperBody Mask`.
- **Tests** (`SurvivalRpg.Cosmetics.CharacterParts.*`, 4 new):
  - part list add and remove with handles, attachment, collision and tag filtering;
  - body-style selection and the untouched-mesh rule;
  - equip and unequip adding and removing exactly the equipment's parts;
  - validation.

  `ue.py test SurvivalRpg.Cosmetics SurvivalRpg.Equipment --null-rhi`: 21 run, 21 passed, no warnings. `ue.py build`: passed, no warnings.
- **Still open for Phase D/F:** a PIE network test for proxies and late joiners, and the dedicated-server check. Both need the pilot content.

### Phase B – migrated content (2026-10-18)

**Result**
- 114 packages, 271.7 MB on disk:
  - `Content/SurvivalRpg/Characters/Polyphoria` (`Base` body, hands, skin materials and masks; `Paladin` helmet, chest, bracers, pants, boots, `SKEL_metahuman_base`), 166 MB;
  - `Plugins/GameFeatures/GF_Dev_Sandbox/Content/Characters/Knight_01` (one full mesh with materials), 94 MB.
- Manifest: [`docs/assets/gasp-16-character-armor-pilot.json`](assets/gasp-16-character-armor-pilot.json).
- No existing project asset changed.

**Steps**
1. Migrated with `AssetTools.migrate_packages` from the asset library editor. 127 files, 624 MB.
2. Moved the folders in the SurvivalRpg editor, then resaved every moved package so it stores the new paths.
3. Re-pointed the body, hands and Knight_01 mesh onto the GASP UE5 mannequin skeleton and physics asset by consolidation. The skeleton has 161 bones, which cover all of them.
4. Deleted the extras that the migration pulled in through soft references:
   - the MetaHuman base body and a second chest variant;
   - the pack copies of the mannequin skeleton and physics asset;
   - the Knight_01 demo mannequin;
   - an unreferenced chest colour variant with four textures, and the skirt physics asset.
5. Removed the old folders on disk with the editor closed.

A registry closure from the eight root meshes and the masks shows no unreferenced package and no dependency outside the two folders. All roots load with the expected skeleton.

**Further details**
- Textures: 25 resized to 2048 px and 52 already at or below it, so all 77 are capped at 2K. The Polyphoria textures were mostly 2K already; the Knight_01 textures were 4K.
- Licence: recorded in [`third-party-notices.md`](third-party-notices.md).
- Empty material slots: the body slot `M_teeth12` and the helmet slot `pasted__Fbx Default Material 1` have no material, as in the pack.

**Failed attempt (undone)**
- The first run consolidated the pack skeletons before moving the folders. `EditorAssetLibrary.rename_directory` then loaded the consolidation redirectors, which resolve to their target, and moved the project's GASP `SK_Mannequin` and `PA_Mannequin` into the new folders.
- Recovery: the editor was closed without saving, the two files were restored from Git, and the migration was repeated with the folders moved first. No reference to the GASP skeleton was saved in between.
- Lessons:
  - Never move folders that contain redirectors.
  - Never call `delete_asset` or `load_asset` on a redirector path.
  - A mesh's `skeleton` property is read-only in Python; consolidation is the scriptable route.
