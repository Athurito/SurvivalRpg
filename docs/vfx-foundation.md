# VFX-01: Foundation

Task report for VFX-01 in the [VFX roadmap](vfx-roadmap.md). It adds the shared VFX building blocks in core and a gallery map in the dev sandbox. No gameplay code changes.

## Behavior

### Folder layout

Everything shared lives under `Content/SurvivalRpg/FX`:

| Folder | Content |
| --- | --- |
| `EffectTypes` | `FXT_Gameplay_Burst`, `FXT_Gameplay_Looping` |
| `Materials` | Master materials `M_FX_SmokeAndFire`, `M_FX_Additive`, `M_FX_Distortion` and `MPC_FX_Exposure` |
| `Materials/Functions` | `MF_FX_ExposureCompensation`, `MF_FX_FastSphereNormal` |
| `Materials/Instances` | `MI_FX_SmokePuff`, `MI_FX_SmokeWispy`, `MI_FX_SoftGlow`, `MI_FX_HeatHaze` |
| `Textures` | Smoke flipbooks, masks and utility maps |
| `Curves` | `Atlas_FX_ColorCurves`, `Curve_FX_EmissiveColor` (used by `M_FX_SmokeAndFire`) |
| `Templates` | Four template systems: `NS_FX_Tpl_SparkBurst`, `NS_FX_Tpl_SmokePuff`, `NS_FX_Tpl_EmberDrift`, `NS_FX_Tpl_HeatHaze` |

### Master materials

**`M_FX_SmokeAndFire`** is Epic's `M_SmokeAndFire_Sprites` from Niagara Examples, renamed.
- **Setup:** lit (DefaultLit), Alpha Composite, Volumetric Directional translucency lighting.
- **Textures:** an EOO flipbook and a matching normal atlas.
  - EOO channels: R = emission/temperature, G = occlusion, A = opacity.
  - SubUV comes from the sprite renderer.
- **Emission:** blackbody colour driven by the temperature channel.
- **Dynamic Parameter 0:** Emissive Gain, Temperature Scale, Temperature Exponent, Opacity Exponent. Niagara's *Dynamic Material Parameters* module defaults all four to 0. Always set Opacity Exponent to 1, otherwise sprites render as squares. Set Emissive Gain to 0 for smoke that does not glow.

**`M_FX_Additive`** renders sparks, embers and glows.
- **Setup:** Unlit, Additive.
- **Emissive:** the `Mask` red channel × particle colour × particle alpha × `Emissive Gain`, passed through `MF_FX_ExposureCompensation`.
- **Colour:** comes from Niagara as HDR particle colour, for example (6, 2.4, 0.6) for a hot spark.

**`M_FX_Distortion`** renders heat haze.
- **Setup:** Unlit, Translucent, opacity 0, Refraction Method *2D Offset*.
- **Offset:** the `Distortion Normal` red and green channels, panned upwards and offset by particle random, × `Falloff Mask` × particle alpha × `Distortion Strength`. The offset is a screen-space fraction, default 0.02.
- **Refraction Coverage Mode is *Coverage Ignored*.** The project uses Substrate, and with *Coverage Accounted For* an opacity of 0 cancels the offset completely.

**`MPC_FX_Exposure`** controls how bright emission reads.
- `EmissiveAutoExposureCompensationMix` = 1 makes emission relative to the current exposure, so an ember reads the same in daylight and in a dark dungeon.
- With 0, physically scaled emission disappears under the manual daylight exposure of the test maps.
- `EmissiveExposureCompensation (EV/Stops)` = 0 is a global offset.

### Textures

