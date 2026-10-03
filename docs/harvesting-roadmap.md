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
| Stoneburst | Becomes Rift Grip, the pickaxe's awakened multi-section ability. The native leaf is replaced by a `GA_*` Blueprint in HARV-04. |
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
| Tool item → equipment → ability set → ability | `URpgInventoryFragment_EquippableItem`, `URpgEquipmentDefinition::SlotAbilitySetsToGrant`, `URpgAbilitySet`, `URpgGameplayAbility_FromEquipment` | Exists; tool content comes in HARV-03 |
| Harvestable component and definition | `URpgHarvestableComponent`, `URpgHarvestProfile` | HARV-01 |
| Harvest request and result | `FRpgHarvestRequest`, `FRpgHarvestResult`, `IRpgHarvestableTarget` | HARV-01 |
| Auto pickup and overflow container | `FRpgHarvestRewardService`: atomic inventory batch or one replicated drop | Exists; multi-section batching added in HARV-01 |
| Harvest input tags | `InputTag.Weapon.Primary` on the main-hand tool, and the weapon-ability and quick-access slots | Existing routing; no new input tags |
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
  - HARV-04 removes the native `URpgGameplayAbility_Stoneburst` leaf.
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
| HARV-01 | Stock core: request/result contract, profile sections and tool requirement, `URpgHarvestableComponent`, multi-section reward batching, tests | In review: [#178](https://github.com/Athurito/SurvivalRpg/pull/178) |
| HARV-02 | `URpgGameplayAbility_Harvest` base, shared targeting query, local `URpgHarvestTargetingComponent` preview read model | Implemented on `claude/harv-02-harvest-ability-targeting` (stacked on HARV-01) |
| HARV-03 | M0 pickaxe content: tool item, equipment and abilities, iron vein, indicator presentation, `Lvl_HarvestPickaxe` test map | Planned (needs the editor with Unreal MCP) |
| HARV-04 | M1 Rift Grip: hold to aim, Mining 2 gate, cooldown; replaces Stoneburst | Planned |
| HARV-05 | M1 weak-point crit: bonus sections from a readable weak point | Planned |
| HARV-06 | PCG resource bridge: harvestable PCG instances with sparse state and a measured budget | Planned, before M2 |
| HARV-07 | M2 axe and Death Wave area harvest, aggregated delivery, protected objects | Planned |
| HARV-08 | M3 grave swarm with separate beneficiary and physical harvester | Planned |
| HARV-09 | M4 talents, resource parity across combat styles, Ash Pact conversion | Planned |
| HARV-10 | M5 resource persistence with stable IDs, portal variant, co-op load | Planned |

## Open questions

- Should a swing sequence move into Core for the pickaxe montage, or should
  `GF_Harvesting_Magic` declare a dependency on `GF_Combat_Core`?
- Can a static `InputTag.Weapon.Ability.1` binding in an ability set coexist
  with `URpgWeaponAbilityLoadoutComponent`?
- For HARV-06: do ISKMC instances support per-instance traces, or are
  collision proxies needed? How are stable PCG instance keys kept under
  World Partition streaming?
- Not decided yet: home-world regeneration, the timing of the awakening, limits
  on large power states, and the final co-op scope.
- The material sets duplicate each other: `ID_Ore` and its relatives versus
  the storage test materials.
