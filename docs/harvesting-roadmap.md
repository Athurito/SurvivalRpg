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
| First unlock | Rift Grip requires `Skill.Gathering.Mining` level 2. This reuses saved, replicated trade-skill progression. |
| Costs | Rift Grip uses only a short cooldown GameplayEffect on the character, with no mana. Re-equipping cannot reset it. |
| Resource representation | Prototype resources are actors with `URpgHarvestableComponent`. Open-world trees and rocks come from PCG; the PCG bridge in HARV-06 makes those instances harvestable. `URpgHarvestableInstancedMeshComponent` remains the legacy path for bushes and the sandbox. |
| Indicators | With a tool equipped, the primary-swing target is always highlighted. Special abilities show their area and every target while the input is held, and trigger on release. |

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
  `URpgHarvestableComponent`. The stock is replicated, dormant and
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
  - The non-reflected `FRpgHarvestStockRules`, shared with the future PCG
    bridge.
  - The abstract `URpgHarvestTargetIndicatorWidget` (HARV-04), which binds an
    indicator widget to exactly one previewed target.
  - HARV-04 removed the native `URpgGameplayAbility_Stoneburst` leaf.
- **Designer assets:** item, equipment and ability set definitions, `GA_*`
  abilities, `HP_*` profiles, `LT_*` loot tables, resource actor Blueprints,
  montages, cues, cooldown effects, indicator widgets, tags and test maps.
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
  `Stale`, `Invalid` and `DeliveryFailed`. A failed delivery leaves the stock,
  revision and XP untouched.
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
  `RPG Gameplay Event` notify that sends the commit tag. The time is resolved
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
    The aim reaches 1500 cm from the camera; the aim point must be within
    1200 cm of the pawn.
  - It needs `Skill.Gathering.Mining` level 2. Below that, activation fails with
    "Mining 2 required".
  - It costs only `GE_Cooldown_Harvest_RiftGrip`: 4 s, granting
    `Cooldown.Harvesting.RiftGrip` on the character. Re-equipping cannot reset
    it.
  - It reuses the pickaxe swing montage and its commit notify.
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
- **Removed:**
  - `URpgGameplayAbility_Stoneburst` and the native tag
    `Ability.Harvesting.Stoneburst`.
  - The Stoneburst grant in the GameFeature. The same action still grants
    `RpgGatheringSet` to player states.
  - Saved quick-access bindings to Stoneburst resolve as missing.
- **Test map:** `Lvl_HarvestPickaxe` adds two veins next to the comparison
  vein, so one Rift Grip covers three veins.

## Performance guardrails

- Resources never tick. Respawn uses a timer. Replicated state is a revision,
  a section count, an active flag and a timestamp.
- Indicators run only on the local client: one query at about 15 Hz, never one
  per resource. The highlight changes only when the target changes.
- In the open world, PCG instances stay actor-free. The server keeps sparse
  state only for touched instances. Budgets are measured with Unreal Insights
  before M2.

## Tasks

| ID | Scope | Status |
| --- | --- | --- |
| HARV-01 | Stock core: request/result contract, profile sections and tool requirement, `URpgHarvestableComponent`, multi-section reward batching, tests | Merged: [#178](https://github.com/Athurito/SurvivalRpg/pull/178) |
| HARV-02 | `URpgGameplayAbility_Harvest` base, shared targeting query, local `URpgHarvestTargetingComponent` preview read model | Merged: [#179](https://github.com/Athurito/SurvivalRpg/pull/179) |
| HARV-03 | M0 pickaxe content: tool item, equipment and abilities, iron vein, indicator presentation, `Lvl_HarvestPickaxe` test map | Merged: [#180](https://github.com/Athurito/SurvivalRpg/pull/180) |
| HARV-04 | M1 Rift Grip: hold to aim, Mining 2 gate, cooldown, Q/E/R ability-set defaults, area indicators; replaces Stoneburst | In review on `claude/harv-04-rift-grip` |
| HARV-05 | M1 weak-point crit: bonus sections from a readable weak point | Planned |
| HARV-06 | PCG resource bridge: harvestable PCG instances with sparse state and a measured budget | Planned, before M2 |
| HARV-07 | M2 axe and Death Wave area harvest, aggregated delivery, protected objects | Planned |
| HARV-08 | M3 grave swarm with separate beneficiary and physical harvester | Planned |
| HARV-09 | M4 talents, resource parity across combat styles, Ash Pact conversion | Planned |
| HARV-10 | M5 resource persistence with stable IDs, portal variant, co-op load | Planned |

## Open questions

- Should the primary target also get an outline highlight? The project has
  no custom-depth outline yet, so HARV-03 marks the target with the projected
  label only.
- For HARV-06: do ISKMC instances support per-instance traces, or are
  collision proxies needed? How are stable PCG instance keys kept under
  World Partition streaming?
- Not decided yet: home-world regeneration, the timing of the awakening, limits
  on large power states, and the final co-op scope.
- The material sets duplicate each other: `ID_Ore` and its relatives versus
  the storage test materials.
