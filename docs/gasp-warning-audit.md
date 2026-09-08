# GASP migration: remaining warning audit (2026-09-07)

Two confirmed asset defects were corrected. The captured user session contains
99 warning/error lines (46 distinct messages); these do not represent 99 separate
defects. No missing GameplayTag, SmartObject StateTree or unknown-structure errors
remain in the inspected session. Physics Control startup timing and two generic
editor startup errors remain unresolved.

## Changes and semantic diff

Both assets were edited and saved through Unreal MCP. Their
`GASP-Migration.Source`, `.Version`, `.Purpose` and `.Deviation` metadata record
the actual source, purpose and deliberate sample deviation; these tags were
checked again after loading the saved assets in a fresh editor.

Source checkout: `D:/Repos/GameAnimationSample`, `EngineAssociation: 5.8`.
The sample declares no more specific build. Validation engine: **5.8.1,
CL 56057345**. Source paths below are provenance only, not runtime dependencies.

### LevelBlock: optional level-style actor

Asset/source: `Content/Levels/LevelPrototyping/LevelBlock.uasset` under the project
and source checkout respectively. Source SHA256:
`2dda286a19ec6a9998fbba59f5ad1f4b712ee233fad341a770548b9e9a78e2e1`.

`Lvl_ThirdPerson` contains a derived LevelBlock but no sample `LevelVisuals`
actor. The original construction script indexes an empty array before reaching
its existing validity guard. The sample has the same graph behavior.

```diff
 LevelBlock.UserConstructionScript
-GetAllActorsOfClass(LevelVisuals) -> ArrayGet[0] -> existing Is Valid
+GetActorOfClass(LevelVisuals)                   -> existing Is Valid
```

The query now returns null when the optional actor is absent. Both existing
validity branches, style consumers and construction continuation are preserved.
No style actor was added, and the map was not changed.

### Forward-run slope BlendSpace: phase matching

Asset/source:
`Content/Characters/UEFN_Mannequin/Animations/Run/BS_Relaxed_Run_Loop_F_Slopes.uasset`.
Source SHA256:
`06c300fbb652fe207fdf015625f2f27b2552d64c15ac286e3293be2124133520`.

```diff
-bShouldMatchSyncPhases = false
+bShouldMatchSyncPhases = true
```

The original sample also disables this flag. Installed Pose Search explicitly
requires it for matching BlendSpaces (`PoseSearchDerivedData.cpp`). All three
flat/uphill/downhill run animations have eight sync markers with the same ordered
names, `L,R,L,R,L,R,L,R`. Sample membership, coordinates, rates and skeleton are
unchanged; no animation or marker data was edited.

## Findings and unresolved work

Counts refer to `Saved/RemainingWarnings20260907/editor-before.log`.

| Message family | Lines | Finding and disposition |
| --- | ---: | --- |
| Physics Control: `Failed to find bone data` | 67 | All occur at `21:05:13.170`, frame 153, the first game frame after BeginPlay. Native `GetBoneData`/`GetModifiableBoneData` report missing cached pose data here; an unknown skeleton bone has a different `Failed to find BoneIndex` diagnostic. An early read before the first cache update is plausible, but the exact caller/trigger is **unconfirmed**. Neither test run reproduced it. Ragdoll Blueprint remains byte-for-byte unchanged. |
| LevelBlock array access and caller | 2 | Fixed as above; absent in fresh rendered PIE. |
| Pose Search phase flag | 1 | Fixed as above; absent in fresh rendered editor/PIE. |
| Pose Search asynchronous index searches skipped | 3 | Temporary startup condition in this log: all 169 databases subsequently loaded from cache (163) or completed a build (6). No failed index build found. |
| `r.MotionVectorSimulation` render-thread access | 1 | Engine registration/read mismatch: `MotionVectorSimulation.cpp` registers the CVar without `ECVF_RenderThreadSafe`; `TemporalSuperResolution.cpp` reads it on the render thread. Still reproduced with D3D12. Changing the INI value cannot supply the missing registration flag. Engine fix remains open. |
| AdvancedFriends voice interface | 6 | Plugin requests a voice interface during GameInstance initialization/shutdown; project inherits `[Voice] bEnabled=false`, while `[OnlineSubsystem] bHasVoiceEnabled=true`. Decide separately whether voice is required: configure/test it, or disable the unused talking-status delegate. Neither behavior was changed. Tests used `-nosteam` and cannot validate Steam voice. |
| GameplayCue scan paths | 1 | No explicit `GameplayCueNotifyPaths`, so GAS scans all `/Game`. This is a discovery/performance fallback. Narrowing requires a complete cue-location inventory and preservation of `RpgGameFeaturePolicy`'s dynamic GameFeature cue paths. No search roots were guessed. |
| MetaHuman optional content | 1 | Creator optional data folders are absent; editor feature availability is reduced. Install the optional Creator data if needed. This does not identify a missing GASP animation dependency. |
| Slate style lookup | 2 | `MetaHumanSDKEditor/Private/UI/MetaHumanStyleSet.cpp:145` requests an invalid button-style key and falls back to defaults. Confirmed engine-plugin source issue; no project style workaround or engine edit. |
| ClassViewer missing parent | 9 | Seven messages concern MVVM, TextureGraph and ScriptableTools parents that **do exist** in the running editor; the failure concerns ClassViewer's hierarchy. Two MoverExamples assets instead reference unregistered native paths `/Script/Mover.PhysicsCharacterMoverComponent` and `/Script/MoverExamples.MoverExamplesPhysicsCharacterMoverComponent`. Exact replacements and use of those engine examples remain open; no guessed redirects. |
| TextureGraph serialization | 1 | `TG_Var.cpp` cannot find a serializer for an argument with type `None` and variable id -1. Reproduces during editor startup. The originating asset/caller is still unresolved. |
| AutomationTest: `Condition failed` | 2 | Reproduces during editor startup immediately after the TextureGraph message, without test names or a stack. Temporal proximity does **not** establish TextureGraph as their cause. These errors remain open; they were not reported as passing project automation tests. |
| MCP notice | 1 | Plugin license/data-handling notice, not a runtime failure. |
| HTTP shutdown | 2 | One Epic telemetry request remains in progress during editor shutdown. No gameplay failure is identified by these lines. |

