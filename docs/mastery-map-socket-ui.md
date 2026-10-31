# MST-06a: Tool sockets in the inventory

> **To be replaced (2026-10-31):** HRW-05 of the [harvesting rework roadmap](harvesting-rework-roadmap.md) redesigns the harvesting runes and rune-gated talents, and the mastery map roadmap marks this playtest superseded. Until HRW-05 merges, this report still describes the running code.

Task report for MST-06a in the [mastery map roadmap](mastery-map-roadmap.md), [PR #316](https://github.com/Athurito/SurvivalRpg/pull/316). It makes the rune sockets of [MST-05](mastery-map-rune-sockets.md) usable by hand, which that task's limits named as the first step of MST-06.

## User decisions (2026-10-28)

- **Split:** MST-06 becomes three PRs.
  - MST-06a: tool sockets in the inventory.
  - MST-06b: Brand's burn as real harvest work.
  - MST-06c: the activation evaluator, Brand's talents, the dormant state and the rune bar on the skill screen.
- **Brand (accepted proposal):**
  - Brand in a socket sets the resource burning on the tool's own hits for 4 s. The burn does 10, 15 or 20 % of a hit's work per second at rank I, II or III.
  - The yield stays the same, and rewards and XP go to the player who lit it.
  - **Anhaltende Macht (Lasting Power, 1 point):** the burn lasts 50 % longer, and a new hit refreshes it.
  - **Entladung (Discharge, 3 points):** a hit on a burning resource releases the remaining burn at once, with 50 % more work.
  - **Wandernde Wirkung (Wandering Effect):** stays a preview until Erntesprung exists.
  - All values are tunings in assets.

## C++ boundary decision

- **Classification:** an extension of the player inventory layout and its view models, plus one Widget Blueprint. No new class.
- **Runtime truth:**
  - Unchanged from MST-05: the inventory graph owns the sockets and the runes in them, and the server's planner validates every insertion.
  - The new socket groups are a read-only projection of that graph, built on the server and on the owning client.
- **Native:**
  - **Group kind:** `ERpgInventorySlotGroupKind::Socket`.
    - Every automatic path filters on `Content`: pickups, first-fit placement, quick transfers and the equip fallback. The new kind keeps sockets out of all of them without touching each path.
    - Static layout validation rejects it, so designers cannot author a static socket group.
  - **Layout:** `URpgPlayerInventoryLayoutComponent` adds a socket group for each socket grid of the item in a carry slot.
    - `FRpgInventorySlotGroupView::SourceCarryRole` names that carry slot. The group gets no semantic role of its own, so the carry roles stay unique.
    - `CanItemUseSlotAddress` accepts an item in a socket group by the grid's category and tag rule; the planner still applies the one-rune-per-kind rule.
    - Gear providers such as backpacks now show only their storage grids, never a socket grid.
  - **View models:**
    - `URpgInventorySlotGroupViewModel` gains `IsSocketGroup` and `GetSourceCarryRole`.
    - `URpgPlayerInventoryViewModel` gains `GetSocketGroups` and `GetSocketGroupForCarryRole`.
  - **Pane:** `URpgPlayerInventoryPaneWidget` binds the optional hosts `Sockets_Weapon1` and `Sockets_Weapon2` to the socket group of their carry slot and collapses them without one. They navigate, accept drops and release like the content grids. The shield slot has no host, because shields have no sockets.
- **Assets (Unreal MCP):**
  - **New:** `CUI_InventorySocketStrip`, a copy of `CUI_InventorySlotGroupEntry` without the header row and with 40-unit cells, so three sockets fit below a weapon slot.
  - **Changed:** `CUI_PlayerInventoryPane` hosts one strip below each of the two weapon slots.

## Behavior

### Problem

MST-05 gave axe and pickaxe three rune sockets, but no screen showed a tool's item-owned grid. A rune could not be socketed by hand.

### Change

- **Sockets in the inventory:** an axe or pickaxe in Weapon I or Weapon II shows its three rune sockets right below that slot.
  - Drag a rune from the pockets onto a socket to socket it, and drag it back to take it out.
  - The server decides as before: only harvesting runes fit, and only one rune of each kind per tool.
  - A tool in the pockets, a backpack or a chest shows no sockets. Its runes stay in it and come back when the tool returns to a weapon slot.
- **No automatic socketing:** pickups, loot, quick transfers and unequipping never put an item into a socket, also when every other cell is full.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
  - **Failed attempt:** the first build failed in the new test, because `TestEqual` takes no native gameplay tag; the test compares with `==` now.
  - After the last native change, which removed the unused offhand host, the build passed again.
- **New test:** `SurvivalRpg.Runes.Sockets.CarrySlotShowsSockets`.
  - A socketed weapon in the pockets shows no socket group. In the primary carry slot it shows exactly one, with its item-owned handle and the carry role as source, and no content group exposes it.
  - The primary carry role stays unique.
  - A rune may target a socket and an item that is no rune may not; a socket is no content address.
  - The view model lists the group as a socket group with three slots, outside the inventory groups, and finds it by handle.
  - With the pockets full, a further grant fails instead of filling a socket.
  - An explicit move sockets the rune, and the first socket slot shows it.
  - Back in the pockets, the weapon keeps its rune and shows no sockets.
- `ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage --null-rhi`: **450 of 450 passed**.
  - The 16 with warnings are the known RequestId collisions of two equipment intent tests, asset logs of the UI lifecycle tests, and the AdvancedFriends and `LogNetPackageMap` messages of the PIE locomotion tests.
  - BaseStorage passes again since PR #315.
- `ue.py test SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.InventoryReorderPIE SurvivalRpg.Network.PhysicalStoragePIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.HarvestCuePIE`: **16 of 16 passed**, with only `LogNetPackageMap` warnings of the temporary PIE worlds.
- After the last asset change, `ue.py test SurvivalRpg.Runes SurvivalRpg.Inventory SurvivalRpg.UI --null-rhi`: 238 of 238 passed.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players.
  - **Setup:** for the check only, the starter component also gave the axe; this was reverted and not committed.
  - **First run:** the axe landed in Weapon I. Its socket strip had a title row and 56-unit cells, which hung over the panel's frame into the quick-access row. That led to `CUI_InventorySocketStrip`.
  - **Host:** dragged the axe from the pockets into Weapon II. Three sockets appeared below it, and dragging the Brand rune onto the first socket socketed it.
  - **Client:** the same through the client's request path; the client's own Brand rune went into the second socket of its own axe.
  - With the sword in Weapon I, no sockets showed under it.

## Playtest

1. **Axe in hand:** start `Lvl_HarvestPickaxe`, pick up the axe (look at it, F), press Tab and drag the axe into Weapon I or Weapon II. Three empty sockets appear below that slot.
2. **Socket:** drag the Brand rune from the pockets onto a socket. It sits there; dragging it back to the pockets takes it out.
3. **Rules:** any other item is refused by the sockets. A tool in the pockets shows no sockets and keeps its rune.
4. **Multiplayer:** in a two-player listen server, the client sockets its own rune into its own axe, and the host sees no change in its own inventory.

Brand has no effect yet; its burn arrives with MST-06b.

## Limits

- **Socket strip size:** the strip fits the equipment panel at 1280 x 720 with little room left. A smaller PIE client window (640 x 520) lets it touch the panel's frame.
- **Only weapon slots:** sockets show only for items in Weapon I and Weapon II. A tool must sit there to be socketed.
- **Not seen by hand:** keyboard and gamepad navigation into the sockets is wired like the content grids but was not checked in PIE.

## Next

MST-06b: Brand's burn as secondary harvest work through the existing stock and reward commit, with the igniting player as beneficiary.
