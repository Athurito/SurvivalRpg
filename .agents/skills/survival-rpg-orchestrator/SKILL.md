---
name: survival-rpg-orchestrator
description: Use for large or ambiguous SurvivalRpg requests and for new or materially extended gameplay, UI, editor, persistence, or content-pipeline systems that need repository-wide ownership boundaries. Routes across project vision, Unreal/Lyra architecture, GASP animation/locomotion, and combat/equipment, then selects only the needed specialist skills and the smallest verifiable slice.
---

# SurvivalRpg Orchestrator

Use this skill as a router for broad, ambiguous, multi-system, or multi-step SurvivalRpg work. For narrow tasks, use the relevant specialist skill directly.

Skill names below refer to the shared skills in `.agents/skills/` (Codex: `$name`, Claude Code: `/name`).

## Classify the request

- Read `docs/game-vision.md` when the task touches product direction, feature scope, gameplay priorities, or vertical-slice tradeoffs.
- Identify whether the task is primarily vision, gameplay design, Unreal implementation, architecture, review, debugging, planning, or production execution.
- Identify whether it changes code, docs, content/assets, or only needs advice, and whether it fits one pass or needs bounded subtasks.
- Treat every new or materially extended gameplay, UI, editor, persistence, or content-pipeline system as a system-ownership decision, even when no Lyra subsystem is involved. Read [the system ownership boundaries](references/system-ownership-boundaries.md) for new systems, cross-cutting refactors, or unclear C++/asset/UI/tooling placement.

## Route to the minimum skill set

| Skill | Owns |
| --- | --- |
| `survival-rpg-project` | Game identity, feature scope, first-playable priorities, progression identity, portal fantasy, survival friction, long-term resource relevance |
| `survival-rpg-combat-foundation` | Combat, equipment and loadouts, hotbar/quick access, item instances, weapon action routing, ability grants, mastery, runes, portal combat, Dungeonbreak |
| `unreal-lyra-expert` | Unreal/GAS/Lyra architecture, replication, Experiences, Game Features, Interaction, CommonUI/MVVM, C++ versus asset boundary, Unreal MCP asset authoring |
| `unreal-gasp-expert` | GASP locomotion on the CMC and Mover variants, traversal, Motion Matching/Pose Search, animation threading, locomotion network parity |

Default combinations:

- New gameplay feature: `survival-rpg-project`, plus `unreal-lyra-expert` once it touches runtime implementation.
- Combat, equipment, loadout, or weapon GAS work: `survival-rpg-combat-foundation` first; add `unreal-lyra-expert` for engine-facing work and `survival-rpg-project` when scope or progression tradeoffs matter.
- Progression, gathering, crafting, portals, bosses, runes, or world events: `survival-rpg-project` and `unreal-lyra-expert` unless only design direction is discussed; keep `survival-rpg-combat-foundation` active when the weapon backbone is touched.
- GASP-only work such as source audits, Pose Search tuning, or cosmetic AnimGraph changes: `unreal-gasp-expert` alone.
- GASP work that touches PawnData, Experiences, Game Features, character lifecycle, movement replication, GAS montages, equipment, death, or ragdoll: add `unreal-lyra-expert`.
- GASP work that touches attacks, dodge, block, hit reactions, combat notifies or tags, or equipment-granted abilities: also add `survival-rpg-combat-foundation`.
- Adopting further sample systems (Locomotor, sample camera, Foley, experimental plugins) or widening Mover scope: `unreal-gasp-expert` for an isolated dependency evaluation against `docs/gasp-integration-roadmap.md`; add `survival-rpg-project` when movement fantasy or scope changes.
- Refactor, bug fix, review, replication, GAS ability work, or subsystem placement: `unreal-lyra-expert`; keep `survival-rpg-project` active if identity or scope could shift.
- Documentation, prioritization, feature triage, or MVP planning: `survival-rpg-project` first; add `unreal-lyra-expert` only when technical constraints matter.

Do not duplicate specialist guidance here; delegate to the specialist skill.

## Record the C++ boundary decision

Before creating code or assets for a new or materially extended system, state:

```text
C++ boundary decision
- Classification: schema, reusable mechanism, or designer-owned content
- Runtime truth: authoritative owner and lifecycle
- Existing seam: inspected project/framework base, asset, component, subsystem, or ViewModel
- Ownership: C++ versus Blueprint, Widget Blueprint, and DataAsset
- New native classes: count and technical justification
- Asset work: Unreal MCP authoring and validation steps
```

- Expect zero new native classes for pure content, tuning, composition, or presentation work.
- Keep reusable mechanisms native only when Blueprint-inaccessible APIs, authority, prediction, lifecycle invariants, or known/measured hot paths require it. No separate user approval is needed when that technical gate is satisfied.
- Never substitute a native content leaf because `.uasset` authoring is less convenient. Use Unreal MCP first; after a confirmed capability gap, propose only the smallest reusable editor-only tooling seam.
- Route inventory-fragment and GameplayAbility ownership through `survival-rpg-combat-foundation` and `unreal-lyra-expert`; route CommonUI, Widget Blueprint, MVVM, and Unreal MCP authoring through `unreal-lyra-expert`.

Documentation comments are part of normal implementation quality. Apply the documentation defaults in `AGENTS.md` without turning them into a separate task.

## Execute large requests as a stable sequence

1. Restate the concrete deliverable.
2. Load the relevant project and technical skills.
3. Inspect the repository before proposing structure changes.
4. Record the system-ownership and C++ boundary decision.
5. Choose the smallest slice that proves fun, clarity, or architectural soundness.
6. Implement or document that slice.
7. Verify the result and name remaining risks.

Delegate to sub-agents only when the task is large enough, the active tool and the user allow delegation, and the subtasks are concrete and non-overlapping, for example one agent exploring affected gameplay files, one implementing a UI slice, and one reviewing replication risks. Prefer local execution for small or tightly coupled work, and keep the project skill in every delegated brief.

## Tradeoff priorities

- Product identity outranks convenience.
- First playable outranks feature breadth.
- Clear architecture outranks speculative abstraction.
- Data-driven extension points outrank hard-coded content.
- Fun and readability outrank simulation depth.

Stop scope drift toward MMO structure, pure sandbox building, or punitive survival micromanagement; technical drift toward hard-coded content, oversized managers, or bypassing existing Unreal/Lyra seams; and planning drift beyond the current vertical-slice goal.

## Reporting

When routing decisions matter (not for small implementation tasks), report in this order: relevant skills and why, the chosen slice, what was implemented, reviewed, or deferred, and remaining risks or next steps. Name the files, systems, or design goals used to classify the task, and state assumptions when context is incomplete.
