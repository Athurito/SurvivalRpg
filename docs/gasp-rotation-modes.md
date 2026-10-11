# GASP-09 – Rotation modes and combat stance

Status: **Ready** (defined 2026-10-11). The package outline is in the [movement plan](gasp-movement-plan.md#gasp-09-rotation-modes-and-combat-stance). This report refines it against the current code and is the task report for the implementation PR.

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

## Current state (checked 2026-10-11 on `master` `338f0120`)

| Area | Finding |
| --- | --- |
| Native rotation defaults | `ARpgCharacter` sets `bUseControllerRotationYaw = true`; its movement component sets `bOrientRotationToMovement = false`, `bUseControllerDesiredRotation = false`, and 720°/s (`Source/SurvivalRpg/Core/Character/RpgCharacter.cpp`). The review recorded that `BP_Rpg_Character` overrides orient-to-movement to `true` at 500°/s. That Blueprint value was not re-read for this definition because the editor was not running. |
| Existing rotation leases | `URpgCharacterMovementComponent::PhysicsRotation` and `ARpgCharacter::FaceRotation` already hand rotation to two prediction-safe owners. Mantle stops all rotation. Block faces the saved control yaw per move, including saved-move replay (`IsBlockControllingRotation`, `ComputeOrientToMovementRotation`). New rotation logic extends this pattern instead of adding a parallel writer. |
| Sprint | Sprint is decided per move in `URpgCharacterMovementComponent::CanSprintForMove` with GAS stamina ([CMC sprint](gasp-cmc-sprint.md)). A direction gate belongs in this per-move check so that client prediction, replay, and server agree. |
| Equipment state for the stance | `URpgEquipmentLoadoutComponent` owns the active hand selection, but it is controller-owned and replicates only to the owning client. The pawn-owned `URpgEquipmentManagerComponent` replicates its `EquipmentList` to everyone, and each `URpgEquipmentInstance` replicates `EquippedSlot`. Only that pawn projection reaches simulated proxies and late joiners. |
| Lock-on and aiming | Neither exists in the code. GASP-09 implements only "weapon in hand" and "blocking" and leaves an extension point for later triggers. |
| Other users of `ARpgCharacter` | `ARpgAICharacter` inherits the rotation defaults and turns through controller focus. The Mover pawn sets its own rotation. Neither may change. |
| Classification tags | `Weapon.Type.Melee` and `Weapon.Type.Ranged` exist, and so do the hand slot tags `Equipment.Slot.MainHand`, `OffHand`, and `BothHands`. |

## Scope

- **Stance resolution.** Derive the stance from replicated, pawn-side state that owner, server, and simulated proxies share: an applied hand equipment instance (see decision 1) or an active block. Keep the resolver small and value-only so that lock-on or aiming can be added later without a new state machine.
- **Rotation policy.** Free movement orients to movement without camera yaw; combat stance faces the control yaw.
  - Make it prediction-safe in `URpgCharacterMovementComponent`, next to the block and mantle leases.
  - Ground and air rotation rates are designer-tuned values. Start from the original GASP behavior: instant on the ground and 200°/s in the air. GASP-11 later moves them into the shared movement profile.
  - Scope it to the GASP CMC pawn through an opt-in setting, so AI and the baseline pawn keep their behavior.
- **AnimBP inputs.** Report `RotationMode` and `WantsToStrafe` from the resolved stance in `BP_RpgGasp_CMC`. This activates the strafe path and the aim offset in combat stance. The values come from replicated state, so proxies and late joiners see the same mode.
- **Sprint direction gate.** In combat stance, sprint only within a designer-tuned angle of the facing (original: 50°). Free-movement sprint is unchanged.
- **Attack facing.** Implement the outcome of decision 2.
- **Cleanup.** Collapse the native and Blueprint rotation defaults the conflict depends on. Other movement values stay for GASP-11.

## Out of scope

- Speeds, acceleration, braking, gait, and the animation set (GASP-10 to GASP-12).
- Mover, Mover-Ragdoll, and AI facing.
- Lock-on, aiming, and new combat abilities.
- Any load dependence: load still only selects the dodge ([game vision](game-vision.md)).

## Decisions to settle at the start

The implementing chat settles these with the user before Phase B and records the answers here.

1. **Which hand items trigger the stance.**
   - Recommended: applied hand equipment that carries a `Weapon.Type.*` tag.
   - Alternative: any hand equipment, including harvesting tools. Check first how tools are tagged.
2. **Attack facing during a montage.** Today a camera turn rotates the character during an attack, because `FaceRotation` is suppressed only for mantle and block (derived from code, not measured).
   - Recommended: an attack commits to the camera facing at activation and ignores camera turns until its recovery, through the existing lease pattern. This keeps positioning readable.
   - Alternative: keep today's behavior and record it as a known limit.
3. **Turn into the stance.** Instant like the original, or a short designer-tuned turn rate for a heavier feel. The final value belongs to GASP-11.

## Ownership

| Responsibility | Location | Reason |
| --- | --- | --- |
| Stance resolution from replicated equipment and block state | C++, value-only helper used by the character and movement component | Feeds per-move prediction and simulated-proxy inputs; needs the same result on every machine |
| Rotation policy, sprint direction gate, attack facing lease | C++ in `URpgCharacterMovementComponent` and `ARpgCharacter` | Movement authority, saved-move replay, and server correction |
| Opt-in switch, rotation rates, sprint angle | Designer-tuned defaults on `BP_RpgGasp_CMC` or its movement component | Tuning, not mechanism |
| `RotationMode` and `WantsToStrafe` to the AnimBP | `BP_RpgGasp_CMC` and the existing AnimBP inputs, read on the game thread | Presentation wiring; the AnimBP only reads a snapshot |

File ownership while the package runs:
- `Source/SurvivalRpg/Core/Character/RpgCharacter.*`
- `Source/SurvivalRpg/Core/Character/RpgCharacterMovementComponent.*` and its tests
- `BP_RpgGasp_CMC`, and `BP_Rpg_Character` only for the conflicting rotation defaults
- new or extended tests under `Source/SurvivalRpgEditor/Private/Network`
- this report

GASP-10 to GASP-12 touch the same files and start only after this package merges.

## Phases

1. **A – baseline.**
   - Settle the three decisions with the user.
   - Read in the editor how `BP_Rpg_Character` and `BP_RpgGasp_CMC` set rotation and feed `RotationMode` and `WantsToStrafe` to the AnimBP.
   - Repeat the review's facing and sprint measurement on UE 5.8.3 as the baseline.
2. **B – native seam.** Add the stance resolver, the opt-in rotation policy, the sprint direction gate, and the attack facing lease, with tests for replay, correction, and proxies.
3. **C – Blueprint wiring.** Enable the policy on `BP_RpgGasp_CMC`, report the mode to the AnimBP, and remove the conflicting rotation defaults.
4. **D – validation.** Run the automation, the PIE probe, and the network checks, then the user check.

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

## Verification

- `python Build/Tools/Unreal/ue.py build`.
- Focused automation through `ue.py test`, including:
  - `SurvivalRpg.GASP.CMC` (including `SurvivalRpg.GASP.CMC.Sprint`);
  - `SurvivalRpg.Combat.Block` and `SurvivalRpg.Combat.BlockLocomotion`;
  - `SurvivalRpg.GASP.Mantle`, `SurvivalRpg.GASP.Vault`, and `SurvivalRpg.GASP.Hurdle`;
  - `SurvivalRpg.GASP.Mover`, as a regression for the untouched variant;
  - new tests for stance resolution, the sprint gate, and proxy and late-join mode.
- A PIE probe that repeats the review's facing measurement at 30, 60, and 120 FPS.
- A user check on `Lvl_RpgGaspCMC`: run free, draw a weapon, strafe, backpedal, try a backward sprint, block, and attack while turning the camera.

## Repro (after implementation)

1. Start PIE on `Lvl_RpgGaspCMC` with the CMC Experience.
2. Without a weapon, run in circles: the character turns toward its movement.
3. Draw a weapon. Move sideways and backward: the character keeps facing the camera and strafes. Hold sprint while moving backward: no sprint.
4. Holster the weapon: free movement resumes.

## Results

Not started.
