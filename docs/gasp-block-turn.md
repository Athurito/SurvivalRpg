# GASP-STAB-06 – Block turning on every screen

Status: **Planned**, defined 2026-10-17 from the first user check of `GASP-15`. It starts only when the user marks it Ready. Implement it after `GASP-15` ([#254](https://github.com/Athurito/SurvivalRpg/pull/254)) is merged, because that package adds the Manny copy of the block layer that this task has to change as well.

## Goal

Turning under the block layer looks the same on every network role:
- A block press after a camera turn faces body and shield toward the block direction at once on every screen, as it does on the acting player's own screen today.
- Block turn clips turn toward the camera without first twisting the other way.
- A block turn keeps its direction once it starts, and remote copies start no turn from smoothing leftovers.

## Why

During the `GASP-15` user check on 2026-10-17, the user found three things while blocking in a listen-server session ([User check 1](gasp-manny-gameplay-mesh.md#user-check-1-2026-10-17--block-turning)):
- turning the camera away and then pressing block is instant on one screen, while another screen drags the legs around;
- turning in place with block held jitters slightly while the camera still turns;
- the body sometimes starts turning one way and then switches to the other.

A scripted probe measured all three with Manny and with the same pawns switched to UEFN at runtime. Both skeletons behave the same, so the cause sits in the shared block layer logic (`GASP-06`, integrated in `GASP-13`) and in its turn clips, not in the skeleton change.

## Current state (measured 2026-10-17 on `claude/gasp-15-manny-gameplay-mesh` `ed5102ad`, Manny and UEFN)

| Area | State |
| --- | --- |
| Block layer | The parent `Shared/RPG/Block/ABP_RpgBlockLocomotion` (UEFN; Mover, and the CMC on `master`) and, with `GASP-15`, its copy `Manny/Block/ABP_RpgBlockLocomotion_Manny` (CMC). Data children `ABP_Block_SwordShield` and `ABP_Block_SwordShield_Manny` select the clips through their choosers. Both parents carry the same turn logic. |
| Turn logic | The layer reads `BlockFacingYaw`, the gameplay mesh's world yaw including visual smoothing, which `URpgAnimInstance` captures on the game thread. Its deltas since `PreviousYaw` build `TurnDebt` and `RootYawOffset`. Above `TurnThreshold` (25°), the Turn phase plays a 90° or 180° left or right clip and pays the debt off along its `BlockRootYaw` curve. At layer entry, `PreviousYaw` starts at the current `BlockFacingYaw`. |
| Base pose | While the layer is active, `Get_OffsetRootRotationMode` returns `Interpolate`, so the base root follows the capsule with a 0.1-s half-life (`GASP-13`). |
| Capsule | A standing combat stance keeps its facing until it moves, blocks or attacks; a block press turns the capsule to the camera at once (`GASP-13`). |
| Remote copies | A simulated proxy smooths its mesh rotation over several frames. The listen server moves a client's mesh with its capsule before it caches the pose (`GASP-STAB-04`). |

Measured behavior, the same on Manny and UEFN:

| Finding | Measured | Cause |
| --- | --- | --- |
| Block after a camera turn | Own screen and listen server: the body reaches the capsule in 0.11–0.15 s without a step. Remote copies, with the host or a client acting: a 90° block turn clip over 0.85–1.2 s with two direction changes of the root; the shield points away from the block direction meanwhile. | Where the mesh follows the capsule at once, the layer starts after the turn, sees no debt, and the base root catches up. A remote copy starts the layer during its mesh smoothing and counts the rest, about 80°, as turn debt. |
| Twist at the start of right turns | `Turn_Block_90_R` and `_180_R` first turn the hips 13.7° and the shoulders 8.6° against the turn for about 0.1 s, on both sets and every role; the left clips do not. | The right turn clips themselves. |
| Turn direction | A 180° camera flick is ambiguous: owner, listen server and remote copies pick different sides, and a remote copy follows the direction of its mesh smoothing. Once, a remote copy started a right turn from a 4° leftover debt and switched to the left 0.25 s later. | Direction from the sign of a debt near ±180°; turn starts from smoothing leftovers. |
| Continuous turning with block held | Root motion is smooth on every role. The turn steps stop-and-go, a 90° clip about every 1.4 s at 90°/s. | Debt threshold and clip lengths. |

## Scope

1. **Phase A – inventory and baseline.**
   - Read the turn graph of both block parents and confirm the entry, debt and direction rules above.
   - Check the Mover with the UEFN layer for the same three findings.
   - Inspect the right turn clips against their Sword & Shield sources.
   - Record a baseline with the user-check probe on the CMC (Manny) and the Mover: owner, listen server and remote copy, host and client acting, block after 90°, 120° and 180° camera turns, sweeps and flicks with block held.
2. **Phase B – block entry.** A block press after a camera turn turns body and shield at once on every role. The layer must not count the capsule turn that the block press causes, including the rest of a remote copy's mesh smoothing, as turn debt. The mechanism is chosen after Phase A; candidates are a yaw reference from the capsule's facing at entry or no debt during the entry blend. Both parents get the same change.
3. **Phase C – turn clips.** The right turn clips lose their wrong-way twist in both sets, for example through a start offset, a corrected derivation from the source, or mirrored left clips, whichever Phase A supports.
4. **Phase D – direction.**
   - A started block turn keeps its direction until it ends.
   - Remote copies start no turn from debt that their own smoothing leaves behind.
   - Ambiguous turns near 180° resolve by one rule on every role; Phase A proposes it.
5. **Phase E – validation.**
   - Probes per role on the CMC and the Mover.
   - A network test for a block press after a camera turn on every role, extending the existing standing-stance host-view test.
   - Regression of the block locomotion, CMC, host-view and Mover suites, and the user check.

## Out of scope

- `GASP-STAB-05` (block release), which stays parked unless the user combines the two.
- The stop-and-go cadence of continuous block turning, unless the user check after Phase C still finds turning jittery.
- Splitting upper and lower body in the block layer so the legs step after the shield; the user chose the instant turn instead.
- The standing turn-in-place offset of remote copies without block (`GASP-15` finding).
- A template parent for the block layer.

## Decisions

- 2026-10-17 (user): the three block turning findings of the `GASP-15` user check become this separate stabilization package; `GASP-15` stays unchanged. The package starts when the user releases it.
- 2026-10-17 (user): after a camera turn, a block press faces body and shield toward the block direction at once on every screen; the feet turn with it without a step.

## C++ boundary

- **Classification:** presentation fix in designer-owned animation content: the block layer parents, their turn rules and the turn clips. If the layer needs the capsule's facing apart from the smoothed mesh yaw, that is one more read-only snapshot value on the existing native seam.
- **Runtime truth:** unchanged. Block activation, the capsule facing and the block direction stay with GAS and the movement component; the block layer remains cosmetic.
- **Existing seams:** `ABP_RpgBlockLocomotion` and `ABP_RpgBlockLocomotion_Manny`, their data children and choosers, `URpgAnimInstance::CaptureBlockAnimationState` (`BlockFacingYaw`), and `Get_OffsetRootRotationMode` in both CMC AnimBPs.
- **Ownership:** AnimBP logic, clips and choosers in assets through the Unreal MCP workflow; native code only for a snapshot value the worker thread cannot get otherwise.
- **New native classes:** none.
- **Asset work:** Unreal MCP edits, compile and save of both parents and the turn clips, probes in listen-server PIE.

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Block entry | After a 90°, 120° and 180° camera turn, owner, listen server, remote copy and late joiner, with the host and a client acting, show hips and shoulders within 10° of the capsule within 0.25 s of the block press, with no block turn clip started by the press and no change of turn direction. |
| Turn clips | No block turn clip, left or right, 90° or 180°, in either set, turns hips or shoulders more than 3° against its turn direction. |
| Direction | A started block turn keeps its direction. Remote copies start no turn from debt below the threshold. For camera turns up to 170°, every role turns the shorter way, the same as the capsule. |
| Unchanged | Continuous turning and walking with block held, release as recorded for `GASP-STAB-05`, block reactions, and the Mover's block. The block locomotion, CMC, host-view and Mover suites pass. |
| User check | The user blocks after camera turns and turns with block held in a listen-server session with two clients and sees the same on every screen. |

## Verification

Build and test through `Build/Tools/Unreal/ue.py`. Raw evidence goes to `Saved/GaspStab06`; this report and the PR carry the summary.

## Repro

1. Open `Lvl_RpgGaspMantle`, set PIE to a listen server with two clients, and equip sword and shield.
2. Lower the shield, turn the camera 120–180°, then press block. Watch the acting player on its own screen and on both other screens.
3. Hold block and turn slowly right and left, then flick the camera.
4. Repeat on `Lvl_RpgGaspMover`.
