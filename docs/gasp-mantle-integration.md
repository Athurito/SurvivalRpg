# GASP standing mantle pilot

This slice adds a contextual jump/mantle action to an isolated CMC test
composition. It starts from the accepted CMC integration, including its material
and Foley adjustments. The first action is a grounded forward standing mantle
onto prepared static ledges. Running/walking variants, airborne catches, Vault,
Hurdle, sustained climbing and the Mover Experiences remain separate work.

**Status:** implemented and validated on UE 5.8.2. The nine pilot packages load,
the five Blueprints compile, and the Editor target builds. The focused mantle
network test and existing CMC/combat regressions pass. The demonstration map is
ready for a manual feel and presentation review.

## Ownership decision

Two native runtime classes are justified by reusable schema and authority:

- `URpgMantleAnchorComponent` defines a prepared static ledge and landing
  location. Designers configure these on obstacle Blueprint instances; native
  validation checks the physical target rather than trusting client positions.
- Abstract `URpgGameplayAbility_Mantle` owns prediction-key-scoped target-data
  exchange, authoritative geometry checks, ability commitment and lifecycle
  cleanup. CharacterMovement remains movement truth and the existing RPG ASC
  remains the ability owner. Traversal state is transient, not saved.

The concrete `GA_RpgGasp_Mantle` Blueprint owns its montage, GAS graph and tuning.
Blueprint/PawnData/Experience assets compose the pilot; the AnimBP presents the
existing gameplay mesh and keeps `DefaultSlot` and montage-only root motion.
Unreal MCP authors, compiles, saves and validates the owned assets. No new
animation coordinator, sample character hierarchy or parallel gameplay manager
is part of this slice.

The existing RPG CharacterMovement component replicates only the mantle's
temporary collision exception to simulated proxies. Its owner applies this
exception predictively; the server installs it only after validation. Unreal's
MotionWarping component replicates warp targets to simulated proxies. Ordinary
movement remains under the existing CharacterMovement replication path.

## Source mapping and deliberate adaptations

The approved foundation is the source of animation content. The external
`D:/Repos/GameAnimationSample` checkout is a comparison source only and is not
required at runtime. Existing imported/foundation packages remain unchanged.

| Approved source | Pilot responsibility |
| --- | --- |
| `Shared/AC_TraversalLogic.TryTraversalAction` | Native prepared-anchor, reach and capsule-clearance validation |
| `Shared/Levels/LevelPrototyping/LevelBlock_Traversable` | Obstacle Blueprint with native ledge/landing schema |
| `CMC/.../Traversal/CHT_TraversalMontages_CMC` | One designer-configured standing montage for the first action |
| `Shared/.../Traversal/Mantle/AM_M_Neutral_Traversal_Mantle_1_0_stand_F_Lfoot` | Owned `AM_RpgGasp_MantleStanding` copy |
| `Shared/AC_TraversalLogic.SetWarpTargets` | Preserve the montage's `FrontLedge` alignment contract |
| `Shared/AC_TraversalLogic.PerformTraversalAction_CMC` | GAS montage lifecycle and CMC handoff with server validation |

Source paths in this table are relative to
`/Game/SurvivalRpg/Characters/GASP/`; `...` expands to
`Characters/UEFN_Mannequin/Animations`.

The source jump input tries traversal first and jumps if geometry or montage
selection fails. Holding the source input also repeats traversal checks, including
airborne catches. The pilot keeps contextual jump fallback and narrows traversal
to the initial grounded standing action.

The source chooser admits standing mantle clips at speeds up to 100 cm/s and
heights up to 150 cm; its first authored clips represent a 1 m mantle. It uses
pose matching to select a foot and entry time. This pilot intentionally starts
with one standing clip; it does not reproduce the complete sample selector.

The standing source montage is 2 seconds long, with 0.25-second Hermite blends.
Its two `FrontLedge` warping windows run approximately 0.141-0.365 and
0.365-0.699 seconds. Both use the UEFN `attach` bone, translation and rotation
warping, feet-relative placement and Z warping. The source target is the front
ledge plus 0.5 cm vertically, facing into the negative ledge normal. Mantle does
not require `BackLedge` or `BackFloor` warp targets. The sequence preserves root
motion enabled, force root lock, normalized scale and reference-pose root lock.

At the final warp window's end, the root is approximately 6 cm before the
authored attach-bone target. The remaining 56 cm of root motion places the
standing clip's endpoint 50 cm beyond `FrontLedge`; the prepared obstacle's
landing offset is `(50, 0, 0)`. The target additionally
compensates the gameplay mesh's base translation relative to capsule feet
(`CapsuleHalfHeight + BaseTranslationOffset.Z`, 2 cm for this pawn), preserving
the source's 0.5 cm clearance without changing the existing mesh transform.

