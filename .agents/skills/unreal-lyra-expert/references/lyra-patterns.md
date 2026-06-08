# Lyra-Derived Project Patterns

Use this reference when the task touches this project's Lyra-derived architecture.

## Intent

This project uses Lyra as a proven architectural baseline, but it is not a plain LyraStarterGame clone.

Adopted Lyra systems should be treated as real project architecture.
Do not replace project-specific RPG adaptations with unmodified Lyra sample behavior unless explicitly requested.
Do not expand the architecture toward unrelated Lyra subsystems merely because they exist in the sample project.

Prefer the project's established patterns over ad hoc code.
When a Lyra-derived system is already present, extend that system instead of creating a parallel manager, pawn-owned state path, or widget-owned gameplay truth.

---

## Current Architectural Defaults

These systems are already part of the project and should be preferred by default.

### Lifecycle / Init State

Prefer staged initialization and explicit lifecycle progression over fragile startup ordering and hidden side effects.

Use modular actor/component lifecycle coordination when features depend on each other during initialization.
Do not bypass the existing init-state flow with unrelated `BeginPlay` assumptions unless the feature is truly local and safe.

### Experiences

Treat Lyra-style Experiences as the project's composition root.

Prefer Experience-driven setup for:
- map or mode specific game rules
- pawn configuration
- ability and action set grants
- input setup that belongs to a mode or pawn setup
- Game Feature activation
- data-driven feature composition

Do not avoid Experiences just because they are a Lyra concept; in this project they are an adopted architecture path.
Only keep a feature outside Experiences when it is genuinely local, always-on, editor-only, or unrelated to mode / pawn / feature composition.

### Game Features

Treat Game Feature plugins as an intended feature and content boundary.

Prefer Game Features for modular gameplay slices, optional content, feature activation, and data-driven registration when the codebase already places that concern there.
Respect plugin boundaries and keep dependencies as narrow as possible.
Do not move feature-specific logic into the core game module merely for convenience when it belongs in an existing Game Feature.

### ASC / GAS

Prefer GAS for gameplay capability and gameplay state when the feature naturally belongs there.

Use abilities, effects, attributes, and gameplay tags instead of custom gameplay state managers or duplicated booleans.
Keep cooldowns, costs, blockers, combat state, equipment-granted abilities, and ability-driven interaction gates in GAS when that domain is already GAS-owned.

### Input

Prefer asset-driven input setup and mapping-context based organization over hard-coded bindings.

Keep gameplay input out of widgets unless the behavior is truly UI-local.
When input is granted by pawn setup, Experience, or Game Feature activation, preserve that flow.

### PawnData

Prefer PawnData-driven pawn composition when configuring pawn class, input-related setup, granted gameplay data, or pawn-specific defaults.

Do not create duplicate pawn setup paths when PawnData / Experience / AbilitySet setup already covers the problem.

### Health / Attribute Ownership

Prefer HealthSet-style attribute ownership for health-like gameplay state.

Do not scatter health, damage, resource, or combat-state logic across pawn, controller, UI, and helper objects.
UI should observe gameplay state, not own it.

### Interaction

Treat the Lyra-style Interaction system as intentionally adopted approximately 1:1.

For pickups, containers, crafting stations, harvest nodes, doors, NPCs, loot objects, vendors, world objects, and similar RPG / survival interactions, reuse the existing interaction framework rather than adding one-off traces, prompts, or widget-driven logic.

Bias toward:
- interactable targets or interfaces
- reusable focus / prompt / trigger flow
- gameplay-driven execution
- tag-aware, ability-aware, or item-aware interaction gating
- interaction options that can be extended for RPG requirements

Adapt the interaction layer for project-specific RPG semantics, but preserve the adopted Lyra interaction ownership and flow unless there is a concrete bug.

### Inventory and Equipment

Treat Lyra inventory/equipment as the root architecture, not merely an inspiration.

The project has adapted this root for RPG systems. Preserve the root lifecycle and ownership model while extending the RPG layer.

