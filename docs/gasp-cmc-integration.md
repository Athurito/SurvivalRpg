# GASP CMC experience

Work branch: `codex/gasp-rpg-cmc-experience`, starting at the merged asset
foundation `3eda9d93`. This slice connects the project-owned GASP animation
assets to the RPG pawn. Traversal and additional visible skeletons follow
separately.

## Ownership and isolation

- CharacterMovement and the existing RPG character/ASC lifecycle remain the
  authoritative runtime owners. The character opts into UE 5.8's existing
  acceleration replication so simulated proxies receive actual movement input.
  No additional replicated movement state or runtime native class is needed.
  `ARpgCharacter::ShouldReplicateAcceleration()` enables the engine packet for
  RPG characters; this does not require changing a global replication CVar.
  The unused custom CMC acceleration setter and preservation override are removed.
- A Blueprint child of `BP_Rpg_Character` supplies the existing GASP pawn
  interface from game-thread character state. The UEFN mesh occupies `GetMesh()`;
  the existing camera, input, equipment and death lifecycle remain inherited.
- A working CMC AnimBP uses `URpgAnimInstance` to retain the listen-server pose
  timing guard. Its authored graph and animation selection remain Blueprint/
  Chooser content. Five class-dependent Chooser tables receive working copies.
- Experience, PawnData and test-map composition are project-owned assets.
  The baseline shares runtime code but keeps its existing character/animation
  asset selection; assets needed for adaptation are copied before editing.
- One reusable editor-only Blueprint library exposes native interface setup and
  object-reference replacement inside an owned asset. These close authoring gaps
  for interface implementation and protected Chooser instanced data. The archive
  never traverses other assets; normal editor notifications can refresh loaded
  referencers, so only working-copy referencers are used and source hashes are
  checked. The helper contains no runtime content.

## CMC presentation contract

`BP_RpgGasp_CMC` implements the existing project-owned GASP pawn interface. Its
game-thread animation snapshot reads the RPG CMC's velocity, current acceleration,
acceleration/braking limits, crouch state, floor and rotation settings. The current
RPG input supports running, crouching and jumping; this first preset uses Run.
Load-dependent gaits and sprint input remain a later movement slice.

The gameplay mesh uses the copied `MI_UEFN_Mannequin_CMC` material in override
slot 0, matching the imported CMC character. The initial RPG character setup
omitted this component override and displayed the mesh's default material.
Restoring the override changes neither the UEFN mesh nor its animation class.

The Blueprint caches the preceding airborne state and velocity solely for the
0.3-second landing presentation on every network role. These four local fields
are neither replicated nor saved. They do not control movement or landing damage.
The native engine movement packet supplies real acceleration to simulated
proxies, including zero acceleration while still coasting.

The working AnimBP retains the existing `DefaultSlot` and montage-only root motion.
It uses the sample's game-thread update route with a fixed initial Motion Matching
preset; sample debug CVars cannot switch this working copy to an experimental
state machine or another locomotion implementation. The engine Offset Root Bone
enable CVar is retained. This is an integration choice, not a claim that Unreal's
thread-safe animation APIs are defective.

The pawn's existing `GaspFoleyEvents` component uses its own
`Audio/DA_RpgGaspCMCFoley` bank. It copies the shared
`DefaultFoleyEventAudioBank` and replaces only `Foley.Event.Run` with
`Audio/MSS_RpgGasp_Run`, a copy of `MSS_FoleySound_Run_Soft` with Volume 0.48
instead of 0.24 (+6.02 dB). This raises sustained-running gain without raising
the other 15 events or changing sound concurrency, spatial attenuation or the
shared GASP bank. The gain is designer-owned audio tuning; it raises existing
sound starts and cannot repair missing starts or establish final perceived
loudness by itself.

The RPG Run preset's `Waves` override excludes only
`Foley_fs_1p_sneaker_concrete_walk_10`, retaining the other 29 source waves in
their original order. Waveform matching identified the scratch in the user's
19:33 recording as this sample's recorded tail. This is a listening trial in
the owned preset; the source sample and shared Run_Soft preset stay available.
The existing gain, pitch variation, Scuff events and notify-filter setting are
preserved. Local diagnosis and authoring evidence:
`Saved/FoleyScratch20260910/` and `Saved/FoleyRunTuning20260910/`.

The RPG AnimBP's Motion Matching node disables `bShouldFilterNotifies`. The
shared `BP_AnimNotify_FoleyEvent` still rejects notifications from blending-out
animations. The imported, foundation and initial RPG graphs combined that guard
with the engine's 0.2-second Run notify cooldown. Local engine source shows that
an outgoing notify can consume this cooldown before the Blueprint rejects it,
suppressing an incoming step. The user also reported possible small gaps in the
original project; this does not establish a universal sample defect. This adaptation
changes only the owned AnimBP; shared notifies and animation clips stay intact.

