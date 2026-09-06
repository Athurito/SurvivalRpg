# GASP-Lyra Integration Contract

Use this contract for later Lyra adaptation when a task crosses SurvivalRpg animation, GAS combat, equipment, character lifecycle, or networking. Archive/removal is the current stage; the user plans to migrate original GASP Blueprints manually before that adaptation. The previous native port is retained on `codex/archive-gasp-native-port-2026-09-06` (`ec8bef45`).

## Canonical sources

Use sources in this order:

1. Current SurvivalRpg runtime code, assets, tests, and plugin contracts.
2. The currently requested migration stage and its documentation; `docs/gasp-cmc-migration-plan.md` and the old plugin contracts are historical archive sources, not the adopted runtime boundary.
3. `D:\Repos\GameAnimationSample` for version-matched GASP comparison when present.
4. `D:\Repos\LyraStarterGame` for Lyra lifecycle and composition comparison when present.

Never let either sample project override a deliberate SurvivalRpg adaptation without evidence and explicit scope.

## Ownership matrix

| Concern | Primary owner | Required collaborator | Contract |
| --- | --- | --- | --- |
| Locomotion pose selection and presentation | GASP adaptation | Lyra for lifecycle/network inputs | Keep cosmetic state outside gameplay authority. |
| AnimGraph, Pose Search, trajectory, procedural nodes | GASP adaptation | Combat when montage layers overlap | Preserve thread-safe snapshots and montage slots. |
| Native animation integration, when justified | The inspected adaptation seam; no required archived GASP parent | AnimBP/DataAssets for presentation | Keep C++ narrow: snapshots, engine-facing hooks, world-query handoff, and justified pure resolvers rather than one growing state owner. |
| Presentation composition and tuning | AnimBP, Choosers, profiles, and DataAssets | Native integration for safe inputs | Keep blend feel, database membership/references, masks, and cosmetic thresholds visible and data-driven by default. |
| Pawn class and pawn configuration | Lyra/PawnData | GASP for AnimBP requirements | Compose through PawnData and Experience rather than a sample-owned character path. |
| Mode and feature composition | Lyra Experiences/Game Features | GASP for required assets/plugins | Keep activation data-driven and dependencies narrow. |
| Abilities, costs, cooldowns, blockers, combat tags | GAS/Combat | GASP for read-only animation gates | Snapshot tags for animation; never infer authoritative gameplay from a selected pose. |
| Combat montages and attack windows | GAS/Combat | GASP and Lyra | Preserve slot, root-motion, notify, prediction, and cancellation semantics. |
| Equipment and attachment | RPG equipment architecture | GASP for skeleton/socket compatibility | Equipment owns truth; animation only presents it. |
| Load-aware gait, sprint, and heavy movement | Equipment/GAS and CharacterMovement | GASP for animation selection | Drive gameplay and visuals from one authoritative movement profile and reconstruct it for simulated proxies. |
| Movement and network truth | Character movement/Lyra gameplay path | GASP for selection inputs | Replicate or reconstruct inputs needed by simulated proxies and late joiners. |
| Mantle, vault, hurdle, and short climb | GAS and CharacterMovement | GASP for curated montages; Lyra for composition | Validate traversal on the server and keep the action project-owned. |
| Sustained climb, ledge hang, and ladders | CharacterMovement custom mode | GAS/Lyra for activation; GASP for presentation | Keep continuous movement authority outside the AnimBP. |
| Death, corpse, and ragdoll | Character/combat lifecycle | GASP for AnimGraph and mesh handoff | Keep `GetMesh()` and physics transitions compatible. |
| Migrated animation content | The approved project content boundary | Lyra only when composition changes | Preserve the imported source baseline; runtime must not depend on the external sample checkout. |

## Seams to verify during later Lyra adaptation

Inspect these contracts against the migrated Blueprint baseline and existing Lyra gameplay; do not assume the archived GASP implementation is present:

