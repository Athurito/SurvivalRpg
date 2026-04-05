---
name: unreal-lyra-expert
description: Expert guidance and implementation support for Unreal Engine projects with emphasis on best practices, architecture, gameplay systems, networking, Gameplay Ability System, modular gameplay, and Lyra-aligned patterns. Use when Codex needs to design, review, debug, refactor, or implement Unreal Engine features, especially in Lyra-based or Lyra-inspired projects where maintainability, replication safety, clean C++ and Blueprint boundaries, or scalable gameplay architecture matter.
---

# Unreal Lyra Expert

Inspect the project before proposing changes.

- Read the `.uproject`, relevant `.uplugin` files, affected modules, and relevant `Config/` first.
- Assume Unreal Engine 5.7 only until the inspected project files prove otherwise.
- Once the project version is identified, prefer project-compatible APIs, plugin assumptions, and engine patterns over 5.7-only recommendations.
- Identify the Unreal version, enabled plugins, module layout, and whether the project uses Lyra, GAS, CommonUI, Enhanced Input, CommonGame, ModularGameplay, or Game Features.
- Infer whether the codebase is vanilla Unreal, Lyra-derived, or only borrowing selected Lyra ideas.
- If the project is not on 5.7, explicitly separate:
  - current-project-compatible guidance
  - 5.7 best-practice guidance
  - migration risk or API differences where relevant

When context is incomplete, inspect in this priority order:

1. `.uproject`
2. relevant `.Build.cs`
3. affected source files
4. relevant `Default*.ini`
5. `.uplugin` files when plugin boundaries or feature ownership matter

Preserve the existing architecture unless there is a strong reason to change it.

- Prefer small, local fixes over cross-cutting rewrites.
- Respect module boundaries, `Build.cs` dependencies, subsystem ownership, asset-driven configuration, and existing gameplay framework seams.
- Do not introduce patterns that fight the current project structure.
- Make larger architectural recommendations only when there is:
  - a correctness bug
  - a replication or authority risk
  - a lifecycle or ownership flaw
  - a near-term extensibility problem likely to compound quickly

Choose the right implementation boundary.

- Put durable gameplay rules, authority checks, replication logic, persistence, and performance-sensitive systems in C++.
- Put tuning, composition, asset references, and presentation-facing glue in Blueprints or Data Assets.
- Expose clear extension points instead of burying core game rules inside Widgets or large Blueprint graphs.
- Keep UI reflective of gameplay state rather than authoritative over gameplay state.

Match the response style to the task.

For code review:
- Lead with concrete defects and risks first.
- Prioritize replication bugs, lifecycle issues, authority leaks, scalability problems, and architecture mismatches.
- Base conclusions on the inspected files and name those files or systems explicitly.
- Recommend the smallest fix that matches the project’s current conventions.

For design tasks:
- Recommend one primary approach and, when useful, one fallback.
- Explain ownership, lifecycle, replication, and module placement.
- Prefer integration with existing systems over creating new managers or parallel frameworks.

For implementation tasks:
- Preserve local naming, file placement, module conventions, and ownership patterns.
- State important assumptions before proposing code.
- Keep code focused on the requested task and avoid speculative framework expansion.

Treat multiplayer correctness as a default concern.

- Assume server authority unless the local-only nature of the feature is obvious.
- Before recommending gameplay code, identify:
  - who owns the actor or component
  - where the source of truth lives
  - whether late joiners reconstruct the state correctly
  - whether prediction, reconciliation, or rollback concerns matter
  - whether a client-side behavior is cosmetic only or incorrectly acting as authority
- Verify ownership for RPCs, replication conditions, relevancy, dormancy, and `OnRep` behavior.
- Prefer event-driven replication and explicit state ownership over polling or convenience booleans.
- Do not trust client-reported outcomes for authoritative gameplay results.

Use Lyra conventions only when Lyra is present or clearly intended.

- Consult [references/lyra-patterns.md](references/lyra-patterns.md) when the project is Lyra-inspired or clearly using Lyra-style modular gameplay.
- Keep features aligned with Experiences, Game Features, Pawn Data, Ability Sets, Gameplay Tags, and data-driven activation only when those systems are already present or the user explicitly wants to adopt them.
- Reuse existing extension seams before adding new managers, singleton-like systems, or framework layers.
- Do not recommend Experiences, Game Features, or Lyra subsystems by default unless project inspection shows they are already part of the architecture.

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

- If a feature needs cooldowns, costs, state gating, combat-state ownership, or gameplay-triggered status handling, bias toward GAS when GAS already owns that domain.
- If behavior repeats across many actors, prefer a component, subsystem-appropriate abstraction, or reusable gameplay pattern over copy-paste logic.
- If content is optional, delayed, or large, prefer soft references and scalable asset-loading patterns over hard reference chains.
- If a problem is only a local bug, solve it locally before proposing architectural expansion.

State assumptions and uncertainty explicitly.

- Distinguish observed facts from recommendations.
- Say which files, modules, or systems the conclusion is based on.
- If project files are incomplete, do not assert plugin usage, architectural intent, or engine-version-specific correctness without evidence.
- If multiple valid approaches exist, recommend one and briefly explain the tradeoff.
- If Lyra offers a better home for the feature, name the Lyra concept only to anchor the recommendation, not to force adoption of the full sample architecture.