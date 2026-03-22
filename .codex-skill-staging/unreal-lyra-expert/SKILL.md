---
name: unreal-lyra-expert
description: Expert guidance and implementation support for Unreal Engine 5.7 projects with emphasis on best practices, architecture, gameplay systems, networking, Gameplay Ability System, modular gameplay, and Lyra patterns. Use when Codex needs to design, review, debug, refactor, or implement Unreal Engine features, especially in Lyra-based or Lyra-inspired projects where maintainability, replication safety, clean C++ and Blueprint boundaries, or scalable gameplay architecture matter.
---

# Unreal Lyra Expert

Inspect the project before proposing changes.

- Read the `.uproject`, relevant `.uplugin` files, `Source/`, and `Config/` first.
- Treat Unreal Engine 5.7 as the default target version for APIs, subsystem behavior, and recommendations.
- Identify the Unreal version, enabled plugins, module layout, and whether the project uses Lyra, GAS, CommonUI, Enhanced Input, CommonGame, ModularGameplay, or Game Features.
- Infer whether the codebase is vanilla Unreal, Lyra-derived, or only borrowing selected Lyra ideas.
- If the project is not on 5.7, still reason from a 5.7 baseline but explicitly call out any likely version mismatch, migration risk, or API difference before recommending changes.

Preserve the existing architecture unless there is a clear defect.

- Prefer small, local fixes over cross-cutting rewrites.
- Respect module boundaries, `Build.cs` dependencies, subsystem ownership, and asset-driven configuration.
- Avoid introducing patterns that fight the existing gameplay framework.

Choose the right implementation boundary.

- Put durable gameplay rules, authority checks, replication logic, and performance-sensitive systems in C++.
- Put tuning, composition, asset references, and presentation-facing glue in Blueprints or Data Assets.
- Expose clear extension points instead of burying core game rules inside Widgets or large Blueprint graphs.

Treat multiplayer correctness as a default concern.

- Assume server authority unless the local-only nature is obvious.
- Verify ownership for RPCs, replication conditions, relevancy, dormancy, and `OnRep` behavior.
- Prefer event-driven replication and explicit state ownership over per-frame polling.

Use Lyra conventions when Lyra is present or clearly intended.

- Read [references/lyra-patterns.md](references/lyra-patterns.md) when the project uses Lyra or follows Lyra-style modular gameplay.
- Keep features aligned with Experiences, Game Features, Pawn Data, Ability Sets, Gameplay Tags, and data-driven activation instead of hard-coded startup logic.
- Reuse existing extension seams before adding new managers or singleton-like systems.

Apply Unreal best practices consistently.

- Read [references/unreal-best-practices.md](references/unreal-best-practices.md) for implementation and review heuristics.
- Prefer explicit ownership, predictable lifetimes, soft references for optional content, and subsystem-appropriate responsibilities.
- Watch for common Unreal hazards: invalid UObject lifetimes, missing reflection macros, circular hard references, unnecessary ticking, heavyweight constructors, asset loads on hot paths, and client-trusting gameplay logic.
- Default to Unreal Engine 5.7 naming, APIs, and engine patterns unless the inspected project files prove the codebase is pinned to another version.

When reviewing or explaining code, lead with concrete risks.

- Call out replication bugs, lifecycle issues, authority leaks, scalability problems, and architecture mismatches first.
- Suggest the smallest fix that matches the project's current conventions.
- If Lyra offers a better home for the feature, name the Lyra concept to anchor the recommendation.

State assumptions explicitly when context is incomplete.

- Say which files or systems the conclusion is based on.
- If the engine version or plugin stack matters, verify it from project files before claiming a pattern is correct, while keeping Unreal Engine 5.7 as the default assumption.
- If multiple valid approaches exist, recommend one and briefly explain the tradeoff.
