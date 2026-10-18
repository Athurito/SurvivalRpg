# GASP-16 – Character and armor pilot

Status: **Defined** 2026-10-18, not yet **Ready**. The user added character packs to the asset library on 2026-10-18 and chose the pilot character and the character-editor scope (see [Decisions](#decisions)). The user marks the package **Ready** before implementation starts. This report defines the package and becomes its task report.

## Goal

A marketplace player character runs on the Manny gameplay skeleton from `GASP-14`/`GASP-15`: the Polyphoria MetaHuman Paladin with one modular armor set. The armor pieces are equipment that follow the gameplay mesh by Leader Pose and hide the body parts they cover. This works on every network role, including late join, death and ragdoll. Knight_01 runs as a comparison and fallback. The MetaHuman route to a later character editor stays open.

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
   - Migrate the Paladin base body, hands, the materials and textures they need, and one armor set: one helmet, chest, bracers, pants and boots.
   - Migrate one full Knight_01 mesh with its materials.
   - Re-point the base body to the Lyra `SK_Mannequin` only if that adds no bone (the "muscle_head_v3" name needs checking). Otherwise the body keeps its own skeleton and follows by Leader Pose.
   - The armor keeps `SKEL_metahuman_base`, because adding the MetaHuman bones to `SK_Mannequin` is ruled out.
   - Leave out demo content, `MH_Creator`, weapons and UE4 variants.
   - Record each pack's licence and the added LFS volume. Cap textures above 2K only with the user's agreement.
3. **Phase C – Paladin pilot pawn.**
   - Add a child of `BP_RpgGasp_MantleCharacter` whose `GetMesh()` is the Paladin base body, with `ABP_RpgGasp_CMC_Manny` and a matching physics asset.
   - Give it a PawnData and an Experience on a test map copied from the approved `Lvl_RpgGaspMantle` presentation.
   - The gameplay mesh stays on `SK_Mannequin` or plays the Manny AnimBP as a compatible skeleton, so that montages, traversal and the Manny block presentation work unchanged.
   - Weapon grips are checked against Manny.
4. **Phase D – armor as equipment.**
   - Add Paladin armor items for `Head`, `Chest`, `Hands`, `Legs` and `Feet`, each with an equipment definition. Each definition spawns an armor actor through `ActorsToSpawn`. The existing `ID_BasicHelmet` gets the Paladin helmet.
   - The armor actor (Blueprint) binds its skeletal mesh to the attach parent by Leader Pose on every machine and unbinds on removal.
   - A Blueprint appearance component on the pilot pawn collects the body regions that the active armor hides and sets the body material masks. Removing one piece reveals only regions that no other piece still covers.
   - On a gameplay mesh whose skeleton the armor does not fit (the UEFN Mover), the armor actor stays hidden instead of showing a distorted mesh.
   - Load weights and other armor balance values stay unchanged unless the user sets them.
5. **Phase E – Knight_01 comparison.**
   - Swap the pilot pawn's mesh to the full Knight_01 mesh in the same PIE session, as the `GASP-15` skeleton A/B did, without a separate pawn.
   - Compare fit, animation quality and cost with the Paladin.
   - If Phase A or D fails for the Paladin, Knight_01 becomes the pilot character after the user agrees.
6. **Phase F – validation and the user check.**
   - Network: equip and unequip on owner, listen server, proxy and late joiner.
   - Combat, block, hit reaction, death, corpse ragdoll and respawn with full armor.
   - Cost with one and four armored characters against Manny.
   - The user check. The user then decides whether the production CMC pawns adopt the Paladin.

## Out of scope

- A player-facing character editor, and enabling the MetaHuman plugins or `MH_Creator` assets. That is a later package (user decision 2026-10-18).
- Armor Foley and all other sound (user decision 2026-10-18).
- Switching the production CMC pawns or the Mover pawns. The Mover keeps UEFN and shows no armor.
- Enemy characters and their retargeting (`SkeletonArmy`, `BLACKMOON`, `YI_NPC`, `SkeletonCharacter`, Paragon).
- Paladin weapons and shields as weapon items, armor stats, armor balance and new load values.
- Female body variants, hair and beard customization.

## Decisions

- 2026-10-18 (user): the Polyphoria MetaHuman Paladin is the pilot player character, with Knight_01 as a comparison.
- 2026-10-18 (user): a character editor comes later. `GASP-16` builds one fixed Paladin and keeps the MetaHuman route open.
- 2026-10-18 (user): sound, including armor Foley, comes later because things may still change. This reverses the 2026-10-14 move of armor Foley into `GASP-16`.
- Earlier, still in force: Manny (`SK_Mannequin`) is the gameplay and animation skeleton (`GASP-14`/`GASP-15`), and `SK_Mannequin` gets no bone or hierarchy change.

## C++ boundary

- **Classification:** designer-owned content and composition on existing mechanisms.
- **Runtime truth:**
  - Equipped armor is server-owned equipment state, as today.
  - The armor actors are replicated, spawned by the server through `ActorsToSpawn`.
  - Leader Pose binding and body masks are cosmetic, and each machine derives them from the replicated actors.
- **Existing seams:**
  - `URpgEquipmentDefinition::ActorsToSpawn` and `URpgEquipmentInstance::SpawnEquipmentActors`;
  - `URpgPawnExtensionComponent::FindGameplayMesh`;
  - `FRpgWeaponBlockDefinition::SkeletonPresentations`;
  - the armor equipment slots;
  - `URpgRuntimeRetargetComponent`, only if Phase A requires it.
- **Ownership:**
  - Blueprint and assets: armor actor, appearance component, item and equipment definitions, pilot pawn, PawnData, Experience and map.
  - C++: nothing new.
- **New native classes:** none expected. One exception is possible. If Blueprint cannot bind Leader Pose reliably when a late joiner receives the armor actor before the pawn's mesh, the smallest native seam would be a reusable component that rebinds on attachment replication. The report would record that need.
- **Asset work:** migration with the asset project's MCP (port 8001), authoring through the Unreal MCP workflow (port 8000), compile and save, asset-registry closure checks, PIE probes.

## Ownership

| Concern | Owner |
| --- | --- |
| Equipped armor, slots, grants | Unchanged: equipment components on the server |
| Armor visuals | Armor actor Blueprint per piece, spawned by the equipment definition |
| Body masking under armor | Blueprint appearance component on the pawn; cosmetic, per machine |
| Character mesh, physics asset, pilot pawn, PawnData, Experience, map | Designer content through the Unreal MCP workflow |
| Migrated third-party content | Curated subset, licence entry in `docs/third-party-notices.md`, manifest under `docs/assets` |
| Verification | PIE network tests and probes, asset contract checks; no C++ test per visual leaf |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Fit | Phase A reports bone-length differences, extra-bone weights and the chosen binding (Leader Pose or retarget), with the decision rule applied. |
| Character | The pilot pawn shows the Paladin body and plays locomotion, traversal, attacks, block, hit reaction and death natively on Manny animations. Weapon grips match Manny within 1 cm and 2°, or the user accepts the difference. |
| Armor | Each of the five pieces equips and unequips through the inventory, follows the body without visible lag, and hides the body regions it covers. Removing one piece reveals only its own regions. |
| Network | Owner, listen server, proxy and late joiner see the same armor and body masks after equip, unequip and respawn. |
| Death and ragdoll | Armor follows the corpse ragdoll and is gone or restored correctly after respawn. |
| Mover | The Mover pawn with armor equipped shows no distorted armor. |
| Comparison | Knight_01 is compared with the Paladin in the same session on fit, animation and cost. |
| Performance | Game-thread, render-thread and animation-worker cost and draw calls for one and four armored characters against Manny, in the same editor run. |
| Content | Only the curated subset is migrated. No remaining reference into the asset library or to demo content. Licences and LFS volume are recorded. |
| Regression | Native, animation, CMC, block, traversal, equipment, network and Mover suites pass apart from documented timing cases. |
| User check | The user plays the pilot map alone and in a listen-server session, equips and removes armor, fights and dies. |

## Verification

Build and test through `Build/Tools/Unreal/ue.py`. Run regressions as in `GASP-15`, plus the equipment and inventory suites. Raw evidence goes to `Saved/Gasp16`. This report, the PR and the manifest `docs/assets/gasp-16-character-armor-pilot.json` carry the summary.

## Repro (after implementation)

1. Open the pilot map and start PIE with its Experience.
2. Move, sprint, crouch, jump and traverse. Equip sword and shield: attack, block while walking and take a hit.
3. Equip the five Paladin armor pieces one by one, then remove them in a different order. Watch the body under each piece.
4. Die and respawn with full armor.
5. Repeat as a listen server with two clients, one of which joins late with armor already equipped on the others.
