# VFX-03: Soul harvest

Task report for VFX-03 in the [VFX roadmap](vfx-roadmap.md). When a kill awards XP, souls rise from the killed enemy and fly into the killer. The server triggers the effect from the XP award through a GameplayCue, so every player sees it.

## Behavior

### The effect

1. **Release.** A few wisps of grey smoke leave the body.
2. **Rise.** Two to six souls lift off the corpse. Each soul is a small flickering flame of fire and smoke around a hot core.
3. **Flight.** The souls arc up and to the side, then curve into the killer's chest. They shed a smoke trail that glows briefly where it leaves the flame, then drifts and fades. They follow the killer while it moves.
4. **Arrival.** Each soul flares on the body as it enters and throws off a few sparks. A short warm light (High+) lights the killer and the ground.

The whole effect lasts about 2 to 3 s; the flight itself takes 0.8 to 1.8 s, depending on distance.

### Gameplay hook

`URpgGameFeatureAction_AwardKillXP` gains `XPAwardGameplayCueTag`.
- **When:** after a kill awards XP, the server executes that cue on the killer's ability system.
- **Parameters:** `Location` is the victim's location, `RawMagnitude` the awarded XP.
- **Prediction:** kills usually resolve inside the killer's predicted attack. A cue sent with that prediction key counts as already predicted on the killer's client, which then skips it. The action clears the key with `FScopedPredictionWindow(ASC, FPredictionKey(), false)`, so the killer's own client plays it too.
- **Replication:** the cue is a multicast from the server on the player state's ability system. Every client that has the killer's character plays it there. Dedicated servers play no cues. The effect is cosmetic, so late joiners do not need it.
- **Configuration:** `GF_Progression` sets the tag to `GameplayCue.Progression.SoulHarvest` and now registers its `/GameplayCues` folder with `RpgGameFeatureAction_AddGameplayCuePath`. An empty tag disables the cue.

### Assets

| Asset | Where | What |
| --- | --- | --- |
| `GameplayCue.Progression.SoulHarvest` | `Config/DefaultGameplayTags.ini` | Cue tag |
| `GCN_Progression_SoulHarvest` | `GF_Progression/GameplayCues` | Burst cue notify. See below. |
| `NS_Progression_SoulHarvest` | `GF_Progression/FX` | The effect. See below. |
| `BP_VfxSoulHarvestDemo` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Gallery demo. See below. |

**`GCN_Progression_SoulHarvest`** is a `GameplayCueNotify_Burst` Blueprint. Its `OnBurst`:
- runs only for a character target, and only when the victim is within `MaxDistance` (2500 cm);
- spawns the system attached to the character's mesh with pooling AutoRelease;
- sets `SourcePosition` from the cue location and `SoulCount` from the XP: `clamp(round(XP / XPPerSoul), MinSouls, MaxSouls)`, by default 5 XP per soul, 2 to 6 souls.

The burst effects list is empty; a sound can be added there later.

**`NS_Progression_SoulHarvest`** is attached to the killer's mesh (feet origin, +Y forward). User parameters:
- `SourcePosition` (position): where the souls rise from;
- `SoulCount`: 1 to 6;
- `TargetOffset`: where the souls enter, in mesh space (default 10 cm forward, 125 cm up).

Each soul flies a quadratic Bezier curve from the corpse to the chest, evaluated every frame against the current chest position. Every soul's random values come from a hash of its index and the source position, so the emitters agree on each path without events or particle readers. All emitters are CPU, in world space unless noted:

| Emitter | Per soul | What |
| --- | --- | --- |
| `Release` | 5 in total | Lit wispy smoke leaving the body |
| `Wisps` | 1 | Hot core: 2.5–3.5 cm additive glow, flickering, velocity-stretched |
| `Flame` | 1 | Flame of smoke and fire (smoke-and-fire master, emissive gain 14 ± 4); flares to 32 and grows on arrival, then fades in 0.35 s |
| `Trail` | 14 | Smoke that rides the path invisibly, then is released at its slot, glows briefly and drifts |
| `Embers` | 4 | Ride to the chest, then burst off the body and fall |
| `Light` | 1 in total, local space, High+ | Warm light 35 cm in front of the chest while the souls arrive, radius 200 cm |

**`BP_VfxSoulHarvestDemo`** stands right of the templates in `Lvl_VfxGallery`, labelled "Soul harvest (VFX-03)".
- A Manny "killer" sways ±160 cm sideways, so the souls must follow a moving target.
- A second Manny lies 4.6 m away as the victim.
- The demo spawns the system 2 s after start and then every 3 s (`CycleSeconds`, `SoulCount` are editable). `GF_Dev_Sandbox` now depends on `GF_Progression`.

## Performance

- **Particles:** 20 CPU particles per soul (1 core, 1 flame, 14 trail, 4 embers), plus 5 release puffs and 1 light. Four souls give 86 particles for about 2 s; the cap of six souls gives 126. That is above the roadmap's "about 100" CPU guideline only at the cap and only briefly.
- **Quality tiers:** the system uses `FXT_Gameplay_Burst`, whose spawn scaling halves every emitter on Low. Counted with `fx.DumpParticleData` for four souls:

  | Quality | Cores | Flames | Trail | Embers | Release | Light |
  | --- | --- | --- | --- | --- | --- | --- |
  | Epic (`sg.EffectsQuality 3`) | 4 | 4 | 56 | 16 | 5 | 1 |
  | Low (`sg.EffectsQuality 0`) | 2 | 2 | 27 | 8 | 2 | 0 |

  Every soul emitter scales the same way, so the shared soul indices stay consistent: Low shows half the souls, each still with its flame, embers and most of its trail. The light is High+ only.
