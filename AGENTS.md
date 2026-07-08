# AGENTS.md

## SurvivalRpg repository guidance

This repository is a dark-fantasy survival action RPG derived from Lyra architecture.

Established architecture:
- Lyra-style Experiences are the composition root.
- Game Feature plugins are feature/content boundaries.
- Lyra Interaction is adopted approximately 1:1.
- Inventory and Equipment use Lyra as the root architecture.
- Inventory and Equipment are adapted for RPG systems.

Use the closest matching skill:
- Use `$survival-rpg-project` for game identity, feature scope, first-playable priorities, survival/crafting/progression tradeoffs, portal fantasy, and long-term resource relevance.
- Use `$survival-rpg-combat-foundation` for combat, equipment, loadouts, item instances, ability grants, mastery/progression, runes, portal combat, Dungeonbreak, and combat GameFeature content.
- Use `$unreal-lyra-expert` for Unreal Engine, Lyra-derived architecture, GAS, replication, CommonUI/CommonGame, Enhanced Input, Experiences, Game Features, Lyra Interaction, and Lyra-rooted RPG inventory/equipment implementation.
- Use `$survival-rpg-orchestrator` for broad or ambiguous multi-system tasks that need routing before implementation.

Architecture guardrails:
- Prefer extending existing Lyra-derived systems over creating parallel managers.
- Keep UI reflective of gameplay state, not authoritative.
- Preserve RPG inventory/equipment adaptations.
- Do not revert RPG systems to plain Lyra sample behavior unless explicitly requested.
- Do not introduce unrelated Lyra subsystems only because Lyra has them.

## Documentation defaults

Codex should add concise Unreal-style documentation comments by default when creating or modifying designer-facing or gameplay-facing APIs.

Document by default:

- UCLASS, USTRUCT, UENUM, and important UINTERFACE types
- public or protected UFUNCTION APIs
- UPROPERTY fields exposed to Blueprints, DataAssets, config, save data, replication, or editor tuning
- DataAsset fields
- item definitions
- equipment definitions
- fragments
- ability sets
- interaction options
- GameFeature-facing configuration
- portal, rune, recipe, crafting, enemy, loot, progression, and combat tuning data
- replicated properties
- saved properties
- authority-sensitive properties
- fields with non-obvious lifecycle, ownership, or runtime mutation rules

For Blueprint-configurable fields, comments should explain:

- what the field controls
- expected units, ranges, or gameplay meaning
- whether it is designer-tuned, runtime-mutated, replicated, saved, derived, or cosmetic-only
- important ownership assumptions such as server-authoritative, owning-client-only, UI-read-only, static definition data, or runtime mutable state

Prefer useful intent comments over noisy restatements.

Do not add comments for obvious local variables or trivial private helpers unless the behavior is non-obvious.

Verification:
- Do not claim the project compiles unless the relevant Unreal build was actually run.
- For replicated gameplay, check server authority, replicated state, late join behavior, and OnRep / FastArray behavior.