# Arrow aim and visibility (playtest fix)

[PR #374](https://github.com/Athurito/SurvivalRpg/pull/374). The playtest after RNG-06 reported two problems: a shot arrow had an offset and did not stay in the crosshair, and the arrow was too small to follow in flight. This fix keeps the arrow on the crosshair at every distance, makes it 1.4 times larger and gives it a subtle flight trail. It tunes neither the arrow's speed nor its gravity: the measured gravity drop and two tuning options are listed below for a decision.

## C++ boundary decision

- **Classification:** a change to the existing native shot mechanism (server launch geometry), plus designer content (meshes, a Niagara system).
- **Runtime truth:** the server's `RpgProjectileShot::ResolveGeometry` decides the launch point and direction of every release, for players and NPCs alike; the projectile's flight replicates from there.
- **Existing seams:** `RpgProjectileShot::ResolveGeometry` and `GetMuzzleLocation`, `URpgAIAttackDriverComponent::GetRangedAimPoint`, `BP_Projectile_Arrow`, `BP_EmbeddedPickup_Arrow`, and the projectile's stop presentation, which already deactivates every FX component when the flight ends.
- **Native, and why:** the launch geometry is server authority inside the existing release; the new helper `GetAimLineLaunchPoint` is shared by that geometry and the NPC's arc solver.
- **New native classes:** none.
- **Content:** mesh scale and offsets on the two arrow Blueprints, a `Trail` Niagara component on the projectile, and `NS_Arrow_Trail` in `GF_Combat_Ranged`.
- **Asset work:** authored through Unreal MCP (Niagara toolset, `SubobjectDataSubsystem`, `compile_blueprint`, `save_loaded_asset`).

## Behavior

### Problem

- **Geometry:** the arrow left the bow's muzzle, 60 cm in front of and 50 cm above the body, and flew toward the first hit of the camera ray, or toward the ray's end at 100 m when nothing was hit. The shoulder camera sits 75 cm to the right of the body, so the arrow crossed the screen from the bow toward the crosshair and met it only at that hit point. Against open sky, or with a target in front of a distant background, it passed up to 73 cm left of the crosshair (table below).
- **Visibility:** the 62.6 cm arrow mesh was small next to the 171 cm bow, and nothing marked its path.

### Change

- **Aim line:** the shot now leaves from the aim line, the camera ray, at the muzzle's depth along it, and flies along that ray. With the shoulder camera, that is about 2.8 m in front of the camera. The arrow stays on the crosshair at every distance; only gravity takes it below.
- **Fallback:** the aim-line start must lie within 150 cm of the body (`MaxAimLineLaunchOffset`), with nothing on the projectile channel between the body and that point. Otherwise the shot leaves from the muzzle as before, toward the camera ray's first hit. This keeps a shot from starting beyond a wall at the shoulder or from a camera far off to the side.
- **NPC archers:** they aim from their eyes, with no shoulder offset. `GetRangedAimPoint` now solves the ballistic arc from the aim-line launch point (`RpgProjectileShot::GetAimLineLaunchPoint`) instead of the muzzle. Without that, their arrows would land about 14 cm higher than aimed, the height between eyes and muzzle.
- **Arrow size:** `ArrowMesh` on `BP_Projectile_Arrow` and `DisplayMesh` on `BP_EmbeddedPickup_Arrow` are scaled 1.4, so the arrow is 87.6 cm long. Their relative X is now -86.8 cm, which keeps the tip at the actor origin. Corpse shafts copy the pickup template's relative transform, scale included, so flight, world pickups and corpse shafts stay identical.
- **Trail:** `NS_Arrow_Trail` is attached as `Trail` under `VisualRoot`, at the arrow's nock, and plays from launch. When the flight ends, the projectile deactivates it like any FX component, and the ribbon fades in the air.

### Content

| Asset | Change |
| --- | --- |
| `GF_Combat_Ranged/Projectiles/BP_Projectile_Arrow` | `ArrowMesh` scale 1.4 at (-86.8, 0, 0); new Niagara component `Trail` (`NS_Arrow_Trail`, auto activate) under `VisualRoot` at (-86.8, 0, 0) |
| `GF_Combat_Ranged/Projectiles/BP_EmbeddedPickup_Arrow` | `DisplayMesh` scale 1.4 at (-86.8, 0, 0) |
| `GF_Combat_Ranged/FX/NS_Arrow_Trail` | New. One CPU ribbon emitter, `Trail`, from the engine's `LocationBasedRibbon`: <br>- infinite loop, 200 points per second, 0.15 s life, so about 30 points and 9 m of trail behind a full-draw arrow;<br>- width 3 cm, narrowing to 0.9 cm with age;<br>- warm white (1, 0.95, 0.88), fading to 0 with age;<br>- `MI_FX_SoftGlow` (exposure-compensated additive), no shadows;<br>- `FXT_Gameplay_Looping` (Low quality halves the spawn count), fixed bounds of ±10 m. |

## Measurements

Probe in standalone PIE on `Lvl_CombatSandbox`:
- full draw, 6000 cm/s, gravity scale 1;
- shoulder camera `CM_Bow_Aim`;
- offsets of the arrow from the camera ray where it crosses the plane at the given distance from the camera, in the camera's right and up axes.

| Distance | Before, open sky at 8° pitch (right, up) | After, open sky at 8° pitch | After, wall at that distance (pitch 0) |
| --- | --- | --- | --- |
| 5 m | -73 cm, -57 cm | 0, -0.8 cm | — |
| 10 m | -69 cm, -60 cm | 0, -7.1 cm | 0, -7 to -9 cm |
| 20 m | -62 cm, -88 cm | 0, -40.4 cm | 0, -40.2 cm |
| 40 m | -46 cm, -225 cm | 0, -189.7 cm | 0, -187.2 cm |

- **Before:** walls hit by the old geometry were already exact sideways, because the arrow flew to the camera ray's hit; the gap opened against open sky and backgrounds.
- **After:** sideways the arrow stays on the crosshair everywhere. The remaining vertical offset is gravity drop, ½·g·t² over the 2.8 m shorter path from the launch point. One early 20 m wall shot measured -60 cm; repeats gave -40 cm, so that shot most likely had an incomplete draw.

### Gravity drop: tuning options (not applied)

| Option | Drop at 20 m | Drop at 40 m |
| --- | --- | --- |
| Current: 6000 cm/s at full draw, gravity scale 1 | 40 cm | 188 cm |
| Full-draw speed 8000 cm/s (`CF_Bow_LaunchSpeedByDraw` top key) | 23 cm | 106 cm |
| Gravity scale 0.5 on `BP_Projectile_Arrow` | 20 cm | 94 cm |

Either option keeps the arrow ballistic. The speed option also shortens the flight time, from 0.29 s to 0.22 s at 20 m. NPC archers solve their arc with the projectile's gravity and the full-draw speed, so they follow either change.

## Follow-up: flat near range (2026-11-04)

The user rejected half gravity: „halbe schwerkraft dropped viel zu krass am anfang“. The early drop they saw was not the full-draw ballistics.

### Findings

Measured in PIE on `Lvl_CombatSandbox`, as offset (right, up) from the crosshair ray where the arrow crosses the plane at that distance from the camera:

- **Partial draw that looks full:** the hold layer shows the full draw after about 0.35 s, but `CF_Bow_LaunchSpeedByDraw` rose linearly from 2000 to 6000 cm/s over the full second. A release at 0.4 s launched at 3589 cm/s and fell 20 cm at 10 m and 114 cm at 20 m. This was the strong drop "am Anfang".
- **Visual hand-off (#377, #379):** the visuals start at the nocked arrow, 40 to 50 cm below and left of the crosshair ray, and caught up over more than 10 m (still 20 cm low at 10 m at full draw). From the shooter's camera this reads as an early dip.
- **Full-draw ballistics:** 7 cm at 10 m and 40 cm at 20 m.

### Change

- **Speed by draw:** `CF_Bow_LaunchSpeedByDraw` now rises from 4000 cm/s to 6000 cm/s by draw fraction 0.4 and stays there. A draw that looks full is a full-speed shot, and a release at the 0.3 s nock flies at 5500 cm/s. Damage keeps its 8 to 30 curve over the full second. The enemy bow shares the curve and shoots at full draw, so it is unchanged.
- **Gravity delay:** the new `ARpgCombatProjectile::GravityDelayDistance` (cm, designer data, default 0) keeps gravity off until the projectile is that far from its replicated launch origin, then applies `GravityScale`.
  - Server and clients switch at the same distance, a late receiver at once.
  - `BP_Projectile_Arrow` uses 1000 cm.
  - `RpgProjectileShot::SolveLaunchDirection` takes the delay into account, so the bow enemy's arcs still meet their targets.
- **Hand-off catch-up:** the new `ARpgCombatProjectile::VisualHandOffCatchUpSeconds` (default 0.05 s, previously the movement's 0.1 s) is the visuals' interpolation time after a hand-off.

### Drop before and after

Offsets in cm (right, up) from the crosshair ray. Distances are measured from the camera; the arrow starts about 2.8 m in front of it, so its straight part ends about 12.8 m from the camera.

| Shot | Path | 5 m | 10 m | 15 m | 20 m | 40 m |
| --- | --- | --- | --- | --- | --- | --- |
| Before, full draw (6000 cm/s) | collision | 0, -0.6 | 0, -7.1 | 0, -20.3 | 0, -40.4 | 0, -189.5 |
| Before, full draw | visuals | -38.5, -33.2 | -15.4, -20.0 | -6.2, -25.5 | -2.5, -42.5 | -0.1, -189.5 |
| Before, release at 0.4 s (3589 cm/s) | collision | 0, -1.9 | 0, -19.9 | 0, -57.1 | 0, -113.6 | hit the floor first |
| Before, release at 0.4 s | visuals | -29.3, -26.5 | -6.2, -25.1 | -1.3, -58.2 | -0.3, -113.8 | — |
| After, full draw (6000 cm/s) | collision | 0, 0.0 | 0, 0.1 | 0, -0.6 | 0, -6.9 | 0, -100.3 |
| After, full draw | visuals | -24.8, -20.8 | -3.4, -2.8 | -0.5, -1.0 | -0.1, -6.9 | 0, -100.3 |
| After, release at 0.4 s (5955 cm/s) | collision | 0, 0.0 | 0, 0.0 | 0, -0.6 | 0, -6.7 | 0, -100.7 |
| After, release at 0.4 s | visuals | -24.3, -20.6 | -3.1, -2.7 | -0.4, -0.9 | 0, -6.7 | 0, -100.7 |

The tuning options above, 8000 cm/s or half gravity, are superseded.

### Validation (2026-11-04)

- **Build:** `ue.py build` passed (0 errors, 0 warnings).
- **Native tests:** `ue.py test SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.Ranged SurvivalRpg.AI --null-rhi`: 56 / 56 passed, with the final assets.
  - New `Projectile.GravityDelay`: a 500 cm delay keeps the flight level for 500 cm, then the class's gravity applies, and a projectile received past the delay falls at once.
  - `ProjectileShot.LaunchDirectionArcsOntoTheTarget` now also solves arcs with a 1000 cm delay, inside and beyond the straight part, within 1 cm and no higher than the plain arc.
- **PIE network tests:** `ue.py test SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatImpactCuesPIE` (listen server, two clients): 13 / 13 passed, with the suites' usual warnings. The bow enemy still hits the player with its delayed arcs.
- **Two-player listen server:** the host shot at 3° up while both worlds recorded the arrow. The client's arrow stayed within 0.4 cm of the server's over 115 m: level along its line for the first 10 m, then the same arc.
- **Capture:** the shooter's view 0.16 s after a full-draw release shows the arrow on the crosshair at about 10 m, with its trail coming from the bow. Editor window only.

### Limits

- **Visuals still start at the bow:** they are about 21 cm below and 25 cm left of the crosshair at 5 m and on it by about 10 m. That is the nocked arrow's place, not a drop.
- **Gravity onset:** it switches on the first frame past the delay, so up to one frame of straight flight more than 1000 cm; that is about 1 m at 60 fps and 6000 cm/s. The server's switch decides the hit; clients follow it within the measured 0.4 cm.
- **Not a ballistic arc:** the path is straight for 10 m, then curves. The onset is smooth in velocity, but the arc now starts later than a real arrow's.
- **Partial draws:** damage still needs the full second, while flight speed is full from 0.4. The strength bar keeps showing the damage build-up.
- **Curve import record:** `CF_Bow_LaunchSpeedByDraw` was rewritten through a CSV re-import, because its keys cannot be written from Python. It now records that temporary CSV as its import source; edit the keys in the curve editor, not by reimport.

## Validation (2026-11-03)

- **Build:** `ue.py build` passed (0 errors, 0 warnings), before and after the NPC aim change.
- **Native tests:** `ue.py test SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.Ranged SurvivalRpg.AI --null-rhi` passed 51 / 51 on the final build (44 / 44 without `SurvivalRpg.AI` and the new test before the NPC aim change).
  - `ProjectileShot.DrawCurvesSetDamageAndSpeed` now expects the launch on the aim line at the muzzle's depth.
  - The new `ProjectileShot.ShotLeavesFromTheAimLine` checks three cases: a shoulder camera launches on the aim line at the muzzle's depth and along the aim; a camera 300 cm to the side launches from the muzzle; a wall between the body and the aim line, clear of the muzzle, launches from the muzzle unobstructed.
  - `ProjectileShot.LaunchObstructionStopsTheShotAtTheWall` is unchanged and still passes: the wall blocks the way to the aim line as well, so the muzzle path stops the shot at the wall.
- **PIE network tests:** listen server with two clients, `--ue-arg=-ini:Engine:[ConsoleVariables]:TestFramework.CQTest.CommandTimeout.Network=90`.
  - Before the NPC aim change, `ue.py test SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatImpactCuesPIE` passed 12 / 12, with the suites' usual warnings.
  - On the final build, `ue.py test SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedAimPIE`: 11 / 11 passed, with the same warnings.
- **PIE probe:** the measurements above. Screenshots of the editor and game windows only: the shooter's view 0.1 s after the release shows the arrow on the crosshair; a listen-server client beside the flight line shows the larger arrow with its trail.
- **Two players:** a listen server with two clients in one process. The host shot along a line 7 to 20 m in front of the client. The client's copy of the arrow flew the same line (y 1075, the host's aim line), and its `Trail` component was active with `NS_Arrow_Trail`.

## Playtest

1. Start `Lvl_CombatSandbox`, equip the bow (`Rpg.Inventory.GrantItem` for backpack, bow and arrows), aim with the right mouse button and draw fully.
2. Shoot at the sky, at the ground about 20 m away and at an enemy: the arrow flies out from the crosshair and stays on it sideways. It only sinks: about 7 cm at 10 m, 40 cm at 20 m, 1.9 m at 40 m.
3. Watch the arrow: it is larger than before, and a thin, pale trail of a few metres follows it and fades when it hits.
4. Collect an embedded arrow and kill an enemy with arrows: the pickup and the corpse's shafts have the same larger size as the flying arrow.
5. **Multiplayer:** listen server with two players. Player A shoots past player B. B sees the arrow with its trail along A's line of fire, and the embedded arrow at the same place as A.

## Limits

- **Launch point for onlookers:** the arrow starts on the shooter's camera line, about 75 cm to the right of the bow. The shooter sees it leave from the crosshair. Other players see it appear beside the archer's right shoulder. Starting it at the bow and blending it onto the aim line during its first metres would need presentation work in the projectile.
- **Gravity drop:** unchanged. The tuning options above need a decision.
- **Fallback path:** near a wall at the right shoulder, or with a camera far to the side, the shot leaves from the muzzle and meets the crosshair only at the camera ray's hit, as before.
- **Trail look:** judged in the sandbox's daylight only.
- **No trajectory preview:** left out, as asked.
- **NPC aim:** the arc is solved from the aim-line point on the straight line to the target; the steeper launch direction tilts that point by a few centimetres.
