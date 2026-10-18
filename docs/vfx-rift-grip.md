# VFX-07: Rift Grip

Task report for VFX-07 in the [VFX roadmap](vfx-roadmap.md). Rift Grip is the pickaxe's awakened power: hold Q to aim, release to rip three sections from up to three ore veins ([harvesting-roadmap.md](harvesting-roadmap.md), HARV-04). Until now it had no effect of its own. With this task a short lightning strike hits every harvested vein, which bursts into rock debris, dust and sparks with a thunder crack and rattling stone.

## Behavior

### The effect

For each vein the cast harvests:
1. **Strike.** A bolt comes down from the sky onto the top of the vein. The leader draws the channel top-down in about 35 ms with a few forks. It flickers through two return strokes and is gone after a quarter second. A blue-white light flashes over the surroundings (High+).
2. **Burst.** The vein bursts at the strike point:
   - hot fragments fly off as sparks;
   - rock chunks and grit are thrown out and bounce on the ground around it;
   - a pale dust cloud rolls out;
   - glowing spots stay on the struck rock for one to two seconds, with a short orange glow and light.
3. **Mark.** A scorch mark darkens the vein's top and the ground around it and fades after about 18 s.
4. **Sound.** A thunder crack, a rock smash and a debris rattle play at the strike. Nearby players feel a short camera shake (full within 4 m, gone at 25 m).

The vein itself still shrinks or vanishes through its replicated stock as before; the burst covers that moment.

### Gameplay hook

`URpgGameplayAbility_Harvest` gets a per-resource cue next to the success cue:
- **`TargetGameplayCue`:** the server executes it once for every resource the commit harvests directly.
  - `Location` is the top center of the resource's bounds, `Normal` points up, and `RawMagnitude` carries the bounds' horizontal radius in cm.
  - The bounds are taken from the presentation *before* the commit: the struck instance for instanced resources (PCG veins), the struck primitive for actor resources. A depleting commit hides an instance at once, and its bounds would otherwise be empty.
  - At most 8 per commit, nearest to the area's center first. Strides execute it on every pulse. Swarms and resources taken along by a chain execute none.
  - It plays at the commit; the presentation wave does not delay it. Rift Grip has no wave.
  - It goes through `ExecuteHarvestCue` like the success cue, so the predicting client plays it too (VFX-06).
- **Content:** `GA_Harvest_RiftGrip` sets `TargetGameplayCue` to the new tag `GameplayCue.Harvesting.RiftGripStrike`. Its `SuccessGameplayCue` stays empty.
- **Network budget:** every cue is one unreliable multicast on the harvester's ability system. The engine default `net.MaxRPCPerNetUpdate` is 2 per actor and net update; Lyra raises it to 10. This project still had the default, so a commit with two target cues dropped its success cue on every client. `Config/DefaultEngine.ini` now sets Lyra's 10. The cap of 8 target cues leaves room for the success cue.

### The cue notify

`GCN_Harvest_RiftGripStrike` (burst notify) runs locally on every client:
1. Machines whose camera is farther than `MaxDistance` (90 m) skip the strike.
2. A line trace from just above the cue location finds the vein's actual surface, so the bolt lands on the rock instead of the top of its bounding box.
3. A trace next to the vein, on the camera's side, finds the ground. Debris bounces there and the dust starts there.
4. A trace upward finds a ceiling or overhang. In a cave the bolt then starts under the ceiling instead of passing through rock. Under open sky it is 85–100 % of `MaxBoltHeight` (35 m).
5. It spawns `NS_Harvest_RiftGrip_Strike` at the strike point with AutoRelease pooling. It sets `BoltHeight`, `Radius` and `GroundDepth`.
6. It plays one random sound from each of its three lists at 0.92–1.08 pitch.
7. The notify's own burst effects play `CS_FX_RockBurst` as a world camera shake.

### Data-driven setup

No code knows a system, sound or shake.

