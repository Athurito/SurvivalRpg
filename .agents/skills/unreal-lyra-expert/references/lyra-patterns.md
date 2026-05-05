# Lyra-Inspired Patterns

Use this reference when the project borrows selected architectural ideas from Lyra, but is not a Lyra project and should not be treated as one.

## Intent

This project uses Lyra as a reference for proven architectural ideas, not as a framework to reproduce wholesale.

Adopt the Lyra-inspired systems that are intentionally part of this project.
Do not expand the architecture toward full Lyra unless explicitly requested.

Prefer the project’s established patterns over ad hoc code, but do not force unrelated Lyra subsystems into the project.

---

## Current Architectural Defaults

These patterns are already part of the project and should be preferred by default.

### Lifecycle
Prefer staged initialization and explicit lifecycle progression over fragile startup ordering and hidden side effects.

Use modular actor/component lifecycle coordination when features depend on each other during initialization.

### ASC / GAS
Prefer GAS for gameplay capability and gameplay state when the feature naturally belongs there.

Use abilities, effects, attributes, and gameplay tags instead of custom gameplay state managers or duplicated booleans.

### Input
Prefer asset-driven input setup and mapping-context based organization over hard-coded bindings.

Keep gameplay input out of widgets unless the behavior is truly UI-local.

### PawnData
Prefer PawnData-driven pawn composition when configuring pawn class, input-related setup, granted gameplay data, or pawn-specific defaults.

### Health / Attribute Ownership
Prefer HealthSet-style attribute ownership for health-like gameplay state.

Do not scatter health logic across pawn, controller, UI, and helper objects.

### Partial Fragment-Based Item Design
The project already borrows fragment-style thinking in parts of its item setup.

When item behavior or item data is modular, prefer fragment-oriented composition over large monolithic item classes.

---

## Preferred Direction

These patterns are desired and should be preferred when expanding the project, but should not be assumed to already exist everywhere.

### Inventory
Prefer an inventory model centered around item definitions, fragments, and structured runtime item data.

Bias toward:
- fragment-based item composition
- fast array style replication where appropriate
- gameplay-tag based runtime semantics
- compact replicated item state instead of scattered actor-owned booleans

When item behavior is data-driven, prefer composing it from fragments rather than growing one large item class.

### CommonUI
Prefer CommonUI-style activatable screens, layered navigation, and framework-owned focus/input routing instead of ad hoc widget stacks.

UI should reflect gameplay state, not become the authoritative owner of gameplay state.

### Interaction
Prefer a gameplay-driven interaction model for contextual world interaction.

For pickups, containers, crafting stations, harvest nodes, doors, world objects, and similar RPG / survival interactions, prefer a unified interaction framework over one-off traces and widget-driven logic.

Bias toward:
- interactable targets or interfaces
- reusable focus / prompt / trigger flow
- gameplay-driven execution
- tag-aware or ability-aware interaction gating

Treat Lyra’s interaction approach as a reference direction, not as a requirement to copy exactly.

---

## Do Not Assume

Do not assume the project uses or wants by default:

- Experiences as a composition root
- Game Feature plugins
- full Lyra inventory/equipment architecture
- Lyra interaction implementation 1:1
- Lyra-specific spawning/possession flow
- every Lyra subsystem just because it exists in the sample project

Only recommend a Lyra-derived subsystem when it matches the project’s actual direction.

---

## Decision Order

When reviewing or generating code, ask:

1. Can this remain a small local Unreal feature?
2. Should this integrate into the existing lifecycle / init-state flow?
3. Is this a GAS concern?
4. Should this state live in attributes, effects, or gameplay tags?
5. Should this be configured through PawnData?
6. If item-related, should this move toward fragment-based composition?
7. If UI-related, should this move toward CommonUI-style flow?
8. If world-interaction related, should this move toward a reusable interaction framework?

If none of these apply, use normal Unreal patterns and keep the solution simple.

---

## Prefer

- explicit lifecycle progression
- modular startup coordination
- GAS for gameplay capability and gameplay state
- gameplay tags over duplicate booleans
- attribute-based ownership for health-like state
- PawnData-driven pawn composition
- fragment-oriented item design
- compact structured runtime item state
- CommonUI-style screen flow when UI grows
- reusable interaction patterns for repeated world interactions

---

## Avoid

- introducing major Lyra systems the project does not actually use
- recommending Experiences or Game Features by default
- singleton managers for gameplay state
- custom state models that duplicate GAS or gameplay tags
- health logic spread across multiple unrelated classes
- widget-owned gameplay truth
- hard-coded startup order dependencies
- monolithic item classes when fragment composition would be cleaner
- one-off interaction traces and prompts duplicated across many actors
- importing Lyra terminology when the underlying system is not actually present

---

## Anti-Patterns

Flag these aggressively:

### 1. Full-Lyra Recommendation Drift
Do not recommend a subsystem only because Lyra has it.

Examples:
- "Add an Experience for this"
- "Make this a Game Feature"
- "Use Lyra’s full inventory pipeline"
- "Use Lyra interaction exactly as-is"

### 2. Lifecycle Bypass
Avoid fragile startup logic based on implicit ordering, BeginPlay chains, or manual cross-object assumptions.

Prefer explicit lifecycle coordination.

### 3. GAS Duplication
Avoid parallel cooldown systems, custom combat-state managers, or boolean state that duplicates tags, effects, or attributes.

### 4. Scattered Health Ownership
Avoid splitting health and damage logic across pawn, UI, controller, and helper managers.

### 5. Monolithic Item Design
Avoid giant item classes that own every possible behavior.

Prefer fragment-oriented composition when the item model becomes modular.

### 6. Widget-Driven Gameplay
Avoid widgets owning authoritative gameplay state or directly coordinating gameplay systems that should live elsewhere.

### 7. Repeated One-Off Interaction Logic
Avoid re-implementing "trace, detect, prompt, execute" separately for every interactable type.

Prefer a reusable interaction pattern when the game has many contextual world interactions.

---

## Review Heuristics

Prefer comments like:

- "This should integrate with the existing lifecycle / init-state flow."
- "This state looks like a better fit for GAS or gameplay tags."
- "Health ownership should stay in the attribute-based path."
- "This should probably be configured through PawnData."
- "This item logic would be cleaner as fragment-based composition."
- "This UI flow should move toward CommonUI-style activatable screens."
- "This feature looks like a candidate for a reusable interaction framework."
- "Do not introduce a Lyra subsystem here unless the project adopts it explicitly."

Avoid comments like:

- "Lyra does X, so do X."
- "Add an Experience for this."
- "Make this a Game Feature."
- "Use Lyra’s exact interaction system."
- "Use the full Lyra inventory model."

unless that system is actually present in the project.

---

## Reference Bias

Use Lyra as inspiration for:
- lifecycle / init-state thinking
- modular actor extension
- GAS composition
- attribute-set based gameplay state
- PawnData-driven setup
- fragment-based item design
- CommonUI-style screen flow
- gameplay-driven interaction patterns

Do not treat Lyra documentation as a requirement to reproduce the full sample architecture.

---

## Strong Default

Reuse the Lyra-inspired systems that are intentionally part of this project.

Do not expand the architecture toward full Lyra unless explicitly requested.