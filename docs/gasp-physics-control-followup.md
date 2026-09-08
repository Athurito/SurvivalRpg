# GASP Physics Control follow-up and optional selection cleanup (2026-09-08)

The four deliberately omitted appearance variants are now removed from
`GM_Sandbox.VisualOverrides_Soft`. This fixes the selection of absent content;
it is **not** a demonstrated fix for the intermittent Physics Control cache warning.
The warning did not recur in the tests below, including both Sandbox GameModes
and an explicit ragdoll entry/get-up sequence.

Work is isolated on `codex/gasp-physics-control-investigation`. The existing tag
`gasp-migration-baseline-2026-09-08` continues to point at `3acb2492`.

## Actual asset change and source

Only `Content/Blueprints/GM_Sandbox.uasset` was edited and saved through Unreal
MCP. Original source:
`D:/Repos/GameAnimationSample/Content/Blueprints/GM_Sandbox.uasset`,
`EngineAssociation: 5.8`, more specific sample build unspecified. Source SHA256:
`3e44e060c4aa136d00ae3326ea586a28d24d1439836d25e7f668e44c695632e6`.

```diff
 GM_Sandbox.VisualOverrides_Soft
-/Game/Blueprints/RetargetedCharacters/BP_Echo.BP_Echo_C
-/Game/Blueprints/RetargetedCharacters/BP_Twinblast.BP_Twinblast_C
-/Game/MetaHumans/Kellan/BP_Kellan.BP_Kellan_C
 /Game/Blueprints/RetargetedCharacters/BP_Manny.BP_Manny_C
 /Game/Blueprints/RetargetedCharacters/BP_Quinn.BP_Quinn_C
-/Game/Blueprints/RetargetedCharacters/BP_UE4_Mannequin.BP_UE4_Mannequin_C
```

The user intentionally did not migrate the removed variants. Both retained
classes exist, load and compile. Their relative order is preserved: Manny moves
from index 3 to 0, Quinn from 4 to 1. `DDCvar.VisualOverride = -1` still means no
override and remains the project default. Old manually entered indices 3/4 are
now outside the array; the existing selector returns no override for invalid
indices. The cycling function derives its bounds from the actual array length.

`GM_Sandbox_Ragdoll` inherits the two remaining entries after a fresh load; its
asset was not saved or changed. Pawn classes, default pawn choices and all
Blueprint graphs are unchanged. The base asset carries `GASP-Migration.Source`,
`.Version`, `.Purpose` and `.Deviation` metadata with this rationale and mapping.

The MCP array setter reported invalid old soft classes and applied partial
removals. Each intermediate array was inspected; removal continued only while
the intended excluded entries decreased and Manny/Quinn remained intact. The
final saved result was validated in a separate Unreal process, including the
inherited child defaults, rather than relying on the setter's return value.

## What caused the old warnings, and what remains unknown

The captured 2026-09-07 log explicitly reports `BP_Rpg_GameMode_C` at line 3283.
Its placed `SandboxCharacter_Mover_Ragdoll` then emits all 67 bone-cache warnings
at frame 153, immediately after BeginPlay at frame 152. Thus the affected actor
was a Sandbox Ragdoll character even though the GameMode was the RPG GameMode.

Inspected selection path:

1. `AC_VisualOverrideManager.FindAndApplyVisualOverride` casts the active GameMode
   to `GM_Sandbox` before reading `VisualOverrides_Soft`.
2. `BFL_HelpfulFunctions.GetVisualOverrideWithCVAR_Soft` selects only a valid
   array index. With the default/current value -1, it returns an empty reference.
3. The manager asynchronously loads only a valid selected soft reference. It
   does not preload every array member. Both Sandbox GameModes use this path.

The excluded references therefore do not explain that recorded RPG startup
event. Under a Sandbox GameMode, choosing an excluded entry could cause a failed
appearance load, which is why the selection cleanup is useful independently.

Native `UPhysicsControlComponent::GetBoneData` first resolves the skeleton bone
index. Missing skeleton bones produce `Failed to find BoneIndex`; a missing
skeletal mesh returns before the observed message. `Failed to find bone data`
instead means the component lacks usable cached pose data for that mesh at that
moment. Registration creates a cache record; a subsequent target-cache update
populates it. This narrows the issue to pose-cache availability/startup ordering,
but **the precise caller/timing that produced the historical miss is unresolved**.
No failure stack was captured, and no speculative tick or cache-seeding fix was
added to the Ragdoll Blueprint or engine.

Live inspection also confirmed the actual UEFN skeletal mesh, animation not
paused, `AlwaysTickPoseAndRefreshBones`, update-rate optimization disabled and
enabled mesh/post-animation/Physics Control ticks. The source Blueprint's
mesh -> post-animation component -> Physics Control prerequisite chain remains
unchanged. Ragdoll and child GameMode asset hashes match their pre-edit backups.

## Verification actually performed

All new runs used the installed **UE 5.8.2, CL 56702186**. The original intermittent
warning was logged under 5.8.1. This difference does not prove an engine fix;
the warning had also failed to reproduce in earlier 5.8.1 tests.

- RPG map PIE before cleanup, then simulation and PIE after cleanup: no
  `Failed to find bone data` messages.
- Strict MCP compilation of `GM_Sandbox` and `GM_Sandbox_Ragdoll`: passed.
- Fresh read-only Unreal commandlet: **exit 0**, `GASP_OPTIONAL_VARIANTS_VALIDATION_PASS`.
  Both GameModes resolve exactly Manny/Quinn, keep their original pawn arrays and
  defaults, and have no removed variants in their reported soft dependencies.
  Both retained variant Blueprints compile with status `BS_UP_TO_DATE`.
  Saved migration metadata was also verified.
- Fresh D3D12 offscreen editor, temporary unsaved overrides of `Lvl_ThirdPerson`
  to **both** `GM_Sandbox` and `GM_Sandbox_Ragdoll`: PIE starts/stops passed.
- In each Sandbox GameMode, invoked the existing `CycleVisualOverride` function:
  **Manny -> Quinn -> none**, with the actual replicated manager property checked
  on the spawned pawn and placed Ragdoll actor after each step.
- `GM_Sandbox`: invoked `CyclePawn`; confirmed both Mover and CMC pawn instances.
- `GM_Sandbox_Ragdoll`: invoked existing `TriggerRagdoll` and `ExitRagdoll` events;
  the actual Mover mode reported **Walking -> Ragdoll -> Walking**.
- **Zero bone-cache warnings** across these runs. Other documented engine/plugin
  diagnostics remain; these were not warning-free editor sessions.
- Restored the original RPG GameMode after testing; the map was never saved.

The extra runtime invocation helper was loaded only into the test editor from
`Saved/`; it adds no project asset, runtime class or installed plugin. Tests
used `-nosteam` and offscreen rendering. Visible animation quality, network
replication/late join, online services, cooking and a C++ rebuild were not tested.

Local evidence: `Saved/PhysicsControlInvestigation20260908/`, particularly
`array-change.json`, `validation-results.json`, `validation.log`,
`sandbox-matrix-results.json`, `runtime-*.json`, `mcp-operations.jsonl`,
`editor-before.log` and `editor-matrix.log`.
