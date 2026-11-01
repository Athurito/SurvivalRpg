# VFX-05: Fireball

Task report for VFX-05 in the [VFX roadmap](vfx-roadmap.md). The game vision lists the fireball as the first elemental spell of Magic Mastery ([game-vision.md](game-vision.md), section 9.3). This task builds its presentation: a launch, a fireball in flight and a heavy explosion with sound and camera shake. The gallery casts it as a prototype. No gameplay changes: casting, the projectile, damage and mana come with the projectile foundation and combat magic ([combat-roadmap.md](combat-roadmap.md), "Later").

The roadmap originally grouped seal break, rift bolt, ash fetter and Rift Sense under VFX-05. The project owner asked for more powerful spells on 2026-10-20, so VFX-05 became the fireball. The tornado, meteor, flame wall, earth rift and chain lightning followed as VFX-08 to VFX-12, and the earlier spells moved to VFX-13 and VFX-14.

## Behavior

### The effect

1. **Launch.** Fire bursts from the launch point and leaves a puff of smoke. For the first 0.12 s the ball's glow and light flare. A fire whoosh plays.
2. **Flight.** A roiling ball of fire, about 80 cm across, flies with:
   - a white-hot heart and flame tongues that stream about a metre back against the flight direction;
   - heat haze (High+) and its own light (Medium+, radius 7 m), which lights the ground and walls it passes;
   - shed sparks and a thin smoke trail that stays in the air.

   A deep roaring loop follows the ball.
3. **Impact.**
   - A short flash (0.09 s) and a light that floods 28 m and then glows on as fire for 1.7 s.
   - A fire billow that cools into a dark smoke cloud rising for 5–7 s.
   - The shockwave: a ring of dust rolls out over the ground, and a heat ripple races out (High+).
   - Sparks, embers that ride up with the smoke, and rock chunks torn out of the ground.
   - Small fires burn on for 3–5 s, embers smoulder in the scorch mark for 7–11 s, and the mark fades from 15 s to 22 s.
   - Against a wall the billow, sparks and debris leave along the wall's normal. The scorch is projected onto the wall when the ground is more than 2 m (scaled by the blast radius) below the impact, otherwise onto the ground.
4. **Sound and shake.** A boom, a rock crack and a fire crackle play at the impact, and a debris rattle when the impact is near the ground. Nearby players get a strong camera shake, full within 6 m and gone at 35 m.

### Impact cue

`GameplayCue.Spell.Fireball.Impact` is the hook for the later projectile:
- **Parameters:**
  - `Location`: the impact point;
  - `Normal`: the surface normal;
  - `RawMagnitude`: the blast radius in cm, where 0 uses the cue default of 300 cm.
- **Sender:** the projectile (`ARpgCombatProjectile`, RNG-01) executes it on authority through the GameState's ability system, and GAS multicasts it to every client. The GameState stays relevant and keeps its avatar, so the cue also plays after the shooter died. The gallery demo executes it locally.
- **Registration:** `GF_Combat_Magic` now registers its `/GameplayCues` folder through `RpgGameFeatureAction_AddGameplayCuePath`. It had no content before.

`GCN_Spell_Fireball_Impact` (burst notify) runs locally on every machine:
1. Machines whose camera is farther than `MaxDistance` (120 m) skip the impact.
2. A missing normal counts as up.
3. A trace down from just off the surface finds the ground below the impact, at most `MaxGroundDepth` (15 m) down.
4. It spawns `NS_Spell_Fireball_Impact` at the impact point with AutoRelease pooling. It sets `Radius`, `GroundDepth` and `Normal` only if the Effect Type did not refuse the spawn (`IsValid`).
5. It plays one random sound from each list: boom, crack and burn, and debris when the ground is within `DebrisMaxHeight` (2.5 m). The sounds play even when the spawn was refused.
6. The notify's own burst effects play `CS_FX_RockBurst` as a world camera shake at scale 1.6.

### Flight system

`NS_Spell_Fireball_Projectile` is attached to whatever carries the fireball:
- **Start:** `Activate` (reset) at launch plays the launch burst.
- **Stop:** `Deactivate` on impact removes the ball, glow, flames and light within a tenth of a second. The smoke trail and sparks fade in the air.
- **Direction:** the flames take the flight direction from the owner's velocity, so the carrier's rotation does not matter. While the carrier stands still they point along its local −X.
- **Size:** `User.Scale` (1 = about 80 cm across) scales the ball, the flames, the light and the trail.

What the projectile foundation has to do with it:
1. Attach the system and the flight loop `SW_Fireball_FlightLoop_01` to the projectile on every machine, and play a launch sound.
2. On the authoritative hit, execute the impact cue with the hit point, the hit normal and the blast radius.
3. On every machine, deactivate the system and stop the loop when the projectile stops. Keep the component alive for about 2 s so the trail can fade.

