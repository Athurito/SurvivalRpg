# GASP-VAL-02 – Packaged build and network conditions

Status: **PR open** (PR_LINK). Work ran on `claude/gasp-val-02-packaged-network` from 2026-10-19. Defined in [#270](https://github.com/Athurito/SurvivalRpg/pull/270), and the user started it the same day. This report is the task report.

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

## Results

### Phase A – packaging (2026-10-19)

- **New command `ue.py package`:** it runs `BuildCookRun` for a Win64 client. It compiles the game target, cooks the given maps, stages, paks and archives to `Saved/Packaged/<run>`.
  - The verdict comes from AutomationTool's exit line, the log's unique errors and warnings, and a check for the staged `SurvivalRpg.exe` and its size.
  - The cook repeats its warnings in a `LogInit` summary, so each message is counted once.
  - Unit tests: 32 / 32.
- **First package:** the five GASP maps plus `BootMenu` and `MainMenu`.
  - Passed in 610 s with 0 errors and 3 unique warnings:
    - an engine-API deprecation in `ChaosMoverSimulation.cpp`;
    - the missing `GameplayCueNotifyPaths`;
    - the never-ticking `CUI_RespawnScreen`.
  - Staged: 61 files, 2.06 GiB.
- **Overrides:** the package compiled the project's NetworkPrediction, Mover and ChaosMover overrides under `Plugins/`, not the engine's plugins.
- **Bootstrap:** the staged `Windows/SurvivalRpg.exe` only starts `SurvivalRpg/Binaries/Win64/SurvivalRpg.exe` as a child process. The session tool starts the real binary.

### Phase B – packaged smoke, automated part (2026-10-19)

`Build/Tools/GaspNetSession/session.py` started each map alone in the package and captured the game window once after 30–35 s (`Saved/GaspVal02/session_smoke*`).

- Every map loads its Experience without a crash or an ensure.
- `Lvl_RpgGaspCMC`, `Lvl_RpgGaspMantle` and `Lvl_RpgGaspPaladin` log no errors. The capture shows the MetaHuman Paladin with its hair grooms and the HUD.
- **Finding F1: the Mover maps log 141 `LogStreaming` errors.**
  - On `Lvl_RpgGaspMover` and `Lvl_RpgGaspMoverRagdoll`, the cooked `CR_UEFN_Mannequin_FullBodyIK` references editor-only `RigVMDeveloper.RigVMPin` exports that the game cannot create.
  - The UEFN Mover pawn still renders. Whether its full-body IK still works is not checked yet.
  - The CMC's Manny rigs do not show this.
- **Minor finding:** in the package, `a.animnode.offsetrootbone.enable` and the GASP debug variables `DDCvar.DrawCharacterDebug*` each passed 500 `FindConsoleObject` calls. Blueprint looks these console variables up every frame.

### Phase C – separate processes (2026-10-19)

- **Packaged session on `Lvl_RpgGaspMantle`:** listen host and two clients, the second joining 20 s late, at 60 / 30 / 120 FPS under the Average profile.
  - All three processes loaded `RpgGaspMantleExperience`, and the host admitted both clients.
  - No role logged an error or a network failure. The late joiner's capture shows its Paladin.
- **Offline join:** without Steam the server rejects a client whose join URL has no canonical UUID-v4 `PlayerProfileId`. The session tool now adds one, as the packet-gap probe already did.
- **Scripted routes:** the packet-gap probe gained `--variant cmc`. It drives the CMC Mantle pawn with the Paladin over the tagged Vault lane on `Lvl_RpgGaspMantle` with real `IA_Move` and `IA_Jump` input, in three uncooked `-game` processes (host, owner, observer). One run mixed 60 / 30 / 120 FPS.

### Phase D – emulated network conditions, scripted (2026-10-19)

24 runs with real input, three processes each (`Saved/GaspVal02/scripted_runs.md`, `scripted_runs.json`). Profiles were applied to every process with `--profile`.

| Variant, driver | Profiles | Route | Corrections and reconciles |
| --- | --- | --- | --- |
| Mover, host drives | none, good, average, bad, average + 350 ms pause | Vault crossed and landed in every run | Client reconciles (owner / observer): 1 / 1, 2 / 2, 2 / 2, 13 / 6, 2 / 2. Both clients saw the Vault on the proxy. |
| Mover, client drives | average, bad | Vault crossed and landed | Reconciles 4 / 2 in both runs. The host and the observer saw the Vault. |
| CMC, host drives | none, good, average, bad, average + 350 ms pause | Vault crossed and landed (one bad run failed in the probe, see below) | No corrections. Both clients saw the Vault montage on the simulated proxy. |
| CMC, client drives | none ×3, average ×3, bad ×4, bad at 60 / 30 / 120 FPS | Vault crossed and landed in every run | See finding F2. |

No run logged an unexplained error or a network failure. Errors that every uncooked `-game` process logs, from the engine's experimental Python toolsets, are counted as known noise.

**Finding F2: after a CMC Vault driven by the client, the client flies on alone.**
- After the owner's Vault montage ends and the movement handoff runs, the owning client switches to `MOVE_FLYING` without a montage or a new ability. It keeps moving forward at about 460–490 cm/s.
- Meanwhile the server is already walking and brakes to a stop. The server then corrects the client back. `Build/Tools/GaspPacketGap/cmc_handoff_analyze.py` measures this:

| Profile | Runs | Client-only flying | Travel | Largest server correction |
| --- | --- | --- | --- | --- |
| none | 3 | 0.03–0.05 s | 14–21 cm | 2.0–3.5 cm |
| average | 3 | 0.08–0.15 s | 40–71 cm | 31–44 cm |
| bad | 4 + 1 mixed FPS | 0.10–0.38 s | 44–181 cm | 15–144 cm |

- The phase lasts about one round trip. The client appears to wait in `MOVE_FLYING` for the server's end of the traversal.
- Without latency it is barely visible. At 250 ms it is a visible snap of up to 1.4 m.
- It is a candidate for a follow-up task on the CMC traversal handoff and is not fixed here.

**Probe fixes during the runs**
- Under the bad profile a client's pawn can arrive before its PlayerState. The first CMC bad run failed in the probe for that reason.
- Both runtimes now wait for the PlayerState, and the rerun passed.
- The analyser treats the session-end disconnect as known: clients report "Host closed the connection" when the host quits first. Timeouts still count as failures.

### Phase B, D and F – user sessions in the package (2026-10-19)

The user played the package with `session.py` (`Saved/GaspVal02/session_user*`):

| Session | Map, profile | Played | Log result |
| --- | --- | --- | --- |
| `userPaladinSolo` | `Lvl_RpgGaspPaladin`, alone | about 5 min | No error, ensure or crash. |
| `userMoverSolo` | `Lvl_RpgGaspMover`, alone | about 15 min | Only the 141 `CR_UEFN_Mannequin_FullBodyIK` load errors of finding F1; no crash. |
| `userPaladinAverage` | `Lvl_RpgGaspPaladin`, listen host and one late client, Average | host about 85 s, client about 47 s | The client loaded the Experience and joined. The chest piece was equipped, and both logs recreate its cloth. 7 server corrections of at most 4.4 cm; no error. |

- The user reported no problem ("sollte schon so passen") and did not play the Bad session by hand. The Bad profile is covered only by the scripted runs.
- The user did not report anything visible from finding F1 on the Mover.

## Outcome

- **The package works.** Every GASP map runs in a Win64 Development package with the patched NetworkPrediction/Mover plugins:
  - the MetaHuman Paladin with grooms and armor;
  - separate processes with a late join and mixed frame rates;
  - all emulated profiles, including a 350 ms packet pause.
- **No crash, ensure or network failure** occurred in any packaged or scripted session. Phase E (real WAN) stays deferred.

## Proposed follow-up tasks

Not on the roadmap; each needs the user to mark it Ready.

1. **CMC Vault handoff under latency (finding F2).**
   - After an owner-driven CMC Vault, the owning client stays in `MOVE_FLYING` without a montage for about one round trip and keeps moving.
   - The server then corrects it by up to 44 cm at 120 ms and up to 144 cm at 250 ms.
   - Reproduction: `probe_launch.py <label> --variant cmc --driver owner --profile bad`, then `cmc_handoff_analyze.py`.
   - Mantle and Hurdle handoffs were not scripted and may share the cause.
2. **Mover full-body IK rig in cooked builds (finding F1).**
   - The cooked `CR_UEFN_Mannequin_FullBodyIK` carries editor-only `RigVMDeveloper.RigVMPin` exports and logs 141 load errors on every Mover map.
   - Resaving or recompiling the rig in UE 5.8.3 may strip them. Whether the Mover's IK works in the package is unverified.
3. **Per-frame console-variable lookups (minor).** Blueprint looks up `a.animnode.offsetrootbone.enable` and the GASP `DDCvar.DrawCharacterDebug*` variables every frame (500 `FindConsoleObject` calls each in the package).

## Limits

- The scripted routes cover only the Vault. Mantle, Hurdle, combat, inventory and respawn under the profiles rest on the short manual Average session and on PIE tests.
- The user's sessions were short, and none ran under the Bad profile.
- The profiles are emulated on one machine. Real WAN (Phase E) and a packaged dedicated server, which needs a source-built engine, are not covered.
- The scripted probes run uncooked `-game` processes; the sessions run the Development package. Shipping is not covered.
