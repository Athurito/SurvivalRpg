# VFX-08: Fire tornado

Task report for VFX-08 in the [VFX roadmap](vfx-roadmap.md). This task builds the presentation of a fire tornado: a column of fire that forms with a burst, wanders over the ground and pulls in dust, debris and embers. It leaves a burned track and collapses at the end. The tornado comes with sound and a ground rumble.

The gallery spawns it as a prototype on a replicated stand-in actor. There are no gameplay changes. The spell, its damage and its moving damage area come later with combat magic and a spell area foundation ([combat-roadmap.md](combat-roadmap.md), "Later").

## Behavior

### The effect

1. **Formation.** Fire bursts out at the base and a ring of dust is thrown over the ground. The column rises from the ground in 0.9 s. A fire whoosh, a wind gust and a ground rumble play.
2. **The funnel.**
   - **Fire column:** about 9 m tall. It is narrow at the ground, 90 cm across, and opens to about 5.5 m at the top.
   - **Fire:** fills the lower 60 %. Its flame tongues spiral up in three twisted bands around a rolling body of fire and a white-hot base.
   - **Smoke:** above the fire, dark smoke spirals off the top. It is left behind as the tornado moves, and it rises to about 20 m.
   - **Motion:** the funnel leans back against its motion and snakes slowly.
   - **Heat haze:** shimmers along the lower funnel (High+).
   - **Light:** radius 13 m, lights the ground around it (Medium+).
3. **What it pulls in.**
   - Dust is sucked in along the ground from 2.6–5.2 m away and lifted into the funnel.
   - Rock chunks are dragged in along the ground, lifted around the funnel and thrown out. They then fall and bounce.
   - Embers spiral up the funnel and are flung out at the top.
4. **The burned track.**
   - Scorch marks are laid every 1.7 m the tornado travels. While it stands, one is laid at least every 4 s.
   - Embers smoulder in the track, small fires burn on for 2–3 s, and thin smoke rises from it.
   - The marks fade from 14 s to 20 s. The track stays even after the carrier actor is gone.
5. **Collapse.** When the cue is removed:
   - no new particles start, and the flames burn out within about a second;
   - the last chunks are thrown out and fall;
   - the smoke drifts for 3–5 s;
   - the roar and wind loops fade over 1.5 s while a dying gust plays.
6. **Rumble.** While the tornado lives, nearby players feel a low camera rumble, full within 4 m and gone at 28 m.

### Looping cue

`GameplayCue.Spell.FireTornado` is a looping cue on the actor that carries the tornado.

`GCN_Spell_FireTornado` (`GameplayCueNotify_Looping`) runs on every machine:

| Phase | Effects |
| --- | --- |
| Application | Sounds `SW_Fire_Whoosh_01`, `SW_FireTornado_Gust_01`, `SW_FireTornado_Rumble_01` |
| Looping | `NS_Spell_FireTornado` attached to the target's root; loops `SW_FireTornado_RoarLoop_01` and `SW_FireTornado_WindLoop_01`, each fading out over 1.5 s |
| Removal | Sound `SW_FireTornado_Collapse_01` |
| Graph | On looping start, a timer plays `CS_FX_Rumble` as a world camera shake at the target every 0.45 s. On removal the timer stops. |

Effects attach with the attachment rule *Keep World*. *Snap to Target* applied the target's world location a second time, as a relative offset.

### Carrier contract

The cue carries no gameplay. Whatever moves the tornado (later a server-authoritative, replicated spell area) has to do this:

1. **Replicate:** the carrier itself, its movement and one collapse flag.
2. **Add the cue on every machine:** the carrier has no ability system, so `AddGameplayCueOnActor` plays the cue locally. Each machine adds it in `BeginPlay`, unless the collapse flag has already arrived.
3. **Remove the cue on every machine:** when the collapse flag arrives (RepNotify), and in `EndPlay` if the cue is still active. `EndPlay` also covers an actor that loses net relevancy.
4. **Keep the actor about 4 s after the collapse:** the attached loops fade out and the collapse gust plays out. The particles do not need it: when the carrier goes, the pooled Niagara component detaches and finishes the smoke and the track on its own.

`BP_VfxFireTornadoCarrier` in `GF_Dev_Sandbox` is the gallery's stand-in for that actor. It follows the contract.

### Data-driven setup

No code knows a system, sound or shake.

