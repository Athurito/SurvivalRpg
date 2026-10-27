# Mastery map implementation plan

Date: 2026-10-26. Status: documented design; implementation started on 2026-10-27 with MST-01, after CMB-01 to CMB-07 merged.

Scheduling, task status, and the next implementation step live in the separate [mastery map roadmap](mastery-map-roadmap.md). User decision, 2026-10-26: finish CMB first, then start this system. Publishing these documents does not start implementation or change the [combat roadmap](combat-roadmap.md).

## Deliverable and scope

Replace the tool-by-tool skill-screen workflow with a CommonUI mastery map that supports continuous zoom, pan, spatial selection, fixed-size talent details, and equipment/rune context. Implement shared harvesting progression on the existing authoritative skill-tree foundation. Connect the rune acquisition and upgrade loop in subsequent bounded tasks.

The conversation's accepted direction supersedes the old per-tool harvesting/Q/E/R design when the relevant implementation step is completed. [skill-trees.md](skill-trees.md), [harvesting-roadmap.md](harvesting-roadmap.md), and the harvesting section of [game-vision.md](game-vision.md) currently describe that older implemented behavior; they are evidence of the baseline, not an instruction to retain the old harvesting design indefinitely.

The first deliverable is a usable zoomable map with existing project data. The first gameplay slice is one shared harvesting tree with the three Wucht nodes, real XP, learning/refunding, an atomic choice switch, and save/reconnect support. The full six-rune catalog and fifteen talents are a later expansion beyond the first-playable minimum in game-vision section 12.

This plan does not redesign combat skills, replace the inventory, change GASP movement, or implement new combat/magic regions. The map leaves room for those regions through authored data. Combat and harvesting retain distinct progression budgets, also once they share the map.

## Accepted interaction and gameplay contract

- One mastery screen and one navigable map. Area shortcuts move the view to a region rather than opening a separate tool skill tree.
- The map fills the Skills tab (user decision, 2026-10-27; MST-03c). Character level and professions stay on the Character tab. The screen has no tree list, no permanent Q/E/R bar and no fixed detail column. A slim top bar holds the available and spent points of each budget on the map, the region shortcuts once a second region exists, and the zoom and fit controls.
- Later skill trees with their own budget, for example combat or magic, become further regions of the same map, as in Path of Exile's passive tree, not separate screens or list entries (MST-10). Each tree keeps its own budget.
- Wheel zoom remains anchored at the cursor. Drag pans after a movement threshold; releasing a drag never buys, refunds, or selects a talent accidentally.
- Overview shows branches and connections; closer zoom reveals names and costs. Detail text, action buttons, and navigation stay at readable UI scale.
- A node click selects it. Learning, refunding, and switching an exclusive choice are explicit commands on the selected node's detail card. The present immediate left-click purchase/right-click refund behavior is replaced.
- The detail card opens next to the selected node at fixed UI size, follows it while the map pans or zooms, and stays fully inside the map. It shows name, effect, cost, state and requirements, the switch, pending and rejection feedback (MST-04, [choice swap](mastery-map-choice-swap.md)), and later the rune activation state (MST-06). Keyboard and gamepad reach its commands through bound CommonUI actions shown in the menu's key hints, without moving focus out of the map.
- Assigning a learned active ability to Q/E/R appears on that node's card only once a tree has active abilities; the HUD keeps showing the bound abilities. The assignment rules of `URpgSkillTreeComponent` and `URpgWeaponAbilityLoadoutComponent` stay unchanged.
- Focus navigation can reach every node. Moving focus brings an off-screen node into view. Zoom controls, region shortcuts, and explicit actions remain usable without a mouse wheel. Touch pinch uses the same camera model where the target platform supports it; controller navigation is validated before final acceptance.
- Learned, learnable, unaffordable, prerequisite-locked, excluded alternative, pending request, and learned-but-inactive states are distinguishable. Current effectiveness is separate from ownership.
- The current tool and its three rune sockets are context for the same harvesting tree, shown as a strip over the map rather than a separate column. Clicking a rune highlights related talents; highlighting changes no gameplay state.
- A missing rune suppresses a talent's current effect, not its purchase or saved learned state. Purchasing ahead of obtaining a rune is allowed.
- Each branch has a one-point entry and two mutually exclusive three-point choices in the initial design. Free switching works even with zero free points, provided the candidate allocation is valid. Parents cannot be refunded while required by learned descendants.
- UI prototype values such as ten points, twenty XP per outing, rank multipliers, loot cycles, and material costs are illustrative. Production curves, costs, rewards, and upgrade effects are authored assets, not copied JavaScript constants.
- The permanent runtime UI uses the existing Cinzel/Roboto, occult-register palette, node plaques, materials, and CommonUI styles from [ui-style-guide.md](ui-style-guide.md). The browser prototype establishes interaction and information hierarchy, not a replacement visual style or an embedded browser implementation.

