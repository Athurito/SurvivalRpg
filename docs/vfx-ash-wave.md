# VFX-06: Ash Wave

> **Superseded in part (2026-10-31):** MST-03b retired Death Wave and its forms ([shared harvesting](mastery-map-shared-harvesting.md)). The Niagara systems, `BP_HarvestBurnArea` and the gallery demo below still exist.

Task report for VFX-06 in the [VFX roadmap](vfx-roadmap.md). Ash Wave is the Death Wave form that turns the felled wood into charcoal ([harvesting-roadmap.md](harvesting-roadmap.md), HARV-09d). This task gives it a look: the trees burn and char as they fall, and a burned area stays behind. Death Wave without the form looks as before.

## Behavior

### The effect

1. **Cast.** A fire whoosh and a crackle start at the area's center as the wave sets off.
2. **Ignition.** When the presentation wave reaches a tree, it flares up with an ignition sound. Flames lick up the lower trunk, embers rise and warm light (High+) hits the trunk.
3. **Fall.** The tree falls burning. Its bark darkens to charcoal within about a second, and glowing tongues run along the trunk like smouldering charcoal. Dark smoke rises off it.
4. **Landing.** Sparks and ash burst along the trunk where it hits the ground. It keeps glowing and smoking while it sinks.
5. **Burned area.** Scorch patches appear across the area as the wave crosses it, sparks drift up and a low haze hangs over it. Each tree's base keeps a scorch mark with glowing embers and thin smoke for about 16 s; the scorch fades out after about 24 s.

### Gameplay hook

`URpgGameplayAbility_Harvest` gains form cues, the presentation counterpart of the yield conversions:
- **`FormSuccessGameplayCues`:** a list of `FRpgHarvestFormGameplayCue` (`RequiredOwnerTag`, `GameplayCue`). The first entry whose tag the owner has replaces `SuccessGameplayCue`. The server resolves it with the other tuned values when execution starts (`FRpgHarvestTunedValues::SuccessGameplayCue`), so a form unlearned mid-swing changes nothing.
- **Cue parameters for area harvests:** the success cue now plays at the area's center (the aim point, or the harvester for an area around it) with the tuned area radius in `RawMagnitude`. Single-target swings still play at the struck point. No ability used the old area location.
- **Prediction:** a commit without a montage or delay, such as a stride's first pulse, runs inside the owning client's predicted activation. `ExecuteHarvestCue` now clears the prediction key (`FScopedPredictionWindow(ASC, FPredictionKey(), false)`), so the harvesting client plays the cue too, as in VFX-03.
- **Content:** `GA_Harvest_DeathWave` maps `Harvest.Form.AshWave` to the new tag `GameplayCue.Harvesting.AshWave`. Its plain `SuccessGameplayCue` stays empty.

### How the trees catch fire

The cue and the falling trees reach each machine on separate paths: the cue by multicast, the trees by the replicated stock and its presentation wave. Either can arrive first. Everything below runs locally and is cosmetic:
- **`GCN_Harvest_AshWave`** (burst notify) spawns a local `BP_HarvestBurnArea` at the area center and calls `Start(Radius, WaveOrigin)` with the cue's radius and the harvester's location. Machines whose camera is farther than `MaxDistance` (80 m) skip it.
- **`BP_HarvestBurnArea`** spawns the ground system, then ignites every falling tree that is already there. It lives for `IgnitionWindowSeconds` (3.5 s), covering the wave (up to 2.5 s) and late replication.
- **`BP_HarvestFallingTree`** looks for burn areas on its first tick, not at BeginPlay, because `BPC_HarvestInstances_DeadPine` sets its mesh after spawning it.
- **`TryIgnite`** ignites a tree whose base lies within `Radius + IgnitionMargin` and that is younger than `MaxTreeAgeSeconds`. It then spawns the smoulder system at the tree's base. A tree ignites once, even under overlapping areas.
- **`Ignite`** on the falling tree:
  - attaches the tree burn system to the mesh with its trunk height and the time left until landing;
  - creates dynamic instances of the tree materials and lerps their `BaseColor Tint` to the charred tint over `CharSeconds`.

### Data-driven setup

All look and sound choices are Blueprint defaults or Niagara user parameters; no code knows a system or sound.

