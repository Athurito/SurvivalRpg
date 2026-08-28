# Melee combat network smoke

This runbook owns the reproducible network acceptance for remote-client melee attack windows in
issue #57. GAS and the server remain authoritative for activation, attack-window timing, traces,
hit deduplication, and damage. Montage notifies remain designer-authored presentation/gameplay
signals, while the server derives its authoritative one-shot schedule from the same authored notify
times.

The tests temporarily select `RpgGaspPilotExperience`; they do not save or mutate Experience,
PawnData, Game Feature, montage, weapon, or AnimBP assets. Experience switching remains intact.

## Automated asset contract

Test:

`SurvivalRpg.Combat.WeaponAttackAssetContract`

The contract covers every attack definition on the Basic Sword and Basic Two-Handed Sword. It
requires a finite positive play rate, exactly one montage section starting at zero with no section
link or jump, no time-stretch curve, and exactly one ordered direct project start/end notify pair.
Both notifies must use 100% trigger chance, queued delivery, `NotifyFilterType=NoFiltering`, and
dedicated-server delivery. Request-time filtering remains outside this asset contract; authority
damage and traces do not depend on notify delivery. The end notify must occur before normal auto
blend-out begins.

Run from PowerShell:

```powershell
& 'D:\Programme\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Repos\SurvivalRpg\SurvivalRpg.uproject' `
  -unattended -nop4 -nosteam -nosplash -nosound -NullRHI `
  -stdout -FullStdOutLogOutput -NoLogTimes `
  -ddc=InstalledNoZenLocalFallback `
  '-LocalDataCachePath=D:\Repos\SurvivalRpg\Intermediate\GaspNetworkDDC' `
  '-ExecCmds=Automation RunTests SurvivalRpg.Combat.WeaponAttackAssetContract; Quit' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=D:\Repos\SurvivalRpg\Saved\Automation\Issue57AssetContract' `
  '-abslog=D:\Repos\SurvivalRpg\Saved\Logs\Issue57AssetContract.log'
```

## Automated rendered network contract

Test:

`SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation`

Topology and network profile:

- one-process PIE listen server with one external remote client
- real `RpgGaspPilotExperience`, GASP pawn, ASC input route, Basic Sword, equipment grant,
  predicted GameplayAbility, montage, sockets, traces, GameplayEffect, and health state
- `PktLag=60`, `PktLagVariance=10`, and no configured packet loss
- rendered offscreen RHI; `NullRHI` is intentionally not used

The test performs 20 sequential remote-client attacks at the authored rate, one additional attack
with a transient 1.5x montage-rate override, and one cancellation during the open authority
window. The override is applied independently to the server and owner weapon instances and is
restored after complete cleanup. For each completed attack it requires:

- the exact server-granted ability handle and current MainHand weapon on the owner
- one owner input against the exact granted ability and a corresponding successful server attack
- exactly one authority window open and close
- authority trace samples and at least 2 cm of live authority blade-center movement while the
  window remains open
- exactly one real health decrease with the expected weapon source, instigator, and causer
- stable montage, timer, trace, hit-dedupe, and ability cleanup after completion

The 1.5x timing/scaling case additionally requires the actual montage effective rate to match the
override while the normal authority-window, trace, damage, and cleanup assertions still pass.
Lifecycle logs record the derived authority schedule delays for review; automation does not assert
their exact numeric values. The cancellation case requires one authority open and close, zero
damage, no residual timers or attack state, and no mutation during the post-cleanup observation
period.

Run from PowerShell:

```powershell
& 'D:\Programme\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Repos\SurvivalRpg\SurvivalRpg.uproject' `
  -unattended -nop4 -nosteam -nosplash -nosound -RenderOffscreen `
  -stdout -FullStdOutLogOutput -NoLogTimes `
  '-ShaderWorkingDir=D:\Repos\SurvivalRpg\Intermediate\GaspNetworkShaders' `
  -ddc=InstalledNoZenLocalFallback `
  '-LocalDataCachePath=D:\Repos\SurvivalRpg\Intermediate\GaspNetworkDDC' `
  '-ini:Engine:[ConsoleVariables]:TestFramework.CQTest.CommandTimeout.Network=90' `
  '-ExecCmds=Automation RunTests SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation; Quit' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=D:\Repos\SurvivalRpg\Saved\Automation\Issue57CombatNetwork' `
  '-abslog=D:\Repos\SurvivalRpg\Saved\Logs\Issue57CombatNetwork.log'
```

Record the tested commit, UE version, result, topology, network profile, report path, log path,
request/predicted/accepted/rejected log counts, authority open/close counts, damage count, derived
schedule delays for the 1.5x case, and any notify fallbacks in
the PR or issue. `Saved` reports and logs are local evidence and are not committed.

## Packaged Steam close gate

Automation proves the tested GASP remote-client path under PIE latency and jitter. Issue #57 still
requires a packaged Development build with two separate Steam processes/accounts before merge or
issue closure. Run the following matrix for both `RpgPrototypeExperience` and
`RpgGaspPilotExperience`:

| Actor | Movement | Input cadence | Minimum attacks |
| --- | --- | --- | ---: |
| Remote client | Standing | Normal | 20 |
| Remote client | Standing | Fast | 20 |
| Remote client | Moving | Normal | 20 |
| Remote client | Moving | Fast | 20 |
| Listen-server host control | Standing and moving | Normal and fast | 20 total |

Use practical latency/jitter for at least one complete remote-client run. Test downed and otherwise
blocked activation separately. Correlate every client request with an accepted activation or a
rejection carrying a reason. For every accepted attack that reaches its damage section, verify one
authority open, one close, at least one authority trace sample, no trace timer outside the window,
and at most one damage application per target. There must be no unexplained missing remote window
or trace, and host attacks must remain reliable.

Enable `Log Weapon Attack Lifecycle` in Combat developer settings for correlated activation,
montage, window, trace, cancellation, cleanup, dedupe, and damage records. Enable `Draw Weapon
Attack Traces` only as supporting visual evidence; persistent debug shapes are not timing proof.

## Boundary

The rendered test proves the current remote GASP authority lifecycle, real damage path, timer
cleanup, deduplication, and live server blade-pose movement against a fixed pre-input Basic Sword
contact fixture. It does not judge visual combat polish, exact rendered client/server blade
alignment, moving-target contact, fast-input behavior, host attacks, Prototype parity,
rejected/downed activations,
multi-process packaged Steam transport, packet loss, or late join. Those remain manual close-gate
checks here and feed the broader multiplayer acceptance in issue #55.
