# GASP Mover foundation

This document records the foundation accepted in PR #138. The subsequent
[equipment/montage integration](gasp-mover-gameplay.md) extends its composition;
the original pilot's empty RPG inputs/ability sets describe that earlier stage.

This pilot adds a separate `RpgGaspMoverExperience` using the project-local GASP
Mover Blueprint. It proves the composition boundary for ordinary locomotion,
camera, crouch and jump. The accepted baseline and CMC traversal Experiences
keep their existing movement stack. UEFN remains the visible and animated mesh.

## Ownership and simulation

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

`Config/DefaultNetworkPrediction.ini` is unchanged: Independent ticking,
Interpolated simulated proxies, 100 ms interpolation buffering, 250 ms maximum
Independent buffering and input send count 6. The original kinematic Mover
simulation is retained; this slice does not introduce the sample physics or
ragdoll Mover variants. Interpolation starvation warnings require measurement
of the affected world, role and frame/network timing; no global settings change
is presented as an animation-speed fix.

## Asset mapping

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

## Intentional sample deviations

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

## Playtest

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

## Validation

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
