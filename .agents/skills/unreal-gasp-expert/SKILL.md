---
name: unreal-gasp-expert
description: Guides SurvivalRpg's Game Animation Sample (GASP) locomotion and traversal on the CMC and Mover variants, covering Motion Matching, Pose Search, AnimBP and Chooser ownership, sprint and equipment-load-aware gait, mantle, vault, and hurdle, retargeting, animation thread safety, and multiplayer locomotion parity. Use for any plan or change touching character animation, locomotion or movement feel, traversal such as mantle, vault, climbing, or ledges, or their replication; it starts from the GASP roadmap and handoff.
---

# Unreal GASP Expert

Start every animation or movement change from the accepted integration state and the current repository, not from earlier migration plans. The archived native GASP port (branch `codex/archive-gasp-native-port-2026-09-06`, `ec8bef45`) is historical reference only and must not be restored.

## Start from current repository truth

1. Read `docs/gasp-integration-handoff.md` and `docs/gasp-integration-roadmap.md`, then only the report linked to the affected task ID. Respect the active task, its branch, and recorded file ownership; keep one bounded roadmap task per branch/PR. Continue an already authorized active task at its recorded next step without requiring it to be marked ready again. When starting a new task, if no task marked ready covers the request, say so and ask the user to define the task and mark it ready before implementing; a proposed task outline may accompany the question.
2. Before building or switching branches, verify the checkout-local NetworkPrediction/Mover plugin overrides as described in `Build/Patches/NetworkPrediction/README.md`. They are Git-ignored and not present in a fresh worktree.
3. Read `SurvivalRpg.uproject` to confirm the engine version and enabled animation plugins.
4. Inspect the affected project-owned GASP content under `/Game/SurvivalRpg/Characters/GASP`, the AnimBP parent (`URpgAnimInstance` where used), character and movement code, traversal code (`Source/SurvivalRpg/Traversal`), the relevant Experience and PawnData, and the existing tests.
5. Compare with the GASP reference project listed under "Local environment" in `AGENTS.md` when available. It is a version-matched comparison source, never a runtime dependency; the original import was removed from this project.
6. Compare with the Lyra reference project listed there when a change crosses PawnData, Experience, GAS, character lifecycle, equipment, death, or ragdoll.

If a reference project is unavailable, continue from repository truth and say that the comparison could not be performed. Read [the GASP-Lyra integration contract](references/gasp-lyra-integration.md) whenever a task crosses animation, gameplay, composition, combat, equipment, or networking ownership.

## Integration state and scope

Take the current state from the roadmap, not from memory: its "Bereits vorhanden" and status tables list the accepted movement variants, Experiences, traversal actions, and pilots, and the handoff names the active task and the open findings. Independent of that state:

- GASP content is project-owned; runtime never depends on the external sample checkout.
- Scope a change to the variant its task names. When touching shared assets or seams, check that the other variant still works.
- Further sample systems (Locomotor, sample camera, additional Foley or audio, experimental plugins) need explicit roadmap scope and an isolated dependency evaluation; they are never incidental additions.

## Classify the work before changing it

Place the request in one or more categories and choose the smallest slice that proves the intended behavior:

- source-parity audit against GASP
- Blueprint or animation import, retargeting, or content cleanup
- Pose Search schema, database, normalization, or chooser work
- AnimGraph, trajectory, Motion Matching, Blend Stack, Steering, Offset Root Bone, or Foot Placement work
- starts, stops, pivots, turn-in-place, gait, crouch, jump, or landing debugging
- equipment-load-aware locomotion, sprint, mantle, vault, hurdle, climb, or ledge movement
- animation threading, replication, simulated-proxy, or late-join debugging
- Lyra, GAS montage, combat, equipment, death, or ragdoll integration

Separate observed GASP behavior, current SurvivalRpg behavior, and the proposed adaptation.

## Choose the implementation boundary explicitly

Epic's GASP is Blueprint-authored glue on native engine animation systems. That proves neither that a production Lyra integration must stay Blueprint-only nor that all adaptation logic belongs in C++. Before implementing a slice, state which responsibility belongs in C++, Blueprint, or data assets and why.

