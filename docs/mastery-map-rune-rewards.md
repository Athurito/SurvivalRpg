# MST-07b: Guaranteed rune rewards and rune drops

Task report for MST-07b in the [mastery map roadmap](mastery-map-roadmap.md). It covers the reward part of the [implementation plan](mastery-map-implementation-plan.md), section "Physical runes and the workstation loop":
- Loot tables and a guaranteed selectable reward use authored reward data.
- Claim identity is saved, so a reconnect cannot claim twice.
- A full inventory preserves the claim.

The recipe part is [MST-07a](mastery-map-rune-recipes.md).

## User decision and scope (2026-10-28)

- **Sources:** guaranteed rewards are configurable, from mastery milestones and from closing a portal. Runes also drop from loot pools.
- **Split:**
  - **MST-07b (this report):**
    - the reward schema and the player's claims;
    - mastery milestones as the first source;
    - claiming on the Character tab;
    - Brand in the vein and pine loot pools.
  - **MST-07c:** portal completion as the second source. It needs the portal's participants and its own claim identity per completion.

## C++ boundary decision

- **Classification:**
  - A new native schema, the reward definition.
  - A new native mechanism, the player's claims.
  - Two seams extended: the skill tree's mastery milestones and the player save.
  - Rewards, milestone levels, loot rows and the panel are content.
- **Runtime truth:**
  - `URpgRewardClaimComponent` on the PlayerState owns the player's open claims and the ids already claimed.
  - The server mutates them. Open claims replicate only to the owning player, and the host saves both with the player.
  - The items of a claimed reward live in the inventory graph like any other item.