| What | Where |
| --- | --- |
| Which cue a form plays | `GA_Harvest_DeathWave`, `FormSuccessGameplayCues` |
| Default radius, distance cut-off | `GCN_Harvest_AshWave` defaults (category *Ash Wave*) |
| Systems, sounds, charred tint, char time, wave speed, ignition window and margins | `BP_HarvestBurnArea` defaults (category *Burn Area*) |
| Tint parameter name of the tree materials | `BP_HarvestFallingTree`, `CharTintParameter` |
| Particle look | the three Niagara systems |

Sounds on `BP_HarvestBurnArea`:
- `TreeIgniteSounds` (five fire ignitions): each tree picks one at random and plays it at 0.9–1.1 pitch;
- `GroundIgniteSound` (a fire whoosh) and `AreaBurnSound` (a 4.75 s crackle): play at the area's center when the burn starts.

The sounds use the effect sound class `SFX_Effects` and `ATT_FX_World`. That attenuation is full volume within 4 m, fades out over 40 m (natural sound) and low-pass filters with distance.

### Assets

| Asset | Where | What |
| --- | --- | --- |
| `GameplayCue.Harvesting.AshWave` | `Config/DefaultGameplayTags.ini` | Cue tag |
| `GCN_Harvest_AshWave` | `GF_Harvesting_Magic/GameplayCues` | Burst cue notify (above) |
| `BP_HarvestBurnArea` | `GF_Harvesting_Magic/FX` | Local burn area (above) |
| `NS_Harvest_AshWave_TreeBurn` | `GF_Harvesting_Magic/FX` | Attached to a falling tree. See below. |
| `NS_Harvest_AshWave_Smoulder` | `GF_Harvesting_Magic/FX` | At a tree's base: scorch decal, 12 embers, thin smoke for 16 s |
| `NS_Harvest_AshWave_Ground` | `GF_Harvesting_Magic/FX` | At the area center. See below. |
| `BP_HarvestFallingTree` (changed) | `GF_Harvesting_Magic/Harvesting/Instances` | `Ignite` and the bark charring |
| `M_FX_DecalSubUV`, `MF_FX_DecalNormalFade`, `T_FX_ScorchMasks` | core `FX/Materials`, `FX/Textures/Decals` | Niagara Examples scorch decal master (decal renderer ready) and its 2×2 mask atlas |
| `MI_FX_ScorchDecal` | core `FX/Materials/Instances` | Dark scorch, random frame per decal position |
| `T_FX_FireRoil_EOO`, `T_FX_FireRoil_N` | core `FX/Textures/Fire` | Niagara Examples fire flipbook, 8×8, cooked at 1024 px |
| `MI_FX_FireRoil` | core `FX/Materials/Instances` | Fire flipbook on `M_FX_SmokeAndFire`, emissive, blackbody up to 3500 K |
| `MI_FX_FlameWispy` | core `FX/Materials/Instances` | Wispy flipbook as emissive glow up to 4300 K: the smouldering charcoal tongues |
| `SW_Fire_Ignite_01`–`05`, `SW_Fire_Whoosh_01`, `SW_Fire_Burn_01` | core `Audio/Sounds/Fire` | Fire sounds from the sound library, with `SFX_Effects` and `ATT_FX_World` |
| `ATT_FX_World` | core `Audio` | World-space attenuation for effect sounds |
| `BP_VfxAshWaveDemo` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Gallery demo. See below. |

**`NS_Harvest_AshWave_TreeBurn`** is attached to the tree mesh (origin at the trunk base, +Z up the trunk). The frame rotates with the fall; particles live in world space. User parameters: `TrunkHeight` (cm, from the mesh bounds) and `LandingTime` (s).

| Emitter | Sim | What |
| --- | --- | --- |
| `Flames` | CPU | Fire tongues on the lower quarter of the trunk for the first second, 32/s |
| `Glow` | GPU | Glowing charcoal tongues along the lower half of the trunk through fall and sink, 70/s |
| `Smoke` | GPU | Dark wispy smoke off the trunk, 10/s |
| `Embers` | GPU | Embers streaming up, 50/s |
| `LandingSparks` | GPU | 90 sparks along the trunk at `LandingTime` |
| `LandingAsh` | CPU | 8 dust and ash puffs at `LandingTime` |
| `Light` | CPU, local space, High+ | Warm light on the trunk until shortly after landing, radius 500 cm |

**`NS_Harvest_AshWave_Ground`**, user parameters `Radius`, `WaveOrigin` and `WaveSpeed`:
- `Scorch`: six decal patches (300–520 cm) that appear when the wave reaches them;
- `Sparks` (GPU): embers drifting up for 6 s;
- `Haze`: seven low smoke sprites;
- `Light` (High+): a short warm flash over the area, radius 1.5 × `Radius`.

