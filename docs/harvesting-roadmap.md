# Harvesting roadmap

This roadmap maps the harvesting design plan (version 0.1, 2026-10-03) onto the
existing `GF_Harvesting_Magic` feature. It records accepted decisions, the
runtime contract, and the task sequence. Product tracking stays in issue
[#26](https://github.com/Athurito/SurvivalRpg/issues/26); moving concrete
harvesting abilities into Blueprint content belongs to issue
[#86](https://github.com/Athurito/SurvivalRpg/issues/86).

## Design goals

- The player starts with a pickaxe and an axe. The tool then awakens into a
  focus for supernatural harvesting powers that save real work.
- A resource has a defined stock split into logical sections. Every harvest
  method extracts from the same stock. A valid hit on a resource with remaining
  stock yields material immediately. A miss or an empty resource is reported
  clearly.
- Normal harvesting has no durability or fuel cost. Large powers are limited by
  area, duration and short cooldowns.
- A few base materials stay relevant for the whole game. Rewards go straight to
  the inventory; a full inventory never destroys material.
- Companions are abilities, not workers: summon them, they harvest, they are gone.
- Players see what an ability will hit before they trigger it.

## Decisions

| Topic | Decision |
| --- | --- |
| Stoneburst | Replaced by Rift Grip (`GA_Harvest_RiftGrip`), the pickaxe's awakened multi-section ability. HARV-04 removed the native leaf, its tag and its PlayerState grant. |
| Manual harvest | Profiles with `RequiredToolTag` offer no interaction harvest. Bushes keep the interaction harvest. |
| First unlock | Rift Grip, Death Wave and Grave Swarm are first-point nodes of the tool skill trees (HARV-09b). Level 2 of the tool's trade skill earns that point; until HARV-09b the abilities had their own level gate. |
| Costs | Rift Grip uses only a short cooldown GameplayEffect on the character, with no mana. Re-equipping cannot reset it. |
| Resource representation | Prototype resources are actors with `URpgHarvestableComponent`. Open-world trees and rocks come from PCG; the PCG bridge in HARV-06 makes those instances harvestable. `URpgHarvestableInstancedMeshComponent` remains the legacy path for bushes and the sandbox. |
| Indicators | With a tool equipped, the primary-swing target is always highlighted. Special abilities show their area and every target while the input is held, and trigger on release. |
| Trees | Open-world trees are PCG instanced skinned meshes without collision. An invisible PCG trunk proxy at the same point holds the stock and is the harvest target (HARV-07). |
| Protection | Area powers skip resources inside a harvest protection box and report them as protected. A deliberate single-target swing still harvests them (HARV-07). |
| Area rewards | A multi-target harvest delivers the rewards of all its targets as one batch: into the inventory, or into one drop at the harvester (HARV-07). |
| Skill trees | Every tool carries a skill tree in the style of New World (HARV-09a, [skill-trees.md](skill-trees.md)). The tool's trade skill level earns the points. Learned active powers are placed on Q/E/R in the tree, upgrades change their values through tunings, and the tree can be reset for free. The system is core and data-driven, so combat weapons can use it later. |
| Swarm | Grave Swarm is the axe's second awakened power. Its creatures work through every resource in its area, one strike at a time, through the same `IRpgHarvestableTarget` path. The player's player state receives the rewards; the swarm is the physical harvester. A summoned swarm keeps working after a tool switch and ends when the player dies (HARV-08). |

The trees from #177 are Nanite-assembly skeletal meshes with dynamic wind.
HISM cannot render them, and the UE 5.8 foliage tool places only static meshes
and actors. Mass placement uses the PCG skinned mesh spawner, which writes
`UInstancedSkinnedMeshComponent` instances. This is why representation and
stock rules are kept separate.

## Mapping to the project

| Plan element | Project seam | Status |
| --- | --- | --- |
| Tool item → equipment → ability set → ability | `URpgInventoryFragment_EquippableItem`, `URpgEquipmentDefinition::SlotAbilitySetsToGrant`, `URpgAbilitySet`, `URpgGameplayAbility_FromEquipment` | Exists; the pickaxe chain is authored in HARV-03 |
| Harvestable component and definition | `URpgHarvestableComponent`, `URpgHarvestProfile` | HARV-01 |
| Harvest request and result | `FRpgHarvestRequest`, `FRpgHarvestResult`, `IRpgHarvestableTarget` | HARV-01 |
| Auto pickup and overflow container | `FRpgHarvestRewardService`: atomic inventory batch or one replicated drop | Exists; multi-section batching added in HARV-01 |
| Harvest input tags | `InputTag.Weapon.Primary` on the main-hand tool; Q/E/R defaults declared by ability sets | No new input tags; ability-set defaults for Q/E/R added in HARV-04 |
| Harvesting progression | `URpgTradeSkillProgressionComponent` (`Skill.Gathering.*`, saved); tool skill trees in `URpgSkillTreeComponent` | Skill levels exist; skill tree foundation in HARV-09a; skill UI and tool trees in HARV-09b |
| World persistence of resources | none (depletion is session-scoped) | HARV-10 |

## C++ boundary decision

- **Classification:** schema for profile sections and tool requirement,
  request/result, and targeting parameters and preview read model. Reusable
  mechanism for stock extraction, harvest ability execution and local target
  preview. Tools, abilities, resources, loot, presentation and tuning are
  designer content.
- **Runtime truth:** the server owns the remaining stock of each resource in
  `URpgHarvestableComponent`, or for instanced resources in the GameState's
  `URpgHarvestInstanceStockComponent`. The stock is replicated and
  session-scoped. Rewards go through `FRpgHarvestRewardService` into
  `URpgInventoryManagerComponent`. Unlocks read saved trade-skill levels.
  Ability specs come from equipment grants. The target preview is a local
  cosmetic read model; the server re-selects targets at commit time.
- **Native classes:**
  - `URpgHarvestableComponent` (HARV-01).
  - The abstract `URpgGameplayAbility_Harvest` (HARV-02), which owns server
    target selection, exactly-once commit, cancel safety, commit timing taken
    from authored montage data, hold-to-aim and, since HARV-09e, strides that
    harvest around the walking player.
  - The local `URpgHarvestTargetingComponent` (HARV-02).
  - The non-reflected `FRpgHarvestStockRules`, shared by actor nodes, HISM
    instances and the PCG bridge.
  - The abstract `URpgHarvestTargetIndicatorWidget` (HARV-04), which binds an
    indicator widget to exactly one previewed target.
  - HARV-04 removed the native `URpgGameplayAbility_Stoneburst` leaf.
  - `URpgHarvestableInstancesComponent` (HARV-06), an instanced static mesh
    target for PCG, and `URpgHarvestInstanceStockComponent` (HARV-06), the
    sparse replicated stock of all instanced resources on the GameState.
    Both are engine-facing mechanisms: authority, replication, instance
    identity and the instanced mesh API.
  - `URpgHarvestProtectionComponent` and its world registry
    `URpgHarvestProtectionSubsystem` (HARV-07). Every target evaluates the
    same boxes on the server and in previews.
  - The non-reflected `FRpgHarvestRewardBatch` (HARV-07), which merges the
    rewards of one multi-target commit into one delivery.
  - `ARpgHarvestSwarm` (HARV-08), a short-lived replicated swarm. It owns the
    reservations, the exactly-once commit of every strike, the work through its
    area and its lifecycle, and replicates the creature flights in server
    time. The non-reflected `FRpgHarvestSwarmPlanner` distributes the
    creatures for the swarm and the preview alike.
- **Designer assets:** item, equipment and ability set definitions, `GA_*`
  abilities, `HP_*` profiles, `LT_*` loot tables, resource actor Blueprints,
  instanced resource component Blueprints (`BPC_HarvestInstances_*`), PCG
  graphs, the falling-tree presentation, protection zone actors, swarm
  Blueprints and their creatures (`BP_HarvestSwarm_*`), montages,
  cues, cooldown effects, indicator widgets, tags and test maps.
  They are authored through Unreal MCP.

## Runtime contract (HARV-01)

- `IRpgHarvestableTarget` is the only extraction path. Interaction, abilities
  and future swarms all use it.
  - `GetHarvestRevision(Hit)` returns the revision an ability records in
    `FRpgHarvestRequest::ExpectedRevision`.
  - `EvaluateHarvest(Request)` is read-only. It is valid on the server and on
    owning clients, which use it for previews. It reports the outcome, the
    sections it would take, and the stock left afterwards.
  - `CommitHarvest(Request)` is authority-only. It re-evaluates, delivers the
    reward for all taken sections as one batch, and only then mutates the stock.
- Rejections are distinguishable: `Depleted`, `WrongTool`, `SkillGate`,
  `Stale`, `Invalid`, `DeliveryFailed` and, since HARV-07, `Protected`. A
  failed delivery leaves the stock, revision and XP untouched.
- Since HARV-08 the request separates the beneficiary from the physical
  harvester. `Harvester` receives the rewards and XP, and its skills are
  checked. `PhysicalHarvester`, when set, struck the resource, such as a
  swarm. Presentation that depends on where a strike came from uses
  `GetStrikingActor()`.
- `URpgHarvestProfile::SectionCount` (1–16) defines the stock. Every section
  rolls the loot table once and awards `SkillExperience` once, so every method
  yields the same total.
- `RequestedSections` is clamped to the remaining stock.
- The revision changes only on depletion and respawn. Concurrent harvesters
  therefore share a resource without rejecting each other, and the stock never
  goes negative.
- `RequiredToolTag` must be matched by the request's `ToolTag`; child tags
  match, parent tags do not.
- Instanced HISM resources keep a single section. A profile with more sections
  logs a warning and depletes on the first harvest there.

### Actor resource setup

- Add `URpgHarvestableComponent` to a replicated actor and assign a
  `URpgHarvestProfile`.
- Use `NetDormancy = DORM_Initial` for placed resources and `DORM_DormantAll`
  for spawned ones, with a low net update frequency of about 1–2 Hz. Untouched
  placed resources then send no state. The component flushes dormancy and
  forces a net update on every change.
- Present sections, depletion and respawn in the actor Blueprint through
  `OnHarvestStateChanged`. `bInitialState` is true for the state at BeginPlay
  and for changes older than 1.5 seconds when they reach a client, such as on
  late join. A late joiner therefore sees a felled tree already felled instead
  of falling again.
- `OnHarvested` is server-only telemetry. Neither event may grant loot.

## Harvest abilities and target preview (HARV-02)

- **Base class:** concrete harvest abilities are `GA_*` Blueprints derived from
  the abstract `URpgGameplayAbility_Harvest`. They configure:
  - `HarvestAbilityId` and `Targeting` (single target or area at the aim point,
    aim distance, aim radius, reach from the avatar, area radius, maximum
    targets, trace channel).
  - `SectionsPerTarget` and `HarvestPowerScale`.
  - An optional trade-skill unlock (`RequiredSkillTag`, `MinimumSkillLevel`).
  - `bAimWhileInputHeld`, the montage and its play rate, the commit event tag
    (or `CommitDelaySeconds` without a montage), and the success and no-yield
    cues.
  - Costs and cooldowns as usual for GAS.
- **Blueprint children:** they do not implement the ActivateAbility event.
  They use `On Harvest Resolved` and cues for feedback.
- **Tool context:** the tool category and harvest power come from the source
  equipment's item (`URpgInventoryFragment_HarvestingTool`). Input-bound tool
  abilities only activate while their equipment holds the matching hand role.
- **Commit timing:** the server commits exactly once, at the time of the single
  `RPG Gameplay Event` notify that sends the commit tag. Since HARV-04 it
  selects targets from the aim it captured when execution started: the press
  of a swing, or the release of a held aim. Turning the camera during the
  swing no longer changes what is hit; stock and reach are still evaluated at
  commit time. The time is resolved
  from the montage data, not from server-side notify delivery. Ending or
  cancelling the ability before that moment, for example by switching tools or
  by an interrupted montage, yields nothing.
- **Hold to aim:** the ability previews while its input is held, executes on
  release, and spends its cooldown only on execution.
- **Target preview:** `URpgHarvestTargetingComponent` is added to player
  controllers by the harvesting GameFeature (HARV-03 content).
  - On the local controller it re-evaluates at `UpdateRateHz` with the same
    query the server uses.
  - It previews either the held aim ability or the primary-input ability of
    the active main-hand tool.
  - `OnPreviewChanged` fires only on presentation-relevant changes. It carries
    the targets, the sections each would take, the stock left, the outcome,
    reach, the aim point and the area.
  - Presentation (highlight, section pips, state text, area decal) belongs in
    Blueprints and widgets, preferably on the existing indicator system
    (`URpgIndicatorManagerComponent`).

## Pickaxe content (HARV-03)

All assets live in `GF_Harvesting_Magic` and are authored through Unreal MCP.

- **Tool chain:**
  - `ID_Tool_Pickaxe` configures the native fragments `EquippableItem`
    (`ED_Tool_Pickaxe`), `HarvestingTool` (`Tool.Harvesting.Pickaxe`, power 1),
    `ItemTraits`, `UIData` and `SpatialItem`.
  - `ED_Tool_Pickaxe` allows the main hand only, spawns the placeholder
    `BP_Tool_PickaxeActor` at `hand_r` and grants `AS_Tool_Pickaxe` while the
    tool holds the main hand.
  - `AS_Tool_Pickaxe` binds `GA_Harvest_PickaxeStrike` to
    `InputTag.Weapon.Primary` with the id `Ability.Harvesting.PickaxeStrike`.
- **Pickaxe Strike:** `GA_Harvest_PickaxeStrike` takes one section per swing.
  It reaches 250 cm from the pawn and aims up to 900 cm from the camera. The
  longer aim distance only lets the indicator report targets that are out of
  reach; the commit still requires the 250 cm reach.
- **Swing animation:** `AM_Harvest_PickaxeSwing` commits at 0.3 s through one
  `RPG Gameplay Event` notify. Its sequence `MM_Harvest_PickaxeSwing` is a copy
  of the core unarmed `MM_Attack_01` with root motion disabled. With root
  motion, the swing pushed the pawn into the vein and later swings missed.
  UE 5.8 montages have no per-montage root-motion override. The feature does
  not depend on `GF_Combat_Core`.
- **Item category:** the pickaxe uses the item category `Weapon`. The carry
  weapon slots accept only that category, and the asset contract allows one
  category per item. Its tool identity comes from the `Tool.Harvesting.Pickaxe`
  item tag and the `HarvestingTool` fragment.
- **Pickup:** `BP_Pickup_Pickaxe` uses `GA_Harvest_CollectTool`, a
  `URpgGameplayAbility_Collect` Blueprint that assigns collected equippable
  items to equipment. The interact key is F.
- **Iron vein:**
  - `HP_IronVein` has 4 sections and requires the pickaxe, with Mining XP 9
    per section and a respawn after 120–180 s.
  - `LT_IronVein` yields 6 `ID_Ore` per section.
  - `BP_HarvestNode_StaticMeshBase` is the reusable static-mesh resource
    actor: dormant, no tick, and it shrinks the mesh per section through
    `OnHarvestStateChanged`.
  - `BP_HarvestNode_IronVein` assigns the profile and the rock mesh.
- **Target indicator:**
  - `URpgHarvestTargetingComponent` exposes `GetPrimaryTargetStatus` and
    anchors one projected indicator over the primary target through the
    controller's `URpgIndicatorManagerComponent`.
  - `ERpgHarvestTargetStatus` summarizes the target as harvestable, out of
    reach, depleted, wrong tool, skill locked or unavailable.
  - `BPC_HarvestTargeting` selects `WBP_HarvestTargetIndicator`. The widget
    shows the remaining stock (`3/4`) or the blocking state.
  - The GameFeature adds `BPC_HarvestTargeting` to client player controllers
    only.
- **Test map:** `Lvl_HarvestPickaxe` uses the `Lvl_RpgGaspMantle`
  presentation. It contains the pickup, three iron veins and one comparison
  vein.
- **Sandbox:** HARV-04 added the pickaxe requirement to `HP_MiningNode` and a
  pickaxe pickup to `Lvl_LootHarvestSandbox`.

## Rift Grip (HARV-04)

- **Ability:** `GA_Harvest_RiftGrip` derives from `URpgGameplayAbility_Harvest`.
  - The player holds Q to aim at an area and releases to harvest.
  - It takes 3 sections from up to 3 ore veins within 300 cm of the aim point.
    A vein counts as soon as its collision touches the area, even when it is
    only partly inside the ring (accepted in playtest).
    The aim reaches 1500 cm from the camera; the aim point must be within
    800 cm of the pawn.
  - When the view ray hits nothing, as with a level camera over open ground,
    the aim point is the ground below the ray end, kept within reach. The ring
    follows the camera pitch.
  - It needs `Skill.Gathering.Mining` level 2. Below that, activation fails and
    the HUD shows "Mining 2 required". HARV-09b replaced this gate with the
    pickaxe skill tree.
  - It costs only `GE_Cooldown_Harvest_RiftGrip`: 4 s, granting
    `Cooldown.Harvesting.RiftGrip` on the character. Re-equipping cannot reset
    it.
  - It reuses the pickaxe swing montage and its commit notify.
  - While Q is held, `AimCameraMode` (`CM_Harvest_Aim`) gives the owning
    client a higher view that looks at least 20 degrees down, so the ring and
    the targets stay readable. Lyra's ability camera mode seam sets it, and it
    is cleared on release or when the ability ends. HARV-09c relaxed the limit
    to 10 degrees, because at 20 degrees no aim point on flat ground lay more
    than about 6 m from the pawn.
- **Q/E/R defaults:** `AS_Tool_Pickaxe` grants Rift Grip with
  `InputTag.Weapon.Ability.1`. Since HARV-09b the tool set grants only the
  swing, and the pickaxe tree places Rift Grip on Q/E/R.
  - Weapon ability input belongs to `URpgWeaponAbilityLoadoutComponent`. An
    ability set entry with `InputTag.Weapon.Ability.N` therefore does not bind
    statically. It marks its spec as the default of slot N
    (`Rpg.WeaponAbilityLoadout.DefaultSlot.N`) and needs an `AbilityIdTag`.
  - A slot without a player selection follows the default of the currently
    granted abilities. A player selection always wins; clearing it hands the
    slot back to the default.
  - Two different defaults for one slot block it and log a content error.
  - `URpgEquipmentManagerComponent` re-resolves the controller's Q/E/R and
    quick-access bindings on the next tick whenever equipment grants change.
    Grants made inside an executing ability, such as the collect interaction
    that picks up the pickaxe, stay pending until the ability list unlocks.
  - `IA_WeaponAbility01`–`03` no longer use a Pressed trigger. With it, Enhanced
    Input reported a release one frame after the press, so a held Q could
    never aim. The native Started and Completed bindings still deliver exactly
    one press and one release per key press.
- **Indicators while aiming:**
  - Every target in the area gets its own projected indicator. The indicator
    widget derives from `URpgHarvestTargetIndicatorWidget`. It reads the
    status of exactly its target: stock, plus the sections it would take while
    aiming (`4/4  -3`).
  - `BP_HarvestAreaMarker` shows the area with the decal material
    `M_HarvestAreaRing`. `URpgHarvestTargetingComponent::AreaMarkerClass`
    spawns it locally, scales it to the area radius and hides it on release.
- **Ability failure toast (core HUD):** abilities already map failure tags to
  text in `FailureTagToUserFacingMessages`, but nothing displayed them.
  - `URpgAbilityFailureToastWidget` listens for
    `Ability.UserFacingSimpleActivateFail.Message` of its owning player.
  - `CUI_AbilityFailureToast` presents the reason for 2 s.
  - It sits in the new `UI.HUD.Slot.Notification` extension point of
    `CUI_RpgHudLayout` and is registered by every experience.
- **Removed:**
  - `URpgGameplayAbility_Stoneburst` and the native tag
    `Ability.Harvesting.Stoneburst`.
  - The Stoneburst grant in the GameFeature. The same action still grants
    `RpgGatheringSet` to player states.
  - Saved quick-access bindings to Stoneburst resolve as missing.
- **Test map:** `Lvl_HarvestPickaxe` adds two veins next to the comparison
  vein, so one Rift Grip covers three veins.
- **Multiplayer:** checked in a listen-server PIE session with one client.
  - The pickaxe's equipment actor `BP_Tool_PickaxeActor` did not replicate, so
    clients never saw a picked-up pickaxe. It replicates now. Only the server
    spawns equipment actors, so `URpgEquipmentDefinition` data validation now
    rejects `ActorsToSpawn` classes that do not replicate.
  - Clients showed no Q/E/R icons. The HUD binds the weapon ability view
    model when it is constructed, which on a client can happen before the
    PlayerState and its ability system replicate. The view model now adopts
    the controller's ability system once it exists. It also refreshes when
    replicated ability specs arrive, because a slot's ability id can arrive
    before the spec it names.
  - Verified for the client and the host: pickup and equip, the primary
    indicator, the pickaxe strike with replicated stock and ore, the swing
    montage seen by the other player, the "Mining 2 required" toast, and Rift
    Grip. The server harvested exactly the veins the client indicated.
  - The server selects a remote player's targets from the camera that player
    reports through `ServerUpdateCamera`. The character movement component
    sends it with its moves, so it is at most one move interval old (12 Hz
    while standing still).
  - Pawns without a `UPawnMovementComponent`, such as the Mover pawns, never
    report their camera. Harvesting is not enabled in those experiences. To
    enable it there, the client first has to send its aim with the activation
    as target data.

## Weak points (HARV-05)

The plan's crit is a readable weak point: a precise swing on it extracts more
of the same stock, so it saves work without adding yield.

- **Rule:** a swing that strikes the active weak point takes
  `URpgHarvestProfile::WeakPointBonusSections` extra sections, clamped to the
  remaining stock. The total yield and XP of a resource never change, and normal
  swings stay fully worthwhile. Zero disables weak points for a resource type.
- **Placement:** `URpgHarvestableComponent::WeakPointLocations` lists points in
  the local space of `WeakPointFrame`, usually the resource mesh. Weak points
  follow the mesh's placement and its section scaling.
  `WeakPointRadius` (35 cm) is the hit tolerance.
- **Active weak point:** one point is active at a time. It is
  `(Revision + HarvestedSections) % Num`, derived only from replicated stock
  state. The server and every client agree without extra replication, and the
  point moves on after every extracted section and every respawn.
- **Hit test:** a swing counts when its aim ray passes within the radius of the
  active point, and the point lies at most two radii beyond where the ray
  stopped. The marked point sits on the visible surface; a coarser collision
  hull or a swept contact can stop the ray before it. Points deeper inside or
  behind the resource never count. Hits without trace data fall back to the
  impact point.
- **Who can crit:** abilities opt in with `bCanHitWeakPoints`, and only
  single-target targeting uses it. Pickaxe Strike can crit. Rift Grip and other
  area powers never do; their area hits carry no aim ray.
- **Commit and preview:** the stock rules evaluate the bonus, so the server
  commit and the owning client's preview agree. `FRpgHarvestResult::bWeakPointHit`
  reports it. `URpgHarvestTargetIndicatorWidget::IsWeakPointTargeted` lets the
  indicator show `4/4  -2  Weak point` before the swing.
- **Content:**
  - `HP_IronVein` awards one bonus section, so the plan's "up to 2 remaining
    sections" holds.
  - `BP_HarvestNode_IronVein` places three points on the front face of
    `SM_RP_Vol_01_03`: left, upper middle and right.
  - `BP_HarvestNode_StaticMeshBase` shows the active point with
    `WeakPointMarker`, a small pulsing ember (`M_HarvestWeakPoint`) without
    collision or shadow. Every player sees it; it hides on depletion.
- **Multiplayer:** in a listen-server session the client saw the same marker
  as the server, previewed `-2` on it, and its swing took two sections on both
  sides. Two weak-point swings emptied a vein with the full stock of 24 ore.

## PCG resource bridge (HARV-06)

Open-world resources are placed by PCG and stay actor-free. The bridge makes
their instances harvestable with the same stock rules as actor nodes, so every
harvest ability works on them unchanged.

- **Representation:** `URpgHarvestableInstancesComponent` is an instanced static
  mesh component and an `IRpgHarvestableTarget`. A PCG static mesh spawner entry
  uses a Blueprint subclass per resource type as its descriptor
  `ComponentClass`. The subclass assigns the harvest profile and may present
  sections. The component replicates nothing, because PCG partition actors never
  replicate.
- **Stock:** `URpgHarvestInstanceStockComponent` lives on `ARpgGameStateBase`;
  the harvesting GameFeature adds it. It holds one FastArray entry for each
  instance whose stock differs from its authored state. An entry stores the
  revision, the extracted sections, the active flag and the change time.
  - Untouched instances cost nothing.
  - A respawn removes the entry again, so a respawned instance starts over at
    revision 0.
- **Stable keys:** an instance is identified by its authored world location
  rounded to whole centimeters.
  - Server and clients load identical generated transforms, so they derive the
    same key without replicating any identity. The key does not depend on which
    component or World Partition cell holds the instance.
  - Instances closer than one centimeter share a key; the component logs a
    warning.
  - The same key can identify saved depletion in HARV-10, as long as the
    generated PCG output stays unchanged.
- **Authority:** instance owners report authority on clients too, so commits
  check the GameState's authority instead. The GameState is also the loot
  source: rewarded item instances must not be outered to an actor that can
  stream out.
- **Streaming and late join:** on BeginPlay, a representation builds its key
  map, registers with the stock and presents every stored change as initial
  state. On EndPlay it unregisters.
  - The stock and the server's respawn timers do not depend on which cells are
    loaded.
  - When the GameFeature adds the stock after representations began play, the
    stock registers them itself.
- **Presentation:** depleted instances are scaled to zero, which also removes
  their collision; a respawn restores the authored transform.
  `OnInstanceStockChanged` lets the Blueprint subclass present sections.
  `SetInstancePresentationScale` changes only the presented scale, so the
  authored transform and the key stay intact.
- **Indicators:** target indicators now mark the targeted instance, not the
  bounds of the whole component. A held area ability shows one indicator per
  instance, also when several instances share one component.
- **Not supported on instances yet:**
  - weak points;
  - the manual interaction harvest (bushes stay on the HISM path);
  - runtime PCG generation (unverified);
  - instanced skinned meshes as targets; HARV-07 harvests trees through
    static trunk proxies instead.

### Content

- `BPC_HarvestInstances_IronVein` uses `HP_IronVein`. It shrinks an instance
  with its sections like the actor vein does (`MinimumSectionScale` 0.55).
- `PCG_HarvestIronVeins` chains three nodes:
  - `Create Points Grid`: 3×3 points in 5 m cells, local to the volume.
  - `Transform Points`: ±1.2 m offset, random yaw, scale 3.6–4.4.
  - `Static Mesh Spawner`: `SM_RP_Vol_01_03` with the component class above
    and `BlockAll` collision.
- `Lvl_HarvestPickaxe` has the PCG volume `Harvest_PcgIronVeinField` north of
  the actor veins. It is generated in the editor and saved with the map. Its
  generation trigger is On Demand, so play never regenerates it.
- The GameFeature data adds the stock component to `RpgGameStateBase` on server
  and clients.
- `M_RP_Vol_01_01` now has `bUsedWithInstancedStaticMeshes`. The editor set the
  flag when it first rendered the mesh instanced; cooked instances need it.

### Trees and skinned meshes

- In UE 5.8, `UInstancedSkinnedMeshComponent` creates per-instance bodies from a
  merged body setup of the mesh's physics asset, and its hits report the
  instance index. The Megaplant trees from #177 have no physics asset, so their
  instances neither collide nor can be traced.
- The component exports only some of its virtual functions, so a project
  subclass like `URpgHarvestableInstancesComponent` cannot link outside the
  engine. This was read from the engine headers, not attempted.
- HARV-07 follows the recommendation from this task: an invisible trunk proxy
  at the same PCG points holds the stock, and felling hides the visible
  instance and spawns a cosmetic falling tree. See the HARV-07 section.

### Budget

Measured in the Development editor with null RHI, through
`SurvivalRpg.Harvesting.Instances.Budget` and an Unreal Insights CPU trace of
that test. The test field has 10,000 instances in one component and 1,000
stored changes. The ranges come from two runs.

| Work | Cost |
| --- | --- |
| Building the key map of 10,000 instances | 0.36–0.71 ms |
| Registering and presenting 1,000 stored changes | 0.31–0.77 ms |
| Whole BeginPlay of the field, with and without stored changes | 0.6 ms / 1.2–1.5 ms |
| Commit without loot delivery | 6–10 µs on average; `ExtractSections` 1–2 µs |
| Evaluation for the preview | 1.5–5.6 µs |

- **Memory:** about 40 bytes of key map per loaded instance, local only.
- **Network:** one FastArray entry per changed instance, sent to every client
  because the GameState is always relevant. The bytes were not measured with
  Network Insights.
- **Engine costs dominate:**
  - Physics bodies of instanced meshes: `Init Body` takes about 6 µs per
    instance in this test.
  - Overlap queries at BeginPlay when a component generates overlap events. The
    component disables them by default, and PCG descriptors do too.
- **Test ceilings:** about ten times the measured costs.

### Multiplayer

The client was checked in a listen-server PIE session in `Lvl_HarvestPickaxe`:
- Its indicator marked a single PCG instance with `4/4`.
- Each swing took one section on server and client, and the instance shrank
  on both.
- Four swings emptied the instance for 24 ore, and it disappeared on both.
- Rift Grip at Mining 2 previewed two instances of the same component with
  `4/4  -3` each and took three sections from each.

In the first PIE session, the first two client swings on an instance took two
sections each; the cause was not found. Then 14 later swings each took one
section: 9 on instances and 5 on actor veins, in that session and after a
fresh PIE start. Injected key presses left over from failed pickup attempts
are the likely cause. Once, right after a pickup, the first swing on an actor
vein had no effect.

## Axe, Death Wave and trees (HARV-07)

The plan's M2: the axe fells trees one section at a time, and its awakened
Death Wave fells a whole stand of trees at once. The wood arrives without a
pickup chore, and protected trees survive the wave.

### Area rewards and protection

- **One delivery per harvest:** `URpgGameplayAbility_Harvest` opens an
  `FRpgHarvestRewardBatch` around its commit.
  - Every target still validates, rolls and materializes its own reward,
    extracts its stock and awards XP.
  - The batch then delivers all rewards as one atomic inventory batch, or as
    one drop. A single target drops overflow where it was struck; an area drops
    it at the harvester's feet. Stacks of the same item are merged.
  - The committed results report the batch's delivery path.
  - The targets have already extracted their stock when the batch delivers. If
    the inventory commit fails after its preflight, the batch falls back to the
    drop; only a failed drop spawn loses the batch, and it is logged as an error.
- **Protection:** `URpgHarvestProtectionComponent` is a box without collision,
  hidden in game, that can sit on any actor; `BP_HarvestProtectionZone` places
  one in a map. Boxes register with `URpgHarvestProtectionSubsystem` at
  BeginPlay.
  - Requests from area abilities carry `FRpgHarvestRequest::bAreaHarvest`. A
    resource whose location lies in a box rejects them as
    `ERpgHarvestOutcome::Protected`; the indicator shows "Protected".
  - Single-target swings and manual harvests ignore protection: the player
    chose that resource.
  - Actor nodes, PCG instances and legacy HISM instances check their own
    location, so previews and the server commit agree.
- **Area target count:** `MaxTargets` now counts only targets the ability
  would harvest. Nearer protected or depleted targets stay in the preview with
  their status but do not take a slot.
- **Presentation wave:** `URpgGameplayAbility_Harvest::PresentationWaveSpeed`
  (cm/s, 0 = at once) staggers how the targets of one harvest are presented.
  - The harvested target nearest to the harvester is presented at once. Every
    other target waits for its extra distance divided by the speed, up to
    2.5 s; the request carries the delay as `PresentationDelaySeconds`.
  - The stock, the rewards and the XP change at once; only the presentation
    waits.
  - The instance stock replicates the delay with the change. Each machine
    subtracts the server time that has passed since the change, so the server
    and every client present a target at the same server time.
  - Instanced resources honor the delay; actor nodes present at once.
- **Harvest direction:** an instanced resource also records the horizontal
  direction from the harvester toward it with every change (a yaw in 256
  steps) and replicates it with the stock.
  `URpgHarvestableInstancesComponent::GetInstanceHarvestDirection` returns it
  on every machine.

### Axe content

All assets live in `GF_Harvesting_Magic` and are authored through Unreal MCP.

- `ID_Tool_Axe` (tool category `Tool.Harvesting.Axe`) → `ED_Tool_Axe` (main
  hand, placeholder `BP_Tool_AxeActor`) → `AS_Tool_Axe`:
  - `GA_Harvest_AxeChop` on `InputTag.Weapon.Primary` takes one section per
    swing. It reuses the pickaxe swing montage.
  - `GA_Harvest_DeathWave`, the default of weapon ability slot 1 (Q):
    - Hold to aim and release to fell. It empties every tree in a 6 m area at
      the aim point, up to six trees.
    - The aim point must lie within 10 m of the pawn.
    - It needs `Skill.Gathering.Logging` level 2; below that the HUD shows
      "Logging 2 required". HARV-09b moved Death Wave into the axe skill tree
      and removed the gate and the slot default.
    - It costs only `GE_Cooldown_Harvest_DeathWave` (6 s,
      `Cooldown.Harvesting.DeathWave`).
    - Its presentation wave travels at 15 m/s, so the nearest tree falls
      first.
- `BP_Pickup_Axe` lies next to the pickaxe pickup in `Lvl_HarvestPickaxe`.
- `HP_DeadPine` has 4 sections, requires the axe and awards 9 Logging XP per
  section. It respawns after 120–180 s. `LT_DeadPine` yields 5 `ID_Wood` per
  section.

### PCG trees

- `PCG_HarvestDeadPines` spawns two meshes from the same points:
  - A skinned mesh spawner places the visible `Tree_Dead_Pine_A` trees with
    the `DW_HarvestTrees` wind provider and the component tag
    `HarvestTreeVisual`. The trees have no collision.
  - A static mesh spawner places an invisible cylinder per tree with
    `BPC_HarvestInstances_DeadPine`. The proxy is about 55 cm wide and reaches
    2.5 m high, where the target indicator stays in view. Only its scale
    differs from the tree's point, so both share the stock key.
- **Linked presentation:** `URpgHarvestableInstancesComponent::LinkedPresentationTag`
  links instances of tagged sibling instanced static or skinned meshes at the
  same location to the centimeter.
  - A depleted proxy hides its linked instances, and a respawn restores them,
    also on stream-in and late join.
  - `GetLinkedPresentationInstance` gives Blueprints the linked mesh and its
    authored transform.
- **Felling:** `BPC_HarvestInstances_DeadPine` spawns `BP_HarvestFallingTree`
  with the linked skeletal mesh for a live depletion on every machine with a
  local player. The instance presents the depletion when the harvest's
  presentation wave reaches it.
  - The component faces the falling tree along the replicated harvest
    direction and keeps the mesh's authored rotation. The tree falls along its
    forward axis, away from whoever felled it, the same way on every machine.
    It then sinks into the ground and destroys itself.
  - It has no collision, never replicates, and its wood has already been
    delivered.
  - A first version took both the delay and the direction from each machine's
    local player. A host far from the trees saw them fall up to 1 s later than
    the harvesting client, and in another direction. The replicated
    presentation wave and harvest direction replaced it.
- **Test map:** `Lvl_HarvestPickaxe` has the PCG volume
  `Harvest_PcgDeadPineStand`, twelve dead pines east of the ore veins.
  - It is generated in the editor, saved, and set to generate On Demand.
  - `Harvest_ProtectedPines` protects its two eastern trees.
- **Area marker:** the ring decal now projects 40 cm deep at a 100 cm radius
  (240 cm for Death Wave). Before, a large area tinted whole tree trunks.

### Budget

The budget test adds a field whose 10,000 proxies link 10,000 visible
instances. Its stream-in with 1,000 stored changes took 2.26 ms in one run,
against 1.18 ms without links. Linking adds about one key lookup per visible
instance at BeginPlay. The test ceiling is 30 ms.

### Multiplayer

A listen-server PIE session with one client in `Lvl_HarvestPickaxe` checked
the following on the client:
- All twelve proxies are linked to their visible trees on the server and the
  client.
- The client picked up the axe.
- The chop indicator showed `4/4` on the trunk.
- Four chops took one section each on both machines, for 20 wood.
- The fourth chop hid the tree on both machines and felled it away from the
  player.
- Below Logging 2, Q showed "Logging 2 required" and changed nothing.
- At Logging 2, holding Q previewed `4/4  -4` on the trees in the ring.
  Releasing felled them on both machines and added their wood in one
  delivery, without a drop.
- With the host far away at the spawn, a wave over three trees hid them 0.33 s,
  0.39 s and 0.72 s after the release. The server and the client hid each
  tree in the same probe, which polled about every 60 ms.
- The falling trees of a chop and of a three-tree wave had the same forward
  and up vectors on the server and the client, pointing away from the
  harvesting client.
- Aimed at the protected corner, the preview showed "Protected" on both
  protected trees. Only the unprotected tree in the ring was felled; a chop
  on a protected tree still took a section.
- A tree felled at the start of the session returned to its full stock on both
  machines after its respawn time.

### Not done

- The plan's small building task with both tools is open: the workbench
  recipes use the storage test materials, not `ID_Wood` and `ID_Ore`.
- Trees present a felled state only through the falling actor; no stump
  remains.
- Weak points on instances and runtime PCG generation stay unsupported.

## Grave Swarm (HARV-08)

The plan's M3: the awakened axe raises a swarm of grave wisps at a chosen spot.
The wisps work through the trees around it one after another until all of them
have fallen. The wood reaches the player without orders, pickups or companion
inventories.

### Beneficiary and physical harvester

- `FRpgHarvestRequest::Harvester` is the beneficiary: it receives the rewards
  and XP, and its trade skills are checked.
- `FRpgHarvestRequest::PhysicalHarvester` is set when something else strikes
  the resource. `GetStrikingActor()` returns it, or the harvester.
- A swarm's requests name the summoner's player state as the harvester, so the
  rewards arrive even when the pawn dies first. The swarm is the physical
  harvester: instanced resources record the felling direction from it, so a
  tree falls away from the swarm, not from the player.

### Swarm behavior

- **Summon:** `URpgGameplayAbility_Harvest::SwarmClass` turns an area ability
  into a summon. Its commit spawns the swarm at the aim point instead of
  harvesting.
  - `FRpgHarvestSwarmParams` tunes the creature count, flight speed, rise
    time, launch interval, the rest between strikes, misses, lifetime and line
    of sight.
  - `SectionsPerTarget` is what one creature takes per strike.
- **Work:** the swarm takes every resource the area selected, up to the
  ability's `MaxTargets`, and no others. The hold-to-aim preview marks each of
  them with its whole remaining stock.
- **Strikes:** creatures start with `FRpgHarvestSwarmPlanner`'s assignment,
  nearest first, covering one resource's stock before the next.
  - After every strike a creature rests briefly. It then strikes the same
    resource again or flies to the nearest selected resource with stock that no
    other creature reserved, until nothing is left. The resources therefore
    fall one after another.
  - Every strike commits once through `IRpgHarvestableTarget`. Reservations
    are not locks: other players keep harvesting, and only the stock left when
    a creature arrives counts.
- **Misses:** a creature that arrives at a resource emptied, removed or
  protected meanwhile heads for another selected resource, up to
  `MaxReassignments` times. Creatures with nothing left to take finish; in an
  area emptied early they dissipate without harvesting.
- **Reach and obstacles:** resources must lie in the area, and protected
  resources are skipped like for every area power. With
  `bRequireLineOfSight`, only resources and pawns may lie on the line from the
  summon point; resources behind anything else are previewed as out of reach
  and not taken.
- **Delivery:** the rewards of all strikes reach the player as one batch when
  the last creature finished: into the inventory, or as one drop at the
  player's feet. The swarm opens its `FRpgHarvestRewardBatch` only around each
  strike, so it never collects unrelated harvests in between.
- **Lifecycle:**
  - The swarm keeps working when the player switches tools or the ability
    ends. It keeps the tool category and power it was summoned with.
  - When the summoner dies (`Status.Death`) or is gone, the remaining
    creatures stop at once and harvest nothing more. Rewards already harvested
    are still delivered.
  - Creatures that have not finished after `MaxLifetimeSeconds` stop.
- **Presentation:** the swarm replicates each creature's flight with server
  times.
  - Every machine with a local player spawns a non-replicated `CreatureClass`
    actor per creature and moves it along the same arc at the same server
    time.
  - `On Creature Launched`, `On Creature Struck` and `On Creature Finished` let
    the swarm Blueprint add effects.
  - A machine that receives the swarm late replays neither finished creatures
    nor earlier strikes.

### Grave Swarm content

All assets live in `GF_Harvesting_Magic` and are authored through Unreal MCP.

- `GA_Harvest_GraveSwarm` is the default of weapon ability slot 2 (E) in
  `AS_Tool_Axe`:
  - Hold to aim and release to summon `BP_HarvestSwarm_Grave` at the aim
    point, which must lie within 15 m of the pawn.
  - Five creatures fell every tree within 8 m, up to ten. Each strike takes
    two sections and is followed by a 0.7 s rest, so the trees fall one after
    another.
  - It needs Logging 2, like Death Wave. HARV-09b moved Grave Swarm into the
    axe skill tree and removed the gate and the slot default.
  - It costs only `GE_Cooldown_Harvest_GraveSwarm` (15 s,
    `Cooldown.Harvesting.GraveSwarm`).
- `BP_HarvestSwarmCreature_Grave` is a pale green wisp with a short tail and a
  small light. Its `MI_HarvestSwarmWisp` reuses the weak point material.
- `BP_HarvestSwarm_Grave` spawns a short glowing pop,
  `BP_HarvestSwarmStrikeBurst`, at every strike and lets a finished wisp
  shrink away.
- `Lvl_HarvestPickaxe` has a second stand of twelve dead pines,
  `Harvest_PcgGraveSwarmStand`, west of the ore veins. Death Wave and Grave
  Swarm therefore each have their own trees.

### Multiplayer

Listen-server PIE sessions with one client in `Lvl_HarvestPickaxe` checked the
following on the client:
- Below Logging 2, E showed "Logging 2 required" and summoned nothing.
- At Logging 2, holding E in the middle of the stand showed `4/4  -4` on the
  four trees inside the ring.
- On release, the server and the client each showed five wisps. The four trees
  fell one after another on both machines, 1.8 s to 4.9 s after the release,
  with the same stock on both machines in every probe.
- The 80 wood arrived as one delivery when the last wisp finished, without a
  drop. The wisps and the swarm were gone on both machines about 7 s after the
  release.
- A summon at the edge of the stand selected and felled the three trees inside
  its ring.
- A session before the swarm cleared its area compared the computed creature
  positions: they agreed within 8 cm while rising and within about 90 cm in
  fast flight, because the client's estimate of the server time differs by
  about 0.1 s.

The network test `SurvivalRpg.Network.LootHarvestPIE.SwarmReplicatesFlightsAndLateJoins`
runs a slow swarm on a dedicated server with clients:
- The server spawns no creature actors. The client presents one per creature,
  each close to its replicated flight.
- A client that joins after the first creature finished presents only the
  creatures that have not.
- The swarm delivers once into the summoner's inventory. Both clients then show
  the harvested field, and the swarm and its creatures are gone.

### Not done

- The plan's talent variants, Swarm Brood and Grave Detonation, wait for the
  HARV-09 talent tree. Creature count, sections per strike and the rest between
  strikes are their knobs. HARV-09c added both as axe tree nodes.
- Concurrent player harvesting, misses and the summoner's death are covered by
  automation tests, not by a PIE session.
- Creatures fly straight arcs, also through obstacles; only the selection
  respects the line of sight.

## Skill UI and tool trees (HARV-09b)

HARV-09b makes the skill trees of HARV-09a playable; see
[skill-trees.md](skill-trees.md) for the system.

### Content

- `DA_SkillTree_Axe` (`SkillTree.Tree.Axe`, Logging) has the branches Death
  and Grave with one active node each in row 0: Death Wave in column 0 and
  Grave Swarm in column 2. Column 1 stays free for the Ash Pact (HARV-09d).
  HARV-09d made the Ash Pact a form of Death Wave instead.
  HARV-09c widened the tree to seven columns; see its layout rule.
- `DA_SkillTree_Pickaxe` (`SkillTree.Tree.Pickaxe`, Mining) has Rift Grip in
  row 0 of the Rift branch.
- Every node costs one point and grants its own ability set
  (`AS_Node_DeathWave`, `AS_Node_GraveSwarm`, `AS_Node_RiftGrip`) without an
  input tag. Logging or Mining level 2 earns the first point.
- `ID_Tool_Axe` and `ID_Tool_Pickaxe` carry a skill tree fragment for the main
  hand. `AS_Tool_Axe` and `AS_Tool_Pickaxe` grant only the swing.
- The three abilities lost their own level gate (`RequiredSkillTag`).
- `GF_Harvesting_Magic` scans `/GF_Harvesting_Magic/Progression/SkillTrees`
  for skill trees, so server and clients know both trees.

### Skill screen

- H (`IA_UI_Skills`, already mapped in `IMC_UI_PlayerHUD`) opens the game menu
  (`UI.Screen.GameMenu`) on its Skills tab. The menu is now a
  `URpgActivatableWidget` in menu input mode and selects the tab named by the
  payload's screen tag (`UI.Screen.GameMenu.Skills`).
- `CUI_Skills` shows the character level, every trade skill with its XP bar,
  the trees with their free points, and the selected tree: the tree picked in
  the list, otherwise the tree of the main-hand weapon.
- The tree view lays out the nodes with `URpgSkillTreeGridWidget` and draws
  the prerequisite links and the locked rows. Nodes show their state by color.
  Clicking a node selects it and learns it when it is available; Q, E and R
  place the selected learned ability; Reset tree refunds every point.

### Multiplayer

A listen-server PIE session with one client in `Lvl_HarvestPickaxe` checked
on the client:
- H opened the menu on the Skills tab. Logging 2 showed one free axe point
  and both actives as learnable.
- Clicking Grave Swarm learned it on the server, put it on Q, and showed it as
  learned on Q. After closing the menu, Q summoned the swarm, and 61 wood
  arrived.
- Reset refunded the point and emptied Q. Death Wave then took Q.
- With the pickaxe in the main hand, Q/E/R were empty and the menu opened on
  the pickaxe tree. Mining 2 let Rift Grip take Q, while the axe tree kept
  Death Wave on Q.
- Extensibility probe: a node added only in `DA_SkillTree_Axe` (prerequisite
  Death Wave, two points spent, one tuning) appeared below Death Wave with a
  link and a locked row. After two learned nodes it became learnable. It was
  removed again without saving.

### Not done

- Save and load in PIE were not exercised; the automation tests of HARV-09a
  cover them.
- Escape closes the menu through CommonUI; the PIE script closed it directly,
  because injected keys bypass the Slate back action.
- Gamepad navigation inside the tree uses plain buttons and was not tuned.

## Power forms through tunings (HARV-09c)

HARV-09c gives the tool powers upgrade and form nodes. The nodes carry only
tuning entries; the harvest mechanism reads them.

### Harvest tunings

`URpgGameplayAbility_Harvest` applies the learned tunings of its weapon's tree
to its authored values. Each tag names one value:

| Tag | Changes | Unit |
| --- | --- | --- |
| `Ability.Tuning.Harvest.AreaRadius` | `Targeting.AreaRadius`; the preview ring follows it | cm |
| `Ability.Tuning.Harvest.Reach` | `Targeting.MaxReachFromAvatar`; the aim ray grows by the same amount | cm |
| `Ability.Tuning.Harvest.MaxTargets` | `Targeting.MaxTargets`, rounded, at least 1 | targets |
| `Ability.Tuning.Harvest.Sections` | `SectionsPerTarget`, rounded, at least 1; per strike for a swarm | sections |
| `Ability.Tuning.Harvest.Creatures` | `Swarm.CreatureCount`, rounded, 1 to 16 | creatures |
| `Ability.Tuning.Harvest.StrikeInterval` | `Swarm.StrikeIntervalSeconds`, the rest after a strike | s |
| `Ability.Tuning.Harvest.StrikeRadius` | `Swarm.StrikeRadius` | cm |
| `Ability.Tuning.Harvest.Cooldown` | duration of the cooldown effect | s |

- The preview resolves the values each time it evaluates. The server
  captures them, with the aim, when execution starts; learning or resetting a
  node during the swing changes nothing.
- The cooldown tuning sets the duration of the cooldown effect's spec, so
  existing cooldown effects need no change. Only effects with a duration are
  tuned.
- The plan suggested a cooldown through SetByCaller. Setting the spec's
  duration keeps the existing `GE_Cooldown_Harvest_*` assets unchanged.

### Strike radius

- `FRpgHarvestSwarmParams::StrikeRadius` (default 0) lets every creature
  strike also commit each other resource the swarm works on within the radius,
  once per strike, with the strike's sections.
- Stock that other creatures reserved stays theirs, so the swarm never
  empties a resource another creature is on its way to. Commits clamp to the
  stock, as always.
- The swarm replicates the radius; `Get Strike Radius` lets the presentation
  use it. `BP_HarvestSwarm_Grave` flashes `BP_HarvestAreaMarker` on the ground
  at strikes with a radius, scaled to it, for half a second.

### Content

All nodes cost one point and carry only tunings.

**Gates:** as in New World, a node only needs the node above it in its chain;
loose passives need nothing. No node below the ultimate has a point gate
(`RequiredPointsInTree` 0), so the first point may go to any power or passive.
The ultimate of HARV-09e keeps a point gate.

**Layout rule:**
- Exclusive forms sit side by side in one row below their power, so the links
  fork. Upgrades that stack chain straight down.
- The axe uses seven columns: Death Wave in column 1 with its branch in
  columns 0 to 2, Grave Swarm in column 5 with its branch in columns 4 to 6,
  and the loose passives in the middle column 3.
- Rift Grip sits in the middle of five columns, between the two passives.

| Tree | Node | Row, column | Needs | Tunings |
| --- | --- | --- | --- | --- |
| Axe | Wide Wave | 1, 1 | Death Wave | Death Wave area +300 cm, targets +4 |
| Axe | Long Reach | 2, 1 | Wide Wave | Death Wave reach +500 cm |
| Axe | Swarm Brood | 1, 4 | Grave Swarm; excludes Grave Detonation | Grave Swarm 8 creatures, 1 section per strike |
| Axe | Grave Detonation | 1, 6 | Grave Swarm; excludes Swarm Brood | Grave Swarm 3 creatures, strike radius 700 cm |
| Axe | Keen Edge (passive) | 0, 3 | nothing | Axe swing sections +1 |
| Axe | Quick Recovery (passive) | 1, 3 | nothing | Every axe ability's cooldown ×0.8 |
| Pickaxe | Wide Rift | 1, 1 | Rift Grip; excludes Deep Grip | Rift Grip area +150 cm, targets +2 |
| Pickaxe | Deep Grip | 1, 3 | Rift Grip; excludes Wide Rift | Rift Grip sections +1 |
| Pickaxe | Steady Hands (passive) | 0, 0 | nothing | Pickaxe swing sections +1 |
| Pickaxe | Quick Recovery (passive) | 0, 4 | nothing | Every pickaxe ability's cooldown ×0.8 |

- The plan proposed a 3 m detonation. The trees of the grave stand stand 5 to
  8 m apart, so 3 m never reached a neighbor; 7 m reaches the direct
  neighbors.
- Wide Wave stays outside an exclusive group until the ultimate of HARV-09e
  exists. HARV-09d paired it with Ash Wave instead; the ultimate stands alone.
- `CM_Harvest_Aim` now looks at least 10 degrees down instead of 20. At 20
  degrees no aim point on flat ground lay more than about 6 m from the pawn, so
  Long Reach, and already the 10 m and 15 m reach of Death Wave and Grave
  Swarm, could not be used.

### Refunding single nodes

A right-click on a learned node refunds just that node, so switching forms
needs no reset; [skill-trees.md](skill-trees.md) has the rules.
- `URpgSkillTreeComponent::RefundNode` forgets the node for free, refunds its
  cost and clears its Q/E/R slots. Clients use `RequestRefundNode`.
- It is rejected while another learned node requires the node, or needs its
  points for a point gate. As in New World, the later node goes first.
- `CUI_SkillTreeNode` overrides On Mouse Button Down: a right-click selects the
  node and calls the node view model's `RequestRefund` when `bCanRefund` is set.

### Multiplayer

Listen-server PIE sessions with one client in `Lvl_HarvestPickaxe` checked on
the client:
- The skill screen showed the new nodes with their links, row gates and the
  excluded form. Logging 5 learned Death Wave, Grave Swarm, Wide Wave and
  Grave Detonation; Swarm Brood showed as excluded.
- After the layout change, Grave Swarm and Rift Grip fork into their two
  forms, and Death Wave chains into Wide Wave and Long Reach.
- Without point gates, Logging 3 learned Keen Edge and Grave Swarm as the
  first two points. Keen Edge made the axe swing preview take 2 sections.
  Grave Detonation and Quick Recovery followed; Grave Swarm's 15 s cooldown
  then ended after about 12 s.
- Death Wave with Wide Wave previewed a 900 cm ring. The commit felled exactly
  the previewed trees (four, and three on a second cast).
- With Long Reach, the aim point lay 1074 cm from the pawn and in reach;
  without it, that is beyond the 1000 cm reach.
- Grave Swarm with Grave Detonation summoned three creatures. The first
  strike took 8 sections: its own 2 and 2 from each of three neighbors within
  7 m. The three creatures emptied five trees in about 2.5 s, delivered once,
  and the ground ring showed the strike area.
- Grave Swarm with Swarm Brood summoned eight creatures with one section per
  strike. They emptied the four selected trees.
- Rift Grip with Wide Rift previewed a 450 cm area. After a reset, Deep Grip
  previewed and took 4 sections from each of two veins.
- Refunds, with real right-clicks sent to the client window through Unreal MCP's
  Slate inspector:
  - Right-clicking Grave Swarm changed nothing while Grave Detonation built on
    it.
  - Grave Detonation, then Grave Swarm and Keen Edge refunded on the server,
    and Q emptied.
  - Grave Swarm and Swarm Brood were then learned without a reset.

### Not done

- The values are first tuning; HARV-09f compares the forms by harvest time.
- In two PIE runs, the first held power right after the script closed the
  menu executed before the scripted release; every later hold waited for the
  release. HARV-09d traced it to the script's window captures, which change
  focus; it is no game bug.
- Refunds need a mouse right-click; there is no gamepad binding yet.

## Ash Wave and charcoal (HARV-09d)

HARV-09d lets a skill change what a harvest yields. The first such form, Ash
Wave, turns Death Wave's wood into charcoal.

### Yield conversions

- **Schema:** `FRpgHarvestYieldConversion` names an input and an output material,
  a whole-number ratio (`InputPerOutput`), and a `RequiredOwnerTag`.
- **On the ability:** `URpgGameplayAbility_Harvest::YieldConversions` lists the
  conversions an ability can make. One is active while the ability's owner has
  its tag; the skill tree form grants it through `GrantedTags`, only while the
  weapon is in use.
- **Capture:** the server captures the active conversions with the other values
  when execution starts and passes them in `FRpgHarvestRequest`. A summoned
  swarm keeps those of its summon, even when the form is unlearned meanwhile.
- **Delivery:** `FRpgHarvestRewardBatch` applies them to the merged rewards of
  one harvest (`FRpgHarvestRewardService::ApplyYieldConversions`).
  - Remainders of several targets therefore combine. What does not fill a whole
    output stays the input material; nothing is lost.
  - Only plain stackable materials convert. Other loot, such as rare finds,
    stays unchanged.
- **Preview:** `FRpgHarvestPreview::YieldConversions` and the indicator's
  `GetYieldConversion` give the target markers "→ Charcoal 2:1".

### Content

- `ID_Charcoal`: a stackable material like `ID_Wood`, with a placeholder icon.
- `GA_Harvest_DeathWave` converts `ID_Wood` into `ID_Charcoal` at 2:1 while
  its owner has `Harvest.Form.AshWave`.
- **Axe tree:** Death Wave now forks into Wide Wave (row 1, column 0) and the
  new Ash Wave (row 1, column 2). The two exclude each other (`DeathWaveForm`).
  Ash Wave grants `Harvest.Form.AshWave`. Long Reach chains straight from Death
  Wave (row 2, column 1).
- **Kiln:**
  - `BP_CraftingStation_Kiln` is a copy of the workbench with the station tag
    `Crafting.Station.Kiln` and `DA_RecipeSet_Kiln`.
  - `DA_Recipe_Charcoal` burns 2 `ID_Wood` into 1 `ID_Charcoal` in 3 s, so
    charcoal stays reachable without the form.
  - `Harvest_Kiln` stands next to the tool pickups in `Lvl_HarvestPickaxe`.
- `WBP_HarvestTargetIndicator` appends the conversion to the marker text through
  `F_ConversionSuffix`.
- **Design change:** the plan had the Ash Pact as a toggle ability with its own
  mode effect. In review the user preferred a skill form like the Diablo 4 skill
  transformations, so Ash Wave is an exclusive form of Death Wave. A passive
  that also turns manual swings into charcoal is optional and was left out,
  because powers do most of the felling.

### Area aim stops at the reach

Found in review: with the flatter aim camera of HARV-09c, the ring could lie
beyond the reach, and the trees inside it read "Out of reach". Now an area aim
point beyond `MaxReachFromAvatar`, hit or not, moves back toward the harvester
and onto the ground within reach (`FRpgHarvestTargeting::SelectAndEvaluate`).
The ring stops at the reach, and everything inside it can be harvested.

### Multiplayer

Listen-server PIE sessions with one client in `Lvl_HarvestPickaxe` checked on
the client:
- Logging 3 learned Death Wave and Ash Wave; Wide Wave showed as excluded.
- While Q was held, the target markers read "4/4  -4  → Charcoal 2:1".
- **Death Wave casts:** two casts felled two trees each. Each cast's 40 wood
  arrived as 20 charcoal with no wood left over (40 charcoal in total).
- **Kiln:** pressing F at the kiln opened the crafting screen with the Charcoal
  recipe. Two crafts turned 4 of 5 wood into 2 charcoal in the station output.
- **Reach:** aiming Death Wave far beyond its 10 m reach left the aim point
  about 9.3 m away. The two trees in the ring fell (40 wood).

### Not done

- **Kiln screen:** the kiln still uses the shared manual crafting screen,
  titled "Crafting Station". Decided in review: processing stations such as
  the kiln or a smelter will run a selected recipe automatically, as in other
  survival games, while workbenches stay manual. That needs its own crafting
  task.
- **Icons:** the charcoal icon is a placeholder.
- **Early release in PIE:** a held power executed before the scripted release
  whenever the script captured the client window during the hold. The capture
  changes window focus, which releases held keys. Holds without a capture
  waited for the release, and the user could not reproduce it by hand, so it
  is a test-tool artifact.

## Striding Wave (HARV-09e)

HARV-09e gives the axe its ultimate: for a few seconds, every dead tree around
the walking player falls.

### Stride

- **Shape:** `ERpgHarvestTargetShape::AreaAroundHarvester` selects every
  resource within `AreaRadius` around the harvester, nearest first, up to
  `MaxTargets`. It needs no aim, and everything in the area is in reach. The
  area tunings apply to it as to every area power.
- **Stride:** `URpgGameplayAbility_Harvest::Stride` (`FRpgHarvestStrideParams`:
  duration and pulse interval) turns such an ability into a stride.
  - The commit starts it at once. Every pulse selects again around where the
    harvester is now and commits each target once through
    `IRpgHarvestableTarget`, with the values captured when execution started.
    The presentation wave applies to each pulse.
  - At most `ceil(duration / interval) × MaxTargets` commits happen, 48 for
    Striding Wave.
  - The ability stays active during the stride; a montage does not end it. The
    server ends the activation when the duration is over.
  - The stride opens its reward batch only around each pulse. When it ends, the
    rewards of all pulses reach the player as one delivery: into the inventory,
    or as one drop at the player's feet.
  - Switching tools, dying or cancelling ends it early, and what it harvested is
    still delivered. Only a world that ends takes it along. A dead harvester
    harvests nothing more.
  - Trees fall away from the player, who is the physical harvester.
- **Cue:** `StrideGameplayCue` is a looping cue on the harvester while the
  stride runs. Its `RawMagnitude` is the tuned area radius.
  - The stride starts inside the owning client's predicted activation. The
    server adds the cue without that prediction key, so the owning client plays
    it from replication instead of skipping it as already predicted.
  - `ActiveGameplayCues` replicates to every machine, also to late joiners.

### Content

- `GA_Harvest_StridingWave`:
  - Executes on press, without aim or montage.
  - Harvests 6 m around the player, up to four trees per pulse with all their
    sections (16). A pulse comes every 0.5 s for 6 s, with a 15 m/s
    presentation wave.
  - `GE_Cooldown_Harvest_StridingWave` lasts 60 s and grants
    `Cooldown.Harvesting.StridingWave`. Quick Recovery shortens it to 48 s.
- `AS_Node_StridingWave` grants the ability without an input tag, so the tree
  places it on Q/E/R.
- **Axe tree:** the ultimate node Striding Wave sits at row 3, column 3
  (`SkillTree.NodeKind.Ultimate`).
  - It needs 4 points spent in the tree and no prerequisite, so every build can
    reach it. With one point per Logging level above the first, that is
    Logging 6.
  - It excludes nothing; the axe has one ultimate.
- `GCN_Harvest_StridingWave`, a `GameplayCueNotify_Actor`, carries the area
  ring decal of `BP_HarvestAreaMarker`. It attaches to the player and scales to
  the radius.
- `GF_Harvesting_Magic` now registers its `/GameplayCues` folder with the
  GameplayCue manager (`AddHarvestingGameplayCuePath`).
- **Design change:** the first plan made Striding Wave a Death Wave form that
  excluded Wide Wave.
  - HARV-09d already paired Wide Wave with Ash Wave, and HARV-09c kept the
    point gate for the ultimate. So Striding Wave is its own ability on Q/E/R.
  - As a capstone without a chain, it follows the "choose freely" rule of
    HARV-09c.
  - Ash Wave's conversion stays a Death Wave form and does not apply to
    Striding Wave.

### Multiplayer

Listen-server PIE sessions with one client in `Lvl_HarvestPickaxe` checked on
the client:
- **Gate:** Logging 6 gave five points. Striding Wave showed as locked after
  three spent points and as learnable after four. Learned, it went to E.
- **Felling:** the client pressed E and walked with W through the Death Wave
  stand.
  - The trees within 6 m fell in pairs as the client passed: after about 1.2,
    3.0 and 5.2 s.
  - The column 9 m to the side and the row beyond the end of the walk stayed.
  - Server and client showed the same stock in every sample.
- **Ring:** on the client, the ring followed the client's pawn at 6 m radius;
  the host also showed it. It disappeared when the stride ended.
- **Delivery:** the 124 wood of six trees arrived once, when the stride ended
  after 6 s, without a drop. The E slot showed the cooldown.
- **Failed attempt:** in the first session the owning client showed no ring.
  The cue still carried the client's prediction key. Since the fix,
  `Stride.HarvestsAroundTheWalkingHarvester` checks that the cue carries none.
- In the first session the client also stopped walking when the script captured
  its window, the known focus artifact of HARV-09d.

### Not done

- **Network test:** there is no new PIE network test. The stride reuses the
  replicated stock, ability end and cue; native tests and the PIE session cover
  it.
- **Presentation:** the ring reuses the area marker's decal, whose line grows
  with the radius. There is no extra effect at the struck trees beyond their
  falling presentation.
- **Montage:** a stride ignores the end of its montage. Striding Wave has no
  montage, so content does not exercise that path.
- **Pickaxe:** the pickaxe has no ultimate yet.
- **Tuning:** the values are first tuning; HARV-09f compares the powers by
  harvest time.

## Performance guardrails

- Resources never tick. Respawn uses a timer. Replicated state is a revision,
  a section count, an active flag and a timestamp.
- Indicators run only on the local client: one query at about 15 Hz, never one
  per resource. The highlight changes only when the target changes.
- In the open world, PCG instances stay actor-free. The GameState keeps sparse
  state only for changed instances. HARV-06 measured the bridge with Unreal
  Insights; see its budget.

## Tasks

| ID | Scope | Status |
| --- | --- | --- |
| HARV-01 | Stock core: request/result contract, profile sections and tool requirement, `URpgHarvestableComponent`, multi-section reward batching, tests | Merged: [#178](https://github.com/Athurito/SurvivalRpg/pull/178) |
| HARV-02 | `URpgGameplayAbility_Harvest` base, shared targeting query, local `URpgHarvestTargetingComponent` preview read model | Merged: [#179](https://github.com/Athurito/SurvivalRpg/pull/179) |
| HARV-03 | M0 pickaxe content: tool item, equipment and abilities, iron vein, indicator presentation, `Lvl_HarvestPickaxe` test map | Merged: [#180](https://github.com/Athurito/SurvivalRpg/pull/180) |
| HARV-04 | M1 Rift Grip: hold to aim, Mining 2 gate, cooldown, Q/E/R ability-set defaults, area indicators; replaces Stoneburst | Merged: [#181](https://github.com/Athurito/SurvivalRpg/pull/181) |
| HARV-05 | M1 weak-point crit: bonus sections from a readable weak point | Merged: [#183](https://github.com/Athurito/SurvivalRpg/pull/183) |
| HARV-06 | PCG resource bridge: harvestable PCG instances with sparse state and a measured budget | Merged: [#184](https://github.com/Athurito/SurvivalRpg/pull/184) |
| HARV-07 | M2 axe and Death Wave area harvest, aggregated delivery, protected objects, PCG trees | Merged: [#185](https://github.com/Athurito/SurvivalRpg/pull/185) |
| HARV-08 | M3 grave swarm with separate beneficiary and physical harvester | Merged: [#186](https://github.com/Athurito/SurvivalRpg/pull/186) |
| HARV-09a | M4 skill tree foundation (core): tree definition, item fragment, PlayerState progress, weapon grants, Q/E/R per tree, tunings, save schema 4; see [skill-trees.md](skill-trees.md) | Merged: [#188](https://github.com/Athurito/SurvivalRpg/pull/188) |
| HARV-09b | Skill UI (progression overview, tree grid, Q/E/R, reset) and the tool trees; Rift Grip, Death Wave and Grave Swarm move from level gates to tree nodes | Merged: [#189](https://github.com/Athurito/SurvivalRpg/pull/189) |
| HARV-09c | Power forms through tunings: Wide Wave, Long Reach, Swarm Brood, Grave Detonation (strike radius), Wide Rift, Deep Grip; loose passives, chain-only gates, right-click refund of single nodes | Merged: [#190](https://github.com/Athurito/SurvivalRpg/pull/190) |
| HARV-09d | Ash Wave: Death Wave form that delivers charcoal through yield conversions at a shown ratio, charcoal item, kiln recipe | Merged: [#192](https://github.com/Athurito/SurvivalRpg/pull/192) |
| HARV-09e | Striding Wave, the axe's ultimate: trees around the walking player fall for a few seconds; strides around the harvester, point-gated ultimate node, ring cue | In review: [#193](https://github.com/Athurito/SurvivalRpg/pull/193) |
| HARV-09f | Resource parity across combat styles and a build target that stronger harvesting makes easier | Planned |
| HARV-10 | M5 resource persistence with stable IDs, portal variant, co-op load | Planned |

## Open questions

- Should the primary target also get an outline highlight? The project has
  no custom-depth outline yet, so HARV-03 marks the target with the projected
  label only.
- Answered in HARV-06: instanced skinned mesh instances can be traced only with a
  physics asset; the trees need one or a collision proxy. Stable keys are
  authored locations, which survive World Partition streaming.
- Answered in HARV-07: trees use static trunk proxies; a felled tree is a
  local cosmetic actor that falls away from the local player and sinks.
- Answered in HARV-09c: the swarm's talent variants are the axe tree nodes
  Swarm Brood and Grave Detonation.
- Still open from M2: the small building task with both tools. The workbench
  recipes use the storage test materials, not the harvested `ID_Wood` and
  `ID_Ore`; see the duplicated material sets below.
- Not decided yet: home-world regeneration, the timing of the awakening, limits
  on large power states, and the final co-op scope.
- The material sets duplicate each other: `ID_Ore` and its relatives versus
  the storage test materials.
