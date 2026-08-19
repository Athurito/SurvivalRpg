# GASP real-network smoke

This runbook owns the reproducible PIE network acceptance for the isolated
`RpgGaspPilotExperience`. It exercises the real Experience, PawnData, GASP character, AnimBP,
CharacterMovement, Ability System Component, and replicated montage path without changing the
default PawnData or adding production runtime behavior.

## Automated acceptance

Test:

`SurvivalRpg.Network.GaspPilotPIE.ReplicationLateJoinCorrectionAndDefaultSlotMontage`

Topology and network profile:

- one-process PIE listen server
- one initial external client plus one external client joining during movement
- the original subject is observed as Authority, AutonomousProxy, and late-join SimulatedProxy
- `PktLag=60`, `PktLagVariance=10`, and no configured packet loss
- one server-spawned editor-only floor fixture provides a shared network-addressable movement base

The test verifies:

- the pilot Experience and GASP pawn composition on every world
- start, reversal/pivot, stop, and replicated acceleration reconstruction after late join
- grounded Foot Placement snapshot validity and the expected network roles
- replicated Aim and CombatStrafe presentation modes
- a deliberate 90-degree facing change activates and completes the turn-in-place lifecycle on all three views
- crouch, physical jump, landing, and return to grounded presentation
- owner-only movement divergence followed by server-authoritative correction and convergence
- local-owner and authoritative ASC montage starts through `DefaultSlot`
- replicated simulated-proxy montage playback, AnimInstance montage gating, authoritative root
  motion, montage completion, and stable client convergence

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
  '-ExecCmds=Automation RunTests SurvivalRpg.Network.GaspPilotPIE.ReplicationLateJoinCorrectionAndDefaultSlotMontage; Quit' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=D:\Repos\SurvivalRpg\Saved\Automation\GaspNetworkSmoke' `
  '-abslog=D:\Repos\SurvivalRpg\Saved\Logs\GaspNetworkSmoke.log'
```

Record the tested commit, UE version, test result, topology, network profile, report path, and log
path in the PR or issue. `Saved` reports are local evidence and are not committed.

The test temporarily selects the pilot Experience and disables disk persistence on the concrete
GameMode CDO. Both values are restored during teardown. The owner and authority start the same
dynamic montage through their ASCs, but this is not a GameplayAbility activation or prediction
confirmation test.

## Visual smoke boundary

The automation proves real replication, lifecycle state, movement correction, and montage/root
motion plumbing. It cannot judge rendered pose choice or presentation quality. A rendered manual
pass should still inspect start/stop/pivot/TIR, crouch, jump/landing, Foot Placement on uneven
ground, Aim/CombatStrafe facing, and correction for persistent mesh/capsule separation.

This runbook does not claim gameplay-notify or attack-window reliability, ability costs/cooldowns,
combos, equipment sockets, death/ragdoll presentation, packaged multi-process or Steam behavior,
relevancy return, packet-loss handling, performance, memory, or the default PawnData cutover. Those
remain in their dedicated multiplayer/combat/cutover issues.