Prefer:
- item definitions and fragments for static item behavior/data
- runtime item instances for mutable item state
- equipment definitions / equipment instances for equipped behavior
- ability/effect grants from equipment through the established grant path
- gameplay tags for item semantics, restrictions, states, and gates
- compact replicated state, FastArray-style replication where appropriate, and intentional `OnRep` side effects
- server-authoritative inventory/equipment mutations
- save/persistence data that reconstructs runtime item state without making UI authoritative

RPG adaptations may include:
- rarity
- item level
- affixes
- sockets
- durability
- class or stat requirements
- generated loot rolls
- crafting data
- vendor/economy data
- stat aggregation
- progression scaling
- persistence and save/load mapping

Do not reintroduce a separate inventory manager, pawn-owned item arrays, or widget-owned equipment truth when the Lyra-rooted architecture can be extended.
Do not blindly revert RPG-adapted item/equipment code back to plain Lyra sample behavior.

### CommonUI

Prefer CommonUI-style activatable screens, layered navigation, and framework-owned focus/input routing instead of ad hoc widget stacks.

UI should reflect gameplay state, not become the authoritative owner of gameplay state.
For inventory/equipment UI, prefer view models, presenters, or focused widget APIs that read from the authoritative gameplay path.

---

## Do Not Import Automatically

Do not introduce unrelated Lyra subsystems only because they exist in the sample project.

Still require evidence or explicit user request before adding:

- Lyra subsystems not already present in this project
- Lyra-specific spawning/possession flow changes outside the adopted Experience/PawnData path
- Shooter-specific Lyra assumptions that do not fit the RPG project
- Lyra sample UI flows that conflict with the project's RPG UI requirements
- Lyra sample inventory/equipment behavior that would remove the project's RPG item model
- new framework layers, singleton managers, or plugin boundaries not aligned with current project structure

---

## Decision Order

When reviewing or generating code, ask:

1. Can this remain a small local Unreal feature?
2. Should this integrate into the existing lifecycle / init-state flow?
3. Should this be configured or activated through an Experience?
4. Does this belong in an existing or new Game Feature plugin?
5. Is this a GAS concern?
6. Should this state live in attributes, effects, or gameplay tags?
7. Should this be configured through PawnData or AbilitySets?
8. If world-interaction related, should this use the adopted Lyra Interaction path?
9. If item/equipment related, should this extend the Lyra-rooted RPG inventory/equipment architecture?
10. If UI-related, should this move toward CommonUI-style flow while keeping gameplay truth outside widgets?

If none of these apply, use normal Unreal patterns and keep the solution simple.

---

## Prefer

- explicit lifecycle progression
- modular startup coordination
- Experience-driven composition for mode / pawn / feature setup
- Game Feature boundaries for modular gameplay and content slices
- GAS for gameplay capability and gameplay state
- gameplay tags over duplicate booleans
- attribute-based ownership for health-like state
- PawnData-driven pawn composition
- AbilitySets and data-driven grants where already established
- the adopted Lyra Interaction path for repeated world interactions
- Lyra-rooted, RPG-adapted inventory/equipment architecture
- fragment-oriented item design
- compact structured runtime item state
- server-authoritative inventory/equipment mutation
- CommonUI-style screen flow when UI grows

---

## Avoid

- bypassing Experiences for feature setup that belongs in the composition root
- bypassing Game Features for modular gameplay slices that already live there
- adding one-off interaction traces when the adopted Interaction path fits
- creating parallel inventory/equipment systems beside the Lyra-rooted RPG path
- reverting RPG-adapted inventory/equipment to plain Lyra sample behavior without user approval
- singleton managers for gameplay state
- custom state models that duplicate GAS or gameplay tags
- health logic spread across multiple unrelated classes
- widget-owned gameplay truth
- hard-coded startup order dependencies
- monolithic item classes when fragment composition would be cleaner
- replicated convenience variables instead of authoritative reconstructed state
- importing Lyra terminology when the underlying system is not actually present

---

## Anti-Patterns

