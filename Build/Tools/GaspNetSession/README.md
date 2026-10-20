# GASP packaged network sessions

Starts a packaged SurvivalRpg build as separate game processes for `GASP-VAL-02`
([task report](../../../docs/gasp-packaged-network-validation.md)) and summarises
their logs. People play the routes in the windows. Packaged builds have no
Python, so this tool only launches, applies the network profile, optionally
captures each game window once, and stops.

Package first:

```powershell
python Build/Tools/Unreal/ue.py package Lvl_RpgGaspCMC Lvl_RpgGaspMantle Lvl_RpgGaspMover Lvl_RpgGaspMoverRagdoll Lvl_RpgGaspPaladin BootMenu MainMenu
```

Then start a session and analyse it:

```powershell
python Build/Tools/GaspNetSession/session.py mantleAverage --map Lvl_RpgGaspMantle --clients 2 --late-join 60 --fps 60,30,120 --profile average
python Build/Tools/GaspNetSession/analyze.py Saved/GaspVal02/session_mantleAverage
```

- **Processes:**
  - The host opens the map with `?listen`.
  - Clients join `127.0.0.1` with a fresh UUID-v4 `PlayerProfileId`, which the server requires without Steam. All processes run with `-nosteam`.
  - `--clients 0 --solo` plays the map alone.
  - `--late-join` delays the last client.
  - `--fps` caps host and clients in that order; the last value repeats.
  - The tool starts the real binary under `SurvivalRpg/Binaries/Win64`, not the bootstrap `SurvivalRpg.exe`.
- **Profiles** (`--profile`): `none`, `good` (50 ms round trip, 10 ms jitter, 0 % loss), `average` (120 ms, 30 ms, 1 %) and `bad` (250 ms, 60 ms, 5 %).
  - Every process delays its outgoing packets by half the latency (`NetEmulation.PktLag`) with half the jitter (`NetEmulation.PktLagVariance`).
  - Every process drops the given share of outgoing packets (`NetEmulation.PktLoss`).
- **Logging:** every process logs CMC corrections (`p.NetShowCorrections 1`, drawn for only 0.01 s) and NetworkPrediction reconciles (`np.PrintReconciles 1`). Both exist only in non-Shipping builds.
- **Captures:** `--capture N` saves each game window once, N seconds after the last process started, with `PrintWindow`. It never captures the desktop.
- **End of a session:** the session ends when every window is closed, or after `--minutes`. It then stops the remaining processes.
- **Evidence:** `Saved/GaspVal02/session_<label>` holds `<role>.log`, `manifest.json` and the captures.
- **`analyze.py`:** writes `summary.json` and prints, per role:
  - the loaded Experience and joins (counted on the host);
  - server and client corrections (count, maximum and median distance);
  - reconciles, network failures, ensures, crashes and unexplained `Error:` lines.

  It fails when a role crashed or logged an error that is not listed as known noise.

Tests: `python -m unittest discover -b -s Build/Tools/GaspNetSession -p "test_*.py"`
