# GASP CMC migration plan

## Goal

Adopt the useful CMC locomotion and Motion Matching patterns from Epic's Game Animation Sample Project (GASP) without replacing SurvivalRpg's Lyra-derived character, GAS montage, equipment, ragdoll, PawnData, or Experience architecture.

The migration uses an isolated pilot before changing the default player character. The stock GASP character and AnimBP remain reference material rather than runtime dependencies.

## Why the sample is not migrated wholesale

An Unreal Asset Registry audit against the UE 5.8 sample found these approximate recursive closures across hard, soft, and management references:

| GASP root | Packages | Size |
| --- | ---: | ---: |
| `SandboxCharacter_CMC_ABP` | 1,000+ | about 1.0 GiB |
| `SandboxCharacter_CMC` | 2,500+ | about 4.2 GiB |
| `BP_Manny` | 470+ | about 2.2 GiB |

The CMC AnimBP closure still pulls in experimental state-machine databases, large Foley sets, and generic retarget assets. The broader character closure adds Mover, traversal, and sample camera dependencies. Normalization sets and Pose Search branch-in notifies expand the closure further. Copying that graph would therefore import unrelated sample architecture and make future upgrades difficult.

## Migration slices

### 1. Native animation and network foundation

- Make `URpgAnimInstance` the exported base class for player AnimBPs.
- Snapshot character and movement state on the game thread through an AnimInstance proxy.
- Consume only snapshot values during the worker-thread animation update.
- Replicate compressed acceleration to simulated proxies so remote starts, stops, and pivots have the same inputs as the owning client.
- Reparent the current `ABP_Unarmed` to the RPG base without changing its pose graph.

### 2. Curated locomotion asset set

- Enable `PoseSearch` for the project-local graph; its engine dependencies provide Chooser, BlendStack, Motion/Animation Warping, and RigVM. Enable `IKRig` for editor retargeting, and add `AnimationLocomotionLibrary` only if the final graph actually calls it.
- Select a small CMC locomotion set for stand, walk, run, sprint, start, stop, pivot, jump, and land. Add crouch as a later slice instead of pulling it into the first pilot.
- Retarget selected source animations to SurvivalRpg's authoritative player skeleton.
- Create project-owned schemas, normalization data, databases, and Choosers. Do not migrate sample databases or generic retarget collections.
- Strip sample Foley, traversal, Mover, experimental state-machine, camera, and character Blueprint dependencies.

### 3. Isolated GASP pilot

- Add `BP_Rpg_Character_GASP`, `DA_PawnData_GASP`, and `RpgGaspPilotExperience`.
- Derive the pilot AnimBP from `URpgAnimInstance`.
- Preserve controller-facing strafe rotation, `DefaultSlot`, and root motion from montages only.
- Keep `GetMesh()` authoritative for GAS montages, notifies, equipment sockets, corpse physics, and ragdoll.
- Select the pilot through the Experience override until acceptance checks pass.

### 4. Integration and cutover

- Verify locomotion transitions, foot placement, LOD cost, and database memory.
- Verify attack combos, block, dodge, hit reactions, death, equipment sockets, and harvesting montage rewards.
- Test listen server plus two clients, simulated proxies, and late join.
- Replace the default PawnData character only after the old and pilot paths pass the same checks.
- Remove the old AnimBP only after the cutover is proven and reversible in version control.

## Acceptance criteria

- No runtime dependency on GASP's character Blueprint, Mover graph, traversal graph, generic retarget collection, or sample camera stack.
- The AnimBP update path performs no character, component, ASC, or world queries on worker threads.
- Simulated proxies receive acceleration needed for start, stop, and pivot selection, including after late join.
- Existing GAS montages continue to use `DefaultSlot` and retain their gameplay notifies and root-motion behavior.
- The pilot is selected through PawnData and an Experience; the default experience is not overwritten during development.
- The asset diff and Git LFS payload are reviewed before the curated animation slice is committed.
