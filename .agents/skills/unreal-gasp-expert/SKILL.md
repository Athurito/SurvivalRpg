---
name: unreal-gasp-expert
description: Use for Unreal Engine 5 Game Animation Sample Project (GASP) CMC locomotion, RPG movement, equipment-load-aware gait and sprint profiles, curated mantle/vault/climb/traversal, Motion Matching, Pose Search schemas/databases, trajectory generation, Blend Stack, Steering, Offset Root Bone, Foot Placement/IK, retargeting, animation-thread safety, multiplayer locomotion parity, GASP C++/Blueprint/DataAsset ownership, URpgAnimInstance refactoring, and integration with SurvivalRpg's Lyra-derived PawnData, Experiences, GAS montages, equipment, death, or ragdoll. Inspect the project and local GASP reference first; preserve project-owned curated content and pair with $unreal-lyra-expert at gameplay, composition, or character-lifecycle boundaries.
---

# Unreal GASP Expert

Treat GASP as a technical reference for a project-owned animation system. Preserve SurvivalRpg's curated CMC path and integrate it through the Lyra-derived gameplay architecture instead of importing the sample wholesale.

## Start from current repository truth

Inspect the affected project and reference assets before proposing or making changes.

1. Read `SurvivalRpg.uproject` to confirm the engine version and enabled animation plugins.
2. Read `docs/gasp-cmc-migration-plan.md` and `Plugins/RpgGaspLocomotion/Docs/README.md`.
3. Read `Plugins/RpgGaspLocomotion/RpgGaspLocomotion.uplugin` and, for content changes, `Plugins/RpgGaspLocomotion/Docs/CuratedAssetManifest.csv`.
4. Inspect `Source/SurvivalRpg/Animation`, the affected character/movement code, the active GASP AnimBP/PawnData/Experience, and the focused tests under `Source/SurvivalRpgEditor/Private/Animation`.
5. Compare with `D:\Repos\GameAnimationSample` when available. Treat it as a version-matched comparison source, never as a required runtime dependency.
6. Compare with `D:\Repos\LyraStarterGame` when a change crosses into PawnData, Experience, GAS, character lifecycle, equipment, death, or ragdoll.

If either reference project is unavailable, continue from repository truth and state that the comparison could not be performed.

Read [references/gasp-lyra-integration.md](references/gasp-lyra-integration.md) whenever a task crosses animation, gameplay, composition, combat, equipment, or networking ownership.

## Classify the work before changing it

Place the request in one or more concrete categories:

- source-parity audit against GASP
- curated animation import or retargeting
- Pose Search schema, database, normalization, or chooser work
- AnimGraph, trajectory, Motion Matching, Blend Stack, Steering, Offset Root Bone, or Foot Placement work
- starts, stops, pivots, turn-in-place, gait, crouch, jump, or landing debugging
- equipment-load-aware locomotion, heavy running, sprint, mantle, vault, hurdle, climb, or ledge movement
- animation threading, replication, simulated-proxy, or late-join debugging
- Lyra, GAS montage, combat, equipment, death, or ragdoll integration

Choose the smallest slice that proves the intended behavior. Separate observed GASP behavior, current SurvivalRpg behavior, and the proposed adaptation.

## Choose the implementation boundary explicitly

Treat the Epic GASP project as Blueprint-authored project glue built on native Engine animation systems. Its lack of project C++ proves neither that a production Lyra integration should stay Blueprint-only nor that all adaptation logic belongs in native code.

Before implementing a slice, state which responsibility belongs in C++, Blueprint, or data assets and why.

- Keep authority, replication, CharacterMovement truth, server-validated traversal, game-thread-to-worker snapshots, world queries, engine-facing custom AnimNodes, and narrowly scoped durable pure resolvers in C++.
- Keep AnimGraph composition, layer/mask wiring, blend and warping feel, database membership, animation references, character/profile tuning, and presentation-only configuration in AnimBPs, Choosers, or DataAssets by default.
- Keep gameplay state in GAS, equipment, CharacterMovement, and Lyra lifecycle systems. Let animation consume only read-only presentation inputs.
- Require a concrete threading, networking, engine-integration, performance, or deterministic-testability reason before moving cosmetic selection or tuning into C++.
- Do not translate a complex native state machine one-to-one into Blueprint. Reduce and separate responsibilities first, then place each remaining part at its natural boundary.

Treat `URpgAnimInstance` as a narrow integration coordinator, not the default owner of every GASP rule.

