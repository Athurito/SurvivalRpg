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
| First unlock | Rift Grip requires `Skill.Gathering.Mining` level 2. This reuses saved, replicated trade-skill progression. Later, the player picks such powers in a talent tree whose nodes are gated by the skill level (HARV-09). |
| Costs | Rift Grip uses only a short cooldown GameplayEffect on the character, with no mana. Re-equipping cannot reset it. |
| Resource representation | Prototype resources are actors with `URpgHarvestableComponent`. Open-world trees and rocks come from PCG; the PCG bridge in HARV-06 makes those instances harvestable. `URpgHarvestableInstancedMeshComponent` remains the legacy path for bushes and the sandbox. |
| Indicators | With a tool equipped, the primary-swing target is always highlighted. Special abilities show their area and every target while the input is held, and trigger on release. |
| Trees | Open-world trees are PCG instanced skinned meshes without collision. An invisible PCG trunk proxy at the same point holds the stock and is the harvest target (HARV-07). |
| Protection | Area powers skip resources inside a harvest protection box and report them as protected. A deliberate single-target swing still harvests them (HARV-07). |
| Area rewards | A multi-target harvest delivers the rewards of all its targets as one batch: into the inventory, or into one drop at the harvester (HARV-07). |

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
| Harvesting progression | `URpgTradeSkillProgressionComponent` (`Skill.Gathering.*`, saved) | Exists; talents come later |
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
    from authored montage data, and hold-to-aim.
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
- **Designer assets:** item, equipment and ability set definitions, `GA_*`
  abilities, `HP_*` profiles, `LT_*` loot tables, resource actor Blueprints,
  instanced resource component Blueprints (`BPC_HarvestInstances_*`), PCG
  graphs, the falling-tree presentation, protection zone actors, montages,
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
    the HUD shows "Mining 2 required".
  - It costs only `GE_Cooldown_Harvest_RiftGrip`: 4 s, granting
    `Cooldown.Harvesting.RiftGrip` on the character. Re-equipping cannot reset
    it.
  - It reuses the pickaxe swing montage and its commit notify.
  - While Q is held, `AimCameraMode` (`CM_Harvest_Aim`) gives the owning
    client a higher view that looks at least 20 degrees down, so the ring and
    the targets stay readable. Lyra's ability camera mode seam sets it, and it
    is cleared on release or when the ability ends.
- **Q/E/R defaults:** `AS_Tool_Pickaxe` grants Rift Grip with
  `InputTag.Weapon.Ability.1`.
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
      "Logging 2 required".
    - It costs only `GE_Cooldown_Harvest_DeathWave` (6 s,
      `Cooldown.Harvesting.DeathWave`).
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
  local player.
  - The tree falls away from the local player, then sinks into the ground and
    destroys itself.
  - It waits by its distance to the local player, up to 1 s, so a Death Wave
    fells the nearest trees first.
  - It has no collision, never replicates, and its wood has already been
    delivered.
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
| HARV-07 | M2 axe and Death Wave area harvest, aggregated delivery, protected objects, PCG trees | In review |
| HARV-08 | M3 grave swarm with separate beneficiary and physical harvester | Planned |
| HARV-09 | M4 talents (powers such as Rift Grip chosen in a level-gated talent tree), resource parity across combat styles, Ash Pact conversion | Planned |
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
- Still open from M2: the small building task with both tools. The workbench
  recipes use the storage test materials, not the harvested `ID_Wood` and
  `ID_Ore`; see the duplicated material sets below.
- Not decided yet: home-world regeneration, the timing of the awakening, limits
  on large power states, and the final co-op scope.
- The material sets duplicate each other: `ID_Ore` and its relatives versus
  the storage test materials.
