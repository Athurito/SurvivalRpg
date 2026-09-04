# GASP real-network smoke

This runbook owns the reproducible PIE network acceptance for the separately selectable
`RpgGaspPilotExperience`. On the cutover branch it exercises the real Experience, PawnData, GASP
character, AnimBP, CharacterMovement, Ability System Component, and replicated montage path while
staging the global Experience fallback. It does not change `DefaultPawnData` or introduce an
alternate gameplay/runtime owner.

## Automated acceptance

Primary test:

`SurvivalRpg.Network.GaspPilotPIE.ReplicationLateJoinCorrectionAndDefaultSlotMontage`

Focused issue #101 coast-gait test:

`SurvivalRpg.Network.GaspPilotPIE.GroundCoastLateJoinAndRelevancyReturn`

Focused issue #100 analog-gait regression:

`SurvivalRpg.Network.GaspPilotPIE.AnalogGaitPredictionAndCorrection`

Focused issue #103 moving-base correction regression:

`SurvivalRpg.Network.GaspPilotPIE.MovingBaseCorrectionPreservesAnimationHistory`

Review regressions for retained gait and rotation handoff:

- `SurvivalRpg.Network.GaspPilotPIE.RotatedBasePreservesSavedRunAtExitThreshold`
- `SurvivalRpg.Network.GaspPilotPIE.ActiveRunLateJoinAndRelevancyReturn`
- `SurvivalRpg.Network.GaspPilotPIE.RotationModeExitConvergesAfterInputRelease`

Focused combat-profile GameFeature lifecycle regression:

`SurvivalRpg.Network.GaspPilotPIE.CombatProfileGameFeatureDeactivationReactivation`

Default-cutover and rollback selection contracts:

`SurvivalRpg.Network.GaspPilotPIE.DefaultExperienceFallbackSelectsGasp`

`SurvivalRpg.Network.PrototypeExperiencePIE.OverrideRemainsSelectable`

Both suites are defined in
[`RpgGaspPIENetworkTests.cpp`](../Source/SurvivalRpgEditor/Private/Network/RpgGaspPIENetworkTests.cpp).

Shared `GaspPilotPIE` topology and network profile:

- one-process PIE listen server
- one initial PIE client; tests that own late-join or relevancy contracts add their required
  observer clients, while the smaller contracts use only the shared initial topology
- `PktLag=60`, `PktLagVariance=10`; the active-gait and rotation-handoff regressions additionally
  use 10% packet loss, while the other tests use no configured loss
- movement contracts spawn a replicated editor-only floor or movable platform as their shared
  network-addressable movement base; selection-only contracts need no collision fixture
- the primary contract adds one client during movement and a final client while the original subject
  is stationary, observing the subject as Authority, AutonomousProxy, and SimulatedProxy
- the #103 focus uses a replicated platform with deterministic fast translation and rotation, one
  AutonomousProxy owner, and a late-joined SimulatedProxy observer
- the separate Prototype selection contract uses one client and does not inject packet simulation

The suite checks the following contracts; record the actual outcome of each run separately:

- the pilot Experience and GASP pawn composition on every world
- the global fallback selects the GASP Experience when PIE has no override, while an explicit PIE
  override still selects the separate Prototype Experience and pawn on server and client
- UE 5.8 `FRepMovement` acceleration plus `AnalogInputModifier` parity at 25%, 50%, and 100%
  input, including the nonzero-to-zero stop edge
- start, reversal/pivot, stop, and native acceleration reconstruction after moving and stationary
  late join
- grounded Foot Placement snapshot validity and the expected network roles
- replicated Aim and CombatStrafe presentation modes
- a deliberate 90-degree facing change activates and completes the turn-in-place lifecycle on all three views
- crouch, physical jump, landing, and return to grounded presentation
- owner-only movement divergence followed by server-authoritative correction, tight convergence,
  and exactly one autonomous presentation-history reset without authority/simulated-proxy resets
- a semantic server teleport produces exactly one presentation-history reset on Authority,
  AutonomousProxy, and SimulatedProxy
- local-owner and authoritative ASC montage starts through `DefaultSlot`
- replicated simulated-proxy montage playback, AnimInstance montage gating, authoritative root
  motion, montage completion, and stable client convergence
- Walk coast on Authority, AutonomousProxy, and a newly joined SimulatedProxy
- Run coast below the 200 cm/s Walk cap on existing and newly joined proxies
- an actual simulated-proxy relevancy loss, actor/channel teardown, recreated proxy on return, and
  preservation of the authoritative Run coast classification
- deterministic coast-gait clearing to Idle at physical stop on every role
- SavedMove Run remains selected at exactly 0.65 on a movable base at 50-degree yaw, including
  the 500 cm/s cap and a three-second observation without recurring owner corrections
- active Run held at 0.69 survives initial proxy creation and actor/channel teardown/recreation
  without requiring that observer to witness the earlier Run-entry edge
- CombatStrafe exit followed by input release during replication delay converges authoritative,
  owner and observer capsule yaw through an actual CMC correction, then stays converged for one second
- same-base/bone owner correction while the platform moves more than the reset threshold without a
  false history reset on either client role
- a real relative owner divergence above the threshold producing exactly one AutonomousProxy reset
  while the moving SimulatedProxy remains stable
- explicit unit coverage for base/bone switches and unusable-base world-space fallbacks; UE's
  unresolved relative-base RPC is rejected before the project correction callback
- deterministic server-authoritative Sword/Shield equipment setup, combat-profile provider removal
  while the mesh cannot poll, neutral fallback, and reacquisition of one registered provider/profile
  on the same authority and simulated-proxy pawn/AnimInstance after `GF_Combat_Core` reactivation