| What | Where |
| --- | --- |
| System, sounds, fade times, placement | `GCN_Spell_FireTornado` defaults: *Application Effects*, *Looping Effects*, *Removal Effects*, *GCN Defaults* |
| Rumble shake, interval and radii | `GCN_Spell_FireTornado` defaults, category *Fire Tornado* |
| Particle look, size (`User.Scale`) | `NS_Spell_FireTornado` |
| Demo path, life time, collapse linger | `BP_VfxFireTornadoCarrier` defaults (category *Fire Tornado Carrier*) |
| Demo cycle | `BP_VfxFireTornadoDemo` defaults (category *Fire Tornado Demo*) |

Sounds, all with `SFX_Effects` and `ATT_FX_World`:

| Sound waves | Use | Source |
| --- | --- | --- |
| `SW_Fire_Whoosh_01` (VFX-06) | Ignition | *Dungeon Sounds Pack*: `Fire_Whoosh_2-15` |
| `SW_FireTornado_Gust_01` | Formation gust | *Magic Spell SFX Pack Vol1*: `Wind_Magic_Spell_2-1` |
| `SW_FireTornado_Rumble_01` | Formation rumble | *All In One Destruction Rock*: `Rock_Rumble_Close_2_Mono` |
| `SW_FireTornado_RoarLoop_01` | Fire roar, looping | *Dungeon Sounds Pack*: `Fire_Burning_Loop_1` |
| `SW_FireTornado_WindLoop_01` | Wind, looping | *Magic Spell SFX Pack Vol1*: `Wind_Loop_1` |
| `SW_FireTornado_Collapse_01` | Collapse | *Dungeon Sounds Pack*: `Wind_Gust_1-1` |

43 candidates were exported as WAV and their envelope, spectrum and loop seams measured, as in VFX-05:

| Sound | Length | Measured |
| --- | --- | --- |
| Roar loop | 35 s | 75 % of its energy below 150 Hz; first and last 0.2 s within 0.3 dB |
| Wind loop | 7 s | 93 % below 150 Hz, a deep rush; its ends differ by 4.5 dB, which the roar covers |
| Gust | 2.3 s body | 91 % below 150 Hz |
| Rumble | 8.6 s | 83 % below 150 Hz |
| Collapse gust | 2.7 s body | Starts after 0.6 s |

A 64 s howling wind loop measured well but was left out at 11 MB.

### Assets

| Asset | Where | What |
| --- | --- | --- |
| `GameplayCue.Spell.FireTornado` | `Config/DefaultGameplayTags.ini` | Cue tag |
| `GCN_Spell_FireTornado` | `GF_Combat_Magic/GameplayCues` | Looping cue notify (above) |
| `NS_Spell_FireTornado` | `GF_Combat_Magic/FX` | The tornado. See below. |
| `CS_FX_Rumble` | core `FX/CameraShakes` | Perlin camera shake, 1 s, soft blend in and out; a copy of `CS_FX_RockBurst` tuned low for a rumble |
| `SW_FireTornado_*` (5) | core `Audio/Sounds/FireTornado` | Sounds (above) |
| `BP_VfxFireTornadoCarrier` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Replicated stand-in carrier. See below. |
| `BP_VfxFireTornadoDemo` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Gallery demo. See below. |

**`NS_Spell_FireTornado`** is attached to the carrier and loops until the carrier deactivates it.
- **User parameter:** `Scale`, where 1 is about 9 m tall and 5.5 m across at the top. It scales the funnel, the light, the debris and the track.
- **Funnel direction:** the funnel's lean comes from the owner's velocity, so the carrier's rotation does not matter.
- **Ground:** the carrier's height is taken as the ground.

| Emitter | Sim | Rate | What |
| --- | --- | --- | --- |
| `Funnel` | GPU, local | 130/s | Fire flipbook stretched along the spiral, in three turning bands, 0.7–1.05 s |
| `Roil` | GPU, local | 35/s | Rolling fire between the bands, lower funnel |
| `Core` | CPU, local | 16/s | Fireball flipbook at the base |
| `Smoke` | GPU | 24/s | Dark smoke puffs spiralling off the top, left behind as they age, 3.5–5 s |
| `SmokeWisp` | GPU | 30/s | Wisps in the smoke column, same motion |
| `Dust` | GPU | 60/s | Dust drawn in along the ground and lifted |
| `Debris` | CPU mesh | 9/s | Rock chunks drawn in, lifted, thrown out, falling and bouncing, 2.4–3.4 s |
| `Embers` | GPU | 110/s | Sparks spiralling up, thrown out at the top |
| `Haze` | CPU, local, High+ | 6/s | Heat shimmer along the lower funnel |
| `Light` | CPU, local, Medium+ | 4/s | Light, radius 13 m, as overlapping sin² pulses |
| `Ignite` | CPU | 10 once | Fire burst at the base |
| `IgniteDust` | GPU | 24 once | Ring of dust thrown out |
| `Scorch` | CPU decal | per 1.7 m, ≥ 0.25/s | Burn marks, 20 s |
| `TrackGlow` | GPU | per 35 cm + 1/s | Embers smouldering in the track, 5–9 s |
| `TrackFire` | CPU | per 1.3 m + 0.5/s | Small flames in the track, 1.8–3.2 s |
| `TrackSmoke` | GPU | per 60 cm + 1/s | Thin smoke from the track |

