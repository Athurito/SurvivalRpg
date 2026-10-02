---
name: survival-rpg-orchestrator
description: Produces the ownership and C++ boundary decision for new or materially extended SurvivalRpg systems and splits large multi-system requests into the smallest verifiable slice. Use before building a new gameplay, UI, persistence, editor, or content-pipeline system, or when a request spans several systems and the split between C++, assets, UI, and tooling is unclear. Not needed for new content inside an existing system, such as an item, ability, widget, or interaction, or for deciding what to build next.
---

# SurvivalRpg Orchestrator

Use this skill to plan broad or multi-system work and to decide ownership before a system is built. Skill selection lives in `AGENTS.md`; load only the specialist skills whose boundaries the request crosses, and do not duplicate their guidance here.

## Plan the work

Copy this checklist and track it:

```text
- [ ] 1. Restate the deliverable and whether it changes code, docs, assets, or only needs advice
- [ ] 2. Load the needed specialist skills and check the request against the first-playable scope
- [ ] 3. Inspect existing systems and extension seams before proposing structure
- [ ] 4. Record the C++ boundary decision
- [ ] 5. Choose the smallest slice that proves fun, clarity, or architectural soundness
- [ ] 6. Implement or document that slice
- [ ] 7. Verify it and name the remaining risks
```

The game vision is `docs/game-vision.md`. When a request lies outside its first-playable scope (section 12), say so and propose the smallest slice or ask whether to build it now.

## Record the C++ boundary decision

Treat every new or materially extended gameplay, UI, editor, persistence, or content-pipeline system as an ownership decision, also when no Lyra subsystem is involved. Read [the system ownership boundaries](references/system-ownership-boundaries.md), then write this block in the answer before creating code or assets, also when the user asks to start right away:

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
- Keep reusable mechanisms native only when Blueprint-inaccessible APIs, authority, prediction, lifecycle invariants, or known or measured hot paths require it. No separate user approval is needed when that technical gate is satisfied.
- Never substitute a native content leaf because `.uasset` authoring is less convenient. Use Unreal MCP first; after a confirmed capability gap, propose only the smallest reusable editor-only tooling seam.
- Take inventory-fragment and GameplayAbility details from `survival-rpg-combat-foundation`, and CommonUI, Widget Blueprint, MVVM, and Unreal MCP details from `unreal-lyra-expert`.

Documentation comments are part of normal implementation quality; apply the documentation defaults in `AGENTS.md`.

## Delegation

Delegate to sub-agents only when the task is large enough, the active tool and the user allow it, and the subtasks are concrete and non-overlapping, for example one agent exploring affected gameplay files, one implementing a UI slice, and one reviewing replication risks. Prefer local execution for small or tightly coupled work, and keep `survival-rpg-project` in every delegated brief.

## Tradeoff priorities

- Product identity outranks convenience.
- First playable outranks feature breadth.
- Clear architecture outranks speculative abstraction.
- Data-driven extension points outrank hard-coded content.
- Fun and readability outrank simulation depth.

Stop scope drift toward MMO structure, pure sandbox building, or punitive survival micromanagement; technical drift toward hard-coded content, oversized managers, or bypassing existing Unreal/Lyra seams; and planning drift beyond the current vertical-slice goal.

## Reporting

For multi-system work, report the skills used and why, the chosen slice, what was implemented, reviewed, or deferred, and the remaining risks or next steps. Name the files, systems, or design goals the plan rests on, and state assumptions when context is incomplete.
