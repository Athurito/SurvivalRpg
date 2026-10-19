# GASP-VAL-02 – Packaged build and network conditions

Status: **In progress** since 2026-10-19 on `claude/gasp-val-02-packaged-network`. Defined in [#270](https://github.com/Athurito/SurvivalRpg/pull/270), and the user started it the same day. This report is the task report.

## Goal

The accepted GASP movement works outside the editor and over a network that is neither instant nor lossless. This covers:
- CMC with the MetaHuman Paladin;
- Mover;
- Mover-Ragdoll.

Concretely:
- A packaged Development build starts, loads every GASP test map and shows the same characters, armor, traversal, combat, death and respawn as PIE.
- Separate game processes play together as a listen server with clients, also at different frame rates and with late join.
- Under emulated latency, jitter, packet loss and a short packet pause, movement, traversal, corrections, combat and respawn stay correct and readable.

Findings become follow-up tasks. This task measures and documents; it fixes only trivial packaging blockers.

## Why

- All movement evidence so far comes from editor PIE.
  - Mostly one process, without latency.
  - Cooks were checked, but never run.
  - Only a few loopback runs in separate uncooked processes, with one 350 ms packet pause (`GASP-NET-03`).
- The handoff lists this as the open prerequisite for production readiness (`GASP-VAL-02`).
- Since `GASP-17`, the production CMC Experiences carry the MetaHuman Paladin. It brings grooms, RigLogic, LODSync, cloth and many textures, which cooked builds treat differently from the editor.

## Current state (checked 2026-10-19 on `master` `8e497136`)

| Area | State |
| --- | --- |
| Engine | Installed UE 5.8.3 (`Engine/Build/InstalledBuild.txt`). The project has a Game and an Editor target. An installed engine cannot build a dedicated server target. |
| Tooling | `Build/Tools/Unreal/ue.py` builds, tests and cooks; `--platform WindowsServer` only cooks. There is no packaging command. |
| Earlier cooks | `ue.py cook` passed with 0 errors for `Lvl_VfxGallery`, `Lvl_HarvestPickaxe`, `Lvl_RpgInventoryStorage` and the GASP maps of earlier tasks. None of these cooks was packaged or run. |
| Separate processes | `Build/Tools/GaspPacketGap` starts uncooked game processes (`UnrealEditor.exe -game`) for the Mover vault map. It drives real Enhanced Input from a Python probe, injects packet loss with `NetEmulation.PktIncomingLoss` and records role-tagged evidence. It covers only the Mover vault route. |
| Network config | `DefaultPlatformService=Steam` (`SteamDevAppId=480`); the game net driver is `SteamSocketsNetDriver` with an `IpNetDriver` fallback. Local tests without Steam need `-nosteam`. |
| Maps and Experiences | `Lvl_RpgGaspCMC` and `Lvl_RpgGaspMantle` (CMC with Paladin), `Lvl_RpgGaspPaladin` (armor chest), `Lvl_RpgGaspMover`, `Lvl_RpgGaspMoverRagdoll`. The game starts on `BootMenu`. |
| Plugin overrides | `SurvivalRpg.Target.cs` requires the prepared NetworkPrediction/Mover overrides. A package must contain them, not the engine's unpatched plugins. |
| Open network findings | `GASP-NET-03`: after a packet pause, the root deviates up to 64 cm at the same phase. Recurring PIE-only noise: NetGUID and voice warnings, and teardown crashes. |

## Scope

1. **Phase A – packaging seam.**
   - Add a `package` command to `ue.py`. It runs `BuildCookRun` for Win64 Development with the GASP maps, `BootMenu` and the GameFeature content, and stages to `Saved/Packaged/<label>`.
   - Like `build`, it first checks the plugin overrides. It prints a compact verdict with log paths and the staged size.
   - Document it in `Build/Tools/Unreal/README.md`.
   - Fix only blockers that stop packaging, such as a missing map in the cook list or a plugin that is not packaged. Report everything else.
2. **Phase B – packaged smoke, one process.**
   - Start the package once per GASP map and check:
     - spawn and Experience load;
     - walk, sprint and crouch; Mantle, Vault and Hurdle;
     - sword attack and block; death and respawn;
     - on `Lvl_RpgGaspPaladin`: chest, inventory equip, body masks, grooms and level-of-detail switches.
   - Search the logs for missing assets and errors that PIE does not show.
   - Measure the frame time with one Paladin against PIE.
3. **Phase C – separate processes, loopback.**
   - Packaged listen server plus two clients as separate processes on this machine, with `-nosteam` and the IP driver.
   - Include a late join, and frame rates 30, 60 and 120 FPS mixed between host and clients.
   - Extend `Build/Tools/GaspPacketGap` (or a sibling tool) for the scripted part. Its uncooked processes drive real input for CMC routes as well as the Mover vault: walk, sprint, Mantle, Vault, Hurdle and stop. A packaged build has no Python, so packaged routes are played by hand.
4. **Phase D – emulated network conditions.** UE packet simulation (`NetEmulation.*`) in Development builds, for each profile on the CMC and the Mover:

   | Profile | Latency | Jitter | Loss |
   | --- | --- | --- | --- |
   | Good | 50 ms | 10 ms | 0 % |
   | Average | 120 ms | 30 ms | 1 % |
   | Bad | 250 ms | 60 ms | 5 % |
   | Pause | Average plus one 350 ms incoming packet pause during a traversal | | |

   Scenarios:
   - starts, stops, pivots and sprint;
   - Mantle, Vault and Hurdle on a remote client;
   - combat hit and block;
   - inventory equip of Paladin armor;
   - death and respawn;
   - late join under the profile;
   - on the Mover, fixed-tick rollback.

   Evidence per run: role-tagged logs, correction counts (CMC server corrections; Mover NetworkPrediction reconciles), CSV profiles, captures of remote characters, and the scripted probes from Phase C where they apply.

   Compare with the PIE results and `GASP-NET-03`.
5. **Phase E – real WAN (deferred).**
   - Two machines over the internet, through Steam (App 480 needs two Steam accounts) or a direct IP connection.
   - The user cannot provide a second machine or player now (2026-10-19). This run leaves Phase E out; it stays open as a later check.
6. **Phase F – report and user check.**
   - Results per variant, profile and scenario, with findings as proposed follow-up tasks.
   - The user plays the packaged build as a listen server with a client under the Average profile.

## Out of scope

- A packaged dedicated server. It needs a source-built engine; the `WindowsServer` cook remains possible but is not run.
- Shipping configuration, consoles, Linux, Steam lobby and session UI, and matchmaking.
- Fixing the findings, except trivial packaging blockers. Performance work beyond recording the cost.
- Real WAN (Phase E, deferred by the user).
- `GASP-VAL-01`, `GASP-STAB-05`, and the unplanned leftovers in the handoff.

## Decisions

- 2026-10-19 (user): define `GASP-VAL-02`, merge the definition and start it.
- 2026-10-19 (user): no real WAN test now. Phase E is deferred, and the WAN part of the original `GASP-VAL-02` finding stays open.
- 2026-10-19 (user): the user plays the packaged routes by hand with launch scripts from this task. No development-only route command is added.
- 2026-10-19 (user): the emulation profiles in Phase D stay as proposed.

## C++ boundary

- **Classification:**
  - Validation and tooling.
  - `ue.py package` and the probe extension are Python tooling under `Build/Tools`.
  - The task changes no runtime schema or content.
- **Runtime truth:** unchanged. CMC/Mover movement, GAS, equipment and Character Parts stay as merged.
- **Existing seams:**
  - `Build/Tools/Unreal/ue.py`;
  - `Build/Tools/GaspPacketGap`;
  - UE's `BuildCookRun` and `NetEmulation.*`;
  - the CSV profiler;
  - the GASP test maps and their Experiences.
- **New native classes:** none. The user chose to play the packaged routes instead of a development-only route command.
- **Asset work:** none planned. Packaging blockers in asset references are fixed through the Unreal MCP workflow.

## Ownership

| Concern | Owner |
| --- | --- |
| Packaging command | `Build/Tools/Unreal/ue.py` and its README |
| Scripted separate-process routes | `Build/Tools/GaspPacketGap` or a sibling under `Build/Tools` |
| Packaged manual routes | The user, with launch scripts from this task |
| Evidence | `Saved/GaspVal02` (local); summaries in this report and the PR |
| Findings | Proposed follow-up tasks in the roadmap, only after the user marks them Ready |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Package | `ue.py package` produces a Win64 Development build containing every GASP map, the GameFeatures and the patched NetworkPrediction/Mover plugins. The staged size and the warnings are recorded. |
| Smoke | Each GASP map runs in the package with the characters, traversal, combat, death, respawn and Paladin features listed in Phase B. Every log error is explained. |
| Separate processes | A listen server with two clients and a late join works at mixed frame rates, for CMC and Mover. |
| Network conditions | Every profile in Phase D was run on the CMC and the Mover, with every scenario. Correction counts, visible artefacts and gameplay outcomes are recorded and compared with PIE. |
| Findings | Each deviation is described with its reproduction, profile and evidence, and proposed as a follow-up task or accepted as a limit. |
| User check | The user played the packaged build under the Average profile. |

## Verification

- Build, package, test and cook through `Build/Tools/Unreal/ue.py`.
- The tooling has unit tests where it parses evidence, as `Build/Tools/GaspPacketGap` does.
- Raw evidence goes to `Saved/GaspVal02`. This report and the PR carry the summary.

## Repro (after implementation)

1. Run `python Build/Tools/Unreal/ue.py package` and start the staged build on `Lvl_RpgGaspMantle`.
2. Start a listen server with `-nosteam` and two clients, one of them later, then apply a profile with `NetEmulation.*`.
3. Walk, sprint and traverse; fight, die and respawn; equip Paladin armor. Watch the remote characters on every window.
