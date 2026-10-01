# Physical storage implementation

Status: implemented and validated locally, 2026-09-30. Branch: `codex/physical-storage`.

## Accepted behavior

- Physical chests own concrete inventory instances; there is no material-count ledger in the new flow.
- Costs consume the acting player's inventory first, then eligible storage. Other players' inventories are never sources.
- Base areas are horizontal circles and must be disjoint, including their boundaries. A workstation inside a base uses that whole base. An outside workstation uses its positive 3D radius, including individual chests inside a base when they are in range. Sources never extend transitively through another station or base.
- Deposit targets are exact-item assignments, then matching category assignments, then other chests already containing that exact material. Within each tier: largest exact-material count first, earliest matching assignment order second, stable container ID last. Fill each target before advancing. Overflow may advance through all tiers; leftovers remain at the source.
- Assignment slots contain an item definition or hierarchical material GameplayTag. Slots are combined with OR. Manual deposits do not learn assignments. Explicit assignments persist at zero stock. Recreating a rule gives it a new server-issued order.
- Workstation outputs default to the tray; automatic deposit is configurable. Trays are material sources in either mode. Blocked output or refunds remain pending rather than becoming world drops.
- Chest construction and same-base relocation use a preview and server placement checks. Upgrades preserve external footprint, item positions, identity and contents. Prototype grids are 6x4, 6x6 and 6x8.
- Moving starts with a server-authorized direct interaction. The same pawn may then walk beyond the original chest's interaction radius inside that base. Confirmation requires its controller-scoped session, unchanged chest settings, a reachable target and valid ground/footprint. Cancellation, respawn and a committed move invalidate the authorization. Inventory contents may change normally while the preview is open.
- World persistence includes physical actor identity, placement, metadata and inventory, plus crafting queues and outstanding refunds. Old prototype saves may be reset; no legacy migration is required.
- New inventory test map uses the approved project-local GASP blocks, grid and LevelVisuals from `Lvl_RpgGaspMantle`. Existing CMC gameplay composition is reused.

## Ownership

Existing inventory managers own items; existing container components own settings. BaseCamp and BaseStorage provide area validation, registration and routing. Existing GameMode owns world-save lifecycle. Existing controller UI actions own client requests. C++ owns schemas, authority, transactions, replication and persistence. Concrete definitions, actors, screens and preview presentation remain Blueprint/DataAsset/Widget Blueprint content, authored and validated with Unreal MCP.

## Work packages

- [x] Physical container metadata, assignment contracts and prepared multi-inventory transactions.
- [x] Base areas, source/target resolution, actor reconstruction and world persistence.
- [x] Physical crafting, durable queues/refunds and player-first construction costs.
- [x] Controller commands, assignment UI, chest upgrades and placement/relocation.
- [x] Concrete assets, isolated GASP test map and reference-safe legacy removal.
- [x] Editor build, focused automation, asset compilation, multiplayer/late join, manual UI and cook validation.

## Verification

Initial implementation results (before the movement/concurrency follow-up below):

- All four local NetworkPrediction/Mover overrides passed `prepare.py verify` before implementation.
- Editor Development build 08 succeeded without compiler warnings (22.24 seconds).
- `AutomationFinal08`: **213/213 passed**, zero failures or skipped tests; five passed with warnings. Coverage includes inventory, physical commands, source/routing rules, crafting, save, screen registry and network replication. The warnings cover deliberately rejected request collisions, existing GASP pose assets, isolated UI fixtures and CQTest's temporary unsaved level/WorldSettings.
- The dedicated-server test with two remote clients and a third late joiner checks real assignment/deposit/upgrade RPCs, server-issued rule order, player-first costs, unchanged other-player inventory, request replay, item identity and late-join reconstruction.
- Real `BP_SharedChest` construction/relocation and native world-save reconstruction passed: capture, memory serialization, recreation of a missing actor, saved identity, position, capacity, assignment order and opaque item state, plus idempotent reapplication.
- Existing pickup/collect/transfer paths published final graphs before updating their revision. Regression tests exposed this baseline issue; they now publish all participating revisions before notifying observers, retaining one post-commit notification per inventory.
- Blueprint composition and the new map were authored, compiled and saved through Unreal MCP. All six modified/new inventory widgets compiled with warnings treated as errors. Screenshots confirmed the GASP presentation, separate bases, range markers, the assignment picker and a chest with wood-category plus iron-item slots.
- Actual PIE input confirmed construction, upgrade to 6x6, adding a wood category and exact iron slot, replacing/removing rules, rotation/relocation and successful deposit of two carried planks. The inside workbench displayed all 280 remaining oak despite its 1 m station radius. Crafting consumed three oak and produced two planks; enabling automatic deposit drained the tray. Disabling it restored tray output. The final editor restart preserved the built chest's ID, position, yaw, 6x6 grid and wood assignment order. Build costs were visibly correct. The final editor log contains no Blueprint runtime errors.
- The physical-storage view model also observes the existing inventory capacity message. The passing `ViewModelCapacityReplicationOrder` test reproduces inventory capacity arriving after chest metadata and checks unrelated inventories, rebind and unbind.
- Windows cook of `Lvl_RpgInventoryStorage` succeeded: **3,186 packages, zero errors, three warnings**, 170 seconds. The map, shared chest and all four new widgets are present under `Saved/Cooked/Windows/`. Warnings concern the existing GameplayCue search-path configuration, the MCP plugin notice and the existing Respawn widget tick setting. This was a map cook, not a packaged executable playthrough.
- Legacy cleanup removed the terminal and wooden virtual-storage actor from both `Lvl_RpgBaseline` and `Lvl_ThirdPerson`, cleared their two workstation provider references, and removed only the old BaseTerminal screen route. Reload checks preserved the other 40 actors in each map. Existing Rift, locker and save schemas remain for their separate dependencies. The old construction RPC rejects physical chests and retired virtual-storage actors.
- Six old `SurvivalRpg_World*` and `SurvivalRpg_BaselineTest*` prototype saves were backed up to `Saved/PhysicalStorage20260930/PrototypeSaveBackup` before reset. The unrelated slot-manager save was retained.
- After the final restore check, the three isolated smoke-test save slots were backed up with hash verification to `Saved/PhysicalStorage20260930/FinalPlaytestSaveBackup` and reset. The editor remains on the test map with PIE stopped and no unsaved packages, ready for a fresh run.