| What | Where |
| --- | --- |
| Which cue a resource gets | `GA_Harvest_RiftGrip`, `TargetGameplayCue` |
| System, sounds, bolt heights, distance cut-off, default radius, debris volume | `GCN_Harvest_RiftGripStrike` defaults (category *Rift Grip Strike*) |
| Camera shake, its radii and scale | `GCN_Harvest_RiftGripStrike`, *Burst Effects* → *Burst Camera Shake* |
| Particle look | `NS_Harvest_RiftGrip_Strike` |

Sounds:
- `StrikeSounds`: three thunder cracks;
- `BurstSounds`: four rock smashes;
- `DebrisSounds`: two debris rattles, played at `DebrisVolume` 0.6.

They use `SFX_Effects` and `ATT_FX_World` like the Ash Wave sounds.

### Assets

| Asset | Where | What |
| --- | --- | --- |
| `GameplayCue.Harvesting.RiftGripStrike` | `Config/DefaultGameplayTags.ini` | Cue tag |
| `GCN_Harvest_RiftGripStrike` | `GF_Harvesting_Magic/GameplayCues` | Burst cue notify (above) |
| `NS_Harvest_RiftGrip_Strike` | `GF_Harvesting_Magic/FX` | The strike. See below. |
| `GA_Harvest_RiftGrip` (changed) | `GF_Harvesting_Magic/GAS/Abilities` | `TargetGameplayCue` |
| `SM_FX_RockChunk`, `MI_FX_RockChunk`, `M_FX_DebrisMesh`, `T_FX_RockChunk_D`/`_DpR`/`_N` | core `FX/Meshes`, `FX/Materials`, `FX/Textures/Debris` | Niagara Examples debris rock: 150 triangles, 4 LODs, no Nanite, 512 px textures; tinted per particle |
| `M_FX_Pebbles`, `MI_FX_Pebbles`, `MF_FX_MotionStretchSpark` | core `FX/Materials` | Niagara Examples grit: masked lit sprites, motion-stretched, coloured per particle |
| `CS_FX_RockBurst` | core `FX/CameraShakes` | Niagara Examples camera shake: Perlin, 0.25 s |
| `M_FX_Additive` (changed) | core `FX/Materials` | Now also used with Niagara ribbons |
| `SW_Lightning_Strike_01`–`03` | core `Audio/Sounds/Lightning` | Thunder cracks from the sound library |
| `SW_Rock_Smash_01`–`04`, `SW_Rock_Debris_01`–`02` | core `Audio/Sounds/Rock` | Rock smashes and debris rattles from the sound library |
| `BP_VfxRiftGripDemo` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Gallery demo. See below. |

**`NS_Harvest_RiftGrip_Strike`** is spawned at the strike point. User parameters:
- `BoltHeight`: cm above the strike point;
- `Radius`: the vein's horizontal radius in cm; it scales debris, dust, glow and scorch;
- `GroundDepth`: cm from the strike point down to the ground.

| Emitter | Sim | Per strike | What |
| --- | --- | --- | --- |
| `Bolt` | GPU ribbon | 114 points | White core, 4–10 cm wide |
| `BoltGlow` | GPU ribbon | 114 points | Violet halo, 40–80 cm wide, same channel |
| `Flash` | CPU sprites | 2 | Blue-white flash at the strike, then an orange heat glow on the rock |
| `Light` | CPU, High+ | 2 | Blue-white flash light, radius 22 m, flickering with the strokes; short orange glow, radius about 4 m |
| `Sparks` | GPU | 70 | 56 hot fragments flying off, 14 glowing spots on the struck rock |
| `Debris` | CPU mesh | 12 | Rock chunks 20–60 cm, spinning, bouncing on the ground plane |
| `Grit` | GPU | 70 | Stone chips 2–8 cm, bouncing |
| `Dust` | GPU | 9 | Pale dust puffs rolling out over 2.4–3.8 s |
| `Scorch` | CPU decal | 1 | Burn mark over the vein's top and the ground around it, 18 s |

How the bolt is built:
- **Channel:** 72 points from the sky down to the strike point, denser towards the ground. The offset of the sky end is hashed from the owner position, so every strike differs.
- **Zigzag:** two scales, both scaled to the local segment length, so the channel is jagged up close and from afar. It fades over the last 4 m, so the channel meets the rock in a straight last leap.
- **Branches:** three branches fork between 25 % and 70 % of the way down and point down and outward.
- **One ribbon:** the ribbon renderer's default `RibbonID` binding expects a `NiagaraID`, and an int attribute is ignored. Each branch is therefore an excursion of the one ribbon, out from its fork to the tip and back along the same points. Branches get half the gain, because they are drawn twice.
- **Core and halo:** two emitters with the same hashes build the same channel. One draws the white core and the other a wide violet halo.

