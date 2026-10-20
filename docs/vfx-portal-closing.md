# VFX-04: Portal closing

Task report for VFX-04 in the [VFX roadmap](vfx-roadmap.md). Until now a portal showed only a placeholder mesh while open and simply vanished when it closed. With this task the open portal is a burning rift, and its closing is staged as a short sequence with a rune ring, an implosion and a scar. Glowing embers float above the scar for half a minute afterwards.

## Behavior

### The open rift

`NS_Portal_Rift` replaces the visible placeholder mesh of `BP_Portal_RiftGruntTrial`. The mesh keeps its collision; only its visibility is off.
- A black, slowly churning void, about 1.8 m wide and 2.7 m high, in the vertical plane of the portal.
- A rim of white-hot beads and a soft glow band around it, with small flames licking off the edge.
- Embers drift inside the void; ash and thin smoke rise from the edge.
- A warm light from the rim (High+) and heat shimmer over the rift (High+).

User parameters, set by the closing sequence:
- `Destabilise` (0–1): more rim particles, brighter flames, and sparks drawn into the rift from all around.
- `Collapse` (0–1): shrinks the whole rift towards its centre.

### The closing sequence

A machine that sees the portal close plays this sequence. All times are seconds after the close and are designer data on `BP_PortalCloseSequence`.

| Time | Effect | Sound |
| --- | --- | --- |
| 0 | The rift starts to destabilise. | A rising tension swell at the rift (one of two). |
| 0.55–1.32 | Eight runes brand into the ground in a ring of 3 m radius, one every 0.11 s. Each flares white-hot, throws sparks and leaves a burn mark. | A metallic lock click per rune, pitch rising from 0.9 to 1.35. |
| 1.43 | The ring is complete. | A heavy gate slams shut (one of two). |
| 1.5–1.95 | The rift collapses into its centre, faster towards the end. | A sucking whoosh into the rift. |
| 1.95 | **Implosion:** the rift is gone in one frame behind a white pinch flash and a light flash (High+). Dust races outward over the ground, a heat ripple runs across it (High+), sparks fly out, and the runes flare once more. Nearby players feel a camera shake. | An impact, a dark boom, a long rock rumble and chimes, all at once. |
| after | Up to 220 embers float and drift in the air for 14–40 s, cooling from orange-white to orange. Ash flakes sink down for 8–22 s, and thin smoke rises from the scar. | A quiet arcane residue loop fades out over 22 s. |
| 0–20 | The runes cool from white-hot over orange to dull red and fade out between 6 s and 18 s after their brand. | |
| up to 60 | The burn marks under the runes and a large scar under the rift fade between 42 s and 58 s. | |

### Gameplay hook

`ARpgPortalActor` (C++, `GF_Portals_Core`) gets one presentation event; gameplay is unchanged.
- **`OnPortalClosedPresentation(bool bInitialState)`** is a `BlueprintNativeEvent`. It runs when the closed state reaches a machine: on the server through `SetPortalState`, on clients through `OnRep_PortalState`. A dedicated server skips it.
- **`bInitialState`** is true when this machine never saw the portal open: a late joiner, or a client the portal becomes relevant to only after it closed. Such a machine shows the end state instead of the sequence. The portal remembers in `bSawOpenState` whether it was ever open on this machine.
  - A first attempt compared the close time with the replicated server time. It failed the late-join test, because a joining client has not synced the server clock yet when the state arrives.
- **Why not a GameplayCue:** the close is a state change of a replicated actor that late joiners must reconstruct, not a one-off ability event. The replicated state already reaches every client, so no cue RPC is needed.
- **`Rpg.Portal.MakeSealable`** (console, not in shipping builds) fills the stability of every open portal on the server and makes it sealable at once. The close can then be playtested without fighting the encounter.

### Blueprint content

