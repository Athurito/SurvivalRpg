# GASP Mover equipment and montage integration

This follow-up extends the accepted `RpgGaspMoverExperience` with the existing
RPG equipment, combat input and GAS montage path. UEFN remains the gameplay and
default visible mesh. The original GASP walk/sprint/movement selection remains
in the working Blueprint and its simulation callbacks.

Accepted in PR #139. The subsequent [health/death/respawn integration](gasp-mover-lifecycle.md)
extends the lifecycle intentionally left out of this stage. The user accepted
the current block behavior and deferred stationary legs during movement with
block held to a later combat/locomotion blending step, alongside CMC.

## Ownership

`URpgEquipmentManagerComponent` owns equipped instances and their replicated
actors/ability grants. `URpgPawnExtensionComponent::FindGameplayMesh` preserves
`ACharacter::GetMesh()` for existing characters and selects the same first
skeletal mesh as GAS for project-composed pawns. Equipment attachment, avatar
trace sockets and optional retargeting use this shared contract.

GAS remains responsible for montage playback and replication. The existing
primary sword animation uses root motion, so Mover additionally requires an
opt-in bridge from that exact GAS montage instance into its movement simulation.
The integration must not start the montage a second time through the sample
PlayMoverMontage proxy. The opt-in `URpgCharacterMoverComponent`, its
`FRpgMoverAbilityRootMotion` layer and `FRpgMoverAbilityRootMotionInputs`
history entry are the new native class and two simulation structures for this
engine-facing boundary. Animation assets and tuning remain unchanged.

The owner records its locally approved playback interval in Network Prediction's
existing input history. Simulation reconstructs a contribution for each tick,
so rollback can repeat starts and cancellation boundaries without playing the
montage again. A strong local reference keeps the montage alive for the lifetime
of its retained input frames. The input's network serializer carries no playback
payload; authority uses its own GAS instance, never a client-selected montage,
play rate or displacement. Simulated proxies receive montage playback through
GAS and movement through Mover. Executed validation is recorded below.

The existing editor-only Blueprint tools also gain an operation to change an
owned SCS component to a derived class while preserving its authored state and
references. The engine's `ChangeSubobjectClass` only supports native components,
and MCP's component add/remove operations cannot preserve this existing SCS
contract on their own. No new editor tool class or runtime content class is
introduced for asset authoring.

The existing runtime-retarget component now supports project-composed APawn
owners. Its checks still ensure that the cosmetic follower never replaces the
gameplay mesh used by GAS. Source visibility changes only after a valid target
pose; clearing or rejecting a profile restores the source. The existing CMC
retarget AnimBP is reusable because it obtains the component from its owning
actor rather than casting to a Character.

## Asset composition

The Experience enables `GF_Combat_Core`, which provides its existing
PlayerState attributes/ability sets, controller starter equipment and combat
input configuration. The existing RPG UI layout and input actions are composed
into this Experience. This activates the feature's content; it does not by
itself implement the Mover-specific health/death/respawn lifecycle.

New packages under `/Game/SurvivalRpg/Characters/GASP/Mover/RPG`:

| Package | Source and adaptation |
| --- | --- |
| `Input/DA_InputConfig_GaspMover` | Copy of the RPG input config, retaining UI/action-bar and interaction entries, with native movement bindings removed. |
| `Retargeting/DA_RpgGaspMover_RuntimeRetarget` | Copy of the existing optional profile, retaining its retarget AnimBP with no target mesh or retargeter selected. |

The existing Mover PawnData selects these assets and the existing default
AbilitySet. The working pawn adds the runtime-retarget component. Its Mover
component keeps its authored settings when adopting the gameplay-compatible
native subclass. CMC assets and imported sample originals are preserved.

The Mover mapping removes only its right-mouse Aim binding so the RPG combat
mapping can own right-mouse Block. Walk, sprint, jump, crouch, movement and look
bindings retain their original actions, modifiers and triggers. Gamepad Aim
remains as authored. No global Network Prediction setting changes.

