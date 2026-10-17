# GASP-STAB-06 – Block turning on every screen

Status: **PR open** ([#257](https://github.com/Athurito/SurvivalRpg/pull/257)), implemented 2026-10-17/18 on `claude/gasp-stab-06-block-turn`; in the user check on 2026-10-18 the user found it looks good. The user asked to start it on 2026-10-17 after merging #256. Defined 2026-10-17 from the first user check of `GASP-15` (definition PR [#255](https://github.com/Athurito/SurvivalRpg/pull/255), merged as `b3005564`). Results are under [Results](#results).

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
- 2026-10-17 (user): start `GASP-STAB-06` after merging #256.

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

## Results

All numbers come from runs in this task on UE 5.8.3 in the editor of the producing checkout (worktree `musing-ishizaka-7bac20`), with a scripted listen-server PIE probe (owner, listen server and a remote copy; the host or a client acting; 60 FPS) and the automation suites. Raw evidence is in `Saved/GaspStab06` and `Saved/Gasp15/UserCheck` of that checkout and is not versioned. The [manifest](assets/gasp-stab-06-block-turn.json) lists the changed packages with their hashes.

### Acceptance status

| Area | Result |
| --- | --- |
| Block entry | **Met** for owner, listen server and remote copy, with the host and a client acting. After −180° and −90° camera turns, hips and shoulders reach the block stance within 0.07–0.16 s on every role (remote copies before: 0.85–1.03 s), no block turn clip starts, and at most 1.5° (CMC) or 4.5° (Mover) of debt remains. The late joiner is a remote copy on the same code path; the network test measures it 0.1° behind the capsule 0.3 s after a block press. |
| Turn clips | **Met at the start of a turn, not over a whole partial turn.** The right turn clips no longer twist against the turn at the start: 0.0° hips and shoulders on every role in sweeps and flicks (before 12–14° hips, 7–10° shoulders). A full 180° turn shows no reversal at all. When the debt is smaller than the clip's angle, left turns still come back 12–24° at their end, and right turns up to 4° hips and 12° shoulders (unchanged from before); see [Limits](#limits). |
| Direction | **Met up to 170°.** A remote copy waits until a smoothed flick has arrived before it starts a turn: a 180° flick plays one 180° clip instead of two 90° clips, and a −120° flick no longer starts a right turn from a 4° leftover. All roles turn the same way as the capsule for flicks up to 120° and sweeps. An exact 180° flick is still decided by the sign of ±180° per role; in the final runs all roles chose the same side, but nothing enforces it. |
| Unchanged | **Met** for continuous turning (same turn starts and debts as before) and the Mover's block; see the regression below. |
| User check | **Positive.** On 2026-10-18 the user tested in the editor and found it looks good ("schaut gut aus denke ich"), without further findings; the partial-turn overshoot was not raised. |

### Phase A – inventory and baseline

- **Live logic.** The EventGraph of both block parents holds the update logic five times; only one copy is reachable from `Event Blueprint Update Animation`, the four others are dead exec chains from earlier authoring that hang on `DeltaTimeX` by data pins only. Only the live chain was edited. Each update, the facing change since `PreviousYaw` adds to `TurnDebt`, except in the frame the block starts; `RootYawOffset` is `−TurnDebt`; at `|TurnDebt| ≥ 25°` while standing, `EnterBlockPhase(4)` picks a 90° or 180° clip by debt (90° below about 120°) and pays the debt along `BlockRootYaw`, scaled by debt over the clip's remaining curve.
- **Mover.** The Mover turns its capsule toward the camera at a rate (about 0.7 s for 180°), not at once. So on every role the layer counted that turn as debt and played two 90° clips over about 1.5 s; owner and remote copies agreed. The right turn twist is the same as on the CMC. Releasing block turns the Mover's capsule back toward its previous facing (180° in the probe); that is a Mover rotation rule outside this package and recorded as an open finding.
- **Right turn clips.** The wind-up is in the Sword & Shield sources: `Turn_Block_90_R` and `_180_R` turn the hip line 12.2° and 13.6° and the shoulders 7.1° and 10.1° against the turn in the first 0.2–0.3 s; the left clips and the pack's `Turn_Combat_*_R` clips do not. `Turn_Combat_R` keeps the same feet but holds the pelvis 23° further round, so swapping legs between the clips would pop.
- **Partial turns.** The layer shows `hip(t) − (1 − s) · root(t)` of the clip, where `s` is the debt share of the clip's angle. With `s < 1` the clip's own body-to-root twist plays at full size, so bodies that lead their root come back at the end of the turn.

### Phase B – block entry

- New tuning on both parents (category `BlockLocomotion`): `EntrySettleMinTime` 0.2 s, `EntrySettleMaxTime` 0.8 s, `EntrySettleRate` 30°/s, and the runtime values `EntrySettleRemaining` and `EntrySettleMinRemaining`.
- At block entry the settle starts. While it runs, the facing change is not counted as turn debt, so the body follows the mesh as it does on the owner. It ends once the facing turns slower than `EntrySettleRate` after at least `EntrySettleMinTime`, or after `EntrySettleMaxTime`.
- The minimum came from the automation test: a remote copy can receive the block state a frame or more before the capsule turn. Without the minimum the settle ended on the still frames in between, and the arriving smoothing became a turn clip again (`proxy body … 102 deg behind`). With it, the test passed three of three runs in the editor.
- The Mover's capsule turn falls inside the settle, so its body follows the capsule without a clip; about 4° remain as debt.
- Defaults are on the parents, and the four data children carry equal values, so the parents still own them.

### Phase C – turn clips

- The four derived right turns (UEFN and Manny, 90° and 180°) got a corrected pelvis track: per key, the pelvis (and with it the spine) turns about the component up axis so that the hip line never falls behind `(1 − s_min)` of the clip's root turn, where `s_min` is 0 for the 90° clips and 0.6 for the 180° clips (the smallest debt share the chooser plays them with). The thighs keep their component rotation, so the legs only follow the hip joints.
- Largest correction 14.5° (90°) and 20.5° (180°). The feet move at most 2.5 cm (90°) and 3.6 cm (180°), the hands up to 16 cm with the turned upper body. Start and end poses are identical; root track, `BlockRootYaw`, notifies, `FootContacts` markers and sections are untouched.
- A first attempt held the hips at the start pose until the original came back. It removed the twist only for full turns; at a 26° debt with a 90° clip the hips still went 6° the wrong way, because the clip's root then turns only 29 % as far. It was reverted before the final version.

### Phase D – direction

- New tuning `TurnStartMaxRate` 360°/s and `TurnForceStartDebt` 150°: a standing block turn starts only while the facing turns at most `TurnStartMaxRate`, or once the debt reaches `TurnForceStartDebt`. A remote copy therefore waits until a smoothed flick has arrived and chooses the clip from the whole debt. Turning the camera slower than 360°/s is unaffected.

### Phase E – validation

- **Network test.** `StandingStanceKeepsFacingUntilItMovesOnEveryRole` now also checks 0.3 s after the block press that every role's root is within 15° of the capsule. With the layer before the minimum settle time, it failed on the proxy (102° and 103.4° behind) in `ue.py` and in the editor; with the final layer it passed three of three runs in the editor. A second block press after the late join checks the late joiner as well (0.1° behind).
- **Build:** `ue.py build` passed (0 errors, 0 warnings).
- **Regression**, each suite on its own through `ue.py test` on the final assets:

  | Suite | Result |
  | --- | --- |
  | `SurvivalRpg.GASP.CMC.HostView` (3 tests, with the new block press checks) | 3 / 3 in five `ue.py` runs (two before and three after the late-joiner check was added) and three in-editor runs. One further `ue.py` run failed two tests on an error log of the MCP plugin in the test editor ("Call to unknown method server/discover", an MCP client reached its port), not on a test condition. |
  | Native (movement, equipment, harvesting, abilities, block, dodge, combat content and contracts, CMC and mantle assets, speed parity, sprint, runtime retarget, input, UI input, camera) | 143 / 143 |
  | `SurvivalRpg.Animation` | 4 / 4 |
  | `SurvivalRpg.GASP.CMC` | 14 / 14 |
  | `SurvivalRpg.Combat.BlockLocomotion` | 16 / 16 |
  | `SurvivalRpg.GASP.Mantle` | 13 / 13 |
  | `SurvivalRpg.GASP.Vault` | 9 / 9 |
  | `SurvivalRpg.GASP.Hurdle` | 12 / 12 |
  | `SurvivalRpg.GASP.VariantComparison` | 3 / 3 |
  | `SurvivalRpg.Network` | 19 / 19 |
  | `SurvivalRpg.GASP.Mover` (`np.ForceReconcile 0`, `t.MaxFPS 30`) | 66 / 67: `GaspMoverMantlePIE.RemoteStandingMantleKeepsMomentumAfterHandoff` timed out once and passed on its own; it does not block, and the changed layer logic runs only while blocking. |

### Limits

- **Partial turns overshoot:** when the debt is smaller than the clip's angle, a block turn's body overshoots and comes back at its end, 12–24° on left turns and up to 12° shoulders on right turns, on every role and on both skeletons, as before this package. This comes from scaling only the root turn by the debt share. Fixing it needs a different turn model (blending the clip by its share, or clips for smaller angles); it is not part of this package and needs the user's decision after the user check.
- **Exact 180°:** a camera flick of exactly 180° has no shorter side; each role takes the sign of its own ±180° debt. The final runs agreed, but this is not enforced.
- **Entry settle while turning:** if the camera keeps turning faster than 30°/s while block is pressed, the body follows the camera without compensation for up to 0.8 s, so the feet turn with it.
- **Clip correction:** the corrected right turns move the feet up to 3.6 cm while the pelvis is held; it is a correction of the derived clips, so a rebake of the block set from the sources must apply the same correction (Phase C).
- **Dead graph copies:** the four unreachable copies of the update logic in both block parents stay; removing them is cleanup outside this package.
- **Mover release:** the Mover turns back to its previous facing when block is released; outside this package.
- **Stop-and-go cadence** of continuous block turning is unchanged (out of scope).