- **`BP_Portal_RiftGruntTrial`** (changed):
  - New Niagara component `RiftFX` with `NS_Portal_Rift`, 1.5 m above the root.
  - `PortalMesh` is hidden.
  - `CloseSequenceClass` (category *Portal Presentation*) selects the sequence actor.
  - Its `OnPortalClosedPresentation` hides and deactivates `RiftFX` for an initial state. Otherwise it spawns the sequence at the portal and passes it `RiftFX` and the rift's height.
- **`BP_PortalCloseSequence`** (new actor, `GF_Portals_Core/FX`) runs the sequence locally.
  - `Start` spawns `NS_Portal_Close` at the ground with AutoRelease pooling and passes it the timing. It plays the tension sound and steps the sequence on a 16 ms looping timer.
  - Each step drives `Destabilise` and `Collapse` on the rift, plays the rune, seal and suck sounds on time, and handles the implosion. At the implosion it hides and deactivates the rift, plays the implosion sounds and the world camera shake, and starts the residue loop with its fade.
  - The actor lives 45 s. The particle system and the residue sound are independent of it.

### Data-driven setup

No code knows a system, sound or shake. Everything is a default on `BP_PortalCloseSequence` (category *Close Sequence*):

| Value | Default | What |
| --- | --- | --- |
| `RuneStart`, `RuneInterval` | 0.55 s, 0.11 s | First rune and spacing; eight runes |
| `CollapseStart`, `ImplodeTime` | 1.5 s, 1.95 s | Rift collapse and implosion |
| `RingRadius` | 300 cm | Rune ring radius |
| `RuneLockPitchStart`, `RuneLockPitchEnd` | 0.9, 1.35 | Pitch of the first and last rune click |
| `ResidueFadeSeconds` | 22 s | Fade of the residue loop |
| `ShakeInnerRadius`, `ShakeOuterRadius` | 5 m, 30 m | Camera shake, full inside and gone outside |
| `LifeSeconds` | 45 s | Lifetime of the sequence actor |
| `CloseSystem` | `NS_Portal_Close` | Particle system |
| `TensionSounds`, `RuneLockSounds`, `SealLockSounds`, `SuckSound`, `ImplodeSounds`, `ChimeSounds`, `ResidueLoop` | see below | One random sound per list; all implode sounds play together |
| `CameraShake` | `CS_FX_RockBurst` | World camera shake at the implosion |

The particle timing reaches `NS_Portal_Close` as user parameters, so the sounds and the particles stay in sync when a designer changes it.

### Assets

| Asset | Where | What |
| --- | --- | --- |
| `NS_Portal_Rift` | `GF_Portals_Core/FX` | The open rift (above), `FXT_Gameplay_Looping` |
| `NS_Portal_Close` | `GF_Portals_Core/FX` | The closing (below), `FXT_Gameplay_Burst` with a scalability override |
| `BP_PortalCloseSequence` | `GF_Portals_Core/FX` | Sequence actor (above) |
| `BP_Portal_RiftGruntTrial` (changed) | `GF_Portals_Core/Portals` | `RiftFX`, hidden mesh, presentation event |
| `M_FX_AdditiveSubUV`, `MI_FX_RuneBurn` | core `FX/Materials` | `M_FX_Additive` with a SubUV mask; the rune instance uses the existing 3×1 glyph atlas `T_FX_RuneGlyphs` |
| `SW_Portal_*` (16 sound waves) | core `Audio/Sounds/Portal` | Sounds from the sound library, see below |
| `BP_VfxPortalCloseDemo` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Gallery demo, see below |

Sounds, all with `SFX_Effects` and `ATT_FX_World`:

| Sound waves | Source |
| --- | --- |
| `SW_Portal_Tension_01`, `_02` | *Magic Spell SFX Pack Vol1*: `Negative_Magic_Effect_3-1`, `10-4` |
| `SW_Portal_RuneLock_01`–`05` | *Dungeon Sounds Pack*: `Metallic_Gear_1-04`, `1-06`, `2-01`, `2-06`, `3-01` |
| `SW_Portal_SealLock_01`, `_02` | *Dungeon Sounds Pack*: `Gate_Close_1-1`, `2-1` |
| `SW_Portal_Suck_01` | *Magic Spell SFX Pack Vol1*: `Magic_Cast_Whoosh_2-2` |
| `SW_Portal_ImplodeHit_01`, `SW_Portal_ImplodeBoom_01` | *Magic Spell SFX Pack Vol1*: `Special_Hit_1-1`, `Dark_Magic_Spell_8-2` |
| `SW_Portal_Rumble_01` | *All In One Destruction Rock*: `Rock_Rumble_Close_Mono` |
| `SW_Portal_Chimes_01`, `_02` | *Dungeon Sounds Pack*: `Magic_Chimes_1-1`, `1-5` |
| `SW_Portal_ResidueLoop_01` (looping) | *Magic Spell SFX Pack Vol1*: `Arcane_Loop_1` |

**`NS_Portal_Rift`** is attached 1.5 m above the portal root. User parameters `RadiusY` (90 cm) and `RadiusZ` (135 cm) set the void's half width and half height.

| Emitter | Sim | Live when open | What |
| --- | --- | --- | --- |
| `Void` | CPU, local | ~10 | Dark churning puffs; pairs face both sides, because a one-sided custom-facing sprite is culled from behind |
| `InnerEmbers` | GPU, local | ~50 | Embers drifting in the void |
| `Rim` | GPU, local | ~300 | White-hot beads along the edge; twice the rate when destabilised |
| `RimGlow` | GPU, local | ~60 | Soft glow band along the edge |
| `RimFlames` | CPU, local | ~17 | Small flame wisps off the edge |
| `Ash`, `Smoke` | GPU | ~50, ~11 | Ash and thin smoke rising off the edge |
| `Suction` | GPU | 0, up to ~150 | Sparks drawn into the rift while it destabilises |
| `Haze` | CPU, local, High+ | ~3 | Heat shimmer |
| `Light` | CPU, local, High+ | 1 | Warm rim light |

**`NS_Portal_Close`** is spawned on the ground under the portal. User parameters `RuneStart`, `RuneInterval`, `ImplodeTime`, `RingRadius` and `RiftHeight` come from the sequence.

| Emitter | Sim | Count | What |
| --- | --- | --- | --- |
| `Runes` | CPU | 8 | Flat glyph sprites branded into the ground; heat, cooling and the implosion flare are computed per particle |
| `RuneSparks` | GPU | 128 | A spark fountain per rune at its brand |
| `Scorch` | CPU decal | 9 | A burn mark under each rune and a scar under the rift, 60 s |
| `Pinch` | CPU | 1 | The point the rift collapses into, then the white flash |
| `ShockDust` | GPU | 30 | Dust racing outward over the ground |
| `ShockWave` | CPU, High+ | 1 | Heat ripple across the ground |
| `ImplodeSparks` | GPU | 90 | Hot fragments blown out of the closing point |
| `Light` | CPU, High+ | 2 | Ring glow growing rune by rune, then the implosion flash |
| `Motes` | GPU | 220 | Embers floating in the air for 14–40 s |
| `Ash` | GPU | 90 | Ash flakes sinking down for 8–22 s |
| `ScarSmoke` | GPU | 8 | Thin smoke rising from the scar |

**`BP_VfxPortalCloseDemo`** stands at (900, 3100) in `Lvl_VfxGallery`, labelled "Portal Close (VFX-04)".
- An open rift with a mannequin next to it for scale.
- It closes 3 s after the start and then every 30 s, through the same sequence actor as the real portal. The rift reopens 24 s after each close, so the floating embers stay visible first.

## Performance

- **Particles:**
  - Open rift: about 30 on the CPU and 470 on the GPU; up to about 800 on the GPU while it destabilises.
  - Closing: 21 on the CPU and 566 on the GPU. Of the GPU particles, 310 live 8–40 s (embers and ash), the rest at most 2 s.