### Data-driven setup

No code knows a system, sound or shake.

| What | Where |
| --- | --- |
| Impact system, sounds, default radius, distance cut-off, ground trace depth, volumes, debris height | `GCN_Spell_Fireball_Impact` defaults (category *Fireball Impact*) |
| Camera shake, its radii and scale | `GCN_Spell_Fireball_Impact`, *Burst Effects* → *Burst Camera Shake* |
| Particle look | `NS_Spell_Fireball_Projectile`, `NS_Spell_Fireball_Impact` |
| Demo flight: speed, cycle, targets, launch sounds | `BP_VfxFireballDemo` defaults (category *Fireball Demo*) and its target components |

Sounds, all with `SFX_Effects` and `ATT_FX_World`:

| Sound waves | Use | Source |
| --- | --- | --- |
| `SW_Fireball_Launch_01`–`03` | Launch (demo, later the projectile) | *Dungeon Sounds Pack*: `Fire_Whoosh_2-1`, `2-8`, `1-1` |
| `SW_Fireball_FlightLoop_01` | Flight loop, looping | *Magic Spell SFX Pack Vol1*: `Fire_Loop_1` |
| `SW_Fireball_Boom_01`–`03` | `BoomSounds` | *Magic Spell SFX Pack Vol1*: `Fire_Magic_Spell_1-3`, `3-2`, `3-3` |
| `SW_Fireball_Crack_01`–`02` | `CrackSounds`, volume 0.8 | *All In One Destruction Rock*: `Rock_Various_Impact_Crumble_Large_1_Mono`, `_2_Mono` |
| `SW_Fireball_Debris_01`–`03` | `DebrisSounds`, volume 0.6 | *Magic Spell SFX Pack Vol1*: `Debris_1`–`3` |
| `SW_Fireball_Burn_01`–`02` | `BurnSounds`, volume 0.45 | *Magic Spell SFX Pack Vol1*: `Fire_Burn_1-1`, `1-5` |

The sounds were chosen by measurement: 87 candidates were exported as WAV and their envelope and spectrum were measured.
- **Booms:** reach their peak within 0.2 s, carry about 90 % of their energy below 150 Hz and ring out for over 2 s.
- **Cracks:** reach full scale within 20 ms.
- **Flight loop:** a deep 10 s loop whose start and end match in level.

### Assets

| Asset | Where | What |
| --- | --- | --- |
| `GameplayCue.Spell.Fireball.Impact` | `Config/DefaultGameplayTags.ini` | Cue tag |
| `GF_Combat_Magic` (changed) | `GF_Combat_Magic` | Game feature data: registers `/GameplayCues` |
| `GCN_Spell_Fireball_Impact` | `GF_Combat_Magic/GameplayCues` | Burst cue notify (above) |
| `NS_Spell_Fireball_Impact` | `GF_Combat_Magic/FX` | The explosion. See below. |
| `NS_Spell_Fireball_Projectile` | `GF_Combat_Magic/FX` | The fireball in flight. See below. |
| `T_FX_FireBall_EOO`/`_N`, `T_FX_Explosion_EOO`/`_N` | core `FX/Textures/Fire` | Niagara Examples flipbooks, 8×8 frames ([vfx-foundation.md](vfx-foundation.md)) |
| `MI_FX_FireBall`, `MI_FX_Explosion` | core `FX/Materials/Instances` | The flipbooks on `M_FX_SmokeAndFire`, as `MI_FX_FireRoil` |
| `SW_Fireball_*` (14) | core `Audio/Sounds/Fireball` | Sounds (above) |
| `BP_VfxFireballDemo` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Gallery demo. See below. |
| `GF_Dev_Sandbox.uplugin` (changed) | `GF_Dev_Sandbox` | Depends on `GF_Combat_Magic` for the demo |

**`NS_Spell_Fireball_Impact`** is spawned at the impact point, without rotation. User parameters:
- `Radius`: the blast radius in cm, default 300; it scales every part between 0.4× and 2×;
- `GroundDepth`: cm from the impact point down to the ground. Dust, fires, embers and debris fade out above about 2.5 m (scaled);
- `Normal`: the surface normal.

