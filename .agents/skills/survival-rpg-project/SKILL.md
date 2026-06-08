---
name: survival-rpg-project
description: Use for SurvivalRpg project vision, feature scope, gameplay priorities, first-playable planning, portal-centric world design, survival/crafting/progression tradeoffs, and content architecture so implementation stays aligned with the game identity.
---

# SurvivalRpg Project

Use this skill for product direction and project guardrails. It should define what the game is trying to become; it should not override concrete repository evidence or Unreal/Lyra technical constraints.

Start from project identity before choosing a solution.

- Read docs/game-vision.md first when it exists and treat it as the primary source of truth for product direction.
- Build toward a modular dark-fantasy survival action RPG with focus on combat, crafting, portals, and sustainable progression.
- Keep the game's identity centered on a living world threatened by portals rather than on pure sandbox building, pure survival simulation, or Soulslike difficulty escalation.
- If a proposed feature adds complexity without improving first-playable feel, clarity, or extensibility, cut or simplify it.

Use this skill together with the Unreal/Lyra expert when engine-specific decisions matter.

- Pair this skill with `$unreal-lyra-expert` for Unreal Engine, GAS, Lyra-style architecture, replication, or modular gameplay decisions.
- Let this skill define product direction and priorities.
- Let the Unreal/Lyra expert define engine-facing implementation patterns and technical best practices.

Prioritize the first playable over breadth.

- Treat the following as the first-playable target:
  - Third-person character controller
  - Melee combat with block, perfect block, and stagger
  - Simple resource nodes for gathering
  - Inventory and simple item data structure
  - Crafting station with two to three recipes
  - Portal encounter with teleport into a sublevel
  - Boss encounter with a portal-close state
  - Rudimentary character and weapon progression
- Prefer completing a thin vertical slice of this loop over adding disconnected systems.

Use these gameplay priorities to break ties.

- Combat first: preserve responsiveness, readability, hit feedback, and meaningful defensive timing windows.
- Survival without friction: make survival matter through preparation, buffs, and situational hazards rather than constant punishment or micromanagement.
- Lasting economy: avoid systems that make early materials, regions, or gear immediately obsolete.
- Dynamic world threat: favor features that let portals, corruption, or world-state changes affect the overworld.
- Replayability through variation: prefer variable encounters, build diversity, and event combinations over raw grind.

Choose architecture that scales without overbuilding.

- Keep systems modular.
- Prefer data-driven definitions for weapons, runes, enemies, recipes, portal encounters, and progression unlocks.
- Add extension seams that make later content easy to slot in.
- Avoid premature frameworks, generic abstractions, or speculative systems that are not yet required by the vertical slice.
- Require concise designer-facing documentation for DataAssets, Blueprint-configurable fields, tuning values, item/equipment definitions, recipe data, portal encounter data, rune data, enemy data, crafting data, loot data, and progression unlock data.

Evaluate feature proposals against these questions.

- Does this improve the feel of combat, gathering, crafting, or the portal loop?
- Does this reinforce the portal-threat fantasy and world reactivity?
- Does this preserve long-term usefulness for resources and older content?
- Can this be expressed in data so new content can be added cheaply later?
- Is this the smallest version that proves the fun?

When making tradeoffs, prefer these outcomes.

- Prefer readable and fun over exhaustive simulation.
- Prefer one strong portal encounter over many shallow activities.
- Prefer a few differentiated weapons or runes over many barely distinct options.
- Prefer systems that compose cleanly over bespoke one-off logic.
- Prefer temporary placeholders only when they do not lock in bad architecture.

Call out drift early during reviews.

- Flag features that push the game toward MMO structure, pure base-building, heavy survival micromanagement, or linear tier replacement.
- Flag implementations that hard-code content likely to grow into data sets.
- Flag designs that make old materials, regions, or recipes worthless.

State assumptions explicitly when context is incomplete.

- Name which files, systems, or project goals the recommendation is based on.
- If project docs exist, read them and treat them as the source of truth over this compact skill.