- **GPU (`ProfileGPU`):** in Simulate on the gallery at Epic quality, close view, in four phases: open rift, runes (1.55 s), implosion (2.1 s) and tail (10 s).
  - Niagara GPU simulation: at most 0.06 ms.
  - Decals: about 0.05 ms.
  - The effect's own translucency stays below about 0.1 ms. Most of the translucency pass is clearing its render target, as in VFX-07.
  - The heat shimmer is the only notable cost: the engine's distortion pass took 0.8–0.95 ms while the rift's `Haze` or the implosion's `ShockWave` was on screen, and 0 in the tail. Both emitters therefore run only on High and above.
  - Whole frame times varied between 16 and 22 ms with Lumen and virtual shadow maps in the editor and are not comparable.
- **Quality tiers:**
  - Low halves the spawn counts of the rift's emitters and of the closing's sparks, dust, embers, ash and smoke, and turns off the lights and both heat shimmers.
  - `Runes`, `Scorch`, `Pinch`, `ShockWave` and `Light` keep their counts on every level. A ring needs all eight runes, and single sprites and decals would vanish when halved.
  - Checked at Low (`sg.EffectsQuality 0`): rift, runes, scars and floating embers show.
- **Scalability override:** `NS_Portal_Close` overrides its Effect Type on every quality level:
  - no global instance budget;
  - no visibility culling or frustum pre-culling;
  - at most 4 instances of the system;
  - distance culling at 80 m.

  On Low, `FXT_Gameplay_Burst` allows only 12 burst instances. The gallery's long burning Ash Wave demo held 11, and the close was dropped at its spawn. A once-per-encounter close that leaves marks for a minute must also not be lost because the camera looked away at that moment.
- **Bounds:** fixed. Rift ±9 m horizontally, −4 m to +9 m vertically. Closing ±18 m horizontally, −3 m to +14 m vertically.
- **Network:** no RPC. The close travels in the replicated portal state that existed before.

## Validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed after the native hook (commit `b4a92270`).
- **New native test `SurvivalRpg.Portals.Close.PresentsAFreshCloseOnce`:** passed. A portal that closes on a server presents one fresh close; setting the closed state again presents nothing.
- **New network test `SurvivalRpg.Network.PortalClosePIE.CloseIsPresentedOnceOnClientsAndAsEndStateOnLateJoin`:** passed.
  1. A dedicated server with two clients, and a replicated portal fixture that runs a real outbreak encounter.
  2. The server closes the portal: each client presents exactly one fresh close.
  3. A third client joins after the close and presents only the end state.
- **Regression proof:** with `bInitialState` forced to false, the late-join step timed out. It passed again with the code restored.
- **`SurvivalRpg.Portals` and the network test (5 tests):** passed on 2026-10-20 after the content changes. The network test logs engine NetGUID warnings for its transient test level and fixture subobject. The storage test logs its expected warning for a full inventory.
- **Real portal, listen server with one client (PIE, `Lvl_ResourceParity`):**
  1. `RootGrove_Portal` (`BP_Portal_RiftGruntTrial`) was active, with the rift visible on both machines.
  2. `Rpg.Portal.MakeSealable` on the server, then `TryClosePortal`: closed.
  3. After 1 s each machine ran exactly one sequence with all 8 runes. After 4 s both had imploded and hidden the rift.
  4. Calling the presentation with `bInitialState` on the client hid the rift and started no second sequence.
- **Look:** checked in Simulate in the gallery in slow motion and in real time, from the ground and from above, at Epic and at Low, up to 25 s after the close.
- **Asset validation:** the 24 new or changed assets are valid with 0 warnings, the gallery map included. No dependency points into the asset library projects.
- **Cook:** `python Build/Tools/Unreal/ue.py cook Lvl_VfxGallery Lvl_ResourceParity` passed with 0 errors, again after the last change to the rift. It cooked both systems, the sequence, the demo and the sounds. Its 3 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick.

## Authoring notes

