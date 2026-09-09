# Selectable RPG baseline (2026-09-09)

Branch: `codex/rpg-baseline-experience`. This step gives the existing RPG
composition its own Experience, PawnData and test map. It introduces no native
classes, gameplay changes or GASP/Lyra movement integration.

## Assets and selection

| New asset | Purpose |
| --- | --- |
| `/Game/SurvivalRpg/System/Experiences/RpgBaselineExperience` | Copy of `RpgPrototypeExperience`, selecting `DA_PawnData_Baseline`. |
| `/Game/SurvivalRpg/Core/Character/DA_PawnData_Baseline` | Copy of `DA_PawnData`, retaining `BP_Rpg_Character`, input, camera, ability sets, team and inventory layout. |
| `/Game/SurvivalRpg/Maps/Test/Lvl_RpgBaseline` | Copy of `Lvl_ThirdPerson`, including 35 external actor packages and four external object packages. |
| `/Game/SurvivalRpg/Maps/Test/Baseline/BP_Rpg_BaselineTestGameMode` | Blueprint child of `BP_Rpg_GameMode` with three isolated disk-save slots. |

Open `Lvl_RpgBaseline` and start a fresh PIE session. Its World Settings select
`RpgBaselineExperience` as **Default Gameplay Experience** and
`BP_Rpg_BaselineTestGameMode` as **GameMode Override**. Clear any explicit
Experience override in PIE settings, URL or command line when testing this map
default. The existing Experience discovery directory already covers the copy.

Experience GameFeatures, action sets and action values match the original.
Instanced actions belong to the copied Experience. The copied level preserves
the existing content, including the already-placed sample actors; their presence
does not mean the RPG pawn has been adapted to GASP.

## Save isolation and return path

Only these inherited GameMode defaults are overridden:

| Property | Test slot |
| --- | --- |
| `WorldSaveSlotName` | `SurvivalRpg_BaselineTest` |
| `WorldSaveBackupSlotName` | `SurvivalRpg_BaselineTest_Backup` |
| `WorldSaveRecoverySlotName` | `SurvivalRpg_BaselineTest_Recovery` |

Disk persistence remains enabled. Test saves must use this GameMode, including
comparisons that temporarily select the original Experience on the test map.
The original map, Experience, PawnData, character and GameMode remain unchanged;
their recorded hashes matched after runtime testing. The original world-save
files also matched at the initial static audit. A later regression-test save
incident is recorded below; final save preservation is not claimed.

Return to `Lvl_ThirdPerson` for the original selection. The existing
`gasp-migration-baseline-2026-09-08-v2` tag remains the pre-change reference;
this branch's additions can be reverted independently. Do not revert unrelated
later work to return to the original map.

## Verification

- Both new Blueprints compiled through Unreal MCP with warnings treated as
  errors; changed assets were saved. Reloaded map settings selected the intended
  Experience and GameMode.
- PawnData fields and Experience action properties matched the originals. The
  copied actions had their own Experience subobjects. Original and copied map
  actor inventories matched after normalizing the map name (44 actors each).
- Five automation tests passed: `SurvivalRpg.Combat.StarterEquipmentAssetContract`,
  `SurvivalRpg.Inventory.Layout.AssetComposition`,
  `SurvivalRpg.Inventory.Layout.AuthoredAssetsDataValidation`,
  `SurvivalRpg.UI.Frontend.CommonUIComposition`, and
  `SurvivalRpg.UI.Indicator.CompositionAssets`.
- Fresh A/B sessions on the test map loaded the original and baseline
  Experiences respectively, confirmed by Experience logs and PlayerState/pawn
  PawnData. Both possessed `BP_Rpg_Character` with `ABP_Unarmed`, the existing
  camera configuration, HUD and ASC owner/avatar; the placed Mover pawn did not
  take possession. Captured ability lists, attributes and tags matched.
- Enhanced Input action pulses moved both pawns at up to 600 cm/s, changed yaw
  by 17 degrees and triggered jumping. Their timing-dependent travel distances
  were comparable (about 212/214 cm); this is a behavior check, not an assertion
  of identical native binding counts or frame-exact motion.
- Baseline save/reload retained all three captured inventory `ItemId` values,
  stack counts and placements, with profile restoration complete. Runtime
  `EntryId` values regenerated. The runtime GameMode used all three test slots.
- Death entered the waiting state with no possessed pawn; respawn created a
  new RPG pawn using baseline PawnData and `ABP_Unarmed`. Core combat,
  `GA_Interaction` and `GA_Test` grants matched before/after. Nearby interaction
  grants changed with position, so total ability count is not a respawn contract.
- Listen-server plus two PIE clients initialized all three player states with
  baseline PawnData and valid ASC owner/avatar pairs; both clients possessed
  their RPG pawn. A separate process then joined an established server in a
  second session: server/existing-client snapshots showed the additional player,
  and its own log confirmed baseline Experience loading and HUD creation.
- The existing
  `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation`
  regression passed (1/1, 27.59 seconds, no test errors). It exercises the
  Prototype Experience in a temporary world, not the new baseline map. Its
  warnings included offline voice/EOS and temporary-level NetGUID messages.

The late external client's internal state was not inspected by the PIE probe;
the evidence is its own startup log plus server/existing-client replication.
Protected/native-only properties reported as unavailable were not treated as
passed assertions. These checks do not constitute a full visual animation,
combat-input, interaction or equipment-operation matrix.

### Regression-test save incident

At 21:01:11 UTC, during the existing combat regression's teardown, the three
`SurvivalRpg_World*` files were rewritten despite the baseline map's isolated
slots. Its temporary GameMode CDO persistence override did not protect the
actual test instance: that instance loaded the original sequence 93. The
original primary/recovery now contain sequence 97 and backup contains 96.
Their final hashes differ from the pre-test manifest; `SlotManagerName.sav`
and all five checked original assets are unchanged.

The previous world saves have **not been recovered**. An exact hash search of
321 `.sav` files under `Saved` found no matching pre-test copy. No previous
save objects survived in the editor. Offline sequence-only candidates also
failed the original hashes and were not installed. The affected files were
preserved separately under `Saved/RpgBaseline20260909/affected-world-saves`;
restoration requires an existing external backup. All test processes have
stopped and temporary editor settings were restored. Do not rerun that
regression against existing saves until its test-world persistence isolation
is addressed. Future tests must preserve actual save-file bytes before launch,
in addition to using isolated slots and recording hashes.

Local evidence is under `Saved/RpgBaseline20260909` and is not committed. No
Unreal C++ build is claimed for this asset-only change.

## Boundary for later movement work

This is a selectable comparison configuration, not a frozen copy of all
gameplay and animation dependencies. Character, AnimBP, input, abilities,
camera, equipment and other referenced content remain shared. Inspect the
dependency closure and make targeted working copies before changing shared
assets for a later GASP integration.

GASP CMC adaptation, Mover, ragdoll changes, new movement rules and restoration
of the archived native port are outside this step. Future integration keeps
project-owned Lyra Experience/PawnData composition and gameplay authority,
with concrete animation content remaining designer-owned.
