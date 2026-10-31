# MST-08b: The Runenschrein screen

> **To be replaced (2026-10-31):** HRW-05 of the [harvesting rework roadmap](harvesting-rework-roadmap.md) redesigns the harvesting runes and rune-gated talents, and the mastery map roadmap marks this playtest superseded. Until HRW-05 merges, this report still describes the running code.

Task report for MST-08b in the [mastery map roadmap](mastery-map-roadmap.md), [PR #324](https://github.com/Athurito/SurvivalRpg/pull/324). It completes MST-08 of the [implementation plan](mastery-map-implementation-plan.md) with the "integrated Runenschrein presentation". [MST-08a](mastery-map-rune-shrine.md) added the transactions behind it: upgrade and salvage, rune essence, and a plain Runes section on the crafting screen.

## C++ boundary decision

- **Classification:** presentation, plus a small extension of two existing mechanisms:
  - The station's screen routing.
  - The crafting view model's rune read model.
- **Runtime truth:** unchanged.
  - The rank stays on the rune instance in the player's inventory graph.
  - Materials stay in the connected chests.
  - The screen only reads them and sends the MST-08a requests.
- **Existing seams:**
  - The UI screen registry: a `UI.Screen` tag maps to a widget class.
  - `ARpgPlayerController::ClientOpenCraftingInteraction`.
  - `URpgCraftingStationWidget`, the native presenter that fills authored lists and buttons and forwards intents.
  - `URpgCraftingStationViewModel`.
  - `URpgMvvmListEntryButton` for rows.
  - `URpgCraftingIngredientViewModel` and its entry for material rows.
- **Native:**
  - **`URpgCraftingStationComponent::ScreenTag`:** designer data on the station class. `GetScreenTag()` returns it, or `UI.Screen.Crafting` when it is empty. The controller opens that screen, and the presenter accepts a payload only for its station's screen tag.
  - **`URpgCraftingStationWidget`:**
    - **Optional recipe controls:** the recipe controls (`RecipeList`, `IngredientList`, `CraftButton`, `PauseButton` and the quantity buttons) are now optional bindings. A screen of a station without recipes can omit them.
    - **Compile check:** the order input rows are required only when the screen authors their buttons (`ValidateCompiledWidgetTree`).
    - **Rune bindings:** new optional bindings `RuneList`, `RuneUpgradeCostList`, `RuneIcon`, the rune texts, `RuneUpgradeButton`, `RuneSalvageButton`, `RuneDetailsPanel` and `RuneEmptyHint`. The presenter fills them, as it fills the recipe details.
    - **Commands:** `RequestUpgradeSelectedRune` and `RequestSalvageSelectedRune` replace MST-08a's index-based `RequestRuneWorkAction`.
    - **Focus:** the rune list takes CommonUI focus when it has rows.
  - **`URpgRuneWorkRuneViewModel`:** one row per rune the requester carries, with:
    - id, name and "Brand II" title, and the icon;
    - rank and highest rank;
    - "Carried" or "Socketed in Pickaxe";
    - whether an upgrade or a salvage would run now.
  - **`URpgCraftingStationViewModel`:** it replaces MST-08a's flat action list with:
    - the rune rows and a selection that follows its rune;
    - the selected rune's title, icon, location and current rank description;
    - the rank preview: "Next: Rank III" and the next rank's description;
    - upgrade cost rows with the counts in the chests;
    - the upgrade label and the reason it cannot run;
    - the salvage line ("Returns 5 Rune Essence", or why not).
  - **New native classes:** one, the rune row view model. A row needs its own FieldNotify object, as the other crafting rows do.
- **Assets (Unreal MCP):**
  - **`CUI_RuneShrine`:**
    - **Origin:** a Widget Blueprint of the native presenter, built from a copy of `CUI_CraftingStationSpatial`. It keeps the frame, header and chest line, and drops the categories, recipes, filters, target chest and order strip.
    - **Rune list column:** "Your runes" with an empty hint.
    - **Details:** large icon, title, location, current rank and the next-rank panel.
    - **Work column:** "Upgrade", with the material rows (Each / In chests), the reason and the upgrade button. "Salvage", with the return line and a salvage button in the danger style.
  - **`CUI_RuneShrineRuneEntry`:**
    - **Look:** a `URpgMvvmListEntryButton` row in the chest-option look: icon, title, location and a "Ready" badge.
    - **Bindings:** MVVM bindings to the row view model; the icon goes through `SetRuneIcon`.
  - **`DA_RpgUIScreenRegistry`:** `UI.Screen.RuneShrine` routes to `CUI_RuneShrine` on the GameMenu layer.
  - **`BP_CraftingStation_RuneShrine`:** `ScreenTag` is `UI.Screen.RuneShrine`.
  - **`CUI_CraftingStationSpatial`:** MST-08a's Runes section, its two functions and its event binding are removed. The crafting screen shows no runes any more.
  - **Tag:** `UI.Screen.RuneShrine`.

## Behavior

### Problem

The Runenschrein reused the crafting screen. It showed a Runes section of text buttons next to an empty recipe list, filters and an order strip. The screen had no rune icons and no preview of the next rank.

### Change

- **Rune list:** pressing F at the Runenschrein opens a screen of its own. On the left, "Your runes" lists every rune you carry, sockets included, with its icon, "Brand II", where it is ("Carried" or "Socketed in Pickaxe"), and "Ready" when an upgrade would run now.
- **Details:** the selected rune shows its icon, name and rank, what its current rank does, and in a panel what the next rank does.
- **Upgrade:**
  - The materials of the next rank are listed with what each step needs and what the connected chests hold.
  - A missing material shows a reason, and the upgrade button is disabled.
  - The button reads "Upgrade to Rank III", or "Highest rank" at the top.
- **Salvage:** "Returns 5 Rune Essence" over a red Salvage button. A socketed rune says "Take it out of Pickaxe to salvage it."
- **Selection:** the selection stays on its rune through an upgrade. When the rune is salvaged, the next rune is selected; without runes, the screen shows a hint where they come from.
- **Other stations:** the Kiln and other crafting stations open the crafting screen as before, now without the Runes section.

## Validation

Run on 2026-10-29 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Crafting.RuneWork.ShrineReadModel`:**
    - The read model lists both carried runes and selects the first.
    - It names title, location, current and next rank, and lists the next rank's cost with the chest count.
    - It explains missing materials and names the salvage outputs.
    - After an upgrade the selection stays on the upgraded rune and the preview moves to rank III.
    - A socketed rune names its tool and cannot be salvaged.
    - Salvaging the selected rune moves the selection to the remaining one.
    - A station without rune work lists nothing.
  - **`SurvivalRpg.Crafting.Station.ScreenTagDefaultsToCrafting`:** an empty screen tag opens the crafting screen, and an authored one opens that screen.
  - **`SurvivalRpg.Inventory.UI.Crafting.RuneShrineScreen`:**
    - The registry routes `UI.Screen.RuneShrine` to a crafting station screen.
    - A payload for the crafting screen is refused at a rune station.
    - The rune screen binds once and lists and selects the carried rune.
    - The rune list takes focus, rows are list entries, and cost rows use the ingredient entry.
    - The screen authors both actions.
- **Changed tests:**
  - **`SurvivalRpg.Harvesting.Content.RuneWorkContract`:** also checks that the Runenschrein opens its rune screen.
  - **`SurvivalRpg.UI.ScreenRegistry.InventoryScreenFamilyClosure`:** now expects five inventory interaction screens and the `UI.Screen.RuneShrine` route.
  - **`SurvivalRpg.UI.Inventory.EditorContracts.RequiredBindWidgets`:** no longer requires the crafting presenter's recipe controls. `SurvivalRpg.Inventory.UI.Crafting.ScreenContract` still checks that the crafting screen authors them.
- **Failed attempts and fixes:**
  - **Old Runes section:** the first test run failed in `SurvivalRpg.Inventory.UI.Crafting.PayloadLifecycle`, because `CUI_CraftingStationSpatial` still called the removed MST-08a functions. It passed after the section was removed.
  - **Screen family contract:** `InventoryScreenFamilyClosure` failed on the new screen until the contract listed it.
  - **Required bindings contract:** the first full run failed only in `RequiredBindWidgets`, which still required the recipe controls; the contract now leaves them optional.
  - **Stale level instance:** in the first PIE check the Runenschrein opened the crafting screen. The shrine already placed in the open level kept its old empty screen tag after the Blueprint compile. The saved map stores no override, so a fresh load takes the new default. The check set the value on the placed instance for that session only. This is recorded in the skill reference.
  - **Salvage click:** one salvage click did nothing in PIE after a click on the button's inner text. A clean session's click on the button worked; this is recorded in the skill reference as well.
- **Full test run:** `ue.py test SurvivalRpg.Portals SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **487 of 487 passed**, 17 with the known warnings.
- **Network tests:** `ue.py test SurvivalRpg.Network.PhysicalStoragePIE`: **4 of 4 passed**, only `LogNetPackageMap` warnings.
- **Cook:** `ue.py cook Lvl_HarvestPickaxe`: passed, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **Skill sync:** `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players:
  - **Opening:** the host opened the Runenschrein with F. The new screen showed:
    - "Brand I / Carried" with the icon;
    - the rank I description and "Next: Rank II" with its description;
    - Rune Essence 3, Ore 4 and Charcoal 2, each with 0 in chests;
    - "The connected chests lack materials." over a disabled "Upgrade to Rank II";
    - "Returns 2 Rune Essence" over Salvage.
  - **Upgrade:** for the check only, Brand's rank II was made free in memory, then restored and not saved. A click on "Upgrade to Rank II" turned the row and title into "Brand II", and the preview and costs moved to rank III.
  - **Salvage:** a click on Salvage removed the rune. The screen then showed the empty hint, and the details collapsed.
  - **Client:** in the client window the rune screen was not opened, because a client's view cannot be aimed with the PIE tools. The client path is covered by the MST-08a network test.

## Playtest

1. **Open:** in `Lvl_HarvestPickaxe`, walk to the Runenschrein beside the Kiln and press F. The rune screen opens, not the crafting screen.
   - **Left:** "Your runes" with the starter Brand ("Brand I", "Carried", its icon).
   - **Middle:** what rank I does, and "Next: Rank II" with what rank II does.
   - **Right:** Rune Essence, Ore and Charcoal with "Each" and "In chests", a reason, a disabled "Upgrade to Rank II", and below "Returns 2 Rune Essence" over a red Salvage.
2. **Upgrade:** put 3 Rune Essence, 4 ore and 2 charcoal into the shared chest. The counts fill, the row shows "Ready", and "Upgrade to Rank II" raises the rune: "Brand II", and the preview shows rank III.
3. **Socketed rune:** a Brand socketed in the pickaxe reads "Socketed in Pickaxe" and "Take it out of Pickaxe to salvage it."; Salvage is disabled.
4. **Salvage:** Salvage removes the selected rune; the next one is selected, or the hint appears.
5. **Kiln:** the Kiln still opens the crafting screen, without any Runes section.
6. **Multiplayer:** in a two-player listen server, the client opens the Runenschrein and sees only its own runes; its upgrade and salvage change only its runes.

## Limits

- **Client view in PIE:** the client's screen was not opened in the agent PIE check; the network test covers the client requests.
- **Socketed rune in PIE:** only the read model test covers the socketed line; the agent check did not socket a rune.
- **Input:** no gamepad shortcuts for upgrade and salvage. The buttons are reached with focus navigation, and the list takes focus first.
- **Costs and essence:** these remain the MST-08a first tuning.

## Next

MST-09a: Harvesting Echo, Kraftuebertrag and Runenfluss.