Local evidence: `Saved/PhysicalStorage20260930/` (ignored, not portable to another checkout): `build-08.log`, `AutomationFinal08/index.json`, `cook.log`, and `ui/smoke-summary-final.json` with screenshots. Automation tests themselves are in Source and can be rerun with the `SurvivalRpg.Inventory`, `SurvivalRpg.Storage`, `SurvivalRpg.Crafting`, `SurvivalRpg.Save`, `SurvivalRpg.UI.ScreenRegistry` and `SurvivalRpg.Network.PhysicalStoragePIE` filters.

## Movement and multiplayer follow-up (2026-09-30)

The movement warning conflated an unchanged chest with leaving its 350 cm direct interaction radius. Relocation now starts beside the chest through an authority request, then permits movement inside the same base. Confirmation uses the admitted controller/pawn session and settings revision. It consumes the session before transform callbacks, and the container rejects nested relocation during transform publication. Normal crafting or withdrawal does not invalidate the session solely because item counts changed.

The concurrency audit also found persistence gaps: missing stations/chests were omitted by live-actor snapshot rebuilding, and refunds could reach provisional inventories before a later saved-graph replacement. Absent actors now retain their last committed snapshots; returning actors restore without replaying startup grants. Live empty state replaces old saved contents. Refund lookup excludes player profiles and chest graphs still awaiting restoration, preserving the existing tray fallback or retained claim.

Additional validation covers:

- Two remote clients crafting against exactly one affordable recipe, racing to withdraw the same item identity, and canceling a blocked job while the other player moves its source chest beyond direct interaction reach.
- Two remote construction requests with one shared payment, request replay, resulting item totals and late join.
- Competing stations, reentrant source/settings changes during output preparation, partially completed jobs, full output/refund inventories, lost source actors and relative-time restore.
- Serialized absent-station claims and absent-chest contents, including empty-state replacement and interrupted reentry.
- Refunds during player/chest graph restore, with inventory totals checked after replacement.

Follow-up results:

- Editor Development build **10 succeeded**. Build 09 exposed a pointer-deduction error in the new network test; the explicit `APawn*` correction is included in build 10. Runtime and editor modules are built from the current source.
- `AutomationFollowup10`: **224/224 passed**, zero failures or skipped tests, including eleven added regressions. Seven tests passed with warnings: the existing deliberate request-collision, GASP pose/UI-fixture warnings and the three CQTests' temporary level/WorldSettings NetGUID warnings. Both new remote contention tests and all added conservation/restore tests passed.
- Actual PIE input: opened the existing oak chest, started relocation, walked until the pawn was **503.50 cm** from its original position, and aimed at supported empty ground. The screen displayed **Platzierung möglich** with **Bestätigen** enabled. Cancel returned to gameplay and left the chest transform unchanged. The remote-client test separately confirms a move beyond 350 cm while another client cancels a paid craft.
- The existing playtest saves were retained. The rebuilt editor is open on the test map with PIE stopped and no unsaved packages. The smoke run logged no Blueprint runtime errors or persistence restore failures.

Evidence: `Saved/PhysicalStorage20260930/build-10.log`, `AutomationFollowup10/index.json`, `ui/followup-walk-valid.json`, `ui/followup-placement-fixed.png` and `editor-20260930-162445.log` (local, ignored). The network cases use a PIE dedicated server and real owner RPCs from two remote clients, followed by a late joiner. Disconnect/source loss and delayed graph replacement are also covered through native lifecycle tests; a full external-process reconnect/crash/packet-loss matrix was not run. These checks cover the named scenarios, not every possible multiplayer schedule or crash timing.

## Split/merge persistence follow-up (2026-09-30)

