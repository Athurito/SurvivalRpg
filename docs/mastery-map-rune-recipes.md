# MST-07a: Rune recipes known to the world

Task report for MST-07a in the [mastery map roadmap](mastery-map-roadmap.md), [PR #320](https://github.com/Athurito/SurvivalRpg/pull/320). It covers the recipe part of the [implementation plan](mastery-map-implementation-plan.md), section "Physical runes and the workstation loop":
- Discovering a rune unlocks its base recipe without consuming the rune.
- Crafting produces a new rank-I instance.
- Base rune recipes are shared knowledge of the current world and are saved with it.

## User decision and scope (2026-10-28)

- **Recipe scope:** the user chose recipe knowledge per world, the plan's proposed co-op default. Every player of a world shares what it has learned, and a rune's items and ranks stay with their players.
- **Split:** MST-07 is split, so each PR stays bounded:
  - **MST-07a (this report):** world recipe knowledge, learning by finding, and rank-I crafting.
  - **MST-07b:** rune drops through loot tables, and guaranteed reward claims that cannot be claimed twice and survive a full inventory.

## C++ boundary decision

- **Classification:** an extension of two existing native mechanisms, plus content:
  - **Persistence:** the GameState's recipe unlock store is now saved with the world.
  - **Learning:** a rune fragment field and one server hook teach recipes.
  - The recipe, its costs and its station are content.
- **Runtime truth:**
  - `URpgRecipeUnlockComponent` on the GameState owns what the world knows. The server mutates it, it replicates to every player, and the host saves it with the world snapshot.
  - Rune items, their ranks and their places stay in the inventory graph.
- **Existing seams:**
  - `IRpgWorldSaveParticipant`: the world snapshot already saves GameFeature state such as harvest stocks through it.
  - The inventory's `OnInventoryPostCommit`: the equipment manager already reacts to committed inventory changes there.
  - The crafting station's recipe sets, unlock check and order transaction.
- **Native:**
  - **`URpgRecipeUnlockComponent`:**
    - It is a world save participant (feature id `Crafting.RecipeUnlocks`, schema 1). It registers with the GameMode when it begins play, marks the world save dirty when the world learns a recipe, and restores the saved tags or the pristine state.
    - It keeps and restores only concrete `Recipe.*` tags.
  - **`URpgInventoryFragment_Rune`:**
    - The new field `BaseRecipe` names the recipe that crafts a new rank-I copy of the rune.
    - `LearnBaseRecipesOfRunesIn` teaches the world the base recipes of all runes in an inventory, including runes in a tool's sockets. It runs on the server only and never consumes a rune.
    - Data validation requires the base recipe to output the rune, to be locked by default and to have an unlock tag.
  - **`ARpgPlayerState`:** on the server, it calls `LearnBaseRecipesOfRunesIn` after each committed change of its own inventory. A rune therefore counts as found as soon as a player carries it, by pickup, loot, transfer, starter or load. A rune in someone else's chest teaches nothing.
  - **New native classes:** none.
- **Assets (Unreal MCP):**
  - **`DA_Recipe_Rune_Brand`** in `GF_Harvesting_Magic/Crafting`:
    - Brand Rune, category `Crafting.Category.Runes`, tier I.
    - Unlocked by `Recipe.Rune.Brand` and locked by default.
    - Needs the Kiln. Costs 4 ore and 2 charcoal and takes 6 s.
    - Outputs one Brand.
  - **`DA_RecipeSet_Kiln`:** offers it after the charcoal recipe.
  - **`ID_Rune_Brand`:** its rune fragment's `BaseRecipe` points at the recipe.
  - **Tags:** `Recipe`, `Recipe.Rune.Brand` and `Crafting.Category.Runes`.

## Behavior

### Problem

Recipe unlocks lived only for the session, and nothing taught a recipe, so a found rune could never be crafted again.

### Change

- **Learning by finding:** as soon as any player carries a Brand rune, in the pockets, a bag or a tool's sockets, the world learns the Brand Rune recipe. The rune stays where it is.
- **Shared:** every player of the world sees the recipe at once, also players who join later.
- **Saved:** the learned recipe is saved with the world and known again after loading.
- **Crafting:** the Kiln lists Brand Rune under Runes. With 4 ore and 2 charcoal in a connected chest, one run takes 6 s and puts a new Brand at rank I into the chosen chest. It is its own item; the found rune keeps its rank.
- **No side effects:** a locked recipe cannot start. An order whose target chest is full is refused without taking materials, as for every station recipe.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Runes.Recipes.FindingARuneTeachesTheWorldItsRecipe`** uses the real player state hook:
    - A rune in a chest or a plain item teaches nothing.
    - A found rune teaches its recipe and is not consumed.
    - A second rune teaches nothing new.
    - A rune that sits only in a tool's socket teaches it again after the world forgot it.
    - A client never teaches.
  - **`SurvivalRpg.Runes.Recipes.RecipeKnowledgeSavesWithTheWorld`** uses the RPG GameMode and GameState:
    - Beginning play registers the store with the world save.
    - The learned recipe survives the world snapshot's disk format and a pristine reset.
    - Tags outside `Recipe`, another schema and a client restore are rejected without changing what is known.
  - **`SurvivalRpg.Crafting.Order.CraftedRuneIsANewRankOneCopy`:**
    - The station lists the locked rune recipe but cannot start it.
    - After unlocking, a full target refuses the order without taking materials.
    - An order pays one run and delivers a second rune with its own id at rank I, beside a found rune at rank II.
  - **`SurvivalRpg.Harvesting.Content.RuneRecipeContract`:**
    - `ID_Rune_Brand` names a locked base recipe with a `Recipe` tag, which crafts one Brand and costs materials.
    - The Kiln's recipe set offers the recipe, and the Kiln meets its station tags.
- **Failed attempts while writing the tests:**
  - **Locked recipe:** the first crafting test expected the locked recipe to be missing from the station's offers. The station lists locked recipes so the screen can show them, and only `IsRecipeUnlocked` and the order check refuse them.
  - **Sockets:** the first discovery test added a rune straight into a socket. Sockets accept only an explicit move, so the test moves the rune there through the inventory's move intent.
- `ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **472 of 472 passed**; 16 with the known warnings.
- `ue.py test SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.InventoryReorderPIE SurvivalRpg.Network.PhysicalStoragePIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.SkillTreePIE`: **14 of 14 passed**, only `LogNetPackageMap` warnings.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players. Nothing was changed for the check.
  - **Recipe state:** on both the server's and the client's GameState, `UnlockedRecipeTags` held `Recipe.Rune.Brand`, because the starter gives each player a Brand.
  - **Host:**
    - At the Kiln, F opened the station.
    - It listed Brand Rune under Runes, tier I, 1 x 1 cells, 4 ore and 2 charcoal, 6 s.
    - It showed "Missing materials" with the empty shared chest, not a lock.
  - **Client:**
    - The client's station screen was not opened. `set_pie_view_rotation` reaches only the host's world, and the client view could not be aimed at the Kiln.
    - This limit is now recorded in the skill reference `mcp-widget-recipes.md`.

## Playtest

1. **Recipe:** in `Lvl_HarvestPickaxe`, walk to the Kiln (the grey block) and open it with F. Under Runes it lists Brand Rune, which needs 4 ore and 2 charcoal. The starter's Brand in your pockets taught it.
2. **Materials:**
   - Mine iron veins until you have 4 ore.
   - Chop dead pines for wood, and turn 4 wood into 2 charcoal at the Kiln.
   - Put the ore and the charcoal into the shared chest next to the Kiln.
3. **Craft:** start Brand Rune. After 6 s a new Brand at rank I lies in the chest, and your own Brand is still in your pockets.
4. **Multiplayer:** in a two-player listen server, the client opens the Kiln and sees Brand Rune unlocked too. Either player can craft it from the shared chest.

## Limits

- **The lock is hard to see:** every player starts with a Brand from the starter, so a test world knows the recipe from the first moment. The locked state and the save round trip are covered by the automated tests.
- **No other recipes:** only Brand has a base recipe; the other runes come with MST-09.
- **Not crafted by hand in PIE:** a full craft in a running game is covered by the native station test. The agent's PIE session had no materials.
- **Client screen not seen:** the client's station screen was not opened by the agent. Its replicated recipe state was checked instead.
- **Drops and guaranteed rewards:** rune drops and guaranteed reward claims follow in MST-07b.

## Next

MST-07b: rune drops through loot tables and guaranteed selectable reward claims, saved so that a reconnect cannot claim twice and a full inventory keeps the claim.
