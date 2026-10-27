# MST-03b: Shared harvesting tree

Task report for MST-03b in the [mastery map roadmap](mastery-map-roadmap.md); design and acceptance contracts are in the [implementation plan](mastery-map-implementation-plan.md). It builds on [MST-02](mastery-map-skill-screen.md) and [MST-03a](mastery-map-harvest-work.md).

## User decisions (2026-10-27)

- **Old powers:** "Wie im Plan entfernen". The axe and pickaxe trees and their active powers are retired. They covered Rift Grip, Death Wave, Grave Swarm, their forms and Striding Wave. The shared tree starts with the Wucht (Impact) branch only. Until the first rune changes harvesting (MST-06), there is no active magical harvesting power. This leaves open the first-playable item "a magical harvesting ability like Rift Grip" in [game-vision.md](game-vision.md), section 12.
- **Wucht talents:** the proposal was accepted.
  - **Spaltkraft (Cleaving Force, 1 point):** own hits do 25 % more work.
  - **Zertruemmern (Shatter, 3 points):** a weak-point hit does twice its bonus work.
  - **Nachsetzen (Follow Through, 3 points):** after a weak-point hit, the next hit on the same resource within 3 s does 50 % more work.
  - Shatter and Follow Through exclude each other. All values are tunings in the tree asset.
- **Saves:** no migration, per the user's standing preference of 2026-10-26.
  - **Legacy states:** saved states of the retired trees stay unregistered. They grant nothing, requests for them are rejected, and their points are not refunded.
  - **Mastery start:** the shared mastery starts at level 1 for every player; old Logging and Mining XP is not converted.
  - This replaces the plan's migration steps for MST-03b.

## C++ boundary decision

- **Classification:** extensions of existing mechanisms plus designer content. No new class.
- **Runtime truth:**
  - `URpgSkillTreeComponent` owns the learned talents, keyed by the shared tree's tag.
  - `URpgTradeSkillProgressionComponent` owns the mastery XP.
  - The harvest ability owns the follow-up of its last weak-point hit, server-side only.
- **Native:**
  - **Mastery skill:** a core trade skill `Skill.Mastery.Harvesting`.
  - **Mastery XP:** `URpgHarvestRewardProfile::MasterySkillTag` and `MasteryExperience`, which `FRpgHarvestRewardService::AwardExperience` awards once per completed section, next to the profession XP. There is no generic XP propagation between tags.
  - **Weak-point scale:** `FRpgHarvestRequest::WeakPointWorkScale` scales a weak-point hit's bonus work in the stock rules.
  - **Tunings:** four tags under `Ability.Tuning.Harvest` (Work, WeakPointWork, FollowUpWork and FollowUpWindow), which `URpgGameplayAbility_Harvest` resolves into the request.
  - **Follow-up:** the harvest ability keeps the follow-up of its last weak-point hit (target, instance and deadline) on the server.
- **Designer content (Unreal MCP):**
  - **Tree:** `DA_SkillTree_Harvesting`, a free-position map with the region Impact and the three talents. It reuses existing skill icons.
  - **Tools:** `ID_Tool_Axe` and `ID_Tool_Pickaxe` point their skill tree fragment at it.
  - **Profiles:** `HP_DeadPine`, `HP_IronVein` and `HP_MiningNode` award mastery XP equal to their profession XP (9, 9 and 15 per section). `HP_Bush` and `HP_Tree` need no tool and award none.
  - **Display name:** `DA_TradeSkillData` names the mastery "Harvesting" with the pickaxe category icon.
- **Retired content:** deleted after checking that nothing else referenced it:
  - the trees `DA_SkillTree_Axe` and `DA_SkillTree_Pickaxe`;
  - the ability sets `AS_Node_DeathWave`, `GraveSwarm`, `RiftGrip` and `StridingWave`, and the four `GA_Harvest_*` powers;
  - their four `GE_Cooldown_Harvest_*` effects, `BP_HarvestSwarm_Grave` and the aim camera mode `CM_Harvest_Aim`.

  Their tree, node, ability, form and cooldown tags were removed from `DefaultGameplayTags.ini`; `ClearInvalidTags=False` keeps old saves loadable. The native swarm, stride, area and chain mechanisms stay for later talents and runes. So do the presentation cues and skill icons, which MST-09d may clean up.

## Behavior

### Problem

The axe and the pickaxe each carried their own tree with its own budget from Logging or Mining. That tree held active powers on Q/E/R. The mastery map needs one shared harvesting tree with one budget and talents that change how hits work.

### Change

- **One tree, one budget:**
  - H shows the Harvesting tree with axe or pickaxe. Learned talents count for both tools, because both carry the same tree.
  - Its points come from the mastery Harvesting: one per level above 1, at most 20.
  - Felling dead pines and mining iron veins or nodes advances Harvesting once per completed section, in addition to Logging or Mining. Partial work, rejected or stale requests and failed deliveries award nothing.
