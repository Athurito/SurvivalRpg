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

## Placed Ragdoll actor A/B investigation (2026-09-08)

The historical warnings name the specific placed instance
`SandboxCharacter_Mover_Ragdoll_C_UAID_8A884BC111253CFF02_1573519336`
in `Lvl_ThirdPerson`, rather than a GameMode-spawned player. This identifies the
affected actor; it does not establish that placing it in the level is itself
the cause of the first-frame cache miss.

Read-only MCP inspection compared the actor with its Blueprint CDO and component
templates. Physics Control (38 properties) and `AC_PostABPTick` (13 properties)
matched. The mesh template comparison showed `bNotifyRigidBodyCollision` and
tick `endTickGroup` differences. A fresh instance of the **same Ragdoll Blueprint**,
created temporarily at the same transform, had those same values. Across all
169 inspected actor/component properties (52 actor, 38 Physics Control, 66 mesh,
13 post-animation), the original and fresh instances matched after normalizing
instance-owned component references. No properties in this selection were
unreadable. This excludes a difference in those inspected instance settings;
it is not an exhaustive comparison of every serialized property or runtime state.

A separate UE **5.8.2, CL 56702186** D3D12 offscreen editor ran four PIE sessions:

| GameMode | Placed Ragdoll present | Ragdoll actors verified in PIE | Bone-cache warnings |
| --- | --- | --- | --- |
| `BP_Rpg_GameMode` | Yes | Placed instance only | 0 |
| `GM_Sandbox_Ragdoll` | Yes | Placed instance and spawned player | 0 |
| `BP_Rpg_GameMode` | No, temporarily removed | None | 0 |
| `GM_Sandbox_Ragdoll` | No, temporarily removed | Spawned player only | 0 |

The original `TriggerRagdoll` and `ExitRagdoll` events were also exercised:

- Placed instance under the RPG GameMode: measured **Walking -> Ragdoll -> Walking**.
- Placed instance under the Sandbox Ragdoll GameMode: calls completed, but the
  sampled mode was Walking throughout; a transition was **not verified** in this
  case. The samples cannot establish whether an intermediate transition occurred.
- Spawned Sandbox Ragdoll player with the placed actor removed: measured
  **Walking -> Ragdoll -> Walking**. Thus the negative warning result without the
  placed actor was also checked with an active Physics Control/Ragdoll character.

No Physics Control warnings occurred in any of the four sessions. Because the
warning was absent in the unchanged-actor control runs too, this A/B test does
**not** demonstrate that removing the actor fixes it. The historical startup
cache failure remains unresolved; no new engine or Blueprint fix is justified
by these results. Other editor/plugin warnings remain outside this comparison.

Both actor removals and GameMode overrides were restricted to the unsaved test
world. The RPG GameMode was restored, PIE stopped, and the isolated editor ended
without saving. SHA256 before/after checks confirm that the map and Ragdoll
Blueprint are unchanged; the baseline tag still resolves to `3acb2492`. The only
tracked change from this investigation is this report. No build, cook, visible
animation-quality assessment or multiplayer/late-join test was performed.

Local evidence: `Saved/PlacedRagdollAudit20260908/`, including
`instance-comparison.json`, `fresh-instance-comparison.json`, `pie-with.json`,
`pie-without.json`, the four per-case logs, `editor.log`, `content-before.json`,
`content-after.json` and `mcp-operations.jsonl`.

## CMC selected before Play: new reproduction evidence (2026-09-08)

The user reports intermittent warnings when selecting CMC **before** starting
Play. A new snapshot of the user's actual editor log confirms four further
bursts, at 18:52:03, 18:52:24, 18:52:30 and 18:52:41 UTC, with **67 warnings each**.
All 268 messages identify the same placed Ragdoll instance above, immediately
after startup under `GM_Sandbox_C`. This log is from **UE 5.8.2, CL 56702186**:
the intermittent problem demonstrably still occurs on this engine build.
The log identifies the emitting actor; the CMC selection at those historical
starts is user-reported, rather than independently recorded by that log.

A read-only MCP inspection of the user's editor found the test map open with
an unsaved `GM_Sandbox` world override. This editor was left untouched. A second
offscreen editor used its own MCP port 8001 for controlled tests, selecting
`DDCvar.PawnClass = 0` before each PIE start. All twelve runs verified the actual
spawned `SandboxCharacter_CMC_C_0`, rather than assuming the selector worked:

- Six starts with the original placed Ragdoll: zero bone-cache warnings.
- Six starts with that Ragdoll removed **only in the unsaved test world**: zero
  bone-cache warnings; CMC spawn succeeded each time.
- MCP inspection of the CMC Blueprint's 14 component templates found no
  `PhysicsControlComponent`. The warning's owner is the separate Ragdoll NPC.

These observations support separating the placed Ragdoll test NPC from a map
intended solely for CMC tests. Removing it would remove this particular emitter
from that map; it would not repair or explain the intermittent Ragdoll cache
failure. The controlled runs again did not reproduce the warning with the actor
present, so they cannot establish a causal CMC/Ragdoll interaction or prove a
cache fix. No failure stack was captured and no Blueprint/engine fix was applied.

The isolated editor restored its selector and RPG GameMode and exited without
saving. Map, Ragdoll Blueprint and `GM_Sandbox` SHA256 values remained unchanged.
The user's open editor and its unsaved settings were preserved. This follow-up
only adds documentation; the migration baseline tag remains unchanged. No C++
build, cook or multiplayer parity test was performed.

Local evidence: `Saved/CmcSpawnAudit20260908/user-before.log`,
`user-warning-lines.json`, `user-editor-readonly.json`, `cmc-components.json`,
`cmc-with-results.json`, `cmc-without-results.json`, the twelve per-run logs,
`mcp-operations.jsonl`, `content-before.json` and `content-after.json`.
