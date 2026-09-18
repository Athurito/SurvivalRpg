# UE 5.8.2 Fixed interpolation recovery

This is a reproducible source patch for the engine NetworkPrediction plugin.
It does not modify the installed Engine and does not redistribute complete Epic
plugin sources. A licensed, exact UE 5.8.2 installation supplies those files.

## Prepare before building

Close Unreal Editor first, then from the repository root:

```powershell
python Build/Patches/NetworkPrediction/prepare.py check --engine D:/Programme/UE_5.8/Engine
python Build/Patches/NetworkPrediction/prepare.py stage --engine D:/Programme/UE_5.8/Engine
python Build/Patches/NetworkPrediction/prepare.py verify
```

Now run the normal `SurvivalRpgEditor` or `SurvivalRpg` build. Do not build only
the original installed plugin or assume an old Editor binary contains this fix.
The module-load log must resolve NetworkPrediction to
`SurvivalRpg/Plugins/NetworkPrediction/Binaries/Win64`, not the installed Engine.
The build must also compile the newly staged Mover consumer modules.

The script checks the UE version and all original source/content SHA-256 hashes,
applies exact-context patch hunks in memory, verifies the expected result, then
creates four ignored project-local plugin overrides:

- NetworkPrediction: patched implementation and focused native test.
- Mover, ChaosMover, MoverExamples: unchanged licensed sources/content.

The consumer copies are required by UnrealBuildTool's Engine-to-project-module
dependency rule. All four are enabled in this project. Their original asset
mount names and public module layouts stay unchanged. No binaries, intermediate
files or engine caches are copied. Further plugins consuming Mover or
NetworkPrediction require reviewing this closure before enabling them.

The manifest records original and patched hashes. Never casually regenerate it
after an Engine update: review the upstream implementation and dependencies first.
Existing or partial overrides are rejected instead of overwritten. If all four
copies exist without staging markers and every file still matches the pristine
baseline, staging can safely resume after an interrupted initial copy.
All traversed paths are checked before reads or writes: symbolic links, junctions,
other Windows reparse points and paths escaping the plugin root are rejected,
including markers and generated binary directories. Source/content hard links
are rejected too, because writing them could otherwise modify their Engine source.

## Correction

The Fixed interpolation clock can remain permanently behind after a network
dispatch stall. The engine previously advanced normally or stopped when empty,
but had no equivalent of Independent ticking's excess-buffer recovery.

Before Fixed interpolation services reconcile, the patch bounds excess buffered
frames and recenters the shared presentation clock to the configured target.
`np.FixedInterpolation.MaxBufferedMS` defaults to 250 ms. Both tuning values are
constrained by this pinned engine's 64-frame history, reserving valid From and To
frames. Recovery only moves presentation forward; simulation, input, authority,
rollback offsets and montage speeds remain untouched.

A game-thread-only map carries a one-frame recovery marker from Reconcile to
BeginNewSimulationFrame for each world. Begin always consumes it and Deinitialize
removes it. It contains no clock or history and adds no field to any exported
type. The marker prevents the same hitch delta from immediately consuming the
fresh target buffer, including a moderate 160 ms hitch. Normal advancement is
bounded by the newest received frame and can reach its exact endpoint without
extrapolation or retaining excess wall time. Existing services still reconcile
and finalize once; their history and cue ownership remain intact.

Verbose recovery diagnostics contain the exact prefix
`Fixed interpolation recovered backlog:` and report previous/current/received
frames and buffer milliseconds. No per-frame log is added.

## Verification and rollback

Run `SurvivalRpg.NetworkPrediction.FixedInterpolationRecovery.Clock`. It calls
the same helper used by the production module and covers ordinary fractional
progress, large and moderate hitch recovery, repeated recovery, shared AP/SP
receipts, history-wrap boundary, excessive tuning and received-state starvation.
This clock test does not replace separate-process tests of actual movement,
corrections, late joins, traversal and observer latency after stalls.

`python Build/Patches/NetworkPrediction/test_prepare.py` covers the installer's
patch destination, idempotence, verified interrupted-copy recovery and preservation
of user-modified overrides using tiny synthetic plugin trees.

No build or runtime success is implied by staging or `verify`; record those
results separately. `verify` checks source/content, deliberately excluding build
outputs. Runtime module paths and actual probes establish which binary ran.

To remove the override, close Editor and run:

```powershell
python Build/Patches/NetworkPrediction/prepare.py remove
```

Generated overrides are ignored by Git, so checking out another branch does not
remove them. Run `remove` **before** switching this checkout to an unpatched
branch, then rebuild there. Other checkouts and the installed Engine are unaffected.

Removal first verifies every source/content file, refuses unknown or modified
files, checks each resolved path remains the exact expected repository plugin
directory, and then deletes only those four generated copies. Rebuild against
the original Engine afterward. The installed plugin has remained untouched.
On a branch requiring the patch, use the repository's build guard as intended;
rollback the patch change as a whole before resuming unpatched builds.