## Observed project baseline

Original inspection baseline: `65c8f8fb` on `claude/vfx-08-tornado`; it was clean at the start of the planning task. MST-01 refreshed it on `29b4dae3`; [mastery-map-foundation.md](mastery-map-foundation.md) records what changed. This is a dated source snapshot, not the expected post-CMB implementation baseline. Before MST starts, inspect the completed CMB work, current source/save schemas, branch, concurrent ownership, and editor state, and refresh the affected assumptions below. In particular, CMB-07 may already supply the item-instance rune foundation. Each implementation step gets its own bounded `codex/mst-<id>-<slug>` branch/PR; do not mix it into a CMB or VFX branch.

| Existing seam | Observed behavior and consequence |
| --- | --- |
| `Source/SurvivalRpg/UI/RpgSkillTreeGridWidget.*` | Creates WBP entries in `SUniformGridPanel`; connections use row/column cell centers. Reuse entry contracts, but a real pan/zoom surface requires a different geometry/input mechanism. |
| `Source/SurvivalRpg/Mvvm/SkillTrees/RpgSkillTreeViewModels.*` | Root, tree, node, row, and link read models already exist. Nodes are reused by tag and refreshes are coalesced. Extend these models rather than introducing a second progression/UI manager. |
| `Content/SurvivalRpg/UI/Menus/GameMenu/GameMenu/Skills/CUI_Skills.uasset` | Live read-only UMG inspection confirmed a `CommonActivatableWidget`, left skill/tree lists, `GridScroll` hosting `TreeGrid`, Q/E/R assignment, and a right detail panel. |
| `Source/SurvivalRpg/UI/RpgUIScreenSubsystem.cpp` and `Core/Player/RpgPlayerGameplayInputRouterComponent.cpp` | H routes through `InputTag.UI.Skills` into `UI.Screen.GameMenu.Skills`. Keep the existing GameMenu route, payload, focus, Back, and retained-screen lifecycle. |
| `Progression/SkillTrees/RpgSkillTreeDefinition.*` | Costs, prerequisites, exclusive groups, grants, tuning, and a mastery skill tag exist. Layout currently uses `Row`/`Column`. |
| `Progression/SkillTrees/RpgSkillTreeComponent.*` | PlayerState-owned, server-authoritative, owner-only replicated, saved learned states. Points are derived per TreeTag from its mastery level. Atomic replacement and rune-context activation are missing. |
| `Progression/Skills/RpgTradeSkillProgressionComponent.*` | Tag-keyed profession progression already supports config/curves and persistence. Keep Logging/Mining, which also feed existing harvesting rules. |
| `Inventory/RpgInventoryFragment_SkillTree.*` | Different tools can reference the same tree and share its learned state. |
| `Equipment/RpgEquipmentManagerComponent.cpp` | Grants learned-node effects/tags/abilities using the concrete equipment instance as source. Existing grant removal and itemization refresh are useful lifecycle examples. |
| `GF_Harvesting_Magic/.../Harvesting/RpgHarvestRewardService.cpp` | Successful harvesting has a common XP award seam. Actor and instanced-resource harvesting already converge on it. |
| `GF_Harvesting_Magic/.../Harvesting/RpgHarvestStockRules.cpp`, `RpgHarvestProfile.h`, `RpgGameplayAbility_Harvest.h` | Stock consumption uses integer requested/weak-point sections. Existing `HarvestPower` scales yield-sensitive loot, not depletion work. Percentage work, fractional burn/echo work, and actual overkill are additional mechanics. |
| `Core/Game/RpgPlayerSaveData.h`, `RpgGameModeBase.cpp` | Player schema is 4, independently of world schema 5. Trade skills restore before skill-tree purchases. Unknown/unregistered tree states can survive a load and need explicit migration handling. |
| `Inventory/RpgInventoryItemInstance.*`, `RpgInventoryItemDefinition.h`, `RpgInventoryFragment_ItemContainer.*` | Persistent item IDs, item-owned containers, authoritative graph mutations, and versioned fragment state already exist. Reuse these for physical runes and sockets. |
| `GF_Runes_Core` | Plugin/content and rune VFX exist. Since CMB-07, `URpgWeaponRuneDefinition` and `URpgInventoryFragment_WeaponRunes` engrave runes on item definitions, which drives the combat Echo. MST-05 added rune item instances with ranks and tool socket grids ([rune sockets](mastery-map-rune-sockets.md)); rune transactions are still missing. |
| `Crafting/RpgRecipeUnlockComponent.*`, `RpgCraftingRecipeDefinition.*` | Existing recipe unlocks are GameState-wide/session-replicated. Station recipes use connected physical storage and output chests. Persistent rune recipe knowledge and in-place instance upgrades need explicit integration. |