Physics Control follow-up requires a reproducible first-Play, character-switch or
ragdoll-action sequence and the caller at the cache miss. The inspected Blueprint
already establishes the dependency order mesh -> `AC_PostABPTick` -> Physics
Control. Adding extra per-frame cache updates without that evidence could alter
velocity/timing behavior. The user does not yet know the trigger.

The four deliberately excluded optional character soft references in `GM_Sandbox`
were subsequently removed on 2026-09-08. Additional tests covered both Sandbox
GameModes, appearance/pawn cycling and ragdoll entry/get-up without reproducing
the bone-cache warning. The exact historical timing remains unresolved; see the
[Physics Control follow-up](gasp-physics-control-followup.md).
No collision assignment conflicts were introduced or changed in this follow-up.

## Verification actually performed

- Compared the relevant project graphs/settings against the original sample for
  LevelBlock, the run BlendSpace and `SandboxCharacter_Mover_Ragdoll`.
- Compiled LevelBlock through MCP with warnings treated as errors: passed.
- Fresh read-only Unreal Python commandlet: **exit 0**,
  `WARNING_FIXES_VALIDATION_PASS`. Confirmed the enabled phase flag, unchanged
  BlendSpace sample data/skeleton, matching sync-marker order, and LevelBlock
  compile status `BS_UP_TO_DATE`; also probed the ClassViewer parent classes.
- NullRHI PIE smoke test, about **4 s**, after the LevelBlock fix: no empty-array
  warning and no reproduction of the Physics Control messages.
- Fresh **D3D12 offscreen** editor and about **12 s** PIE in
  `/Game/SurvivalRpg/Maps/Test/Lvl_ThirdPerson` after both fixes: started/stopped
  successfully. Zero empty-array, phase-flag, Physics Control bone-data,
  invalid-GameplayTag or unknown-structure messages. Startup still emits the
  unresolved editor diagnostics listed above; this is not a warning-free run.
- Verified the saved migration metadata and unchanged Ragdoll asset hash.

No C++ build was run for these asset-only fixes. Visible locomotion/slope behavior,
ragdoll interaction, multiplayer/late join, Steam voice and cooking were not
tested. An auxiliary source-sample inspection produced its comparison data but
exited 1 because enabling the inspection toolset exposed a sample GameFeatureData
asset-manager rule error; it is not counted as a passing validation.

Local evidence: `Saved/RemainingWarnings20260907/` contains `inventory.json`,
`asset-inspection*.json`, `mcp-operations.jsonl`, `saved-migration-metadata.json`,
`validation-results.json`, `validation.log`, `editor-mcp.log` and
`editor-rendered.log`. These generated logs and backups remain unversioned.

## MoverExamples parent-load comparison with original GASP (2026-09-08)

The two suspect engine example Blueprints were explicitly loaded, using the same
read-only Python commandlet in `D:/Repos/GameAnimationSample` and SurvivalRpg.
Both used **UE 5.8.2, CL 56702186**, from `D:/Programme/UE_5.8`. The result is
identical: the missing-parent warnings also reproduce in the original GASP.
This particular load failure is not introduced by the project migration.

| Engine Blueprint under `/MoverExamples/Components/` | Original GASP | SurvivalRpg |
| --- | --- | --- |
| `DefaultPhysicsCharacterMoverComponent` | Missing parent; generated class null | Same |
| `ExtendedPhysicsCharacterMoverComponent` | Missing parent; generated class null | Same |
| `DefaultCharacterMoverComponent` | Parent and generated class resolve | Same |
| `ExtendedCharacterMoverComponent` | Parent and generated class resolve | Same |
| `ChaosMoverComponent` | Parent and generated class resolve | Same |

The two failing native paths are `/Script/Mover.PhysicsCharacterMoverComponent`
and `/Script/MoverExamples.MoverExamplesPhysicsCharacterMoverComponent`.
Both direct native-class loads return null in both projects. Asset Registry tags
confirm that the affected Blueprints still declare exactly those parents.
Although loading returns the Blueprint container object, its generated class
does not exist, and the log reproduces `CreateExport: Failed to load Class ... as
Parent`. The healthy controls resolve `CharacterMoverComponent` or
`ChaosCharacterMoverComponent` and their generated classes normally.

Both final probes completed with **exit 0** and verified the expected success and
failure cases. This means the diagnostic completed; the two broken assets remain
broken. The initial probe also reproduced the warnings, but its optional Python
`parent_class` property read was unavailable in this engine build. The corrected
probe uses registry parent tags, direct native-class resolution and actual
generated-class availability; initial property-read failures were not counted
as asset-load failures.

No compilation, save, reparenting or redirect was performed. SHA256 checks for
the five engine assets and both `.uproject` files match before/after. Whether
these inconsistent engine examples are installation leftovers or distributed
sample-content defects remains unverified. This is separate from the intermittent
Physics Control bone-cache warning. No gameplay, cook or C++ build test was run.

Evidence: `Saved/MoverExamplesParentAudit20260908/`, especially
`original-verified.log`, `project-verified.log`, `GameAnimationSample-results.json`,
`SurvivalRpg-results.json`, `comparison.json`, `hashes-before.json` and
`hashes-after.json`. The only tracked change is this audit update.