**`BP_VfxFireTornadoCarrier`** replicates itself and its movement:
- On the server it moves along a figure-eight of 6 m by 3.5 m around its spawn point, starting slowly.
- After 7 s it sets the replicated `bCollapsed` (RepNotify) and lives 4 s more.

**`BP_VfxFireTornadoDemo`** stands at (2100, 1100) in `Lvl_VfxGallery`. Its label "Fire Tornado (VFX-08)" stands 7 m towards the start. On the server it spawns a carrier every 14 s, the first after 2 s. It does nothing on clients; they receive the carrier.

## Performance

- **Particles:** a dump at the peak, 5 s after the formation, counted:
  - **CPU:** 59 particles (10 core, 26 chunks, 3 haze, 2 light, 10 scorch decals, 8 track fires).
  - **GPU:** about 790 particles, mostly embers (241), smoke (231) and the funnel (124).
  - **At Low:** the GPU emitters run about half.
  - **3 s after the collapse:** the smoke and the track remain. After 6 s only 13 decals and 6 glows are left.
- **GPU (`ProfileGPU`):** in Simulate at Epic quality, from about 13 m, with no other gallery effect in view. Each capture held two main-view blocks, so the table gives both values.

  | Phase | Translucency | Distortion | Lights | Decals |
  | --- | --- | --- | --- | --- |
  | Before the tornado | 0.02–0.03 ms | 0 | 0.21–0.23 ms | 0 |
  | Tornado | 0.44–0.46 ms | 0.26 ms | 0.23–0.27 ms | 0.02 ms |
  | Track after the collapse | 0.06–0.08 ms | 0 | 0.16–0.38 ms | 0.03–0.04 ms |

  The tornado adds about 0.4 ms of translucency and 0.26 ms of heat haze. The haze uses refraction and runs on High+ only.
- **Quality tiers:**
  - `FXT_Gameplay_Looping`. Low halves the spawn counts and turns off the light and the heat haze. A Low capture shows a complete tornado.
  - `Core`, `Light`, `Haze` and `Scorch` keep their counts on every level, so the base stays bright and the track has no gaps.
- **Scalability overrides:** the system overrides two settings of its Effect Type and keeps its instance limits.
  - **Visibility:** it is never culled for being out of view, because the track lasts 20 s ([vfx-roadmap.md](vfx-roadmap.md), Performance).
  - **Distance:** it is culled only beyond 100 m on every level, instead of 30/45/60 m. The column is 20 m high and seen from far. A looping cue spawns its system only once, so a spawn refused for distance would stay missing for the whole life of the tornado.
- **Budget:** twelve tornados at once at Low: 8 ran, 4 were deactivated by the Effect Type. All twelve played their sounds.
- **Bounds:** fixed. ±30 m horizontally and −5 m to +24 m vertically, for the track and the smoke left behind.
- **Network:**
  - The carrier's replicated movement and one bool.
  - The cue itself sends nothing: every machine adds and removes it locally.

## Validation

- **No C++ changes.** The editor was rebuilt only for C++ that master brought in (`ue.py build` passed).
- **Look:** checked in Simulate in the gallery:
  - the formation, the moving funnel from the side and from above, and the collapse;
  - the track after the carrier was gone;
  - Epic and Low (`sg.EffectsQuality 0`).
