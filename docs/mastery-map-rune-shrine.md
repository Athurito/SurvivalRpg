# MST-08a: Rune upgrades and salvage at the Runenschrein

Task report for MST-08a in the [mastery map roadmap](mastery-map-roadmap.md). It covers the upgrade and salvage transactions of the [implementation plan](mastery-map-implementation-plan.md), section "Physical runes and the workstation loop":
- Upgrade mutates the selected instance in place after complete cost, ownership and station validation.
- Salvage consumes one eligible unsocketed instance and awards its configured essence.
- Failed validation or capacity checks consume nothing.

The user accepted rune essence and the proposed costs on 2026-10-28.

## Scope

- **Split:** MST-08 is split, so each PR stays bounded:
  - **MST-08a (this report):** the transactions, rune essence, Brand's costs and outputs, the Runenschrein station, and a plain Runes section on its screen.
  - **MST-08b:** the integrated Runenschrein presentation, with rune icons, a rank preview and a screen of its own instead of the crafting layout.

## C++ boundary decision

- **Classification:**
  - An extension of the existing crafting station mechanism and inventory commit, plus two schema fields on the rune ranks.
  - Essence, costs, outputs, the station class and the screen section are content.
- **Runtime truth:**
  - The rank stays on the concrete rune instance in the player's inventory graph (`URpgInventoryItemInstance::RuneRank`, replicated and saved).
  - Materials and essence stay in the connected chests and the inventory graph.
- **Existing seams:**
  - The crafting station's connected-chest consumption plan.
  - `ApplyStationBatch` and `ApplyInventoryBatch`: an atomic multi-inventory commit with side effects and revalidation.
  - The station's access check.
  - `URpgInventoryUiActionComponent`: server RPCs for the requester.
  - The equipment manager's post-commit grant rebuild.
  - The crafting view model and station widget.
- **Native:**
  - **`FRpgRuneRank`:** `UpgradeCosts` (the cost to reach the rank, paid from connected chests) and `SalvageOutputs` (what salvaging a rune of that rank returns). Data validation requires costs for ranks above I.
  - **`URpgCraftingStationComponent`:** the designer flag `bOffersRuneWork`, plus `EvaluateRuneUpgrade`, `UpgradeRune`, `EvaluateRuneSalvage` and `SalvageRune` with `ERpgRuneWorkResult`.
    - **Upgrade:** the cost debit and the rank change are one commit. The rank rises in the batch's side effect, after revalidation that the rune is still the requester's and at the same rank.
    - **Salvage:** the removal and the outputs are one batch. Outputs go into the first connected chest that takes them automatically and has room, else into the requester's own inventory.
    - **Refusals:** socketed runes are refused, and recipe knowledge is untouched.
  - **`URpgInventoryManagerComponent::MarkItemRuntimeStateChanged`:** publishes a trusted runtime-state change of one item (revision, change message, post-commit). The equipment manager then swaps rank grants once, and saves and UI follow.
  - **`URpgInventoryUiActionComponent`:** `RequestUpgradeRune` and `RequestSalvageRune`, server RPCs for the requester.
  - **`URpgCraftingStationViewModel`:** `bOffersRuneWork`, `RuneWorkActions` (upgrade and salvage per carried rune, with two-line labels and enabled state), and `OnRuneWorkChanged`.
  - **`URpgCraftingStationWidget::RequestRuneWorkAction`:** sends the chosen action.
  - **New native classes:** none.
- **Assets (Unreal MCP):**
  - **`ID_RuneEssence`** (`GF_Harvesting_Magic/Items/Materials`): a stackable material, duplicated from charcoal, with the rift essence icon.
  - **`ID_Rune_Brand`:**
    - Rank II costs 3 Rune Essence, 4 ore and 2 charcoal; rank III costs 6, 8 and 4.
    - Salvaging returns 2, 5 or 9 Rune Essence by rank.
  - **`BP_CraftingStation_RuneShrine`:** a crafting station class with `Crafting.Station.RuneShrine`, the name "Runenschrein", rune work on and no recipes. It is placed in `Lvl_HarvestPickaxe` beside the Kiln, connected to the shared chest.
  - **`CUI_CraftingStationSpatial`:** a Runes section in the recipe column (`RuneWorkBox`, `RuneWorkList`, a hint) with one button per action. It has the functions `F_RefreshRuneWork` and `F_HandleRuneWorkClicked`, and follows `OnRuneWorkChanged`.
  - **Tag:** `Crafting.Station.RuneShrine`.

## Behavior

### Problem

A rune stayed at rank I forever, and an unwanted rune could not be turned into anything.

### Change

- **Runes section:** at the Runenschrein (F to open), a Runes section lists two buttons for every rune the player carries, also socketed ones:
  - **Upgrade:** "Upgrade Brand I to II" over its cost.
  - **Salvage:** "Salvage Brand I" over its output.
  - **Disabled buttons:** a button is disabled while the chests lack the materials, at the highest rank ("Brand III: highest rank"), or for a socketed rune ("Brand I is socketed").
