# GASP real-network smoke

This runbook owns the reproducible PIE network acceptance for the isolated
`RpgGaspPilotExperience`. It exercises the real Experience, PawnData, GASP character, AnimBP,
CharacterMovement, Ability System Component, and replicated montage path without changing the
default PawnData or introducing an alternate gameplay/runtime owner.

## Automated acceptance

Primary test:

`SurvivalRpg.Network.GaspPilotPIE.ReplicationLateJoinCorrectionAndDefaultSlotMontage`

Focused issue #101 coast-gait test:

`SurvivalRpg.Network.GaspPilotPIE.GroundCoastLateJoinAndRelevancyReturn`

Focused issue #100 analog-gait regression:

`SurvivalRpg.Network.GaspPilotPIE.AnalogGaitPredictionAndCorrection`

Focused issue #103 moving-base correction regression:

`SurvivalRpg.Network.GaspPilotPIE.MovingBaseCorrectionPreservesAnimationHistory`

Topology and network profile:

- one-process PIE listen server
- one initial external client, one external client joining during movement, and one final external
  client joining while the original subject is stationary
- the original subject is observed as Authority, AutonomousProxy, and SimulatedProxy on both
  late-join clients
- `PktLag=60`, `PktLagVariance=10`, and no configured packet loss
- one server-spawned editor-only floor fixture provides a shared network-addressable movement base
- the #103 focus uses a replicated platform with deterministic fast translation and rotation, one
  AutonomousProxy owner, and a late-joined SimulatedProxy observer

The test verifies:

- the pilot Experience and GASP pawn composition on every world
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
- same-base/bone owner correction while the platform moves more than the reset threshold without a
  false history reset on either client role
- a real relative owner divergence above the threshold producing exactly one AutonomousProxy reset
  while the moving SimulatedProxy remains stable
- explicit unit coverage for base/bone switches and unusable-base world-space fallbacks; UE's
  unresolved relative-base RPC is rejected before the project correction callback

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

The test temporarily selects the pilot Experience and disables disk persistence on the concrete
GameMode CDO. Both values are restored during teardown. The owner and authority start the same
dynamic montage through their ASCs, but this is not a GameplayAbility activation or prediction
confirmation test.

The coast test applies a deterministic low release-deceleration value only to in-memory copies of
the authority and owner movement profiles, then drives real CharacterMovement velocity. Teardown
restores the exact captured movement profiles and actor cull distance; no PawnData, Experience, or
other content asset is mutated or saved.

## Visual smoke boundary

The automation proves real replication, analog movement intent, lifecycle state, movement-history
reset plumbing, base-relative owner correction, correction/teleport convergence,
montage/root-motion plumbing, Walk/Run coast initial replication, and the tested actor-channel
relevancy-return path. It cannot judge rendered pose choice or presentation quality. A rendered
manual pass should still inspect
start/stop/pivot/TIR, crouch, jump/landing, Foot Placement on uneven ground, Aim/CombatStrafe facing,
and correction for persistent mesh/capsule separation.

This runbook does not claim gameplay-notify or attack-window reliability, ability costs/cooldowns,
combos, equipment sockets, death/ragdoll presentation, packaged multi-process or Steam behavior,
relevancy-return behavior outside the focused coast-gait contract, packet-loss handling,
performance, memory, or the default PawnData cutover. Those remain in their dedicated
multiplayer/combat/cutover issues.
