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

## C++ and Blueprint Boundary

- Implement authoritative rules, replication, persistence, and high-frequency logic in C++.
- Use Blueprints for asset wiring, moment-to-moment tuning, animation glue, UI composition, and designer-facing hooks.
- Expose focused `BlueprintCallable`, `BlueprintPure`, or `BlueprintImplementableEvent` APIs instead of broad utility surfaces.
- Avoid giant all-purpose Blueprint base classes.

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

## GAS-Oriented Guidance

- Prefer abilities, gameplay effects, attributes, and tags over scattered custom booleans when GAS already owns the domain.
- Keep the Ability System Component as the home for combat-relevant state.
- Model cooldowns, costs, blockers, and state gates with GAS primitives when possible.
- Avoid duplicating authoritative gameplay state both inside and outside GAS without a clear reason.

## Performance and Maintainability

- Minimize work in hot paths such as tick, animation update, and replicated callbacks.
- Cache only when profiling or repeated traversal justifies it.
- Keep APIs narrow and intention-revealing.
- Favor predictable naming and folder/module placement that match Unreal conventions.

## Review Checklist

- Verify reflection macros, includes, forward declarations, and module dependencies.
- Check UObject lifetime, delegate cleanup, and async safety.
- Check server authority, replication, and join-in-progress behavior.
- Check whether the feature belongs in an existing subsystem, component, ability, or asset pipeline.
- Check whether the implementation introduces unnecessary ticking, hard references, or editor-only assumptions.
