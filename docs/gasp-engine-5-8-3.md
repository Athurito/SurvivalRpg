# GASP-ENG-01: Engine update to UE 5.8.3

Status: **Merged.** [PR #221](https://github.com/Athurito/SurvivalRpg/pull/221)
from branch `claude/gasp-eng-01-ue-5-8-3` (based on `master` `f563bd08`) was
merged on 2026-10-11 at 14:26:58 UTC as `e13ab82b`; final reviewed head
`2e15e87e`. This status update ran no new builds or tests.

## Goal

The installed launcher engine at `D:/Programme/UE_5.8` moved from UE 5.8.2 to
UE 5.8.3 (CL 58210709) on 2026-10-11. Every build then stopped at the
NetworkPrediction pin in `Source/SurvivalRpg.Target.cs`: "Review and update
the pinned NetworkPrediction correction before changing from UE 5.8.2". The
editor still opened with the last 5.8.2 binaries, but no C++ could be
compiled. The user asked for the move to 5.8.3 as its own task.

## Upstream review

`prepare.py` stages four plugin copies (NetworkPrediction with the project
patch; Mover, ChaosMover and MoverExamples unchanged). Their 5.8.3 trees were
compared with the 5.8.2 manifest baselines using the installer's own
`files()` walk, so the file sets match what staging copies.

| Plugin | Files | Added | Removed | Changed |
| --- | --- | --- | --- | --- |
| NetworkPrediction | 59 | 0 | 0 | 0 |
| Mover | 185 | 0 | 0 | 1 |
| ChaosMover | 104 | 0 | 0 | 0 |
| MoverExamples | 205 | 0 | 0 | 0 |

- **NetworkPrediction** is identical, including
  `NetworkPredictionWorldManager.cpp`, the file the patch changes. The patch
  applies with exact context and produces the recorded output hash, and the
  64-frame history the recovery helper relies on is unchanged.
- **Mover:** `Source/Mover/Private/MovementModeStateMachine.cpp` changes one
  line in `UMovementModeStateMachine::RollbackModifiers`:

  ```diff
  -	if (UMoverComponent* MoverComp = Cast<UMoverComponent>(GetOuter()))
  +	if (UMoverComponent* MoverComp = Simulation ? Simulation->GetTypedOuter<UMoverComponent>() : nullptr)
  ```

  The state machine is created by `UKinematicActorSimulation`, so its outer is
  the simulation, not the Mover component. In 5.8.2 the cast failed and the
  function skipped everything after clearing the queued modifiers: a modifier
  that a rollback started or ended never received `OnStart` or `OnEnd`. In
  5.8.3 the component is found through the simulation, and both callbacks run
  (with a `LogMover` Log line each).

### Project impact

- SurvivalRpg declares no movement modifiers of its own. The Mover variant
  uses Mover's crouch: `URpgCharacterMoverComponent` derives from
  `UCharacterMoverComponent`, whose stance handling queues `FStanceModifier`.
  Its `OnStart` and `OnEnd` resize the capsule and offset the mesh.
- After a reconcile that starts or ends a crouch, 5.8.3 now applies or reverts
  that capsule change; 5.8.2 left the capsule as it was before the rollback.
  This is an upstream correctness fix for corrections during stance changes.
  Steady-state crouching, traversal, death and ragdoll locks of
  `bHandleStanceChanges` are not affected, because the callbacks only run for
  modifiers that the rolled-back state differs in.
- The CMC variant does not use Mover.

## Changes

- `Build/Patches/NetworkPrediction/manifest.json`: `engine_version` 5.8.3;
  the reviewed baseline hash of the one changed Mover file; the overlay hash
  of the recovery header, whose comment no longer names a version. All other
  baselines, the patched output and the patch hash are unchanged.
- `prepare.py` reads the required version from the manifest instead of a
  second literal and names it in its messages. `test_prepare.py` uses a 5.8.3
  synthetic engine and adds `test_other_engine_version_is_refused_before_copying`.
- `Source/SurvivalRpg.Target.cs` pins UE 5.8.3.
- The patch README describes the engine update procedure, including how a
  checkout with old overrides moves to the new manifest.
- Version mentions that act as instructions follow the pin: the ignore
  comment, the GaspPacketGap tool, the Lyra skill and the GASP roadmap. Earlier
  reports keep the version they ran on.

## Validation

All runs on UE 5.8.3, CL 58210709, in this worktree.

- `prepare.py remove` with the 5.8.2 manifest: the old overrides verified and
  were removed. `check`, `stage` and `verify` against 5.8.3: passed; the staged
  Mover file is the 5.8.3 version.
- `python Build/Patches/NetworkPrediction/test_prepare.py`: 9 tests passed.
- `python Build/Tools/Unreal/ue.py build`: `SurvivalRpgEditor` Win64
  Development succeeded in 172 s with one warning, C4996 for the internal
  `Chaos::Filter::FInstanceData::GetComponentId` in the unchanged
  `ChaosMoverSimulation.cpp`. The same warning appears in the 2026-10-03
  full build under 5.8.2.
- `SurvivalRpg.NetworkPrediction` (`--null-rhi`): the recovery clock test
  passed. Its log loads `UnrealEditor-NetworkPrediction.dll` and
  `UnrealEditor-Mover.dll` from the project's `Plugins/.../Binaries/Win64`,
  not from the installed engine.
- `SurvivalRpg.GASP.Mover` with `np.ForceReconcile 0` and `t.MaxFPS 30`:
  66 of 67 passed in 868 s (64 with the known warnings), including both crouch
  cases with late join (remote client and listen host), Mantle, Vault, Hurdle,
  corrections, lifecycle and ragdoll.
  - The failure is the known timing-sensitive fixture
    `Vault.GaspMoverVaultPIE.EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection`
    ([VAL-03](gasp-buffered-traversal-replay.md), open finding). Six isolated
    reruns: three passed, three failed. Every failure is the same
    precondition, "Equipment replacement overlapped the observer's old active
    NP traversal", missing its window of about one Fixed step. The behavior
    assertions never failed.
  - The test does not crouch and the project queues no movement modifiers, so
    the changed rollback code iterates empty modifier lists here. Under 5.8.2
    the batch failed the same way and one isolated run passed. That evidence
    is too thin for a failure rate, so a timing shift on 5.8.3 cannot be ruled
    out.
- `SurvivalRpg.GASP.CMC` and `SurvivalRpg.GASP.MovingBlock` (same flags):
  7 of 8 passed. `GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage`
  timed out at "Owner accelerates and its animation snapshot tracks real
  movement". Of 18 isolated runs, 16 of them with temporary logging, 11
  passed and 7 failed.
  - Temporary logging, removed afterwards, found a fixture race. CQTest's
    empty map lets both pawns spawn near z=0 and fall. The test spawns its
    floor only in its first step.
  - When the remote pawn is still at z≈0, it lands at z=92.2 and the test
    passes. When it has already fallen below the floor top (z=-86 in a
    failing run), it falls forever. The owning client first reports it
    grounded, because its own copy has not received the server state yet,
    and then jumps to the server's falling position.
  - CMC does not use Mover or NetworkPrediction. No 5.8.2 runs of this test
    exist in the available logs, so whether the update changed the odds is
    unknown. The fixture fix is proposed as a separate task.
- Broad native run (`--null-rhi`): UI, Inventory, Crafting, Save, Storage,
  Combat, Itemization, Interaction, Harvesting, Progression, CMC sprint and
  both GASP asset compositions. 437 of 438 passed.
  - The one failure, `BlockLocomotionPIE.MoverBlockPreventsSprintInBothInputOrdersAndResumesHeldSprintOnRelease`,
    is a PIE test. It failed only on an error line from an MCP client probing
    the test editor (`Call to unknown method "server/discover"`).
- `SurvivalRpg.Combat.BlockLocomotion` with rendering, same flags as the Mover
  suite: 15 of 16 passed, including the case above.
  - `MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease`
    timed out at "Fixed replay reconstructs old blocked history while today's
    head and GAS activation are already released". In three isolated reruns it
    passed once.
  - The test's own log shows the cause. The release-crossing correction
    passes when the client-server frame offset is 24: the restore lands one
    frame before the tracked frame (restored 588, tracked 589) and replays the
    blocked history. It fails at offset 22: the restore lands on the tracked
    frame itself (587 = 587), where the block is already released, so the
    expected replay never occurs.
  - Block lives in the project's own Mover sync state, not in movement
    modifiers; the changed rollback code does not take part. No 5.8.2 history
    of this test is documented, so whether 5.8.3 shifts the offsets more often
    is unknown.

## Limits

- Three timing-dependent PIE fixtures fail intermittently on 5.8.3: the known
  Mover VAL-03 vault case, the CMC late-join case (a floor race, found here)
  and the Mover release-crossing block rollback (depends on the frame
  offset). None of them reaches the changed upstream code. No 5.8.2 is
  installed any more for a direct comparison of failure rates.
- No project test runs a crouch change through a forced rollback, so the
  changed upstream path itself is covered only by review. The crouch tests run
  crouch with a remote client, a listen host and late join, without a forced
  reconcile.
- Cooked, packaged and WAN runs (`GASP-VAL-02`) were not repeated.
- The GaspPacketGap probes were not re-run on 5.8.3.
- Other checkouts keep their 5.8.2 overrides until they follow the README:
  run `remove` with the old manifest, then `check`, `stage`, `verify` and
  build.

## Rollback

Revert this change as a whole and reinstall UE 5.8.2. With UE 5.8.3 installed,
the 5.8.2 pin cannot build.
