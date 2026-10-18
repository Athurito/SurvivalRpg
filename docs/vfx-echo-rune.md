# VFX-02: Echo rune

Task report for VFX-02 in the [VFX roadmap](vfx-roadmap.md). It adds the presentation of the Echo rune from the game vision ("every third attack produces a ghost strike") as a prototype the VFX gallery triggers by hand. No gameplay, rune system or GameplayCue changes.

## Behavior

### The effect

1. **Charge.** Runes engraved on the blade smoulder dull red like heated iron, one more per hit. Faint embers rise off the lit runes.
2. **Flare (third hit).** The runes flare orange-yellow, and the strip sheds:
   - sparks and soot;
   - a short warm flash light (High+);
   - a short heat haze (High+).

   The charge resets and the runes cool down.
3. **Ghost strike, 0.2 s later.** The swing repeats as an ash afterimage:
   - ash puffs are laid down along the blade as it sweeps the same arc again;
   - the ash glows hot at first, then drifts and fades;
   - hot flecks fall away from the arc;
   - heat distortion follows the arc (High+).

### Assets

| Asset | Where | What |
| --- | --- | --- |
| `M_FX_RuneOverlay` | core `Content/SurvivalRpg/FX/Materials` | Reusable weapon rune overlay. See below. |
| `T_FX_RuneGlyphs` | core `FX/Textures/Masks` | Procedural three-rune atlas (R stroke, G halo), `TC_Masks`, 384×128 |
| `MI_Rune_Echo_Overlay` | `GF_Runes_Core/FX` | Echo look and the strip placement for `Sword01` |
| `NS_Rune_Echo_Smoulder` | `GF_Runes_Core/FX` | Looping embers off the rune strip. User parameter `EmberRate` (embers/s, 0 = none), set by the owner from the charge. |
| `NS_Rune_Echo_Flare` | `GF_Runes_Core/FX` | Third-hit burst attached to the weapon. See below. |
| `NS_Rune_Echo_Ghost` | `GF_Runes_Core/FX` | Ghost strike. See below. |
| `BP_VfxEchoRuneDemo` | `GF_Dev_Sandbox/VfxGallery`, dev-only | Gallery demo. See below. |

**`M_FX_RuneOverlay`:** an Unlit Additive overlay material.
- **Placement:** runes are projected in the mesh's local space onto both flat faces of the blade, so they need no UVs. Strip axis, across axis, face axis, start, length and width are parameters.
- **State:** Custom Primitive Data 0 = charge (0–3, glyph *n* lights when charge > *n*), CPD 1 = flare (0–1). The owner calls `SetCustomPrimitiveDataFloat` on the weapon mesh; no dynamic material instances.
- **Colour:** blackbody 1000 K while smouldering, 2800 K at full flare, passed through `MF_FX_ExposureCompensation`.

**`NS_Rune_Echo_Flare`:** a duplicate of the spark template.
- 30 sparks from the rune strip and a 30 cm flash with a 180 cm light (High+).
- 7 soot wisps (lit, cutout, view-depth sorted).
- 3 heat-haze sprites (High+).

**`NS_Rune_Echo_Ghost`:** three emitters spawning over 0.3 s along a 115 cm blade box attached to the moving ghost blade.
- `Ash`: 400/s lit smoke puffs, emissive gain from 6 to 0 over age.
- `Embers`: 120/s.
- `Haze`: 27/s, High+.

**`BP_VfxEchoRuneDemo`:**
- **Swing:** a sword with the overlay swings a 140° slash every 1.5 s (`CycleSeconds`, `SlashSeconds` are instance editable).
- **Hits:** each hit raises the charge; the third spawns the flare attached to the sword with pooling AutoRelease.
- **Ghost:** a `GhostPivot` with absolute rotation sweeps the same arc 0.2 s later while the ghost system is attached to its `GhostBlade`.
- **Gallery:** it stands left of the four templates in `Lvl_VfxGallery`, labelled "Echo rune (VFX-02)". `GF_Dev_Sandbox` now depends on `GF_Runes_Core`.

### Integration intent