- **Trimmed attributes break CPU update expressions:** with `bTrimAttributes`, a CPU emitter dropped attributes that the spawn script writes and only an HLSL expression input of the update script reads. The scorch decals showed at once, because `Delay` and `DecalFade` were missing. The rune colour fell back to the default white, or the rune vanished, depending on the expression. `fx.DumpParticleData 1` for one frame shows which attributes a CPU emitter really has. Both portal systems turn trimming off. GPU emitters were not affected.
- **Build with PIE stopped:** rebuilding a system through the toolset while Simulate runs left it half applied, and saving failed. End PIE first.
- **Additive brightness:** with the exposure-compensated `M_FX_Additive` in the daylight gallery, a particle colour around 0.5–4 reads orange and above about 8 reads yellow-white. Below about 0.25 it is barely visible.
- **One-sided sprites:** a sprite with a custom facing vector is culled from behind. The void emits pairs facing ±X.
- **Blueprint graph DSL:**
  - `Utilities|IsValid` resolves to the exec macro. Write it with both continuations, `(Utilities|IsValid x (:"Is Valid" ...) (:"Is Not Valid"))`.
  - `FadeOut` resolves to the synth component's function. Create the node with `AudioComponent` as declaring class and wire it by hand.
  - A native event that is shaped like an event goes into the EventGraph as `(event Category|EventName (Param) ...)`, not as a function graph.
- **Ticking:** setting the tick through Python stopped the actor from ticking. The sequence uses a looping timer instead.
- **`DeactivateImmediate`** is not callable from Blueprint in 5.8. The sequence hides the rift with `SetVisibility` behind the implosion flash and then deactivates it.
- **Listen-server PIE in one process:** besides `PlayNetMode` and `PlayNumberOfClients`, set `RunUnderOneProcess`. Otherwise the client runs as its own process and Python sees only the server world.

## Playtest

1. **Gallery:** open `/GF_Dev_Sandbox/Maps/Lvl_VfxGallery`, press Play and walk to "Portal Close (VFX-04)", ahead and to the right of the start. Three seconds after the start the rift destabilises, eight runes brand into the ground one by one with rising clicks, a gate slams, the rift collapses and implodes with a boom and a rumble. Embers keep floating over the scar for about half a minute. The rift reopens 24 s later and closes again every 30 s.
2. **Real portal, two players:**
   1. Set Play to 2 players, *Play As Listen Server*, and open `/GF_Dev_Sandbox/Maps/Lvl_ResourceParity`.
   2. Walk to the rift at the root grove (x −2300, y −1200).
   3. On the host, open the console and run `Rpg.Portal.MakeSealable`.
   4. Seal the portal with the interaction prompt, as the host or as the client.
   5. Expect in both windows: the runes, the implosion and the floating embers; the portal is closed for both.
3. **Late join:** close the portal, then connect another client. It sees the closed portal without the rift and without a sequence. The rune marks are local to the machines that saw the close, so the late joiner sees no scorch either.

## Limits

- **Sounds unheard:** the sounds were chosen by name, pack and length and checked only for playback, not by ear. The project owner has not listened to them yet.
- **Real seal not played with input:** the real portal was closed in a two-player PIE through `TryClosePortal` on the server, not through the seal interaction with player input. The seal ability itself is unchanged.
- **Late joiners see no marks:** the scorch, the cooling runes and the embers are presentation. A machine that joins after the close shows the end state only: no rift, no marks.
- **Other portals:** only `BP_Portal_RiftGruntTrial` uses the rift and the sequence. Other portal Blueprints keep their placeholder mesh until they set up `RiftFX` and the event the same way.
- **Low quality:** the rift's `FXT_Gameplay_Looping` still culls it beyond 30 m on Low and under its instance budget. The close itself is protected by its override.
- **Flat ground:** the rune ring and the scar are placed on a plane at the portal's root. On uneven ground the rune sprites can float or sink; the decals follow the surface.
- **Look:** judged from editor captures in the daylight gallery; the embers and runes read stronger in the dark.