- **GPU (`ProfileGPU`):** measured in Simulate in the gallery at Epic quality (internal 1469×783), from the side of the flight path. `RenderTranslucency` was 0.058 ms without souls and 0.061 ms with four souls in flight; the frame was about 5 ms either way. The 0.27 ms distortion pass in both frames comes from the gallery's heat-haze template; the soul harvest has no distortion.
- **Bounds:** the bounds are fixed at ±25 m around the killer (−5 m to +9 m in height), because the souls travel up to the cue's `MaxDistance` of 25 m. The cue does not play for farther kills.
- **Pooling:** the cue spawns with AutoRelease pooling. The system is non-looping and completes when the last particle dies.

## Validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed (0 errors, 0 warnings).
- **New network test `SurvivalRpg.Network.KillXPCuePIE`:** passed (1 of 1). Its setup:
  - a dedicated server and two clients with the real RPG player state;
  - a copy of the configured `GF_Progression` kill XP action, unless the feature is already active.

  Client 0 predicts a test ability, whose server activation broadcasts a kill message for a victim with the production reward component. The test checks:
  - only the killer earns XP;
  - the dedicated server plays no cue;
  - both clients, the predicting killer included, route `GameplayCue.Progression.SoulHarvest` for the killer, with the victim's location and the awarded XP.

  Its 30 warnings are the `LogNetPackageMap` "NOT Supported" lines of the untitled PIE map, which the existing network tests also log.
- **Regression proof:** with the `FScopedPredictionWindow` line commented out, the same test failed: it timed out waiting for the killer's client to play the cue. With the line restored it passed again.
- **Existing combat test `SurvivalRpg.Network.CombatRemoteMeleePIE`:** the first run completed all steps but then crashed in the engine during PIE teardown (`IsInGameThread()` in `FSceneViewport::Destroy` on the render thread, no project frame). That is the same engine crash as in GASP-09 and the walk-gait task. The rerun alone passed.
- **Cue in PIE:** `GameplayCueFunctionLibrary.ExecuteGameplayCueOnActor` on the gallery's GASP character found the notify through `GF_Progression`'s cue path and attached the system to `CharacterMesh0`.
- **Look:** checked in Simulate in the gallery with `slomo` and fixed camera angles:
  - souls rise from the victim's torso, arc and enter the killer's chest while it moves;
  - trails of smoke glow and fade;
  - on arrival, sparks and the warm light hit the body and the floor.

  `fx.DumpParticleData` confirmed the paths: the souls start at the source and reach the moving chest at `SoulT` = 1.
- **Asset validation:** the 5 changed or new assets are valid with 0 warnings: the system, the cue notify, `GF_Progression`, the demo and the gallery map. A dependency scan shows the system, cue and feature data reference only core FX, `/GF_Progression`, `/Engine`, `/Niagara` and `/Script`. The cross-plugin reference check passes now that `GF_Dev_Sandbox` depends on `GF_Progression`.
- **Cook:** `python Build/Tools/Unreal/ue.py cook /GF_Dev_Sandbox/Maps/Lvl_VfxGallery` passed with 0 errors. It cooked the system, the cue notify and the demo. Its 3 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick.

## Authoring notes

- **Path expressions:** the path, flicker and fades are one-line HLSL expressions in SetParameters modules. Entries in one SetParameters module read the values from before that module, so dependent values need separate modules.
- **VectorVM `frac`:** `frac` keeps the sign of its argument on the CPU VM. The hashes use `frac(abs(...))`; without it, delays and flight times went negative.
- **Release in world space:** the burst template emitter is local space. The release smoke is moved to the source in world space, so its emitter is switched to world space.
- **Inspecting a moving effect:** `fx.DumpParticleData` also dumps the Niagara editor's preview, which runs with the default user parameters. Close the asset editor before dumping.

## Playtest

1. **Gallery:**
   1. Open `/GF_Dev_Sandbox/Maps/Lvl_VfxGallery` and press Play.
   2. Walk forward and look right to "Soul harvest (VFX-03)".
   3. Expect: every 3 s, smoke leaves the lying mannequin and four souls rise from it, arc and fly into the chest of the swaying one; their smoke trails drift away; on arrival sparks fly off and a warm light flickers on the body and the floor.
2. **Real kill with two players:**
   1. Set Play to 2 players, *Play As Listen Server*.
   2. Use a map whose Experience activates `GF_Progression` (for example `Lvl_RpgBaseline`) and place an enemy with an XP reward, such as `BP_BaseEnemy`, near the player start.
   3. Kill it with a weapon as the server player, then again as the client.
   4. Expect in both windows: souls rise from the enemy and fly into the killer, and only the killer's XP increases. A 10 XP enemy gives two souls.

## Limits

- **No in-editor melee kill:** the automated test drives the real kill XP action with a kill message, and PIE checked the cue on a real character. A full sword kill was not played in the editor: a `BP_BaseEnemy` placed for it did not appear in PIE on the World Partition test map. The playtest covers it.
- **Look not yet judged by you:** the look is judged from editor captures. The flame layer uses the smoke-puff flipbook's emission channel, so the flare on arrival reads more as glowing smoke than as fire; the light carries most of the "glow on the body".
- **Chest offset:** `TargetOffset` assumes the UE5 mannequin proportions in mesh space. Characters with other proportions or mesh rotations need their own offset.
- **Bounds:** the fixed ±25 m bounds are large, so the system culls less tightly than the small template effects.
- **Cap:** at six souls the effect briefly exceeds about 100 CPU particles.