| Emitter | Sim | Per impact | What |
| --- | --- | --- | --- |
| `Flash` | CPU sprites | 2 | Yellow-white flash, 0.09 s; orange afterglow, 0.6 s |
| `Light` | CPU, Medium+ | 2 | Flash light, radius 28 m, 0.16 s; flickering fire glow, radius 10 m, 1.7 s |
| `Billow` | CPU sprites | 20 | Explosion flipbook played once, 2–3 m sprites; most stay at the core, a few lobes are flung out; they rise as they cool |
| `Smoke` | CPU sprites | 10 | Dark smoke above the billow, fading in after 0.55 s, rising for 5–7 s |
| `DustRing` | GPU | 28 | Dust puffs racing out along the ground at 9–16 m/s with drag |
| `Shockwave` | CPU, High+ | 1 | Heat ripple, growing to 16 m across in 0.42 s |
| `Sparks` | GPU | 110 | Hot fragments, velocity-stretched, bouncing on the ground |
| `Embers` | GPU | 55 | Glowing flakes that ride up and drift for 2.5–5.5 s |
| `Debris` | CPU mesh | 14 | Rock chunks 20–60 cm, spinning and bouncing; only near the ground |
| `GroundFire` | CPU sprites | 7 | Flame tongues around the impact, lit one after another, 3–5 s |
| `Glow` | CPU sprites | 10 | Smouldering embers lying flat in the scorch mark, 7–11 s |
| `Scorch` | CPU decal | 1 | Burn mark, 4.5 m across at the default radius, 22 s |

**`NS_Spell_Fireball_Projectile`** loops until its carrier deactivates it:

| Emitter | Sim | Rate | What |
| --- | --- | --- | --- |
| `Core` | CPU, local | 16/s | Fireball flipbook, looping at 30 fps, 70–95 cm |
| `Glow` | CPU, local | 14/s | Hot heart; two overlapping sin² pulses keep it steady |
| `Flames` | CPU, local | 90/s | Fire tongues stretched along the flight direction, streaming back about 1 m |
| `Haze` | CPU, local, High+ | 8/s | Heat shimmer around the ball |
| `Light` | CPU, local, Medium+ | 20/s | Light, radius 7 m, as overlapping sin² pulses |
| `Smoke` | GPU | 75/s | Faint wispy trail, spread along each frame's path so it has no gaps |
| `Embers` | GPU | 70/s | Sparks shed in flight |
| `Launch` | CPU | 6 once | Fire burst at the launch point |
| `LaunchSmoke` | CPU | 5 once | Smoke puff at the launch point |

**`BP_VfxFireballDemo`** stands at (700, −700) in `Lvl_VfxGallery`, labelled "Fireball (VFX-05)".
- A mannequin faces a 4×3.5 m wall 20 m ahead.
- Every 4 s (first after 2 s) it throws a fireball at 24 m/s from its hand, in turn at the wall, the ground in front of the wall and the ground to the left.
- Each frame a line trace along the flight step finds the hit. The demo then deactivates the flight system and the loop, and executes the real impact cue with the hit point, normal and a 300 cm radius.
- It runs locally on every machine and is not replicated.

## Performance

- **Particles:**
  - Impact: a live dump at 0.5 s counted 64 CPU particles (flash, light, 20 billow, 10 smoke, 14 chunks, 7 fires, 10 embers in the scorch, the decal) and 193 GPU particles. After 5 s, 21 CPU particles remain; after 12 s, the decal.
  - Flight: about 37 CPU particles (rate × lifetime), plus 11 for 0.5 s after the launch, and about 155 on the GPU, mostly the smoke trail.
- **GPU (`ProfileGPU`):** in Simulate on the gallery at Epic quality (internal 1468×783), view along the flight path towards the wall, real demo cast in slow motion. Other gallery effects were in view, and each capture held two main-view blocks, so the table gives both values.

  | Phase | Translucency | Distortion | Lights |
  | --- | --- | --- | --- |
  | Before the cast | 1.28–1.57 ms | 0.27–0.70 ms | 0.31–1.03 ms |
  | In flight | 1.87–1.88 ms | 0.85–0.86 ms | 0.30–0.33 ms |
  | Impact (0.1 s) | 0.91–1.72 ms | 0.28–0.84 ms | 0.28–0.62 ms |
  | Smoke (1.5 s) | 0.84 ms | 0 | 0.97 ms |

  - The ball adds about 0.3–0.6 ms of translucency close up. Its heat haze and the impact's heat ripple use refraction and run on High+ only; the ripple lasts 0.42 s.
- **Quality tiers:**
  - The impact uses `FXT_Gameplay_Burst` and the flight `FXT_Gameplay_Looping`.
  - Low halves the spawn counts and turns off the lights and the heat effects. A Low capture shows a complete explosion.
  - `Flash`, `Light`, `Shockwave` and `Scorch` of the impact and `Core`, `Glow`, `Haze` and `Light` of the flight keep their counts on every level.
  - The lights run from Medium up, not only from High as in earlier tasks: the light of a fireball is its main read in the dark. They are single particles.
  - The impact overrides only the visibility settings of its Effect Type, because it leaves marks for 22 s ([vfx-roadmap.md](vfx-roadmap.md), Performance).
