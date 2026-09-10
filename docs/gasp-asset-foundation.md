# GASP asset foundation (2026-09-10)

Branch: `codex/gasp-rpg-asset-foundation`. The source is the GASP content
already imported into this repository at baseline commit `535b4123`, which
includes the listen-server animation fix `8f54d995`. The external GASP sample
checkout is a reference, not a runtime dependency.

This change establishes project-owned copies under
`/Game/SurvivalRpg/Characters/GASP`. It changes references inside those copies
and preserves the imported originals and existing RPG baseline. Experience,
PawnData, input and gameplay integration remain separate work; this step adds
no C++ classes or public APIs and activates no additional character.

## Dependency scope and layout

The inventory starts with `/Game/Blueprints/SandboxCharacter_CMC`,
`/Game/Blueprints/SandboxCharacter_CMC_ABP` and
`/Game/Blueprints/AC_TraversalLogic`. Recursive Asset Registry hard and soft
dependencies resolve to 2,138 project packages, approximately 2.4 GB before
copying. Scanning serialized package paths found no additional resolvable
packages beyond that closure. This scan supplements the registry; it is not
proof that arbitrary runtime-generated path strings are covered.

The [source-to-target manifest](assets/gasp-asset-map.json) records one unique
destination per source package, its class, source commit and both SHA-256
hashes. Shared Blueprint subfolders sit directly below `Shared`; other shared
content retains its path relative to `/Game`. Specific variants retain their
source-relative paths beneath their ownership folder:

| Destination below `Characters/GASP` | Packages | Content |
| --- | ---: | --- |
| `Shared` | 2,107 | Common animation, traversal, retargeting, rigs, meshes, audio, camera and supporting sample content. |
| `CMC` | 11 | CMC character, AnimBP and explicitly CMC-specific data/materials. |
| `Mover` | 19 | Referenced Mover character, AnimBP, movement modes and Mover-specific data/materials. |
| `Mover/Ragdoll` | 1 | Referenced `S_MoverCustomInputs_Ragdoll` structure. |

The closure includes 1,329 animation sequences, 72 montages, 149 Pose Search
databases, 27 Pose Search schemas and 12 Chooser tables. Sample GameMode,
PlayerController, Foley, camera, Smart Object and Mover dependencies are
retained where the source assets reference them. Inclusion preserves the
source dependency graph; it does not select these systems for RPG gameplay.
The closure does not include a separate Mover-Ragdoll pawn or any map.

Engine content, Control Rig and ACL plugin assets, and native `/Script`
dependencies remain shared. Existing SurvivalRpg and GameFeature content
would also be reused rather than copied; this inventory reaches no such
content packages.

Configured dynamic loading was inspected in the Blueprint graphs and defaults.
`GM_Sandbox.PawnClasses_Soft` selects CMC/Mover;
`GM_Sandbox.VisualOverrides_Soft` selects Manny/Quinn through the visual override
manager; `ABP_GenericRetarget.IKRetargeter_Map` selects
`RTG_UEFN_to_UE5_Mannequin`. All five targets and the configured Chooser
results/fallbacks are in the closure. No additional Asset Registry directory
query or string-built asset loading was found in the 65 Blueprint assets and
12 Choosers inspected.

The copy uses Unreal's native header-patching Advanced Copy through Unreal MCP,
followed by a native batch rename of the 31 variant-specific assets. The first
in-memory Advanced Copy attempt crashed while recompiling the duplicated Foley
Blueprint types; it saved no target packages and was discarded. Hash checks
confirmed that it did not change the originals or saves.

The two copied Blueprint Control Rigs required an additional native refresh
of their function dependency data. `LibraryNodePath` strings in cached
function headers survived the package copy even though their host objects
already pointed to the copies. Unreal rebuilt dependencies for 17 functions,
updating 29 referencing headers. Temporary unconnected reference nodes were
removed immediately; complete RigVM graph, pin, default, type and link
snapshots matched before/after the refresh and again after a fresh load.
Only the two copied rigs were saved. No additional nodes remain.