- Before adding another enum, state machine, watchdog, database switch, or asset classifier, inspect whether an existing responsibility should be split into a focused value-only runtime/helper or moved into a profile, Chooser, AnimBP, or DataAsset.
- Prefer configured roles, tags, or explicit profile membership over hard-coded asset package-path classification.
- Avoid duplicating the same database contract across enums, fixed arrays, switch statements, validation, and tests when one data-driven contract can own it.
- Preserve a concise mapping from relevant source GASP Blueprint/Chooser behavior to the SurvivalRpg C++/Blueprint/DataAsset seam so the adaptation remains learnable.

## Preserve the project-owned CMC architecture

- Treat the current CMC/Pose Search path as the production default unless the user explicitly requests a replacement evaluation.
- Keep the stock GASP character, AnimBP, databases, and supporting systems as reference material rather than runtime dependencies.
- Do not introduce GASP's full Mover/Traversal stack, Locomotor, NetworkPrediction, sample camera, Foley, audio, experimental state machines, generic retarget collections, or dense sample content merely because the sample contains them.
- Require an isolated dependency and lifecycle evaluation before adopting any excluded sample subsystem.
- Preserve the authoritative project skeleton, the native `URpgAnimInstance`/proxy integration seam, project-local schemas/databases, and the `RpgGaspLocomotion` content boundary.
- Inspect current runtime selection before assuming the sample's Chooser or State Controller should own selection. Preserve project-native selection where it is intentional without treating that choice as permission to hard-code presentation tuning or grow a monolithic AnimInstance.
- Record intentional source deviations in the plugin contract, manifest, or focused tests rather than relying on memory or issue-specific scripts.

## Shape RPG locomotion and curated traversal

- Build grounded dark-fantasy movement that can communicate equipment load, stamina, combat state, terrain, and learned traversal without becoming constant survival friction or superhero parkour.
- Treat the existing `Equipment.Load.Light`, `Equipment.Load.Medium`, and `Equipment.Load.Heavy` state as gameplay input. Keep equipment/GAS and CharacterMovement authoritative; let GASP presentation consume a thread-safe snapshot.
- Define load-aware movement as a data-driven profile. Consider maximum speed, acceleration, braking, rotation response, jump, dodge, stamina, gait, and Pose Search selection, but apply only the differences that improve feel and readability.
- Do not implement heavy running only by slowing animation playback. Keep movement physics, gameplay costs, gait selection, and visual weight aligned to the same authoritative profile.
- Replicate or reconstruct the animation-relevant movement profile for simulated proxies. Do not assume owner-only equipment load state is sufficient for remote locomotion or late join.
- Inspect the current gait resolver before adding sprint. Add an explicit gameplay-owned sprint/gait state when the project needs true sprint instead of inferring it from a locally selected pose or speed alone.
- Treat mantle, vault, hurdle, and short climb as curated RPG traversal actions. Prefer server-validated GAS abilities using the existing CMC path, project-owned montages, and Motion Warping where appropriate.
- Treat sustained climbing, ledge hanging, ladders, or wall movement as explicit CharacterMovement custom modes or equally authoritative movement state. Do not implement sustained traversal only inside the AnimBP.
- Retarget only the GASP sequences and metadata required by the approved action. Replace sample Blueprint helpers, BranchIn dependencies, camera logic, and traversal state machines with project-owned seams.
- Start with one isolated traversal action, such as a forward standing mantle, in separate PawnData/Experience or a disposable development GameFeature. Prove collision validation, cancellation, multiplayer correction, and rollback before widening the move set.
- Preserve combat, equipment, montage-slot, root-motion, hit-reaction, death, and ragdoll behavior while traversal is active or interrupted.
- Evaluate Mover only as a separate movement-stack replacement study when CMC demonstrably cannot provide a required capability. Do not make Mover a prerequisite for curated traversal.

## Keep animation updates thread-safe

- Gather character, movement, ASC, component, and world state on the game thread.
- Transfer only immutable or snapshot-safe values through the AnimInstance proxy.
- Consume snapshot values during worker-thread animation update; do not query actors, components, the ASC, or the world there, and do not perform dynamic asset loading or mutable UObject work.
- Snapshot gameplay tags or gameplay-derived gates on the game thread before animation consumes them.
- Keep Motion Matching selection and procedural animation cosmetic. Never make authoritative gameplay outcomes depend on the locally selected pose.
- Preserve parallel animation update for non-GASP AnimBPs and avoid adding game-thread work when the feature is disabled.
- Keep game-thread snapshots focused. Do not move presentation-only state into the proxy merely to avoid a clean AnimBP, Chooser, or DataAsset boundary.