- **Bounds:** fixed. Impact ±26 m horizontally, −10 m to +36 m vertically; flight ±35 m, for the trail it leaves in world space.
- **Network:** one unreliable multicast per impact, on the instigator's ability system.

## Validation

- **No C++ changes;** no build needed.
- **Look:** checked in Simulate in the gallery:
  - ground and wall impacts, from the ground and from above, in slow motion (0.1 and 0.25);
  - the flight close up at 0.1;
  - the impact at Epic and at Low (`sg.EffectsQuality 0`).
- **Lifetime:** a component dump after one impact showed every emitter alive at 0.5 s, the smoke and embers at 5 s, and only the decal at 12 s.
- **Multiplayer (listen server and one client, one process, gallery):**
  - A temporary demo function executed the cue on the host's ability system from a timer on the server, so the multicast was sent in the server tick. The log shows the notify run on the server and on the client, on the host's simulated proxy. Both spawned `NS_Spell_Fireball_Impact`, and the client's window shows the explosion. The function was removed afterwards.
  - The demo's own cast on the client spawned the explosion in the client world.
- **Refused spawn:** with `ImpactSystem` cleared in memory, 20 s of gallery PIE ran the demo's impacts without a Blueprint runtime error. The sounds played, the explosion did not.
- **Gallery PIE:** 40 s with the demo casting, 0 Blueprint runtime errors.
- **Asset validation:** the 26 new or changed assets are valid with 0 warnings, the gallery map included. No dependency points into the asset library projects.
- **After an editor restart:** a particle dump showed the scorch embers orange, and the demo's flight and wall impact looked as before.
- **Cook:** `python Build/Tools/Unreal/ue.py cook Lvl_VfxGallery` passed with 0 errors. It cooked both systems, the cue notify, the demo, the sounds and the core textures and materials. Its 3 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick.

## Authoring notes

- **Stale compiled emitters after an editor restart:** after a restart, the scorch embers rendered white although their colour expression was set. A particle dump showed the default colour. Forcing a recompile by opening the system fixed it. Since then the systems are saved with their editor closed, and a package reload confirms the result.
- **`ExecuteGameplayCueOnActor`** on an actor with an ability system fires only on authority and then replicates. Called on a client, it does nothing. Actors without one, such as the demo, play the cue locally.
- **Testing replicated cues from `exec_python`:** multicasts sent from a Python call outside the world tick did not reach the PIE client. Start them from a timer (`SystemLibrary.set_timer` on a Blueprint function), so they run in the server tick.
- **Template emitters:** a system created from a template keeps the template's emitters with their modules. Gravity and drag modules then add to the expressions. Remove them and rebuild from the Minimal emitter.
- **Local flames, world velocity:** the local-space emitters turn the owner's world velocity into local space with `Engine.Owner.SystemWorldToLocalNoScale`.

## Playtest

1. **Gallery:**
   1. Open `/GF_Dev_Sandbox/Maps/Lvl_VfxGallery`, press Play and walk to "Fireball (VFX-05)", about 23 m ahead and 7 m to the left of the start.
   2. Every 4 s the mannequin throws a fireball, in turn at the wall, at the ground in front of it and at the ground to the left. Expect:
      - fire and a whoosh at the hand;
      - the ball with a flame tail and its light on the ground, roaring as it passes;
      - on impact a boom and a crack, a flash, a fire billow, a ring of dust, flying chunks and sparks, and a camera shake within about 35 m;
      - black smoke rising for several seconds;
      - small fires and glowing embers in the scorch mark, which fades after about 20 s.
2. **Low quality:** run `sg.EffectsQuality 0` in the console. The explosion stays complete, with fewer particles and without lights or heat haze.
3. **Two players:** set Play to 2 players, *Play As Listen Server*. Both windows see the demo. Each machine flies its own fireball, so the windows are not in sync.

## Limits

- **Presentation only:** there is no cast ability, projectile, damage or mana. The projectile foundation and combat magic follow as their own tasks.
- **No hand charge:** the launch burst sits in the flight system. A charge-up at the caster's hand needs the cast animation of combat magic.
- **Demo not replicated:** the replicated path was checked through the cue, not through a projectile. There is no automated network test, because no gameplay sends the cue yet. The projectile foundation should add one.
- **Sounds not heard:** they were chosen by measured envelope and spectrum and checked only for playback, not by ear. The project owner has not listened to them yet.
- **Ground:** debris and sparks bounce on a flat plane at the height found below the impact. Small fires and the scorch assume that plane too.
- **GPU numbers:** taken with other gallery effects in view, so they show the range, not an isolated cost.
- **Look:** judged from editor captures in the daylight gallery; fire and its light read stronger at dusk and at night.
