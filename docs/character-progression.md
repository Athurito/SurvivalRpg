# Character progression

The character level is general progression next to the trade-skill and weapon
masteries ([skill-trees.md](skill-trees.md)). Killing enemies gives
experience; every level grants skill points.

## Ownership

| Part | Owner | Notes |
| --- | --- | --- |
| Level, experience, unspent points | `URpgPlayerProgressionComponent` on the PlayerState | Server-authoritative, replicated to the owner only, saved in the host save. |
| Level curve, points per level, maximum level | `DA_PlayerProgression` (`URpgPlayerProgressionData`) | Static designer data. |
| Experience to the next level | `Curve_PlayerXPToNextLevel` (`UCurveFloat`), assigned as `XPToNextLevel` | Designer-owned curve next to the data asset. |
| Kill experience | `URpgGameFeatureAction_AwardKillXP` and the enemy's `URpgExperienceRewardComponent` | The enemy XP curve table row for the enemy level, otherwise the fallback reward (10). |
| Presentation | `URpgCharacterStatsViewModel` (inventory stats column, HUD) and `URpgSkillProgressionViewModel` (`CUI_Skills`) | Read-only. |

## Level curve

- The curve's time is the current level and its value the experience needed
  to reach the next level. `TryLevelUp` subtracts that value and raises the
  level as long as the experience covers it; leftover experience carries over.
- A level whose value is 0 or less stops levelling, so the curve needs a key
  for every level below `MaxLevel` (60).
- The curve follows `100 · L^1.5`, rounded to tens, with one linear key per
  level from 1 to 60. The user chose this shape on 2026-10-10.
  - If enemy rewards later grow with `10 · enemy level`, a level costs 10 kills
    at level 1 and about 77 at level 59.

| Level | To next level | Total from level 1 |
| --- | --- | --- |
| 1 | 100 | 0 |
| 2 | 280 | 100 |
| 5 | 1,120 | 1,700 |
| 10 | 3,160 | 11,100 |
| 20 | 8,940 | 67,140 |
| 30 | 16,430 | 189,040 |
| 45 | 30,190 | 528,380 |
| 59 | 45,320 | 1,046,990 |

- Key 60 (46,480) is not used while `MaxLevel` is 60; it lets a higher maximum
  level work without a zero step. At the maximum level the stats column shows
  "Max level".

## Retuning

- Edit the keys in the curve editor, or replace the whole curve with
  `AssetContractTools.import_float_curve` (see
  [unreal-mcp-asset-authoring.md](../.agents/skills/unreal-lyra-expert/references/unreal-mcp-asset-authoring.md#dataassets-and-itemdefinition-fragments)).
- Raising `MaxLevel` needs keys up to the new maximum.
- Saved characters keep their level and experience. Restoring a save does not
  level up; the next experience gain converts any surplus into levels, including
  experience collected while no curve was assigned.

## Validation (2026-10-10)

- `python Build/Tools/Unreal/ue.py build` passed after the documentation
  comments in `URpgPlayerProgressionData`.
- Through Unreal MCP: `import_float_curve` created the curve, `set_properties`
  assigned it, both packages were saved and freshly reloaded, and the T3D
  export showed 60 linear keys equal to the formula and the curve reference.
- PIE on `Lvl_ThirdPerson` as listen server with one client:
  - Killing the map's enemy with the client player's damage effect gave that
    player 10 experience through the kill reward; the host stayed at 0.
  - Server `AddXP` of 90 raised the client player to level 2 (0 / 280,
    1 point); 2,400 more raised it to level 5 (800 / 1,120, 4 points).
  - The client's replicated state and its stats view model matched the
    server. The inventory stats column on the client showed level 5, a bar at
    71 % and `800 / 1.120` (German number format).
- `python Build/Tools/Unreal/ue.py test SurvivalRpg.UI.CharacterStats --null-rhi`
  ran 3 tests, all passed.
