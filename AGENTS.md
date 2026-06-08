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
- `UCLASS`, `USTRUCT`, `UENUM`, and important `UINTERFACE` types
- public or protected `UFUNCTION` APIs
- `UPROPERTY` fields exposed to Blueprints, DataAssets, config, or editor tuning
- DataAsset fields, item/equipment definitions, fragments, ability sets, interaction options, and GameFeature-facing configuration
- replicated properties, saved properties, authority-sensitive fields, and fields with non-obvious lifecycle or ownership rules

For Blueprint-configurable fields, comments should explain:
- what the field controls
- expected units, ranges, or gameplay meaning
- whether it is designer-tuned, runtime-mutated, replicated, saved, or derived
- important ownership assumptions, for example server-authoritative, cosmetic-only, or UI-read-only

Prefer useful intent comments over noisy restatements.
Do not add comments for obvious local variables or trivial private helpers unless the behavior is non-obvious.

Verification:
- Do not claim the project compiles unless the relevant Unreal build was actually run.
- For replicated gameplay, check server authority, replicated state, late join behavior, and OnRep / FastArray behavior.