## C++ boundary decision

- **Classification:** reusable native map geometry/input plus extensions to existing progression, inventory, equipment, and save schemas/mechanisms. Concrete layouts, talents, runes, costs, effects, and screens remain designer-owned content.
- **Runtime truth:** `URpgSkillTreeComponent` owns learned choices; `URpgTradeSkillProgressionComponent` owns XP. Inventory graph/item instances own rune identity, location, and rank. Equipment owns applied grants. UI camera, selection, and highlights are local presentation state.
- **Existing seam:** the skill-tree definitions/components/ViewModels, screen route, native grid entry contract, inventory fragment/transaction/persistence seams, equipment grant reconciliation, harvesting reward service, and crafting station infrastructure listed above.
- **Ownership:** C++ handles transforms, clipping, hit testing, input capture, focus geometry, server validation, atomic mutations, replication, save migration, and shared activation evaluation. CommonUI WBP owns screen/node/detail layout, labels, icons, styling, animations, and presentation by zoom level. DataAssets and definition assets own positions, regions, requirements, costs, curves, and gameplay grants. Concrete abilities remain `GA_*` Blueprint assets.
- **New native classes for the map:** one reusable `URpgSkillTreeMapWidget` UMG wrapper and one internal `SRpgSkillTreeMapPanel` Slate implementation. Two classes, only one reflected. Their justification is the shared geometry/input/hit-test/focus mechanism, not screen styling. Extend existing definitions/ViewModels; no new screen manager or progression component.
- **Single-map screen:** MST-03c is WBP content on `CUI_Skills` and its node entry. If the detail card cannot follow its node from the existing entry geometry without a per-frame Blueprint tick, the map widget may gain one read-only accessor or event for a node's position in widget space; no new class. MST-10 extends the same two map classes to several trees; no new class, manager or progression component.
- **Later rune schema allowance:** at most two proposed native semantic fragments, `URpgInventoryFragment_Rune` and `URpgInventoryFragment_RuneSockets`, if the post-CMB foundation cannot already express those semantics. Names are proposals, not required new classes. Reuse and extend CMB-07's actual item-instance rune schema, grants, and lifecycle before considering either type. Use existing inventory/equipment owners for mutation; no parallel rune inventory manager.
- **Harvest-work foundation:** extend the existing stock/request/result rules and resource state owners with work/residual/provenance semantics. No separate resource-health or harvest manager is planned. Revision, reward commit, replication, and persistence belong to the native harvesting mechanism; work quantities and rune effects remain authored content.
- **Asset work:** use Unreal MCP discovery and the narrow Blueprint, DataAsset, UMG, MVVM, and object tools to inspect, author, compile, save named packages, reload, and validate. A confirmed tooling gap may justify a small reusable editor-only helper, never a native content leaf.

## Architecture and data changes

### Map presentation

Extend the existing tree/node schema with an explicit legacy-grid/free-position layout mode, authored local 2D positions, stable branch/region IDs, and region bounds/default focus. Existing tree identity and NodeTags remain the keys. Link read models expose source/target identities and positions; actual prerequisite edges come from gameplay data. Decorative connections to a visual center must not invent prerequisites or point costs.

The map projects world coordinates into a clipped viewport with one pan/zoom transform. Node hit testing and connection drawing use the same transform. Keep readable minimum hit targets and semantic zoom levels. WBP entries receive zoom-detail state without rebuilding every node on each wheel event.

Reuse node ViewModels by identity; update affected states on progression/equipment changes. Recalculate layout only when its inputs change. Pan/zoom invalidates geometry/paint, not the authoritative tree or all VM objects. Validate larger synthetic maps before adding virtualization; do not create a custom graph editor framework or depend on the editor-only GraphEditor module at runtime.

