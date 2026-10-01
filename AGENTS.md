# AGENTS.md

Shared instructions for coding agents (Codex, Claude Code, and others) working in this repository.

## SurvivalRpg repository guidance

This repository is a dark-fantasy survival action RPG derived from Lyra architecture.

Established architecture:
- Lyra-style Experiences are the composition root.
- Game Feature plugins are feature/content boundaries.
- Lyra Interaction is adopted approximately 1:1.
- Inventory and Equipment use Lyra as the root architecture.
- Inventory and Equipment are adapted for RPG systems.
- GASP is migrated into project-owned content and integrated on a CMC variant and a Mover variant. The previous native GASP port is archived and removed; use `docs/gasp-integration-roadmap.md` for the current accepted state instead of historical migration intentions.

## Agent skills

The canonical skills live in `.agents/skills/<name>/SKILL.md`. Codex discovers them there (invoke with `$name`). Claude Code discovers the generated entry points in `.claude/skills/<name>/SKILL.md` (invoke with `/name`); each one points back to the canonical file.

- Edit only the canonical files under `.agents/skills/`. After adding, renaming, or changing the frontmatter of a skill, run `python Build/Tools/AgentSkills/sync.py`, then `python Build/Tools/AgentSkills/sync.py --check`.
- Write skill text tool-neutrally: refer to other skills by their plain name, not by a tool-specific invocation syntax.

Use the closest matching skill:
- `survival-rpg-project` for game identity, feature scope, first-playable priorities, survival/crafting/progression tradeoffs, portal fantasy, and long-term resource relevance.
- `survival-rpg-combat-foundation` for combat, equipment, loadouts, item instances, ability grants, mastery/progression, runes, portal combat, Dungeonbreak, and combat GameFeature content.
- `unreal-lyra-expert` for Unreal Engine, Lyra-derived architecture, GAS, replication, CommonUI/CommonGame, Enhanced Input, Experiences, Game Features, Lyra Interaction, Unreal MCP asset authoring, and Lyra-rooted RPG inventory/equipment implementation.
- `unreal-gasp-expert` for GASP on the CMC and Mover variants, load-aware RPG movement, sprint/gait profiles, curated mantle/vault/hurdle/climb traversal, Motion Matching, Pose Search, trajectory, procedural animation, retargeting, animation threading, and multiplayer locomotion parity.
- `survival-rpg-orchestrator` for broad or ambiguous multi-system tasks that need routing before implementation.

## Architecture guardrails

- Apply the native-foundation versus designer-owned-content boundary to every new or materially extended system, not only to Lyra-derived systems. This includes crafting, progression, portals, AI, save/load, world events, interaction, building, economy, UI, and editor workflows.
- Before implementing a system, identify its authoritative runtime truth, reusable native schema/mechanism, designer-owned assets and tuning, presentation/read model, editor tooling, and stable test contracts. Use `survival-rpg-orchestrator` for this ownership map when the boundary is not already obvious.
- Prefer extending existing Lyra-derived systems over creating parallel managers.
- Keep UI reflective of gameplay state, not authoritative.
- Keep stable native schemas, engine-facing mechanisms, authority, replication, prediction, persistence, lifecycle safety, and proven hot paths in C++; keep concrete content, tuning, composition, and presentation in Blueprint, Widget Blueprint, or DataAssets.
- Keep `URpgInventoryItemFragment` semantic subclasses native C++ types; do not create Blueprint fragment subclasses. Configure native fragment instances on concrete ItemDefinition assets instead.
- Make concrete GameplayAbilities `GA_*` Blueprint assets by default, directly from `URpgGameplayAbility` or a Blueprint family base. Add an abstract native intermediate ability only for Blueprint-inaccessible APIs, authority/prediction/lifecycle invariants, or known/measured hot paths.
- Build concrete screens, entries, tooltips, toasts, layout, styling, and animation in CommonUI Widget Blueprints backed by reusable native foundations and MVVM.
- Use the configured Unreal MCP workflow (`unreal-mcp` at `http://127.0.0.1:8000/mcp`) to inspect, author, compile, save, and validate Blueprint/DataAsset work. Tooling inconvenience is not a reason to replace designer-owned content with a native leaf class.
- Test reusable native seams and stable asset contracts, not each visual leaf. Do not freeze exact widget-tree names, binding counts, colors, text, animations, or the absence of Blueprint graphs in C++ automation tests.
- Preserve RPG inventory/equipment adaptations.
- Do not revert RPG systems to plain Lyra sample behavior unless explicitly requested.
- Do not introduce unrelated Lyra subsystems only because Lyra has them.
- Do not restore the archived native GASP port. Runtime assets must not depend on the external GASP sample checkout.
- Keep movement gameplay authority in project-owned CMC/Mover, GAS, and Motion-Warping seams. Widening Mover scope or adopting Locomotor, sample camera, Foley, or experimental systems requires explicit roadmap scope and an isolated dependency evaluation; they are never incidental additions.
- Pair `unreal-gasp-expert` with `unreal-lyra-expert` when animation work touches PawnData, Experiences, character lifecycle, movement replication, GAS montages, equipment, death, or ragdoll.
- Add `survival-rpg-combat-foundation` when GASP work touches attacks, dodge, block, hit reactions, combat tags, montage notifies, or equipment-granted combat behavior.

## GASP roadmap and cross-chat handoff

- Standing user preference (26.09.2026): when a roadmap step cannot reasonably be tested manually by the user, perform the appropriate automated validation and review, then push/merge the verified work and continue with the next bounded step without requesting another manual sign-off. Preserve actual failures and remaining limits; this does not authorize bypassing required checks. For readily observable gameplay changes, provide a simple reproduction path.
- Before GASP integration or movement follow-up work, read `docs/gasp-integration-roadmap.md` and `docs/gasp-integration-handoff.md`, then verify the current branch, PR and repository state.
- Use the roadmap's stable task IDs and keep one bounded task per implementation chat/PR. Respect the active task and recorded file ownership; coordinate shared editor/MCP sessions and binary assets across worktrees.
- Update roadmap status and the handoff in the same work PR with actual commits, validation, open findings and the next concrete action. Do not report an open PR as merged or historical test results as newly executed.
- Ignored `Saved/` evidence and generated NetworkPrediction/Mover plugin overrides are not automatically available in another checkout. Follow the roadmap and `Build/Patches/NetworkPrediction/README.md` before building or changing branches.

## Map presentation preference

- For further prototype and test maps, use the approved GASP presentation in `Lvl_RpgGaspMantle` as the starting point: original project-local GASP blocks and floor/grid materials, with the matching LevelVisuals lighting, skylight, fog and exposure.
- Keep lighting and exposure coordinated so surfaces and movement remain readable without overexposure. Reuse this setup for new test environments unless the user requests a different look; it is not the final dark-fantasy art direction.
- The approved setup and asset ownership are documented in `docs/gasp-mantle-integration.md`.

## Documentation defaults

Add concise Unreal-style documentation comments by default when creating or modifying designer-facing or gameplay-facing APIs. This section is the canonical documentation rule; skills add only domain-specific emphasis.

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

## Verification

- Do not claim the project compiles unless the relevant Unreal build was actually run.
- For replicated gameplay, check server authority, replicated state, late join behavior, and OnRep / FastArray behavior.
- For GASP animation changes, check worker-thread safety, simulated-proxy inputs, late join, montage/root-motion compatibility, and project-local asset dependencies.
