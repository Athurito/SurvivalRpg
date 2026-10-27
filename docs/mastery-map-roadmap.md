# Mastery map roadmap

Updated: 2026-10-27. Status: **In progress**; MST-01 is in review.

This is the execution roadmap for the [mastery map implementation plan](mastery-map-implementation-plan.md): a zoomable shared harvesting mastery map, meaningful talent choices, and the rune loot/crafting/upgrade loop. The plan owns design, architecture, and detailed acceptance contracts; this roadmap owns task status, order, and the current handoff.

## Scheduling decision

The user's order is **finish CMB first, then start MST** (2026-10-26). The [combat roadmap](combat-roadmap.md) remains a separate workstream with its own task IDs, scope, status, and acceptance. Merging the two MST documents records future work; it does not start MST or change CMB priorities.

On 2026-10-27, after CMB-01 to CMB-07 had merged, the user started MST with this plan. The CMB gate is open; the remaining tasks are Planned and follow the order below.

## Outcome and milestones

- **MST-02:** H opens the real CommonUI skill screen with cursor-anchored zoom, pan, selection, fixed-size details, readable focus, and view restoration after close/reopen.
- **MST-04:** axe and pickaxe use one harvesting mastery budget, with functional Wucht talents, atomic choice switching, XP, and migrated saves.
- **MST-06:** the first rune changes real harvesting and activates the matching learned talents on its source tool.
- **MST-08:** finding, learning a recipe, crafting, socketing, upgrading, and salvaging work as one persistent loop.
- **MST-09d:** all six runes and fifteen talents work together and the old harvesting UI/content paths are retired.

The full catalog goes beyond the first-playable minimum in [game-vision.md](game-vision.md), section 12. Deliver the narrow milestones in order; the first map does not wait for the entire rune or helper catalog.

## Task rules

- One bounded task per branch/PR, using `codex/mst-<id>-<slug>` or `claude/mst-<id>-<slug>` (for example `codex/mst-01-map-foundation`). Keep the IDs below stable. MST-09 is a milestone umbrella, not an implementation PR.
- Status vocabulary: Deferred, Planned, Ready, In progress, In review, Merged. Deferred means the CMB scheduling gate is still closed; Planned means future work after that gate opens. Record completion only with the actual PR and validation evidence.
- Once CMB is complete and MST is started, mark MST-01 Ready and keep dependent work Planned. Update only the current task's status/evidence and replace the current handoff; do not append a second competing progress log.
- Record each task's C++ boundary decision, file/asset ownership, actual checks, failures, limits, and next action in a `docs/mastery-map-<topic>.md` task report. Link the report and PR in the table when they exist.
- Reuse existing progression, inventory, equipment, screen routing, ViewModels, and CommonUI styles. The plan's native map mechanism does not authorize native screen/content leaf classes or a parallel rune manager.
- Author and validate concrete WBP, Blueprint, and DataAsset content through Unreal MCP. Coordinate editor ownership and binary assets; run one PIE network process at a time.
- Keep the CMB roadmap and CMB task reports owned by CMB. MST reports refer to their delivered contracts without renumbering, rescheduling, or reopening them implicitly.

## Tasks

All dependencies below are within MST; every row also has the CMB scheduling gate.