Camera and selected node survive refresh and reopen locally. Extend the existing `URpgUIScreenSubsystem` with a small presentation-state cache keyed by map identity, scoped to its LocalPlayer: world-space center, zoom, and selected-node tag only, with no widget/VM references. Restore it for both newly created and pooled screen widgets; clear it at LocalPlayer deinitialization. A retained CommonUI stack alone does not preserve state after an actual screen close. Initial delivery needs session retention only; keep this cache out of replicated progression and world/player gameplay saves. Closing/deactivating the menu, losing pointer capture, or losing window focus cancels any drag and restores normal click/input behavior.

### One screen for every tree

MST-03c turns `CUI_Skills` into the single-map layout of the interaction contract. It removes the profession and tree lists, the Q/E/R bar with Assign Ability, and the fixed detail column, and gives the map the whole tab below the menu's tab bar. The top bar binds each listed tree's points from the existing tree ViewModels; Reset tree stays an explicit command beside its budget. The selection, drag and zoom hints move into the menu's key hints. The zoom buttons leave the map's corner, where MST-02 found they could cover a label. The existing H/GameMenu/Back route, retained-screen lifecycle, focus handoff, semantic zoom and view restoration stay as MST-02 delivered them. Tree switching through a list ends here; the Harvesting tree is already the only one after MST-03b.

The detail card is part of the screen WBP, not of each node entry, so only one card exists. It reads the selected node's ViewModel, places itself beside the node on the side with room, and closes when the selection clears. Its commands call the same validated requests as today's panel; it adds no optimistic state.

MST-10 is split by the user's decision of 2026-10-27 and comes before MST-04: MST-10a builds the map for several trees and proves it with a synthetic second tree in automation; MST-10b adds the Combat tree as the first real second cluster (below). The map then hosts several tree ViewModels, each placed by authored data at its own position on the map. Hit testing, focus, selection and links use the tree together with the NodeTag, because NodeTags only need to be unique per tree. The LocalPlayer view cache is keyed by the screen's map instead of one TreeTag and stores the selected tree with its node. Fit frames all trees, and the region shortcuts span every tree. Points stay derived per TreeTag; do not merge trees into one definition asset, which would give them one shared budget. Validate the combined map against the synthetic 200-node performance comparison before adding virtualization.

Each tree shows its budget twice: an entry in the top bar (name, level, free and spent points, Reset tree) and a point counter at its cluster on the map, as in Path of Exile's atlas. The counter is a per-tree widget the map places just below the tree's lowest node, so the map classes gain one entry class for trees beside the node entries (MST-10a, [report](mastery-map-several-trees.md)).

**Combat cluster (MST-10b, user decision 2026-10-27):**
- **Points:** a core mastery `Skill.Mastery.Combat`, earned by kills through the existing kill XP feature action, next to character XP. No XP per hit, so training targets cannot be farmed.
- **Tree:** one `DA_SkillTree_Combat` carried by the melee weapons, so switching weapons keeps the learned talents.
- **Talents:** granted GameplayEffects on attributes the combat code already reads, or a tuning the attack reads; values are tunable in the asset.
  - **Kampfrhythmus (Battle Rhythm), 1 point:** +10 % attack damage through the tuning tag `Ability.Tuning.Attack.Damage`. The player's `BaseDamage` attribute is a flat bonus of 0, so a percentage on it would do nothing (MST-10b, [report](mastery-map-combat-tree.md)).
  - **Konter (Riposte), 3 points:** a perfect block deals +50 % stagger damage and restores more stamina.
  - **Bollwerk (Bulwark), 3 points:** blocking costs 25 % less stamina and stops 15 % more damage.
  - Konter and Bollwerk exclude each other.

### Shared harvesting progression

Create **one** `DA_SkillTree_Harvesting` with all five branches and one stable TreeTag. All applicable harvesting tools reference this asset. Do not create five trees sharing one mastery tag: today's point accounting would give each tree its own full budget.

Add one configured shared harvesting mastery tag through the existing trade-skill schema. Successful harvesting grants its configured shared XP exactly once, in addition to existing profession XP. Do not implement generic parent-tag XP propagation. Rejected, stale, duplicate, or failed-reward harvest requests produce no extra XP; portal loot grants no harvesting XP merely for being collected.