The deliberate GameFeature reactivation currently emits ModularGameplay component-replacement
warnings. The lifecycle contract proves the final single-provider/profile state; it does not claim a
warning-free activation sequence.

Run from PowerShell with a rendered RHI; `NullRHI` is intentionally not used for this PIE test:

```powershell
& 'D:\Programme\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Repos\SurvivalRpg\SurvivalRpg.uproject' `
  -unattended -nop4 -nosteam -nosplash -nosound -RenderOffscreen `
  -stdout -FullStdOutLogOutput -NoLogTimes `
  '-ShaderWorkingDir=D:\Repos\SurvivalRpg\Intermediate\GaspNetworkShaders' `
  -ddc=InstalledNoZenLocalFallback `
  '-LocalDataCachePath=D:\Repos\SurvivalRpg\Intermediate\GaspNetworkDDC' `
  '-ini:Engine:[ConsoleVariables]:TestFramework.CQTest.CommandTimeout.Network=90' `
  '-ExecCmds=Automation RunTests SurvivalRpg.Network.GaspPilotPIE; Quit' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=D:\Repos\SurvivalRpg\Saved\Automation\GaspNetworkSmoke' `
  '-abslog=D:\Repos\SurvivalRpg\Saved\Logs\GaspNetworkSmoke.log'
```

Record the tested commit, UE version, test result, topology, network profile, report path, and log
path in the PR or issue. `Saved` reports are local evidence and are not committed.

Run the focused coast-gait contract with the same rendered-RHI options by substituting the test
name above and using dedicated `Issue101Network` report/log paths. The test uses one initial client,
late-joins two more clients during real CharacterMovement coast, and moves the first observer out
of and back into network relevancy while another client remains near the subject.

Run the focused moving-base contract the same way with the #103 test name and dedicated
`Issue103MovingBase` report/log paths. It late-joins one observer, keeps both views based while the
platform translates and rotates, then validates small and above-threshold relative corrections.

The suite command runs all nine `GaspPilotPIE` contracts. Run the separate rollback contract
with the same rendered command by replacing the test filter with
`SurvivalRpg.Network.PrototypeExperiencePIE.OverrideRemainsSelectable` and using dedicated
`Issue55PrototypeExperience` report/log paths.

The `GaspPilotPIE` tests clear the PIE Experience override before building their worlds, so they
exercise the global GASP fallback rather than a test-only pilot selection. The separate
`PrototypeExperiencePIE` contract sets the Prototype override before its world is built. Both suites
disable disk persistence on the concrete GameMode CDO and restore the original settings during
teardown. The owner and authority start the same dynamic montage through their ASCs, but this is not
a GameplayAbility activation or prediction confirmation test. After the authority is stopped,
the montage test requires clients to settle within 10 cm with speed at most 5 cm/s for 0.5 seconds.

The focused native filters `SurvivalRpg.Health.Lifecycle.DeathStateBeforeAbilitySystemInitialization`
and `SurvivalRpg.Animation.Threading.ServerAutonomousPoseConsumesEveryMoveDelta` cover reversed
DeathState/ASC initialization order and two autonomous mesh pose ticks in one engine frame.
The latter uses real graph updates in Listen/Dedicated net modes but no dedicated transport session.
Their implementations are in
[`RpgHealthComponentTests.cpp`](../Source/SurvivalRpg/Core/Character/RpgHealthComponentTests.cpp) and
[`RpgAnimationFoundationTests.cpp`](../Source/SurvivalRpgEditor/Private/Animation/RpgAnimationFoundationTests.cpp).

`SurvivalRpg.Character.RotationMode.HandoffTimestampBoundary` checks stale/equal/new correction
timestamps, timestamp resets, invalid values and cancellation.
`SurvivalRpg.Character.Movement.SavedMovePrediction` also exercises native `ForcePositionUpdate`
when the server extrapolates a missing owner move, retaining the last validated gait through the
same base-rounding boundary. These tests live in
[`RpgCharacterRotationModeTests.cpp`](../Source/SurvivalRpgEditor/Private/Character/RpgCharacterRotationModeTests.cpp)
and [`RpgCharacterMovementProfileTests.cpp`](../Source/SurvivalRpgEditor/Private/Character/RpgCharacterMovementProfileTests.cpp).

The coast test applies a deterministic low release-deceleration value only to in-memory copies of
the authority and owner movement profiles, then drives real CharacterMovement velocity. Teardown
restores the exact captured movement profiles and actor cull distance; no PawnData, Experience, or
other content asset is mutated or saved.

## Visual smoke boundary

The automated acceptance covers real replication, analog movement intent, lifecycle state, movement-history
reset plumbing, base-relative owner correction, correction/teleport convergence,
montage/root-motion plumbing, active/coasting Walk/Run initial replication, and the tested actor-channel
relevancy-return path. It cannot judge rendered pose choice or presentation quality. A rendered
manual pass should still inspect
start/stop/pivot/TIR, crouch, jump/landing, Foot Placement on uneven ground, Aim/CombatStrafe facing,
and correction for persistent mesh/capsule separation.

This runbook does not claim gameplay-notify or attack-window reliability, ability costs/cooldowns,
combos, equipment sockets, death/ragdoll presentation, packaged multi-process or Steam behavior,
relevancy-return behavior outside the focused gait contracts, arbitrary packet-loss patterns,
performance, memory, or packaged default/command-line Experience selection. Those remain in their
dedicated multiplayer/combat/cutover issues.

Do not close #55 or #99 from this in-process PIE evidence alone. Their same-build visual A/B capture,
real multi-process listen-server/late-join run, packaged Experience selection, and remaining
performance/memory gates still require separate evidence. The #55 matrix also retains manual
equipment/socket, block/dodge/hit/death/ragdoll, harvesting-reward, fresh/restored-profile, and
Experience-composition parity checks.