**`BP_VfxAshWaveDemo`** stands at (−800, −1100) in `Lvl_VfxGallery`, labelled "Ash Wave (VFX-06)".
- A mannequin faces six standing dead pines 5.6–11 m ahead.
- Every 16 s (first after 2 s) the demo fells the trees in wave order at 15 m/s, spawning the real falling tree actor with the real burn area.
- It shows the standing trees again after 12 s.

## Performance

- **Particles per burning tree:** at most about 25 CPU particles:
  - flames, only during the first second (32/s × about 0.5 s);
  - 8 ash puffs at the landing;
  - the light.

  The GPU emitters hold about 35 glow, 30 smoke and 80 embers alive, plus 90 landing sparks. Smoulder: 21 CPU particles per tree. Ground: 14 CPU plus about 100 GPU sparks.

  These are peak values from spawn rates and lifetimes. `fx.DumpParticleData` dumps parallel emitters interleaved, so it confirmed the emitters but gave no clean per-instance counts.
- **Six trees (Death Wave's cap):** about 300 CPU particles in all, each system below 100.
- **GPU (`ProfileGPU`):** in Simulate on the gallery at Epic quality (1469×783), close view, six trees burning, compared with the same view before the cast:

  | Pass | Before | Peak |
  | --- | --- | --- |
  | Translucency | 0.048 ms | 0.220 ms |
  | Lights | 0.191 ms | 0.216 ms |
  | Deferred decals | 0.016 ms | 0.050 ms |
  | Frame | 6.7 ms | 7.4 ms |

  Lights at the peak: 6 (`NumLights 6`).
- **Quality tiers:** all three systems use `FXT_Gameplay_Burst`.
  - Low halves spawns and turns off every light; a Low capture still reads as burning, charred trunks.
  - Burst allows 8 instances per system at High and 4 at Low. On Low, at most four trees of a cast burn; the others fall unburned but still char.
- **Visibility (2026-10-20):** the tree burn, smoulder and ground systems override only the Effect Type's visibility settings: no culling when not rendered, no frustum culling and no frustum pre-culling. Distance and instance limits still come from `FXT_Gameplay_Burst`.
  - `FXT_Gameplay_Burst` culls a running burst the moment it leaves the view, with no resume. A player who looked away for a moment erased the burning trees and the scorch for good; another player who looked away at the cast never saw them.
- **Refused spawns (2026-10-20):** a spawn that the Effect Type refuses returns None.
  - `BP_HarvestFallingTree.Ignite` and `BP_HarvestBurnArea.Start` set user parameters only on a valid system (`IsValid`). Charring, sounds and the ignition of the trees run either way.
  - Before, every refused spawn logged "Accessed None" errors, about 70 in one minute of play in the gallery.
- **Bounds:** fixed. Tree burn ±15 m around the trunk (−7 m to +29 m along it), ground ±14 m, smoulder ±6 m.
- **Pooling:** the burn area and the tree spawn every system with AutoRelease pooling. A tree's system can outlive the falling tree actor by a few seconds; its world-space smoke then finishes where it is.

## Validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed (0 errors, 0 warnings).
- **New native test `SurvivalRpg.Harvesting.Feedback.FormSelectsSuccessCueAtAreaCenter`:** passed. An area harvest plays the plain success cue at the aim point with the tuned 450 cm radius. Once the owner has the form tag, the form's cue replaces it.
- **`SurvivalRpg.Harvesting` (59 tests, `--null-rhi`):** passed.
- **New network test `SurvivalRpg.Network.HarvestCuePIE`:** passed. It runs a dedicated server and two clients:
  1. Client 0 predicts an area harvest that commits inside its predicted activation.
  2. Both clients, the predicting one included, play the harvest cue once at the harvester.
  3. The dedicated server plays none.

  It uses the configured `GameplayCue.Harvesting.AshWave` tag, so the clients also ran the real cue notify: the log shows `NS_Harvest_AshWave_Ground` initialised in the client worlds.
- **Regression proof:** with the prediction-window line commented out, the same test failed (timeout waiting for every client to play the cue). With the line restored it passed again.
- **Combined run:** `ue.py test SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Harvesting --null-rhi` crashed after all harvesting tests passed. The PIE world-leak check found an old test world held through `HarvestRenewableAreaTest` (`RpgHarvestAutomationInstancesActor` → `HarvestProfile`). That fixture was not touched here. Each suite passes on its own, and network PIE tests need rendering anyway.
- **Look:** checked in Simulate in the gallery with `slomo 0.25` and fixed cameras, at Epic and Low. Shown were:
  - the wave of ignitions;
  - charring bark;
  - charcoal glow through the fall;
  - landing sparks and ash;
  - scorch decals with embers and haze after the trees sank.
- **Asset validation:** the 17 new or changed assets are valid with 0 warnings: the systems, Blueprints, cue, ability, materials, textures and the gallery map. After the sound and colour pass, the 15 assets it touched validated again with 0 warnings, the seven sounds and the attenuation included.
- **Cook:** `python Build/Tools/Unreal/ue.py cook /GF_Dev_Sandbox/Maps/Lvl_VfxGallery /GF_Harvesting_Magic/Harvesting/Tests/Lvl_HarvestPickaxe` passed with 0 errors. The new assets were cooked. Its 3 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick. After the sound pass, the gallery cook passed again (0 errors, the same 3 warnings) and cooked the sounds.

## Authoring notes

- **Emission in `M_FX_SmokeAndFire`:** only with the static switch *Use Emissive Color* on. `MI_FX_SmokePuff` and `MI_FX_SmokeWispy` have it off, so their dynamic-parameter emissive gain does nothing. The VFX-03 soul flame therefore glowed only through its light. The fire and flame instances turn it on.
- **Blackbody brightness:** the BlackBody node scales brightness with (T/2500 K)⁴. A temperature range that is too low leaves fire black; one that is too high turns it white. A temperature exponent below 1 (dynamic parameter Z, 0.65 here) lifts the mid tones, so flame edges turn orange instead of red.
- **Flame colour:** fire sprites need a near-black particle colour. The material uses it as lit base colour, and white turns the flames into sunlit puffs.
- **Graph DSL:**
  - Positional arguments bind to `self` first; use keywords such as `:NewRotation`, `:InLifespan` or `:bEnabled`.
  - Literals on by-ref wildcard pins (`SetArrayElem`) are dropped; feed an expression such as `(not false)`.
  - Rewriting a function graph keeps old nodes that still hang on the entry's data pins. Delete every node except entry and result first.
  - In a UObject Blueprint (cue notify), `SpawnActorFromClass` and `GetPlayerCameraManager` need `:WorldContextObject`.
- **Native tags in the editor module:** tags defined there ensure ("module type is Editor"). Editor-module tests use configured tags.
- **Texture export:** `TextureExporterTGA` on these flipbooks asserts (`SupportsTexture`) and crashed the editor.

## Playtest

1. **Gallery:** open `/GF_Dev_Sandbox/Maps/Lvl_VfxGallery`, press Play and walk to "Ash Wave (VFX-06)", left of the start. Every 16 s a fire whoosh and crackle start, six dead pines catch fire one after another with an ignition sound each, fall burning and char, and the ground keeps scorch marks with embers and smoke.
2. **Real cast, two players:**
   1. Set Play to 2 players, *Play As Listen Server*, and open `Lvl_HarvestPickaxe`.
   2. As the client, pick up the axe, reach Logging 3 and learn Death Wave and Ash Wave in the axe tree.
   3. Hold Q on the dead pine stand east of the ore veins and release.
   4. Expect in both windows: the trees in the ring ignite in wave order, fall burning, char and leave a burned area, and the client receives charcoal.
   5. Repeat as the host.
3. **Without the form:** Death Wave with Wide Wave instead of Ash Wave fells the trees without fire, as before.

## Limits

- **No scripted cast with the real axe:** in a listen-server PIE session the pickup did not react to simulated interact keys, so the full axe → skill tree → Q cast was not played in the editor. Covered instead:
  - the native test (form cue, area center and radius);
  - the network test (cue on the predicting client and peers, real cue notify on clients);
  - the gallery demo (same burn area and falling tree actors).

  The two-player cast is the playtest above.
- **Late cues:** a tree that started falling more than 0.75 s before the cue reached a machine stays unburned there. That only happens with more than 0.75 s between the two paths.
- **Bark charring:** it needs the tree materials' `BaseColor Tint` parameter (the Megaplant dead pines have it). Trees with other materials burn but do not char.
- **Look:** judged from editor captures in the daylight gallery; fire reads stronger at night. After review the fire was made warmer (3500 K and 4300 K, temperature exponent 0.65, yellower sparks and lights); the glow's edges are still slightly red.
- **Sounds:** chosen by name and length from the library and not judged by ear in the editor.
- **Low quality:** at most four trees per cast burn.