Implement one server request for replacing an exclusive choice. Construct a candidate state, refund the old choice in that candidate, validate budget, prerequisites, point gates, and resulting allocations, then commit once. Failure leaves purchases and grants unchanged. Return a clear result to the owner; the UI shows pending/rejection feedback and does not invent an optimistic successful purchase.

Keep node ownership and activation separate. Author a rune/tool activation query on the node and evaluate it through one native function against the **source equipment item's** rune context. Grants, numeric tuning, and the UI all use that evaluator. A global ASC tag from another item must not accidentally activate the node. Removing a rune or changing tools reconciles grants/tuning without deleting purchases.

### Harvest work before percentage-based gameplay

Do not implement Spaltkraft, burn ticks, Echo fractions, or Kraftuebertrag by modifying the existing `HarvestPower`: that field feeds loot yield. Today `FRpgHarvestStockRules` consumes whole `RequestedSections` plus weak-point bonus sections. Simply rounding every 25-percent bonus to whole sections would also erase or distort the proposed choices.

Add a bounded native work contract before the functional Wucht/rune pilot. Recommended model: deterministic quantized work with residual progress inside the existing reward sections. Keep sections as the once-only stock/reward/XP boundary. Extend existing stock snapshots and actor/instance state rather than maintaining a parallel HP pool.

Specify and test accepted work, work required, section crossings, actual remaining work, true overkill, and root-action/direct-versus-secondary provenance. An accepted direct hit can count toward Echo even if it has not yet crossed a reward-section boundary; it must not award XP for an unearned section. A weak-point/critical result must be server-validated. Threshold completion does not manufacture carryable overkill.

The same evaluator/commit behavior serves normal hits, burn, echoes, helpers, and propagation. Positive partial work advances the revision so concurrent/stale requests cannot reuse it. A failed reward transaction must not leave a partially committed stock/work change. Extend actor/instance parity, stock-shard replication, saved resource state, and legacy-stock migration; measure the extra instance-state/network cost. This is a gameplay foundation task independent of the map UI.

### Physical runes and the workstation loop

Runes are non-stackable concrete item instances for this initial implementation. Each keeps its persistent ID and rank when moved. Three equal sockets use item-owned storage/graph locations; socket occupancy is not duplicated in a second authoritative array. Every insertion path must enforce tool compatibility and one rune kind per tool, including generic drag/drop and transfers.

The three-socket policy is for the harvesting tools in this plan. Preserve the completed CMB combat-weapon socket/rune rules and combat Echo trigger. Harvesting Echo uses its own accepted direct-harvest events and proc limits; sharing rune infrastructure does not imply sharing combat hit counters or damage behavior.

Configure rune identity, I/II/III behavior, grants, requirements, recipe association, upgrade costs, and salvage outputs as content. Rank belongs to the concrete item and is saved/replicated through versioned instance state. Upgrading an equipped rune reconciles its grants without double-granting; switching a socket does not clone the rune.

Use the existing pickup/loot and station transaction paths. Discovering a rune unlocks its base recipe without consuming it. Crafting produces a new rank-I instance. Upgrade mutates the selected instance in place after complete cost/ownership/station validation. Salvage consumes one eligible unsocketed instance and awards its configured essence. Failed validation/capacity checks must not consume materials or the rune.

**Proposed co-op default:** base rune recipes are shared knowledge of the current world, extending the existing GameState recipe-unlock model, and are saved with that world. Rune items/ranks and talent allocations remain player/item-owned. This scope was not settled by the browser prototype; it is a design recommendation for review, not an already implemented personal unlock system. If personal recipes are desired, resolve that before the recipe step rather than silently changing all crafting ownership.

Loot tables and a guaranteed selectable reward use authored encounter/reward data. Persist claim identity so reconnect cannot claim twice. A full inventory must preserve a claim or use the established physical reward-delivery path. The browser's repeating reward cycle and free starter selection are prototype pacing examples; the production starter milestone and guaranteed-reward source are authored content.

### Existing-save migration

Prepare and validate a migration before normal trade-skill/tree restoration, then apply through the existing host-owned save pipeline. Choose the next player schema/migration marker from the actual post-CMB checkout; do not assume it is still schema 4 or confuse it with the separate world schema version. Include a save produced by completed CMB, preserving its combat rune instances and grants, alongside the original legacy fixtures.

