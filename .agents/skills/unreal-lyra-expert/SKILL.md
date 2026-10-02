---
name: unreal-lyra-expert
description: Applies SurvivalRpg's Lyra-derived Unreal architecture (Experiences, Game Features, GAS, Lyra Interaction, RPG inventory and equipment, CommonUI with MVVM, replication) and its boundary between C++ and Blueprint, Widget Blueprint, or DataAsset content, including Unreal MCP asset authoring. Use for any plan, implementation, review, or debugging of Unreal C++, Blueprints, Widget Blueprints, UI, GAS, or DataAssets in this project, including asset tuning and build or test questions.
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

Compare against the Lyra reference project listed under "Local environment" in `AGENTS.md` when it exists; if it is unavailable, say so and continue. Use Lyra as a comparison baseline to detect accidental drift in copied systems, never to overwrite project-specific RPG adaptations.

## Project architecture facts

Treat these as established architecture, not optional direction:

- Lyra-style Experiences are the composition root for game mode, pawn, ability, and action setup.
- Game Feature plugins are the feature and content activation boundary.
- Lyra Interaction is adopted approximately 1:1; reuse it instead of one-off traces or widget-driven interaction logic.
- Inventory and Equipment use Lyra as the root architecture, adapted for this project's RPG systems.
- GASP content is migrated into project-owned assets and composed through Experiences and PawnData. The accepted integration state and active tasks live in `docs/gasp-integration-roadmap.md` and `docs/gasp-integration-handoff.md`. The older native GASP port is archived and must not be restored.

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
- Keep UI WBP/MVVM-first: native subsystems, ViewModels, Slate primitives, lifecycle integration, geometry algorithms, and real hot paths may be C++; screens, entries, toasts, tooltips, layout, styling, and animation live in CommonUI Widget Blueprints. UI reflects gameplay state and never owns item or equipment truth. When a native widget already renders a fallback layout, add new rows and styling in its Widget Blueprint child and limit native changes to exposing read-only data through the ViewModels.
- Expose narrow extension points instead of burying authoritative rules in widgets or creating a native class per presentation leaf.

Read and follow the [Unreal MCP asset-authoring workflow](references/unreal-mcp-asset-authoring.md) for any work that creates or changes Blueprint, Widget Blueprint, Gameplay Ability, or DataAsset assets.

## Testing

- Test native algorithms, authority and lifecycle behavior, ViewModel invalidation, routing, focus/input contracts, pooling, and cleanup once at their reusable seam.
- For Blueprint and Widget Blueprint assets, validate compilation, intended parent class or interface, required MVVM source, registry/cook reachability, and genuinely stable references.
- Do not add a C++ automation suite per screen, entry, tooltip, toast, or Blueprint specialization, and do not assert widget-tree structure, cosmetic names, binding counts, colors, text, animation details, or the absence of Blueprint graph functions.
- Do not create a native widget or GameplayAbility leaf merely to make content easier to unit test.

## Build, test, and cook

Run these from the repository root instead of assembling `Build.bat` or `UnrealEditor-Cmd.exe` calls:

```powershell
python Build/Tools/Unreal/ue.py build
python Build/Tools/Unreal/ue.py test SurvivalRpg.Combat.Block.Lifecycle --null-rhi
python Build/Tools/Unreal/ue.py test SurvivalRpg.GASP.Mover.Mantle --exec "np.ForceReconcile 0" --exec "t.MaxFPS 30"
python Build/Tools/Unreal/ue.py cook Lvl_RpgGaspMantle
```

- Exit code 0 means passed, 1 failed, and 2 a setup error such as a missing engine or unverified NetworkPrediction/Mover overrides. Act on the printed errors or failed tests; open the named log or report only for details.
- Build after C++ or `.Build.cs` changes and before testing them. An open editor with Live Coding blocks the build, so close it first.
- PIE network tests need rendering, so use `--null-rhi` only for native and asset tests. Add `--expect-tests N` when a filter must select exactly N tests.
- `Build/Tools/Unreal/README.md` lists the remaining options and the engine lookup.

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

- Read [project-patterns.md](references/project-patterns.md) for the adopted systems and how to extend them, the decision order, anti-patterns, and verification by area.
- Prefer a component or subsystem-appropriate abstraction over behavior copied across actors, and soft references with scalable loading for optional, delayed, or large content.
- A local bug gets a local fix before any architectural expansion.

Distinguish observed facts from recommendations, name the files, modules, assets, or systems a conclusion rests on, and do not assert class names, plugin usage, or version-specific correctness without evidence.