- Keep authority, replication, movement truth, server-validated traversal, game-thread-to-worker snapshots, world queries, engine-facing custom AnimNodes, and narrowly scoped durable pure resolvers in C++.
- Keep AnimGraph composition, layer and mask wiring, blend and warping feel, database membership, animation references, character/profile tuning, and presentation-only configuration in AnimBPs, Choosers, or DataAssets by default.
- Keep gameplay state in GAS, equipment, the movement component, and Lyra lifecycle systems; animation consumes only read-only presentation inputs.
- Require a concrete threading, networking, engine-integration, performance, or deterministic-testability reason before moving cosmetic selection or tuning into C++.
- Do not translate a complex native state machine one-to-one into Blueprint; reduce and separate responsibilities first.
- Keep `URpgAnimInstance` and any native animation coordinator narrow. Before adding another enum, state machine, watchdog, database switch, or asset classifier, check whether a focused value-only helper, profile, Chooser, AnimBP, or DataAsset should own it.
- Prefer configured roles, tags, or explicit profile membership over hard-coded package-path classification, and avoid duplicating one database contract across enums, fixed arrays, switches, validation, and tests.
- Keep a concise mapping from the relevant source GASP Blueprint/Chooser behavior to the SurvivalRpg seam so the adaptation stays learnable.

## RPG locomotion and curated traversal

- Build grounded dark-fantasy movement that communicates equipment load, stamina, combat state, terrain, and learned traversal without constant survival friction or superhero parkour.
- `Equipment.Load.Light`, `Equipment.Load.Medium`, and `Equipment.Load.Heavy` are gameplay input. Equipment/GAS and the movement component stay authoritative; animation consumes a thread-safe snapshot.
- Define load-aware movement as a data-driven profile (speed, acceleration, braking, rotation response, jump, dodge, stamina, gait, Pose Search selection) and apply only differences that improve feel and readability. Never fake heavy running only by slowing playback; movement physics, gameplay costs, gait selection, and visual weight follow one authoritative profile.
- Replicate or reconstruct the animation-relevant movement profile for simulated proxies and late joiners; owner-only equipment state is not enough.
- Sprint is gameplay-owned with GAS stamina (see `docs/gasp-cmc-sprint.md`). Extend that seam instead of inferring sprint from a locally selected pose or adding parallel sprint state.
- Mantle, vault, hurdle, and short climb are curated, server-validated RPG traversal actions using project-owned montages and Motion Warping where appropriate, on both CMC and Mover. Neither variant may silently depend on the other.
- Sustained climbing, ledge hanging, ladders, or wall movement need explicit movement modes or equally authoritative movement state, never AnimBP-only logic.
- Pilot a new traversal action in isolation (separate PawnData/Experience or a development GameFeature) and prove collision validation, cancellation, multiplayer correction, and rollback before widening the move set.
- Preserve combat, equipment, montage-slot, root-motion, hit-reaction, death, and ragdoll behavior while traversal is active or interrupted.

## Keep animation updates thread-safe

- Gather character, movement, ASC, component, and world state on the game thread and pass only immutable or snapshot-safe values through the AnimInstance proxy.
- During worker-thread update, do not query actors, components, the ASC, or the world, load assets, or mutate UObjects. Snapshot gameplay tags and gameplay-derived gates on the game thread.
- Keep Motion Matching selection and procedural animation cosmetic; authoritative outcomes never depend on the locally selected pose.
- Preserve parallel animation update for non-GASP AnimBPs and add no game-thread work when a feature is disabled.
- Keep snapshots focused; do not move presentation-only state into the proxy to avoid a clean AnimBP, Chooser, or DataAsset boundary.

## Preserve source fidelity without importing source coupling

For asset or Pose Search changes:

- Audit the recursive dependency closure and import only what the approved task includes.
- Verify the source and target skeleton contract before retargeting, and keep migrated assets project-owned.
- Preserve root-motion enablement, normalization, force-lock, and reference-pose locking where the import contract requires it, and preserve curve spelling and case unless a documented normalization replaces them.
- Preserve sampling ranges, exclusion/transition/cost notifies, mirroring, database membership, schema channels, normalization, tags, and cost biases.
- Remove Foley, EarlyTransition, BranchIn, state-machine, or other references only with an explicit local replacement policy.
- Update the task report and the relevant manifest under `docs/assets` when membership or source parity changes.
- Review binary/LFS scope and hard, soft, and management references before accepting an asset diff. On-disk compression size is not the content contract.

## Preserve multiplayer and gameplay seams

- Verify autonomous proxy, authority, and simulated proxy inputs separately, including the replicated inputs remote starts, stops, pivots, gait, rotation modes, turn-in-place, jump, and landing need.
- Verify reconstruction after late join and after movement correction, not only steady-state local play.
- Keep gameplay authority in movement, GAS, equipment, and character systems; pose selection, foot placement, and presentation state stay cosmetic.
- Preserve `GetMesh()` as the mesh for GAS montages, notifies, equipment sockets, corpse physics, and ragdoll unless the project establishes a different authoritative seam.
- Preserve the `DefaultSlot` montage path and root motion from montages only unless a reviewed gameplay requirement changes that contract.
- Recheck attack combos, block, dodge, hit reactions, harvesting rewards, death, equipment attachment, and ragdoll after AnimGraph or skeleton changes.

## Route across specialist skills

- Add `unreal-lyra-expert` for PawnData, Experiences, Game Features, character classes, movement replication, GAS integration, montage lifecycle, equipment, death, or ragdoll.
- Add `survival-rpg-combat-foundation` when the task changes attacks, block, dodge, hit reactions, combat montages, equipment-granted abilities, or combat animation tags.
- Add `survival-rpg-project` only when an animation decision changes product scope, combat feel, traversal scope, progression identity, or first-playable priorities.
- Keep isolated source audits, retargeting checks, Pose Search tuning, and purely cosmetic AnimGraph work within this skill.
- Author AnimBP, layer interface, montage, notify, BlendSpace, Chooser, and retarget assets through the Unreal MCP workflow; its [project toolsets](../unreal-lyra-expert/references/unreal-mcp-asset-authoring.md#project-toolsets) cover the operations the engine toolsets lack.

## Verify proportionally

Use the narrowest verification that proves the change, then widen when the boundary demands it.

- Run the relevant editor build after C++ or module dependency changes; never claim compilation without running it.
- Discover and run the current focused automation filters for the changed behavior (see the task reports for filters). Archived GASP-port tests are historical evidence, not active fixtures.
- Build, test, and cook through `Build/Tools/Unreal/ue.py` as described in unreal-lyra-expert; pass reconciliation and frame-rate preconditions with `--exec`, for example `--exec "np.ForceReconcile 0" --exec "t.MaxFPS 30"`.
- Validate affected assets: AnimBP compilation, parent class, skeleton, exposed defaults, graph nodes, reference closure, and cook/load assumptions.
- Test locomotion in editor for starts, stops, pivots, gait boundaries, crouch, turns, jump/landing, uneven ground, LOD changes, and rapid input reversals.
- For replicated changes, test a listen server with at least two clients, simulated proxies, correction scenarios, and late join. Value-only simulations are regression coverage, not proof of replication, correction, notify delivery, or late join.
- For load-aware movement, test equipment changes while idle and moving, all load thresholds, sprint/gait transitions, prediction correction, simulated proxies, and late join.
- For traversal, test authoritative obstacle validation, activation cost, montage/root-motion handoff, cancellation, collision failure, correction, combat interruption, death/ragdoll, and rollback of the isolated feature.
- For gameplay integration, run montage, notify, root-motion, equipment socket, hit reaction, death, corpse, and ragdoll regressions.
- Inspect Pose Search cost/debug output and profile animation-thread and game-thread cost before tuning by feel alone.
- Record actual results, failed attempts, and remaining limits in the task report and PR; update only the task's roadmap status row and replace the handoff's current state. Never present historical results as freshly executed.

Report which project files and reference assets were inspected, which behavior intentionally follows GASP and which intentionally differs, why each changed responsibility lives in C++, Blueprint, or DataAssets, and which verification actually ran.