1. Preserve original Logging/Mining state. Reconstruct cumulative eligible harvesting XP from stored level/rest XP using the frozen legacy curves, then map it to the new mastery curve. Use an explicit eligible-profession list and overflow/cap handling.
2. Initialize the shared mastery once. Derive its budget from the new curve. Do not sum old available/spent points or add a second currency refund on top of migrated XP.
3. Start the new tree with its migrated budget unspent. Do not map old active spells onto unrelated new passive talents by name.
4. Retire legacy Axe/Pickaxe purchases and only their harvesting Q/E/R assignments. Keep combat and unrelated explicit bindings intact. Retain legacy tag/asset lookup support long enough to read supported saves.
5. Reject new commands/grants for retired harvesting trees on authority; hiding old entries in the UI is insufficient. Asset-manager discovery must not reactivate retired content.
6. Test a second migration/load for idempotence, late feature registration after restore, schema-4 fixtures, missing assets, and failed validation without partial application. Preserve a pre-migration fixture/backup for rollback.

Before the migration task merges, freeze the legacy curve fixture and the new authored progression curve in its report. The exact production curve is balance data; a missing legacy curve must produce an explicit supported fallback/migration result, not silently erase progress.

## Bounded implementation sequence

All rows describe planned scope; their current execution status is **Deferred until CMB is complete**, tracked only in the [mastery map roadmap](mastery-map-roadmap.md). Dependencies define technical order within MST, not permission to start during CMB. Every step includes its own relevant build, automated checks, and observable playtest before moving on.

| ID | Deliverable and ownership | Dependencies | Acceptance |
| --- | --- | --- | --- |
| MST-01 | Free-position layout schema, link identities, native map wrapper/Slate panel, LocalPlayer presentation cache. Use existing tree data to prove rendering; no economy changes. | None | Cursor-anchored zoom, drag threshold, clipping, fit, resize/DPI, selection, and focus geometry work; legacy layout fallback still loads. Capture/focus loss and screen deactivation cancel a gesture cleanly. |
| MST-02 | Integrate the map into `CUI_Skills` and update node/detail WBPs using the existing style and MVVM. Preserve H/GameMenu/Back routing. Select-only node clicks; explicit existing learn/refund actions. | MST-01 | The real in-game menu opens a movable/zoomable tree; drag never purchases; detail panel stays readable; close/reopen restores the view for newly created or pooled widgets and releases input. Legacy tree switching is temporary until MST-03b. |
| MST-03a | Quantized harvest work/residual/overkill and provenance in the existing stock/commit mechanism, including actor/instance replication and persistence. | None; can run independently of MST-01/02 | Fractional work changes depletion without increasing total yield; rewards/XP occur once per section; concurrent, stale, failed, and restored requests preserve stock and work. |
| MST-03b | Shared harvesting mastery/XP, common tree reference on tools, migration and retirement policy; start with functional Wucht entry and two choices. Replace tool-tree navigation with shared-map region navigation. | MST-02, MST-03a | Axt/Spitzhacke share one budget and learned state; actual harvest XP advances it once; old saves migrate predictably; unrelated combat state survives. |
| MST-03c | Single-map `CUI_Skills`: full-tab map, top bar with points per budget, region shortcuts and zoom/fit, a node-anchored detail card with Learn/Refund, and removal of the profession/tree lists, the Q/E/R bar and the detail column. WBP content; at most one read-only node-position accessor on the map widget. | MST-03b | H opens a map that fills the tab; selecting a node opens one fixed-size card beside it that stays on screen and readable at every zoom and follows the node; Learn/Refund work by mouse and by bound keyboard/gamepad actions; drag never triggers them; close/reopen restores view, selection and card; a client in a two-player listen server learns only for itself. `CUI_Skills` compiles and still hosts the map. |
| MST-04 | Atomic exclusive-choice replacement, owner result/pending feedback on the detail card, grant reconciliation, and VM updates. | MST-03c | Swap at zero free points succeeds; invalid candidates leave state unchanged; repeated/stale client actions cannot overspend or duplicate grants. |
| MST-05 | Physical rune/rank and socket schemas integrated into inventory graph, authoritative insertion/removal, equipment projection, replication, and save round-trip. First test with one authored rune. | MST-04 | Three equal sockets; duplicate-kind prevention; no lost/copied rune on move/drop/load; rank follows the instance; equipped changes refresh once. |
| MST-06 | Shared node-activation evaluator, real rune-context bar, dormant state/highlights, and first functional rune-dependent talent path. Implement Brand and its implemented talents first; the transfer choice waits for Erntesprung. | MST-05 | A learned Brand-dependent talent activates only on the correct tool, sleeps when the rune is removed, and reactivates without repurchase. Real secondary harvest work has correct ownership/rewards and is safe in co-op. |
| MST-07 | Rune acquisition, permanent recipe knowledge, rank-I crafting, and guaranteed reward claims through existing loot/station infrastructure. | MST-06 | Find a rune, learn its recipe, craft a distinct copy, reconnect, and retain recipe/claim state. Capacity failure does not lose rewards or costs. |
| MST-08 | Instance-specific I→II→III upgrade and salvage transactions, integrated Runenschrein presentation. | MST-07 | Exactly the chosen instance changes rank, including while equipped; insufficient costs have no side effects; salvage rejects socketed runes and keeps recipe knowledge. |
| MST-09 | Complete the six-rune/fifteen-talent content set in separate bounded content slices; final cutover, cleanup, input/performance/network acceptance. | MST-08 | Every purchasable talent has implemented gameplay, all rune/branch combinations are consistent, legacy harvest paths are retired, combat routes survive, and packaged assets resolve. |
| MST-10a | Several skill trees with their own budgets on the one map: tree placement as authored data, tree-qualified node identity, a screen-level view cache, Fit and region shortcuts across trees, a budget entry per tree and a point counter at each cluster. Extends the existing map classes and ViewModels. | MST-03c | Both trees appear as regions of one map; learning in one never spends the other's points; focus, selection, fit and close/reopen work across the trees; the combined map meets the synthetic-map performance comparison. |
| MST-10b | Combat tree with kill-based combat mastery and the three approved talents as granted effects on melee weapons. | MST-10a | Kills advance the combat mastery once each; its points learn only combat talents; the talents change damage and blocking in combat and stay with every melee weapon; a client learns only for itself. |