Flag these aggressively:

### 1. Adopted Lyra System Bypass

Do not bypass an adopted Lyra-derived system with a new local framework.

Examples:
- adding a custom mode composition manager instead of using Experiences
- adding ad hoc plugin activation instead of Game Features
- adding one-off interaction traces instead of the adopted Interaction system
- adding pawn-owned inventory arrays instead of the Lyra-rooted inventory/equipment path

### 2. Full-Lyra Recommendation Drift

Do not recommend a subsystem only because Lyra has it.

Correct distinction:
- Experiences, Game Features, Interaction, and inventory/equipment root are adopted here.
- Other Lyra subsystems still need project evidence or explicit user request.

### 3. Lifecycle Bypass

Avoid fragile startup logic based on implicit ordering, BeginPlay chains, or manual cross-object assumptions.

Prefer explicit lifecycle coordination, Experience-driven setup, and existing init-state flow.

### 4. GAS Duplication

Avoid parallel cooldown systems, custom combat-state managers, or boolean state that duplicates tags, effects, or attributes.

### 5. Scattered Health Ownership

Avoid splitting health and damage logic across pawn, UI, controller, and helper managers.

### 6. Inventory / Equipment Forking

Avoid creating a second inventory or equipment architecture beside the Lyra-rooted RPG path.

Prefer extending existing item definitions, fragments, runtime item instances, equipment instances, ability/effect grants, tags, and replicated item state.

### 7. Monolithic Item Design

Avoid giant item classes that own every possible behavior.

Prefer fragment-oriented composition when the item model becomes modular.

### 8. Widget-Driven Gameplay

Avoid widgets owning authoritative gameplay state or directly coordinating gameplay systems that should live elsewhere.

### 9. Repeated One-Off Interaction Logic

Avoid re-implementing "trace, detect, prompt, execute" separately for every interactable type.

Prefer the adopted Lyra Interaction pattern when the game has contextual world interactions.

---

## Review Heuristics

Prefer comments like:

- "This should integrate with the existing lifecycle / init-state flow."
- "This looks like Experience-driven setup rather than local setup."
- "This feature belongs in the existing Game Feature boundary."
- "This state looks like a better fit for GAS or gameplay tags."
- "Health ownership should stay in the attribute-based path."
- "This should probably be configured through PawnData or AbilitySets."
- "This interaction should use the adopted Lyra Interaction flow instead of a one-off trace."
- "This item logic should extend the Lyra-rooted RPG inventory/equipment path."
- "This UI flow should move toward CommonUI-style activatable screens while keeping gameplay state authoritative outside widgets."
- "Do not introduce unrelated Lyra systems here unless the project adopts them explicitly."

Avoid comments like:

- "Avoid Experiences because they are Lyra-specific."
- "Avoid Game Features by default."
- "Build a custom interaction trace for this even though the adopted Interaction path fits."
- "Create a separate RPG inventory manager beside the Lyra-rooted inventory."
- "Use Lyra’s sample inventory/equipment behavior exactly and remove the RPG adaptations."
- "Lyra does X, so do X."

---

## Reference Bias

Use Lyra as the project baseline for:
- lifecycle / init-state thinking
- modular actor extension
- Experiences
- Game Features
- GAS composition
- attribute-set based gameplay state
- PawnData-driven setup
- AbilitySets and data-driven grants
- Lyra-style Interaction
- inventory/equipment root architecture
- fragment-based item design
- CommonUI-style screen flow

Adapt Lyra for:
- RPG itemization
- generated loot
- item stats and affixes
- durability / sockets / rarity
- progression and requirements
- persistence and save/load
- RPG-specific UI and interaction semantics

Do not treat Lyra documentation as a requirement to reproduce unrelated sample architecture.

---

## Strong Default

Reuse the Lyra-derived systems that are intentionally part of this project.

For this project, Experiences, Game Features, Interaction, and Lyra-rooted RPG inventory/equipment are established architecture.

Do not expand toward unrelated Lyra systems unless explicitly requested.
