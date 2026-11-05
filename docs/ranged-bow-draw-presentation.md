# Bow draw presentation: string pull and nocked arrow (playtest request)

[PR #377](https://github.com/Athurito/SurvivalRpg/pull/377). After RNG-06 the playtest asked for a real draw: the arrow was only shot. Now the bow's string follows the drawing hand back, and a nocked arrow lies on the bow and moves with the string. At the release the string snaps back, the nocked arrow disappears, and the flying arrow starts at its place and joins the aimed flight. Player and enemy bows share it, on every machine.

The string first ran from an actor Tick (#377 to #380). [Rebuild without Tick](#rebuild-without-tick-2026-11-05) replaced that with an AnimBP and draw messages; the sections below describe the current state.

## C++ boundary decision

- **Classification:** a reusable bow foundation in content (one bow actor base, one bow AnimBP, one bow skeleton), every concrete bow as data on it, small native presentation seams, and editor-only tooling.
- **Runtime truth:** unchanged. The shot's draw state stays in GAS: `URpgAbilitySystemComponent::IsRangedDrawingForPresentation`, the owner's prediction or the authority's replicated `State.Ranged.Drawing`. The projectile's collision path stays the server's aimed launch (PR #374).
- **Existing seams:** the equipment definitions' `ActorsToSpawn`, which spawn the bow actor; the hold layer's draw pose; the presentation read model above; the GameplayMessageRouter; `ARpgCombatProjectile`'s `VisualRoot`, which `ProjectileMovement` already interpolates for network smoothing.
- **Native, and why:**
  - **`IRpgProjectileLaunchPresenter`:** the projectile is native and its flight presentation starts in its native `BeginPlay`. The interface lets it ask the shooter's attached actors for the visual start without knowing bows; the bow implements it in Blueprint.
  - **`URpgWieldedItemAnimInstance`:** an AnimBP may not read another actor on an animation worker. This small base snapshots, on the game thread before each update, where a wielder's bone lies in a frame of the item's mesh and whether the wielder's shot shows as drawn. It decides nothing; the AnimBP owns every reaction. It knows no bow, so other wielded items can use it.
  - **`Rpg.Combat.Message.RangedDrawingChanged` (`FRpgRangedDrawingMessage`):** the ability system sends it on each machine when its presentation read model changes, from the owner's prediction and from the replicated tag's event, and again when an avatar initializes while it draws. Equipment presents draws from it without polling. Blueprint cannot reach the owner's prediction as an event.
  - **Editor tooling** (`URpgAnimationAssetTools::CreateSkeletonFromStaticMesh`, `AnimationAssetTools.rig_bow_mesh`, and `URpgBlueprintAssetTools` for function tooltips and purity, comment boxes and a component's parent socket): capability gaps of the 5.8 toolsets, editor only.
- **New native classes:** the interface `URpgProjectileLaunchPresenter` and the animation base `URpgWieldedItemAnimInstance`.
- **Content:** see [Bow foundation](#bow-foundation). A concrete bow adds no graph logic.
- **Asset work:** editor Python and Unreal MCP (GeometryScript, `SkeletonModifier`, `SubobjectDataSubsystem`, `BlueprintTools.write_graph_dsl`, the project's `AnimationAssetTools` and `AssetContractTools`).

## Bow foundation

Every bow is content on one shared foundation, the way Lyra builds its weapons on `B_Weapon`:

| Lyra (ShooterCore) | SurvivalRpg bow | Owns |
| --- | --- | --- |
| `B_Weapon`, the weapon actor base | `Equipment/Bow/BP_BowActor`, abstract | `BowMesh` (root) and `NockedArrow`, the draw presentation and hand-off graphs, default tuning |
| `B_Pistol`, a configured child | `Equipment/Bow/SkeletonArmy/BP_Bow_SkeletonArmy`, data only | its rigged mesh, its nocked arrow mesh and transform, `ArrowTipOffset` |
| `ABP_Weap_Pistol`, the weapon mesh's AnimBP | `Equipment/Bow/ABP_Bow` on `SKEL_Bow`, shared by every bow | pulling `Bow_String` from the drawing hand, with the bow's own tuning |
| `WID_Pistol`, spawning `B_Pistol` at `weapon_r` | `ED_Bow` and `ED_EnemyBow`, spawning `BP_Bow_SkeletonArmy` at `Grip_Bow_L` | grip socket and grip transform |
| `B_WeaponInstance_Pistol` and its `PickBestAnimLayer` | `BP_WeaponInstance_Bow` (`URpgRangedWeaponInstance`) and the equipment's hold layer `ABP_Hold_Bow_Manny`, resolved per skeleton | the character's bow poses |

- **One skeleton, one AnimBP:** Lyra gives each weapon mesh its own AnimBP. Bows share the skeleton `SKEL_Bow` (root and `Bow_String`), so one AnimBP serves them all. Each bow mesh keeps its own reference pose, so `Bow_String` rests at that bow's string centre.
- **Per-bow data** lives on the bow child's class defaults. `ABP_Bow` reads the string values once; the actor's graphs read the rest.

| Variable | Unit | Base default | Meaning |
| --- | --- | --- | --- |
| `MaxStringPull` | cm | 75 | farthest the string's centre follows the hand behind its rest |
| `MaxHandOffset` | cm | 30 | how far across the draw axis the hand may sit and still pull |
| `StringPullSpeed` | 1/s | 25 | how fast the string eases toward the hand and snaps back |
| `StringSettleSeconds` | s | 0.3 | how long the string keeps animating after a draw ends |
| `ArrowTipOffset` | arrow mesh units | 0, set per bow | from the nocked arrow mesh's pivot (the nock) to its tip along its +Z |
| `HandOffWindowSeconds` | s | 0.5 | how long a released arrow waits for its projectile |
| `HandOffProjectileClass` | class | `BP_Projectile_Arrow` | the projectiles that take the nocked arrow |

- **Component overrides per bow:** `BowMesh`'s skeletal mesh, and `NockedArrow`'s static mesh and transform on the `Bow_String` socket.

### Add a bow

1. **Mesh:** migrate the static bow mesh as [asset library migration](../.agents/skills/unreal-lyra-expert/references/asset-library-migration.md) describes, with its [third-party notice](third-party-notices.md).
2. **Rig** with `AnimationAssetTools.rig_bow_mesh` on `SKEL_Bow` and `Bow_String`:
   - `string_start_json` and `string_end_json`: the string's two ends at the limb tips, in mesh space (cm).
   - `draw_direction_json`: from the bow toward the archer, in mesh space.
   - `string_radius`: large enough for every string vertex and small enough to leave the limbs out. Compare the returned `string_vertices` with the string's vertex count, and look at the skin weights in the skeletal mesh editor.
   - Save the new skeletal mesh, and the materials its first skeletal use marks dirty.
3. **Bow actor:** create a Blueprint child of `BP_BowActor`. In it set `BowMesh`'s skeletal mesh, `NockedArrow`'s arrow mesh with the rotation and scale that lay it from the string toward the grip, and `ArrowTipOffset`. Change the other values above only where this bow differs.
4. **Equipment:** an equipment definition whose instance is a `URpgRangedWeaponInstance` of `Weapon.Family.Bow`, spawning the child at `Grip_Bow_L` with this bow's grip transform, and an item definition that equips it.
5. **Check:** `SurvivalRpg.Combat.Ranged.BowFamilyContract` finds the new equipment in the asset registry and checks its actor, mesh, skeleton, AnimBP and tuning. Then draw it in PIE, also as a listen-server client.

## Behavior

### Rig

- **Asset choice:** the asset library's archer pack has a skeletal demo bow with a `Bow_String` bone, but it is untextured. The SkeletonArmy bow keeps its look and is rigged in the project instead:
  - `SKEL_Bow` has the root bone and `Bow_String`. On `SK_Skeleton_Archer_Bow`, `Bow_String` rests at the string's centre, (0, -2.27, 27.53) cm in mesh space, without rotation, so its Z points toward the archer.
  - `SK_Skeleton_Archer_Bow` is built from `SM_Skeleton_Archer_Bow`. `rig_bow_mesh` rebuilds it exactly from the string ends (-73.5, -2.27, 27.53) and (73.5, -2.27, 27.53), the draw direction (0, 0, 1) and a radius of 0.6 cm.
- **String weights:** the string is a flat strip between the limb tips at x ±73 cm. Its 94 vertices follow `Bow_String` with a weight from 1 at the centre to 0 at the tips, so a pull bends it into a V. The limbs stay rigid.
- **Material:** `M_SK_Archer_Bow` carries `Used with Skeletal Mesh`.

### String: `ABP_Bow`

- **Parent:** `URpgWieldedItemAnimInstance` with `WielderBone` `hand_r` and `LocalFrameBone` `Bow_String`. Its snapshot gives the drawing hand's location in the string's rest frame, so Z is the hand's depth behind the string.
- **Tuning:** `ReadBowTuning`, called once from *Blueprint Initialize Animation*, copies `MaxStringPull`, `MaxHandOffset` and `StringPullSpeed` from the bow actor. A mesh without a bow actor, such as the editor preview, keeps the AnimBP's own defaults.
- **Thread-safe update:** `BlueprintThreadSafeUpdateAnimation` calls `UpdateStringOffset`, which eases `StringOffset` toward the pure `GetStringPullTarget` at `StringPullSpeed`, so the string follows the hand and snaps back within about a tenth of a second at the release.
- **Pull target:** while the wielder draws and the hand lies within `MaxHandOffset` of the centre line, the hand's depth from 0 to `MaxStringPull`; otherwise, such as while the arms still blend into the draw, 0.
- **AnimGraph:** the mesh-space reference pose, a *Transform (Modify) Bone* that adds `StringOffset` to `Bow_String` in its own frame (fast-path binding), and the output pose.

### `BP_BowActor`

- **Components:**
  - `BowMesh` is the root: a `SkeletalMeshComponent` with `ABP_Bow` and no collision. The equipment's attach transform places it, as it placed the former empty root, so the grip offsets of #373 hold. It ticks in `TG_PostUpdateWork`, after the wielder's pose of the frame; its pose ticks only while rendered; it never ticks on a dedicated server; and it starts without tick and without auto activation.
  - `NockedArrow` is attached to the `Bow_String` socket of `BowMesh`, so it rides the string without code. It is hidden by default and has no collision.
- **No Tick.** The actor has no tick. `BeginPlay` listens for `Rpg.Combat.Message.RangedDrawingChanged` and catches up once with a draw already in progress (`SyncWithWielderDraw`). `HandleDrawMessage` passes a message about its owner to `PresentWielderDraw`:
  - **`NockArrow`:** shows the arrow and turns on the tick of `BowMesh`, so the string animates.
  - **`ReleaseArrow`:** keeps the tip of an arrow that no projectile took for `HandOffWindowSeconds` and hides it, then stops the tick of `BowMesh` after `StringSettleSeconds`, when the string is back at rest.
- **Draw state source:** the owner's prediction on its own machine, the replicated tag elsewhere, through the message. Simulated proxies and late joiners show the same from the replicated tag and their local hand pose; nothing new replicates.
- **Hand-off (`HandOffProjectileLaunch`):** only a projectile of `HandOffProjectileClass` (`BP_Projectile_Arrow` and its children) takes an arrow; another projectile of the same shooter, such as a spell, starts where it was launched. The bow returns the arrow tip's world location (`GetArrowTip`):
  - **Current draw:** a projectile launched on this machine as authority (host, server, bow enemy) takes the drawn arrow (`TakeNockedArrow`), which stays hidden until the next draw.
  - **Released arrow:** a projectile that a client receives after the draw ended takes the kept arrow (`TakeReleasedArrow`): the shooting client's own shot arrives about a round trip after its predicted release, often while the next draw already shows its arrow.
  - **Observers:** a projectile that reaches an observer before the replicated release takes the drawn arrow of that shooter (`IsWielderObserved`).
- **Idle:** without a draw the bow does no per-frame work: the actor has no tick, and the tick of `BowMesh` is off between draws.

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

## Rebuild without Tick (2026-11-05)

The playtest review of `BP_BowActor` found its functions hard to read, its graphs tangled, and the string driven from an actor Tick. This rebuild keeps the visible behavior and changes how it is built.

### Old and new

| | #377 to #380 | Now |
| --- | --- | --- |
| String bone | `PoseableMeshComponent`, set by `UpdateString` from the actor Tick in `TG_PostUpdateWork` | `SkeletalMeshComponent` with `ABP_Bow`: thread-safe update, *Transform (Modify) Bone* |
| Hand input | `GetDrawPull` read the shooter's `hand_r` socket on the game thread inside the Tick | `URpgWieldedItemAnimInstance` snapshots it on the game thread before the anim update |
| Draw start and end | `TrackDrawState` compared the draw state with the previous frame | `Rpg.Combat.Message.RangedDrawingChanged` from the ability system |
| Nocked arrow | `SetRelativeLocation` each frame | attached to the `Bow_String` socket |
| Idle cost | an idle skip inside the Tick | no actor Tick; the mesh ticks only from a draw until 0.3 s after it, and its pose only while rendered |
| Graphs | 7 functions, two of them long single expressions, no comments | 13 small functions with tooltips; getters are pure; arranged and commented |

### Why this design

- **AnimBP instead of a poseable mesh:** the bone belongs to the animation system, which already orders evaluation, renders the pose with the attached arrow in the same frame and skips meshes that are not rendered.
- **Game-thread snapshot:** the hand is on another actor, which a worker-thread update may not read. Property Access cannot reach it either: its paths call only parameterless getters, and the hand needs the wielder's socket transform in the bow's frame. The native base keeps that read on the game thread and leaves every decision to the AnimBP.
- **A message, not an ability-system delegate or a tag wait:**
  - The owner shows its predicted draw before the replicated tag arrives, so a tag wait would show the arrow a round trip late on the shooting client.
  - A replicated bow can begin play before its owner's ability system initializes on a late joiner, so a delegate bound in `BeginPlay` could miss every draw. The message channel needs no ability system to listen, and the ability system repeats the current draw when its avatar initializes.
- **Not a looping GameplayCue:** the release has no prediction window on the owner, so a predicted cue would end a round trip late there.

### Findings

- **Replicated tags skip `OnTagUpdated` on clients:** UE 5.8 replicates `GameplayTagCountContainer` itself, and on clients `FGameplayTagCountContainer::NetDeltaSerialize` updates the counts and fires the `RegisterGameplayTagEvent` delegates without calling `UAbilitySystemComponent::OnTagUpdated`. A first version that sent the message from `OnTagUpdated` showed observers the string pull but never the nocked arrow; the tag event fixed it. `RangedAimPIE.RemoteAimDrawAndReleaseShowEverywhereAndCapMovement` now checks both clients' messages.
- **Auto activation turns ticks on:** registering a component with `bAutoActivate` calls `Activate`, which enables its tick whatever `bStartWithTickEnabled` says. `BowMesh` turns auto activation off.

### Validation

- **Build:** `ue.py build` passed (0 errors, 0 warnings).
- **Native tests:** `ue.py test SurvivalRpg.Combat.Ranged SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Animation SurvivalRpg.AI SurvivalRpg.Blueprint --null-rhi` passed 67 / 67.
  - New: `Animation.WieldedItem.SnapshotsTheWielder` checks the snapshot: the wielder bone in the item's string frame, whatever the wielder's world transform, the draw state from the wielder's ability system, and nothing for a missing bone or a detached item.
  - New: `ProjectileShot.PresentationTagsFollowTheShot` also checks that a draw, a cancel and a release each send one message for the shooter's pawn.
  - New: `Blueprint.AssetTools.AuthorsReadableGraphsAndSockets` covers the editor helpers.
  - `Ranged.BowAssetContract` and `EnemyBowAssetContract` now check that the string mesh is a skeletal mesh whose AnimBP derives from the wielded item base, follows a bone of the Manny skeleton from the string's frame, and ticks after the wielder's animation.
- **PIE network tests:** `ue.py test SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatImpactCuesPIE` passed 14 / 14, with the suites' usual `LogNetPackageMap` warnings. `RangedAimPIE` now also checks that the observer and the shooting client each receive the draw and its end once.
- **Python tool tests:** AgentSkills 60, GaspImportAudit 17, GaspPacketGap 19 and Unreal 32 passed; `sync.py --check` and `skill_evals.py check` passed.
- **PIE probes:** a per-frame editor script sampled, in every PIE world, the shooter's drawing state, the string's pull against the hand's depth, the nocked arrow's visibility and its distance from the string bone, the bow mesh's tick and each arrow projectile's visual offset from its collision.
  - **Standalone and listen server, host shoots, client watches (`Lvl_RpgGaspCMC`):** in the second half of each draw the pull matched the clamped hand depth within 0.0 cm on both machines, up to 75 cm; the arrow sat 0.0 cm from the string bone, showed in every drawn frame and in no other; after the release the string was back at rest and the mesh tick off within 0.3 s.
  - **Client shoots three fast shots with `Net PktLag=100`:** every draw kept its arrow on both machines. With the catch-up time raised to 0.5 s for the run, the host's projectiles started 61 to 69 cm from their collision at the drawn arrow; the shooting client's own projectiles started 260 to 385 cm from it at the released arrow once the arrival limit was raised too (see Limits).
  - **Bow enemy (`Lvl_CombatSandbox`, listen server):** 6 to 7 draws per run; on server and client the string followed the hand and the arrow showed in every drawn frame. The server's projectiles all took the drawn arrow; the client's took it in 4 of 6, the rest hit at once or arrived beyond the limit.
  - The temporary projectile values were reloaded from disk afterwards; nothing of them was saved.
- **Graph screenshots:** every `BP_BowActor` function and string AnimBP graph, captured from the editor (`Saved/BowDraw/GraphShots` in the worktree that produced them).

### Limits

- **Shooting client with low frame rates:** a client receives its own projectile where the server last sent it. Below about 15 frames per second that point can lie beyond `VisualHandOffMaxDistance` (450 cm), and the projectile then appears on the aim line without taking the released arrow. This is unchanged projectile behavior; the bow's own released-arrow path was verified with the limit raised.
- **Late join mid-draw:** covered by `SyncWithWielderDraw` and the message the ability system repeats at avatar initialization, but not exercised in a late-join PIE run.
- **Hidden pins:** the unused Rotation and Scale pins of the *Transform (Modify) Bone* node are set to hidden but show until the node is refreshed in the editor; no tool refreshes a node.
- **Game captures:** the two-player PIE windows rendered at 640 × 482 with exhausted video memory, too small to judge the string; the checks rely on the per-frame probes below.

## Reusable bow foundation (2026-11-05)

The user asked for the rebuild to become the foundation of every later bow, following Lyra's weapon pattern: a new bow needs only content. [Bow foundation](#bow-foundation) describes the result and [Add a bow](#add-a-bow) the recipe.

### Old and new

| | Rebuild without Tick | Now |
| --- | --- | --- |
| Bow actor | `BP_BowActor`, a `StaticMeshActor` with an empty static mesh root, holding the SkeletonArmy meshes | `BP_BowActor`: an abstract `Actor` with `BowMesh` as root and no meshes; `BP_Bow_SkeletonArmy` is a data-only child with the meshes |
| String tuning | constants in the AnimBP | class defaults of the bow actor, which `ABP_Bow` reads once (`ReadBowTuning`) |
| Skeleton | `SKEL_Skeleton_Archer_Bow`, named after one bow | `SKEL_Bow`, shared by every bow; each mesh keeps its own reference pose |
| AnimBP | `ABP_BowString` | `ABP_Bow`, shared by every bow |
| Equipment | `ED_Bow` and `ED_EnemyBow` spawned `BP_BowActor` | both spawn `BP_Bow_SkeletonArmy`, with unchanged sockets and grip transforms |
| Rig | written by a one-off editor script | `AnimationAssetTools.rig_bow_mesh` |
| Tests | `BowAssetContract` and `EnemyBowAssetContract` read one Blueprint's own components | `BowFamilyContract` finds every bow equipment in the asset registry and reads each actor's class-default components, including a child's overrides |

### Why this shape

- **One skeleton and one AnimBP instead of Lyra's AnimBP per weapon mesh:** every bow animates the same single bone, so a bow-specific AnimBP would repeat the same graph. A shared skeleton keeps the bone name and hierarchy fixed, and per-mesh reference poses keep each string's own rest frame.
- **Tuning on the actor, not on a new data asset:** the bow actor already is the per-bow asset, as Lyra's `B_Pistol` is. A separate data asset would add a second place to configure the same bow.
- **`BowMesh` as root:** equipment spawning replaces the actor root's relative transform with the attach transform, so the former root's own rotation never showed in game. The bow mesh as root removes the empty component and keeps the in-game placement exactly; the probes below measured it.
- **Kept:** the hand-off, the draw message, `URpgWieldedItemAnimInstance` and `IRpgProjectileLaunchPresenter` stay as they were; the character side stays on the equipment's hold layer and `URpgRangedWeaponInstance`.

### Validation

- **Build:** `ue.py build` passed (0 errors, 0 warnings).
- **Native tests:** `ue.py test SurvivalRpg.Combat.Ranged SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.EquipmentAttachSocketContract SurvivalRpg.Animation SurvivalRpg.AI SurvivalRpg.Blueprint --null-rhi` passed 69 / 69.
  - New: `Ranged.BowFamilyContract` checked 2 bows: each spawns a spawnable child of `BP_BowActor`, whose bow mesh uses `SKEL_Bow` with `Bow_String`, runs the `ABP_Bow` family after the wielder's animation, shows a nocked arrow mesh and sets positive string tuning and `ArrowTipOffset`.
  - **Red check:** with `ED_EnemyBow` pointed at the abstract `BP_BowActor` in the editor's memory, the test failed at "ED_EnemyBow: its actor is a spawnable child of the shared bow actor"; the asset was reloaded from disk afterwards.
- **PIE network tests:** `ue.py test SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatImpactCuesPIE` passed 14 / 14 with the same warnings as before (`LogNetPackageMap`, and the arrow-body fixture's socket and voice-interface warnings).
- **Python tool tests:** AgentSkills 60, GaspImportAudit 17, GaspPacketGap 19 and Unreal 32 passed; `sync.py --check` and `skill_evals.py check` passed.
- **Rig tool:** `rig_bow_mesh` rebuilt the SkeletonArmy bow from its static mesh on `SKEL_Bow` (string ends (±73.5, -2.27, 27.53), draw direction (0, 0, 1), radius 0.6 cm), once from an editor script and once through the registered MCP toolset. Compared by vertex position with `SK_Skeleton_Archer_Bow`, all 1178 vertices and both bones matched, and all 94 string weights within 0.01. A second run with the string at y -2.03 and a tilted draw direction moved `Bow_String` in that mesh only, to (0, -2.03, 27.53) with an 11.3° roll, and left `SKEL_Bow` unchanged. The tool refused to overwrite an existing asset; the scratch meshes were deleted unsaved.
- **PIE probes (two-player listen server):**
  - **Host shoots, client watches (`Lvl_RpgGaspCMC`):** both machines spawned `BP_Bow_SkeletonArmy` with `BowMesh` as root at `Grip_Bow_L`, at exactly `ED_Bow`'s attach transform. The pull matched the clamped hand depth within 0.0 cm up to 75 cm, the arrow sat 0.0 cm from the string bone and showed only while drawn, the mesh tick was off after each release, and the actor tick was never enabled. With the catch-up raised to 0.5 s for the run, the host's projectiles started 62 to 63 cm from their collision at the drawn arrow and the watching client's 418 to 447 cm from it, as in the rebuild without Tick.
  - **Client shoots three fast shots with `Net PktLag=100`:** every draw showed its arrow in all 41 drawn frames on both machines. The server's projectiles started 63 to 65 cm from their collision; the shooting client's 342 to 419 cm from it at the released arrow, with the arrival limit raised as well.
  - **Bow enemy (`Lvl_CombatSandbox`):** `ED_EnemyBow` spawned the same child at its own attach transform. Nine draws pulled 62 to 67 cm with the arrow in every drawn frame, except one client frame in which an arriving projectile had already taken it; all 16 projectiles started at the bow.
  - The temporary projectile values were reloaded from disk; nothing of them was saved.
- **Graph screenshots:** every `BP_BowActor` and `ABP_Bow` graph and the class defaults of `BP_Bow_SkeletonArmy`, captured from the editor (`Saved/BowDraw/GraphShotsFoundation` in the worktree that produced them).

### Limits

- **New bows are untested:** only the SkeletonArmy bow exists, so the recipe and the per-mesh reference pose were checked with that bow and with a moved scratch rig, not with a second bow model.
- **Shared skeleton changes:** limb bones for flexing limbs would change `SKEL_Bow` for every bow; `SkeletonModifier` then opens a merge dialog, so add them deliberately.
- **Editor preview:** `ABP_Bow`'s preview and `BP_BowActor`'s viewport show the AnimBP's own defaults and no mesh; the bows' values show only on the bow children.
