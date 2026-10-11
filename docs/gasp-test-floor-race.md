# GASP-STAB-03: Floor before pawns in CQTest network tests

Status: **Merged.** [PR #223](https://github.com/Athurito/SurvivalRpg/pull/223)
from branch `claude/gasp-stab-03-test-floor-race` was merged on 2026-10-11 at
15:14:07 UTC as `426c3fa3`; final reviewed head `0e44a720`. The branch was
first stacked on `GASP-ENG-01` (`2e15e87e`), because only the UE 5.8.3 pin
builds on the installed engine; after
[PR #221](https://github.com/Athurito/SurvivalRpg/pull/221) merged as
`e13ab82b`, it was retargeted to `master` and merged with it. Implementation
commit `a36ebe44`. This status update ran no new builds or tests.

## Problem

`SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage`
timed out in 7 of 18 isolated runs on UE 5.8.3, at "Owner accelerates and its
animation snapshot tracks real movement" ([GASP-ENG-01](gasp-engine-5-8-3.md)).
Temporary logging during GASP-ENG-01, removed afterwards, found the cause:

- CQTest builds an empty temporary map. With no player start,
  `AGameModeBase::FindPlayerStart` falls back to the WorldSettings at the
  origin, so the listen host's and the remote client's pawns spawn there and
  start falling at once.
- The test spawned its floor, `ARpgCombatNetworkFloorFixture` (a 1 km box with
  its top at z=0), only in its first step, through `SpawnAndReplicate`.
- If the remote pawn was still near z=0 when the floor appeared, it was pushed
  up, landed at z=92.2 and the test passed. If it had already fallen below the
  floor top (z=-86 in one failing run), the push pointed down: the pawn ended
  up under the floor and fell forever. The owning client first reported it
  grounded, because its own copy had not yet received the server state, then
  jumped to the server's falling position. The readiness gate had already
  passed, so the movement step timed out.
- The host pawn always fell through. The test does not use it.

## Affected tests

All CQTest network tests that use `ARpgCombatNetworkFloorFixture` on the empty
map were checked.

| Test | Floor | Exposure | Change |
| --- | --- | --- | --- |
| `SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage` | first step | movement starts from the initial landing | fixed |
| `SurvivalRpg.GASP.RuntimeRetarget.GaspRuntimeRetargetPIE.RemotePoseLateJoinTraversalEquipmentAndFallback` | first step | same: the first movement and the idle check run before any placement | fixed |
| `SurvivalRpg.GASP.Mantle.GaspMantleExperiencePIE.AuthoritativeValidationRootMotionLateJoinAndLifecycle` | first step | the first gate needs the owner grounded before the server parks every pawn; a pawn below the floor can hang that gate | fixed |
| `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation` | first step | the first gate needs the owner grounded before the server teleports every pawn; same exposure | fixed |
| Mover experience, gameplay and lifecycle; Mover ragdoll; CMC sprint; block locomotion | inside `InitGame` | none: floor and player starts exist before any pawn | unchanged |
| `SurvivalRpg.GASP.Mantle.GaspMantleAuthoredMapPIE` and the other authored-map tests | authored map | none | unchanged |

The Mantle and Combat exposure follows from the code; no run of either test
was observed hanging at that gate. The loot and harvest PIE tests do not use
the floor fixture and never check whether a pawn is grounded.

## Change

The five unaffected test files already spawn the floor and a few player starts
in their `FGameModeEvents::OnGameModeInitializedEvent` handler, inside
`InitGame` and before any player logs in. The fix brings the four affected
tests to the same order:

- `FRpgScopedNetworkTestGround` in `RpgCombatNetworkTestTypes` installs that
  hook. For every authoritative PIE world it spawns the floor fixture and four
  player starts in lanes 500 cm apart along Y, 120 cm above the floor top.
  Four starts cover host, owning client and late joiner and keep one free for
  a respawn. Each test class owns one, starts it in `BEFORE_EACH` and keeps it
  alive through network teardown, like the save isolation hooks. It is opt-in
  so that the Mantle file's authored-map class, which shares that file's save
  isolation, keeps its map untouched.
- The four tests drop their `SpawnAndReplicate` floor step and the unused
  `Floor` state member.
- The CMC and runtime retarget tests add a server gate before their first
  client gate: every logged-in player's pawn must be ready and grounded on the
  listen server. Movement input therefore starts only after the server and the
  owning client both hold the subject on the floor.

The movement steps are unchanged: ordinary CMC input, no transform edits.
Pawns now spawn at player starts instead of the world origin. This affects
only where the subject stands before its first step; the Mantle and Combat
tests teleport every pawn to their own positions afterwards, as before.
Player starts use the engine's `NoCollision` capsule, so they block neither
movement nor weapon traces.

Ownership: test fixtures only. No runtime code, asset, map or config changes.

## Validation

All runs on UE 5.8.3 (CL 58210709) in worktree `musing-ishizaka-7bac20`, with
`--exec "np.ForceReconcile 0" --exec "t.MaxFPS 30"`. Logs and reports are in
the ignored `Saved/ToolRuns/20261011-*` folders of that worktree.

- The 5.8.2 overrides were removed with the old manifest (`prepare.py remove`)
  before rebasing onto GASP-ENG-01; `check`, `stage` and `verify` against
  5.8.3 passed. Test logs load `NetworkPrediction` and `Mover` from the
  checkout's `Plugins/.../Binaries/Win64`.
- `python Build/Tools/Unreal/ue.py build`: passed in 588 s with one warning,
  the known C4996 in the unchanged `ChaosMoverSimulation.cpp`.
- `GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage` in
  isolation with `--expect-tests 1`: **8 of 8 passed**, 54–56 s each, every
  run with only the recurring warnings (temporary-map NetGUID, Voice, console
  performance). Before the fix: 11 of 18 under 5.8.3. The new server gate
  finished on its first check, and the log no longer shows "PATHS NOT
  DEFINED", so the pawns spawned at the new player starts.
- `SurvivalRpg.GASP.CMC`: 8 of 8 passed.
- `SurvivalRpg.GASP.RuntimeRetarget`: 2 of 2 passed; the PIE test alone
  passed twice more (3 of 3 in total).
- `SurvivalRpg.GASP.Mantle.GaspMantleExperiencePIE`: 3 of 3 passed.
- `SurvivalRpg.Network.CombatRemoteMeleePIE`: 2 of 5 passed. Every failure
  timed out at "Attack NN/21: authority applies damage and fully cleans up",
  with a full authority trace and no damage hit, at attacks 01, 16 and 21. The
  same test with its base version (this branch's fixture, the old test file,
  rebuilt) passed 2 of 4 and failed the same way at attacks 06 and 07. This is
  a pre-existing miss, most likely of the test's content-calibrated
  sword-contact offset, not the floor race, and the change does not make it
  worse. The floor race no longer
  applies: every run reached the attack steps.
- `SurvivalRpg.GASP.MovingBlock` from the task description matches no test:
  those tests were removed with the reverted first GASP-06 attempt
  (`de852e98`). Its successor `SurvivalRpg.Combat.BlockLocomotion` ran once:
  14 of 16 passed. `MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease`
  failed as known from GASP-ENG-01 (1 of 3 isolated there).
  `CMCActivationSnapshotsCapAndZeroCapPreservesNormalSpeed` failed once
  ("Both legs actually animate during directional movement") and then passed
  2 of 2 in isolation. Neither test file nor its floor changed.

After merging `master` `de30a390` (GASP-ENG-01 and the UI-07 main menu) into
the branch: overrides still verified against the merged manifest, all LFS
files present, `ue.py build` passed in 235 s without warnings, and the CMC
late-join test passed 2 of 2 in isolation.

Failed attempts:

- The first isolated CMC run failed in 28 s because 220 of this worktree's
  5359 LFS files were still pointers for assets changed on `master` since its
  last pull ("The summary for the package ... is invalid"). After
  `git lfs pull` the series was restarted; that run is not counted.
- Two test runs started by a stopped runner script were killed before they
  reported; they are not counted either.

## Limits

- The remote melee test still fails intermittently at a random attack step.
  Its content-calibrated contact offset needs its own investigation; it is
  outside GASP and this fixture.
- `CMCActivationSnapshotsCapAndZeroCapPreservesNormalSpeed` failed once in the
  full block locomotion run; one failure is too thin for a rate.
- 8 consecutive passes of the CMC test make a remaining failure rate near the
  previous 39 % very unlikely, but do not prove zero.
- Cooked, packaged and WAN runs (`GASP-VAL-02`) are out of scope.

## Reproduce

```powershell
python Build/Tools/Unreal/ue.py test SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage --expect-tests 1 --exec "np.ForceReconcile 0" --exec "t.MaxFPS 30"
```

Run PIE network tests one process at a time; parallel editor test runs on one
machine were avoided throughout.