Splitting and then merging a chest stack exposed a separate notification boundary: split listeners could observe the source debit before the new stack existed, and full-merge listeners could observe a zero-count source row before its removal. The strict graph exporter correctly rejected the latter and latched the session's disk-write protection.

Single-inventory commands now retain change-message snapshots until the entire graph, replication subobjects, revision and request result have committed. Fragment hooks and synchronous observers cannot start another mutation during this scope; cached retries still return the committed result. Failed split insertion discards its intermediate debit/rollback messages. Container capture replaces the cached save only after export succeeds, preserving the previous valid snapshot on a genuine export failure. The export validator and write protection remain in place.

Validation:

- Editor Development build **11 succeeded**, including runtime and editor modules (74.76 seconds).
- `AutomationSplitMerge11`: **228/228 passed**, zero failures or skipped tests; seven passed with the same deliberate-collision, GASP/UI fixture and temporary CQTest level warnings described above. All three dedicated-server contention/late-join cases passed again.
- Three new native tests inspect every row/post-commit notification for split, full merge and partial merge: complete validated graph, conserved quantity, current revision, stable identities, cached replay and rejected nested mutations. A fourth test uses the real GameMode save listener, serializes its committed snapshot and reconstructs the chest without resurrecting the merged source identity.
- Real PIE input on `StorageLab_A_ExactEarly`: selected its nine-unit oak stack, used **Y / Split** to create five plus four, then dragged the four back onto the original. The stack returned to nine, the primary/backup save files were written, and a new PIE session loaded the same five original stacks (9, 5, 1, 2, 2). No graph-export, disk-write-protection or Blueprint runtime errors occurred in this run.
- Existing saves were backed up to `Saved/PhysicalStorage20260930/SplitMergeSaveBackup` before the UI check. The rebuilt editor remains on the test map with PIE stopped and no unsaved packages. The earlier failed session had already ended before this fix; this work does not recover changes that were never saved from that session.

Evidence: `Saved/PhysicalStorage20260930/build-11.log`, `AutomationSplitMerge11/index.json`, `editor-20260930-170744.log`, and `ui/splitmerge-{before,split,merged,reloaded}.png` (local, ignored).

## Test content

- Map: `/Game/SurvivalRpg/Maps/Test/Lvl_RpgInventoryStorage`.
- Experience: `/Game/SurvivalRpg/System/Experiences/RpgInventoryStorageExperience`, reusing the accepted GASP CMC pawn composition.
- GameMode and controller: `/Game/SurvivalRpg/Maps/Test/InventoryStorage/`.
- Save slots: `InventoryStorageTest`, `InventoryStorageTest_Backup`, `InventoryStorageTest_Recovery`.
- Physical actors and definitions: `/Game/SurvivalRpg/Storage/Physical/`. The buildable chest has no starting items. A separate test Blueprint seeds placed fixtures synchronously on authority before world restoration; restored graphs replace those initial contents.
- Item examples: oak, birch, pine, planks, iron, copper, granite and flax. Their native traits contain hierarchical `Item.Material.*` tags; the definitions retain native spatial, UI and physical-storage fragments.
- Base A is centered at `(0, 0)` with a 20 m radius; Base B at `(6000, 0)` with an 18 m radius. The outside workstation at `(2500, 0)` has a 9 m source radius. It reaches the A edge chest at `(1800, 0)` but not the distant A chest at `(-1500, 500)`.

## Short playtest

Open `Lvl_RpgInventoryStorage` and start PIE. Use **I** for the inventory and **F** for the highlighted interaction.

- In the inventory, choose **Kiste bauen**, review the definition's costs, then start placement. Aim the camera at the ground, rotate if needed and confirm. Construction takes eligible materials from Base A when the player's own inventory is empty.
- Open a chest to edit its assignment slots, deposit carried materials, upgrade its grid or move it within the same base. A slot can select an exact material or a category; add separate slots to combine wood and iron.
- The two oak chests start with 25 oak each. The earlier assignment wins that tie. Exact oak assignments take priority over category chests and the unassigned chest, even when those contain more oak.
- The inside workbench uses the full base despite its deliberately small 1 m station radius. The outside workbench uses its 9 m radius and cannot reach the far chest through the nearby edge chest.
- Craft planks with **C** and check the output tray. Enable automatic deposit to route its contents and subsequent outputs to the plank chest. **Ctrl + click** transfers a tray stack to the player; a chest's **Einlagern** action routes carried materials. The eight material examples and both base areas are labeled in the map.

## Transaction and save boundaries

`ApplyInventoryBatch` stages every participant before publishing any inventory. Costs, outputs, capacity changes and caller-owned queue/metadata updates share the commit. Final context checks include region membership, settings and revisions. Construction runs Blueprint setup before payment, gates the pending actor from access and saves, and includes its empty inventory in the batch before publishing it.

Crafting blocks nested queue mutations during staging and publication. Relative remaining times and retained refund claims are saved with chest/tray graphs. World saves cannot overwrite startup state before experience loading and candidate restoration have completed.
