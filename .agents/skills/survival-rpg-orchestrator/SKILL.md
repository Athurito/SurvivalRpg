---
name: survival-rpg-orchestrator
description: Use for large or ambiguous SurvivalRpg requests that need routing across project vision, Unreal/Lyra architecture, combat/equipment, planning, review, or implementation. Select only the needed specialist skills and break work into the smallest useful slice.
---

# SurvivalRpg Orchestrator

Use this skill as a router for broad, ambiguous, multi-system, or multi-step SurvivalRpg work.
For narrow tasks, go directly to the relevant specialist skill instead of routing through this skill.

Classify the request before doing work.

- Read docs/game-vision.md first when the task touches product direction, feature scope, gameplay priorities, or vertical-slice tradeoffs.
- Identify whether the task is primarily about game vision, gameplay design, Unreal implementation, architecture, review, debugging, planning, or production execution.
- Identify whether the task changes code, project docs, content structure, or only needs advice.
- Identify whether the request is narrow enough for one pass or should be split into bounded subtasks.

Route to the minimum skill set that covers the task.

- Use `$survival-rpg-project` for any decision that affects game identity, feature scope, progression priorities, portal fantasy, survival friction, or long-term resource relevance.
- Use `$survival-rpg-combat-foundation` for combat foundation, weapon equip/unequip, loadout, hotbar or hotwheel, weapon action routing, or early GAS weapon architecture in this repository.
- Use `$unreal-lyra-expert` for Unreal Engine, GAS, Lyra-style architecture, modular gameplay, replication, C++ versus Blueprint boundaries, or engine-facing reviews.
- Use both skills together when implementing gameplay systems in this repository.
- Do not duplicate specialist guidance inside this skill. Delegate to the specialist skill instead.

Apply these default routing rules.

- New gameplay feature: use `$survival-rpg-project`, then add `$unreal-lyra-expert` if the feature touches runtime implementation.
- Combat foundation, weapon equip/unequip, hotbar or hotwheel loadout, or early weapon GAS work: use `$survival-rpg-combat-foundation` first, then add `$unreal-lyra-expert` for engine-facing work and `$survival-rpg-project` when feature scope or progression tradeoffs matter.
- Combat, progression, gathering, crafting, portals, bosses, runes, or world events: use both core skills unless the user is only discussing design direction; keep `$survival-rpg-combat-foundation` active whenever the task touches the Phase 1 weapon backbone.
- Refactor, bug fix, review, replication, GAS ability work, or subsystem placement: use `$unreal-lyra-expert` and keep `$survival-rpg-project` active if the change could shift the game's identity or scope.
- Documentation, prioritization, feature triage, or MVP planning: use `$survival-rpg-project` first and only add `$unreal-lyra-expert` when technical constraints matter.

Turn large requests into a stable execution sequence.

- Step 1: restate the concrete deliverable internally.
- Step 2: load the relevant project and technical skills.
- Step 3: inspect the local repository before proposing structure changes.
- Step 4: choose the smallest slice that proves fun, clarity, or architectural soundness.
- Step 5: implement or document that slice.
- Step 6: verify the result and call out remaining risks.

Use sub-agents only when the task is explicitly large enough to benefit from delegation.

- Prefer local execution for small or tightly coupled work.
- Split only into concrete, non-overlapping subtasks with clear ownership.
- Good delegation examples: one agent explores affected gameplay files, another implements a UI slice, another reviews replication risks.
- Avoid delegation when the next action is blocked on a small local answer.
- When delegating, keep the project skill active in the framing so all agents stay aligned with the same product guardrails.

Use these orchestration priorities when tradeoffs appear.

- Product identity outranks convenience.
- First playable outranks feature breadth.
- Clear architecture outranks speculative abstraction.
- Data-driven extension points outrank hard-coded content.
- Fun and readability outrank simulation depth.

Catch coordination failures early.

- Stop scope drift toward MMO structure, pure sandbox building, or punitive survival micromanagement.
- Stop technical drift toward hard-coded content, oversized managers, or systems that bypass Unreal and Lyra extension seams already present.
- Stop planning drift when a task grows beyond the current vertical-slice goal.

When reporting back, summarize in this order when routing decisions matter. Do not force a routing report for small implementation tasks.

- Which specialist skills were relevant and why.
- What concrete slice was chosen.
- What was implemented, reviewed, or deferred.
- What risks or next steps remain.

State assumptions explicitly when context is incomplete.

- Name the files, systems, or design goals used to classify the task.
- If there is no project documentation yet, use `$survival-rpg-project` as the temporary source of truth and recommend creating docs when the same decisions repeat.


