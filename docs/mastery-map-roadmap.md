# Mastery map roadmap

Updated: 2026-10-27. Status: **In progress**; MST-04 is merged, MST-05 is in review.

This is the execution roadmap for the [mastery map implementation plan](mastery-map-implementation-plan.md): a zoomable shared harvesting mastery map, meaningful talent choices, and the rune loot/crafting/upgrade loop. The plan owns design, architecture, and detailed acceptance contracts; this roadmap owns task status, order, and the current handoff.

## Scheduling decision

The user's order is **finish CMB first, then start MST** (2026-10-26). The [combat roadmap](combat-roadmap.md) remains a separate workstream with its own task IDs, scope, status, and acceptance. Merging the two MST documents records future work; it does not start MST or change CMB priorities.

On 2026-10-27, after CMB-01 to CMB-07 had merged, the user started MST with this plan. The CMB gate is open; the remaining tasks are Planned and follow the order below.

## Outcome and milestones

- **MST-02:** H opens the real CommonUI skill screen with cursor-anchored zoom, pan, selection, fixed-size details, readable focus, and view restoration after close/reopen.
- **MST-03c:** the Skills tab is one map that fills the screen, with points per budget on top and a detail card at the selected node; Character keeps level and professions.
- **MST-10b:** harvesting and combat share the one map as clusters with their own budgets, and the first combat talents work.
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
- User decision, 2026-10-27: merge MST steps the user could test in the game once the build, the automated tests and the agent's own listen-server PIE check with two players pass. Collect each merged step's playtest in the [playtest backlog](#playtest-backlog); the user tests in batches, and findings become fixes in later PRs. Actual failures and limits are still reported.
- Author and validate concrete WBP, Blueprint, and DataAsset content through Unreal MCP. Coordinate editor ownership and binary assets; run one PIE network process at a time.
- Keep the CMB roadmap and CMB task reports owned by CMB. MST reports refer to their delivered contracts without renumbering, rescheduling, or reopening them implicitly.

## Tasks

All dependencies below are within MST; every row also has the CMB scheduling gate.

| ID | Scope | Dependencies | Status | Report / PR |
| --- | --- | --- | --- | --- |
| MST-01 | Free-position node/link schema, reusable native map geometry/input, LocalPlayer camera/selection cache; prove the surface with existing data | Post-CMB baseline refresh | Merged (2026-10-27, `293fce4f`) | [mastery-map-foundation.md](mastery-map-foundation.md), [PR #302](https://github.com/Athurito/SurvivalRpg/pull/302) |
| MST-02 | Integrate `CUI_Skills`, node/details WBPs and existing MVVM/style; explicit learn/refund actions, focus, semantic zoom and close/reopen behavior | MST-01 | Merged (2026-10-27, `a888a078`) | [mastery-map-skill-screen.md](mastery-map-skill-screen.md), [PR #304](https://github.com/Athurito/SurvivalRpg/pull/304) |
| MST-03a | Quantized harvesting work, residual progress, true overkill and proc provenance through existing stock/reward commits; actor/instance replication and saves | Post-CMB baseline refresh; technically independent of MST-01/02 | Merged (2026-10-27, `2554bb6c`) | [mastery-map-harvest-work.md](mastery-map-harvest-work.md), [PR #306](https://github.com/Athurito/SurvivalRpg/pull/306) |
| MST-03b | Shared harvesting mastery/XP/tree/tool references, functional three-node Wucht branch, old-save migration and retirement of legacy harvesting allocations | MST-02, MST-03a | Merged (2026-10-27, `d92bc04b`) | [mastery-map-shared-harvesting.md](mastery-map-shared-harvesting.md), [PR #308](https://github.com/Athurito/SurvivalRpg/pull/308) |
| MST-03c | Single-map skill screen: full-tab map, points per budget and zoom/region controls on top, node-anchored detail card with Learn/Refund; remove the profession/tree lists, the Q/E/R bar and the detail column | MST-03b | Merged (2026-10-27, `5d45019e`) | [mastery-map-single-map.md](mastery-map-single-map.md), [PR #310](https://github.com/Athurito/SurvivalRpg/pull/310) |
| MST-04 | Atomic replacement of exclusive choices, owner request/result feedback on the detail card, grant reconciliation and VM updates | MST-03c | Merged (2026-10-27, `08285c1e`) | [mastery-map-choice-swap.md](mastery-map-choice-swap.md), [PR #313](https://github.com/Athurito/SurvivalRpg/pull/313) |
| MST-05 | Extend the delivered rune/item foundation with physical instances, ranks and three harvesting-tool sockets; validated moves, grants, replication and persistence | MST-04; CMB-07 contract re-inspection | In review | [mastery-map-rune-sockets.md](mastery-map-rune-sockets.md) |
| MST-06 | Source-tool activation evaluator, rune context/highlights and learned-but-inactive UI; Brand and the first implemented rune-dependent talent path | MST-05 | Planned | — |
| MST-07 | Rune drops, guaranteed claims, persistent base-recipe knowledge and rank-I crafting through existing loot/station paths | MST-06 | Planned | — |
| MST-08 | In-place rank II/III upgrades, salvage and Runenschrein presentation; atomic costs and safe equipped-rune refresh | MST-07 | Planned | — |
| MST-09a | Harvesting Echo, Kraftuebertrag and Runenfluss; bounded own-hit triggers and actual-overkill transfer | MST-08 | Planned | — |
| MST-09b | Erntesprung, Durchschlag, Ausbreitung and remaining Nachwirkung choices; finite propagation and no duplicated work | MST-09a | Planned | — |
| MST-09c | Totenarbeiter and Helfer; temporary workers, correct beneficiary/reward attribution, lifecycle and bounded triggers | MST-09b | Planned | — |
| MST-09d | Full catalog/cutover acceptance, legacy cleanup, input/performance/network regression and cook reachability | MST-09c | Planned | — |
| MST-10a | Several skill trees with their own budgets as regions of the one map, Path of Exile style: tree placement as authored data, tree-qualified node identity, screen-level view cache, fit and shortcuts across trees, a budget entry per tree in the top bar and a point counter at each cluster; proven with the Harvesting tree and a synthetic second tree in automation | MST-03c | Merged (2026-10-27, `9f0dbcf0`) | [mastery-map-several-trees.md](mastery-map-several-trees.md), [PR #311](https://github.com/Athurito/SurvivalRpg/pull/311) |
| MST-10b | Combat tree: core mastery `Skill.Mastery.Combat` earned by kills through the existing kill XP action; one `DA_SkillTree_Combat` carried by the melee weapons, with Kampfrhythmus (1 point, +10 % attack damage) forking into the exclusive Konter (3 points, perfect block +50 % stagger damage and more stamina back) and Bollwerk (3 points, blocking costs 25 % less stamina and stops 15 % more damage) as granted GameplayEffects | MST-10a | Merged (2026-10-27, `a8ebbfe3`) | [mastery-map-combat-tree.md](mastery-map-combat-tree.md), [PR #312](https://github.com/Athurito/SurvivalRpg/pull/312) |

Default implementation order follows the table after CMB. MST-03a's technical independence documents an ownership boundary; it does not schedule concurrent work during CMB. Only expose purchasable talents once their gameplay exists. The Brand transfer choice therefore waits for Erntesprung in MST-09b.

The user decided on 2026-10-27 to make the skill screen one map for every tree. MST-03c comes before MST-04, so that MST-04 builds its feedback into the new detail card instead of the old panel. After reviewing MST-03c, the user moved MST-10 ahead of MST-04 the same day, with a Combat tree as the second cluster. MST-10 is split into MST-10a (the map for several trees) and MST-10b (the Combat tree); both come before MST-04. The user approved the Combat talents and the kill-based mastery XP of MST-10b on 2026-10-27.

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

## Playtest backlog

Playtests of merged MST steps that wait for the user's batch test. Remove a row once tested; findings go into later PRs.

| Task | Playtest | Status |
| --- | --- | --- |
| MST-02 | [Playtest](mastery-map-skill-screen.md#playtest): H, wheel and drag, select, Learn/Refund with points, zoom levels, reopen, keyboard/gamepad, two players | Waiting for the user |
| MST-03a | [Playtest](mastery-map-harvest-work.md#playtest): regression only, harvesting unchanged; pickaxe, axe, groves, swarm, two players on one vein | Waiting for the user |
| MST-03b | [Playtest](mastery-map-shared-harvesting.md#playtest): one Harvesting tree for axe and pickaxe, mastery XP, Cleaving Force, Shatter or Follow Through, no Q/E/R powers, two players | Waiting for the user |
| MST-03c | [Playtest](mastery-map-single-map.md#playtest): full-tab map and top bar, node card that follows its node, F/R and X/Y learn and refund, gamepad zoom, reopen, two players | Waiting for the user |
| MST-04 | [Playtest](mastery-map-choice-swap.md#playtest): Switch from Bulwark to Riposte with no free point, blocking follows, Shatter and Follow Through, two players | Waiting for the user |
| MST-05 | [Playtest](mastery-map-rune-sockets.md#playtest): a Brand rune in the pockets, axe and pickaxe unchanged in pockets, backpack and chests, two players; socketing comes with MST-06 | Waiting for the user |

## Current handoff

- **Active phase:** MST, started by the user on 2026-10-27 after CMB-01 to CMB-07 merged.
- **Active task:** MST-05, branch `claude/mst-05-rune-sockets`; the report is [mastery-map-rune-sockets.md](mastery-map-rune-sockets.md).
  - `URpgInventoryFragment_Rune` makes runes non-stackable items with a saved, replicated rank I to III; `bSocketGrid` makes an item-owned grid sockets with one rune of each kind.
  - Axe and pickaxe have three sockets; the equipment manager grants socketed runes while their tool is equipped. A temporary starter gives every player one Brand rune.
- **Previous task:** MST-04, merged on 2026-10-27 as `08285c1e` ([PR #313](https://github.com/Athurito/SurvivalRpg/pull/313)); the report is [mastery-map-choice-swap.md](mastery-map-choice-swap.md).
- **User decisions (2026-10-27):**
  - One map for every tree. After the MST-03c review, MST-10a and MST-10b come before MST-04, with Combat as the second cluster, and the game window has a minimum size of 1280 x 720.
  - Combat talents as proposed: Kampfrhythmus (+10 % base damage), then Konter (perfect block +50 % stagger damage, more stamina back) or Bollwerk (block 25 % less stamina, 15 % more damage stopped). Combat mastery XP comes from kills.
  - For MST-03b, the retired tool trees and powers and the Impact talents as recorded in its report.
- **Next MST action:** after MST-05 merges, MST-06. It starts by making tool sockets reachable in the inventory UI, which no screen opens yet: show the socket grids of the tools in the carry slots, outside first-fit placement and quick transfers.
- **Rune contract:** rechecked for MST-05. CMB-07's engraved weapon runes and the combat Echo stay unchanged; harvesting runes are separate rune items in `GF_Harvesting_Magic`.
- **Later content decisions:** production XP and cost curves and the starter and guaranteed-reward pacing remain authored tuning. Shared-world rune recipe knowledge is the plan's proposed co-op default and must be settled before MST-07.