| ID | Scope | Dependencies | Status | Report / PR |
| --- | --- | --- | --- | --- |
| MST-01 | Free-position node/link schema, reusable native map geometry/input, LocalPlayer camera/selection cache; prove the surface with existing data | Post-CMB baseline refresh | In review | [mastery-map-foundation.md](mastery-map-foundation.md), [PR #302](https://github.com/Athurito/SurvivalRpg/pull/302) |
| MST-02 | Integrate `CUI_Skills`, node/details WBPs and existing MVVM/style; explicit learn/refund actions, focus, semantic zoom and close/reopen behavior | MST-01 | Planned | — |
| MST-03a | Quantized harvesting work, residual progress, true overkill and proc provenance through existing stock/reward commits; actor/instance replication and saves | Post-CMB baseline refresh; technically independent of MST-01/02 | Planned | — |
| MST-03b | Shared harvesting mastery/XP/tree/tool references, functional three-node Wucht branch, old-save migration and retirement of legacy harvesting allocations | MST-02, MST-03a | Planned | — |
| MST-04 | Atomic replacement of exclusive choices, owner request/result feedback, grant reconciliation and VM updates | MST-03b | Planned | — |
| MST-05 | Extend the delivered rune/item foundation with physical instances, ranks and three harvesting-tool sockets; validated moves, grants, replication and persistence | MST-04; CMB-07 contract re-inspection | Planned | — |
| MST-06 | Source-tool activation evaluator, rune context/highlights and learned-but-inactive UI; Brand and the first implemented rune-dependent talent path | MST-05 | Planned | — |
| MST-07 | Rune drops, guaranteed claims, persistent base-recipe knowledge and rank-I crafting through existing loot/station paths | MST-06 | Planned | — |
| MST-08 | In-place rank II/III upgrades, salvage and Runenschrein presentation; atomic costs and safe equipped-rune refresh | MST-07 | Planned | — |
| MST-09a | Harvesting Echo, Kraftuebertrag and Runenfluss; bounded own-hit triggers and actual-overkill transfer | MST-08 | Planned | — |
| MST-09b | Erntesprung, Durchschlag, Ausbreitung and remaining Nachwirkung choices; finite propagation and no duplicated work | MST-09a | Planned | — |
| MST-09c | Totenarbeiter and Helfer; temporary workers, correct beneficiary/reward attribution, lifecycle and bounded triggers | MST-09b | Planned | — |
| MST-09d | Full catalog/cutover acceptance, legacy cleanup, input/performance/network regression and cook reachability | MST-09c | Planned | — |

Default implementation order follows the table after CMB. MST-03a's technical independence documents an ownership boundary; it does not schedule concurrent work during CMB. Only expose purchasable talents once their gameplay exists. The Brand transfer choice therefore waits for Erntesprung in MST-09b.

## Post-CMB baseline refresh

The implementation plan was inspected on `65c8f8fb` before later CMB deliveries. Its class names and save versions are evidence from that snapshot. Before MST-01:

1. Read the then-current combat roadmap and relevant completed CMB reports, verify their merged commits and remaining accepted limits, and record the current base commit in the MST-01 report.
2. Re-inspect progression, equipment grants, item-instance state, save schemas, UI routing, and any CMB-07 rune/proc mechanism. Reuse and extend that foundation; the plan's proposed rune fragment types are conditional, not instructions to duplicate delivered types.
3. Preserve combat Echo's hit counting, damage and weapon behavior. Harvesting Echo consumes direct-harvest events with its own proc limits. Three equal harvesting-tool sockets do not redefine combat weapon socket policy.
4. Add a completed-CMB save fixture to the migration checks alongside supported older harvesting saves. Choose schema increments from the actual implementation baseline and preserve CMB rune instances, ranks if present, grants and explicit combat bindings.
5. Update only assumptions in the implementation plan that changed, then take file/asset ownership for MST-01. Recheck the rune contract again before MST-05 if it changed in the meantime.

## Acceptance and evidence

Each task uses the detailed contracts in the implementation plan. Run relevant checks through `python Build/Tools/Unreal/ue.py`; no historical result counts as a newly executed build, test or cook.

- Native map checks cover transforms, cursor anchoring, clipping, focus, drag/click arbitration, capture/focus loss, pooled or recreated screen restoration, and LocalPlayer cleanup.
- Gameplay checks cover server authority, owner-only progression, atomic choices, source-item activation, inventory transactions, once-only rewards/XP, replication, late join/reconnect and save migration.
- Asset checks cover compilation, intended parents, VM sources, stable references and cook reachability. Avoid tests that freeze cosmetic WBP structure or style.
- Observable changes include a short playtest with expected results; gameplay changes include another player's view. Verify map input/DPI/readability and compare real/synthetic-map performance when the map lands.
- PR descriptions use Behavior, Validation, Limits and, for observable gameplay, Playtest. Record exact evidence before marking a task Merged.

## Current handoff

- **Active phase:** MST, started by the user on 2026-10-27 after CMB-01 to CMB-07 merged.
- **Active task:** MST-01, branch `claude/mst-01-map-foundation`, [PR #302](https://github.com/Athurito/SurvivalRpg/pull/302); the report is [mastery-map-foundation.md](mastery-map-foundation.md), which records the baseline refresh at `29b4dae3`.
- **Delivered in MST-01:**
  - The layout schema (grid fallback, free positions, regions, default region) and the map read models.
  - `URpgSkillTreeMapWidget` with its Slate panel.
  - The LocalPlayer view cache in `URpgUIScreenSubsystem`.
  - No screen, asset or save change; `CUI_Skills` still shows the grid.
- **Next MST action:** after MST-01 merges, take MST-02: integrate the map into `CUI_Skills` through Unreal MCP. The editor and the `unreal-mcp` server must be running for that.
- **Rune contract for MST-05:** CMB-07 delivered `URpgWeaponRuneDefinition` and `URpgInventoryFragment_WeaponRunes`, with runes engraved on item definitions and no sockets yet. Recheck them before MST-05.
- **Later content decisions:** production XP and cost curves and the starter and guaranteed-reward pacing remain authored tuning. Shared-world rune recipe knowledge is the plan's proposed co-op default and must be settled before MST-07.