MST-09 is an umbrella, split into the roadmap's MST-09a through MST-09d PRs: Echo/Kraftuebertrag and Runenfluss; Erntesprung/Durchschlag and Ausbreitung/Nachwirkung interactions; Totenarbeiter and Helfer; final catalog/cutover verification. Reuse existing Grave Swarm lifecycle/beneficiary ideas and harvest-chain infrastructure after inspecting their exact compatibility; do not assume their current active-spell behavior already implements the proposed passive rune.

The first reviewable UI milestone is MST-02; MST-03c gives the screen its final single-map layout. The first real shared-mastery gameplay milestone is MST-04. The first complete rune-driven gameplay slice is MST-06. This prevents the zoom UI from waiting for the entire loot/crafting/AI catalog.

## Target content catalog

| Branch | Entry, 1 point | Exclusive choices, 3 points each | Rune context |
| --- | --- | --- | --- |
| Wucht | Spaltkraft | Zertruemmern / Nachsetzen | Own critical harvest hits |
| Nachwirkung | Anhaltende Macht | Entladung / Wandernde Wirkung | Brand; transfer additionally needs Erntesprung |
| Ausbreitung | Weite Bahnen | Gabelung / Tiefer Riss | Erntesprung or Durchschlag |
| Helfer | Gemeinsamer Hieb | Rotte / Werkmeister | Totenarbeiter |
| Runenfluss | Runenresonanz | Teilung / Verdichtung | Echo or Kraftuebertrag |

Display names remain localized FText; ASCII spellings above are documentation shorthand, not new gameplay-tag names. Node identity uses stable authored tags.

Rune identities: Brand, Erntesprung, Durchschlag, Totenarbeiter, Echo, Kraftuebertrag. Ranks change authored power/capacity, while talent choices change behavior. Own direct hits drive Echo/helper triggers. Follow-up work carries beneficiary/source/proc provenance and a finite chain budget; helpers do not summon helpers and echoes do not trigger an unbounded echo chain. Brand preserves the ordinary resource output in this accepted baseline. Only actual overkill can enter Kraftuebertrag; finish-threshold talents do not manufacture it.

Do not publish placeholder talents that spend points but have no implemented effect. During staged content work, unavailable future nodes may appear only as clearly non-purchasable previews in development content. Keep all runtime availability decisions server-validatable.

## Validation and playtest contract

Existing filters to reuse and extend, not claims of tests run in this planning task:

