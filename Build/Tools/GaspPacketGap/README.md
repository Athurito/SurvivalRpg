# GASP packet-gap measurement

Runs the saved Mover test map in separate uncooked Unreal game processes. The
host traverses the authored one-metre Vault with existing Enhanced Input actions.
A client can drop incoming packets for 350 ms during the traversal. No asset,
transform, velocity, movement mode, simulation clock or montage speed is set.

Requires Windows, Python 3.11+, licensed UE 5.8.2 and a matching, built
`SurvivalRpgEditor` checkout. Prepare and verify the local plugin overrides using
[the patch instructions](../../Patches/NetworkPrediction/README.md) before building.
Coordinate with the checkout's editor/build owner; run one probe at a time on a
free port. The launcher does not close existing editors or rebuild stale binaries.

From the repository root, run a control and then the packet-gap case:

```powershell
python Build/Tools/GaspPacketGap/probe_launch.py control --engine D:/Programme/UE_5.8/Engine --without-observer --port 17994
python Build/Tools/GaspPacketGap/probe_launch.py gap350 --engine D:/Programme/UE_5.8/Engine --without-observer --port 17994 --observer-gap-ms 350
```

Adjust `--engine` to your Engine directory. Defaults are host-driven running
Vault, 60 FPS on both processes, lane 0 and 15 seconds of warmup. Gap injection
requires `--driver host`; it affects receiving clients, never Authority input.
The process named `owner` observes the host's pawn as a **SimulatedProxy** here.
Read `player_id` and `net_role`, not the filename, when assigning network roles.

Evidence goes to `Saved/GaspPacketGap/probe_run_LABEL`; `--output` may select
another directory under this checkout's `Saved`. Use a new label for every attempt.
The launcher refuses existing run directories, verifies plugin sources, records
script/binary hashes, checks all maps and SaveGames before/after, and shuts down
only its exact child handles. A failed or forced shutdown makes the run fail.
Temporary packet loss is restored on completion, abort and quit. Logs retain
startup warnings/errors as well as the measurement; a successful route does not
mean a warning-free engine startup. Existing binary hashes are provenance, not
proof that they match the current source.

Analyze a completed run:

```powershell
python Build/Tools/GaspPacketGap/probe_analyze.py Saved/GaspPacketGap/probe_run_control
python Build/Tools/GaspPacketGap/phase_analyze.py Saved/GaspPacketGap/probe_run_control
python Build/Tools/GaspPacketGap/probe_analyze.py Saved/GaspPacketGap/probe_run_gap350
python Build/Tools/GaspPacketGap/probe_gap_analyze.py Saved/GaspPacketGap/probe_run_gap350
python Build/Tools/GaspPacketGap/phase_analyze.py Saved/GaspPacketGap/probe_run_gap350
python -m unittest discover -s Build/Tools/GaspPacketGap -p 'test_*.py' -v
```

- `analysis-v2.json` retains the original first-forward-X-crossing phase criterion
  and tolerance. Read its `passed` result; this CLI historically exits zero even
  when that criterion fails. A nonmonotone authority path or accelerated recovery
  phase makes this criterion an incomplete geometry assessment.
- `gap-analysis.json` checks that montage and movement freeze together during an
  actual completed packet pause. Do not apply it to the no-gap control.
- `phase-analysis.json` compares visual component roots at equal montage phase.
  It includes signed XYZ/3D residuals, both authority bracket endpoints, the nearest
  raw authority sample and its phase difference. It is diagnostic and has **no
  pass threshold**. Repeated/ambiguous plays and missing brackets are excluded;
  missing coverage never establishes success. Post-gap means after packet receipt
  was re-enabled, not a measured internal NetworkPrediction recovery boundary.

These are root-position observations, not bone/contact-pose readback. They do not
capture native NP receive endpoints, exact cross-peer GAS play identities or
montage-instance callbacks, and do not replace packaged/WAN, late-join, rollback
or lifecycle tests. Linear reconstruction of a missing curved path remains a
known runtime limitation; this tool does not change that policy or loosen the
original tolerance. See [NET-03 results](../../../docs/gasp-packet-gap-recovery.md).