- **Existing seams:**
  - `URpgTradeSkillProgressionComponent::HandleSkillLevelUp`, which was kept free for milestone choices.
  - The skill tree definitions and component.
  - The player save (`FRpgPlayerSaveData` and the GameMode's capture and restore).
  - The inventory's all-or-nothing pickup batch and the loot resolver.
  - The Character tab's `URpgSkillProgressionViewModel`.
- **New native classes (2):**
  - **`URpgRewardClaimDefinition`:** a schema type that assets configure. A reward is either a choice, `Options` of item and count from which the player picks one, or a loot pool, a `LootTable` rolled once when claimed.
  - **`URpgRewardClaimComponent`:** a mechanism that needs server authority, owner-only replication, a server RPC with a client result, persistence, and idempotency:
    - `GrantClaim(Id, Reward, Source, Level)` refuses an id that is open or claimed.
    - `ClaimReward(Id, Option)` delivers through `CanAddPickupBatch` and `AddPickupBatch`. Without room it returns `NoRoom`, and the claim stays open.
    - A loot pool roll is seeded by the claim id, so a retry rolls the same items.
- **Extended native types:**
  - **`FRpgSkillTreeMasteryMilestone`:** `URpgSkillTreeDefinition::MasteryMilestones` holds a mastery level and a reward per entry. The claim id is `<TreeTag>/Mastery/<Level>`.
  - **`URpgSkillTreeComponent::GrantReachedMasteryMilestones`:** grants every milestone reached so far. Three places call it:
    - each trade skill level-up;
    - the GameMode after restoring a player;
    - tests.
    A jump over several levels or a milestone added later is therefore never missed.
  - **`FRpgPlayerSaveData`:** player schema 5 with `bHasRewardClaims` and `RewardClaims`, validated with the rest of the player save. No migration; older saves start without claims.
  - **`URpgSkillProgressionViewModel`:**
    - `RewardClaims`, `RewardFeedbackText`, `GetRewardClaimCount`, `GetRewardChoiceCount` and `GetRewardChoiceLabel`.
    - `ClaimRewardChoice`, which sends the server RPC.
    - The result of a claim, for example "No room in your inventory. The reward stays open.", arrives through `OnClaimResult`.
- **Assets (Unreal MCP):**
  - **`DA_Reward_HarvestingRuneChoice`** (`GF_Harvesting_Magic/Items/Runes`): "Rune of your choice", with Brand as its only option until more harvesting runes exist.
  - **`DA_SkillTree_Harvesting`:** a mastery milestone at Harvesting level 3 with that reward.
  - **`LT_IronVein` and `LT_DeadPine`:** a new independent group with Brand at 2 % per roll, scaled by rare find.
  - **`CUI_Character`:**
    - A Rewards section above the professions: `RewardsBox` with `RewardList` and `RewardFeedback`.
    - `F_RefreshRewards`, called from `F_Refresh`, creates one button per choice, for example "Brand (Harvesting level 3)". It hides the section without open rewards.
    - `F_HandleRewardChoiceClicked` claims the clicked choice.

## Behavior

### Problem

Runes could only come from the starter or a recipe. Nothing guaranteed a rune, and no source could promise a reward that survives a full inventory or a reconnect.

### Change

- **Milestone reward:** reaching Harvesting mastery level 3 earns "Rune of your choice". The Character tab (N) shows a Rewards section with one button per choice, for example "Brand (Harvesting level 3)".
- **Claiming:** clicking a choice puts a new rank-I rune into the inventory, and the section disappears.
- **Full inventory:** the claim stays open and the panel says "No room in your inventory. The reward stays open."
- **Once only:**
  - Each milestone is granted once per player.
  - After claiming, the same milestone never returns, also after a reload.
  - Open claims are saved with the player.
- **Drops:** mining iron veins and chopping dead pines can drop a Brand rune, 2 % per roll and more with rare find.
- **Co-op:** each player earns and claims their own rewards. A host's claim does not touch a client's.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Progression.Rewards.ClaimsAreGrantedOnceAndSurviveAFullInventory`:**
    - **Grants:** an open id is refused, and so is an invalid reward.
    - **Refusals:** an unknown claim and an invalid choice are refused.
    - **Full inventory:** a claim returns `NoRoom`, stays open and adds nothing. After a filler is dropped, the claim delivers rune A.
    - **Claimed ids:** a resolved id cannot be claimed or granted again.
    - **Loot pool:** it has no room at first and delivers its two items after room frees up.
    - **Saves:** export and import keep open claims and resolved ids. A loaded resolved id is refused, and invalid data is rejected, also in `FRpgPlayerSaveData::IsSchemaSupported`.
    - **Clients:** a client can neither grant nor claim.
  - **`SurvivalRpg.Progression.Rewards.MasteryMilestonesGrantOnce`:**
    - Milestones at levels 2 and 4 are granted as the mastery skill reaches them, and asking again grants nothing.
    - The progression view model lists four choices, labeled with item and source.
    - Claiming the third choice resolves the second milestone and puts the rune into the inventory. A claimed milestone is not granted again.
  - **`SurvivalRpg.Harvesting.Content.RuneRewardContract`:** the harvesting tree has a milestone of 2 or higher offering Brand as a valid choice, and both loot pools drop Brand as a rare independent find.
- **Changed tests:** `SurvivalRpg.Save.WorldSave.MemoryRoundTrip` now also round-trips a player's open claim (id, reward path, source, level) and resolved ids.
- **Failed attempts while authoring:**
  - **Panel call:** the first panel script used `Widget|AddChildtoVerticalBox`. A vertical box's call is in the category `Panel`, recorded in the skill reference.
  - **Label length:** the first label, "Rune of your choice (Harvesting level 3): Brand", overflowed the narrow panel in PIE. It is now "Brand (Harvesting level 3)", and a loot pool uses the reward's name.
- `ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **475 of 475 passed**; 16 with the known warnings.
- `ue.py test SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.RuneSocketPIE SurvivalRpg.Network.InventoryReorderPIE SurvivalRpg.Network.KillXPCuePIE`: **14 of 14 passed**, only `LogNetPackageMap` warnings.
- `ue.py cook Lvl_HarvestPickaxe`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players. Mastery XP came from `Rpg.Progression.AddSkillXP Skill.Mastery.Harvesting 400` for both players.
  - **Host:**
    - The Character tab (N) showed Rewards with "Brand (Harvesting level 3)".
    - Clicking it removed the section. The pockets then held two Brand runes, the starter's and the new one.
  - **Client:**
    - The client's own Character tab still offered its reward after the host had claimed theirs.
    - The click went through the server RPC, and the client's pockets held two Brand runes.
  - **Restart:** after a PIE restart both players started at Harvesting level 1 again, because PIE kept no save between sessions. The save round trip was therefore not seen in a running game.

## Playtest

1. **Earn:** in `Lvl_HarvestPickaxe`, reach Harvesting level 3 by mining and chopping, or run `Rpg.Progression.AddSkillXP Skill.Mastery.Harvesting 400` on the host.
2. **Claim:** press N. Above Professions, Rewards shows "Brand (Harvesting level 3)". Click it: the section disappears, and a second Brand lies in your pockets (Tab).
3. **Full inventory:** with a fresh reward and full pockets, clicking says there is no room, and the reward stays.
4. **Drops:** mine iron veins and chop dead pines for a while. Now and then a Brand rune drops with the ore or wood.
5. **Multiplayer:** in a two-player listen server, the client earns, sees and claims their own reward, independently of the host.

## Limits

- **Portal rewards:** these follow in MST-07c.
- **Reload not seen in PIE:** a reload in a running game was not seen, because PIE keeps no save between sessions. The component's save round trip, the player save validation and the world save's disk format are covered by tests. The GameMode's restore order (claims after skill trees, then the catch-up grant) has no automated test of its own.
- **Only Brand to choose:** "Rune of your choice" offers only Brand until more harvesting runes exist.
- **Simple panel:** the Rewards section is a plain button list with text labels, without icons or a dedicated reward screen.
- **Drop rate is a first guess:** the 2 % Brand drop rate is first tuning and has not been checked in long sessions.

## Next

MST-07c: guaranteed rewards when an eligible portal closes, for the instigator and the realm's participants, with one claim identity per completion.
