# Unreal Best Practices

Use this reference for implementation and review work in Unreal Engine projects.

## Version Policy

- Assume Unreal Engine 5.7 by default.
- Prefer Unreal Engine 5.7 APIs, idioms, and plugin-era architecture unless inspected project files show a different engine target.
- If the project appears to be on an older version, call out the mismatch explicitly and separate "5.7 best practice" from "compatible fallback for this codebase".

## Architecture

- Prefer extending the existing gameplay framework over creating parallel systems.
- Keep responsibilities clear across `GameMode`, `GameState`, `PlayerController`, `PlayerState`, `Pawn`, and subsystems.
- Favor data-driven setup with assets, config, and tags over hard-coded branching.
- Add dependencies to the smallest reasonable module instead of widening global coupling.
- Respect existing plugin, Game Feature, Experience, and asset-registration boundaries.

## C++ and Blueprint Boundary

- Implement authoritative rules, replication, persistence, and high-frequency logic in C++.
- Use Blueprints for asset wiring, moment-to-moment tuning, animation glue, UI composition, and designer-facing hooks.
- Expose focused `BlueprintCallable`, `BlueprintPure`, or `BlueprintImplementableEvent` APIs instead of broad utility surfaces.
- Avoid giant all-purpose Blueprint base classes.

## Documentation and Designer-Facing APIs

Add concise documentation comments by default for reflected Unreal APIs that designers, Blueprint authors, or future gameplay programmers will touch.

Document by default:

- UCLASS, USTRUCT, UENUM, and important UINTERFACE types
- public or protected UFUNCTION APIs
- Blueprint-callable, Blueprint-pure, Blueprint-native, or Blueprint-implementable functions
- UPROPERTY fields exposed to Blueprints, DataAssets, config, save data, replication, or editor tuning
- DataAsset fields
- item definitions
- equipment definitions
- item fragments
- equipment fragments
- ability sets
- interaction options
- recipe data
- rune data
- portal encounter data
- enemy data
- progression unlock data
- gameplay tag fields
- asset references
- class references
- replicated fields
- saved fields
- authority-sensitive fields

For designer-facing UPROPERTY fields, comments should explain:

- what the value controls
- expected unit, range, or scale
- whether the value is static asset data or runtime state
- whether it is designer-tuned, runtime-mutated, replicated, saved, cosmetic, or UI-only
- whether server authority owns the value
- whether the field is safe to change in child Blueprints or DataAssets
- whether the field expects a hard reference, soft reference, gameplay tag, class reference, or DataAsset reference

Prefer comments that explain intent, constraints, ownership, or designer impact.

Avoid noisy comments on obvious local variables, simple private helpers, boilerplate code, or comments that only repeat the property name.

When useful for editor clarity, also use Unreal metadata such as ToolTip, ClampMin, ClampMax, UIMin, UIMax, ForceUnits, Units, AllowedClasses, Categories, DisplayName, EditCondition, or EditConditionHides.

Keep comments short, practical, and accurate.

## UObject and Lifetime Safety

- Store UObject references with `UPROPERTY` when lifetime tracking matters.
- Be careful with raw pointers, async callbacks, timers, and delegates that can outlive the target object.
- Favor subsystem or component ownership over ad-hoc global caches.
- Do not perform expensive work or asset loading in constructors unless the object truly requires it at creation time.

## Actor and Component Patterns

- Use components for reusable behavior that belongs to an Actor's lifecycle.
- Avoid ticking by default; prefer events, timers, gameplay tasks, or ability tasks.
- Guard tick usage with explicit need, sensible tick groups, and early-outs.
- Keep replicated state as small and intentional as possible.

## Assets and References

- Prefer soft object references for optional, delayed, or large content.
- Use primary assets, asset manager patterns, or project-specific loading flows for scalable content.
- Avoid hard-reference chains that pull large content sets into memory unintentionally.

## Networking

- Start by deciding who owns the truth: server, owning client, or everyone.
- Validate RPC direction and ownership before writing gameplay code.
- Replicate state, not transient convenience variables, when the state must survive joins or prediction corrections.
- Use `OnRep` for side effects that belong to replicated state changes.
- Do not trust client-reported results for authoritative gameplay outcomes.
- For replicated arrays or item lists, prefer intentional FastArray-style replication when appropriate and verify add/remove/change notifications.

## GAS-Oriented Guidance

- Prefer abilities, gameplay effects, attributes, and tags over scattered custom booleans when GAS already owns the domain.
- Keep the Ability System Component as the home for combat-relevant state.
- Model cooldowns, costs, blockers, and state gates with GAS primitives when possible.
- Avoid duplicating authoritative gameplay state both inside and outside GAS without a clear reason.

## Inventory / Equipment Guidance

- Keep authoritative item and equipment mutation on the server.
- Keep UI reflective of inventory/equipment state, not authoritative over it.
- Prefer item definitions/fragments for static data and runtime item/equipment instances for mutable state.
- Keep replicated item state compact and reconstruct derived state locally where safe.
- Use gameplay tags, attributes, effects, and abilities for equipment-granted gameplay semantics where those systems already own the domain.
- Treat save data as a reconstruction source for runtime item state, not as an always-live gameplay authority.

## Performance and Maintainability

- Minimize work in hot paths such as tick, animation update, and replicated callbacks.
- Cache only when profiling or repeated traversal justifies it.
- Keep APIs narrow and intention-revealing.
- Favor predictable naming and folder/module placement that match Unreal conventions.

## Verification

When modifying code:

- Identify the smallest relevant build target or editor validation path.
- Run or recommend the project’s established build/test command when available.
- Do not claim code compiles unless a build was actually run.
- For replicated gameplay, verify authority path, replicated state, `OnRep` side effects, late join behavior, and prediction assumptions.
- For inventory/equipment, verify server mutation, replicated item state, equipment grant lifecycle, ability/effect removal, persistence reconstruction, and UI refresh behavior.
- For Interaction, verify authority handoff, prompt ownership, target lifetime safety, and gameplay-tag/ability gating.
- For Game Features and Experiences, verify activation/deactivation, plugin dependencies, data asset registration, and load/cook implications.
- For Blueprint-facing APIs, verify reflection macros, categories, metadata, and null-safety.
- For asset-driven systems, verify soft references, Primary Asset usage, config registration, and cook/load implications.

## Review Checklist

- Verify reflection macros, includes, forward declarations, and module dependencies.
- Check UObject lifetime, delegate cleanup, and async safety.
- Check server authority, replication, and join-in-progress behavior.
- Check whether the feature belongs in an existing subsystem, component, ability, Experience, Game Feature, Interaction path, or asset pipeline.
- Check whether inventory/equipment changes preserve the established authoritative and replicated path.
- Check whether the implementation introduces unnecessary ticking, hard references, or editor-only assumptions.
