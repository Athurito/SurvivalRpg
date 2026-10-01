---
name: survival-rpg-combat-foundation
description: Use for SurvivalRpg combat, equipment, inventory-facing item instances, loadouts and quick access, GAS ability grants, mastery/progression, runes, portal combat, Dungeonbreak, and modular GameFeature combat content. Prefers current repo truth and preserves the Lyra-rooted RPG equipment/inventory architecture. Pair with unreal-lyra-expert for engine-facing implementation and add unreal-gasp-expert for combat animation, montage/locomotion overlap, dodge, block, hit reactions, or GASP-driven presentation.
---

# SurvivalRpg Combat Foundation

## Skill boundaries

This skill owns SurvivalRpg combat, equipment, inventory-facing item instances, weapon action routing, runes, mastery/progression, portal combat, Dungeonbreak combat escalation, and combat GameFeature guidance.

- Add `unreal-lyra-expert` when implementation touches Unreal Engine, GAS internals, replication, Experiences, Game Features, Lyra Interaction, or Lyra-rooted inventory/equipment.
- Add `unreal-gasp-expert` when combat work changes the GASP AnimBP, Motion Matching layers, montage slots, root motion, dodge or block presentation, hit reactions, equipment sockets, death, ragdoll, or locomotion-driven combat tags. Combat outcomes stay authoritative in GAS and equipment; animation remains presentation.
- Add `survival-rpg-project` when a decision changes product scope, progression identity, survival friction, first-playable priorities, or long-term resource relevance.

Do not create a parallel combat, equipment, inventory, ability-grant, or item-instance authority beside the existing Lyra-rooted RPG architecture.

## Combat philosophy

The long-term fantasy is a powerful portal hunter and rune warrior in a living world threatened by portals. Combat is direct and readable, skill-based without becoming a pure Soulslike, built on timing, positioning, and build synergy, and driven by GAS abilities, effects, cues, and tags so melee, ranged, magic, runes, and portal-specific mechanics stay modular.

The foundation must prove that attack input triggers the correct ability, the montage plays reliably, hit detection is authoritative and readable, damage is applied through GameplayEffects, hit reactions, stagger, and death work, equipment grants and removes combat capabilities, and progression unlocks or modifies abilities cleanly.

## Start from current repository truth

Inspect the repository instead of relying on remembered document or class names. Read what is relevant:

- `docs/game-vision.md`, especially sections 8 to 10 and 12
- `docs/combat-mastery-reference.md` for the combat loop, mastery, and skill-group model
- `docs/combat-network-smoke.md`, `docs/rpg-baseline.md`, and `docs/inventory-refactor-plan.md` when networking, the baseline Experience, or inventory are involved
- `docs/gasp-cmc-sprint.md` and `docs/gasp-block-locomotion.md` when stamina, sprint, or block movement are involved

Then inspect the code that actually exists, especially `Source/SurvivalRpg/Equipment`, `Source/SurvivalRpg/AbilitySystem`, `Source/SurvivalRpg/Combat`, `Source/SurvivalRpg/Inventory`, `Source/SurvivalRpg/Progression`, `Source/SurvivalRpg/ActionBar`, `Source/SurvivalRpg/Core/Character`, the GameFeature plugins under `Plugins/GameFeatures`, and the active Experience setup. If a referenced doc or class is missing, flag the reference as stale and continue from code and the vision document.

## Equipment authority

Current runtime model; verify details in code before relying on them:

- Inventory graph locations (Gear and Carry grids) are the sole physical truth of what a character carries and wears.
- `URpgEquipmentLoadoutComponent` (controller-owned) reconciles a read-only Gear projection, owns the active Main-/OffHand selection and remembered pairings, runs side-effect-free conflict preflights, derives the equipment load tier (`Equipment.Load.*`), and drives pawn equipment and GAS grants.
- `URpgEquipmentManagerComponent` (pawn-owned, Lyra-rooted) holds the replicated applied equipment instances and their ability/effect grants. Each `URpgEquipmentInstance` keeps its source item instance as replicated instigator.
- `URpgWeaponAbilityLoadoutComponent` only chooses which granted ability ids are bound to `InputTag.Weapon.Ability.1..3`; `FRpgAbilityBindingResolver` resolves those ids to exactly one granted spec on the server.

Keep these ownerships:

- Server authority owns equip/unequip, slot conflict resolution, grant rebuilds, replicated equipped state, equipment-driven loose tags, aggregated ability and effect grants, active equipment context, source bindings, and authoritative damage application.
- Equip input abilities stay thin: they map input to requests and delegate state changes to the equipment components.
- Characters and UI mirror equipment state for presentation only.
- A new weapon or equipment helper is acceptable only with a narrow role that does not overlap equipped-state ownership.

## Native schema, mechanism, and designer content

Classify combat and inventory work before adding a native type:

- **Native schema** defines a reusable runtime concept that assets must be able to configure.
- **Native mechanism** enforces engine-facing behavior, authority, prediction, replication, lifecycle invariants, or a known or measured hot path.
- **Designer content** selects, composes, and tunes those schemas and mechanisms for a concrete item, ability, weapon, rune, portal, or encounter.

Pure designer-content work adds no native classes. Task size is not a native justification, and tooling inconvenience never justifies a C++ content leaf. The repository-wide rules live in `.agents/skills/survival-rpg-orchestrator/references/system-ownership-boundaries.md`.

### Inventory fragments are native schema types

- Keep `URpgInventoryItemFragment` and every semantic fragment subclass in C++. Do not create Blueprint fragment subclasses; Blueprint exposure exists so assets can configure native fragment instances.
- Add a native fragment subclass only for new item semantics or reusable runtime behavior that existing fragment types cannot express, never for a single item, a balance variant, different presentation data, or another combination of existing properties.
- Build concrete items by configuring inline instances of existing native fragment types on `URpgInventoryItemDefinition` assets. Sword, axe, and armor definitions differ in configured values, not in `BP_*Fragment` classes or one native fragment per item.

### Gameplay Abilities are asset-first

Use `URpgGameplayAbility` as the native foundation and implement concrete `GA_*` abilities as Blueprint assets. Shared designer flow may live in a Blueprint parent:

```text
URpgGameplayAbility (C++)
  GA_MeleeBase (Blueprint, optional shared design flow)
    GA_SwordLight (Blueprint)
    GA_AxeHeavy (Blueprint)
```

Add an abstract native intermediate only when the shared mechanism needs Blueprint-unavailable engine APIs, natively enforced authority/prediction/lifecycle invariants, or a known or measured hot path. Task size, shared designer sequencing, graph neatness, or `.uasset` convenience do not qualify. A justified native intermediate stays generic and exposes focused extension points for Blueprint descendants.

Keep concrete ability identity and content in assets, AbilitySets, equipment or item definitions, GameplayEffects, and GameplayCues: ability and input tags, costs and cooldowns, montage and cue selection, names, text, icons, range, radius, damage, duration and other balance values, and weapon-, rune-, element-, portal-, or encounter-specific composition.

Author Blueprint, GameplayAbility, GameplayEffect, GameplayCue, AbilitySet, and ItemDefinition assets through the Unreal MCP workflow of `unreal-lyra-expert`. Only a confirmed MCP capability gap justifies a small reusable editor-only tooling seam, never a runtime C++ content class.

Test the reusable native mechanism once, then validate concrete ability content through stable asset contracts: Blueprint compilation, intended parentage, required tags and references, and AbilitySet or equipment grants. Do not require a content ability to exist as a native `/Script/...` class or create a native leaf only for unit-test convenience.

## Documentation for combat data

Apply the documentation defaults in `AGENTS.md`. For combat data, cover in particular damage, cooldown, cost, range, duration, radius, tick interval and targeting values, gameplay tag fields, GameplayEffect and GameplayAbility class references, ability grant configuration, runtime item instance fields, replicated equipment state, and save-relevant item or progression state. State units, valid ranges, authority, replication, save behavior, and whether UI only reads the value.

## Item architecture model

Keep these concepts separate: static item definition, concrete item instance, physical inventory location, equipped runtime projection and hand selection, ability-binding presets, and presentation state. Presets describe a target configuration; they are never the final runtime truth of what is equipped.

Inventory owns and supplies item instances. Equipment owns equip rules and combat activation. Flag any implementation that bypasses item instances where instance identity matters, especially for generated loot, affixes, rune sockets, upgrades, durability, mastery-relevant traits, and instance-specific granted abilities or effects.

## GAS aggregation model

Combat behavior that depends on the equipped setup flows through GAS, not hard-coded pawn branches. Aggregate from all currently equipped item instances: granted abilities, granted gameplay effects, loose gameplay tags, gameplay cues, source objects, and ability input bindings.

`InputTag.Weapon.Primary` and `InputTag.Weapon.Secondary` route through the active equipment context, slot occupancy, and gameplay tags instead of per-weapon-class branches.

## GameFeature boundaries

Use GameFeature plugins for modular gameplay packages without overcomplicating early prototypes.

- Existing under `Plugins/GameFeatures`: `GF_Combat_Core`, `GF_Combat_Magic`, `GF_Runes_Core`, `GF_Portals_Core`, `GF_Harvesting_Magic`, `GF_Progression`, `GF_AI_RiftMonsters`, `GF_AI_Wildlife`, and `GF_Dev_Sandbox`.
- Planned in the vision document: `GF_Combat_Ranged`, `GF_Runes_Elemental`, `GF_Runes_Rift`, `GF_Dungeonbreak_System`, and `GF_WorldState`. Create one only when a slice needs it.