| Asset | Source | Cooked size | Use |
| --- | --- | --- | --- |
| `T_FX_SmokePuff_EOO`, `T_FX_SmokePuff_N` | Niagara Examples `T_SmokePuff_*_Loop`, 8×8 frames | 1024 px (source 2048) | Lit smoke puffs |
| `T_FX_SmokeWispy_EOO`, `T_FX_SmokeWispy_N` | Niagara Examples `T_Smoke_Wispy*`, 8×8 frames | 1024 px | Thin wisps (used by later tasks) |
| `T_FX_BaseEOO`, `T_FX_BaseNormal` | Niagara Examples utility defaults | 32 px | Material defaults |
| `T_FX_SoftDot` | Generated procedurally (project-owned), grayscale | 128 px | Spark, ember and glow mask; distortion falloff |
| `T_FX_HeatNoise_N` | Generated procedurally (project-owned), tileable sum-of-sines normal map | 256 px | Heat haze offset |

All FX textures use the `Effects` texture group. The flipbooks keep their 2048 px source but cook at most 1024 px (128 px per frame).

### Effect Types

Both types are distance-significance based. Neither culls effects owned by, attached to or instigated by a locally controlled pawn (`bAllowCullingForLocalPlayers = false`). Effects quality tiers:

| Effect Type | Update | Cull reaction | Low | Medium | High, Epic, Cinematic |
| --- | --- | --- | --- | --- | --- |
| `FXT_Gameplay_Burst`: impacts, flares, XP wisps, portal bursts | Spawn only | Deactivate immediately | 25 m, 12 instances (4 per system), spawn ×0.5 | 35 m, 20 (6), ×0.75 | 50 m, 32 (8), ×1 |
| `FXT_Gameplay_Looping`: rune glow, portal states, auras | Medium | Deactivate, resume when back in range | 30 m, 16 (8), ×0.5 | 45 m, 24 (12), ×0.75 | 60 m, 40 (16), ×1 |

Visibility culling:
- **Burst:** pre-culls spawns outside the view frustum.
- **Looping:** culls after 1 s outside the frustum or 2 s without rendering.

### Template systems

All four are CPU emitters with fixed bounds, trimmed attributes and the matching Effect Type. Bursts use `bRequireCurrentFrameData = false` and no owner-velocity inheritance. To start a feature effect, duplicate a template into the owning GameFeature and adjust it.

| System | Effect Type | Emitters | Particles | Notes |
| --- | --- | --- | --- | --- |
| `NS_FX_Tpl_SparkBurst` | Burst | `Sparks`, `Flash` | 24 + 1 | See below |
| `NS_FX_Tpl_SmokePuff` | Burst | `Smoke` | 5 | See below |
| `NS_FX_Tpl_EmberDrift` | Looping | `Embers` | about 24 alive | 14/s, 1.2–2.6 cm, curl noise, buoyancy |
| `NS_FX_Tpl_HeatHaze` | Looping | `Haze`, High+ only | about 6 alive | 5/s distortion sprites, 30–55 cm |

**`NS_FX_Tpl_SparkBurst`:**
- **Sparks:** velocity-aligned additive streaks, 0.8–1.6 × 5–11 cm, 250–700 cm/s, gravity and drag. They shrink and fade over 0.25–0.6 s.
- **Flash:** one 24 cm glow sprite plus a light renderer. The light has a 220 cm radius (`RadiusScale` 2.2 × the default 100 cm), is exposure-relative, does not affect translucency, and renders on High+ only.

**`NS_FX_Tpl_SmokePuff`:** lit flipbook puffs, 45–80 cm, growing ×2.2 over 1.6–2.4 s, with gentle buoyancy.
- **Animation:** SubUV is set up manually (8×8, blended, random start frame).
- **Material parameters:** Emissive Gain 0, Opacity Exponent 1.
- **Overdraw:** the sprite cutout uses `T_FX_SmokePuff_EOO` alpha with eight-vertex bounds and an alpha threshold of 0.05.
- **Sorting:** by view depth, which is cheap at 5 sprites and needed for alpha composite.

### Gallery map

