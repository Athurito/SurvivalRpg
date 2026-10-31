# GASP Mover foundation

The Mover variant's base came in three slices: the foundation (PR #138), equipment and montages (PR #139), then health, death and respawn (PR #140). Each section below is the report of one slice. Traversal follows in [Mover Mantle](gasp-mover-mantle.md).

## Foundation

This document records the foundation accepted in PR #138. The subsequent
[equipment/montage integration](#equipment-and-montage-integration) extends its composition;
the original pilot's empty RPG inputs/ability sets describe that earlier stage.

This pilot adds a separate `RpgGaspMoverExperience` using the project-local GASP
Mover Blueprint. It proves the composition boundary for ordinary locomotion,
camera, crouch and jump. The accepted baseline and CMC traversal Experiences
keep their existing movement stack. UEFN remains the visible and animated mesh.

### Ownership and simulation

The single new abstract native pawn, `ARpgMoverPawn`, derives from
`AModularPawn`. It reuses `URpgPawnExtensionComponent` for replicated PawnData
and the PlayerState-owned ASC, and `URpgPawnGameplayComponent` for player
initialization, Experience input contexts and PawnData camera selection.
Possession and replicated Controller/PlayerState changes advance that existing
lifecycle. No second ASC or CharacterMovement component is introduced.

`BP_RpgGasp_Mover` owns the capsule, UEFN mesh, CharacterMover component and
original Blueprint simulation/input producer. `ProduceInput`,
`On_PreSimulateTick`, post-finalize/based-movement callbacks, source gait and
rotation inputs, and mesh tick prerequisites remain in Blueprint. The working
AnimBP retains the source animation graphs. Its two owned chooser copies use
the working AnimBP as context; animation sequences and databases continue to
reuse project-local foundation content.

Mover retains its `MoverNetworkPredictionLiaisonComponent` backend and input
synchronization for simulated proxies. The pawn replicates, but actor
`ReplicateMovement` remains disabled because the backend owns movement state.
Controller yaw does not directly rotate the pawn; body orientation belongs to
Mover while control rotation supplies the view and movement intent.

The original foundation retained Independent ticking in `Config/DefaultNetworkPrediction.ini`:
Interpolated simulated proxies, 100 ms interpolation buffering, 250 ms maximum
Independent buffering and input send count 6. The original kinematic Mover
simulation is retained; this slice does not introduce the sample physics or
ragdoll Mover variants. Interpolation starvation warnings require measurement
of the affected world, role and frame/network timing; no global settings change
is presented as an animation-speed fix. The later measured frame-rate mismatch
and the switch to Fixed ticking with visual smoothing are documented in
[`gasp-mover-fixed-tick.md`](gasp-mover-fixed-tick.md).

### Asset mapping

All paths below are relative to `/Game/SurvivalRpg/`. These are the nine pilot
packages; shared dependencies are reused rather than duplicated again.

| Source or native schema | Pilot package |
| --- | --- |
| `Characters/GASP/Mover/Blueprints/SandboxCharacter_Mover` | `Characters/GASP/Mover/RPG/BP_RpgGasp_Mover` |
| `Characters/GASP/Mover/Blueprints/SandboxCharacter_Mover_ABP` | `Characters/GASP/Mover/RPG/ABP_RpgGasp_Mover` |
| `Characters/GASP/Shared/Input/IMC_Sandbox` | `Characters/GASP/Mover/RPG/Input/IMC_RpgGasp_Mover` |
| `Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Relaxed` | `Characters/GASP/Mover/RPG/Choosers/CHT_RpgGasp_Mover_PoseSearchDatabases` |
| `Characters/GASP/Mover/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_MoverCharacterAnimations_PoseMatch` | `Characters/GASP/Mover/RPG/Choosers/CHT_RpgGasp_Mover_PoseMatch` |
| `Characters/GASP/CMC/RPG/DA_PawnData_GaspCMC` | `Characters/GASP/Mover/RPG/DA_PawnData_GaspMover` |
| New Blueprint from `URpgExperienceDefinition` | `System/Experiences/RpgGaspMoverExperience` |
| Child of `Maps/Test/GaspCMC/BP_Rpg_GaspCMCTestGameMode` | `Maps/Test/GaspMover/BP_Rpg_GaspMoverTestGameMode` |
| `Maps/Test/Lvl_RpgGaspMantle` | `Maps/Test/Lvl_RpgGaspMover` |

The copied PawnData selects the Mover pawn and existing RPG camera mode. Its
native `InputConfig`, ability sets and runtime retarget profile are empty.
This prevents competing RPG movement bindings and keeps combat and optional
retargeting out of the pilot. The external GASP checkout is a comparison source,
not a runtime dependency.

### Intentional sample deviations

The Experience owns its `URpgGameFeatureAction_AddInputContextMapping` instance
and installs the filtered Mover context at priority 1, above the controller's
generic movement/look contexts at priority 0. Only Move, Look,
gamepad Look, Walk, Sprint, Jump, Crouch, Strafe and Aim mappings remain; source
modifiers and triggers are retained. Sample `SetupInput` and `SetupCamera` are
disconnected from the possession callback. The source camera components are
inactive; an `URpgCameraComponent` uses the existing RPG camera selection.

Space preserves the source movement-mode routing and ordinary crouch/jump
branch. Crouching first uncrouches; otherwise `Jump_JustPressed` is set for one
frame for the Mover input producer. Both sample `TryTraversalAction` paths,
including held-key traversal retries, are disconnected. `IA_Traverse` is absent
from the filtered context. The retained `AC_TraversalLogic` has no tick; its
BeginPlay only caches references and does not start an action.

The six raw sample key nodes are removed: mouse wheel and gamepad D-pad
Up/Down no longer change `DDCVar.CameraStyle`, and Z/gamepad left-stick click
no longer toggle Flying. The sample `AC_VisualOverrideManager` is removed:
its BeginPlay would otherwise bind the global visual-override CVar callback.
The empty VisualOverride child component does not select a replacement mesh.
The actor-tick call to `Update_TwinStickMode` is bypassed with subsequent tick
work preserved, and `TwinStickMode` stays false. Camera-style CVars therefore
do not enable the sample twin-stick control scheme.

The retained SmartObject animation component only caches mesh/movement
references at BeginPlay. Sample interaction is not exposed by this context.
Foley, NavMover and the source movement/presentation update chain are retained.

### Playtest

Open `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover` with no PIE Experience
override. Its WorldSettings select the new Experience and isolated test
GameMode. Disk persistence is disabled; save-slot names and offline identity
are separate from production.

The map reuses the approved Mantle test presentation: project-local GASP
blocks and floor/grid materials with matching LevelVisuals lighting, skylight,
fog and exposure. The inherited obstacles provide collision geometry, but
Space does not mantle, vault or hurdle in this Experience.

| Input | Behavior |
| --- | --- |
| W/A/S/D | Move relative to the view |
| Mouse | Look |
| Space | Ordinary Mover jump, or uncrouch first |
| C | Toggle crouch |
| Left Ctrl | Toggle walk |
| Left Shift | Sprint |

Check starts, stops, turns, sustained running, slope/collision response,
crouch/uncrouch and jump/landing first with one listen-server player, then with
a listen-server host and remote client. Observe both directions: the host
viewing the remote pawn and the client viewing the host. Add another client
after movement has begun and check its initial pose, position and subsequent
movement. Camera turning must update the view and movement direction without
forcing body yaw directly. Compare animation continuity and Foley during
sustained movement, not only the first few steps.

Equipment, combat abilities, death/respawn integration, runtime retargeting,
Mover traversal and Mover ragdoll remain later slices. Binding the existing ASC
does not establish those character-specific gameplay contracts.

### Validation

The UE 5.8.2 `SurvivalRpgEditor Win64 Development` build succeeded. All four
new Blueprints compile, all nine packages load, and the dependency audit found
no references back into import folders across 2,138 reachable project packages.

`SurvivalRpg.GASP.Mover`: **3/3 passed**. The composition check covers
Experience/PawnData/native ownership and save isolation. Separate remote-owner
and listen-host PIE scenarios exercise actual input keys/axes, camera following
and looking, changing UEFN limb poses, jump/landing, crouch replication and a
late-joined observer. Camera measurements begin after stable rendered frames;
the test world creates normal floor collision and PlayerStarts before pawns
spawn. Tests do not set pawn movement or camera state to satisfy assertions.

Existing camera/CMC/traversal/retargeting/remote-melee regression suite:
**39/39 passed**, with no failures or skipped tests. Together with the focused
Mover suite, this is **42/42 passed**.

The authored `Lvl_RpgGaspMover` also starts in listen-server PIE with the
expected pawn, visible UEFN mesh, working AnimBP, active RPG camera and grounded
Mover simulation. A viewport capture confirms the inherited GASP map materials
and lighting. PIE was stopped and the editor was left on this test map.

The offline graph audit matches all 136 AnimBP graphs after reference remapping
and all 35 Pawn function graphs. Only the documented EventGraph changes differ.
This compares exported visible pins, values and links; it does not fully cover
hidden pins, unsupported node data or compiled bytecode.

PIE emits existing temporary-world NetGUID/voice warnings and isolated Network
Prediction buffering warnings around world initialization/join. The successful
scenarios are not a packaged-build, dedicated-server, packet-loss or deferred
combat/lifecycle certification. Sustained movement/Foley and separate-process
multiplayer remain user playtest checks.

Evidence is under `Saved/GaspMoverFoundation20260913`, including
`settled-camera-results.json`, `regression-final-results.json`, graph snapshots
and `graph-parity-audit.json`, asset validation, build logs, authored-map runtime
inspection/capture and preservation hashes. The final hash check after these
runs found all **7,612 existing asset files** and **seven existing saves**
unchanged, with no new save files. All nine new packages are saved; the editor
has no dirty content or map packages.

## Equipment and montage integration

This follow-up extends the accepted `RpgGaspMoverExperience` with the existing
RPG equipment, combat input and GAS montage path. UEFN remains the gameplay and
default visible mesh. The original GASP walk/sprint/movement selection remains
in the working Blueprint and its simulation callbacks.

Accepted in PR #139. The subsequent [health/death/respawn integration](#health-death-and-respawn)
extends the lifecycle intentionally left out of this stage. The user accepted
the current block behavior and deferred stationary legs during movement with
block held to a later combat/locomotion blending step, alongside CMC.

### Ownership

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

### Asset composition

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

### Playtest

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

### Validation

This initial validation used Independent ticking. Current Fixed-backend
correction checks and the render-independent root-motion conversion are covered
in [`gasp-mover-fixed-tick.md`](gasp-mover-fixed-tick.md).

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
predate the [Mover health/death/respawn integration](#health-death-and-respawn). Network tests ran in
single-process PIE; separate-process and packaged multiplayer remain untested.
Existing transient-level NetGUID, voice-interface and Independent interpolation
warnings occurred, especially during joins and simulated latency. No
warning-free result or editor undo/redo verification is claimed.

## Health, death and respawn

This slice, accepted in PR #140, extends the equipment/montage integration
accepted in PR #139. The subsequent [grounded Mantle integration](gasp-mover-mantle.md)
adds traversal to the same Experience.
The existing PlayerState ASC, HealthSet, HealthComponent, DeathComponent,
`GA_Combat_Death`, GameMode respawn and controller-owned inventory/loadout
remain responsible for the RPG lifecycle. No second health or respawn system
is introduced.

### Ownership and behavior

`ARpgMoverPawn` composes the existing Health and Death components and binds
them through PawnExtension's ASC lifecycle. The server restores health on a
fresh living avatar and routes world-boundary damage through the existing
self-destruct GameplayEffect. Death completion returns to the GameMode's
inventory-drop, respawn delay and checkpoint selection path.

The shared HealthComponent now reconstructs death tags from a death state
received before ASC binding. A retired avatar cannot change the replacement
avatar's death tags through a late replication callback or cleanup.

The engine-facing movement addition is one native `URpgDeadMovementMode`.
`URpgCharacterMoverComponent::DisableMovementForDeath` enters that terminal
stationary mode through Mover simulation. Its mode and capsule state use
existing Network Prediction replication and correction. New movement inputs
and root-motion contributions cannot move the dead pawn; history preceding
death remains available for rollback. GAS continues to own death montage
playback. Respawn creates a fresh pawn with the authored GASP movement modes.

The Mover Blueprint selects the existing `RpgPawnMesh` and `RpgPawnCapsule`
collision profiles so RPG weapon traces can reach the gameplay mesh. The
optional retarget follower remains cosmetic. Animation graphs, clips, gait
tuning, the death montage and its two-second ability delay are unchanged.

### Manual validation

Open `Lvl_RpgGaspMover`. The marked **DEATH / RESPAWN** area is forward and
left of the player starts. Its test Blueprint checks the overlapping actor's
authority and calls the existing HealthComponent self-destruct path. Its
marker uses the original project-local GASP cube/grid material. Map lighting
and the isolated GameMode's disabled disk persistence remain unchanged.

1. Walk into the marked area while moving, attacking or holding block.
2. Check that the pawn stops, the death animation plays and the respawn screen
   appears. The existing GameMode uses a five-second respawn wait.
3. Request respawn with the existing UI. Check health, equipment attachment,
   movement, camera and attack/block input on the replacement pawn.
4. Repeat with a remote client and an observer joining during the respawn wait.

Death uses the existing inventory-drop policy; this slice does not introduce
new item-loss rules. Downed/revive, Mover traversal and Mover ragdoll remain
separate steps. The user-observed stationary legs while moving with block
held are accepted for now, as with CMC; combat/locomotion blending is deferred.

### Validation record

This initial record used Independent ticking. The later Fixed-backend
adaptation and fresh lifecycle/correction results are tracked in
[`gasp-mover-fixed-tick.md`](gasp-mover-fixed-tick.md).

UE 5.8.2 Development Editor builds succeeded, including the final lifecycle
test source. All 50 distinct automation tests passed: two Health lifecycle
contracts, two Mover lifecycle network cases and 46 existing camera, CMC,
Mover, traversal, retarget and melee regressions. Results and source hashes
are recorded in `Saved/GaspMoverLifecycle20260913/verified-results.json`.

Nine relevant packages loaded and both affected Blueprints compiled. The
authored listen-server test zone reached the existing death screen; its
focused respawn button created a healthy replacement pawn on the same
PlayerState. The final sight check confirms the readable label and hidden
trigger helpers. PIE ended cleanly, prior play settings were restored and
no packages were dirty.

The hash comparison checks 7,623 existing asset/plugin files against
`4f78c214`: only `BP_RpgGasp_Mover` and `Lvl_RpgGaspMover` changed. The test-zone
Blueprint is the sole new package. All seven existing save files are
unchanged, with no new saves. Source animations, imported assets, baseline
content, death ability/montage and GameMode assets retain their prior bytes.

The lifecycle network tests exercise an incoming weapon-channel sweep against
the physical gameplay mesh, pass its bone hit through the real damage GE,
then trigger lethal damage during an input-driven attack or held block.
They observe cancellation, terminal movement, destruction, the existing
respawn RPC and replacement-pawn equipment/ASC composition. One case includes
an observer and optional retarget follower; the other joins during the respawn
wait and introduces a 50 cm owning-client prediction error while dead.

That correction check observes reconciliation at unchanged simulation
frame/time. It does not establish a rollback whose starting frame predates
death. Coverage uses the Independent game-thread backend in single-process
PIE; separate-process and packaged multiplayer remain untested. The incoming
body-hit check is not a PvP permission test. Existing CMC melee coverage also
exercises enemy damage. Transient-level NetGUID, voice and Independent
interpolation warnings can occur during these PIE joins and delay tests.