For early testing, simple prototype actors and effects (for example a viewport-placed damage-area test actor or `GE_Damage_Instant`) may live in Core when GameFeature placement blocks fast iteration. Mark them as prototype or base assets and refactor later.

Put generic reusable logic in Core or `GF_Combat_Core`: base damage area, base melee attack flow, generic instant damage, generic damage-over-time base, generic stamina cost, and generic hit reaction or stagger effects. Put specializations in their owning feature, for example fire areas in `GF_Combat_Magic` or elemental runes, rift corruption in the Dungeonbreak feature, rune procs in rune features, and portal-monster bonuses in portal features. Dependencies flow from specific features to core, never the reverse.

## DamageArea guidance

A generic damage area may live in Core or `GF_Combat_Core`. Its base behavior: overlap detection, target collection, owner/instigator ignore rules, optional team filtering, tick interval, duration, the GameplayEffect class to apply, one-shot versus repeated application, and self-destroy after duration. The base never knows whether it is fire, poison, frost, or rift; it receives data from exposed variables or a DataAsset, for example:

```text
DamageEffectClass, Radius, Duration, TickInterval, bApplyOnce, bIgnoreOwner, TargetFilterTags, BlockedTargetTags
```

Specialized data lives in feature plugins, for example `DA_DamageArea_FireGround` with `GE_Damage_Fire_DOT` in `GF_Combat_Magic`. Spawn damage areas through GameplayAbilities, AnimNotifies or attack windows, portal actors, Dungeonbreak managers, or encounter spawners. Avoid placing feature-owned gameplay actors in main maps unless the map is a feature test map or the dependency is intentional.

## Abilities with montages

Abilities may live in a GameFeature and reference montages in the same feature; the Experience or GameFeature action grants the ability set and input mapping, and the ability plays the montage and drives attack windows, traces, and effects. Core never references feature-only montages; a feature may reference core or shared character assets.

When montage abilities fail, check the skeleton or compatible skeleton, the AnimBP slot node and slot name, the input config loaded by the active Experience, that the ability is granted to the ASC, and that activation tags are not blocked. With the GASP AnimBP, also check the `unreal-gasp-expert` contract for `DefaultSlot`, root-motion extraction, `GetMesh()`, worker-thread tag snapshots, locomotion interruption, and simulated-proxy presentation.

## Weapon and content model

Keep definitions fragment-friendly rather than monolithic weapon classes. Use tags and data for classification: a weapon type tag (melee, ranged, magic), a weapon family tag (sword, axe, spear, bow, staff, shield), and equipment trait tags (block, parry, charge, casting, harvesting, rune socket, utility). Assume item instances may later add behavior through affixes, rune sockets, generated modifiers, upgrades, durability, mastery unlocks, or special source bindings, so never hard-code behavior only by static weapon class when instance data may matter.

## Progression and mastery

Use the hybrid model from `docs/game-vision.md` section 9 and `docs/combat-mastery-reference.md`: use-based growth, automatic unlocks for basics, milestone choices for build identity, and runes and equipment for further shaping. Layered mastery (roughly 70 % general, 20 % category or style, 10 % weapon familiarity) keeps weapon switches from feeling like a reset.

Skill unlocks grant or modify AbilitySets (preferred for active skills), GameplayEffects (persistent passives), GameplayTags (ability upgrades checked at runtime), input bindings, and passive bonuses.

## Combat MVP priority

When in doubt, finish the smallest playable loop before building broad systems: equip a test weapon, trigger a basic attack ability, play the montage, detect the hit, apply `GE_Damage_Instant` or equivalent, play hit reaction or cue, kill the enemy, and grant XP or mastery progress through the progression seam. That loop must leave room for runes, magic, ranged weapons, portal monsters, Dungeonbreak modifiers, skill-tree unlocks, and item-instance modifiers.

## Drift warnings

Flag proposals that:

- treat stale docs or remembered class names as required truth
- create Blueprint subclasses of `URpgInventoryItemFragment` or its semantic subclasses, or add a fragment type for per-item values or composition
- add a concrete native GameplayAbility leaf when a `GA_*` Blueprint asset can express the behavior, or a native intermediate only to share designer flow, shrink graphs, or avoid MCP authoring
- bake concrete ability tags, costs, cooldowns, montages, cues, text, icons, range, or damage into a generic native base
- require a content ability to exist as a native class or create a native leaf for test convenience
- bypass equipment authority, treat presets as equipped-state truth, or put combat truth into UI or character visuals
- couple inventory directly to plugin-only combat types, hard-code weapon families, or block generated items, affixes, runes, or item instances
- build skill trees that make weapon switching feel like starting from zero
- put fire, rift, or rune specifics into generic core assets, or make main maps hard-reference feature-only actors without reason
- make authoritative combat results depend on GASP pose selection or AnimBP-local state
- remove montage slots, notifies, root motion, equipment sockets, death, or ragdoll seams while simplifying the GASP graph
