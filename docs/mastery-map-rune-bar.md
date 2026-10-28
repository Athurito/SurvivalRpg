# MST-06d: Rune bar on the skill screen

Task report for MST-06d in the [mastery map roadmap](mastery-map-roadmap.md), [PR #319](https://github.com/Athurito/SurvivalRpg/pull/319). It completes the rune context of the [implementation plan](mastery-map-implementation-plan.md), section "Accepted interaction and gameplay contract": "the current tool and its three rune sockets are context for the same harvesting tree, shown as a strip over the map", and a click on a rune highlights its talents without changing gameplay state. MST-06c moved this part out of its own scope ([rune talents](mastery-map-rune-talents.md)).

## C++ boundary decision

- **Classification:** an extension of the skill screen's view models and two Widget Blueprints. No new class.
- **Runtime truth:** unchanged. The bar is a read-only projection of the inventory (sockets, ranks) and the equipment (the item in the main hand). The highlight is local UI state that is never replicated or saved.
- **Native:**
  - **Struct:** `FRpgSkillTreeRuneSlotView` holds one socket: rune tag, label such as "Brand II", icon, highlighted.
  - **Rune bar fields:** `URpgSkillProgressionViewModel` gains `bHasRuneTool`, `RuneToolName`, `RuneSlots` and `HighlightedRuneTag`, plus:
    - the command `ToggleRuneHighlight(SlotIndex)`;
    - the Blueprint helpers `GetRuneSlotLabel`, `IsRuneSlotFilled` and `IsRuneSlotHighlighted`.
  - **Live updates:**
    - The progression view model now listens to the player's inventory and equipment messages, like the inventory screen.
    - Socketing, unsocketing or switching tools updates the bar and re-derives the sleeping talents of every tree at once, instead of only when the screen is built.
    - An explicit `Refresh` does the same.
  - **Highlight propagation:**
    - `URpgSkillTreeViewModel::SetHighlightedRune` marks the nodes that need the rune (`URpgSkillTreeNodeViewModel::bIsRuneHighlighted`) and announces the change, so the map restyles its entries.
    - The highlight ends when its rune leaves the tool in hand.
- **Assets (Unreal MCP):**
  - **`CUI_Skills`:**
    - **Row:** a slim row `RuneBar` under the top bar, with `RuneToolText` and `RuneSlotButtons`.
    - **`F_RefreshRuneBar`:** called from `F_Refresh`, it fills one button per socket in the style of the region shortcuts. A highlighted rune reads "> Brand I", and an empty socket is a disabled "Empty".
    - **`F_HandleRuneSlotClicked`:** toggles the clicked socket's rune.
  - **`CUI_SkillTreeNode`:** the new function `F_ApplyRuneHighlight` frames a highlighted talent in violet; a selected node keeps its gold frame.

## Behavior

### Problem

The skill screen showed whether a rune talent sleeps, but not which tool and runes the player carries, nor which talents a rune drives.

### Change

- **Rune bar:** with a socketed tool in the main hand, a row under the top bar names the tool and shows its three sockets, for example "Pickaxe: Brand I, Empty, Empty". Without such a tool the row is hidden.
- **Highlight:** clicking a rune frames its talents on the map in violet; clicking it again, or socketing it out, ends the highlight. Highlighting changes nothing in gameplay.
- **Live:** the bar and the sleeping state follow socket and tool changes while the screen exists, also on clients.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **Extended test:** `SurvivalRpg.Runes.Talents.RuneBoundNodesSleepWithoutTheirRune` now also binds a progression view model:
  - It shows the tool in hand with three sockets, and the first holds rune A.
  - Clicking rune A highlights it and its talent but not a talent without runes, and clicking again ends the highlight.
  - An empty socket highlights nothing.
  - After rune A leaves the tool, the highlight ends and the bar shows the emptied socket.
- `ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage --null-rhi`: **455 of 455 passed**; 16 with the known warnings.
- `ue.py test SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.InventoryReorderPIE`: **3 of 3 passed**, only `LogNetPackageMap` warnings.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- **Failed attempts while authoring:**
  - The first graph write used `Class|RpgSkillProgressionViewModel|...` ids; UFUNCTIONs with a category are `SkillTree|Runes|...`.
  - A border's color pin is `BrushColor`, not `InBrushColor`.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players. For the check only, the starter also gave a pickaxe; this was reverted and not committed.
  - **Host:**
    - With the pickaxe in Weapon I and Brand in its first socket, the bar read "Pickaxe: Brand I, Empty, Empty", with the empty sockets disabled.
    - Clicking Brand I framed Discharge in violet, while the selected Lasting Power kept gold, and the button read "> Brand I".
    - Clicking again removed the highlight.
  - **Client:**
    - Without a socketed tool in hand, there was no bar.
    - After the client equipped its pickaxe and socketed its Brand, the bar appeared while the screen existed.
    - Clicking Brand I framed both Aftermath talents.

## Playtest

1. **Bar:** in `Lvl_HarvestPickaxe`, put the pickaxe or axe into Weapon I and socket Brand. Press H: a row under the top bar shows the tool and its sockets, "Brand I" and two disabled "Empty".
2. **Highlight:** click Brand I. Lasting Power and Discharge get a violet frame; click again and it is gone.
3. **Live:** take Brand out of the tool and open H again. The bar shows three empty sockets, and the Aftermath talents sleep.
4. **Multiplayer:** in a two-player listen server, each player sees only their own tool and runes in the bar.

## Limits

- **Text instead of icons:** the bar shows rune names, not icons. The view model already carries each rune's icon for a later visual pass.
- **Main hand only:** a socketed item in the off hand is not shown.
- **Highlight marker:** a highlighted rune is marked with "> " in its label; a styled toggle state can replace that later.

## Next

MST-06 is complete. MST-07 (rune drops, recipe knowledge, rank-I crafting) needs the user's decision on recipe knowledge: shared per world or per player.
