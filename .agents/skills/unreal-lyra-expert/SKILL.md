---
name: unreal-lyra-expert
description: Use for Unreal Engine 5 C++/Blueprint review, design, debugging, refactoring, networking, GAS, Enhanced Input, CommonUI, CommonGame, ModularGameplay, Game Features, Lyra Experiences, Lyra Interaction, Unreal MCP asset authoring, and Lyra-rooted RPG inventory/equipment architecture. Inspect the project first, treat adopted Lyra systems as canonical, adapt inventory/equipment to the project's RPG model, and pair with unreal-gasp-expert when GASP locomotion or animation crosses PawnData, Experiences, GAS montages, movement replication, equipment, death, or ragdoll.
---

# Unreal Lyra Expert

## Inspect the project first

The project targets Unreal Engine 5.8 (`SurvivalRpg.uproject`; the GASP roadmap records 5.8.2). Prefer 5.8 APIs and patterns, and verify version-specific claims against the inspected files.

When context is incomplete, inspect in this order:

1. `SurvivalRpg.uproject` and the relevant `.uplugin` files
2. relevant `.Build.cs`
3. affected source files
4. relevant `Config/Default*.ini`
5. Game Feature descriptors, Experience assets, PawnData, AbilitySets, Interaction assets, ItemDefinitions, EquipmentDefinitions, and inventory/equipment runtime classes

Compare against `D:\Repos\LyraStarterGame` when it exists locally; if it is unavailable, say so and continue. Use Lyra as a comparison baseline to detect accidental drift in copied systems, never to overwrite project-specific RPG adaptations.

## Project architecture facts

Treat these as established architecture, not optional direction:

- Lyra-style Experiences are the composition root for game mode, pawn, ability, and action setup.
- Game Feature plugins are the feature and content activation boundary.
- Lyra Interaction is adopted approximately 1:1; reuse it instead of one-off traces or widget-driven interaction logic.
- Inventory and Equipment use Lyra as the root architecture, adapted for this project's RPG systems.
- GASP content is migrated into project-owned assets and integrated on a CMC variant and a Mover variant through dedicated Experiences and PawnData. The accepted state and active tasks live in `docs/gasp-integration-roadmap.md` and `docs/gasp-integration-handoff.md`. The older native GASP port is archived and must not be restored.

Coordinate with `unreal-gasp-expert` instead of duplicating GASP guidance:

- `unreal-gasp-expert` leads Motion Matching, Pose Search, trajectory, Blend Stack, Steering, Offset Root Bone, Foot Placement/IK, retargeting, animation threading, and locomotion parity.
- Use both skills when animation changes affect PawnData, Experiences, Game Features, character class or lifecycle, movement replication, GAS montage execution, equipment sockets, death, or ragdoll.
- Lyra-derived gameplay and composition stay authoritative; GASP owns locomotion presentation and animation-specific selection. Its [GASP-Lyra integration contract](../unreal-gasp-expert/references/gasp-lyra-integration.md) decides ownership when both domains are active.

## Inventory and equipment

- Preserve the Lyra-rooted ownership, grant, activation, replication, and lifecycle patterns unless there is a concrete correctness reason to change them, and extend the RPG adaptation layer rather than reverting to Lyra sample behavior.
- Do not introduce a parallel inventory manager, pawn-owned item arrays, or widget-owned equipment truth.
- Treat stats, rarity, affixes, sockets, durability, item level, class restrictions, loot generation, crafting, persistence, and save/load as project-specific extensions on top of the Lyra-rooted item/equipment architecture.
- Inventory fragments are native schema: keep `URpgInventoryItemFragment` and its semantic subclasses in C++, never as Blueprint subclasses, and configure fragment instances in ItemDefinition assets. `survival-rpg-combat-foundation` holds the detailed fragment and equipment rules.

## Native schema and mechanism versus designer assets

The repository-wide rules are in `.agents/skills/survival-rpg-orchestrator/references/system-ownership-boundaries.md`. For Unreal work:

- Put native-only engine integration, durable authority/replication/prediction/lifecycle invariants, persistence, reusable runtime schemas, and known or measured hot paths in C++.
- Keep concrete content identity, tuning, composition, asset references, tags, costs, cooldowns, montages, cues, text, icons, and presentation in Blueprint or DataAsset assets. Task size, speculative performance, or inconvenient `.uasset` tooling never justify a runtime C++ leaf class.
- Concrete `GA_*` assets inherit directly from `URpgGameplayAbility` or a Blueprint family base such as `GA_MeleeBase`. Add an abstract native intermediate only for Blueprint-inaccessible APIs, authority/prediction/lifecycle invariants, or a known or measured hot path.
- Keep UI WBP/MVVM-first: native subsystems, ViewModels, Slate primitives, lifecycle integration, geometry algorithms, and real hot paths may be C++; screens, entries, toasts, tooltips, layout, styling, and animation live in CommonUI Widget Blueprints. UI reflects gameplay state and never owns item or equipment truth.
- Expose narrow extension points instead of burying authoritative rules in widgets or creating a native class per presentation leaf.

