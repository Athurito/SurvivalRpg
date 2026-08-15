---
name: unreal-lyra-expert
description: Use for Unreal Engine 5 C++/Blueprint review, design, debugging, refactoring, networking, GAS, Enhanced Input, CommonUI, CommonGame, ModularGameplay, Game Features, Lyra Experiences, Lyra Interaction, and Lyra-rooted RPG inventory/equipment architecture. Inspect the project first; treat Lyra systems that exist in the project as canonical, adapt inventory/equipment to the project's RPG model, and pair with $unreal-gasp-expert when GASP locomotion or animation crosses PawnData, Experiences, GAS montages, movement replication, equipment, death, or ragdoll.
---

# Unreal Lyra Expert

Inspect the project before proposing changes.

- Read the `.uproject`, relevant `.uplugin` files, affected modules, and relevant `Config/` first.
- Assume Unreal Engine 5.8 only until the inspected project files prove otherwise.
- Once the project version is identified, prefer project-compatible APIs, plugin assumptions, and engine patterns over 5.8-only recommendations.
- Identify the Unreal version, enabled plugins, module layout, and whether the project uses Lyra, GAS, CommonUI, Enhanced Input, CommonGame, ModularGameplay, Game Features, Experiences, Lyra Interaction, or Lyra-derived inventory/equipment.
- Infer whether the codebase is vanilla Unreal, Lyra-derived, or only borrowing selected Lyra ideas.
- If the project is not on 5.8, explicitly separate:
  - current-project-compatible guidance
  - 5.8 best-practice guidance
  - migration risk or API differences where relevant

Inspect the LyraStarterGame reference project when available.

- Prefer comparing against `D:\Repos\LyraStarterGame` if it exists locally.
- If it is not available, continue and state that the Lyra reference project could not be inspected.
- Use it as a comparison baseline, not a mandatory architectural dependency.
- When the project intentionally copied a Lyra system, compare against Lyra to detect accidental drift, but do not overwrite project-specific RPG adaptations.

When context is incomplete, inspect in this priority order:

1. `.uproject`
2. relevant `.Build.cs`
3. affected source files
4. relevant `Default*.ini`
5. `.uplugin` files when plugin boundaries or feature ownership matter
6. relevant Game Feature plugin descriptors, Experience assets, PawnData, AbilitySets, Interaction assets, ItemDefinitions, EquipmentDefinitions, and inventory/equipment runtime classes

## Project Architecture Facts

For this project, treat the following as established architecture, not optional future direction:

- Lyra-style Experiences are the composition root for game mode / pawn / ability / action setup.
- Game Feature plugins are an intended feature and content activation boundary.
- Lyra-style Interaction is intentionally adopted approximately 1:1 and should be reused instead of rebuilding one-off interaction traces or widget-driven interaction logic.
- Equipment and Inventory use Lyra as the root architecture, but the implementation is adapted for this project's RPG systems.
- Project-owned GASP CMC locomotion is composed through Lyra PawnData and Experiences; the Epic sample character is not the gameplay composition root.

Coordinate with `$unreal-gasp-expert` instead of duplicating GASP guidance here.

- Let `$unreal-gasp-expert` lead Motion Matching, Pose Search, trajectory, Blend Stack, Steering, Offset Root Bone, Foot Placement/IK, retargeting, animation threading, and locomotion parity work.
- Use both skills when animation changes affect PawnData, Experiences, Game Features, character class or lifecycle, movement replication, GAS montage execution, equipment sockets, death, or ragdoll.
- Keep Lyra-derived gameplay and composition systems authoritative while GASP owns locomotion presentation and animation-specific selection.
- Preserve the project-owned curated CMC path; do not replace it with GASP's sample character, Mover, Traversal, camera, Foley, or experimental state-machine stack without explicit evaluation.
- Let the GASP skill load its [GASP-Lyra integration contract](../unreal-gasp-expert/references/gasp-lyra-integration.md) when both domains are active.

Inventory/equipment guidance:

- Preserve the Lyra-rooted inventory/equipment ownership, grant, activation, replication, and lifecycle patterns unless there is a concrete correctness reason to change them.
- Prefer extending the project's RPG adaptation layer over reverting to unmodified Lyra sample behavior.
- Do not introduce a parallel inventory manager, pawn-owned item arrays, or widget-owned equipment truth when the existing Lyra-rooted path can be extended.
- Treat RPG concepts such as stats, rarity, affixes, sockets, durability, item level, class restrictions, loot generation, crafting, persistence, and save/load as project-specific extensions layered on top of the Lyra-rooted item/equipment architecture.
- Treat `URpgInventoryItemFragment` and every semantic fragment subclass as native C++ schema and runtime behavior. Do not create Blueprint subclasses of inventory fragments.
- Add a native fragment subclass only for new item semantics or runtime behavior. Configure instances of existing native fragment types in ItemDefinition assets for item-specific values and combinations; do not add fragment subclasses for data variants.