`GF_Dev_Sandbox/Maps/Lvl_VfxGallery` is a copy of `Lvl_RpgGaspMantle`, with the same presentation, Experience and armed spawn.
- **Placement:** four `BP_VfxGallerySpot` actors stand 6 m in front of the player starts, under the outliner folder *VFX Gallery*, each with a name label.
- **`BP_VfxGallerySpot`** (`GF_Dev_Sandbox/VfxGallery`, dev-only) holds one Niagara component. If its instance-editable `RepeatInterval` is above 0, the component resets on a looping timer, so one-shot bursts repeat while you watch.
- **Later effects:** VFX-02 to VFX-05 add their effects here.

## Performance

Measured with `ProfileGPU` in Simulate on the gallery, all four effects active. Editor viewport 2110×1124, TSR internal 1469×783, Epic quality, frame 5.34 ms:

| Pass | GPU |
| --- | --- |
| Translucency, before distortion and after DOF (smoke, sparks, embers) | about 0.06 ms |
| Distortion, total | 0.26 ms |
| of which Substrate *Rough Refraction* | 0.24 ms |

Consequences:
- **Heat haze is High+ only:** the `Haze` emitter's platform set excludes Low and Medium. Distortion costs about 0.25 ms whenever any refraction is on screen, so use it sparingly.
- **Simulation:** all templates are CPU with fewer than 30 live particles. Per-system particle counts were read with `fx.DumpParticleData`. No emitter was culled in the gallery (`fx.DumpNiagaraScalabilityState`).

## Authoring notes

These apply to Niagara work through the Unreal MCP toolset; they are also listed in the MCP reference of `unreal-lyra-expert`.

- **Curves do not recompile:** replacing an input with *Float From Curve* and then setting the curve keys compiles the system with the empty curve, and the key edit alone triggers no recompile. Particles then read 0, so alpha 0 makes them invisible and size 0 hides them. Opening the system in the Niagara editor (`EditorAppToolset.OpenEditorForAsset`) recompiles it. Save afterwards and verify with `fx.DumpParticleData 1` for one frame.
- **Template flag:** `ObjectTools.set_properties` takes `values` as a JSON *string*. `TemplateSpecification` on a Niagara system cannot be set this way, so the templates are not flagged for the *New System* dialog.
- **Material setters:** `MaterialEditingLibrary.set_material_instance_*` returns `False` even on success; read the value back instead. Static switch changes need a recompile of the base material.

## Validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed on this branch (0 errors, 1 engine-internal ChaosMover warning). There are no C++ changes.
- **Compile:** every template system compiled with no errors and no warnings; stack issues were info notes only. Both new master materials compiled. `BP_VfxGallerySpot` compiled with warnings as errors.
- **Visual:** checked in Simulate on the gallery. The captures show:
  - lit smoke puffs;
  - velocity-aligned sparks;
  - drifting embers;
  - heat distortion bending the floor grid and block edges.
- **Particle data:** confirmed alpha and size curves over normalized age after the recompile fix.
- **Asset validation:** `EditorValidatorSubsystem` checked the 28 new assets: 28 valid, 0 warnings. A dependency scan shows the FX folder references only itself, `/Engine`, `/Niagara` and `/Script`.
- **Cook:** `python Build/Tools/Unreal/ue.py cook /GF_Dev_Sandbox/Maps/Lvl_VfxGallery` passed with 0 errors. Its 3 warnings already existed: Steam disabled, no `GameplayCueNotifyPaths`, and the `CUI_RespawnScreen` tick.

## Limits

- **Look:** judged from editor captures only. The user's in-game check of the gallery is still open, and no tuning against final dark-fantasy lighting has been done.
- **Wisps:** `MI_FX_SmokeWispy` has no template yet; VFX-03 uses it.
- **Light flash:** shows only on High+, and its brightness has not been tuned against real combat lighting.
- **Spawning:** effects are presentation only. No GameplayCue spawns these templates yet; pooled spawning from cues arrives with the first feature effect (VFX-02).
- **Licence:** the Niagara Examples pack licence is recorded but unverified; see [third-party notices](third-party-notices.md).
