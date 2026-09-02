# GASP CMC migration plan

## Goal

Adopt the useful CMC locomotion and Motion Matching patterns from Epic's Game Animation Sample Project (GASP) without replacing SurvivalRpg's Lyra-derived character, GAS montage, equipment, ragdoll, PawnData, or Experience architecture.

The migration was prepared through an isolated pilot. This cutover stages the project-owned GASP
Experience as the global fallback while retaining the Prototype Experience as a separate path. The
stock GASP character and AnimBP remain reference material rather than runtime dependencies.

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
- For standing, uncrouched ground movement, resolve a physical deadzone of `<= 0.10` before physics and SavedMove capture; preserve every magnitude above it, including `0.11`, `0.25`, and `0.50`, without renormalization beyond CharacterMovement's native `NetQuantize10` precision.
- Keep desired gait prediction-owned: enter Run at `>= 0.70`, retain it at `>= 0.65`, and exit only below `0.65`. Store that state in one SavedMove `Custom0` flag and restore it for authoritative server movement and client replay.
- Let simulated proxies reconstruct active-input gait from replicated acceleration. During inputless Walk/Run coast, replicate only the authority's current coast classification to simulated proxies so initial replication and relevancy return preserve the selected gait below the Walk cap; clear it deterministically at physical stop without replicating AnimBP, pose, or movement history. A proxy first observed with active input directly inside the `0.65`-to-`0.70` retention band remains a separate ambiguity outside issue #101.
- Preserve the legacy input response for crouching, falling, and custom movement modes.
- Let a PawnData-selected movement profile own GASP-pilot Walk/Run caps, response values, and the CMC gait consumed by animation; legacy PawnData remains opt-out.
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
- Preserve PawnData-selected Free/Combat/Aim rotation, combat-tag overrides, `DefaultSlot`, and root motion from montages only.
- Keep `GetMesh()` authoritative for GAS montages, notifies, equipment sockets, corpse physics, and ragdoll.
- Select the pilot and prototype through their separate Experiences during acceptance; after cutover, keep both paths directly selectable.

### 4. Integration and cutover

- Verify locomotion transitions, foot placement, LOD cost, and database memory.
- Verify attack combos, block, dodge, hit reactions, death, equipment sockets, and harvesting montage rewards.
- Test a listen server with an autonomous owner and at least two observing clients, including simulated proxies, correction, Walk/Run coast late join, physical-stop cleanup, and actor-channel relevancy loss/return.
- Keep the prototype and GASP PawnData/AnimBP paths cooked, separate, and directly selectable through their Experiences in the same build.
- The staged default cutover changes only the global fallback to `RpgGaspPilotExperience`; it does not repoint the prototype Experience or delete its PawnData/AnimBP. Merge and acceptance remain gated by the visual A/B and real multi-process checks in #99 and #55.
- Switch Experiences only at a supported world/session boundary (World Settings, URL, PIE, or command line), never by hot-swapping an already-running pawn.

The selection order remains URL `?Experience=`, PIE `ExperienceOverride`, command line
`-Experience=`, per-map World Settings, then the global GASP fallback. Selecting
`RpgPrototypeExperience` through any explicit seam therefore remains the immediate rollback and A/B
path intended for the same cooked build. PIE currently proves the explicit DeveloperSettings path;
packaged URL, command-line, and World Settings selection remain cutover gates. `DefaultPawnData`
remains the technical emergency fallback and is not the default Experience selector.

## Acceptance criteria

- No runtime dependency on GASP's character Blueprint, Mover graph, traversal graph, generic retarget collection, or sample camera stack.
- The AnimBP update path performs no character, component, ASC, or world queries on worker threads.
- Simulated proxies reconstruct active-input gait from acceleration needed for start, stop, and pivot selection. A server-owned Walk/Run coast classification seeds only inputless simulated-proxy coast after late join or relevancy return, remains stable below the Walk cap, and clears to Idle at physical stop without replicating presentation history.
- Existing GAS montages continue to use `DefaultSlot` and retain their gameplay notifies and root-motion behavior.
- Walk/Run speed caps, response values, controlled Free yaw, and stable gait are selected by GASP PawnData and applied by CharacterMovement before animation consumes them.
- Standing-ground input at or below `0.10` produces no physical movement; `0.11`, `0.25`, and `0.50` remain analog-scaled without renormalization beyond native movement-network precision, while crouching, falling, and custom movement retain their legacy response.
- Run enters at input `>= 0.70`, remains selected at `>= 0.65`, and exits only below `0.65`; one SavedMove `Custom0` flag restores the prediction-owned gait on the server and during replay.
- Fixed-speed gamepad normalization and directional strafe caps remain separate gameplay contracts.
- `RpgPrototypeExperience` and `RpgGaspPilotExperience` remain independently selectable in the same build; neither path overwrites the other's PawnData or AnimBP.
- The asset diff and Git LFS payload are reviewed before the curated animation slice is committed.