**`BP_VfxRiftGripDemo`** stands at (200, 2200) in `Lvl_VfxGallery`, labelled "Rift Grip (VFX-07)".
- A mannequin faces three real ore vein meshes 6–7 m ahead.
- Every 8 s (first after 2 s) it executes the real cue on each vein through `ExecuteGameplayCueOnActor`.
- It then shrinks the veins like taken sections, and lets them grow back after three casts.

## Performance

- **Particles per strike:** 17 on the CPU (2 flash, 2 light, 12 chunks, 1 decal) and 377 on the GPU. Of the GPU particles, the 228 bolt points live only 0.26 s.
  - A full cast of three veins holds 51 CPU particles; with Wide Rift's five veins, 85.
- **GPU (`ProfileGPU`):** in Simulate on the gallery at Epic quality (internal 1469×783), close view, three strikes, against the same view before the cast:

  | Pass | Before | Strike frame | Dust peak (0.7 s) |
  | --- | --- | --- | --- |
  | Niagara GPU ribbons | 0 | 0.273 ms | 0 |
  | Lights | 0.178 ms | 0.221 ms | 0.181 ms |
  | Deferred decals | 0.025 ms | 0.038 ms | 0.031 ms |
  | Translucency | 0.43 ms | 0.39 ms | 0.41 ms |

  - The ribbon cost lasts only the bolts' 0.26 s.
  - About 0.32 ms of the translucency pass is clearing its render target in every frame. The "before" frame still held dust from the demo's previous cast and the burning Ash Wave demo.
  - Whole frame times are not comparable here, because the capture that forces the render adds its own view.
- **Quality tiers:** the system uses `FXT_Gameplay_Burst`.
  - Low halves the spawn counts of sparks, chunks, grit and dust and turns off the lights.
  - `Bolt`, `BoltGlow`, `Flash` and `Scorch` override the spawn scaling on every level. A halved ribbon would end in mid-air, the flash pair would lose its heat glow, and the single decal could vanish. A Low capture shows complete bolts.
  - Burst allows 8 instances per system at High and 4 at Low. On Low a cast strikes at most four veins.
- **Bounds:** fixed, ±15 m around the strike point, −5 m to +39 m vertically.
- **Network:** one unreliable multicast per harvested vein, three for a normal cast, five with Wide Rift.

## Validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed (0 errors, 0 warnings).
- **New native test `SurvivalRpg.Harvesting.Feedback.TargetCuePlaysOnEveryHarvestedResource`:** passed.
  - An area harvest depletes every cube it covers.
  - Each harvested cube plays exactly one target cue, in selection order, on top of the cube with its 50 cm radius.
- **Regression proof:** with the bounds taken after the commit instead of before, the test failed: every cue carried radius 0, because depletion hides the instance. It passed again with the code restored.
- **`SurvivalRpg.Harvesting` (60 tests, `--null-rhi`):** passed.
- **Network test `SurvivalRpg.Network.HarvestCuePIE` (dedicated server, two clients):** both methods passed.
  - New: `TargetCueReachesEveryMachineOnEachHarvestedResource`.
    1. Every machine loads the same instanced field of two cubes, like a PCG partition actor.
    2. The server moves the harvester between them, and client 0 predicts an area harvest.
    3. Both clients, the predicting one included, play one target cue per cube on top of it with its radius, plus the success cue of the same commit.
    4. The dedicated server plays none, and both cubes lost stock.
  - The test uses the real tag, so the client worlds also ran the real notify: the log shows `NS_Harvest_RiftGrip_Strike` initialised four times, two cubes on each of two clients.