- `SurvivalRpg.Progression.SkillTrees.*` and `SurvivalRpg.Progression.TradeSkills.*`.
- `SurvivalRpg.Network.SkillTreePIE.ClientRequestsAreValidatedAndReachOnlyTheOwner`.
- `SurvivalRpg.Save.WorldSave.MemoryRoundTrip` and relevant inventory graph/runtime-state transaction tests.
- `SurvivalRpg.UI.ScreenRouter.RetainedStackLifecycle` and `SurvivalRpg.UI.ScreenRegistry.Validation.AuthoredAsset`.
- Existing harvesting commit/reward, actor/instance parity, and content contracts. Update old Axe/Pickaxe-specific tree expectations at cutover; do not merely delete them.

Add stable native tests for cursor-anchor invariance, inverse transforms, zoom limits, fit/resize/DPI, drag-versus-click arbitration, spatial focus, selection preservation, cleanup, and invalidation. Test authority, atomic replacement, source-specific activation, duplicate prevention, rune transfer/rank persistence, recipe scope, claim idempotence, and migration. Keep replication tests for owner-only progression, late join/reconnect, FastArray/OnRep refresh, equipment grants, and visible effects for other players.

Validate WBP compile status, intended parentage, VM source resolution, required data/assets, screen registry, and cook reachability. Do not freeze cosmetic widget names, exact layout, node colors, binding counts, or Blueprint graph absence in C++ tests.

Commands follow the repository wrapper, with affected filters/maps selected per task:

```powershell
python Build/Tools/Unreal/ue.py build
python Build/Tools/Unreal/ue.py test SurvivalRpg.Progression.SkillTrees --null-rhi
python Build/Tools/Unreal/ue.py test SurvivalRpg.Progression.TradeSkills --null-rhi
python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.SkillTreePIE
python Build/Tools/Unreal/ue.py cook <verified-affected-map>
```

Do not use null RHI for rendered PIE/network/UI checks. Discover the concrete tests/map before running; new test names and counts are recorded only once they exist. Building requires coordination with the running editor/Live Coding state.

Manual acceptance starts with H, wheel/drag/region focus, node selection, explicit purchase, zero-free-point choice switch, rune insertion/removal, tool switch, and closing/reopening. Repeat at 1080p, 1440p, ultrawide, and supported DPI scales with mouse/keyboard and controller. Keep labels and targets readable at minimum zoom; provide focus/autopan rather than making off-screen nodes unreachable.

Measure map open, sustained pan/zoom, and state refresh on the fifteen-node tree plus a synthetic larger map (for example 200 nodes) against the same idle-menu baseline. Confirm that pan/zoom does not allocate a new VM/entry set each frame and does not send gameplay RPCs. Profile before adding pooling/virtualization beyond the existing reuse mechanisms.

## Implementation handoff and documentation

- After CMB is complete, start with MST-01 in an isolated branch/worktree, refresh the post-CMB baseline, and check current repository/editor ownership. Reuse the existing project UI style, input route, and data sources. Until then, the separate roadmap has no active or ready implementation task.
- Coordinate the shared Unreal Editor before asset authoring. This planning session found active PIE: the asset-contract `editor_status` tool refused inspection until PIE stops. It was not stopped. A separate read-only UMG tree inspection succeeded; no compile, reload, save, or editor mutation was performed.
- MCP toolset discovery confirmed UMG, MVVM, Blueprint, DataAsset, object, and project asset-contract tooling are available. Rediscover capabilities in the implementation session.
- Keep screen/data/GameplayAbility assets in the established UI and owning GameFeature content folders. Generic native schema/mechanisms stay in the existing core module; harvesting-specific behavior stays in `GF_Harvesting_Magic`, rune content in `GF_Runes_Core`.
- At the relevant completed step, update `docs/skill-trees.md`, the affected harvesting-roadmap sections, and game-vision's obsolete harvesting paragraph. Link this plan from the UI roadmap when implementation is scheduled. Preserve historical completed task evidence instead of rewriting past results.
- Each implementation PR records Behavior, Validation, Limits, and Playtest, with its MST ID, actual checks, exact changed assets, and next action.

## Planning validation and limits

Completed: repository/source/documentation inspection; independent UI and progression audits; read-only live UMG structure inspection; Unreal MCP capability discovery. The source inspection supports the ownership and reuse decisions above.

Not performed: UE build, automation execution, cook, runtime input/performance testing, asset compilation, or migration of an actual save. Browser prototype validation is not UE validation. Production XP/cost curves, the proposed shared-world rune recipe scope, and the later rune catalog still require the implementation/content acceptance described above.
