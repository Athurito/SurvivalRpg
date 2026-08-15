---
name: survival-rpg-orchestrator
description: Use for large or ambiguous SurvivalRpg requests and for new or materially extended gameplay, UI, editor, persistence, or content-pipeline systems that need repository-wide ownership boundaries. Route across project vision, Unreal/Lyra architecture, GASP animation/locomotion, combat/equipment, planning, review, or implementation; select only the needed specialists and smallest useful slice.
---

# SurvivalRpg Orchestrator

Use this skill as a router for broad, ambiguous, multi-system, or multi-step SurvivalRpg work.
For narrow tasks, go directly to the relevant specialist skill instead of routing through this skill.

Classify the request before doing work.

- Read docs/game-vision.md first when the task touches product direction, feature scope, gameplay priorities, or vertical-slice tradeoffs.
- Identify whether the task is primarily about game vision, gameplay design, Unreal implementation, architecture, review, debugging, planning, or production execution.
- Identify whether the task changes code, project docs, content structure, or only needs advice.
- Identify whether the request is narrow enough for one pass or should be split into bounded subtasks.
- Treat every new or materially extended gameplay, UI, editor, persistence, or content-pipeline system as a system-ownership decision, even when it does not use a Lyra-specific subsystem.
- Read [the repository-wide system ownership boundaries](references/system-ownership-boundaries.md) for new systems, cross-cutting refactors, or unclear C++/asset/UI/tooling placement.

Route to the minimum skill set that covers the task.

- Use `$survival-rpg-project` for any decision that affects game identity, feature scope, progression priorities, portal fantasy, survival friction, or long-term resource relevance.
- Use `$survival-rpg-combat-foundation` for combat foundation, weapon equip/unequip, loadout, hotbar or hotwheel, weapon action routing, or early GAS weapon architecture in this repository.
- Use `$unreal-lyra-expert` for Unreal Engine, GAS, Lyra-style architecture, modular gameplay, replication, C++ versus Blueprint boundaries, or engine-facing reviews.
- Use `$unreal-gasp-expert` for GASP CMC locomotion, Motion Matching, Pose Search, trajectory, procedural animation nodes, retargeting, animation threading, or multiplayer locomotion parity.
- Combine only the specialists whose ownership boundaries the request crosses.
- Do not duplicate specialist guidance inside this skill. Delegate to the specialist skill instead.

Make a system-ownership decision for every new or materially extended system, then record its C++ boundary before implementation. Lyra is a useful reference implementation, not the scope of this rule.

State this concise decision before creating code or assets:

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
- Keep native schema types such as semantic inventory-fragment subclasses in C++; configure their instances in ItemDefinition assets instead of creating Blueprint fragment subclasses.
- Keep reusable mechanisms native only when Blueprint-inaccessible APIs, authority, prediction, lifecycle invariants, or known/measured hot paths require it. Do not require separate user approval when that technical gate is satisfied.
- Route inventory-fragment and GameplayAbility ownership decisions through `$survival-rpg-combat-foundation` and `$unreal-lyra-expert`.
- Route CommonUI, Widget Blueprint, MVVM, and Unreal MCP asset-authoring decisions through `$unreal-lyra-expert`.
- Never substitute a native content leaf because `.uasset` authoring is less convenient. Use Unreal MCP first; after a confirmed capability gap, propose only the smallest reusable editor-only tooling seam.
- Apply the same decision to crafting, progression, portals, AI, saving, world events, interaction, building, economy, UI, and future systems; route only the domain-specific details to Lyra or another specialist.

Preserve repository-wide documentation defaults.

When a routed task creates or modifies designer-facing Unreal APIs, DataAssets, Blueprint-configurable fields, combat tuning, item/equipment data, portal data, rune data, recipe data, progression data, replicated gameplay state, or save-relevant state, ensure the selected specialist skill adds concise documentation comments by default.

Do not turn this into a separate documentation task unless the user asked for documentation only. Treat it as part of normal implementation quality.

Apply these default routing rules.

- New gameplay feature: use `$survival-rpg-project`, then add `$unreal-lyra-expert` if the feature touches runtime implementation.
- Combat foundation, weapon equip/unequip, hotbar or hotwheel loadout, or early weapon GAS work: use `$survival-rpg-combat-foundation` first, then add `$unreal-lyra-expert` for engine-facing work and `$survival-rpg-project` when feature scope or progression tradeoffs matter.
- Combat, progression, gathering, crafting, portals, bosses, runes, or world events: use both core skills unless the user is only discussing design direction; keep `$survival-rpg-combat-foundation` active whenever the task touches the Phase 1 weapon backbone.
- GASP source audits, curated asset work, Pose Search tuning, or purely cosmetic AnimGraph work: use `$unreal-gasp-expert` alone unless the task crosses a gameplay boundary.
- GASP work that touches PawnData, Experiences, Game Features, character lifecycle, movement replication, GAS montages, equipment, death, or ragdoll: use `$unreal-gasp-expert` together with `$unreal-lyra-expert`.
- GASP work that touches attacks, dodge, block, hit reactions, combat montage notifies, combat tags, or equipment-granted abilities: also use `$survival-rpg-combat-foundation`.
- Requests to adopt GASP Mover, Traversal, Locomotor, camera, Foley, or experimental systems: route through `$unreal-gasp-expert` for an isolated dependency evaluation and add `$survival-rpg-project` when the change affects gameplay scope or movement fantasy.
- Refactor, bug fix, review, replication, GAS ability work, or subsystem placement: use `$unreal-lyra-expert` and keep `$survival-rpg-project` active if the change could shift the game's identity or scope.
- Documentation, prioritization, feature triage, or MVP planning: use `$survival-rpg-project` first and only add `$unreal-lyra-expert` when technical constraints matter.

Turn large requests into a stable execution sequence.

- Step 1: restate the concrete deliverable internally.
- Step 2: load the relevant project and technical skills.
- Step 3: inspect the local repository before proposing structure changes.
- Step 4: record the repository-wide system-ownership and C++ boundary decision.
- Step 5: choose the smallest slice that proves fun, clarity, or architectural soundness.
- Step 6: implement or document that slice.
- Step 7: verify the result and call out remaining risks.

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


