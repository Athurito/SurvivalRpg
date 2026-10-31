# CMB-01: Weapon grips

Status: **Merged** on 2026-10-20 ([#274](https://github.com/Athurito/SurvivalRpg/pull/274) as `885e18f1`) at the user's request, without a user check; the user checks the grips together with CMB-02.

Task report for CMB-01 in the [combat roadmap](combat-roadmap.md). Sword, two-handed sword, shield, pickaxe and axe now sit in the hand the way the Sword and Shield pack authored them, on the player, the enemies and the Mover.

## Behavior

### Problem

The user reported on 2026-10-13 that the sword sits in the wrist and the shield is reversed. The cause:
- `ED_BasicSword`, `ED_BasicTwoHandedSword`, `ED_BasicShield`, `ED_Tool_Axe` and `ED_Tool_Pickaxe` attached their actors to the bare bones `hand_r` and `hand_l`.
- Every attach transform and every component transform in the weapon actors was identity. Nothing was tuned, neither on UEFN nor on Manny.
- The origin of `hand_r` is the wrist joint, so the sword's grip sat in the wrist. The shield had no rotation at all.

### Where the pack keeps its grip

The pack's demo skeleton has prop bones (`Sword`, `Shield` and three holder bones each) that our `SK_Mannequin` does not have. The clips animate them, so the weapon placement is part of the animation, and it was lost when the clips were re-pointed to our skeleton.

Measured headless in `Assets_Animations` (UE5 variants, `Sequence2`, 377 clips, seven samples each), relative to the hand bone:

| Prop | Clips | Location (cm) | Rotation (roll, pitch, yaw) |
| --- | --- | --- | --- |
| `Sword` to `hand_r` | ground combos 1–3, `Walk_Combat`, `Block_Loop`, `Hit_Combat_F`, `Dodge_Combat_F_0` (112 samples) | (−7.75, 1.42, 0.78) median | (−14.52°, 23.32°, −14.39°), spread 0.003° |
| `Shield` to `hand_l` | same clips | (8.01, 4.00, 1.36) median | (68.64°, 82.83°, −123.62°), spread 0.02° |

- In every combat clip the props are rigid in the hand. The location wobbles by up to about 7 cm in a few attack frames. `Idle_Combat` holds the sword turned by about 18°.
- The clips without "Combat" in their name (`Idle`, `Walk_Loop`, `Run_Loop`, `Hit_F`, …) carry the weapons on the back through the holder bones.
- The pack's sword and shield meshes are the ones our actors already use (`Sword01`, `Shield`). Their pivots sit at the grip, the blade points along +Z.
- Our skeleton's `weapon_r` bone has the same hand-relative transform as the pack's, so the hand frames match.
- The second sword pack (`Sword_Animations`) uses its own prop bone `weapon_r1` at (−9.01, 2.98, 0.59) cm, (−92.7°, 4.0°, −168.1°). Its clips are not used yet.

### Change

Three skeleton sockets carry the grip, so the equipment definitions stay skeleton-free:

| Socket | Bone | Location (cm) | Rotation (roll, pitch, yaw) | Source |
| --- | --- | --- | --- | --- |
| `Grip_OneHand_R` | `hand_r` | (−7.7501, 1.4244, 0.7783) | (−14.5208°, 23.3157°, −14.3911°) | measured |
| `Grip_OneHand_L` | `hand_l` | (7.7501, −1.4244, −0.7783) | (−14.5208°, 23.3157°, −14.3911°) | mirrored like `weapon_l`, not measured |
| `Grip_Shield_L` | `hand_l` | (8.0133, 4.0002, 1.3569) | (68.6368°, 82.8297°, −123.6243°) | measured |

- They sit on both gameplay skeletons: the Lyra `SK_Mannequin` (CMC pawns, the Paladin's invisible gameplay mesh, Manny enemies) and `SK_UEFN_Mannequin` (Mover pawns). The hand axes of both match within 1° ([Manny gameplay mesh](gasp-manny-gameplay-mesh.md)).
- `ED_BasicSword`, `ED_Tool_Axe` and `ED_Tool_Pickaxe`: `Grip_OneHand_R`, off hand `Grip_OneHand_L`.
- `ED_BasicTwoHandedSword`: `Grip_OneHand_R`. Its attack is still the placeholder `MM_Attack_01`; a real two-handed grip comes with two-handed clips.
- `ED_BasicShield`: `Grip_Shield_L` for every slot.
- Attach transforms stay identity. Weapon and tool meshes keep their pivot at the grip like the sword, with the handle or blade along +Z and the edge toward +X; a migrated mesh that differs is re-pivoted together with its sockets ([asset library migration](../.agents/skills/unreal-lyra-expert/references/asset-library-migration.md#props-and-weapons)). An item-specific grip correction lives in the relative transform of the actor's mesh component, never in a per-item attach transform or skeleton socket. HRW-04c re-pivoted the axe and pickaxe this way.
- The sword's trace sockets live on the sword mesh, so the attack trace now follows the blade where the animation puts it.

New contract test `SurvivalRpg.Combat.EquipmentAttachSocketContract`: every attach name any project equipment definition authors must exist as a socket or bone on both gameplay skeletons. A missing socket would otherwise attach silently to the mesh origin.

### Skeleton GUID

The handoff forbids bone and hierarchy changes on `SK_Mannequin`, because a new skeleton GUID re-conforms about 1,800 assets on every load.
- Engine source: `USkeletalMesh::AddSocket` only adds to `USkeleton::Sockets`. `RegenerateGuid` runs only for new or duplicated skeletons, old versions, and bone-tree changes.
- Check: after the change the editor restarted with `LogAnimation Verbose` and loaded 3,083 dependent animation assets (combat packs, GASP Manny, UEFN, Lyra). Only 5 logged "Needed to reset skeleton". These are the GASP-15 Manny block montages (`AM_Block_*_Seq`, `AM_Parry_R_Seq` under `BlockLocomotion/SwordShield/Manny/Montages`), duplicated from UEFN montages, and they reset on every load independent of this task. A GUID change would have hit all of them.

## C++ boundary decision

- **Classification:** designer content (sockets, equipment definitions) plus one stable asset contract test.
- **Runtime truth:** unchanged. The server spawns the equipment actors and `URpgEquipmentInstance` attaches them to `GetMesh()` at `FRpgEquipmentActorToSpawn::GetAttachSocketForSlot`.
- **Existing seam:** the per-slot attach sockets of `FRpgEquipmentActorToSpawn`.
- **New native classes:** none. The test lives in `SurvivalRpgEditor`.
- **Asset work:** sockets added through `USkeletalMesh::AddSocket` with promotion to the skeleton and renamed through `SkeletalMeshEditorSubsystem`; the mesh copies were discarded, so only the skeletons changed. Definitions edited on their class defaults, compiled and saved through the editor.

## Validation (2026-10-20, on `master` `72bae714` plus this change)

- **Build:** `ue.py build` passed (0 errors, 0 warnings for the new test file).
- **Contract:** `SurvivalRpg.Combat.EquipmentAttachSocketContract` passed and checked 14 attach names on both skeletons. With the `master` version of `SK_UEFN_Mannequin` swapped in, it failed on every `Grip_*` name as intended; the file was restored afterwards.
- **Regression**, through `ue.py test` with the test editor's MCP server off:

  | Suite | Result |
  | --- | --- |
  | `SurvivalRpg.Combat` (incl. block locomotion, attack and starter-equipment contracts) | 35 / 35 |
  | `SurvivalRpg.GASP.Mover.Gameplay`, `SurvivalRpg.GASP.Mover.Lifecycle`, `SurvivalRpg.GASP.CMC` (`np.ForceReconcile 0`) | 22 / 22 |
  | `SurvivalRpg.Equipment`, `SurvivalRpg.Cosmetics` | 24 / 24 |
  | `SurvivalRpg.Harvesting` | 60 / 60 |
  | `SurvivalRpg.Network` (incl. `CombatRemoteMeleePIE`) | 23 / 23 |

  The warnings are the known PIE network noise (NetGUID, voice interface).
- **Remote melee:** `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation` traces along the sword's sockets, which now sit where the animation puts the blade. It passed in the suite run and in three more runs on its own (4 / 4). Before, it passed 2 of 4 and 2 of 5 runs ([test floor race](gasp-test-floor-race.md)); four runs do not close that open finding.
- **PIE, standalone, `Lvl_RpgGaspCMC` with the Paladin:** idle and held block, without armor and with all five Paladin pieces added through the pawn parts component; a pickaxe attached to `Grip_OneHand_R` by hand. Sword grip in the closed fist, shield on the forearm with its face out, gauntlet and bracer around the grip without visible clipping.
- **PIE, listen server with two clients:** on the server and both clients, every character (authority, autonomous and simulated proxies) has its sword at `Grip_OneHand_R` and its shield at `Grip_Shield_L`, 0.000 cm from the socket.

**Failed attempts**
- Socket names cannot be set from Python (`SocketName` and `BoneName` are read-only, also through `ToolsetLibrary.set_object_properties`). The engine toolset's workaround works: add an unnamed socket, then rename it through `SkeletalMeshEditorSubsystem.rename_socket`, which renames the skeleton copy too.
- The first editor session disappeared during its first PIE start, without a crash report or unsaved changes; the other session's editor restarted in the same window of time. After a relaunch everything worked.
- Mid-attack captures from the front did not work: the character turned away every time. The attack uses the same grip, which the pack holds rigid in every combat clip.

## Playtest

1. Open `Lvl_RpgGaspCMC` and start PIE. The player is the Paladin with sword and shield.
2. Stand still and orbit the camera to the front: the right fist closes on the sword's grip with the crossguard above it, the shield sits on the left forearm with the cross facing out.
3. Hold block: the shield is raised with its face to the front, the sword held low at the side.
4. Attack a few times and watch the blade follow the swing.
5. On `Lvl_HarvestPickaxe`, equip the pickaxe or axe: the handle lies in the right hand.
6. Multiplayer: start a listen server with two clients. Every player sees all three characters' swords and shields in the same place.

## Limits

- **Open hands at rest.** The relaxed GASP idle and locomotion keep the hands half open, so the fingers do not fully close around a sword or tool handle outside combat montages. The weapon still clips into the body while running. Both belong to CMB-02 (armed locomotion).
- **Grip wobble.** In a few attack frames the pack moves the sword up to about 7 cm inside the hand; the fixed socket ignores that.
- **Left hand.** `Grip_OneHand_L` is mirrored, not measured; no clip uses a sword in the left hand yet.
- **Two-handed sword.** It uses the one-handed grip and a placeholder attack.
- **Enemies.** The Rift Grunt still plays `ABP_Unarmed`, whose hands do not hold weapons (CMB-05).
- **Not checked visually:** the Mover's UEFN mesh, the axe, the plain Manny without the Paladin action set, and the attack swing from the front. The Mover suites only prove that its sockets exist and the actors attach.
- The five GASP-15 Manny block montages reset their skeleton on every load (see above); resaving them is outside this task.