- **Upgrade:** it raises exactly the chosen rune in place, also while it sits in the tool in hand. Its rank grants and tunings change at once, without double grants.
- **Salvage:** it removes the rune and puts its essence into the connected chest, or into your pockets when no chest takes it. Recipe knowledge stays.
- **No side effects:** too few materials or no room change nothing.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Crafting.RuneWork.UpgradeAndSalvageTheChosenRune`:**
    - A station without rune work refuses.
    - Salvage without room (a full chest and a full inventory, with outputs larger than the freed cell) is refused and keeps the rune.
    - Without materials an upgrade is refused and keeps the rank, and an unknown rune is refused.
    - The chosen of two runes reaches rank II, and the other keeps rank I. The cost is paid from the chest.
    - Insufficient materials for rank III change and take nothing; with materials it reaches rank III, then reports the highest rank.
    - Salvaging rank III removes the rune, delivers its outputs to the chest, and keeps a known recipe.
    - A socketed rune cannot be salvaged but can be upgraded in place.
  - **`SurvivalRpg.Crafting.RuneWork.SalvageFallsBackToThePlayer`:** with only a full chest connected, the outputs land in the player's inventory.
  - **`SurvivalRpg.Network.PhysicalStoragePIE.RemoteRuneWorkUpgradesAndSalvagesTheOwnersRune`:**
    - A remote client upgrades its rune through `RequestUpgradeRune`: the server raises exactly that rune to rank II and takes the material, and the client sees rank II.
    - The client then salvages it through `RequestSalvageRune`: the server removes it and delivers the outputs, and the client no longer holds it.
  - **`SurvivalRpg.Harvesting.Content.RuneWorkContract`:** Brand's ranks II and III cost rune essence, each rank salvages into more essence than the one below, and the Runenschrein offers rune work.
- **Changed tests:**
  - `SurvivalRpg.Runes.Sockets.EquippedToolGrants` now publishes a rank change through `MarkItemRuntimeStateChanged` and checks that rank I's grants leave and rank II's arrive once, at once.
  - `SurvivalRpg.Inventory.UiActions.Facade.RpcOwnershipContract` lists the two new RPCs.
- **Failed attempts and fixes:**
  - **Salvage target:** the PIE check showed that the shared chest in the test map does not take essence automatically, so salvage had no target. Salvage now falls back to the player's inventory.
  - **No-room test:** the first version expected a full player inventory to block salvage. Removing the rune frees its cell, so the test now uses outputs larger than that cell.
  - **RPC contract:** the facade contract failed on the two new RPCs until they were listed.
  - **Mover timeout:** in one full run, `SurvivalRpg.Combat.BlockLocomotion.BlockLocomotionPIE.MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease` timed out. It passed when run again and in the final full run, and it does not touch this change.
- `ue.py test SurvivalRpg.Portals SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **484 of 484 passed**; 17 with the known warnings.
- `ue.py test SurvivalRpg.Network.PhysicalStoragePIE`: **4 of 4 passed**, only `LogNetPackageMap` warnings.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players.
  - **Temporary setting:** for the check only, Brand's rank II upgrade was free in memory, then restored and not saved.
  - **Opening:** the host opened the Runenschrein with F and saw Runes with "Upgrade Brand I to II" and "Salvage Brand I for 2 Rune Essence".
  - **Upgrade:** after clicking Upgrade, the buttons read "Upgrade Brand II to III" over "6 Rune Essence, 8 Ore, 4 Charcoal", disabled without materials, and "Salvage Brand II for 5 Rune Essence".
  - **Client path:**
    - Calling the client's request from editor Python had no effect, because a Server RPC from that path never reaches the server; this is recorded in the skill reference.
    - The client path is covered by the network test above.

## Playtest

1. **Open:** in `Lvl_HarvestPickaxe`, walk to the Runenschrein (the block beside the Kiln, toward the start) and press F. With the starter's Brand in your pockets, the Runes section shows "Upgrade Brand I to II" (disabled) and "Salvage Brand I".
2. **Salvage:** click "Salvage Brand I". The rune leaves your pockets, and 2 Rune Essence lie in the shared chest, or in your pockets if no chest takes it.
3. **Upgrade:**
   - Gather a second Brand, from the Harvesting level 3 reward, a drop, or the Kiln recipe.
   - Gather 3 Rune Essence by salvaging further Brands.
   - Put 4 ore and 2 charcoal into the shared chest.
   - Click "Upgrade Brand I to II". The same rune now reads Brand II, also when it sits in your pickaxe, and its burn grows stronger.
4. **Multiplayer:** in a two-player listen server, the client upgrades and salvages only its own runes. The host sees the chest change.

## Limits

- **Integrated presentation:** this follows in MST-08b. The Runes section sits in the crafting layout, next to an empty recipe list and the generic order strip.
- **Client screen not opened in PIE:** the client view cannot be aimed with the PIE tools. The client path was proven by the network test instead.
- **Upgrade costs not paid in PIE:** the PIE check used a free rank II. Paid upgrades are covered by the native and network tests.
- **Costs are first tuning:** essence amounts and costs are first tuning, and only Brand has them.

## Next

MST-08b: the integrated Runenschrein presentation.