## Asset composition

The new Experience preserves the baseline's feature list, UI actions, ability
sets, input, camera and inventory definition. It changes the selected PawnData
and character only. The concrete package paths are:

| Role | Package |
| --- | --- |
| Experience | `/Game/SurvivalRpg/System/Experiences/RpgGaspCMCExperience` |
| PawnData | `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/DA_PawnData_GaspCMC` |
| Character | `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/BP_RpgGasp_CMC` |
| AnimBP | `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/ABP_RpgGasp_CMC` |
| Gameplay mesh | `/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin` |
| Test map | `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspCMC` |
| Test GameMode | `/Game/SurvivalRpg/Maps/Test/GaspCMC/BP_Rpg_GaspCMCTestGameMode` |

The working `Choosers` folder beside the AnimBP contains
`CHT_CMCCharacterAnimations`, `CHT_PoseSearchDatabases`, and its `Dense`,
`Sparse` and `ExtremeSparse` variants. Their class contexts target the working
AnimBP. A stale Evaluate Chooser context pin exposed by a cold load was repaired
in the working AnimBP and that copy compiled successfully.

All six animation working copies originate in the merged foundation. In the
following mapping, paths are relative to `/Game/SurvivalRpg/Characters/GASP/`:

| Foundation source | Integration copy |
| --- | --- |
| `CMC/Blueprints/SandboxCharacter_CMC_ABP` | `CMC/RPG/ABP_RpgGasp_CMC` |
| `CMC/Characters/UEFN_Mannequin/Animations/ExperimentalStateMachineData/CHT_CMCCharacterAnimations` | `CMC/RPG/Choosers/CHT_CMCCharacterAnimations` |
| `Shared/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases` | `CMC/RPG/Choosers/CHT_PoseSearchDatabases` |
| `Shared/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Dense` | `CMC/RPG/Choosers/CHT_PoseSearchDatabases_Dense` |
| `Shared/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_Sparse` | `CMC/RPG/Choosers/CHT_PoseSearchDatabases_Sparse` |
| `Shared/Characters/UEFN_Mannequin/Animations/MotionMatchingData/CHT_PoseSearchDatabases_ExtremeSparse` | `CMC/RPG/Choosers/CHT_PoseSearchDatabases_ExtremeSparse` |

The new map uses Engine cube geometry, a 60-by-40-metre running area, 20/40-cm
ground steps and three start points. It contains no imported Mover or traversal
sample actors. Its WorldSettings select the new Experience and test GameMode.
That GameMode disables disk persistence and uses `SurvivalRpg_GaspCMCTest`,
`SurvivalRpg_GaspCMCTest_Backup`, `SurvivalRpg_GaspCMCTest_Recovery`, user index
`0`, and offline profile `GaspCMCTest`.

## Test procedure

1. Open `Lvl_RpgGaspCMC` and clear any PIE Experience Override so WorldSettings
   select `RpgGaspCMCExperience`. Confirm the test GameMode has disk persistence
   disabled before starting the map.
2. Run two players as a listen server. Check both views while running, stopping,
   crouching, jumping and landing; inspect the UEFN pose, feet and equipment
   attachment. Exercise the existing primary weapon attack and its return to
   locomotion. The small steps test grounding, not mantle/vault traversal.
3. For the material/Foley check, run for 30–60 seconds as a listen server,
   initially with one player. Include sustained running, small steering changes,
   pivots, stops and attacks. Confirm the imported CMC appearance and listen for
   missing or clustered steps, particularly at Run/Strafe transitions. Repeat
   with two players and check both views; final loudness still needs listening.
4. With PIE stopped, run automation groups `SurvivalRpg.GASP.CMC`,
   `SurvivalRpg.Movement.Acceleration`, and `SurvivalRpg.Animation.ListenServer`.
   The network test creates a temporary CQTest world and adds a late-joining
   client. Its actual GameMode instances receive unique test slots and disabled
   persistence during `Super::InitGame`, before RPG save loading. It refuses to
   interrupt an existing PIE session.

## Verification status

The latest Unreal Editor target build succeeded. The six targeted native
acceleration/listen-server animation tests passed without warnings or errors.
The initial two CMC tests passed in 17.83 seconds: asset composition and
the listen-server test with a remote owner and late join. They cover movement
and animation velocity agreement, Motion Matching database selection on owner
and server, crouch replication, airborne/grounded presentation, and the existing
equipment-granted GAS primary sword attack on the UEFN gameplay mesh. The server
montage advances at its effective playback rate; its attack-window notifies
close and runtime attack state clears before locomotion resumes.
The same native network test then exercises authoritative death, the remote
owner's normal respawn RPC, a new healthy pawn, rebinding of the persistent ASC,
and restored movement/Motion Matching on the owner, server and late-joined
observer. Both PIE runs used separate network worlds within one editor process;
packaged and separate-process sessions have not been exercised in this slice.

