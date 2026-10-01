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
- GASP is migrated into project-owned content. The previous native GASP port is archived and removed; use `docs/gasp-integration-roadmap.md` for the current accepted state instead of historical migration intentions.

## Agent skills

The canonical skills live in `.agents/skills/<name>/`, where Codex discovers them (`$name`). Claude Code reads generated full copies in `.claude/skills/<name>/` (`/name`).

- Edit only `.agents/skills/`, then run `python Build/Tools/AgentSkills/sync.py`. CI runs it with `--check`, which fails on stale copies, dead repository paths, `/Game` paths, or `URpg*`-style identifiers, Codex-only `$name` syntax in Markdown, and an `AGENTS.md` without a sibling `CLAUDE.md` that imports it.
- Refer to other skills by their plain name in shared Markdown.

Use the closest matching skill and combine only those whose boundaries the task crosses:
- `survival-rpg-project` for game identity, feature scope, first-playable priorities, survival/crafting/progression tradeoffs, portal fantasy, and long-term resource relevance.
- `survival-rpg-combat-foundation` for combat, equipment, loadouts, item instances, ability grants, mastery/progression, runes, portal combat, Dungeonbreak, and combat GameFeature content.
- `unreal-lyra-expert` for Unreal Engine, Lyra-derived architecture, GAS, replication, CommonUI/CommonGame, Enhanced Input, Experiences, Game Features, Lyra Interaction, Unreal MCP asset authoring, and Lyra-rooted RPG inventory/equipment implementation.
- `unreal-gasp-expert` for GASP locomotion and traversal, load-aware RPG movement, sprint/gait profiles, Motion Matching, Pose Search, retargeting, animation threading, and multiplayer locomotion parity.
- `survival-rpg-orchestrator` to plan broad multi-system work and to record the C++ boundary decision for new or materially extended systems.

Common combinations: runtime work on a new feature pairs `survival-rpg-project` with `unreal-lyra-expert`; combat or equipment work starts with `survival-rpg-combat-foundation` and adds `unreal-lyra-expert` for engine-facing changes; GASP work adds `unreal-lyra-expert` when it touches PawnData, Experiences, character lifecycle, movement replication, GAS montages, equipment, death, or ragdoll, and `survival-rpg-combat-foundation` for attacks, dodge, block, hit reactions, combat tags, montage notifies, or equipment-granted combat behavior.

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

Add concise Unreal-style doc comments when creating or changing designer- or gameplay-facing APIs: reflected types, public or protected `UFUNCTION`s, `UPROPERTY` fields exposed to Blueprints, DataAssets, config, saves, or replication, and definitions, fragments, ability sets, interaction options, GameFeature configuration, and tuning data. Say what the value controls, its units or valid range, whether it is designer-tuned static data or runtime-mutated, replicated, saved, derived, cosmetic, or UI-read-only, and who has authority. Skip obvious locals, trivial private helpers, and comments that only restate a name. Skills add only domain-specific emphasis to this rule.

## Local environment

Machine-specific locations on the primary workstation. If one is missing, say so and continue without it.

- Unreal Engine 5.8: `D:/Programme/UE_5.8`
- Lyra reference project: `D:/Repos/LyraStarterGame`
- GASP reference project: `D:/Repos/GameAnimationSample`

## Verification

- Build, test and cook through `python Build/Tools/Unreal/ue.py build`, `... test <filter>...` and `... cook <map>...` instead of assembling `Build.bat` or `UnrealEditor-Cmd.exe` calls. The wrapper verifies the NetworkPrediction/Mover overrides first, prints a compact verdict with log and report paths, and exits with 0 passed, 1 failed or 2 setup error. Options and the engine lookup are in `Build/Tools/Unreal/README.md`; unreal-lyra-expert has the short form.
- Do not claim the project compiles unless the relevant Unreal build was actually run.
- For replicated gameplay, check server authority, replicated state, late join behavior, and OnRep / FastArray behavior.
- For GASP animation changes, check worker-thread safety, simulated-proxy inputs, late join, montage/root-motion compatibility, and project-local asset dependencies.
