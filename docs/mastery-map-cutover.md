# MST-09d: Catalog acceptance and cutover

Task report for MST-09d in the [mastery map roadmap](mastery-map-roadmap.md). It closes the MST-09 umbrella of the [implementation plan](mastery-map-implementation-plan.md).

The plan's acceptance for MST-09:
- Every purchasable talent has implemented gameplay.
- All rune and branch combinations are consistent.
- Legacy harvest paths are retired.
- Combat routes survive.
- Packaged assets resolve.

## User decisions (2026-10-30)

The user asked to build MST-09b, MST-09c and MST-09d one after another, as three PRs. MST-09b ([PR #326](https://github.com/Athurito/SurvivalRpg/pull/326)) and MST-09c ([PR #327](https://github.com/Athurito/SurvivalRpg/pull/327)) are merged.

## C++ boundary decision

- **Classification:** acceptance tests, test-tag cleanup and content cleanup; no system changes.
- **Runtime truth:** unchanged. The catalog is the harvesting tree, the six rune items and the two swings, as MST-06 to MST-09c built them.
- **Native:** no runtime change and no new class.
  - **New tests:** `SurvivalRpg.Harvesting.Ability.AllRunesWorkTogether` and `SurvivalRpg.Harvesting.Content.CatalogContract`.
  - **Test cue tags:** the harvest cue and stride network tests no longer borrow the cue tags of the retired powers.
- **Assets (Unreal MCP and git), in `GF_Harvesting_Magic`:**
  - **`BP_HarvestWorker_Totenarbeiter`:** gains the strike presentation MST-09c lacked. Its `On Creature Struck` event spawns the existing, so far unused `BP_HarvestSwarmStrikeBurst` where the wisp struck. The event is cosmetic and runs on every machine with a local player.
  - **Removed:** `GCN_Harvest_AshWave` and `GCN_Harvest_StridingWave`, the cues of the retired Ash Wave and Striding Wave powers. No asset referenced them.
- **Tags:**
  - `GameplayCue.Harvesting.AshWave` becomes `GameplayCue.Harvesting.AreaHarvest`.
  - `GameplayCue.Harvesting.StridingWave` becomes `GameplayCue.Harvesting.Stride`.
  - Both are neutral cue tags of the native area harvest and stride mechanisms, without a notify yet; the network tests execute them.

## Legacy cleanup

MST-03b retired the old tool powers, their trees, abilities, cooldowns, the grave swarm Blueprint and the aim camera, and left their cues and icons for this task.

- **Retired now:** the Ash Wave and Striding Wave cue notifies and their cue tags.
- **Kept, with reasons:**
  - **Icons:** the old power icons serve the new talents.
  - **Rift Grip:** `GCN_Harvest_RiftGripStrike` and `NS_Harvest_RiftGrip_Strike` play in the VFX gallery's Rift Grip demo (`BP_VfxRiftGripDemo` in `Lvl_VfxGallery`). The harvest cue network test uses the cue as its target cue; its tag comment now says so.
  - **Ash Wave effects:** `BP_HarvestBurnArea` and the `NS_Harvest_AshWave_*` systems burn felled trees (`BP_HarvestFallingTree`) and play in the gallery's Ash Wave demo.
  - **Area marker:** `BP_HarvestAreaMarker` and `M_HarvestAreaRing` mark area previews of the targeting component.
  - **Swarm creature:** `BP_HarvestSwarmCreature_Grave` and `BP_HarvestSwarmStrikeBurst` present the Totenarbeiter workers.
  - **Native mechanisms:** area harvests, strides, swarms, chains, yield conversions and form cues stay, with their tests, as the seams later powers and runes use. MST-03b decided so; Totenarbeiter already reuses the swarm.

## Behavior

- **Problem:** the six runes and fifteen talents had been checked branch by branch, but nothing checked the whole catalog or all runes on one tool at once. Leftover cues of the retired powers still loaded, and the Totenarbeiter wisp struck without a visible hit.
- **Change:**
  - **The catalog is checked as a whole:** fifteen talents in five branches, each a one-point entry and two exclusive three-point choices. The 20 points buy one path through every branch.
  - **Every talent and rune rank tunes a value** the harvest ability reads.
  - **Every rune** drives a branch, is offered by the rune choice and forged at the Kiln.
  - **Every swing** carries all six rune effects.
  - **One tool with all six runes:** the burn with Wandering Effect, echo, transfer, leap, pierce and workers work together. Only the own hits count as hits, every follow-up belongs to one of them, every follow-up ends, and every completed section rewards once.
  - **Worker strikes:** a Totenarbeiter wisp now bursts briefly where it strikes.
  - **Retired cues:** the Ash Wave and Striding Wave cues are gone.
  - **Docs:**
    - The harvest tunings table lists every rune and talent tuning.
    - The skill tree overview describes the full harvesting tree.
    - The game vision describes the six runes as the magical side of harvesting.

## Validation

Run on 2026-10-30 in this worktree:

- `python Build/Tools/Unreal/ue.py build`: passed, 0 errors, 0 warnings.
- **New tests:**
  - **`SurvivalRpg.Harvesting.Ability.AllRunesWorkTogether`:** one node tunes a real swing like all six runes with their widening talents, on seven resources. The test runs two own hits, the second emptying its resource with overkill, then 12 s of world time.
    - No worker, strike or burn is left, one crew rose for both hits, and strikes and burns stay within their per-hit bounds.
    - Exactly the two hits are own hits, and every follow-up result carries the root of one of them.
    - The yield equals the completed sections exactly, and a resource out of every reach stays untouched.
  - **`SurvivalRpg.Harvesting.Content.CatalogContract`:**
    - Axe and pickaxe share the tree, which has fifteen talents in five branches of one entry and two choices.
    - The branch costs add up to the tree's 20 points.
    - Every talent and rune rank tunes a read value.
    - Each of the six runes drives a branch and is forged and offered.
    - Talents need only catalog runes, and both swings carry all six effects.
  - Both passed on their first run.
- FULL_RUN_PLACEHOLDER
- **Skill sync:** `python Build/Tools/AgentSkills/sync.py --check`: consistent. The asset authoring reference records that deleting a loaded GameplayCue notify leaves its file.
- **Failed attempt:** `AssetTools.delete` on the two cue notifies returned `true` and logged "Force Deleting", but the files stayed. They were removed with `git rm` while the editor was closed, after `get_referencers` had shown no referencers.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players:
  - **Inventory:** the host's pockets held the six runes.
  - **Skill map:** the shortcut Harvesting framed the whole tree, five branches of an entry above two choices. Combat, Harvesting, Impact, Aftermath, Rune Flow, Spread and Helpers had shortcuts.
  - **Not checked in PIE:** swings with socketed runes and the worker's strike burst. The burst graph compiled without warnings.

## Playtest

The whole catalog in one pass; each step's own playtest is linked in the [playtest backlog](mastery-map-roadmap.md#playtest-backlog).

1. **All runes:** in `Lvl_HarvestPickaxe`, Tab shows Brand, Echo, Kraftübertragung, Erntesprung, Durchschlag and Totenarbeiter, each with its own icon.
2. **Three sockets:** put the pickaxe into Weapon I and socket any three runes. Strike a vein among others; each rune shows its own effect:
   - Brand: glow.
   - Echo: blue burst every third hit.
   - Kraftübertragung: gold burst on the next vein when one empties.
   - Erntesprung: green bursts on two more veins.
   - Durchschlag: red burst on the vein behind.
   - Totenarbeiter: a wisp every fifth hit, now with a burst where it strikes.
3. **Mixed sockets:** swap runes and strike again; the talents of runes that left the tool sleep, and the others keep working.
4. **Skill map:** press H, choose Harvesting. The map shows five branches of three talents. Learning one choice of a branch locks the other until you switch.
5. **Multiplayer:** in a two-player listen server, the client's effects reward only the client, and the host sees all of them.

## Limits

- **Swings in the agent check:** the agent PIE checks of MST-09a to MST-09d did not swing with socketed runes; the native ability tests cover every rune, and all six together.
- **Retained legacy:** the Rift Grip and Ash Wave effects stay for the VFX gallery and the felled tree, and the native power mechanisms stay as seams. Retiring them is a separate decision.
- **Performance:** the synthetic map comparison is an editor automation measurement without rendering. A frame-time profile in a packaged build was not taken.
- **Controller:** gamepad navigation of the map is covered by the existing UI tests; a manual controller pass is part of the playtest backlog (MST-02, MST-03c).
- **Tuning:** all rune and talent values are first tuning in assets.

## Next

MST-09 is complete once this PR merges. The mastery map roadmap has no planned task left; its playtest backlog waits for the user's batch test.