- **Talents:**
  - **Cleaving Force:** applies to every hit of the tool's harvest abilities, which are now the axe and pickaxe swings.
  - **Shatter:** doubles the weak point's bonus section.
  - **Follow Through:** arms on a weak-point hit and raises the next hit on that resource within 3 s.
  - The preview shows Cleaving Force and Shatter. Follow Through is server state and shows only in the result.
- **Q/E/R:** the tools grant only their swings, and the shared tree has no actives, so Q/E/R stay empty.
- **Skill screen:** the tree list holds only Harvesting, and the professions list shows the mastery as Harvesting. The map frames the Impact region. Region shortcut buttons wait until a second region exists; with one region, Fit frames it.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Harvesting SurvivalRpg.Interaction.Harvesting SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Save --null-rhi`: **167 of 167 passed**. The one warning is the existing `CUI_RespawnScreen` "may require a native tick".
  - **First run:** one failure, because the test binary predated a test edit that removed `HP_Tree` from the tool profiles. It passed after a rebuild.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.SkillTreePIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE`: **11 of 11 passed**. The warnings are the usual net GUID notes of the temporary PIE map.
- `python Build/Tools/Unreal/ue.py cook Lvl_HarvestPickaxe`: see the PR.
- New and changed tests:
  - `SurvivalRpg.Harvesting.Tuning.WorkTalentsShapeOwnHits`: a test tree with the three tunings on a real swing and weak points.
    - Cleaving Force makes 125 work, or 225 on a weak point.
    - Shatter makes 325 on a weak point.
    - Follow Through gives 188 on the next hit, then 125 again, and expires after 3 s.
    - Every swing is a direct hit, and the preview shows the tuned work.
  - `SurvivalRpg.Harvesting.Work.SharedMasteryXPOncePerSection`: mastery XP only for completed sections, none for partial or stale requests, and profession XP unchanged.
  - `...Work.StockRulesQuantizeWorkAndOverkill`, extended: the weak-point work scale, and rejection of a negative scale.
  - `SurvivalRpg.Harvesting.Content.SharedHarvestingTreeContract`, which replaces `AxeSkillTreeContract` and `PickaxeSkillTreeContract`:
    - both tools carry the same free-position tree with the mastery tag and a default region;
    - its talents grant no abilities and form one entry with an exclusive choice;
    - the tool resources award mastery XP.
  - `AxeToolContract` and `PickaxeToolContract`: the swings, renamed from the old tree contracts.
  - `TradeSkills.AuthorityAndLevelUp` and `TagRestoreDefaultsAndReconnect`: seven core skills.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, a listen server with two players, driven by `set_pie_input_key` and the Slate inspector:
  - On the client, H opens Skills with the Harvesting tree: Cleaving Force, Shatter and Follow Through on the map, the region framed, and the ritual tree behind.
  - Harvesting appears under Professions at level 1 with 0 points. A click on Cleaving Force shows its details with "Not enough points", and Learn stays disabled.
  - The host's screen shows the same tree at 1600 x 900, with labels, costs and locks.
  - **Fix found in PIE:** the first name, "Harvesting Mastery", overlapped "Level 1" in the professions list. It is now "Harvesting".

## Playtest

1. **Shared tree:** press H with the axe or the pickaxe. Skills shows one tree, Harvesting, with Cleaving Force on top and Shatter and Follow Through below it. Harvesting is listed under Professions.
2. **Mastery XP:** mine iron veins with the pickaxe and fell dead pines with the axe. Harvesting rises with both, alongside Mining and Logging. Each completed section counts once.
3. **Learning:** at Harvesting level 2, learn Cleaving Force. Swings now take sections sooner: about four swings for five sections. The yield per section stays the same.
4. **Shared state:** switch between axe and pickaxe; Cleaving Force stays learned for both.
5. **Choice:** at level 5, learn Shatter. Weak-point hits now take about three sections instead of two. Refund it and learn Follow Through: after a weak-point hit, the next swing on the same vein within 3 s takes more.
6. **Q/E/R:** the tools have no Q/E/R powers anymore. Rift Grip, Death Wave and Grave Swarm are gone.
7. **Multiplayer:** in a listen-server session with two players, the client learns Cleaving Force. Only the client's swings speed up; the host's tree and swings stay unchanged.

## Limits

- **No active magical harvesting power** until MST-06, by the user's decision.
- **Region shortcuts** are not built yet. There is one region; shortcuts follow with the second.
- **Follow Through preview:** the preview does not show the follow-up bonus, because its state lives on the server.
- **Balance:** the XP and point curves are the project defaults. Reaching level 5 for a choice takes about 1,450 mastery XP, about 160 iron-vein sections. Production tuning remains authored content.
- **Placeholder art:** icons reuse existing skill icons, and the mastery uses the pickaxe category icon.
- **Not checked in PIE:** learning a talent and its effect on a real swing were checked by automated tests only. The test map starts at level 1 with 0 points, and the PIE tools cannot drive a full harvest without the refused Python tool.
- **Atomic choice switching** (swap with zero free points) is MST-04. For now a choice changes by refunding and learning again.
- **Leftover presentation content:** the cues and icons of the retired powers stay until MST-09d.

## Next

MST-04: atomic replacement of exclusive choices with owner feedback.