The owned montage removes only the source movement-input early-blend notify so
GAS owns completion and interruption. The source notify could stop the montage
from approximately 1.421 seconds using a 0.3-second blend and
`FastFeet_InstantRoot`. Warping and the underlying source sequence remain intact;
the shared notify and source montage are preserved.

The sample's CMC path temporarily enables both
`bIgnoreClientMovementErrorChecksAndCorrection` and
`bServerAcceptClientAuthoritativePosition`. The pilot does not adopt these
bypasses: target validation and commitment belong to the server. Cleanup must
restore collision/movement safely on failure, rejection, cancellation and death,
rather than blindly restoring Walking during an interrupted airborne move.

## Isolated asset composition

The following assets live under
`/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/`:

- `BP_RpgGasp_MantleCharacter`
- `DA_PawnData_GaspMantle`
- `GA_RpgGasp_Mantle`
- `LAS_RpgGasp_Mantle`
- `BP_RpgMantleObstacle`
- `AM_RpgGasp_MantleStanding`

The separate Experience is
`/Game/SurvivalRpg/System/Experiences/RpgGaspMantleExperience`, selected by
`/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle`. Its test GameMode is
`/Game/SurvivalRpg/Maps/Test/GaspMantle/BP_Rpg_GaspMantleTestGameMode`.
The test composition disables disk persistence. The baseline and accepted
CMC Experience remain available without the mantle grant.

To try the pilot, open `Lvl_RpgGaspMantle` and approach the front of one of the
three prepared platforms. Stop close to the center of the front face, face the
platform and press Space once. Without a valid standing entry, Space retains
ordinary jump behavior. Repeated presses during mantle do not queue another
jump. Test a remote player from the listen-server window as well as locally.

## Validation performed on 2026-09-12

- `SurvivalRpgEditor Win64 Development`: build passed, including both runtime
  classes, the CMC lifecycle fix and the native network fixture.
- Fresh editor: nine packages loaded; five Blueprints compiled with no errors.
  Dependency audit followed 1,500 packages and 14,727 hard/soft edges, with no
  missing content packages or `/Game` dependencies outside `/Game/SurvivalRpg`.
- Source/owned montage comparison passed for skeleton, slot/sequence, sections,
  blends, root-motion settings/extraction and retained notifies/warp settings.
  The sole intentional difference is the removed early-blend notify.
- `SurvivalRpg.GASP.Mantle.AssetComposition`: passed.
- `SurvivalRpg.GASP.Mantle.GaspMantleExperiencePIE.AuthoritativeValidationRootMotionLateJoinAndLifecycle`:
  passed (15.74 seconds). Actual listen server, autonomous owner and late-joined
  simulated observer; jump fallback, geometry rejection, authority-only landing
  blocker/rejected prediction, repeated input, advancing root motion, landing,
  authoritative cancellation, final death/respawn, 75 ms outgoing latency per
  driver, obstacle destruction and replacement-entry readiness. Server movement
  correction remains enabled throughout. GAS commitment is observed, but this
  pilot configures no stamina cost.
- `SurvivalRpg.GASP.CMC.AssetComposition`,
  `SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage`,
  and `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation`:
  all passed (40.59 seconds combined). These retain equipment/montage regression
  coverage for the existing CMC and combat compositions.
- Existing assets and saves are checked against pre-authoring hashes: 7,570
  tracked content/plugin files and seven saves unchanged, no new saves.
- The demonstration map's three prepared platforms were visually inspected.
  The automation proves movement/network lifecycle, not subjective animation
  feel. Manual mantle presentation and attack-to-mantle transitions remain
  useful review cases; no packaged-build or dedicated-server pass is claimed.

The temporary PIE fixture disables persistence before GameMode initialization
and restores packet simulation settings. After final death it uses a fresh
obstacle lane: the existing death-drop `DisplayMesh` correctly blocks the old
entry, so teleporting a test pawn there would not establish a valid approach.
Known local voice-interface and temporary-world NetGUID warnings were present;
the passing runs contain no automation errors.

Evidence and the read-only preservation script are in
`Saved/GaspMantle20260912/`: `build.log`, `validate.json`,
`final-asset-audit.json`, `mantle-final-results.json`, `regression-results.json`,
the editor log and preservation comparison. Existing source packages remain
unchanged; only the nine new content packages belong to this slice.

Source audit evidence is retained in
`Saved/GaspAssetFoundation20260910/snapshots-source/` and `native_exports/`.
Relevant snapshot prefixes: traversal logic `69bf2b72f0bddd06ad33`, CMC character
`d45b8e7f60de851dba3a`, chooser `07e2b7a177defdf96f20`, prepared block
`0928c64abc0d136f45bd`, standing montage `c2cabefec11015a80eff`, standing
sequence `34aa7168ddf93b166a01`, early-blend notify `062e3e69af38dc966f57`.