Initial fresh-editor validation loaded the 11 movement packages and compiled the four
owned Blueprints with warnings treated as errors. All 86 Chooser tables and
owned child tables validated without warnings or errors. The dependency audit
visited 1,354 project packages and 13,950 dependency edges, with no `/Game`
dependencies outside `/Game/SurvivalRpg`. Engine and feature/plugin dependencies
remain shared. Unreal supplied missing comment-node GUIDs during fresh load;
only the four new Blueprints were resaved after strict compilation.

The final CMC network run reported 83 warnings from disabled voice interfaces,
temporary-map NetGUIDs and the existing respawn widget's native-tick setting.
It reported no Blueprint or animation errors. The existing baseline
`CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation` regression
also passed in 28.88 seconds, with 39 warnings and no errors.

Two-player manual PIE used the authored map. Injecting the real Enhanced Input
`IA_Move` action on the remote owner produced 92 samples, a peak speed of
600 cm/s and approximately 506 cm of movement. An eight-second runtime probe
recorded 73 samples with matching jump, crouch, landing and database-selection
states. Screenshots showed the UEFN character with sword/shield and the existing
death UI, without obvious deformation. This is an initial visual check, not a
complete retarget-quality acceptance of every pose, socket or montage.

A subsequent 31-second two-player Foley check found regular audio starts during
sustained running. After raising the CMC Run preset by 6.02 dB, another 31-second
run confirmed the new preset on host, remote owner and observed proxies. Both
owning pawns retained about 18 Foley starts per five seconds, with no gap above
0.49 seconds during that run. The updated Blueprint compiled, the two new audio
assets resolved locally, and only one of the bank's 16 entries changed. This
verifies component starts and the configured gain, not uninterrupted rendered
audio; the preferred final loudness remains subject to listening in play.
Evidence: `Saved/GaspFoley20260910/`.

The user's subsequent synchronized video/trace captured missing starts during
animation changes: all 112 started sounds reached the mixer with nonzero source
envelopes, while 40 of 41 dispatched notifies without a sound came from a
previously selected animation. Evidence: `Saved/FoleyDiagnostic20260910/analysis/`.

A controlled replay used the same normalized world directions derived from that
recording; it did not reproduce the exact original inputs or pose choices.
With the Motion Matching filter enabled, the initial and warmed repeat runs had
11 and 13 high-speed gaps above 0.55 seconds, with maxima of 1.674 and 1.391
seconds. With it disabled there were no such gaps; the maximum was 0.545 seconds.
The initial run had a 0.4-second sampling hitch; intervals crossing sampling gaps
above 0.1 seconds were excluded. The disabled run had two intervals below 0.15
seconds at Run/Strafe transitions, none within one sound family. Its trace had
147 sound starts (143 steps and four scuffs), while 80 notifies from other,
outgoing animation assets still produced no sound. Background-editor source
envelopes were zero, so this comparison verifies cadence, not audible quality
or the absence of perceived double steps. Evidence:
`Saved/FoleyFilterAB20260910/comparison.json`.

After the material/filter changes, both owned Blueprints compiled with warnings
treated as errors. Both CMC tests passed again in 13.64 seconds, covering network
movement, late join, the equipment montage, death and respawn. Existing warnings
remained, with no errors. No additional native code was changed for this fix.

A Python-driven respawn probe failed in editor scripting: the editor script
execution guard forced local RPC dispatch. The passing native latent test above
uses the normal RPC route without this scripting guard; no gameplay respawn
code was changed to work around the probe.
Traversal, other visible skeletons, sprint/load tuning, and other RPG montages
remain outside the completed validation.

Before runtime tests, the seven existing save files were copied byte-for-byte
to `Saved/GaspCmcIntegration20260910/save-backup`; the corresponding manifest
also records the 8,266 existing tracked files. Test worlds must use isolated
persistence settings before their GameMode initializes.

The repeated SHA-256 preservation audit found all 7,330 pre-existing Content
files, including 6,848 asset/map packages, unchanged. All seven original saves
are unchanged and `Saved/SaveGames` contains no new files. Of the 8,266 tracked
files, only the four intended existing native files changed: `RpgCharacter.h`,
`RpgCharacterMovementComponent.h/.cpp`, and `RpgCombatPIENetworkTests.cpp`.
The detailed local evidence is in `Saved/GaspCmcIntegration20260910/`:
`preservation-current.json`, `preservation-final.json`, `native-tests.json`, `cmc-tests-final.json`,
`baseline-combat-tests-pass.json`, `cold-validation.json`,
`runtime-final-input-IA_Move.json`, `runtime-movement-probe.json`,
`composition-authoring.json`, and `build-final.log`.