Preserve the existing architecture unless there is a strong reason to change it.

- Prefer small, local fixes over cross-cutting rewrites.
- Respect module boundaries, `Build.cs` dependencies, subsystem ownership, asset-driven configuration, Game Feature boundaries, Experience activation, and existing gameplay framework seams.
- Do not introduce patterns that fight the current project structure.
- Make larger architectural recommendations only when there is:
  - a correctness bug
  - a replication or authority risk
  - a lifecycle or ownership flaw
  - a near-term extensibility problem likely to compound quickly


Choose the native schema/mechanic versus design-asset boundary deliberately.

- Put native-only engine integration, durable authority/replication/prediction/lifecycle invariants, persistence, reusable runtime schemas, and known or measured hot paths in C++.
- Do not treat task size, speculative performance, or inconvenient `.uasset` tooling as a reason to create a runtime C++ leaf class.
- Keep concrete content identity, tuning, composition, asset references, tags, costs, cooldowns, montages, cues, text, icons, and presentation in Blueprint or DataAsset assets.
- For Gameplay Abilities, default concrete `GA_*` assets to inheriting directly from `URpgGameplayAbility` or from a shared Blueprint family base such as `GA_MeleeBase`.
- Add an intermediate native ability class such as an abstract `URpgGameplayAbility_MeleeBase` only when it encapsulates Blueprint-inaccessible APIs, authority/prediction/lifecycle invariants, or a known or measured hot path. Keep concrete abilities and design variation in `GA_*` assets below it.
- Expect pure content and design tasks to add zero native classes. Native fragment schemas and technically justified reusable mechanics are explicit exceptions.
- Keep UI WBP/MVVM-first: use native subsystems, ViewModels, Slate primitives, lifecycle integration, geometry algorithms, and real hot paths where appropriate; keep screens, entries, toasts, tooltips, layout, styling, and animation in Widget Blueprints.
- Expose narrow extension points instead of burying authoritative rules in Widgets or creating a native class per presentation leaf.
- Keep UI reflective of gameplay state rather than authoritative over gameplay state. For inventory/equipment, keep UI as a view/controller of replicated gameplay state, not the owner of item truth.

For work that creates or changes Blueprint, Widget Blueprint, Gameplay Ability, or DataAsset assets, read and follow the required [Unreal MCP asset-authoring workflow](references/unreal-mcp-asset-authoring.md).

Test stable native seams and asset contracts instead of concrete presentation implementation.

- Test native algorithms, authority/lifecycle behavior, ViewModel invalidation, routing, focus/input contracts, pooling, and cleanup once at their reusable seam.
- For Blueprint and Widget Blueprint assets, validate compilation, intended parent class or interface, required MVVM source, registry/cook reachability, and genuinely stable references.
- Do not add a dedicated C++ automation suite for every screen, entry, tooltip, toast, or other Blueprint specialization.
- Do not assert exact widget-tree structure, cosmetic widget names, binding counts, colors, display text, animation details, or the absence of Blueprint graph functions.
- Do not create a native widget or GameplayAbility leaf merely to make concrete content easier to unit test.

Document designer-facing Unreal APIs by default.

When creating or modifying C++ that is visible to designers, Blueprints, DataAssets, assets, or config, add concise documentation comments.

Use Unreal-style block comments before reflected declarations:

- UCLASS
- USTRUCT
- UENUM
- UINTERFACE
- UPROPERTY
- UFUNCTION

Document all non-trivial fields exposed with:

- EditAnywhere
- EditDefaultsOnly
- EditInstanceOnly
- BlueprintReadOnly
- BlueprintReadWrite
- Config
- SaveGame

For DataAssets, ItemDefinitions, EquipmentDefinitions, fragments, AbilitySets, Interaction options, GameFeature config assets, and BP-configurable tuning structs, comments should explain designer intent, gameplay meaning, units, valid ranges, ownership, replication/save behavior, and whether the value is runtime-mutated or static definition data.

Prefer comments that help a designer safely configure the asset.
Avoid comments that only repeat the variable name.

When a tooltip needs to be explicit in the editor, also use metadata like ToolTip, ClampMin, ClampMax, UIMin, UIMax, Units, ForceUnits, AllowedClasses, Categories, or DisplayName where appropriate.

Keep comments short, practical, and accurate.


Match the response style to the task.

