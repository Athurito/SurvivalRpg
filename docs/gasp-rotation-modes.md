# GASP-09 – Rotation modes and combat stance

Status: **PR open** ([#232](https://github.com/Athurito/SurvivalRpg/pull/232), draft; implemented 2026-10-11 on `claude/gasp-09-rotation-modes`, first commit `353a0f8a`). The automated checks pass apart from one known Mover timing case; the user's visual check is open. The package outline is in the [movement plan](gasp-movement-plan.md#gasp-09-rotation-modes-and-combat-stance). Results, failed attempts, and limits are under [Results](#results).

## Goal

Give the CMC character two explicit rotation modes and report them correctly to the GASP AnimBP:

- **Free movement:** no weapon in hand and out of combat. The character turns toward its movement.
- **Combat stance:** a weapon in hand or blocking. The character faces the camera and strafes, as in New World.

Sprint in combat stance is allowed only roughly forward, so backward sprinting is impossible.

## Why

The [2026-10-11 review](gasp-movement-plan.md#findings-from-the-2026-10-11-review) found a rotation conflict on the CMC pawn:
- The capsule follows the camera, but the AnimBP is told `OrientToMovement` with `WantsToStrafe = false`.
- Strafing therefore runs 86° off the facing and backpedaling 176° off, at full speed.
- Aim offset and strafe-specific logic stay off.
- Orient-to-movement adds a frame-rate-dependent nudge of about 4° at 120 FPS.
- Sprint has no direction gate.

GASP-10 to GASP-13 tune speeds, movement, gait, and presentation on top of the rotation mode, so this package comes first.

## Starting state (checked 2026-10-11 on `master` `338f0120`)

| Area | Finding |
| --- | --- |
| Rotation defaults | `ARpgCharacter` sets `bUseControllerRotationYaw = true`; its movement component sets `bOrientRotationToMovement = false`, `bUseControllerDesiredRotation = false`, and 720°/s (`Source/SurvivalRpg/Core/Character/RpgCharacter.cpp`). Read in the editor: `BP_Rpg_Character` overrides orient-to-movement to `true` at 500°/s and allows physics rotation during root motion; `BP_RpgGasp_CMC` inherits both. |
| AnimBP inputs | `BP_RpgGasp_CMC::Get_PropertiesForAnimation` derived both `RotationMode` and `WantsToStrafe` from `bOrientRotationToMovement`, which is always `true` on this pawn. The AnimBP therefore never saw the strafe mode. |
| Existing rotation leases | `URpgCharacterMovementComponent::PhysicsRotation` and `ARpgCharacter::FaceRotation` hand rotation to two prediction-safe owners. Mantle, vault, and hurdle (one ability) stop all rotation. Block faces the saved control yaw per move, including saved-move replay. |
| Sprint | Sprint is decided per move in `URpgCharacterMovementComponent::CanSprintForMove` with GAS stamina ([CMC sprint](gasp-cmc-sprint.md)). |
| Equipment state | `URpgEquipmentLoadoutComponent` owns the active hand selection, but it is controller-owned and replicates only to the owning client. The pawn-owned `URpgEquipmentManagerComponent` replicates its `EquipmentList` with `EquippedSlot` to everyone, including simulated proxies and late joiners. |
| Spawn loadout | On `Lvl_RpgGaspCMC` and `Lvl_RpgGaspMantle` the player spawns with sword and shield active in both hands. |
| Lock-on and aiming | Neither exists in the code. |
| Other users of `ARpgCharacter` | `ARpgAICharacter` and its Blueprints inherit the rotation defaults and turn through controller focus. The Mover pawn sets its own rotation. |
| Attack montages | `AM_BasicSword_Attack01` and the combo attack use root motion; the pickaxe harvest swing does not. Attacks and harvesting run through the native `URpgGameplayAbility_BasicWeaponAttack` and `URpgGameplayAbility_Harvest`; the dodge resolves its profile in native code and sequences its montage in Blueprint. |

## Decisions

The user settled these on 2026-10-11 before implementation.

1. **Stance trigger:** any equipment in the hands, weapons and harvesting tools alike, because tools aim their area effect too.
2. **Attack facing:** like action games with committed attacks (Elden Ring, Monster Hunter):
   - the attack starts in the camera direction;
   - during the windup it may still steer toward the camera at a limited rate;
   - from the hit window until the recovery ends, the facing is held and camera turns no longer rotate the character;
   - every combo hit re-aims at its start;
   - area-effect tools aim at the start too.
3. **Turn into the stance:** instant, as in GASP; Offset Root Bone smooths the visible turn. The attack commitment from decision 2 keeps instant turning from spinning attacks.

## Scope

- **Stance resolution:** equipment applied in the main or off hand, or an active block, from replicated pawn state.
- **Rotation policy:** an opt-in on the movement component.
  - Free movement orients to the movement input, combat stance faces the control yaw.
  - Prediction-safe, next to the block and mantle leases.
  - Ground and air rates are designer-tuned: GASP values, instant on the ground and 200°/s in the air. GASP-11 moves them into the shared movement profile.
- **AnimBP inputs:** `RotationMode` and `WantsToStrafe` follow the resolved stance.
- **Sprint direction gate:** in combat stance, sprint only within a designer-tuned angle of the camera yaw. GASP uses 50°.
- **Facing commitment:** a generic ability lease implements decision 2 for weapon attacks and harvesting swings. The dodge uses it to keep its roll direction.

## Out of scope

- Speeds, acceleration, braking, gait, and the animation set (GASP-10 to GASP-12).
- Mover, Mover-Ragdoll, and AI facing.
- Lock-on, aiming, and new combat abilities.
- Any load dependence: load still only selects the dodge ([game vision](game-vision.md)).

## Ownership

| Responsibility | Location | Reason |
| --- | --- | --- |
| Hand equipment read model | `URpgEquipmentManagerComponent::HasEquipmentInHands` | Replicated pawn state that owner, server, proxies, and late joiners share |
| Stance, rotation policy, sprint gate, saved-move history | `URpgCharacterMovementComponent`, `ARpgCharacter::FaceRotation` | Movement authority, saved-move replay, and server correction |
| Facing commitment lease | `URpgAbilitySystemComponent`, helpers on `URpgGameplayAbility` | Activation-scoped lifecycle like the block and sprint leases; Blueprint abilities can call the helpers |
| Commitment timing | `URpgGameplayAbility_BasicWeaponAttack` (windup steering rate, hold at the attack window), `URpgGameplayAbility_Harvest` (hold for the swing), `URpgGameplayAbility_Dodge` (hold for the roll) | Each ability already owns its window or swing timing |
| Opt-in, rates, sprint angle, windup steering rate | `BP_RpgGasp_CMC` movement defaults and the attack ability's class defaults | Designer tuning |
| `RotationMode` and `WantsToStrafe` | `BP_RpgGasp_CMC::Get_PropertiesForAnimation` reads `IsInCombatStance` | Presentation wiring on the game thread; the AnimBP only reads the snapshot |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Free movement | Facing follows velocity on steady straight runs within 1° at 30, 60, and 120 FPS. The AnimBP sees `OrientToMovement` and `WantsToStrafe = false`. |
| Combat stance | The capsule faces the camera. The AnimBP sees the strafe mode and `WantsToStrafe = true`, the aim offset is active, and side and backward movement select strafe animations. |
| Switching | Drawing and holstering a weapon switches the mode while idle and while moving. On the owner, the extra server corrections during the switch stay at the level of a run without the switch. |
| Sprint | In combat stance, sprint works only within the configured angle, and backward sprint is impossible. Free-movement sprint and the stamina cost are unchanged. |
| Block | Block keeps its camera alignment. The block prediction and lifecycle tests pass. |
| Traversal | Mantle, vault, and hurdle keep their rotation lease. Their automation passes. |
| Network | Owner, server, and simulated proxy report the same mode, with a listen server and two clients. A late joiner sees the stance of a pawn that already holds a weapon. A forced correction keeps the mode. |
| Unchanged | AI characters, the Mover variants, and every equipment load tier move as before. |
| Attack facing | Behaves as decision 2 records. |

## Implementation

- **Movement** (`URpgCharacterMovementComponent`):
  - `bUseRotationModes` opts a character in. With it, movement is the only yaw writer: `ARpgCharacter::FaceRotation` no longer applies controller yaw, and `PhysicsRotation` runs one policy:
    1. A held facing commitment does not rotate.
    2. A steering commitment turns toward the camera at its rate.
    3. The combat stance faces the camera at `GroundRotationRate` (≤ 0 instant) or `AirRotationRate`.
    4. Free movement orients to the input at the same rates.
  - Mantle keeps its lease. Characters without the opt-in keep their authored flags.
  - Saved moves record hand equipment, the facing commitment, and the control rotation, so corrections replay the original policy. The server samples its own state, as for block and sprint; only the existing sprint request bit crosses the wire.
  - `CanSprintForMove` rejects sprint in the combat stance when the input is more than `CombatStanceSprintAngle` (50°) from the move's camera yaw.
  - `IsInCombatStance()` is the BlueprintPure read model for the AnimBP. On simulated proxies it reads the replicated equipment list and the replicated blocking tag.
- **Facing commitment:** `URpgAbilitySystemComponent::Begin/Hold/EndFacingCommitment` is an activation-scoped lease.
  - It is validated by ability, avatar, and prediction key like the block lease. A newer activation replaces it, and stale cleanup cannot release a newer one. It resets with the block lease on avatar change and teardown.
  - `URpgGameplayAbility` wraps it in BlueprintCallable helpers and ends it in `EndAbility`.
  - Basic weapon attacks steer at `WindupSteeringRate` (360°/s) and hold from `OpenAttackWindow`, which runs on the owner from the notify and on authority from its schedule.
  - Harvest swings hold from montage start until the montage ends, so a stride stays free.
  - The dodge holds from profile resolution until its end.
- **Equipment:** `URpgEquipmentManagerComponent::HasEquipmentInHands()` reports applied main- or off-hand equipment.
- **Blueprint:**
  - `BP_RpgGasp_CMC` enables `bUseRotationModes`. Its children, the mantle character and the GASP-08 pilot pawns, inherit it.
  - `Get_PropertiesForAnimation` reports `WantsToStrafe = IsInCombatStance` and `RotationMode = Strafe` in the stance (the existing select, fed by `NOT IsInCombatStance`).
  - The unused `bOrientRotationToMovement` getter was removed. Other movement values are unchanged.
- **Tests:**
  - `SurvivalRpg.Movement.CMC.RotationModes`: four native tests on a real CMC floor with GAS leases and real equipment. They cover free movement, stance and its replay, facing commitment steer/hold/replace/cancel/replay, and the stance sprint directions.
  - `SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.CombatStanceReachesOwnerServerAndLateJoin`: a listen server with a late-joining second client.
  - `SurvivalRpg.GASP.CMC.AssetComposition` now requires the opt-in on the CMC pawn.

## Verification and evidence

Raw evidence is in `Saved/Gasp09` and `Saved/ToolRuns` of the producing checkout and is not versioned.

## Results

All results are from this branch on UE 5.8.3.

### Acceptance status

| Area | Result |
| --- | --- |
| Free movement | **Met.** PIE probe on `Lvl_RpgGaspCMC` after holstering: facing against velocity is 0.0° median and maximum in all four directions at 30, 60, and ~110 FPS (the 120 cap reached 108–113 FPS in the editor). The PIE network test sees `OrientToMovement` and `WantsToStrafe = false` on owner, listen server, and late joiner. |
| Combat stance | **Met, except the aim offset, which was not measured.** Same probe with the spawn sword and shield: facing against the camera is 0.0° median and maximum at all three rates. Strafing runs 90° and backpedaling 180° off the facing. The PIE network test sees the strafe mode and `WantsToStrafe = true` on owner, server, and late joiner. The aim offset's activation and the selected strafe animations were not inspected; they are part of the user check. |
| Switching | **Met.** The PIE network test draws and holsters the sword on the server while the owner runs. With `p.NetShowCorrections 1`, no correction occurred in the free, draw, late-join, and holster phases. |
| Sprint | **Met.** Native test: free movement sprints backward; the stance does not sprint backward or sideways, but does at 45° and forward; turning the camera away ends the sprint at once. PIE probe: stance backward plus sprint input stays at 375 cm/s with no effective sprint; forward reaches 585 cm/s. The sprint PIE tests pass. Corrections in them, rotation modes off/on: 24/22, 30/30, 4/0, 4/4. |
| Block | **Met.** Block lifecycle, prediction, and all 16 block-locomotion PIE tests pass, including camera facing with late join; block is part of the stance. |
| Traversal | **Met.** Mantle, vault, and hurdle PIE tests pass on CMC (31/31); mantle keeps its rotation lease. |
| Network | **Met for owner, listen server, and late join.** The native replay tests show the recorded stance and commitment survive corrections. A PIE run with forced corrections was not made. |
| Unchanged | **Met in tests.** Characters without the opt-in keep controller yaw (native test). Equipment and load tests, the remote melee and harvest network tests with AI targets, and the Mover suites pass, apart from one known Mover timing case (see [PIE regression](#pie-regression)). |
| Attack facing | **Met at the native seam.** The lease steers at the leased rate, holds, is replaced by a chained activation, survives stale cleanup, ends on cancellation, and replays from history. A live attack with camera turns is part of the user check. |

### Automation

- `python Build/Tools/Unreal/ue.py build`: passed after each change, 0 errors and 0 warnings.
- `SurvivalRpg.Movement.CMC.RotationModes`: 4/4 passed.
  - A mutation check confirmed the tests detect real regressions: with the sprint gate and the stance target removed, 3 of the 4 failed.
- `SurvivalRpg.GASP.CMC`: 9/9 passed, including the new PIE test, AssetComposition, and the sprint PIE tests.
- Native and asset regression: 113/113 passed. Filters:
  - `SurvivalRpg.Movement`, `SurvivalRpg.Equipment`, `SurvivalRpg.Harvesting`, `SurvivalRpg.AbilitySystem`;
  - `SurvivalRpg.Combat.Block.Lifecycle`, `SurvivalRpg.Combat.Block.Prediction`, `SurvivalRpg.Combat.Dodge`;
  - `SurvivalRpg.Combat.AbilityContent`, `SurvivalRpg.Combat.WeaponAttackAssetContract`, `SurvivalRpg.Combat.BlockLocomotion.Validation`;
  - `SurvivalRpg.GASP.CMC.AssetComposition`, `SurvivalRpg.GASP.Mantle.AssetComposition`.

### PIE regression

Run one suite at a time with rendering:

| Filter | Result |
| --- | --- |
| `SurvivalRpg.Combat.BlockLocomotion` | 16/16 passed, CMC and Mover |
| `SurvivalRpg.GASP.Mantle` | 12/12 passed |
| `SurvivalRpg.GASP.Vault` | 8/8 passed |
| `SurvivalRpg.GASP.Hurdle` | 11/11 passed |
| `SurvivalRpg.GASP.RuntimeRetarget` | 2/2 passed |
| `SurvivalRpg.GASP.VariantComparison` | 3/3 passed |
| `SurvivalRpg.Network` (remote melee, loot harvest, storage, skill tree) | 19/19 passed |
| `SurvivalRpg.GASP.Mover` | 66/67 passed |

The Mover failure is `Vault.GaspMoverVaultPIE.EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection`, failing on "Equipment replacement overlapped the observer's old active NP traversal". Run alone three more times, it passed once.
- This is the known timing case from the [handoff](gasp-integration-handoff.md#offene-befunde): 3 of 6 isolated runs missed the same overlap condition on 5.8.3 before this change.
- The Mover pawn has no `URpgCharacterMovementComponent` and reads neither the rotation modes nor the facing lease.

### Failed attempts

- The first PIE probe toggled `bUseRotationModes` at runtime for a baseline. Python refuses to edit an `EditDefaultsOnly` property on instances, and the `set` console command did not change it either. The baseline numbers above therefore come from the review, and the correction comparison used a temporary local C++ patch that forced the flag off. The patch was reverted before any commit.
- The same probe ran into a wall after repeated runs in one direction; the final probe alternates directions.
- Reading graphs and component defaults over MCP marked `BP_RpgGasp_CMC` dirty without content changes. A forced editor kill then left restore data, which was set aside as `Saved/Autosaves/PackageRestoreData.gasp09-discarded.json`. The asset was saved only after the intended edits.

### Limits

- **Aim offset and strafe animations:** not inspected beyond the AnimBP inputs.
- **Equip latency:** equipment changes are server-authoritative and not predicted. The owner switches modes one replication delay after the server. Sprinting sideways or backward while a weapon is drawn can therefore cause a correction at the switch.
- **Dodge:** the roll keeps the facing it starts with. Directional dodges belong to [#124](https://github.com/Athurito/SurvivalRpg/issues/124).
- **Root-motion attacks:** steering during the windup adds to the montage's root-motion rotation. That is intended, but not tuned.
- **Mover timing case:** `EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection` still fails intermittently, as before this change.
- **Not measured:** the review's 5.8.2 numbers were not re-measured on 5.8.3 with the old behavior, only the correction counts. Cooked builds and real network conditions are untested (see `GASP-VAL-02`).

## Repro

1. Start PIE on `Lvl_RpgGaspCMC` without an Experience override. The player spawns with sword and shield drawn, so in the combat stance.
2. Move sideways and backward: the character keeps facing the camera and strafes. Hold sprint while moving backward: no sprint. Sprint forward works.
3. Attack and turn the camera during the swing: the character still corrects during the windup, then holds the swing's direction until the recovery ends.
4. Press the sword's action bar slot again to holster. Run in circles: the character turns toward its movement, and the camera no longer turns it.
5. Draw the sword again with the same slot: the combat stance resumes.
