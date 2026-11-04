# Bow draw presentation: string pull and nocked arrow (playtest request)

[PR #377](https://github.com/Athurito/SurvivalRpg/pull/377). After RNG-06 the playtest asked for a real draw: the arrow was only shot. Now the bow's string follows the drawing hand back, and a nocked arrow lies on the bow and moves with the string. At the release the string snaps back, the nocked arrow disappears, and the flying arrow starts at its place and joins the aimed flight. Player and enemy bows share it, on every machine.

## C++ boundary decision

- **Classification:** designer content for the bow (rigged mesh, string and arrow presentation in `BP_BowActor`), a small extension of the native projectile's presentation, and an editor-only tooling seam.
- **Runtime truth:** unchanged. The shot's draw state stays in GAS: `URpgAbilitySystemComponent::IsRangedDrawingForPresentation`, the owner's prediction or the authority's replicated `State.Ranged.Drawing`. The projectile's collision path stays the server's aimed launch (PR #374).
- **Existing seams:** `BP_BowActor`, spawned by both bows' equipment definitions on every machine; the hold layer's draw pose; the presentation read model above; `ARpgCombatProjectile`'s `VisualRoot`, which `ProjectileMovement` already interpolates for network smoothing.
- **Native, and why:**
  - **`IRpgProjectileLaunchPresenter`:** the projectile is native and its flight presentation starts in its native `BeginPlay`. The interface lets it ask the shooter's attached actors for the visual start without knowing bows; the bow implements it in Blueprint.
  - **`URpgAnimationAssetTools::CreateSkeletonFromStaticMesh`:** a capability gap. The engine's static-to-skeletal converter is reachable only from C++, and its factories crash when created from Python without their hidden inputs.
- **New native classes:** one interface, `URpgProjectileLaunchPresenter`.
- **Content:**
  - The string and arrow logic is Blueprint: `BP_BowActor`'s `GetDrawPull`, `UpdateString`, `TrackDrawState`, `HandOffProjectileLaunch` with `TakeNockedArrow` and `TakeReleasedArrow`, and `ConfigureStringTick`.
  - The rig is assets: `SK_Skeleton_Archer_Bow` and `SKEL_Skeleton_Archer_Bow`.
- **Asset work:** editor Python and Unreal MCP (GeometryScript, `SkeletonModifier`, `SubobjectDataSubsystem`, `BlueprintTools.write_graph_dsl`, `arrange_nodes`, variable tooltips).

## Behavior

### Rig

- **Asset choice:** the asset library's archer pack has a skeletal demo bow with a `Bow_String` bone, but it is untextured. The SkeletonArmy bow keeps its look and is rigged in the project instead:
  - `SKEL_Skeleton_Archer_Bow` has the root bone and `Bow_String` at the string's centre, (0, -2.27, 27.53) cm in mesh space.
  - `SK_Skeleton_Archer_Bow` is built from `SM_Skeleton_Archer_Bow`.
- **String weights:** the string is a flat strip between the limb tips at x ±73 cm. Its 94 vertices follow `Bow_String` with a weight from 1 at the centre to 0 at the tips, so a pull bends it into a V. The limbs stay rigid.
- **Material:** `M_SK_Archer_Bow` now carries `Used with Skeletal Mesh`.

### `BP_BowActor`

- **Components:**
  - The root static mesh is empty.
  - `BowMesh` is a `PoseableMeshComponent` with the rigged bow and the same transform as before, so the grip offsets of #373 hold.
  - `NockedArrow` uses the arrow mesh at 1.4 scale and lies along the draw axis toward the riser. It is hidden by default.
- **Each frame (`TG_PostUpdateWork`, after the characters' animation; `BowMesh` ticks right after the actor, so the string renders in the same frame as the nocked arrow):**
  - **Pull target:** while the shooter draws, the target is the drawing hand's (`DrawHandSocket`, `hand_r`) depth behind the string's rest along the bow's draw axis, from 0 to `MaxStringPull` (75 cm). The hand must lie within `MaxHandOffset` (30 cm) of the bow's centre line; otherwise, such as while the arms still blend into the draw, the target stays 0.
  - **String:** it eases toward the target (`StringPullSpeed` 25), so it follows the hand and snaps back within about a tenth of a second at the release.
  - **Nocked arrow:** its nock sits on the string, and it shows while drawing until a projectile takes it over.
- **Draw state source:** the owner's local prediction on its own machine, the replicated tag elsewhere. Simulated proxies and late joiners show the same from the replicated tag and their local hand pose; nothing new replicates.
- **Hand-off (`HandOffProjectileLaunch`):** only a projectile of `HandOffProjectileClass` (`BP_Projectile_Arrow` and its children) takes an arrow; another projectile of the same shooter, such as a spell, starts where it was launched. The bow returns the arrow tip's world location:
  - **Current draw:** a projectile launched on this machine as authority (host, server, bow enemy) takes the drawn arrow, which then stays hidden until the next draw.
  - **Released arrow:** a draw that ends without a hand-off keeps its arrow's tip for `HandOffWindowSeconds` (0.5 s) (`TrackDrawState`). A projectile that a client receives later takes that one: the shooting client's own shot arrives about a round trip after its predicted release, often while the next draw already shows its arrow.
  - **Observers:** a projectile that reaches an observer before the replicated release takes the drawn arrow of that simulated proxy.
- **Idle:** without a draw and with the string at rest, the bow skips the string and arrow updates. It never ticks on a dedicated server, which shows nothing and asks no presenter.

### Projectile

- **Visual start:** in `BeginPlay`, on every machine with visuals, a projectile no more than `VisualHandOffMaxDistance` (450 cm) from its launch origin asks the actors attached to its instigator for a visual start.
- **Catching up:** with one, it starts `VisualRoot`, which carries the mesh and trail, at that point and lets `ProjectileMovement`'s interpolation close the gap, as it does for server corrections. This takes about 0.1 to 0.2 s at full draw.
- **Unchanged:** the collision and the replicated flight stay on the aimed path from PR #374, and a projectile without a presenter, such as the fireball, is unchanged.

## Validation (2026-11-03)

- **Build:** `ue.py build` passed (0 errors, 0 warnings).
- **Native tests:** `ue.py test SurvivalRpg.Combat.Ranged SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Animation SurvivalRpg.AI --null-rhi`: 63 / 63 passed.
  - New: `Projectile.VisualLaunchHandOff`. A bow presenter attached to the shooter takes over once, from this projectile. The collision stays at the launch, the visuals start at the nocked arrow and join the flight within 30 frames. An arrow shown beyond the limit, or no arrow, leaves the visuals on the flight.
  - New: `Animation.CreateSkeletonFromStaticMesh.PlacesOneRootBoneInTheBounds`. It refuses a missing mesh, a package outside the project and an existing package, and places one root at the bottom centre of the engine cube.
  - `Ranged.BowAssetContract` now also checks that the bow actor presents launches and that its mesh has the `Bow_String` bone.
- **Python tool tests:** the CI's suites passed: AgentSkills 60, GaspImportAudit 17, GaspPacketGap 19 and Unreal 32. `skill_evals.py check` and `sync.py --check` passed.
- **PIE network tests:** `ue.py test SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatImpactCuesPIE` (listen server, two clients, `CommandTimeout.Network=90`): 13 / 13 passed, with the suites' usual warnings.
- **PIE checks on `Lvl_CombatSandbox`:** captures of the editor and game windows only.
  - **Idle:** the bow is lowered with a straight string and no arrow.
  - **Full draw:** the string pull was 68 cm and the arrow visible; the string forms a V to the drawing hand, and the arrow lies from the hand to the riser.
  - **30 ms after the release:** the string is straight again (11 cm left and easing), the nocked arrow is gone, and the trail starts at the riser. The projectile's visuals were 12, -32 and -26 cm (x, y, z) from its collision, still catching up.
  - **Listen server, client's view of the host:** the host's bow shows the drawn string and the arrow as a simulated proxy; after the release the string snaps back and the trail starts at the bow.
  - **Bow enemy:** drawing at the player, its bow pulled the string (57 to 63 cm) with the arrow shown.

## Playtest

1. `Lvl_CombatSandbox`: grant backpack, bow and arrows (`Rpg.Inventory.GrantItem … equip`).
2. Hold the right mouse button, then hold the left button to draw. **Expected:** the string comes back with the right hand, and an arrow lies on the bow from the string to the grip and moves with the hand.
3. Release. **Expected:** the string snaps straight, the arrow on the bow disappears, and the flying arrow leaves from the bow and settles onto the crosshair line within a few metres.
4. Lower the bow without shooting (release the right button during the draw or dodge). **Expected:** the string returns and the arrow disappears; the next draw shows a new one.
5. Let the skeleton archer draw at you. **Expected:** its string comes back and an arrow lies on its bow; its shot leaves from the bow.
6. **Multiplayer:** on a listen server, player B watches player A draw and shoot from the side. **Expected:** B sees A's string pull back with A's hand and the nocked arrow, and after the release the string snaps back and the arrow flies from A's bow.
7. **Fast shots as the client:** on the client of a listen server with some lag (for example `Net PktLag=100` in both windows), keep the right button held and shoot several times in quick succession. **Expected:** every draw shows its nocked arrow until its own release, and every flying arrow leaves from the bow.

## Limits

- **String depth follows the hand, not the draw strength:** the string follows the hand pose, which the hold layer blends to full draw in 0.35 s. It shows a full pull before the shot reaches full strength at 1 s.
- **Rigid limbs:** the string bends into a V, but the limbs do not flex.
- **Hand-off windows:** a client receives the projectile where the server last sent it, about 140 cm into its flight at full draw, whatever the latency; only a late joiner or a lost update exceeds the 450 cm limit. Latency instead delays the arrival: on the shooting client no arrow shows between the release and the projectile's arrival, about one round trip, and beyond 0.5 s the projectile appears on the aim line without a hand-off.
- **Visual start not traced:** the visuals start at the bow and catch up with the aim line without a collision check, so a bow tip behind a wall corner can show the arrow crossing that corner for about 0.1 s. The collision is unaffected.
- **Orientation at the hand-off:** the flying arrow starts with its tip at the nocked tip but along the flight direction, not along the bow's draw axis; its tail sits a few centimetres off for that frame.
- **Collision runs ahead of the visuals:** for about the first tenth of a second the visual arrow trails the collision path by up to the hand-off gap. A hit within the first few metres can therefore show the arrow arriving slightly beside the hit point.
- **No graph comments:** the 5.8 `BlueprintTools` cannot add comment boxes. Function, variable and tooltip names carry the explanation (see the Blueprint graph pitfalls reference).

## Review follow-up (2026-11-04)

PR #377 was merged without the independent review the master chat asks for. Two review agents, one for mechanism and replication and one for content and tests, read the diff, the exported `BP_BowActor` graphs and the engine sources. Each finding was checked against the code and in listen-server PIE probes (two players and the bow enemy in one process). The fixes are in [PR #379](https://github.com/Athurito/SurvivalRpg/pull/379).

### Confirmed and fixed

1. **The hand-off was not tied to a shot (medium):**
   - **Defect:** `HandOffProjectileLaunch` ignored its projectile and re-armed only when the next draw began.
   - **Reproduced** with `Net PktLag=100` and fast shots: on the shooting client, draws 2 to 4 lost their nocked arrow at about 68 cm pull, when the previous shot's projectile arrived. The last projectile then appeared on the aim line without a hand-off.
   - **Contract violation:** any projectile of the shooter took the arrow too, against the interface's own contract.
   - **Fix:** see "Hand-off" under Behavior.
2. **The string rendered a frame behind the nocked arrow (low):**
   - **Cause:** `BowMesh` applied the bone in its own `TG_PrePhysics` tick.
   - **Fix:** `ConfigureStringTick` moves it after the actor.
3. **Idle work on every machine (low):** every bow updated its string each frame, also without a draw and on a dedicated server.
4. **Test gaps (low):**
   - The enemy bow's presenter was never checked.
   - The interface check passed without the Blueprint's own hand-off.
   - The arrival-distance gate was untested.
   - The skeleton test left an asset registry entry behind.
5. **Documentation (low):** the latency limit above was wrong, and two skill-reference notes were wrong.

### Plausible, kept as limits

- The visual start is not traced against walls.
- The hand-off frame keeps the flight orientation.

Both are under Limits.

### Checked and found correct

- **Presentation only:**
  - Nothing new has authority or replicates.
  - The collision path, sweeps, launch sound and replicated movement are unchanged.
  - The teleport inside `BeginPlay` cannot overlap anything, because the projectile's actor collision is off.
- **Threads:** all new work runs on the game thread, after parallel animation evaluation.
- **Simulated proxies and late joiners:** they draw from the replicated tag and the local hand pose.
- **Grips from #373:** they survive, because the equipment definitions are unchanged and `BowMesh` sits at identity under the root that rendered the mesh before.
- **Third-party notices:** the derived skeletal mesh and skeleton are listed.
- **Start window:** a hand-off right after the world start is impossible, because `LastDrawnSeconds` starts at -100.
- **Empty quiver:** a draw without arrows is refused before any presentation.

### Validation of the follow-up

- **Build:** `ue.py build` passed (0 errors, 0 warnings).
- **Native tests:** `ue.py test SurvivalRpg.Combat.Ranged SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Animation SurvivalRpg.AI --null-rhi` passed 63 / 63.
  - **Red check:** on the #377 `BP_BowActor`, `BowAssetContract` and `EnemyBowAssetContract` fail at "the projectile its ammunition launches takes over the nocked arrow".
- **PIE network tests:** `ue.py test SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatImpactCuesPIE` (listen server, two clients, `CommandTimeout.Network=90`) passed 13 / 13, with the suites' usual `LogNetPackageMap` warnings.
- **PIE probes on `Lvl_CombatSandbox`:** per frame, in the server and the client world.
  - **Host shot, client shot, bow enemy:** the arrow hides in the frame its projectile starts at the bow, and no double arrow appears.
  - **Client fast fire with 100 ms lag:** every draw keeps its arrow, and all four projectiles start at their released arrow.
  - **Dev fireball while drawing:** the fireball takes no arrow, and the following arrow shot still does.
  - **String against the nocked arrow:** 0.0 cm in every run. With `BowMesh` back on `TG_PrePhysics` it was up to 74 cm in the frame the string jumps (17 fps PIE), with a mean of 4.5 cm.
- **Two-player listen server with 100 ms lag:** the client fires fast while the host watches from beside it. Time is frozen in the second draw, after the first projectile reached the client, and `HighResShot` captures the game viewports only.
  - **Client's own view:** the second draw shows its nocked arrow from the hand to past the grip, and the first arrow flies far ahead on the aim line.
  - **Host's view:** the client draws again, and the first arrow's trail starts at the client's bow.
