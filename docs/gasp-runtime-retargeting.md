# Optional GASP runtime retargeting

This slice adds configurable runtime retargeting to the existing CMC traversal
Experience. UEFN remains the visible default and the gameplay mesh. Selecting a
specific replacement character, adjusting equipment for different body
proportions, and adding Mover variants remain separate work.

## Ownership

The Experience selects the existing traversal PawnData. Its optional
`RuntimeRetargetProfile` configures a presentation component on the existing
traversal character Blueprint. The profile owns target mesh, retarget AnimBP,
IK Retargeter and relative transform. An empty target mesh deliberately keeps
the source visible without creating or ticking a second mesh.

`GetMesh()` remains the UEFN animation source and the authoritative mesh for GAS
montages, root motion, notifies, equipment sockets and corpse physics. The
optional target is a collision-free child mesh following the source pose. It
does not run a second gameplay montage or grant abilities. Character movement,
the traversal query/chooser and the three traversal actions keep their existing
owners.

The native component owns source/target lifetime, initialization from the
replicated PawnData and tick ordering. The original GASP retarget AnimGraph and
curve-driven retarget profile remain Blueprint content. The RPG working copy
reads the explicitly configured IK Retargeter instead of GASP's component-tag
lookup and asynchronous sample map.

The follower ticks after physics and after the gameplay mesh. The source keeps
refreshing its bones while hidden. Visibility switches only after the live
retarget node has the correct source, target and retargeter, an initialized
processor, and a subsequent finalized frame to copy the source pose. This
follower contract requires retarget evaluation at all LODs. Failed initialization
keeps UEFN visible and removes the unusable target on the next world tick.
Dedicated servers keep the gameplay mesh without allocating a cosmetic target.

## Assets

Paths below are relative to `/Game/SurvivalRpg/Characters/GASP/`:

| Role | Package |
| --- | --- |
| Source AnimBP | `Shared/RetargetedCharacters/ABP_GenericRetarget` |
| Working AnimBP | `CMC/RPG/Retargeting/ABP_RpgGasp_RuntimeRetarget` |
| Default profile | `CMC/RPG/Retargeting/DA_RpgGasp_RuntimeRetarget` |
| Existing PawnData | `CMC/RPG/Traversal/DA_PawnData_GaspMantle` |
| Existing character | `CMC/RPG/Traversal/BP_RpgGasp_MantleCharacter` |

The default profile selects the working AnimBP and leaves TargetMesh and
Retargeter empty. It therefore does not select Manny or another replacement
character. Retargeting content continues to use project-local foundation rigs
and meshes. The imported originals and the accepted baseline are preserved.

## Configuration and scope

Configure a profile with a compatible target mesh and IK Retargeter to enable
the optional target at pawn initialization. The working AnimBP remains reusable
across target skeletons; the actual compatibility depends on the selected
retargeter's source and target rigs. An unsupported configuration falls back to
the source mesh.

This is static Experience/PawnData configuration. It does not add a player-facing
character picker, persist appearance choices or replicate live skin changes.
Equipment remains attached to gameplay sockets. Matching visual hands, weapon
grips and sockets for different body proportions is a later character-specific
acceptance step.

For a local test, open `Lvl_RpgGaspMantle` with its saved Experience and no PIE
Experience override. The default profile leaves appearance unchanged. With PIE
stopped, a separately configured profile can supply a compatible TargetMesh and
Retargeter and use the provided RetargetAnimClass; select that profile on the
existing traversal PawnData before spawning players. Changing a profile's fields
in place during play is outside this static configuration contract. The exposed
`ApplyProfile` function only changes local presentation, so it is not a replicated
character-selection API.

## Verification

The SurvivalRpgEditor Win64 Development build passed on Unreal 5.8.2. Adding the
test translation unit exposed existing unity-build collisions between two test
files' anonymous namespaces; those namespaces now have distinct names, with
their test values and assertions unchanged.

Fresh-editor validation compiled both affected Blueprints with warnings treated
as errors, compared the AnimGraph links, retarget-node settings and bindings,
and preserved the complete curve-driven `UpdateRetargetProfile` graph. The
four-root dependency audit visited 1,451 packages without import backreferences.
Hashes cover 7,610 pre-existing Content/Plugin files: only the existing traversal
character and PawnData changed. The two new packages are the working AnimBP and
disabled default profile. All seven existing save files remain unchanged.

The initial two retargeting tests passed in 17.37 seconds. A listen-server PIE
test enables the existing Manny asset only as transient test configuration on
the remote owner, authority and late-joined observer. It checks changing limb
poses during movement, a real contextual GAS mantle, source montage/root-motion
ownership, equipment attachment, clear/invalid fallback, and a live retarget
node with an unresolved source. A test-scoped in-memory configuration of the
named default profile then exercises authoritative death, the owner's normal
respawn RPC, old follower cleanup and automatic PawnData initialization on all
three roles. RAII restores the original profile references even if the test
fails; no appearance change is saved.

The current player death path destroys the old pawn rather than retaining a
simulated player corpse. This test therefore verifies destruction/respawn, not
retargeted corpse-ragdoll quality. Packaged/dedicated-server sessions, arbitrary
target skeletons and body-specific equipment alignment are not covered. The
negative cases intentionally emit rejection warnings; existing voice,
temporary-map NetGUID and respawn-widget warnings remain visible.

The final combined regression run passed all 39 tests in 361.42 seconds,
covering camera, CMC, Mantle, Vault, Hurdle, runtime retargeting and remote melee.
The first combined 39-test regression run passed 38 tests. The existing
`ServerBlockedBackFloorRejectsPredictedHurdle` fixture timed out on its approach,
433 cm before the obstacle and before pressing Space or activating GAS. An
input-focus interruption is plausible from the log, but was not directly
measured. The unchanged isolated retry passed in 10.95 seconds; the original
failure and retry evidence are retained. The complete successful repeat used
the same code and assets; no gameplay or fixture change was made for the timeout.

Regression results are recorded in `Saved/GaspRuntimeRetarget20260912`, alongside
build logs, asset/graph audits, source hashes and preservation reports.
