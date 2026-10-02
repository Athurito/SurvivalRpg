---
name: survival-rpg-project
description: Keeps SurvivalRpg work aligned with the game vision, covering identity, feature scope, first-playable priorities, portal-centric world design, and survival, crafting, and progression tradeoffs. Use when deciding what to build, triaging or prioritizing features, planning the vertical slice, reviewing a proposal for scope drift, or when a new system or feature may lie outside the first-playable scope.
---

# SurvivalRpg Project

Use this skill for product direction and project guardrails. It defines what the game is trying to become; it does not override concrete repository evidence or Unreal/Lyra technical constraints.

`docs/game-vision.md` is the source of truth for product direction. Read the relevant sections first; this skill is a compact summary and loses to the vision document when they differ.

## Project identity

- Build a modular dark-fantasy open-world survival action RPG focused on combat, portals and Dungeonbreaks, runes, magical harvesting, and sustainable progression.
- Keep the identity centered on a living world threatened by portals, not on pure sandbox building, pure survival simulation, or Soulslike difficulty escalation.
- Cut or simplify features that add complexity without improving first-playable feel, clarity, or extensibility.

Pair this skill with `unreal-lyra-expert` when engine-specific decisions matter: this skill sets direction and priorities, the Unreal/Lyra skill sets implementation patterns.

## First playable over breadth

The current first-playable scope is section 12 of `docs/game-vision.md` (MVP content and what the slice must prove). In short: third-person character, basic combat with attack, block, and perfect block or stagger, one simple enemy, XP/mastery progress, simple item and equipment structure, one portal that spawns enemies or leads to a sublevel, a portal-close or boss state, one rune that visibly changes a weapon, one magical harvesting ability, two to three resources, and one to two crafting stations.

Prefer completing a thin vertical slice of this loop over adding disconnected systems. Large open world, large base building, many weapon classes, complex factions, full Dungeonbreak simulation, broad magic catalogs, and endgame are explicitly not early priorities.

## Tie-breaking priorities

- Combat first: preserve responsiveness, readability, hit feedback, and meaningful defensive timing windows.
- Survival without friction: make survival matter through preparation, buffs, and situational hazards rather than constant punishment or micromanagement.
- Lasting economy: avoid systems that make early materials, regions, or gear immediately obsolete.
- Dynamic world threat: favor features that let portals, corruption, or world-state changes affect the overworld.
- Replayability through variation: prefer variable encounters, build diversity, and event combinations over raw grind.

## Architecture that scales without overbuilding

- Keep systems modular and prefer data-driven definitions for weapons, runes, enemies, recipes, portal encounters, and progression unlocks.
- Add extension seams that make later content cheap to slot in; avoid premature frameworks, generic abstractions, or speculative systems the vertical slice does not need.
- Designer-facing data needs concise documentation per the documentation defaults in `AGENTS.md`.

## Evaluating proposals

Ask:

- Does this improve the feel of combat, gathering, crafting, or the portal loop?
- Does this reinforce the portal-threat fantasy and world reactivity?
- Does this preserve long-term usefulness for resources and older content?
- Can this be expressed in data so new content can be added cheaply later?
- Is this the smallest version that proves the fun?

Prefer readable and fun over exhaustive simulation, one strong portal encounter over many shallow activities, a few differentiated weapons or runes over many barely distinct options, systems that compose cleanly over bespoke one-off logic, and placeholders only when they do not lock in bad architecture.

## Drift to flag in reviews

- Features that push toward MMO structure, pure base-building, heavy survival micromanagement, or linear tier replacement.
- Implementations that hard-code content likely to grow into data sets.
- Designs that make old materials, regions, or recipes worthless.

Name the files, systems, or project goals a recommendation is based on, and state assumptions when context is incomplete.
