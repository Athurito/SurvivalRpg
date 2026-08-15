# Repository-Wide System Ownership Boundaries

Use this reference for every new or materially extended gameplay, UI, editor, persistence, or content-pipeline system. Lyra patterns are strong implementation examples, but these ownership rules do not depend on Lyra.

## Ownership layers

### Native runtime foundation

Keep responsibilities in C++ when they define reusable runtime semantics or require native enforcement:

- authoritative state mutation, replication, prediction, persistence, or save reconstruction
- lifecycle, thread-safety, memory, engine integration, or Blueprint-inaccessible APIs
- semantic schema types whose behavior assets configure, such as inventory-fragment types
- known engine hot paths or profiling-confirmed performance work
- reusable components, subsystems, tasks, algorithms, or narrow framework integration seams

Do not use feature size, speculative performance, easier unit testing, graph aesthetics, or inconvenient asset tooling as native-class justification.

### Designer-owned assets

Keep concrete content identity and variation in Blueprint, Widget Blueprint, DataAssets, definitions, tables, tags, effects, cues, behavior assets, or other editor-authored assets:

- named abilities, items, recipes, enemies, portals, encounters, progression nodes, rewards, and world-event variants
- numeric tuning, costs, cooldowns, ranges, durations, tags, asset references, text, icons, audio, animation, and styling
- composition of existing native schemas and mechanisms into a concrete feature
- shared design flow, which may use an asset parent without requiring a native intermediate

Create a new native schema type only for new semantics or runtime behavior. Configure a new content instance when existing schema already expresses the difference.

### Presentation and read models

Keep gameplay truth outside UI. Use native ViewModels, presenters, subsystems, Slate primitives, lifecycle/focus seams, or reusable geometry only when their responsibility is genuinely native. Keep concrete screens, rows, prompts, toasts, layout, formatting, styling, and animation in Widget Blueprints/MVVM.

### Editor tooling

Use Unreal MCP to inspect, author, compile, save, and validate Blueprint/DataAsset content. A tooling limitation never moves designer-owned content into a runtime class. After a confirmed capability gap, add only the smallest reusable editor-only seam.

## New-system workflow

1. Inspect current repository systems and extension seams before proposing a manager or framework.
2. Name the authoritative owner, lifecycle, replication/persistence path, and consumers.
3. Separate native schema/mechanism from concrete content, presentation, and tooling.
4. Count every proposed native class and state the technical responsibility that requires it.
5. Start with the smallest seam required by the current use case; do not build speculative frameworks for hypothetical consumers.
6. Author designer-owned assets through Unreal MCP and validate their stable contracts.
7. Test native invariants once, asset integration through stable contracts, and visual quality in editor/PIE rather than freezing presentation internals.

## Examples

- **Crafting:** keep authoritative transactions, inventory mutation, and reusable validation native; keep recipes, station variants, costs, timings, text, and layout in assets/WBP.
- **Progression:** keep replicated/saved progression state and reusable evaluation native; keep unlock graphs, rewards, thresholds, presentation, and balance in assets.
- **Portals and world events:** keep lifecycle, authority, replication, and reusable scheduling native; keep encounter variants, rewards, visuals, cues, and tuning in feature assets.
- **AI:** keep authority-sensitive or profiling-confirmed algorithms native; keep concrete behavior composition, queries, archetypes, perception tuning, and presentation in Behavior Tree/StateTree/DataAssets where suitable.
- **Inventory:** keep semantic fragment subclasses native; configure fragment instances on concrete ItemDefinition assets without Blueprint fragment subclasses.
- **GAS:** keep only required native mechanisms below concrete `GA_*` assets; shared design flow may remain a Blueprint parent.
