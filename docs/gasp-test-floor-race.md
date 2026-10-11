# GASP-STAB-03: Floor before pawns in CQTest network tests

Status: **In progress**. Branch `claude/gasp-stab-03-test-floor-race`.

## Problem

`SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage`
timed out in 7 of 18 isolated runs on UE 5.8.3, at "Owner accelerates and its
animation snapshot tracks real movement" ([GASP-ENG-01](gasp-engine-5-8-3.md)).
Temporary logging during GASP-ENG-01, removed afterwards, found the cause:

- CQTest builds an empty temporary map. Both PIE pawns, the listen host's and
  the remote client's, spawn at the world origin because no player start
  exists. They start falling at once.
- The test spawned its floor, `ARpgCombatNetworkFloorFixture` (a 1 km box with
  its top at z=0), only in its first step, through `SpawnAndReplicate`.
- If the remote pawn was still near z=0 when the floor appeared, it landed at
  z=92.2 and the test passed. If it had already fallen below the floor top
  (z=-86 in one failing run), the floor's penetration push pointed down: the
  pawn ended up under the floor and fell forever. The owning client first
  reported it grounded, because its own copy had not yet received the server
  state, then jumped to the server's falling position. The readiness gate had
  already passed, so the movement step timed out.
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

The loot and harvest PIE tests do not use the floor fixture and never check
whether a pawn is grounded.

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

Ownership: test fixtures only. No runtime code, asset, map or config changes.

## Validation

Pending. Builds and test runs need the UE 5.8.3 pin from GASP-ENG-01.

## Limits

Pending.
