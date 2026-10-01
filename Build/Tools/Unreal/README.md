# Unreal tooling

`ue.py` builds, tests and cooks this checkout from the repository root. The
other Python files here are optional editor-side Unreal MCP toolsets
(`import unreal`) that an editor Python bootstrap loads. Their purpose,
registration and invocation are described in
[unreal-mcp-asset-authoring.md](../../../.agents/skills/unreal-lyra-expert/references/unreal-mcp-asset-authoring.md#project-toolsets).

```powershell
python Build/Tools/Unreal/ue.py build
python Build/Tools/Unreal/ue.py test SurvivalRpg.Combat.Block.Lifecycle
python Build/Tools/Unreal/ue.py cook Lvl_RpgGaspMantle
python Build/Tools/Unreal/ue.py engine
```

Every run prints the log path first, then one verdict line, the relevant
errors, failed tests or warnings, and the paths to the full log and report:

```text
TEST FAILED in 74s: 3 run: 1 passed (0 with warnings), 1 failed, 1 not run
FAIL SurvivalRpg.Inventory.UI.BaseTerminalSpatialComposition
  Expected 'Terminal authors the featured upgrade as designer data' to be ... (Source/...Tests.cpp:212)
NOT RUN SurvivalRpg.Inventory.UI.BaseTerminalContextLifecycle (NotRun)
report: Saved/ToolRuns/20261001-120000-test/report/index.json
log: Saved/ToolRuns/20261001-120000-test/editor.log
```

| Exit code | Meaning |
| ---: | --- |
| 0 | Passed. |
| 1 | Failed: build or cook errors, a failed or skipped test, a filter that matched no test, an unexpected test count, a crash, a timeout or Ctrl+C. |
| 2 | Setup or usage error; Unreal did not start. |

Each run writes to a new folder under `Saved/ToolRuns/<time>-<command>`, or to
the new or empty folder given by `--out`. Like all of `Saved/`, it is local
evidence and is not committed. A test run keeps `editor.log`, the console output
in `console.log` and the automation report in `report/index.json`.

## Engine

The engine install root is taken from, in this order:

1. `--engine <root>` (before or after the command)
2. the `SURVIVALRPG_UE_ROOT` environment variable
3. the registry entry for the `EngineAssociation` in `SurvivalRpg.uproject`:
   launcher installs under `HKLM\SOFTWARE\EpicGames\Unreal Engine\<version>`,
   source builds under `HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds`
4. the "Local environment" entry in `AGENTS.md`

The root or its `Engine` folder are both accepted, and its major and minor
version must match the association. A wrong `--engine` or environment value
fails instead of falling through. `ue.py engine` prints the resolved `Engine`
folder on stdout and its source on stderr, so it can feed other tools:

```powershell
python Build/Patches/NetworkPrediction/prepare.py stage --engine (python Build/Tools/Unreal/ue.py engine)
```

## Overrides

Before Unreal starts, every command except `engine` runs
`Build/Patches/NetworkPrediction/prepare.py verify` and stops with exit code 2
when the checkout-local NetworkPrediction/Mover overrides are missing or
modified. A test run also warns when the log shows NetworkPrediction loaded from
outside this checkout.

## Commands

`build` runs `Build.bat` for `SurvivalRpgEditor Win64 Development` with
`-WaitMutex -NoHotReloadFromIDE`. `--target SurvivalRpg` builds the game and
`--config` picks `DebugGame` or `Shipping`. Live Coding in an open editor
blocks the build; close the editor or compile there with Ctrl+Alt+F11.

`test` starts `UnrealEditor-Cmd.exe` on `/Engine/Maps/Entry` with rendered
offscreen PIE and runs `Automation RunTests` with the filters joined by `+`.
Filters follow RunTests: a substring, `^start`, `end$`, `StartsWith:` or
`Group:`. The verdict comes from the report, never from the editor exit code,
which stays 0 even when tests fail or none match.

| Option | Effect |
| --- | --- |
| `--exec "<command>"` | Console command before the tests, for example `--exec "np.ForceReconcile 0" --exec "t.MaxFPS 30"`. No commas. |
| `--log-cmds "<category> <level>"` | Log verbosity, for example `--log-cmds "LogRpgAbilitySystem Verbose"`. |
| `--expect-tests N` | Fails unless the report holds exactly N tests. |
| `--null-rhi` | No rendering. Faster for native and asset tests; never for PIE network tests. |
| `--map <map>` | Another startup map. |
| `--timeout <minutes>` | Default 60; stops the whole process tree. |

`cook` runs the cook commandlet for `Windows` with `-Map=` set to the given maps.
A map is a long package path such as `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle`
or a unique map name; unknown and ambiguous names fail before Unreal starts.
Cooked output lands in `Saved/Cooked/<platform>`. `--platform WindowsServer`
cooks for a dedicated server. The verdict and the listed errors and warnings
come from the commandlet's unique warning/error summary.

`build`, `test` and `cook` pass further Unreal arguments with
`--ue-arg=-Switch=Value`; the `=` form is required because the value starts with
a dash. Example:
`--ue-arg=-ini:Engine:[ConsoleVariables]:TestFramework.CQTest.CommandTimeout.Network=90`.

## Tests

```powershell
python -m unittest discover -b -s Build/Tools/Unreal -p "test_*.py"
```

They cover engine resolution, log and report summaries, filter matching and map
resolution without Unreal; CI runs them on every pull request.
