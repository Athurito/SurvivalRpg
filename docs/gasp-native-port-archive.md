# Archived native GASP port — 2026-09-06

The native/curated GASP migration is discontinued at the owner's request. The active project
returns to the original Prototype locomotion while retaining unrelated work merged in between.
This is a removal of the old implementation, not a claim that its turn, landing, retargeting,
or multiplayer findings have been fixed. No replacement migration is included.

## Recoverable archive

- Branch: [`codex/archive-gasp-native-port-2026-09-06`](https://github.com/Athurito/SurvivalRpg/tree/codex/archive-gasp-native-port-2026-09-06).
- Archive commit: `ec8bef456d28143485e76a543fbe35adec90cccf`; includes the merged port and
  the final unmerged work from [PR #122](https://github.com/Athurito/SurvivalRpg/pull/122).
- Archive tree: `f39ee40f839fbb20eb719bfc4181a208b7e1c63d`.
- Master before removal: `34c2b54664bc259c79a4eaae08a0754f0a8d2977`.
- Original locomotion comparison baseline: `1d2d617d2ff209bab403db56497fac789703e8ab`.

The archive was pushed and its remote head verified before removal. Its 4,074 tracked files
include 2,591 Git LFS files (1,086,832,698 bytes); all corresponding local LFS objects passed
size and SHA-256 verification. Git connectivity verification passed. Restoring this branch
requires Git LFS. Ignored `Saved` captures/logs and generated caches remain local; they are
not part of the tracked archive. Historical commits and merged PRs are retained.

## Removal and preservation

Reverse changes were applied in descending first-parent order, preserving master history.
These 31 merged PRs contain the retired port:

`#56 #59 #61 #64 #65 #67 #68 #69 #77 #78 #79 #90 #91 #92 #93 #94 #95 #96 #98
#104 #105 #106 #108 #110 #111 #112 #115 #116 #118 #119 #121`.

The GASP content plugin, native locomotion mechanisms, custom AnimGraph module, pilot
Experience/Character/PawnData/AnimBP, retarget pipeline, port-specific tests and old runbooks
are removed from the active tree. The original `RpgPrototypeExperience`, `BP_Rpg_Character`,
`ABP_Unarmed`, movement component, character, generic AnimInstance and test-map setup are
restored. Camera framing introduced solely for inspecting GASP legs is also reverted.

| Interleaved work | Preserved behavior |
| --- | --- |
| #80 | Unreal MCP plugins and configuration |
| #83 | Menu, localization and StringTable redirect |
| #89 | General native/designer architecture guidance |
| #107 / #57 | Server-authoritative remote melee windows, scheduling, damage and cleanup |
| #109 / #102 | AI ability grants through `GF_Combat_Core` and all three AI PawnData assets |
| #76 / #82 | Specialist skills retained, with only the obsolete migration policy rewritten |

Mixed PRs require explicit preservation rather than a blind revert:

- `GF_Combat_Core` uses the #109 asset (`8d8c5f72…` LFS SHA), retaining AI grants and removing
  the later GASP animation-profile provider.
- The #111 starter-loadout resave is retained (`e69c908f…` LFS SHA): its current equipment
  assignment fields correctly place the sword in MainHand and shield in OffHand. GASP-only
  weapon profile tags and additive idle conversions are reverted.
- The #115 defense-profile attribute-set guard is retained for safe Game Feature deactivation.
- The valid `IpNetDriver` fallback, editor preview settings and existing commented-out
  `RpgBaseTerminalWidgetTests` state from #56 are retained; UI-test reactivation is out of scope.
- The #107 combat ability loses only its retired GASP CombatStrafe tag. Its network test now
  uses the Prototype pawn, ordinary `UAnimInstance` APIs and an independent replicated floor.
  Its actual attack, damage, montage and cleanup assertions remain in place.
- The AI composition test drops only the removed pilot Experience. An equipment asset
  contract checks the preserved starter-loadout assignments.

Independent review checked the combined staged/working tree against the preserved PRs and
baseline, including exact binary hashes. Source/config/project files contain no remaining
GASP, Pose Search, custom Foot Placement or rotation-mode references.

## Validation

- UE 5.8 `SurvivalRpgEditor Win64 Development`: passed on the removal working tree.
- Automation: 172 passed / 7 failed across combat, AI composition, frontend, quick access,
  inventory, equipment and health. All combat/AI/frontend/equipment checks passed, including
  the starter equipment contract. The seven inventory failures are classified below.
- Rendered Prototype remote melee PIE: passed (listen server + remote client, 60 ms lag,
  10 ms variance, no packet loss). Twenty authored-rate attacks, one 1.5x-rate attack and
  cancellation passed real montage, authority blade motion, damage and cleanup assertions.
- Unreal MCP compiled `ABP_Unarmed`, `BP_Rpg_Character`, `RpgPrototypeExperience` and the
  starter-loadout Blueprint with warnings treated as errors: passed. Loaded PawnData and
  Game Feature dependencies match the restored composition; retired pilot/plugin paths
  were absent. No binary assets were resaved by this validation.
- Serialized-name scan: 2,367 remaining tracked assets (914,718,381 bytes), ASCII and
  UTF-16 LE/BE, zero retired GASP package/type names, missing LFS payloads or read errors.
- Raw audit, LFS inventory and revert journal: local `Saved/Reviews/GaspArchive20260906`.
- Build log: local `Saved/Logs/GaspArchiveRollbackBuild20260906.log`.

The seven inventory failures are outside the removed port. Five callback/revision assertions
already fail identically in `Saved/Logs/CodexReusablePaneInventory.log` from 2026-07-30:
`CollectBatch.MixedRootsUseSharedScratchAndSingleCommit`,
`PickupBatch.AtomicCommitUsesDeterministicScratchOrder`,
`PickupBatch.MixedMergeDetachedCapacityAndReentrancy`,
`TransferDelta.AtomicMergePlaceCallbacksAndReplay`, and
`TransferDelta.CrossActorSubtreeReconstructsOnlyMovedItems`. The unchanged runtime broadcasts
entry messages before incrementing the revision, while those listeners expect the increment.
`Transaction.LegacyOrderingSurfaceRemoved` sets individual capacity in a fixture whose
default is shared capacity; `UI.LegacyAssetRetirement` expects an old direct ActionbarButton
dependency where the unchanged crafting widget references ActionButtonBase. The implicated
runtime/test files and UI asset hashes match both pre-removal master and the pre-GASP baseline.
No fresh master test run was performed, and the last two have no historical failure capture.
They are documented rather than changed as part of a locomotion rollback.

Packaged Steam transport, late join and visual combat polish are not established by a local
Editor build or same-process PIE test. The preserved [combat runbook](combat-network-smoke.md)
keeps those limits explicit. Old GASP test results remain historical evidence on the archive.

## GitHub lifecycle and next direction

Old-port issues and PRs use the `archived-gasp-port` label. Unfinished port work (#55, #62,
#70, #99, #123, #125, #126 and PR #122) is closed as discontinued, not resolved by a fix.
Already merged PRs remain merged; #117 remains closed. Retained skills #76/#82 and independent
menu, architecture, melee and AI work are not classified as removed implementation.

Surface footsteps (#46) and equipped-gear/dodge design (#124) remain independent future work.
Their old GASP dependencies are historical. Medieval stance and footwork ideas from #123
remain available in its archived discussion. No general inventory-weight locomotion penalty
is introduced by this decision.

The intended later approach is an owner-led migration of original GASP Blueprints, followed
by a separately scoped Lyra integration. Parent classes, assets, skeleton/retargeting and the
native/designer boundary must then be inspected anew. The archive must not be silently
restored, and the old native mechanisms/tests are not acceptance requirements for that work.