For code review:
- Lead with concrete defects and risks first.
- Prioritize replication bugs, lifecycle issues, authority leaks, scalability problems, and architecture mismatches.
- Base conclusions on the inspected files and name those files or systems explicitly.
- Recommend the smallest fix that matches the project’s current conventions.
- Check whether a proposed change preserves Experience activation, Game Feature ownership, Interaction flow, and the RPG-adapted inventory/equipment root.

For design tasks:
- Recommend one primary approach and, when useful, one fallback.
- Explain ownership, lifecycle, replication, and module placement.
- Prefer integration with existing Experiences, Game Features, Interaction, GAS, PawnData, AbilitySets, Inventory, and Equipment over creating new managers or parallel frameworks.
- For RPG item/equipment features, explain which part belongs in item definitions, fragments, item instances, equipment instances, ability/effect grants, attributes, gameplay tags, save data, or UI.

For implementation tasks:
- Preserve local naming, file placement, module conventions, and ownership patterns.
- State important assumptions before proposing code.
- Keep code focused on the requested task and avoid speculative framework expansion.
- Do not replace a project-specific RPG extension with plain Lyra sample behavior unless explicitly asked.

Treat multiplayer correctness as a default concern.

- Assume server authority unless the local-only nature of the feature is obvious.
- Before recommending gameplay code, identify:
  - who owns the actor, component, item instance, equipment instance, or interaction target
  - where the source of truth lives
  - whether late joiners reconstruct the state correctly
  - whether prediction, reconciliation, or rollback concerns matter
  - whether a client-side behavior is cosmetic only or incorrectly acting as authority
- Verify ownership for RPCs, replication conditions, relevancy, dormancy, FastArray behavior, and `OnRep` behavior.
- Prefer event-driven replication and explicit state ownership over polling or convenience booleans.
- Do not trust client-reported outcomes for authoritative gameplay results.

Use Lyra conventions according to what the project actually adopted.

- Consult [references/lyra-patterns.md](references/lyra-patterns.md) when the project is Lyra-derived, Lyra-inspired, or using Lyra-style modular gameplay.
- For this project, Experiences, Game Features, Lyra Interaction, and Lyra-rooted inventory/equipment are established architecture.
- Keep features aligned with Experiences, Game Features, PawnData, AbilitySets, Gameplay Tags, data-driven activation, Interaction options, and inventory/equipment definitions when the task touches those areas.
- Reuse existing extension seams before adding new managers, singleton-like systems, or framework layers.
- Do not recommend unrelated Lyra subsystems merely because they exist in Lyra.

Apply Unreal best practices consistently.

- Consult [references/unreal-best-practices.md](references/unreal-best-practices.md) when implementation or review guidance is needed.
- Apply only the parts relevant to the current task instead of treating the reference as a mandatory full checklist.
- Prefer explicit ownership, predictable lifetimes, soft references for optional content, and subsystem-appropriate responsibilities.
- Watch for common Unreal hazards:
  - invalid UObject lifetimes
  - missing reflection macros
  - circular hard references
  - unnecessary ticking
  - heavyweight constructors
  - asset loads on hot paths
  - widget-owned gameplay truth
  - client-trusting gameplay logic

Use stronger decision heuristics when choosing a pattern.

- If a feature needs map/mode composition, pawn setup, action sets, ability grants, or feature activation, check the Experience and Game Feature path first.
- If a feature needs contextual world use, pickups, containers, crafting stations, harvest nodes, doors, NPC vendors, loot objects, or RPG interaction gating, check the adopted Lyra Interaction path first.
- If a feature needs cooldowns, costs, state gating, combat-state ownership, or gameplay-triggered status handling, bias toward GAS when GAS already owns that domain.
- If a feature modifies equipment, item ownership, item stats, ability grants from equipment, or replicated item state, extend the Lyra-rooted RPG inventory/equipment architecture.
- If a feature modifies GASP locomotion or animation presentation and also touches Lyra composition, movement replication, GAS montages, equipment, death, or ragdoll, use `$unreal-gasp-expert` with this skill.
- If behavior repeats across many actors, prefer a component, subsystem-appropriate abstraction, or reusable gameplay pattern over copy-paste logic.
- If content is optional, delayed, or large, prefer soft references and scalable asset-loading patterns over hard reference chains.
- If a problem is only a local bug, solve it locally before proposing architectural expansion.

State assumptions and uncertainty explicitly.

- Distinguish observed facts from recommendations.
- Say which files, modules, assets, or systems the conclusion is based on.
- If project files are incomplete, do not assert class names, plugin usage, architectural intent, or engine-version-specific correctness without evidence.
- If multiple valid approaches exist, recommend one and briefly explain the tradeoff.
- If Lyra offers a better home for the feature, name the Lyra concept only to anchor the recommendation, not to force adoption of unrelated sample architecture.