## Verification

Validation used **Unreal Engine 5.8.2, CL 56702186**, through Unreal MCP and
fresh read-only Unreal Python processes. Local detailed evidence is under
`Saved/GaspAssetFoundation20260910`; the committed manifest records the final
package hashes.

- All **2,138** new packages loaded in a fresh process and produced semantic
  snapshots plus native text exports. There were no missing package/export
  results.
- **65 Blueprints**, including six AnimBPs and three Blueprint Control Rigs,
  compiled with warnings treated as errors. Rig VMs were explicitly compiled.
  The StateTree passed manual data validation after compilation. These checks
  passed again in a fresh process; the two subsequently refreshed rigs also
  passed a separate fresh compile/validation.
- The final hard/soft dependency graph matches the explicit source-to-target
  map exactly: **zero missing dependencies, import backreferences, unexpected
  packages or redirectors**. The two rigs also have no remaining imported
  function-path references in their semantic exports.
- Blueprint/AnimBP authored graphs, CDOs and component hierarchies matched.
  The audit resolved **67,445 pins** by graph/node/name/direction and tracked
  **15,286 regenerated pin IDs** explicitly. Types, defaults, link order,
  persistent IDs and authored graph/node IDs remained checked. Only identified
  compiler products, generated pin-label keys and the transient FullBodyIK
  function-library editor graph GUID were classified as regenerated data.
- The StateTree retained **nine states and 18 bindings**. All binding endpoints
  resolve to their corresponding state/task/event roles. Its compiler hash
  changes with remapped object paths; the GUIDs and authored binding structure
  are preserved by the final copy method.
- Observed skeleton assignments, retarget assets, Chooser results, Pose Search
  configuration, animation/montage data, root-motion settings and notify data
  match the source after applying the package map. No skeleton adaptation or
  montage authoring was performed.
- Hash comparisons cover all **6,125 pre-existing tracked files** and all
  **seven existing save files**. No pre-existing content, code, configuration,
  baseline asset or save file changed.

The [validation summary](assets/gasp-asset-validation.json) accounts for all
**2,138 packages**: 2,089 have matching observed raw data after package remapping;
49 have precisely classified regenerated identifiers or compiler/cache data
(45 Blueprint/AnimBP, one FullBodyIK editor graph, one StateTree and two rigs).
There are **zero unexplained observed differences**. The unfiltered raw reports
remain in the local evidence directory; the classification does not erase them
or expand the inspection coverage described below.

Inspection limits remain explicit: Python does not expose every native or
protected field. Native text exports supplement reflected authored data, but
non-reflected payloads such as native skeleton retarget-source tables and
derived compressed/DDC data are not a bytewise semantic comparison. Existing
localized-function provenance strings under `/Game/Developers/Jeremie` remain
unchanged; they are origin identifiers, not configured loading targets.

No PIE session, gameplay/save operation, visual animation/network test, cook
or C++ build was performed for this asset-only change. Runtime movement,
montage/equipment compatibility and further skeleton integration belong to
the next integration step.

## Next integration boundary

UEFN remains the starting skeleton for the first CMC Experience on the
existing gameplay mesh. That integration must check RPG montage compatibility
and equipment sockets while preserving Lyra Experience/PawnData composition
and project-owned movement/GAS authority.

The retained `ABP_GenericRetarget`, Manny/Quinn assets, IK rigs and
`RTG_UEFN_to_UE5_Mannequin` preserve the sample's retarget dependencies. Further
visible skeletons are a later retargeting step after stable CMC integration.
The context-sensitive jump action and prepared traversal obstacles follow in
the order Mantle, then Vault and Hurdle. Mover and Mover-Ragdoll Experiences
follow separately; removal of imported originals is the final cleanup step.