- Verify the migrated AnimBP's parent and introduce native integration only for a demonstrated need; do not require the archived `URpgAnimInstance` GASP coordinator.
- The AnimInstance proxy captures gameplay and movement inputs on the game thread.
- Adapt production character selection through PawnData and an Experience after the isolated source Blueprint baseline is available.
- `GetMesh()` remains the montage, notify, equipment socket, corpse, and ragdoll mesh.
- `DefaultSlot` remains available to GAS combat montages.
- Curated sequences preserve their authored root-motion import settings while the AnimInstance extraction policy remains root motion from montages only.
- Preserve the migrated Blueprint/Chooser selection baseline until an explicit adaptation changes it. Project ownership does not require native selection or restoration of the old resolvers.
- Inspect the chosen content boundary separately from PawnData, Experience, and AnimBP composition; the removed content plugin is not a prerequisite.
- Equipment/GAS and CharacterMovement own load and traversal state; the AnimInstance receives only animation-safe snapshots.

## Cross-system decision rules

- If a change affects only source parity, retargeted content, Pose Search metadata, or cosmetic graph tuning, let `$unreal-gasp-expert` lead.
- If a change is presentation-only, prefer AnimBP, Chooser, profile, or DataAsset ownership unless measured performance, engine integration, thread safety, or durable deterministic testing justifies a focused native resolver.
- If it affects PawnData, Experience activation, character class, movement replication, ASC access, montage lifecycle, death, or ragdoll, use `$unreal-gasp-expert` with `$unreal-lyra-expert`.
- If it affects attacks, dodge, block, hit reactions, combat tags, montage notifies, equipment grants, or damage windows, add `$survival-rpg-combat-foundation`.
- If it affects load-aware movement, sprint, stamina, traversal abilities, or equipment-driven gait, use `$unreal-gasp-expert` with `$unreal-lyra-expert` and `$survival-rpg-combat-foundation`.
- If it proposes Mover, Traversal, camera, Foley, or a wider movement fantasy, require an isolated product and dependency evaluation before implementation.

## Integration invariants

- Do not query UObject gameplay state from worker-thread animation update.
- Do not make combat success, hit detection, stamina use, or movement authority depend on cosmetic pose selection.
- Do not replace production Experience/PawnData composition with the GASP sample character hierarchy; a separately requested isolated source Blueprint baseline may precede that integration.
- Do not replace RPG equipment truth with AnimBP variables or socket state.
- Do not remove montage slots, gameplay notifies, or root-motion behavior while simplifying locomotion graphs.
- Do not create runtime references to the external sample checkout or import dependencies beyond the approved Blueprint migration scope.
- Do not add Mover, Traversal, Locomotor, NetworkPrediction, sample camera, or Foley as incidental dependencies.
- Do not add another presentation state machine, watchdog, database switch, or package-path classifier to a native animation coordinator before evaluating a focused helper/runtime split and an AnimBP/Chooser/DataAsset alternative.
- Do not move complex native behavior one-to-one into Blueprint; simplify and separate the responsibility first.
- Do not claim multiplayer completion from value-only unit tests. Verify authority, autonomous proxy, simulated proxy, correction, notify delivery, and late join in an actual network session when the change crosses those seams.

## Verification matrix

| Change | Minimum checks |
| --- | --- |
| Asset membership or metadata | Manifest/README update, content contract test, reference-closure audit, LFS diff review |
| Pose Search or AnimGraph | Asset contract tests, AnimBP compile, database/schema validation, locomotion play test |
| Threading or procedural animation | Threading and focused runtime tests, no worker-thread gameplay queries, LOD/performance check |
| Movement input or rotation mode | Authority/autonomous/simulated-proxy test, correction behavior, late join |
| Equipment load or sprint profile | Threshold and gear-change tests, movement/animation agreement, simulated proxies, correction, late join |
| Curated mantle/vault/climb | Server obstacle validation, GAS cost/cancel, CMC/montage handoff, collision failure, correction, combat/death interruption, removable isolated pilot |
| GAS montage or combat layer | Slot/root-motion/notifies, cancel/block behavior, attack windows, hit reactions |
| Equipment, death, or ragdoll | Socket attachment, mesh ownership, death transition, corpse physics, late join where relevant |
| New sample subsystem | Dependency closure, ownership/lifecycle design, isolated pilot, rollback path, multiplayer acceptance |
| C++/Blueprint/DataAsset boundary or native refactor | Responsibility map, behavior-preserving focused tests, AnimBP/data validation, build after native changes, and real network checks when replicated seams are touched |