Read and follow the [Unreal MCP asset-authoring workflow](references/unreal-mcp-asset-authoring.md) for any work that creates or changes Blueprint, Widget Blueprint, Gameplay Ability, or DataAsset assets.

## Testing

- Test native algorithms, authority and lifecycle behavior, ViewModel invalidation, routing, focus/input contracts, pooling, and cleanup once at their reusable seam.
- For Blueprint and Widget Blueprint assets, validate compilation, intended parent class or interface, required MVVM source, registry/cook reachability, and genuinely stable references.
- Do not add a C++ automation suite per screen, entry, tooltip, toast, or Blueprint specialization, and do not assert widget-tree structure, cosmetic names, binding counts, colors, text, animation details, or the absence of Blueprint graph functions.
- Do not create a native widget or GameplayAbility leaf merely to make content easier to unit test.

## Documentation of designer-facing APIs

Apply the documentation defaults in `AGENTS.md`. In Unreal C++, place block comments before reflected `UCLASS`, `USTRUCT`, `UENUM`, `UINTERFACE`, `UPROPERTY`, and `UFUNCTION` declarations, and document every non-trivial field exposed with `EditAnywhere`, `EditDefaultsOnly`, `EditInstanceOnly`, `BlueprintReadOnly`, `BlueprintReadWrite`, `Config`, or `SaveGame`. Add metadata such as `ToolTip`, `ClampMin`/`ClampMax`, `UIMin`/`UIMax`, `Units`/`ForceUnits`, `AllowedClasses`, `Categories`, `DisplayName`, or `EditCondition` where it makes the editor safer to use.

## Working style

Code review:
- Lead with concrete defects and risks: replication bugs, lifecycle issues, authority leaks, scalability problems, and architecture mismatches.
- Base conclusions on inspected files and name them; recommend the smallest fix that matches current conventions.
- Check that a change preserves Experience activation, Game Feature ownership, Interaction flow, and the RPG-adapted inventory/equipment root.

Design:
- Recommend one primary approach and, when useful, one fallback; explain ownership, lifecycle, replication, and module placement.
- Prefer integration with Experiences, Game Features, Interaction, GAS, PawnData, AbilitySets, Inventory, and Equipment over new managers or parallel frameworks.
- For RPG item/equipment features, say which part belongs in item definitions, fragments, item instances, equipment instances, ability/effect grants, attributes, gameplay tags, save data, or UI.

Implementation:
- Preserve local naming, file placement, module conventions, and ownership patterns; state important assumptions first.
- Keep code focused on the request; avoid speculative framework expansion.
- Make larger architectural changes only for a correctness bug, a replication or authority risk, a lifecycle or ownership flaw, or a near-term extensibility problem likely to compound quickly.

## Multiplayer correctness by default

Assume server authority unless the feature is obviously local-only. Before recommending gameplay code, identify who owns the actor, component, item, equipment, or interaction target; where the source of truth lives; whether late joiners reconstruct state; whether prediction, reconciliation, or rollback matter; and whether a client behavior is cosmetic or wrongly acting as authority. Verify RPC ownership, replication conditions, relevancy, dormancy, FastArray behavior, and `OnRep` side effects. Prefer event-driven replication and explicit state ownership over polling or convenience booleans, and never trust client-reported outcomes.

## Patterns and heuristics

- Consult [lyra-patterns.md](references/lyra-patterns.md) for the adopted Lyra systems, decision order, and anti-patterns, and [unreal-best-practices.md](references/unreal-best-practices.md) for general implementation and review guidance. Apply only what is relevant.
- Map/mode composition, pawn setup, action sets, ability grants, or feature activation: check the Experience and Game Feature path first.
- Contextual world use (pickups, containers, crafting stations, harvest nodes, doors, vendors, loot objects, RPG interaction gating): check the adopted Lyra Interaction path first.
- Cooldowns, costs, state gating, combat state, or gameplay-triggered status: bias toward GAS where GAS owns the domain.
- Equipment, item ownership, item stats, equipment ability grants, or replicated item state: extend the Lyra-rooted RPG inventory/equipment architecture.
- Behavior repeated across many actors: prefer a component or subsystem-appropriate abstraction over copy-paste.
- Optional, delayed, or large content: prefer soft references and scalable loading over hard reference chains.
- A local bug gets a local fix before any architectural expansion.
- Do not recommend Lyra subsystems the project has not adopted merely because Lyra has them.

Distinguish observed facts from recommendations, name the files, modules, assets, or systems a conclusion rests on, and do not assert class names, plugin usage, or version-specific correctness without evidence.