- **Multiplayer (listen server and one client, one process, gallery):**
  - **Normal cycle:** the client received each carrier as a simulated proxy. At the same moment it stood at the same position as on the server, e.g. (2583, 766) on both. Both machines had the cue active, the tornado system attached and running, and four sounds playing. The collapse arrived on the client, which removed the cue. The carrier then went away on both, and the next one came on both.
  - **Relevancy loss:** the server shrank one carrier's net cull distance to 20 m; the client player stood about 45 m away. About 5 s later the client destroyed its copy. Its tornado system went to `Inactive` and the track stayed.
  - **Late join mid-life:** the cull distance was restored at a carrier age of about 5.3 s. The client received the carrier again at about 6.3 s, with the cue active and the tornado running.
  - **Late join after the collapse:** with the carrier still hidden, the server collapsed it. When it became relevant again, the client received it with `bCollapsed` set, added no cue and attached nothing.
  - **Log:** in all runs, no errors apart from the connection close at the end of PIE.
- **Budget:** 12 tornados at Low (above): no errors.
- **Gallery PIE:** 40 s with the demo running, 0 Blueprint runtime errors.
- **Asset validation:** the 11 new or changed assets are valid with 0 warnings, the gallery map included. No dependency points into the asset library projects.
- **After an editor restart:** the demo looked as before, with orange flames and glowing embers in the track.
- **Cook:** `python Build/Tools/Unreal/ue.py cook Lvl_VfxGallery` passed with 0 errors, also after the cue change. It cooked the system, the cue notify, the carrier, the demo, the shake and the sounds. Its 3 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick.

## Authoring notes

- **Placement of looping cue effects:** with the attachment rule *Snap to Target*, `SpawnSystemAttached` and `SpawnSoundAttached` use the target's world location as a relative offset. The tornado then appeared 23 m off, beyond the gallery wall. Use *Keep World*.
- **Looping notify events:** `OnLoopingStart` and `OnRemoval` are event-graph events (`EventOnLoopingStart`), not overridable functions.
- **Effect info structs from Python:** the cue's particle and sound info structs are edit-defaults-only. Build them from text with `import_text` and assign the whole struct.
- **New gameplay tags:** a tag added to `DefaultGameplayTags.ini` while the editor runs is unknown until a restart. Setting it from Python before then stores an empty tag.
- **Net relevancy in tests:** an actor that stops being relevant is closed only after the net driver's relevant timeout, about 5 s. When it becomes relevant again, the client gets a new actor with a new name.
- **Detached pooled systems:** when the carrier is destroyed, its pooled Niagara component detaches and finishes its world-space particles on its own.

## Playtest

1. **Gallery:**
   1. Open `/GF_Dev_Sandbox/Maps/Lvl_VfxGallery`, press Play and walk to "Fire Tornado (VFX-08)", about 30 m ahead and 11 m to the right of the start.
   2. Every 14 s a tornado forms a few metres behind the label. Expect:
      - a whoosh, a gust and a rumble as fire bursts out and dust is thrown over the ground;
      - the column rising and wandering a figure-eight for 7 s, with roaring fire and wind and its light on the ground;
      - dust and chunks drawn in and thrown out, embers flying off the top, and a black smoke column;
      - a camera rumble within about 28 m;
      - the collapse: the flames burn out, a dying gust plays, chunks fall;
      - a burned track with glowing embers and small fires, fading after about 20 s.
2. **Low quality:** run `sg.EffectsQuality 0` in the console. The tornado stays complete, with fewer particles and without light or heat haze.
3. **Two players:** set Play to 2 players, *Play As Listen Server*. Both windows see the same tornado at the same place and time, because the server drives it.

## Limits

- **Presentation only:** there is no spell, damage area, damage or mana. A server-authoritative spell area that moves and applies damage follows as its own task; the carrier contract above is what it needs.
- **Sounds not heard:** they were chosen by measured envelope, spectrum and loop seams and checked only for playback, not by ear.
- **Flat ground:** the funnel, debris, track embers and fires use the carrier's height as the ground. On slopes or steps they float or sink; the scorch decals project 2 m deep.
- **Refused spawn:** if the Effect Type refuses the spawn (budget or distance), that machine shows no tornado for this carrier, even after it comes into range. The sounds and the rumble still play. Simulate did not reproduce a distance refusal, because it has no player view for the pre-cull. A tornado that is culled later is deactivated and resumes, and its formation burst then plays again.
- **Network smoothness:** the carrier sends its location without interpolation. It ran smoothly in PIE, but with real latency the tornado could step. The spell area should smooth its movement.
- **Late join:** checked by moving the carrier in and out of net relevancy, not with a client joining a running session.
- **GPU numbers:** from Simulate in the daylight gallery; fire and its light read stronger at dusk and at night.