## Preserve source fidelity without importing source coupling

For asset or Pose Search changes:

- Audit the recursive dependency closure before migration.
- Import only the animations and metadata required by the approved slice.
- Retarget to the project skeleton and keep assets project-owned.
- Preserve root-motion enablement, normalization, force-lock, and reference-pose locking where required by the current import contract.
- Preserve curve spelling and case, including source variants, unless a documented project normalization replaces them.
- Preserve required sampling ranges, exclusion/transition/cost notifies, mirroring, database membership, schema channels, normalization, tags, and cost biases.
- Remove Foley, EarlyTransition, BranchIn, state-machine, or other excluded references only with an explicit local replacement policy.
- Update the curated manifest and plugin contract when asset membership or source parity changes.
- Review binary/LFS scope and hard/soft/management references before accepting an asset diff.

Do not treat on-disk compression size as the content contract when Unreal can recompress assets with project or engine defaults.

## Preserve multiplayer and gameplay seams

- Verify autonomous proxy, authority, and simulated proxy inputs separately.
- Preserve the replicated movement inputs needed for remote starts, stops, pivots, gait, rotation modes, turn-in-place, jump, and landing selection.
- Verify reconstruction after late join and after movement correction; do not rely only on steady-state local play.
- Keep gameplay authority in movement, GAS, equipment, and character systems. Keep pose selection, foot placement, and presentation state cosmetic.
- Preserve `GetMesh()` as the mesh used by GAS montages, notifies, equipment sockets, corpse physics, and ragdoll unless the project establishes a different authoritative seam.
- Preserve the `DefaultSlot` montage path and root motion from montages only unless a reviewed gameplay requirement changes that contract.
- Recheck attack combos, block, dodge, hit reactions, harvesting rewards, death, equipment attachment, and ragdoll after AnimGraph or skeleton changes.

## Route across specialist skills

- Use `$unreal-lyra-expert` together with this skill for PawnData, Experiences, Game Features, character classes, movement replication, GAS integration, montage lifecycle, equipment, death, or ragdoll.
- Add `$survival-rpg-combat-foundation` when the task changes attacks, block, dodge, hit reactions, combat montages, equipment-granted abilities, or combat animation tags.
- Add `$survival-rpg-project` only when an animation decision changes product scope, combat feel, traversal scope, progression identity, or first-playable priorities.
- Keep isolated source audits, retargeting checks, Pose Search tuning, and purely cosmetic AnimGraph work within this skill when no gameplay boundary is affected.

Do not duplicate Lyra, combat, or product authority inside this skill. Use the integration contract to decide which specialist owns each decision.

## Verify proportionally

Use the narrowest verification that proves the change, then widen when the boundary demands it.

- Run the relevant editor build after C++ or module dependency changes; never claim compilation without running it.
- Run focused automation filters such as `SurvivalRpg.Animation.Gasp`, `SurvivalRpg.Animation.MotionMatching`, `SurvivalRpg.Animation.FootPlacement`, `SurvivalRpg.Animation.TurnInPlace`, `SurvivalRpg.Animation.Jump`, `SurvivalRpg.Animation.Network`, or `SurvivalRpg.Animation.Threading` as applicable.
- Validate affected assets, AnimBP compilation, parent class, skeleton, exposed defaults, graph nodes, reference closure, and cook/load assumptions.
- Test locomotion in editor for starts, stops, pivots, gait boundaries, crouch, turns, jump/landing, uneven ground, LOD changes, and rapid input reversals.
- For replicated changes, test a listen server with at least two clients, simulated proxies, correction scenarios, and late join.
- Treat value-only/unit simulations as regression coverage, not as proof of real replication, correction, notify delivery, or late-join behavior.
- For load-aware movement, test equipment changes while idle and moving, all load thresholds, sprint/gait transitions, prediction correction, simulated proxies, and late join.
- For traversal, test authoritative obstacle validation, activation cost, montage/root-motion handoff, cancellation, collision failure, correction, combat interruption, death/ragdoll, and rollback of the isolated feature.
- For gameplay integration, run montage, notify, root-motion, equipment socket, hit reaction, death, corpse, and ragdoll regressions.
- Inspect Pose Search cost/debug output and profile animation-thread/game-thread cost before tuning by feel alone.

Report which project files and reference assets were inspected, which behavior intentionally follows GASP, which behavior intentionally differs, why each changed responsibility lives in C++/Blueprint/DataAssets, and which verification actually ran.
