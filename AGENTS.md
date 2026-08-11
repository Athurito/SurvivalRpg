# AGENTS.md

## SurvivalRpg repository guidance

This repository is a dark-fantasy survival action RPG derived from Lyra architecture.

Established architecture:
- Lyra-style Experiences are the composition root.
- Game Feature plugins are feature/content boundaries.
- Lyra Interaction is adopted approximately 1:1.
- Inventory and Equipment use Lyra as the root architecture.
- Inventory and Equipment are adapted for RPG systems.
- GASP CMC locomotion is a project-owned, curated animation path integrated through Lyra PawnData and Experiences.

Use the closest matching skill:
- Use `$survival-rpg-project` for game identity, feature scope, first-playable priorities, survival/crafting/progression tradeoffs, portal fantasy, and long-term resource relevance.
- Use `$survival-rpg-combat-foundation` for combat, equipment, loadouts, item instances, ability grants, mastery/progression, runes, portal combat, Dungeonbreak, and combat GameFeature content.
- Use `$unreal-lyra-expert` for Unreal Engine, Lyra-derived architecture, GAS, replication, CommonUI/CommonGame, Enhanced Input, Experiences, Game Features, Lyra Interaction, and Lyra-rooted RPG inventory/equipment implementation.
- Use `$unreal-gasp-expert` for Game Animation Sample Project (GASP), CMC locomotion, load-aware RPG movement, sprint/gait profiles, curated mantle/vault/climb traversal, Motion Matching, Pose Search, trajectory, procedural animation, retargeting, animation threading, and multiplayer locomotion parity.
- Use `$survival-rpg-orchestrator` for broad or ambiguous multi-system tasks that need routing before implementation.

Architecture guardrails:
- Prefer extending existing Lyra-derived systems over creating parallel managers.
- Keep UI reflective of gameplay state, not authoritative.
- Preserve RPG inventory/equipment adaptations.
- Do not revert RPG systems to plain Lyra sample behavior unless explicitly requested.
- Do not introduce unrelated Lyra subsystems only because Lyra has them.
- Treat Epic's GASP project as a comparison source, not a runtime dependency; preserve the project-owned CMC/Pose Search implementation.
- Reuse GASP traversal assets only through project-owned CMC/GAS/Motion-Warping seams; do not import the full GASP Mover/Traversal stack, Locomotor, sample camera, Foley, or experimental state-machine content without an explicit isolated evaluation.
- Pair `$unreal-gasp-expert` with `$unreal-lyra-expert` when animation work touches PawnData, Experiences, character lifecycle, movement replication, GAS montages, equipment, death, or ragdoll.
- Add `$survival-rpg-combat-foundation` when GASP work touches attacks, dodge, block, hit reactions, combat tags, montage notifies, or equipment-granted combat behavior.

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
- For GASP animation changes, check worker-thread safety, simulated-proxy inputs, late join, montage/root-motion compatibility, and project-local asset dependencies.
