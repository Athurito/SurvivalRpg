# GASP-STAB-05 – Releasing block while standing

Status: **PR open** ([#249](https://github.com/Athurito/SurvivalRpg/pull/249) on `claude/gasp-stab-05-block-release`, first commit `788db119`). The user found the problem in the fourth [GASP-13 check](gasp-presentation-polish.md#limits) and deferred it; on 2026-10-16 the user marked it as the next package and chose real steps over a faster blend. This report defines the package and is its task report.

## Goal

Releasing block while standing reads as one motion: the shield comes down and the feet step back from the wide block stance into the idle stance, instead of a shield lowering followed by a second motion that slides the feet. The block, its gameplay and the block-press presentation stay as they are.

## What happened before (measured on `master` `cf40597f`)

A release probe held block, released it and recorded the subject on the owner, the listen server and a proxy (CMC, 3 PIE clients, `Lvl_RpgGaspCMC`). Releasing while standing ran in two phases on every role:
- The shield's `BlockEndMontage` (`AM_Block_End_Seq`, slot `BlockUpperBody`, 0.667 s) played. While standing the block layer's `UpperBodyMaskAlpha` is 0, so the montage drove the whole body. It starts and ends in the pack's wide combat stance (feet 77 cm apart front to back and 59 cm apart sideways). While the layer faded out (0.2 s) and the montage blended in (0.25 s), the feet moved about 15 cm toward the idle stance and back.
- When the montage ended, its 0.25-s blend-out moved the body from the pack's combat stance to the GASP relaxed idle: the feet slid about 40 cm on the ground and the hands moved about 45 cm.

Each foot covered 58 to 72 cm near the ground and 0 cm lifted, on every role. A release while walking played the same montage on the upper body only; the arms then moved twice as well.

## Change

The fix is presentation only; block state, input, the speed cap and the GAS ability are unchanged.

Content in the shared block layer (`ABP_RpgBlockLocomotion`, used by CMC and Mover):
- **Exit phase 5.** When block ends while the character stands and is not interrupted, the layer enters phase 5. Its data child's chooser picks the exit animation and its hold time (`SelectedDuration`). The layer holds `LayerAlpha` at 1 while the exit plays (`ExitHolding`), then fades out with `ExitBlendTime` as before. Movement or a block interruption ends the hold for good. The phase is optional: a data child whose chooser has no phase-5 row keeps the old release (the layer re-enters phase 0 and fades). New function `UpdateBlockExit`, called in the update before `LayerAlpha` is set; the old "re-enter phase 0 on release" branch moved into it.
- **Upper body.** A new layered blend (`spine_01` and below, mesh-space rotation like the existing upper-body blend) puts the base pose on the upper body during the exit (`ExitUpperBodyAlpha`, `ExitUpperBodyBlendTime` 0.3 s), so the arms go straight to the idle pose while the legs step. The layer's input pose now goes through a cached pose (`BlockInput`), so the base locomotion updates once even when the fade and the upper-body blend both read it.
- **Re-press during the exit.** The exit clip's own upper body is a sword flourish. If block is pressed again while the exit is still visible (`LayerAlpha` > 0.01), the upper body stays on the base pose for `TransitionBlendTime` (`ExitUpperBodyHoldRemaining`) while the blend stack leaves the exit clip.
- **Capsule turning during the exit.** The block layer already counts capsule yaw changes as `TurnDebt` while blocking and counter-rotates its pose by it. During the exit hold it keeps counting instead of letting the debt decay, so the stepping legs stay put in the world when the capsule turns (the Mover turns back after a release; see Limits).
- **Validation.** `ValidateAnimationSet` calls the new `ValidateExitSelection`: `ExitUpperBodyBlendTime` must be positive; a phase-5 row must return a compatible animation with `SelectedDuration` > 0; no row is allowed.

Content in `GF_Combat_Core`:
- `Exit/Idle_Combat_to_Idle_Seq_Retargeted`: the pack's `Idle_Combat_to_Idle_Seq` (1.333 s), retargeted to the UEFN skeleton with the same settings as the 35 block clips of `GASP-06` (source `SKM_Manny_Simple`, target rig `IK_UEFN_Mannequin`, root and pelvis mapped by name, toe chains aliased to the foot chains, feet and balls excluded) and normalized the same way (`BlockRootYaw`, first-frame root lock, no root motion, no contact markers). The retarget tool created its own `IK_Source` and `RTG_SourceToTarget` in that folder. It starts in the block stance (both feet within 1 cm) and takes two steps: the right foot at 0.07 to 0.33 s, the left at 0.43 to 0.67 s. It ends 16 cm (left foot) and 7 cm (right foot) from the GASP relaxed idle stance. The upper body after 0.2 s is a sword flourish and is never shown.
- `SwordShield/CHT_BlockLocomotion`: one new row 22, phase 5 → the exit clip, `SelectedDuration` 0.7 s. Rows 0 to 21 are unchanged (checked against the export before the edit).
- `BP_WeaponInstance_BasicShield`: `BlockEndMontage` cleared. `AM_Block_End_Seq` stays in the project unreferenced by the shield. `BlockStartMontage` is unchanged.
- `ABP_Block_SwordShield` and `ABP_Block_TestVariant` are only recompiled and saved; their values are unchanged. `TestVariant` has no exit row and keeps the old release.

Ownership:
- The release timing comes from the replicated block state that every role already reads (`bBlockLocomotionActive`, `bBlockInterrupted`); the exit runs locally in each anim instance and replicates nothing. Owner, server and proxies measured the same.
- Native code is unchanged. Exit selection, timing and the upper-body blend are designer content in the layer and its data children, as the [block-layer contract](gasp-block-locomotion.md) asks for phases.
- The asset closure is in the [manifest](assets/gasp-stab-05-block-release.json).

## Measurements

Release probe (CMC unless named; owner, listen server, proxy; same scenarios on `master` and the fix). "Planted" is the horizontal distance a foot covered while it stayed within 1.5 cm of its height at the start or the end of the 1.3-s window; "lifted" is the rest.

| Case | Left foot planted / lifted (cm) | Right foot planted / lifted (cm) |
| --- | --- | --- |
| `master`, release while standing | 71 / 0 | 58 / 0 |
| Fix, release while standing | 21 / 20 | 11 / 24 |
| `master`, release after a 120° block turn | 72 / 0 | 59 / 0 |
| Fix, release after a 120° block turn | 22 / 20 | 11 / 24 |

All three roles were within 1 cm of each other. In time: 0 to 0.3 s the arms go to the idle pose and the right foot steps (lifted 5 cm); 0.42 to 0.69 s the left foot steps; 0.69 to 0.9 s the layer fades and the left foot slides the remaining ~15 cm.

Edge cases, every role:
- A 0.12-s tap plays the same exit.
- Walking off 0.35 s into the exit blends into the start without a jump (hands and pelvis at most 311 to 371 cm/s).
- Pressing block again 0.3 s into the exit moved the right hand at most 7 cm per frame, as a press from idle does (9 cm). Without the upper-body hold it moved 21 cm in one frame (the exit clip's flourish showed).
- Releasing while walking, stopping or strafing has no exit and no end montage; the arms blend with the layer fade.

Mover (`Lvl_RpgGaspMover`, same probe; "baseline" means the fix with the phase-5 row disabled and the end montage restored, in memory only):

| Case | Owner L / R | Server L / R | Proxy L / R |
| --- | --- | --- | --- |
| Baseline, release while standing | 41 / 48 | 41 / 48 | 41 / 48 |
| Fix, release while standing | 21 / 11 | 21 / 11 | 21 / 11 |
| Baseline, release after a turn (capsule turns back 120°) | 75 / 50 | 46 / 21 | 53 / 30 |
| Fix, release after a turn | 44 / 35 | 26 / 42 | 50 / 46 |

Planted cm per foot. Two intermediate versions of the turning case were worse than the baseline; see Failed attempts.

## Tests

- `SurvivalRpg.GASP.CMC.HostView.GaspCMCHostViewPIE.StandingStanceKeepsFacingUntilItMovesOnEveryRole`: the block step now records each role's feet after the release and requires less than 35 cm of planted travel per foot. With the fix: owner 21.9 / 11.2, server 19.8 / 9.8, proxy 21.2 / 11.3 cm, passed. With the `master` assets checked out: owner 69.2 / 56.5, server 71.4 / 58.4, proxy 72.2 / 59.1 cm, failed on every role.
- The block-layer validation hook now checks the optional exit; `SurvivalRpg.Combat.BlockLocomotion.Validation` runs it for both data children (see Validation).

## Validation

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- `ue.py test SurvivalRpg.GASP.CMC.HostView`: 3 of 3 passed (warnings are the known PIE network noise).

Regression (`ue.py test`, after the build, one suite at a time):
- Passed: native (`--null-rhi`, movement, equipment, block lifecycle and prediction, `BlockLocomotion.Validation`, asset contracts) 139/139; `SurvivalRpg.Animation` 4/4; `SurvivalRpg.GASP.CMC` 14/14; `SurvivalRpg.Combat.BlockLocomotion` 16/16 (CMC and Mover); `SurvivalRpg.GASP.Mantle` 13/13; `SurvivalRpg.GASP.Vault` 9/9.
- `SurvivalRpg.GASP.Mover` 65/67: `MaximumDepthHurdleLandsBeyondRearEdge` and `LateJoinReconstructsTheCurrentVaultState` ran into their 60-s limit; both are known timing cases from earlier regressions and passed alone afterwards.
- The first run of Hurdle (1/12), RuntimeRetarget (1/2), VariantComparison (1/3) and Network (15/19) failed on my side: a script waiting for an editor polled the MCP port 8000, which the test process held, and each poll logged `Toolset ... not found` as an error that the running test counted. Rerun without polling: Hurdle 12/12, RuntimeRetarget 2/2, VariantComparison 3/3, Network 19/19.

## Failed attempts

- **Turn abort.** The first version ended the exit once the capsule turned more than 15° from where it started. On the Mover, whose capsule turns back after a release, the stepping legs still swept with the capsule during the fade: planted travel rose above the baseline (owner 89 / 64 cm against 75 / 50). Counting the turn into `TurnDebt` instead brought it to 44 / 35 cm; with both, 78 / 49 cm. The abort and its two variables were removed.
- **Upper-body hold on every press.** Holding the upper body whenever block was pressed in phase 5 also held a normal press long after an exit: the hand moved 4 to 5 cm per frame, paused for five frames at 1 to 2 cm, then moved again. The hold now applies only while the exit is visible.

## Limits

- **Mover block height (also on `master`).** On the Mover, some block starts stand the block pose about 20 cm too high (feet at 31 to 32 cm instead of 9 to 10), while others do not (after a turn, after walking). The baseline shows it as well. The exit keeps that pose up to 0.7 s longer, and walking off during the exit then drops the body in one or two frames. Not analyzed; recorded as an open finding.
- **Remaining slide.** After the steps, the layer fade still slides the left foot about 15 cm and the right about 7 cm into the GASP idle stance, because the pack's relaxed stance differs from GASP's.
- **Block press unchanged.** Pressing block still snaps the feet into the wide stance within about 0.15 s (about 40 cm each, as before). The user only reported the release.
- **Exit while the capsule turns.** The legs stay in the world while the capsule turns during the exit; the base locomotion then has to resolve the remaining turn after the fade. Measured only for the Mover's 120° turn back.
- **Content coverage.** Only the sword-and-shield data child has an exit. Other block sets keep the old release until they add a phase-5 row.
- **Tuning.** `SelectedDuration` 0.7 s, `ExitUpperBodyBlendTime` 0.3 s and the unchanged `ExitBlendTime` 0.2 s are measured choices, not user-tuned.
- The measurements ran locally in PIE without latency (`GASP-VAL-02`).

## Repro

1. Open `Lvl_RpgGaspCMC`, set PIE to a listen server with two players, start.
2. Stand still with sword and shield, hold the right mouse button for a second, release it.
3. Expected: the shield comes down at once and the feet take two short steps back into the normal stance, in one motion; no second slide afterwards. The other window shows the same.
4. Also: release while walking (no steps, arms blend once), press block again right after a release (no arm jerk), release and walk off at once.

## Commits

- `788db119` feat(gasp): step out of the block stance when block is released
- `e4b7548b` test(gasp): check that releasing block steps out of the block stance
