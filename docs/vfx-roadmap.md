# VFX roadmap

Status, rules and tasks for gameplay visual effects. Each task is one bounded
PR on a `claude/vfx-<nn>-<slug>` or `codex/vfx-<nn>-<slug>` branch. A task
records its assets, validation and limits in its own report and updates only
its own row below.

## Rules

### Look: grounded realism

- Effects look physically plausible. Light comes from believable sources:
  embers, heated metal (blackbody red to orange-white), fire, sparks.
- Magic shows through matter and light: smoke, ash, dust, debris, heat
  distortion, smouldering engravings and burn marks.
- No neon outlines, clean geometric glyphs floating in the air, or solid
  glowing copies of objects. The UI's occult-register look stays in the UI.
- Colour and bloom stay restrained. Short real lights may hit the character
  and the ground.
- Judge the look in the editor (lighting, bloom, motion blur), not in a
  concept sketch.

### Performance

- Every Niagara system references one of the shared Effect Types in
  `Content/SurvivalRpg/FX/EffectTypes`. Do not create another one without
  agreeing on it first.
- Prefer one system with few emitters. One emitter may feed several
  renderers.
- Simulate fewer than about 100 simultaneous particles on the CPU, more on
  the GPU. Light renderers are hero accents only: one or few particles, short
  lifetime, never on high-count emitters, and gated off on Low quality.
- Use fixed bounds and keep them tight.
- Keep translucent overdraw low:
  - small sprites;
  - sprite cutout bounds on flipbook smoke;
  - erosion instead of long fades;
  - no sorting unless blending requires it.
- Flipbooks are at most 2048 px on a side, masks 256 px or smaller, all in
  the `Effects` texture group with matching compression.
- Every emitter needs a Low-quality path: lower spawn counts, or the emitter
  is gated off.
- Gameplay cues that spawn effects use pooled Niagara components (auto
  release for fire-and-forget effects).

### Placement

- **Shared building blocks** live in core under `Content/SurvivalRpg/FX`:
  - master materials and their instances;
  - curated flipbooks and masks;
  - Effect Types.

  GameFeature plugins depend on core, never the other way round.
- **Feature effects** live in the GameFeature that owns the mechanic:
  - systems in `FX/`;
  - cue notifies in `GameplayCues/`, which every feature registers through
    `RpgGameFeatureAction_AddGameplayCuePath`.
- **Library content:**
  - Migrate from the asset library (`D:/Repos/Assets/Assets_VFX`) only what a
    task uses.
  - Rename it to `T_FX_*` and place it in `Content/SurvivalRpg/FX/Textures`.
  - Add the pack to [third-party-notices.md](third-party-notices.md).
- **Effects are presentation only.** Gameplay truth stays in GAS, equipment
  and progression. Effects reach other players through GameplayCues.

## Tasks

| ID | Task | Status | Report / PR |
| --- | --- | --- | --- |
| VFX-01 | Foundation: core FX folder, master materials, curated flipbooks, Effect Types, VFX gallery map | Merged | [vfx-foundation.md](vfx-foundation.md), [PR #258](https://github.com/Athurito/SurvivalRpg/pull/258) |
| VFX-02 | Echo rune: smouldering runes on the blade, flare on the third hit, ash-and-distortion ghost strike (`GF_Runes_Core`), as a prototype the gallery triggers by hand | Merged | [vfx-echo-rune.md](vfx-echo-rune.md), [PR #260](https://github.com/Athurito/SurvivalRpg/pull/260) |
| VFX-03 | Soul harvest: wisps from killed enemies to the player when XP is awarded (`GF_Progression`), with a cue hook in the XP award and a multiplayer check | Merged | [vfx-soul-harvest.md](vfx-soul-harvest.md), [PR #261](https://github.com/Athurito/SurvivalRpg/pull/261) |
| VFX-04 | Portal closing: destabilise, rune ring locks, implosion, scar (`GF_Portals_Core`, `GA_ClosePortal`) | Planned | |
| VFX-05 | Spell prototypes (seal break, rift bolt, ash fetter in `GF_Combat_Magic`) and Rift Sense (`GF_Harvesting_Magic`) | Planned | |
| VFX-06 | Ash Wave: the Death Wave form ignites and chars the felled trees and leaves a burned area (`GF_Harvesting_Magic`), with form cues on harvest abilities and a multiplayer check | In review | [vfx-ash-wave.md](vfx-ash-wave.md) |
| VFX-07 | Rift Grip: a short lightning strike hits each ore vein, which bursts into rock debris, dust and sparks (`GF_Harvesting_Magic`) | Planned | |
