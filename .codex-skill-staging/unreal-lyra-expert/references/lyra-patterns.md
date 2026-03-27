# Lyra Patterns

Use this reference when the project includes Lyra directly or clearly mirrors Lyra's architecture.

## Core Principle

Prefer extending Lyra's existing seams over bypassing them. Lyra is opinionated around modular, data-driven gameplay. New features should usually hook into that flow rather than adding custom startup code, singleton managers, or bespoke possession logic.

## Experience-Centered Design

- Treat the active Experience as the high-level composition root.
- Prefer loading feature behavior through experience definitions, action sets, and feature actions.
- Avoid hard-coding gameplay initialization in unrelated classes when the Experience can own it.

## Game Features

- Put optional or mode-specific features behind Game Feature plugins when the project already uses that model.
- Use activation/deactivation flow instead of assuming all content is always loaded.
- Keep plugin boundaries clean so features can be enabled without entangling the base game module.

## Pawn Data and Spawning

- Prefer `PawnData`-driven setup for pawn class, input, camera, and ability configuration.
- Keep spawn-time wiring declarative where possible.
- Avoid scattering pawn setup across controller, character, widget, and game mode code.

## Input

- Use Enhanced Input mapping contexts and input config assets instead of hard-coded key logic.
- Keep gameplay input definitions close to the feature or pawn data that owns them.
- Avoid direct widget-driven input routing when a gameplay input layer already exists.

## GAS and Ability Sets

- Use ability sets, granted abilities, effects, and tags as the standard way to compose capabilities.
- Prefer tag-based gating and state queries over hand-maintained booleans.
- Keep combat or ability state in GAS unless there is a strong non-GAS reason.

## UI and CommonUI

- Respect the existing UI layer model if CommonUI is present.
- Route screen flow, activatable widgets, and input focus through the framework instead of custom widget stacks.
- Avoid coupling core gameplay rules to widget state.

## Teams, Tags, and Shared Systems

- Reuse Lyra's tag and team concepts when adding rule checks, targeting, or UI decisions.
- Prefer gameplay tags for feature switches, state labels, and cross-system contracts.
- Avoid stringly-typed feature flags or duplicate enums when gameplay tags already express the concept.

## Review Heuristics

- Ask whether the feature should be an Experience concern, a Game Feature, a Pawn Data concern, an Ability Set addition, or a UI-layer change.
- Flag implementations that duplicate Lyra systems instead of extending them.
- Recommend the smallest Lyra-native seam that can own the feature cleanly.
