# GASP-Migration: missing dependencies (2026-09-07)

The imported assets had 3,130 invalid-tag warnings in the captured editor log.
`SmartObject.ObjectType.Player` also caused a GameplayTags ensure and prevented
`ST_Player_SandboxCharacter_SmartObject` from compiling/linking. Both character
AnimBPs referenced an unavailable `AnimSubsystem_SequencerMixer` structure, and
both character Blueprints had a missing visual-override component class.

## Sources and changes

Source checkout: `D:/Repos/GameAnimationSample`, `EngineAssociation: 5.8`.
The sample does not declare a more specific build/version. Validation used the
installed Unreal Engine **5.8.1**, changelist **56057345**.

| Project change | Actual source and purpose | Deviation from sample |
| --- | --- | --- |
| `Config/Tags/GaspMigration.ini` | `D:/Repos/GameAnimationSample/Config/DefaultGameplayTags.ini`; 16 Foley event tags for the imported animation notifies/audio bank, plus Bench and Player tags for SmartObject selection/event matching. Source SHA256: `3225c3d6eb658895ee4279c8acd5ace1a8ce792746f90ab26f39fcfb5a318310`. | Only the 18 used tags are included, in a separate tag source with purpose comments. Parent tags are implicit. Unused tags and redirects are omitted. Names and tag matching are unchanged. |
| `SurvivalRpg.uproject`: `MovieSceneAnimMixer` | `D:/Repos/GameAnimationSample/GameAnimationSample.uproject`; supplies the serialized mixer structures referenced by `SandboxCharacter_CMC_ABP` and `SandboxCharacter_Mover_ABP`. | Same plugin enabled as in the sample, with its declared plugin dependencies. Migration provenance is recorded in the plugin-reference description. No renderer or gameplay defaults are changed. |
| `Content/Blueprints/AC_VisualOverrideManager.uasset` | `D:/Repos/GameAnimationSample/Content/Blueprints/AC_VisualOverrideManager.uasset`; required hard dependency of both `SandboxCharacter_CMC` and `SandboxCharacter_Mover`. Source SHA256: `e6f20daf50d5149d6c318446a67e23d5bad9fe94de8ee490d73ef776506afed7`. | Original Blueprint logic copied, then compiled and saved through Unreal MCP with `GASP-Migration.*` source/version/purpose/deviation metadata. Its game-package dependencies already exist locally. |

Existing collision assignments, RPG configuration, StateTree graphs and character
graphs were not edited. All 181 entries in `DefaultGameplayTags.ini` remain
registered, and that file is byte-for-byte unchanged. No tag-name conflicts were
found. Copied assets resolve within the project, without mounting the source checkout.

## Verification actually performed

- Scanned 2,297 imported assets for sample tag names and script-module references.
- Unreal Asset Registry dependency audit: the missing visual manager was the only
  unresolved hard game-package dependency. After adding it: **zero missing hard
  game-package dependencies** in the audited import set and new component.
- Fresh Unreal Python commandlet, final exit code **0**: **18/18** new tags
  registered, **181/181** existing config tags preserved, **795/795** affected
  assets loaded. No invalid-tag, unknown-structure or missing-component warnings.
- The commandlet explicitly logged successful compilation of
  `ST_Player_SandboxCharacter_SmartObject`.
- Unreal MCP compilation with warnings treated as errors passed for the visual
  manager, both character Blueprints and both character AnimBPs (**5/5**).
  Only the newly imported visual-manager asset was saved.
- `Build.bat SurvivalRpgEditor Win64 Development -Project=... -WaitMutex
  -NoHotReloadFromIDE`: **Succeeded**. The initial sandboxed .NET attempt hung and
  was cancelled; the completed build ran with access to the required build caches.
- MCP PIE smoke test of `/Game/SurvivalRpg/Maps/Test/Lvl_ThirdPerson`, about **64 s**:
  started and stopped successfully, with no invalid-tag, SmartObject StateTree,
  unknown-structure or missing-component errors. Used **NullRHI** and `-nosteam`;
  this does not verify rendered animation, online play or a complete bench interaction.

Local evidence and scripts: `Saved/MigrationErrors20260907/`, especially
`runtime-results.json`, `runtime-final.log`, `editor-mcp.log`, `build.log`,
`dependency-inventory.json` and the MCP compile results. These generated files are
intentionally not versioned.

## Remaining findings and limits

- `GM_Sandbox` still has four unresolved **soft**, optional character references:
  `BP_Echo`, `BP_Twinblast`, `BP_UE4_Mannequin` and `/Game/MetaHumans/Kellan/BP_Kellan`.
  Selecting those variants is not validated. Resolve deliberately by migrating
  the desired variant and its dependency closure or removing that selection entry.
- `LevelBlock.UserConstructionScript` accesses index 0 of an empty
  `GetAllActorsOfClass` result in the RPG test map. Its sample level-composition
  assumption needs a targeted guard or the required level actor; neither was
  silently added to the map.
- Pose Search reports that `BS_Relaxed_Run_Loop_F_Slopes` lacks
  `bShouldMatchSyncPhases`. Source parity and the effect of changing that asset
  setting remain open; no blanket blendspace edits were made.
- Broader existing/editor messages remain, including unspecified GameplayCue scan
  paths, optional MetaHuman content, voice-interface availability and the earlier
  `r.MotionVectorSimulation` render-thread warning. The headless test also emits
  editor-plugin self-test, unsupported-RHI, sandbox file-access and offline-service
  diagnostics. These are not evidence that all editor warnings are resolved.
- Full SmartObject use/cancellation, motion matching under visible movement,
  multiplayer/late join and cooking were **not tested** in this dependency fix.