When the rune gameplay exists, the owner of the Echo rune sets the overlay CPD and `EmberRate` on the equipped weapon mesh and triggers flare and ghost through GameplayCues, so every client sees them. The ghost sweep needs the weapon's arc: the owner replays recorded weapon transforms or a montage-driven attach point instead of the demo's scripted pivot.

## Performance

- **Draw cost:** the overlay is one extra translucent draw of the weapon mesh. It uses CPD instead of material instances.
- **Smoulder:** CPU, at most about 10 embers alive at charge 3.
- **Flare:** CPU, about 40 particles for under 2 s.
- **Ghost:** CPU, about 160 particles for about 1.3 s. That is above the "about 100" GPU guideline, but short-lived and bursty, so it stays on CPU for now.
- **`ProfileGPU`** in Simulate during flare and ghost, internal 1469×783, Epic quality, frame 7.46 ms:
  - translucency for smoke, soot and ash: under 0.1 ms;
  - distortion: 0.60 ms, of which 0.57 ms is Substrate *Rough Refraction*. The ghost haze and the gallery's haze template were both on screen.

  All distortion emitters are High+ only and short-lived.
- **Effect Type change:** `FXT_Gameplay_Looping` no longer culls after 2 s without rendering. The empty smoulder was culled while charge was 0 and never resumed (`fx.DumpNiagaraScalabilityState` showed "Culled By Visibility").

## Authoring notes

- **Shape offsets:** a ShapeLocation offset defaults to the *Simulation* coordinate space, which is world space for world-space emitters. Set *Offset Coordinate Space* (and *Position Offset Coordinate Space* in Initialize Particle) to *Local* for anything placed relative to the owner.
- **Shape rotation:** rotating a ShapeLocation with Axis Angle and *Scale / Offset / Rotate* did not rotate around the expected axis. Moving the attach point from the owner is simpler and matches the later gameplay setup.
- **Vortex modules:** `VortexVelocity` is deprecated. `VortexForce` applies a constant tangential force, so inner and outer particles sweep different angles and a blade line bends.
- **Pure getters:** Blueprint pure getters evaluate when used. Caching the previous phase needs a member variable (`PrevPhase`); a `bind` of a getter returns the new value after the setter runs.
- **Recreating assets:** deleting a half-created material from Python triggered an engine ensure ("Failed to unload all packages during ForceDeleteObjects") and a modal dialog that blocked MCP. Scripts now refuse to recreate an existing material instead.
- **Contrast:** pale ash disappears against the bright test sky. The ghost ash is mid-grey with a short hot glow.

## Validation

- **Compile:**
  - all three Niagara systems compile with no errors or warnings, after recompiling through the Niagara editor;
  - `M_FX_RuneOverlay` compiles at 291 pixel and 272 vertex instructions;
  - `BP_VfxEchoRuneDemo` compiles with warnings as errors.
- **Asset validation:** 9 changed assets valid, 0 warnings. A dependency scan shows the rune assets reference only core FX, `/GF_Runes_Core`, `/Engine`, `/Niagara` and `/Script`.
- **Visual:** in Simulate on the gallery, at normal speed and at `slomo 0.1`, the captures show:
  - one, two and three runes lighting up;
  - the flare on the blade;
  - soot rising;
  - the ash fan of the ghost strike sweeping the arc from the grip.

  `fx.DumpParticleData` confirmed the ghost ash lies in the swing plane over 12–126 cm from the pivot and spans the arc.
- **Cook:** `python Build/Tools/Unreal/ue.py cook /GF_Dev_Sandbox/Maps/Lvl_VfxGallery` passed with 0 errors. Its 3 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick.

## Limits

- **Visual only:** the ghost reads as a fan of separate ash puffs rather than one continuous smear. The look is judged from editor captures; the user's in-game check is open.
- **Placement:** the overlay strip placement is per weapon mesh. `MI_Rune_Echo_Overlay` holds the `Sword01` values; other weapons need their own placement data.
- **Not wired to gameplay:** no rune gameplay, no GameplayCues and no multiplayer check yet. These belong to the Echo rune gameplay task.
- **Demo assumption:** the ghost sweep assumes the demo actor has yaw 0.
