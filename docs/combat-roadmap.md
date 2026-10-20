# Combat roadmap

Status, rules and tasks for combat feel, enemies and the first rune on the way
to the first playable. Each task is one bounded PR on a
`claude/cmb-<nn>-<slug>` or `codex/cmb-<nn>-<slug>` branch. A task records its
assets, validation and limits in its own report (`docs/combat-<slug>.md`) and
updates only its own row below. The user marks the next task **Ready**.

## Why now

The first playable must answer "does combat feel good?" first
([game vision](game-vision.md), section 12). Since GASP-15 and GASP-17 the
player is a Manny-native pawn with the MetaHuman Paladin, so combat animation
no longer needs retargeting. The combat itself has barely moved: one attack,
untuned grips, no armed locomotion and enemies without real behavior.

## Current state (checked 2026-10-20 on `master` `ec8795a5`)

| Area | State |
| --- | --- |
| Player | The CMC pawns play the Lyra Manny skeleton natively ([Manny gameplay mesh](gasp-manny-gameplay-mesh.md)). The Paladin comes from the removable action set `LAS_RpgCharacter_Paladin` ([Paladin production](gasp-paladin-production.md)); the gameplay mesh is then an invisible Manny copy. Free movement and combat stance ([rotation modes](gasp-rotation-modes.md)). |
| Attacks | `URpgGameplayAbility_BasicWeaponAttack` runs attack windows from montage notifies on the owner and from a schedule on authority. The weapon instance maps `Weapon.Attack.*` tags to `FRpgWeaponAttackDefinition` (montage, damage, trace). Only `AM_BasicSword_Attack01` and `AM_Combo_Attack_03_01_Seq_Montage` are used; there is no combo chain. |
| Block | `URpgGameplayAbility_Block` with perfect-block window and guard break. `FRpgWeaponBlockDefinition::SkeletonPresentations` selects the Manny set on the CMC and the UEFN set on the Mover ([block locomotion](gasp-block-locomotion.md), [block turn](gasp-block-turn.md)). The block-release draft [#249](https://github.com/Athurito/SurvivalRpg/pull/249) is parked. |
| Dodge, reactions, death | `URpgGameplayAbility_Dodge`, `GA_Combat_HitReaction`, `GA_Combat_Stagger`, `GA_Combat_Death`; the CMC corpse ragdolls on `PA_Mannequin`. Load-dependent dodge variants are open in [#124](https://github.com/Athurito/SurvivalRpg/issues/124). |
| Weapon grips | `ED_BasicSword`, `ED_BasicTwoHandedSword` and `ED_BasicShield` attach to the bones `hand_r` and `hand_l` with transforms tuned on UEFN. The user reported on 2026-10-13 that the sword sits in the wrist and the shield is reversed. GASP-15 measured matching hand axes and left the visual grip check open. |
| Armed locomotion | None. The arms swing freely in every gait, so the weapons clip into the body (user report). |
| Enemies | `BP_RiftGrunt_Sword` uses `SKM_Manny_Simple` with the old `ABP_Unarmed` and `ST_RiftGrunt_ChaseAndAttack`, on `URpgAIAttackDriverComponent`, `URpgAIBlockDriverComponent` and the `DA_EnemyLoadout_*` loadouts. The user reports that enemies "can't do anything" and their ragdoll looks odd. |
| Feedback | No combat cues or sounds for swing, hit, block or death. The [VFX roadmap](vfx-roadmap.md) covers harvesting, rune, soul and portal effects. |
| Runes | `GF_Runes_Core` holds only the VFX-02 Echo rune presentation ([Echo rune](vfx-echo-rune.md)), triggered by hand in the gallery. No rune gameplay ([#25](https://github.com/Athurito/SurvivalRpg/issues/25)). |
| Animation content | `GF_Combat_Core/Content/Animations` holds the UE5 (`Sequence2`) variants of Sword and Shield and Sword Animations, about 960 assets, mostly unused: four ground combos with four hits each, dodges in eight directions, directional and heavy hit reactions, block, parry, executions, deaths. |

## Rules

### Scope

- Sword and shield first. Spear, bow and mage follow after the first playable
  unless the user pulls them forward (see "Later").
- Grounded and heavy, like the movement ([movement plan](gasp-movement-plan.md)):
  air combos, air dodges and air-to-floor attacks from the packs are not used
  unless the user decides otherwise.
- Combat outcomes stay authoritative in GAS, equipment and the server. Pose
  selection, AnimBP state and effects are presentation only.
- Every gameplay change is checked in multiplayer (user request 2026-10-13):
  PIE network tests, a 2- to 3-player probe of the remote character and a
  multiplayer step in the playtest.

### Ownership

- Each task records its C++ boundary decision in its report before it creates
  code or assets ([system ownership boundaries](../.agents/skills/survival-rpg-orchestrator/references/system-ownership-boundaries.md)).
- Extend the existing native mechanisms: the attack, block, dodge and stagger
  abilities, `FRpgWeaponAttackDefinition`, `FRpgWeaponBlockDefinition`, the
  anim-layer linking in `URpgEquipmentManagerComponent`, the AI drivers and the
  enemy loadouts. Add native code only for authority, prediction, lifecycle or
  measured hot paths, for example a predicted combo chain.
- Concrete abilities are `GA_*` Blueprints. Montages, AnimBPs, linked layers,
  choosers, StateTrees, cues, effects and tuning are assets authored through
  Unreal MCP.

### Skeletons and assets

- Manny (`/Game/SurvivalRpg/Characters/Mannequins/Meshes/SK_Mannequin`) is the
  gameplay skeleton of the CMC player and of Manny-native enemies. Never add
  bones or change its hierarchy: that regenerates its GUID and re-conforms
  about 1,800 assets on every load ([handoff](gasp-integration-handoff.md)).
  A task that adds sockets checks the GUID first.
- The Paladin body, its hands and `Knight_01` hang on the separate GASP UE5
  skeleton copy (`Characters/GASP/Shared/Characters/UE5_Mannequins/Meshes/SK_Mannequin`)
  and follow the gameplay mesh as character parts. Do not move or change that
  skeleton or its `PA_Mannequin`.
- Block logic lives in two bases (`ABP_RpgBlockLocomotion` for the Mover,
  `ABP_RpgBlockLocomotion_Manny` for the CMC); a block change goes into both.
- The asset library lives in `D:/Repos/Assets` (MCP ports: `Assets_3DAssets`
  8001, `Assets_Animations` 8002, `Assets_Sounds` 8003, `Assets_VFX` 8004).
  Migrate only what a task uses, take only UE5 variants (`Sequence2`,
  `Animations_UE5`), re-point them to `SK_Mannequin` so demo skeletons and
  meshes stay behind, and add every pack to
  [third-party notices](third-party-notices.md).
- Effects follow the [VFX roadmap](vfx-roadmap.md) rules (grounded realism,
  shared Effect Types, pooled cue components). Combat tasks define cue tags and
  hook points; larger effect work can become a VFX task.
- Sounds play through gameplay cues. They are picked per sound event once the
  user has curated the sound library; until then a cue may stay silent.

### Coordination

- Tasks that touch `ABP_RpgGasp_CMC_Manny`, the block bases or the CMC pawn
  name them in their file ownership and follow the GASP rules
  ([GASP roadmap](gasp-integration-roadmap.md)).
- Tasks that change the same weapon instance or equipment Blueprints run one
  after another; binary assets cannot be merged.
- Run only one PIE network test process at a time, and check other checkouts
  and sessions before using the editor or MCP.

## Tasks

| ID | Task | Status | Report / PR |
| --- | --- | --- | --- |
| CMB-01 | Weapon grips: grip sockets of the same name on the Manny skeleton (and on UEFN for the Mover), equipment definitions point to them, grips tuned against the visible Paladin hands and gauntlets; sword, two-handed sword, shield, pickaxe and axe | Merged; user check deferred to CMB-02 | [combat-weapon-grips.md](combat-weapon-grips.md), [PR #274](https://github.com/Athurito/SurvivalRpg/pull/274) |
| CMB-02 | Armed locomotion: a per-weapon hold pose from the pack (`Idle_Combat`) layered over the GASP locomotion, delivered by the weapon instance through the anim-layer seam that already links the block layer; weapons stop clipping into the body while walking, running and sprinting | In review | [combat-armed-locomotion.md](combat-armed-locomotion.md) |
| CMB-03 | Player combo: a three- to four-hit sword-and-shield combo from `Combo_Attack_01`, input buffer and chaining window, cancel into dodge and block, damage and trace per step on the weapon | Planned | |
| CMB-04 | Hit feedback: directional and heavy hit reactions from the pack, short hitstop, camera shake, cue tags and hook points for swing, hit, block, perfect block, parry, guard break and death, with placeholder effects | Planned | |
| CMB-05 | First enemy type: the skeleton warrior from `Assets_3DAssets/SkeletonCharacter` replaces the Manny placeholder of the Rift Grunt; skeleton check against `SK_Mannequin`, migration with notice entry, grips, a combat AnimBP with the Sword and Shield clips and the pack's own stun, knockback and death clips, death and ragdoll on its physics asset | Planned | |
| CMB-06 | Enemy AI: StateTree for the skeleton warrior with approach and spacing, telegraphed attacks the perfect block can read, combo, block, hit reaction, stagger and retreat, on the existing AI drivers and loadouts | Planned | |
| CMB-07 | Echo rune: every third hit of the combo triggers a ghost strike that deals damage; the rune sits on the weapon's item instance and drives the VFX-02 presentation; first rune of the vision ([#25](https://github.com/Athurito/SurvivalRpg/issues/25)) | Planned | |

### Order

1. CMB-01, then CMB-02, CMB-03 and CMB-04 in this order; they share the
   sword and shield Blueprints.
2. CMB-05 can start after CMB-01 (it reuses the grip sockets), then CMB-06.
3. CMB-07 needs the combo (CMB-03) and preferably the cue hooks (CMB-04).
4. Then the vertical slice level and a playtest against section 12 of the
   game vision, planned separately.

### Later (not planned)

- **Spear:** second melee family from the Spear pack and the weapon meshes in
  `SkeletonProps` and `SkeletonCharacter`.
- **Bow:** ranged foundation with aiming, projectiles and ammunition, in a new
  `GF_Combat_Ranged`.
- **Mage and combat magic:** with the spell prototypes of VFX-05 in
  `GF_Combat_Magic`.
- **UE4 enemies** (`SkeletonArmy`, the `BLACKMOON_BOSS_PACK1` Golem as a portal
  boss candidate): choose one pipeline in a pilot:
  - re-rig the mesh onto the Manny bone set with the skeletal mesh editing
    tools (skeleton editing, skin-weight transfer from a Manny mesh); the result
    may contain no bone outside `SK_Mannequin`;
  - bake the needed clips offline onto their UE4 skeleton;
  - the existing runtime retarget follower (two meshes per enemy).
- Load-dependent dodge ([#124](https://github.com/Athurito/SurvivalRpg/issues/124)),
  revive and bleedout in co-op ([#31](https://github.com/Athurito/SurvivalRpg/issues/31)),
  and a cleanup of unused pack clips like GASP-05.

## Related documents

- [Combat mastery reference](combat-mastery-reference.md): combat loop, mastery
  and skill groups.
- [Combat network smoke](combat-network-smoke.md): remote melee acceptance.
- [GAS ability content](gas-ability-content.md): native ability families and
  their content.