- **Regression proof for the network budget:** without `net.MaxRPCPerNetUpdate=10`, the new network test timed out: every client got the two target cues but not the success cue. GAS logged *"no more RPCs are allowed this net update. Max:2"*. With the setting, the log has no such warning.
- **Look:** checked in Simulate with `slomo 0.05` in `Lvl_HarvestPickaxe` on its real actor veins and in the gallery demo. Checked at Epic and at Low (`sg.EffectsQuality 0`).
- **Asset validation:** the 25 new or changed assets are valid with 0 warnings, the gallery map included. No dependency points into the asset library projects.
- **Cook:** `python Build/Tools/Unreal/ue.py cook /GF_Dev_Sandbox/Maps/Lvl_VfxGallery /GF_Harvesting_Magic/Harvesting/Tests/Lvl_HarvestPickaxe` passed with 0 errors and cooked the system, the cue, the demo, the sounds and the core assets.
  - Three of its 5 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick.
  - Two were a timeout of the local Zen server in this run.

## Authoring notes

- **Ribbon IDs:** the ribbon renderer binds `Particles.RibbonID` as a `NiagaraID` by default. An int32 attribute of that name is silently ignored, and every point joins one ribbon. `bLinkOrderUseUniqueID` is not editable; write `Particles.RibbonLinkOrder` instead.
- **Spawn scaling and ribbons:** effect type spawn scaling also halves ribbon points. Emitters that need exact counts take `ScalabilityOverrides` with `bOverrideSpawnCountScale` and `SpawnCountScale` 1 on all quality levels.
- **`SetCameraTransform`** takes `location`. With `translation` the camera silently goes to the origin.
- **`ProfileGPU` with an unfocused editor:**
  - The level viewport does not render, so the profiled frame holds only Slate. Trigger `CaptureViewport` right after `ProfileGPU`, so the profiled frame renders the scene.
  - The first `ProfileGPU` opens the GPU visualizer window; `r.ProfileGPU.ShowUI 0` prevents that.
- **GAS cue budget:** see the network budget above. Any ability that executes several cues in one frame needs `net.MaxRPCPerNetUpdate` above the engine default.
- **Graph DSL:** `(* vector (* float float))` creates a vector×vector node and converts the float. That is harmless for a uniform scale.

## Playtest

1. **Gallery:** open `/GF_Dev_Sandbox/Maps/Lvl_VfxGallery`, press Play and walk ahead and to the right of the start to "Rift Grip (VFX-07)". Every 8 s lightning strikes the three veins with a thunder crack, they burst into dust, sparks and rock chunks, and they shrink. After three strikes they grow back.
2. **Real cast, two players:**
   1. Set Play to 2 players, *Play As Listen Server*, and open `Lvl_HarvestPickaxe`.
   2. As the client, pick up the pickaxe, reach Mining 2 and learn Rift Grip in the pickaxe tree.
   3. Hold Q on the three iron veins and release.
   4. Expect in both windows: a bolt on each vein, debris and dust, a scorch mark. The veins lose three sections each, and the client receives the ore. Near the veins the camera shakes briefly.
   5. Repeat as the host, and once on the PCG vein field north of the actor veins.
3. **Cave or overhang:** a vein under a roof gets its bolt from the roof, not through it.

## Limits

- **No scripted cast with the real pickaxe:** as in VFX-06, scripted PIE input did not drive the pickup, so the full pickaxe → skill tree → Q cast was not played in the editor. Covered instead:
  - the native test (one cue per resource, location and radius);
  - the network test (dedicated server, two clients, real notify);
  - the gallery demo on real vein meshes.

  The two-player cast is the playtest above.
- **Sounds:** chosen by name and length and not judged by ear. The three cracks come from a magic spell pack's lightning set (stereo), not from recorded thunder.
- **Ordering:** the cue and the vein's stock reach a client separately. The vein can shrink a frame before or after the bolt.
- **Ground:** debris and grit bounce on a flat plane at the height found next to the vein. They do not follow slopes or other rocks.
- **Ceilings:** the upward trace uses the visibility channel, so trees or other blocking objects above a vein shorten the bolt too.
- **Low quality:** at most four veins per cast get the strike (burst instance cap). The others lose their sections without an effect.
- **Look:** judged from editor captures in the daylight test maps; lightning reads stronger at dusk and at night.
