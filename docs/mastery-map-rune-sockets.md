# MST-05: Rune items, ranks and tool sockets

> **To be replaced (2026-10-31):** HRW-05 of the [harvesting rework roadmap](harvesting-rework-roadmap.md) redesigns the harvesting runes and rune-gated talents, and the mastery map roadmap marks this playtest superseded. Until HRW-05 merges, this report still describes the running code.

Task report for MST-05 in the [mastery map roadmap](mastery-map-roadmap.md), [PR #314](https://github.com/Athurito/SurvivalRpg/pull/314). The design is in the [implementation plan](mastery-map-implementation-plan.md), section "Physical runes and the workstation loop". It builds on the inventory graph of the [inventory refactor](inventory-refactor-plan.md) and on the equipment grants that [MST-04](mastery-map-choice-swap.md) and the skill trees use.

## Rune contract re-inspection

CMB-07 delivered `URpgWeaponRuneDefinition` and `URpgInventoryFragment_WeaponRunes`:
- Runes are engraved on weapon item definitions, and the equipped weapon instance runs the combat Echo from them.
- There are no rune item instances, ranks or sockets.

MST-05 leaves that contract unchanged. Combat weapons keep their engraved runes and the Echo trigger. Harvesting runes are a separate kind of item, so physical runes need their own schema.

## C++ boundary decision

- **Classification:**
  - Extensions of the inventory schema: a rune item fragment, a socket rule on item-owned grids, and the rank as instance state.
  - An extension of the equipment grant mechanism.
  - The rune, its ranks and grants, and the tools' sockets are content.
- **Runtime truth:**
  - The PlayerState's `URpgInventoryManagerComponent` owns each rune's identity and location: an item-owned socket grid of its tool, never a second socket array.
  - `URpgInventoryItemInstance` owns the rank: replicated, and saved in the Rune fragment's payload.
  - `URpgEquipmentManagerComponent` applies the socketed runes' grants while their tool is equipped; server only.
- **Existing seams:**
  - Item-owned grids (`URpgInventoryFragment_ItemContainer`).
  - The planner's two checks: `ValidatePlacementGraphRules` for single moves and swap-displaced items, `ValidateInventoryGraph` for transfers, pickups, restores and batches.
  - Fragment runtime-state payloads.
  - The equipment manager's keyed grant reconciliation, until now used for learned skill nodes.
- **New native class:** `URpgInventoryFragment_Rune`, one of the two the plan allows. Sockets need no class: an `ItemContainer` grid with the new flag `bSocketGrid`.
- **Assets (Unreal MCP):**
  - The 3x1 socket grid on `ID_Tool_Axe` and `ID_Tool_Pickaxe`.
  - `ID_Rune_Brand` and a temporary starter component (below).
  - All in `GF_Harvesting_Magic`, because harvesting runes belong to harvesting and `GF_Runes_Core` is active in no standard experience.

## Behavior

### Rune items

- **Rune fragment:** `URpgInventoryFragment_Rune` turns an item into a rune: `RuneTag`, such as `Rune.Brand`, and one to three `Ranks`, each with an optional `AbilitySet`, `GrantedTags` and a description.
- **Validation:** a rune must use the item category Rune, must not stack and provides no containers.
- **Rank:** `URpgInventoryItemInstance::GetRuneRank` and the authority-only `SetRuneRank`.
  - A new rune starts at rank I, and a rank outside 1 to its highest rank is rejected.
  - The rank replicates through `OnRep_RuneRank` and `OnRuneRankChanged`.
  - It is saved as the fragment payload `Inventory.Rune.Rank` (version 1); a load with an invalid rank is rejected as a whole.

### Socket grids

`FRpgInventoryItemContainerDefinition::bSocketGrid` makes an item-owned grid the sockets of its owning item.

- **One per kind:** the grid holds at most one item of each item definition, so a tool carries one rune of each kind.
  - Every insertion path enforces this: single moves, swaps (including the displaced item), cross-inventory transfers, generated grants and restores.
  - A rejection reports `DuplicateSocketedItem`.
  - Dropping a rune on the socketed rune of its kind swaps them, because the rune it replaces leaves.
- **Not storage:** an item that only provides socket grids does not count as a nested container. A socketed axe therefore still fits into backpacks and chests that refuse nested containers, and its runes move with it as its subtree.
- **No automatic placement:** pickups and first-fit placement never choose a socket grid, because they search only the root layout groups. A socket grid may neither allow nested containers nor quick access.
- **Reading sockets:** `URpgInventoryManagerComponent::GetSocketedItems` lists the items in an item's socket grids in socket order, on the server and on the owning client.

### Grants while equipped

- **Rebuild:** while a tool is equipped, the server's equipment manager grants each socketed rune's `RuneTag` as a loose tag, plus the grants of its current rank, with the tool's equipment instance as source.
- **Keyed grants:** grants are keyed by the rune item. Moving a rune between sockets of the tool keeps its grant, and a rank change replaces the old rank's grants without granting anything twice.
- **Triggers:** the equipment manager follows the post-commit event of exactly the inventories that hold its equipped items. Every inventory commit rebuilds the grants once, and the diff leaves unchanged grants alone.
- **Unequip:** unequipping the tool removes its rune grants.

### Content

- **Tools:** `ID_Tool_Axe` and `ID_Tool_Pickaxe` each have the grid `RuneSockets` (3x1, category Rune, required tag `Item.Rune.Harvesting`).
- **Brand:** `ID_Rune_Brand` is a 1x1 rune with `Rune.Brand` and ranks I to III without grants. Its talents arrive with MST-06, and its icon is the placeholder `T_UI_Item_RiftFragment`.
- **Temporary starter:** `BP_HarvestingRuneStarterInventory`, which the `GF_Harvesting_Magic` GameFeatureData adds to every player controller, gives each player one Brand rune if they have none. MST-07 replaces it with rune acquisition.
- **New tags:** `Item.Rune.Harvesting` and `Rune.Brand` in `Config/DefaultGameplayTags.ini`.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **Failed attempt:** the first build failed because the socket check, written as a free helper, read the private members of `FRpgInventoryEntry`. It is now `URpgInventoryManagerComponent::HasSocketedDuplicate`.
- **New tests:**
  - `SurvivalRpg.Runes.Sockets.PlacementRules`:
    - A rune takes a free socket; a second rune of the same kind is rejected, and an item that is no rune too.
    - Dropping a rune on the socketed rune of its kind swaps them, and a socketed rune moves between sockets.
    - A socketed tool fits into a bag that refuses nested containers, and the socket rule holds there too.
    - A socket grid that allows nested containers is invalid.
  - `SurvivalRpg.Runes.Sockets.TransferKeepsRunesOnce`:
    - Transferring a rune of a socketed kind into the tool from a chest is rejected, and the chest keeps it.
    - A socketed tool moves into a chest as a whole, under the same ids, and leaves no copy behind.
  - `SurvivalRpg.Runes.RankPersistence`:
    - Rank limits, and no rank for items that are no runes.
    - A save round trip keeps ids, sockets and rank III.
    - A save with two runes of one kind in a tool, or with rank IV, is rejected and changes nothing.
  - `SurvivalRpg.Runes.Sockets.EquippedToolGrants`:
    - The equipped tool grants a socketed rune's tag and rank tag once, also after a move between sockets and after an unrelated move.
    - Rank II replaces rank I's tag and grants rank II's ability exactly once.
    - Removing the rune or unequipping the tool removes the grants, and an unequipped tool grants nothing.
  - `SurvivalRpg.Harvesting.Content.RuneSocketContract`: axe and pickaxe each have one socket grid with three sockets that take Brand and store nothing else; Brand reaches rank III.
  - `SurvivalRpg.Network.RuneSocketPIE.OwnerSocketsOneRunePerKindAndTheRankReplicates`, on a dedicated server with two clients:
    - The owner sockets rune A through the inventory UI's request path.
    - The server refuses a second rune A and sockets rune B.
    - Rank III set on the server reaches the owner with both runes in the tool.
    - The other player never receives the owner's tool or runes.
- `ue.py test SurvivalRpg.Runes --null-rhi`: 4 of 4 passed; `SurvivalRpg.Harvesting.Content`: 9 of 9.
- `ue.py test SurvivalRpg.Harvesting SurvivalRpg.Inventory SurvivalRpg.Runes SurvivalRpg.Progression --null-rhi` with the new content: 271 of 271 passed.
- `ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting --null-rhi`: **447 of 447 passed**. The 16 with warnings are the known AdvancedFriends and `LogNetPackageMap` messages of the PIE locomotion tests, the intended RequestId collisions of two equipment intent tests, and asset logs of two UI tests.
- `ue.py test SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.InventoryReorderPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.KillXPCuePIE`: **6 of 6 passed**, with only the usual `LogNetPackageMap` warnings of the temporary PIE worlds.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **Pre-existing failures, not caused by MST-05:** `SurvivalRpg.BaseStorage.ViewModel.FieldNotify.ResourceEntryOnlyChangedFields` and `SurvivalRpg.BaseStorage.ViewModel.Invalidation.FirstResourceRowCoalescesProducerMessages` fail the same way on unchanged `master` (`08285c1e`). They are about base-storage resource messages.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players: the host and the client each received their own Brand rune from the starter, shown with name and icon in their pockets. Socketing could not be checked by hand, see Limits.

## Playtest

1. **Brand:** start `Lvl_HarvestPickaxe` and press Tab. Your pockets hold one Brand rune, and its tooltip describes it.
2. **Regression:** pick up the axe and the pickaxe. Both still go into the pockets, a backpack and chests, and still harvest as before.
3. **Multiplayer:** in a two-player listen server, each player has their own Brand rune.

## Limits

- **Sockets not reachable in the UI:** the inventory UI shows item-owned grids only for gear providers (backpack, belt, pouch, resource bag). No screen opens a tool's grid yet, so a player cannot socket a rune by hand.
  - The automated tests cover socketing through the same server requests the UI sends.
  - MST-06 needs this first: show the socket grids of the tools in the carry slots in the inventory, and keep them out of first-fit placement and quick transfers.
- **Rank without a commit:** a rank change that happens outside an inventory commit, such as the test's direct `SetRuneRank`, regrants on the next commit or equipment change. MST-08's upgrade must commit through the inventory, or refresh the equipment.
- **No effects yet:** Brand grants only its tag; its talents arrive with MST-06.
- **Placeholders:** the icon `T_UI_Item_RiftFragment` and the starter rune for every player stay until MST-07 brings acquisition.
- **Not seen by hand:** picking up the axe in PIE needs mouse aim, which the scripted input cannot give, so the agent did not socket a rune in a running game.

## Next

MST-06: make tool sockets reachable in the inventory, then the source-tool activation evaluator, the rune context on the skill screen and Brand's first talent path.
