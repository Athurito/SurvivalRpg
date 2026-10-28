# MST-07c: Guaranteed rewards when a portal closes

Task report for MST-07c in the [mastery map roadmap](mastery-map-roadmap.md), [PR #322](https://github.com/Athurito/SurvivalRpg/pull/322). It adds the portal source of the guaranteed rewards from [MST-07b](mastery-map-rune-rewards.md), as the user accepted on 2026-10-28:
- Closing an eligible portal rewards the player who closes it and everyone who was in the realm.
- Each completion is its own reward.

## C++ boundary decision

- **Classification:** an extension of the portal completion mechanism and its message, and one encounter field. The reward and where it applies are content.
- **Runtime truth:**
  - `ARpgPortalActor` owns the completion and its participants.
  - The claims stay in each player's `URpgRewardClaimComponent` (MST-07b): server-authoritative, owner-replicated and saved.
- **Existing seams:**
  - `ARpgPortalActor::TryClosePortal` and its `FRpgPortalCompletedMessage`.
  - The realm participant states the portal already keeps per player net id.
  - The encounter definition's reward category.
  - The plugin-owned reward adapter pattern of `FRpgPortalStorageProgressionHook`.
- **Native:**
  - **`URpgPortalEncounterDefinition::CompletionReward`:** an optional `URpgRewardClaimDefinition`.
  - **`FRpgPortalCompletedMessage`:** gains `Participants` and `CompletionId`.
    - Participants are the closing player plus every connected player with a realm participant state, each once.
    - `CompletionId` is a new GUID per close.
  - **`FRpgPortalCompletionRewards::GrantCompletionRewards`:** a plugin-owned adapter like the storage progression hook, called on the server in `TryClosePortal`.
    - It grants the reward to every participant of an eligible completion.
    - The claim id is `Portal/<Encounter>/<CompletionId>`, and the source is `Rpg.Reward.Source.Portal`, shown as "Portal".
    - A repeated call grants nothing.
  - **New native classes:** none. The adapter is a plain struct with static functions.
- **Assets (Unreal MCP):** `DA_PortalEncounter_RootGrove` (`GF_Dev_Sandbox`), the encounter of the playtest portal in `Lvl_ResourceParity`, now gives `DA_Reward_HarvestingRuneChoice` (a rune of the player's choice).
  - `GF_Dev_Sandbox` may reference `GF_Harvesting_Magic`.
  - The core portal encounters in `GF_Portals_Core` keep no reward, because that plugin does not depend on the harvesting feature.

## Behavior

### Problem

Closing a portal gave only the one-time Rift containment reward, and no guaranteed reward for the players who cleared it.

### Change

- **Who receives it:** when an eligible portal closes, the closing player and every player who entered its realm and is still connected each receive the encounter's completion reward.
- **Where to claim it:** it appears on the Character tab (N) under Rewards, for the Root Grove portal as "Brand (Portal)".
- **Eligibility:** it follows the encounter's existing rules. The Root Grove portal still needs its realm boss defeated.
- **Once per completion:** each completion is its own reward, so a later portal rewards again, while one completion never rewards a player twice.
- **Delivery:** claiming, full inventories and saves behave as in MST-07b.

## Validation

Run on 2026-10-28 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Portals.Rewards.EligibleCompletionRewardsEveryParticipantOnce`:**
    - An ineligible completion grants nothing. An eligible one rewards both participants once, and a null participant is ignored.
    - The same completion grants nothing again, and a new completion rewards both again.
    - The claim carries the reward and the portal source, and the id names the encounter.
    - The closer's claim delivers into their inventory and leaves the partner's open.
    - An encounter without a reward grants nothing.
  - **`SurvivalRpg.Portals.Content.ResourceParityPortalRewardsARune`:** the Root Grove encounter's completion reward is a valid choice offering Brand, and rewards still need the realm boss.
- `ue.py test SurvivalRpg.Portals SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting SurvivalRpg.Save SurvivalRpg.Combat SurvivalRpg.Inventory SurvivalRpg.Equipment SurvivalRpg.Runes SurvivalRpg.Crafting SurvivalRpg.BaseStorage SurvivalRpg.Storage --null-rhi`: **481 of 481 passed**. The 17 tests with warnings are the known 16 plus the expected full-inventory warning of `SurvivalRpg.Portals.StorageProgression.FullInventoryDefersGrantWithoutEphemeralDrop`.
- `ue.py test SurvivalRpg.Network.PortalClosePIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.InventoryReorderPIE`: **11 of 11 passed**, only `LogNetPackageMap` warnings.
- `ue.py cook Lvl_ResourceParity`: **passed**, 0 errors; the 3 warnings are the disabled Steam subsystem, the missing GameplayCue notify paths and the `CUI_RespawnScreen` asset log.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.
- **Agent PIE check** in `Lvl_ResourceParity`, listen server with two players.
  - **Temporary setting:** for the check only, `bRewardsRequireBossDefeat` of the Root Grove encounter was off in memory, so the portal could be closed without the boss. It was set back and saved unchanged.
  - **Host:** entered Root Grove through the portal ("Enter Root Grove", F) and became a participant.
  - **Close:** `Rpg.Portal.MakeSealable` made the portal sealable, and the client closed it from outside ("Close Portal", F).
  - **Rewards:** both Character tabs then showed Rewards with "Brand (Portal)". The host received it as a realm participant, the client as the closer. The client claimed it, and the section disappeared.

## Playtest

1. **Run the portal:** in `Lvl_ResourceParity`, enter the portal ("Enter Root Grove"), defeat the realm boss, leave through the exit, and close the portal ("Close Portal").
2. **Claim:** press N. Under Rewards, "Brand (Portal)" appears; click it to receive a Brand rune.
3. **Multiplayer:** in a two-player listen server, both players enter the realm and one closes the portal. Both get "Brand (Portal)". A player who never entered the realm and did not close it gets nothing.

## Limits

- **Disconnected players:** a player who entered the realm but is disconnected when the portal closes receives nothing, because only connected player states are rewarded.
- **Close without the boss:** the agent's PIE check closed the portal without the boss, with the eligibility rule switched off in memory. The boss rule itself is unchanged and covered by the ineligible case of the native test.
- **Only the dev-sandbox portal:** only the Root Grove portal has a reward. The core Rift Grunt encounters stay without one until their plugin owns a fitting reward.
- **Reload not seen in PIE:** saves behave as in MST-07b; a reload was not seen in PIE.

## Next

MST-08: in-place rank II and III upgrades, salvage and the Runenschrein presentation.
