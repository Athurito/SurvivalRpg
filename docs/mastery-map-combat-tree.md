# MST-10b: Combat tree

Task report for MST-10b in the [mastery map roadmap](mastery-map-roadmap.md), [PR #312](https://github.com/Athurito/SurvivalRpg/pull/312); the design is in the [implementation plan](mastery-map-implementation-plan.md), section "Combat cluster (MST-10b)". It builds on [MST-10a](mastery-map-several-trees.md), which put several trees on the one map.

## User decisions (2026-10-27)

- **Second cluster:** Combat.
- **Talents** as proposed:
  - Kampfrhythmus (Battle Rhythm), +10 % damage, forks into the exclusive Konter (Riposte) and Bollwerk (Bulwark).
  - Riposte: a perfect block deals +50 % stagger damage and gives more stamina back.
  - Bulwark: blocking costs 25 % less stamina and stops 15 % more damage.
- **Points:** kills earn the combat mastery through the existing kill XP action.

### Interpretations in this PR

- **Battle Rhythm:** the proposal named `BaseDamage`. The player's `BaseDamage` is 0, and the damage execution adds it as a flat amount, so +10 % of it would do nothing.
  - Battle Rhythm instead multiplies the weapon attack's damage by 1.1 through the skill tree's tuning seam (`Ability.Tuning.Attack.Damage`).
  - The player sees the intended +10 % damage.
- **Riposte:** "more stamina back" is +50 % `PerfectBlockStaminaRestore`, matching the stagger bonus.
- **Kill XP:** every rewarded kill gives combat mastery XP at twice the kill's character XP. A level 1 basic enemy gives 10 XP, so 20 mastery XP; level 2 needs 100. All values are tunable in the assets.

## C++ boundary decision

- **Classification:** concrete content on existing mechanisms, plus three small native additions to existing seams. No new class, ability leaf or manager.
- **Runtime truth:**
  - `URpgTradeSkillProgressionComponent` owns combat mastery XP (server-authoritative, saved, owner-replicated).
  - `URpgSkillTreeComponent` owns learned combat nodes.
  - The equipment manager grants learned node effects while a melee weapon carrying the tree is in use. The damage execution and the block code read the changed attributes.
- **Native additions:**
  - `RpgTradeSkillGameplayTags::Skill_Mastery_Combat`, registered as a core trade skill so every player has a state for it.
  - `URpgGameFeatureAction_AwardKillXP::SkillXPRewards` (`FRpgKillSkillXPReward`: skill tag and XP scale).
    - The server adds scaled skill XP to the killer once per rewarded kill, next to the character XP.
    - Kills without character XP give none.
  - `RpgGameplayTags::Ability_Tuning_Attack_Damage`: `URpgGameplayAbility_BasicWeaponAttack` reads its damage through `GetTunedValue`. Learned nodes of the attacking weapon's tree can then change attack damage without code, the same seam the harvesting abilities use.
  - `URpgAbilitySet::GetGrantedGameplayEffects`, a read accessor for the content test.
- **Assets (Unreal MCP):**
  - `DA_SkillTree_Combat` (new, `GF_Combat_Core/Progression/SkillTrees`): tree tag `SkillTree.Tree.Combat`, points from `Skill.Mastery.Combat`, free-position map at `MapOrigin` (-900, 0) left of Harvesting, region Discipline. Nodes:
    - Battle Rhythm: 1 point; tuning Multiply 1.1 on `Ability.Tuning.Attack.Damage`.
    - Riposte: 3 points; `AS_Talent_Combat_Riposte` grants `GE_Talent_Combat_Riposte`, which multiplies `PerfectBlockStaggerDamage` and `PerfectBlockStaminaRestore` by 1.5.
    - Bulwark: 3 points; `AS_Talent_Combat_Bulwark` grants `GE_Talent_Combat_Bulwark`, which multiplies `BlockStaminaCost` by 0.75 and `BlockDamageReduction` by 1.15.
    - Riposte and Bulwark require Battle Rhythm and share the exclusive group Guard.
  - `GE_Talent_Combat_Riposte`, `GE_Talent_Combat_Bulwark` (new): infinite effects on `URpgDefenseSet`.
  - `AS_Talent_Combat_Riposte`, `AS_Talent_Combat_Bulwark` (new).
  - `ID_BasicSword` and `ID_BasicTwoHandedSword`: a `URpgInventoryFragment_SkillTree` pointing at the combat tree, active in the main hand.
  - `GF_Combat_Core`: scans `Progression/SkillTrees` for `RpgSkillTreeDefinition`, as `GF_Harvesting_Magic` does.
  - `GF_Progression`'s kill XP action: `SkillXPRewards` = combat mastery at scale 2.
  - `DA_TradeSkillData`: `Skill.Mastery.Combat` is named Combat, with the sword icon.
  - **Icons:** existing art: sword for Battle Rhythm, the heart for Riposte, the shield for Bulwark, and the greatsword for the tree. Dedicated talent icons are open.
- **Config:** `DefaultGameplayTags.ini` gains `SkillTree.Tree.Combat` and the three node tags.

## Behavior

### Problem

Only harvesting had a skill tree. Kills earned character XP but no mastery, and the melee weapons carried no tree, so the map of MST-10a had one cluster.

### Change

- **Combat mastery:** every kill that awards character XP also awards combat mastery XP to the killer. Each Combat level above the first earns a point.
- **Combat cluster:** the Skills map shows Combat left of Harvesting, with its own budget entry, label and points.
- **Talents:** while a sword or two-handed sword is the main-hand weapon:
  - **Battle Rhythm:** its attacks deal 10 % more damage.
  - **Riposte:** a perfect block deals 50 % more stagger damage and restores 50 % more stamina.
  - **Bulwark:** blocking costs 25 % less stamina and stops 15 % more damage, up to the full hit.
  - Riposte and Bulwark exclude each other.
- **Weapon switches:** both melee weapons carry the same tree, so switching between them keeps the learned talents. Harvesting tools do not carry it, so the combat talents rest while a tool is in hand.

## Validation

Run on 2026-10-27 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings (three builds: native seams, content test, test count fix).
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Progression SurvivalRpg.UI SurvivalRpg.Harvesting.Content SurvivalRpg.Save.WorldSave.MemoryRoundTrip SurvivalRpg.Combat --null-rhi`: **141 of 141 passed**. The 13 warnings are the usual net GUID and voice interface notes of the combat network tests and the existing `CUI_RespawnScreen` tick warning.
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.Network.KillXPCuePIE SurvivalRpg.Network.SkillTreePIE`: **2 of 2 passed**.
- `python Build/Tools/Unreal/ue.py cook Lvl_CombatSandbox`: passed, 0 errors, 3 known warnings (Steam disabled, no GameplayCueNotifyPaths, `CUI_RespawnScreen` tick). `DA_SkillTree_Combat`, both talent effects and both ability sets are in the cooked output.
- **New and changed tests:**
  - `SurvivalRpg.Progression.SkillTrees.Content.CombatTree` (new):
    - The combat tree has its tag and earns points from combat mastery, and both melee weapons carry it in the main hand.
    - Battle Rhythm is the entry and raises attack damage.
    - Riposte and Bulwark follow it and exclude each other.
    - Riposte's effect raises both perfect-block attributes; Bulwark's lowers the block's stamina cost and raises its damage reduction. Directions only; values stay tuning.
  - `SurvivalRpg.Network.KillXPCuePIE`: the configured kill XP action must reward combat mastery. On a dedicated server with two clients, only the killer's combat mastery advances.
  - `SurvivalRpg.Progression.TradeSkills.*`: the core skill count is now eight.
  - `SurvivalRpg.UI.SkillTreeMap.Content.ExistingTreesFit` now covers Combat with Harvesting: no overlap, every node visible when both are framed.
- **Agent PIE check:**
  - **Standalone, `Lvl_ThirdPerson` with its dev save** (sword in the main hand; `AbilitySystemInspectorToolset` on the player state):
    - `Rpg.Progression.AddSkillXP Skill.Mastery.Combat 1500` raised Combat to level 5 with 4 points. The map shows Combat left of Harvesting.
    - **Learning by click:** Battle Rhythm and Bulwark added `GE_Talent_Combat_Bulwark`, and `BlockStaminaCost` dropped from 20 to 15. `BlockDamageReduction` stayed at its limit of 1.
    - **Persistence:** after a PIE restart the learned nodes and the effect came back from the save.
    - **Exclusion:** with Bulwark learned, Riposte showed Excluded.
    - **Swap:** after refunding Bulwark the effect was gone and `BlockStaminaCost` was 20 again. Riposte then raised `PerfectBlockStaggerDamage` from 35 to 52.5 and `PerfectBlockStaminaRestore` from 15 to 22.5.
  - **Listen server with two players in one process:**
    - The client's map showed both clusters, and the client learned Battle Rhythm, while the host's tree stayed unchanged.
    - In a second session the host swapped Riposte for Bulwark: its `BlockStaminaCost` went to 15 while the client's stayed at 20, with no talent effect.
  - **Cleanup:** the host's combat tree was reset (level 5 with 4 free points stays in the dev save), and the play settings were restored to the listen server with two clients.
- **Failed attempts and fixes:**
  - **Effect magnitudes:** the effects first used deltas (-0.25, +0.15, +0.5) as `MultiplyAdditive` magnitudes. GAS reads those as factors, so `BlockStaminaCost` and `BlockDamageReduction` dropped to 0 in PIE. The magnitudes are factors now (0.75, 1.15, 1.5).
  - **Client without the tree:**
    - The client's map lacked Combat after adding the scan entry to `GF_Combat_Core` in the running editor.
    - Clients learn trees only from the asset manager's list; the server also registers the tree of the equipped weapon.
    - After an editor restart the client listed Combat.
  - **Combat sandbox:** in `Lvl_CombatSandbox` the enemies killed the player while the menu was open. The talent checks moved to `Lvl_ThirdPerson`.
  - **Icons:** the first choice reused Cleaving Force's axe for Battle Rhythm and a no-entry sign for Riposte; both were replaced.
  - **Small client windows:** inspector clicks into the map of a client window (about 640 units) stuck or hit the card, so the client's Riposte was not learned there. The host-side swap shows the separation instead.
- `python Build/Tools/AgentSkills/sync.py --check`: consistent.

## Playtest

1. **Kill XP:** in `Lvl_CombatSandbox`, kill an enemy with the sword; the soul harvest cue plays. Press H: on the Character tab, Combat's XP bar advanced by twice the character XP. Five level 1 basic kills earn the first point.
2. **Map:** the Skills map shows Combat left of Harvesting, each with its own budget entry and label.
3. **Battle Rhythm:** with a point, learn it. Sword hits deal a tenth more damage, which shows on enemies that need one hit more or less.
4. **Bulwark:** learn Bulwark and block an attack with the sword. The stamina bar loses less than before.
5. **Swap to Riposte:** refund Bulwark (Riposte is Excluded until then) and learn Riposte. A perfect block staggers the warrior sooner and gives back more stamina.
6. **Weapon switch:** switch to the two-handed sword. The talents stay. Switch to a pickaxe: they rest, and the harvesting talents apply.
7. **Multiplayer:** in a two-player listen server, each player's kills raise only their own Combat level, and talents one player learns change only their own blocking.

## Limits

- **Kill path in PIE:** kills were checked by the dedicated-server network test, not by a kill in PIE; without a god mode the sandbox enemies won the menu-open checks.
- **Damage bonus:** Battle Rhythm's +10 % was checked through the content test and the generic tuning test, not by measuring a hit in PIE.
- **Talents outside melee:** Battle Rhythm changes only `URpgGameplayAbility_BasicWeaponAttack` damage. Other damage sources, such as magic or rune echoes, ignore it.
- **Where talents apply:** the talents apply only while a melee weapon carrying the tree is in the main hand. A shield's own block still benefits, because the effects change the player's attributes.
- **Client confirmation:** one client learning a GE talent was not confirmed in PIE because of the small-window clicks. The host-side swap and the network tests cover the separation.
- **Icons:** placeholder art from existing icons.
- **Kill XP for everything:** every kill counts, including kills by harvesting tools or magic.
- **Tuning:** the XP scale (2) and the talent values are first numbers for playtesting.

## Next

MST-04: atomic replacement of exclusive choices (switching Riposte and Bulwark in one step even without free points), with pending and rejection feedback on the card.