The existing shared `/GF_Combat_Core/Input/IA_Block` had a Pressed trigger.
With the common GAS input binding (`Triggered`/`Completed`), that completed
one tick after pressing even while RMB remained down and ended block before
its loop began. Removing that trigger gives the Boolean action continuous
Down behavior and completion on physical release. This one combat asset
correction also applies to existing CMC users of the same input; their
movement configuration and animation assets are unchanged.

The reused combat dependency closure still contains five older import targets:
the sword/shield item icons, sword/shield animation preview Manny, and the
unarmed montage's preview Manny and editable animation. Their eight referring
packages and the two icon packages match the baseline hashes; the three older
preview/editable-animation targets are already absent from the baseline and
remain unresolved. This step introduces no reference
back to a GASP import folder and does not migrate unrelated combat content.

## Playtest

Use `Lvl_RpgGaspMover`, initially as a listen server and then with a remote
client. The isolated test GameMode keeps disk persistence disabled. The
starter loadout supplies the existing sword/shield through the RPG inventory
and equipment path.

- Move/look with WASD/mouse; Shift sprints, Ctrl walks, C crouches, Space jumps.
- Left mouse starts the existing primary sword attack; check both its animation
  and the movement produced by root motion, while stationary and while moving.
- Hold right mouse to block; releasing it plays the existing block exit.
- Check equipment attachment from host and client, including a later joiner.
- The optional retarget profile keeps UEFN visible until a compatible target and
  retargeter are explicitly configured. Montages and gameplay sockets stay on
  the source mesh when a cosmetic target is enabled.

Health/death, respawn, Downed/Revive, Mover traversal and Mover ragdoll are later
integration slices. General multi-section combo authoring is also outside this
change; the existing BasicWeaponAttack montage/attack-window contract remains.

## Validation

The Development Editor build succeeded on UE 5.8.2. All 46 distinct tests
passed: seven Mover tests and 39 existing camera, CMC, traversal, retarget and
remote-melee tests. The final block-input fix passed its hold/release test,
followed by a successful CMC remote-movement/equipment/montage recheck.
Evidence and the consolidated results are recorded under
`Saved/GaspMoverGameplay20260913` in `verified-results.json`.

Twelve asset packages loaded and four Blueprints compiled. The baseline hash
comparison checks 7,621 existing asset/plugin files at `9124c647`: only the
four intended Mover packages and shared `IA_Block` changed. The two new
packages are listed above. All seven existing save files remain unchanged,
with no new save files. The final listen-server sight check shows the UEFN
pawn, starter equipment and RPG HUD in the approved GASP map presentation.
PIE ended cleanly, its prior settings were restored, and no packages were dirty.

The component comparison preserves all 49 reflected fields and nine owned movement-mode,
settings and transition objects. The graph comparison preserves 36 graphs,
898 nodes, 2,708 visible pins and 1,940 link endpoints. Its only 43 pin changes
are the intended component type updates; defaults and connections match.
Hidden pins and bytecode are outside that snapshot format's coverage.

The PIE tests cover natural starter equipment, input-triggered sword
montages with capsule root motion, server trace sampling, interruption and
replay, late-joined equipment observers, and optional Manny pose retargeting
with source-mesh preservation and fallback. Holding RMB maintains the shield
block loop on owner, server and observer; releasing it exits cleanly without
creating a root-motion layer.

The correction test introduces a one-time 50 cm lateral error in the owning
client's capsule and pending Mover state under 150 ms simulated network delay,
then cancels an attack and immediately requests the same montage again.
It observes real reconciliation before the next forward tick, requiring
unchanged simulation frame/time and preservation of the new activation and
montage instance. The previous implementation failed that assertion; the
input-history implementation passes. This tests the Independent game-thread
backend, not Chaos physics, extrapolated proxies or montage section jumps.

Mover attack coverage verifies server trace sampling and authored
windows; the existing CMC melee test additionally verifies damage. The tests
predate the [Mover health/death/respawn integration](gasp-mover-lifecycle.md). Network tests ran in
single-process PIE; separate-process and packaged multiplayer remain untested.
Existing transient-level NetGUID, voice-interface and Independent interpolation
warnings occurred, especially during joins and simulated latency. No
warning-free result or editor undo/redo verification is claimed.
