# CMB-05: Skeleton warrior

Task report for CMB-05 in the [combat roadmap](combat-roadmap.md), [PR #292](https://github.com/Athurito/SurvivalRpg/pull/292). The Rift Grunt (`BP_RiftGrunt_Sword`) is now the skeleton warrior from the asset library's `SkeletonCharacter` pack: a Manny-native enemy with sword and shield in the CMB-01 grips, a sword-and-shield combat AnimBP, and its own stun, knockback and death clips, chosen by the attacker's side. It dies into a ragdoll on its own copy of the Manny physics asset, because the pack's own one was unstable. AI behavior is unchanged and belongs to CMB-06.

## C++ boundary decision

- **Classification:** a small schema extension of the existing stagger and death abilities and of PawnData, plus designer content (migrated mesh and clips, montages, AnimBP, blend space, DataAssets, enemy Blueprint).
- **Runtime truth:** unchanged. The server's stagger meter, `GA_Combat_Stagger` and `GA_Combat_Death` own stagger, guard break, death and the corpse lifecycle; the equipment manager owns the weapons. Montages, the AnimBP and the ragdoll pose are presentation; the ability system replicates the montages.
- **Existing seams:** `URpgGameplayAbility_Stagger` already played a native montage for its authoritative duration, `GA_Combat_Death` played one fixed montage in its graph, PawnData selects each pawn type, `FRpgWeaponBlockDefinition` links the block layer into the gameplay mesh's AnimBP, and the enemy loadouts equip by archetype tag.
- **Native, and why:**
  - **Problem:** both abilities are shared by every pawn, so a montage on the ability cannot be the skeleton's and the player's at once, and the stagger picks its montage natively.
  - **Schema:** `URpgCombatReactionProfile`, a DataAsset with front and back montages for stagger, guard break and death, and `URpgPawnData::CombatReactionProfile`, the reference per pawn type. A reusable concept every later enemy type needs; no concrete montage, tag or tuning is native.
  - **Mechanism:** the stagger ability and a new `GetDeathMontage(EventData)` of the death ability resolve the profile on the server, choose front or back from the attacker's position (the effect causer, else the instigator, resolved from a player state or controller to its pawn), and fall back to the ability's own montages. The stagger ability now plays its `GuardBreakMontage` only on a guard break; it used to prefer it for every stagger, but no ability set one.
  - **New native classes:** one DataAsset class. No GameplayAbility or AnimInstance leaf.
- **Content:** everything else is assets authored through Unreal MCP (see [Content](#content)).
- **Tests:** `SurvivalRpg.Combat.ReactionProfile.*` cover the side and fallback rules once, plus one stable asset contract: every PawnData's reaction montages target its gameplay mesh's skeleton.

## Skeleton check

Measured headless in the asset library and in the editor (`SK_Mannequin` is the Lyra skeleton at `Characters/Mannequins/Meshes`):

| Check | Result |
| --- | --- |
| Bones | `SK_Skeleton` and `SKM_SkeletonCharacter_A` have the same 89 bones and parents as `SKM_Manny_Simple`, in another order; every bone exists in `SK_Mannequin` (161 bones) |
| Proportions | About 3 % taller: pelvis 98.7 instead of 95.9 cm, calf 45.8 instead of 43.3 cm, shoulder offset of the upper arm 15.3 instead of 17.8 cm |
| Reference pose | Hands rotated by 9°, fingers up to 14°, spine and calves up to 4° |
| Retarget modes | `SK_Mannequin` scales translations (`AnimationScaled` on 145 bones), so Manny clips keep the warrior's proportions |
| Clips | 48, all in place (root at the origin), no root motion, no notifies |

Decision: re-point the mesh and the clips to `SK_Mannequin` instead of retargeting. The warrior then uses the Grip sockets, the `HitFlinch` slot and the Sword and Shield clips directly. No bone was added, and `SK_Mannequin` itself is unchanged.

## Behavior

### Problem

The Rift Grunt was a plain `SKM_Manny_Simple` with the template `ABP_Unarmed`: an unarmed blend space, no block layer, so the hands did not hold the sword. Its loadout gave it a sword only. Stagger played no animation, and every pawn died with `MM_Death_Front_01`.

### Change

- **Look:** `BP_RiftGrunt_Sword` uses `SKM_SkeletonCharacter_A` with its base material at 2K. Sword and shield attach to `Grip_OneHand_R` and `Grip_Shield_L` like on the player.
- **Loadout:** the warrior's archetype is `Enemy.Archetype.SwordShield`, so the GameFeature loadout component equips sword and shield (`DA_EnemyLoadout_SwordShield`).
- **Combat AnimBP** `ABP_Enemy_SwordShield` (a copy of `ABP_Unarmed` on `URpgAnimInstance`, in `GF_Combat_Core`):
  - Locomotion from `BS_Enemy_SwordShield_Locomotion`: `Idle_Combat` at rest, `Walk_Combat` and `Run_Combat` loops in eight directions at their measured speeds (160 to 177 and 397 to 415 cm/s), and the fast run forward at 750 cm/s.
  - Armed jump, fall and land from the pack's `Jump_Combat_*_0` clips.
  - The `ALI_RpgBlockLocomotion` layer before `DefaultSlot`, as in the player's GASP AnimBP, so the shield's block layer links. Without a linked layer it passes the pose through.
  - Kept from `ABP_Unarmed`: `DefaultSlot`, the `UpperBody` slot, foot IK and the additive `HitFlinch` slot from CMB-04.
- **Reactions** (`DA_SkeletonWarrior_CombatReactions` on `DA_RiftGrunt_Sword_PawnData`):

  | Reaction | Attacker in front | Attacker behind |
  | --- | --- | --- |
  | Stagger | `ANI_StunA`, a dazed stoop (3.7 s) | `ANI_Knock_FromBack`, a stumble (2.0 s) |
  | Guard break | `ANI_Knock_Back`, a stumble (2.0 s) | as in front |
  | Death | `ANI_Death_A`, falls backward | `ANI_Death_B`, falls forward |

  The stagger ability ends the montage after the stagger duration with a 0.3 s blend. The death montages hold their last pose until the ragdoll.
- **Stagger duration:** the warrior's own `DA_SkeletonWarrior_DefenseProfile` is the basic enemy profile with a 2.0 s stagger instead of 0.7 s, so the daze is readable. Meter (200) and immunity (2 s) are unchanged.
- **Death and ragdoll:** `GA_Combat_Death` now plays `GetDeathMontage(EventData)`; its `DeathMontage` default is the previous `MM_Death_Front_01_Montage`, so the player is unchanged. After the existing 2 s delay the corpse ragdolls on `PA_SkeletonWarrior`.
- **Physics asset:** `PA_SkeletonWarrior` is a copy of the project's `PA_Mannequin`. The pack's `PA_Skeleton`, itself a modified copy of Epic's Manny asset, launched the corpse from the lying death pose in every run (see [Validation](#validation)), so it was not kept.
- **Shared clips:** `force_root_lock` is on for the 17 Sword and Shield loops in the blend space; they move their root bone and would slide the mesh off the capsule. One of them, `Walk_Combat_Loop_F_0`, is also read by the CMB-02 hold layers, which blend only the upper body.

### Content

| Asset | Role |
| --- | --- |
| `GF_AI_RiftMonsters/Characters/SkeletonWarrior/Meshes/SKM_SkeletonCharacter_A`, `LODSettings_SkeletonCharacter`, `PA_SkeletonWarrior` | Mesh on `SK_Mannequin`, its LOD settings, ragdoll physics asset |
| `.../Materials/MM_Skeleton_Basic`, `MI_Skeleton_Base_2k`; `.../Textures/T_Skeleton_Base_2k_D/M/N`, `T_DetailNormal` | Material; the master's texture defaults moved from the pack's 4K to its 2K textures |
| `.../Animations/ANI_StunA`, `ANI_Knock_Back`, `ANI_Knock_FromBack`, `ANI_Death_A`, `ANI_Death_B` | Pack clips on `SK_Mannequin`, retarget source the warrior mesh |
| `.../Animations/Montages/AM_SkeletonWarrior_Stagger_Front`, `_Stagger_Back`, `_GuardBreak`, `_Death_Front`, `_Death_Back` | One section each, `DefaultSlot`; blend in 0.1 to 0.15 s, out 0.3 s; death montages without auto blend-out (blend out 0.25 s) |
| `.../DA_SkeletonWarrior_CombatReactions`, `DA_SkeletonWarrior_DefenseProfile` | Reaction profile; defense tuning with a 2.0 s stagger |
| `GF_Combat_Core/Animations/Enemy/ABP_Enemy_SwordShield`, `BS_Enemy_SwordShield_Locomotion` | Combat AnimBP and its 2D blend space (direction −180 to 180 with wrap, speed 0 to 750 cm/s, 28 samples) |
| `GF_AI_RiftMonsters/AI/Characters/BP_RiftGrunt_Sword` | Mesh, AnimBP, archetype `SwordShield` on both loadout and archetype components, defense profile |
| `GF_AI_RiftMonsters/AI/PawnData/DA_RiftGrunt_Sword_PawnData` | `CombatReactionProfile` |
| `GF_Combat_Core/GAS/Abilities/GA_Combat_Death` | `DeathMontage` default; the graph plays `GetDeathMontage(EventData)` |
| 17 `Sword_and_Shield/.../Sequence2` `Walk_Combat_Loop_*`, `Run_Combat_Loop_*`, `Run_Fast_Combat_Loop` | `force_root_lock` |

Left in the asset library: the pack skeleton and physics asset, mesh variants B to D, the weapons, the other 43 clips and the 4K textures. Notice: [third-party notices](third-party-notices.md).

## Validation

Run on 2026-10-26 on `master` `2e9961b9` plus this change.

- **Build:** `ue.py build` passed with the native change, 0 errors, 0 warnings.
- **Assets:**
  - Data validation of the new and changed assets plus the derived `BP_RiftBoss_Prototype`: 44 valid, 0 warnings.
  - After an editor restart no clip logged a skeleton reset. `SK_Mannequin` is unchanged on disk, so its GUID is too.
- **New tests:** `SurvivalRpg.Combat.ReactionProfile.SelectsBySideWithFrontFallback`, `...AttackerSideUsesPlanarFacing` and `...MontagesMatchPawnSkeleton` passed, alone and in the suite; the contract checked 5 reaction montages.
- **Regression** (`ue.py test`, own editor closed, MCP server off):

  | Suite | Result |
  | --- | --- |
  | `SurvivalRpg.Combat`, `.AI`, `.Equipment`, `.Animation`, `.Cosmetics` (`--null-rhi`) | 75 / 75 |
  | `SurvivalRpg.GASP.CMC` (`np.ForceReconcile 0`) | 14 / 14 |
  | `SurvivalRpg.Network` | 27 / 28 |

  - **The failure:** `CombatImpactCuesPIE.TwoRemoteAttackersHitOneTargetAndEachHitPlaysOnceEverywhere` (CMB-04) timed out in "Round 0: each authority attack lands exactly one hit". Alone it failed once and passed once.
  - **Why it is unlikely to be this change:** its target is the network test fixture without PawnData, so the stagger takes the unchanged fallback, and it uses none of the changed assets. The failure was not reproduced on `master`, so it stays open.
  - `SurvivalRpg.GASP.Mover` was not run; no Mover content changed, and the shared abilities take the unchanged fallback there.
- **PIE, standalone and Simulate, `Lvl_RpgGaspCMC` with a placed warrior:**
  - **Look:** front captures in the stance and in the stun: sword in the right fist, shield on the left forearm, feet on the floor.
  - **AI:** in the first run the warrior chased and killed the player with the sword combo. The attack driver kept attacking with the StateTree stopped.
  - **Stagger** (stagger damage from the warrior itself, so from the front): `AM_SkeletonWarrior_Stagger_Front`, ended after 2.0 s; a second meter break during the 2 s immunity started nothing.
  - **Attacker behind or blocking** (the player's ability system as source): `AM_SkeletonWarrior_Stagger_Back` from behind, `AM_SkeletonWarrior_GuardBreak` with `State.Blocking`, `AM_SkeletonWarrior_Death_Back` from behind; from the front the death is `_Death_Front`, a fall onto the back.
  - **Ragdoll from the front death pose**, pelvis travel after the ragdoll start:

    | Physics asset | Runs | Pelvis travel |
    | --- | --- | --- |
    | The pack's `PA_Skeleton` as migrated, with Manny's limb shapes, or with collision disabled on all constraints | 9 | 0.6 to 6.6 m; in six runs the head rose by 48 to 126 cm |
    | `PA_Mannequin` (override) and `PA_SkeletonWarrior` | 6 | 15 to 19 cm |
- **PIE, listen server with two clients in one process:**
  - All three machines show the skeleton mesh, `ABP_Enemy_SwordShield`, sword and shield at `Grip_OneHand_R` and `Grip_Shield_L`, and `PA_SkeletonWarrior`.
  - **Stagger:** the server's meter break interrupted the warrior's attack. Server and both simulated proxies played `AM_SkeletonWarrior_Stagger_Front` at 0.4, 0.8 and 1.5 s with the pelvis at the same position; after 2 s it attacked again on all three.
  - **Death:** `AM_SkeletonWarrior_Death_Front` on all three at 0.8 s. At 3.8 s all three ragdolled; the corpses lay within 9 cm of each other and had not moved at 5.8 s. The weapons were unequipped.
  - **Late join:** the net-cull trick did not drop the corpse on the clients within 7 s, so a late joiner was not checked.
- **Failed attempts:**
  - The first front stagger chained the pack's `ANI_HitReact_FrontA` into the stun in one montage. Its sections cannot be linked, so it stopped after the hit reaction; it is now the stun clip alone.
  - The first blend-space write was rejected: the fast run's 750.2 cm/s lay outside the 750 axis.
  - With only `LoadoutDefinitions` switched, the warrior still got the sword alone: both loadout components pick by archetype tag, which stayed `BasicSword`. The placed test actor kept the old tag until it was respawned.
  - A probe ran `god` for a remote client's controller on the listen server and crashed the editor; nothing unsaved was lost ([editor sessions](../.agents/skills/unreal-lyra-expert/references/editor-session-and-pie.md)).

## Playtest

1. Open `Lvl_RpgGaspCMC`, place a `BP_RiftGrunt_Sword` (`GF_AI_RiftMonsters/AI/Characters`) in front of a player start, and start PIE.
2. The enemy is a skeleton in the sword-and-shield stance, the sword in its right fist and the shield on its left forearm. It walks and runs at you with the armed loops and attacks with the sword combo.
3. Break its stagger meter from the front: it stoops and sways, dazed, for about 2 s, then fights on. From behind it stumbles instead.
4. Break it while it holds its shield up: it stumbles with the knockback clip (guard break).
5. Kill it from the front: it falls on its back; from behind it falls forward. After about 2 s it goes limp and settles without sliding away.
6. Listen server with two clients: both clients see the same skeleton, weapons, stagger and death animations, and the corpse lying where it fell.

## Limits

- **AI unchanged:** the warrior still runs `ST_RiftGrunt_ChaseAndAttack`. Spacing, telegraphs, retreat and when it blocks belong to CMB-06, and so does tuning its move speed: the AI runs at 600 cm/s, between the run loops (about 410) and the fast run (750, forward only), so its feet slide a little in fast strafes.
- **Turning in place:** the AnimBP has no turn-in-place; the controller rotates the capsule, so the feet slide when it turns on the spot.
- **No looping sections:** montage sections cannot be linked through MCP ([authoring reference](../.agents/skills/unreal-lyra-expert/references/unreal-mcp-asset-authoring.md)). A stagger longer than its clip (3.7 s in front, 2.0 s behind or on a guard break) returns to locomotion before the ability ends. The hit itself still shows the CMB-04 flinch; the stun clip has no separate impact.
- **Pose offset of the pack clips:** the pose tool reports the five pack clips about 2.5 cm higher on the warrior mesh than in the pack; the feet stay on the floor in the captures.
- **Physics asset:** `PA_SkeletonWarrior` has Manny's body sizes, which are wider than the bones. Why the pack's own asset fails is not proven; the body-pair collision table, which no tool reads, is the remaining suspect. Its editor preview mesh is `SKM_Manny_Simple`.
- **Coverage:**
  - Only mesh variant A is migrated.
  - The grips were checked in the idle stance and the reaction poses, not frame by frame in the attack swings.
  - `BP_RiftBoss_Prototype` derives from the grunt and now looks like the warrior too.
  - The UEFN Mover pawns are not affected; no enemy uses them.
- **Licence:** the asset library does not record the pack's seller or licence ([third-party notices](third-party-notices.md)).
