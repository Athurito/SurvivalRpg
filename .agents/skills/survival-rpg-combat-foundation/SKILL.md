---
name: survival-rpg-combat-foundation
description: Use for SurvivalRpg combat, equipment, inventory-facing item instances, loadouts, GAS ability grants, mastery/progression, runes, portal combat, Dungeonbreak, and modular GameFeature combat content. Prefer current repo truth and pair with $unreal-lyra-expert for engine-facing implementation.
---

# SurvivalRpg Combat Foundation

## Skill Boundaries

This skill owns SurvivalRpg combat, equipment, inventory-facing item instances, weapon action routing, runes, mastery/progression, portal combat, and combat GameFeature guidance.

Use it together with `$unreal-lyra-expert` when implementation details touch Unreal Engine, GAS internals, replication, Experiences, Game Features, Lyra Interaction, or Lyra-rooted inventory/equipment.
Use it together with `$survival-rpg-project` when a combat or equipment decision changes product scope, progression identity, survival friction, or first-playable priorities.

Do not let this skill create a parallel combat/equipment authority beside the existing Lyra-rooted RPG architecture.

## Always start from current repository truth

Before proposing changes, Codex should inspect the current repository instead of relying on stale document names.

Read first, if present:

- `docs/game-vision.md`
- `docs/combat/README.md`
- `docs/combat/*.md`
- `docs/progression/*.md`
- `docs/equipment/*.md`

Then inspect current code paths that actually exist, especially:

- `Source/SurvivalRpg/Equipment`
- `Source/SurvivalRpg/AbilitySystem`
- `Source/SurvivalRpg/Core/Character`
- inventory-facing code
- progression / experience / mastery code
- active Lyra Experience and GameFeature setup

Do **not** assume `docs/combat/phase-1-weapon-equip-foundation.md` exists. If a referenced doc is missing, flag it as stale and continue from existing code and current vision docs.

---

# Current game direction

The project is a **dark fantasy open-world survival action RPG** with a strong focus on:

- responsive third-person combat
- weapon, magic and rune builds
- portal encounters and Dungeonbreak escalation
- magical harvesting instead of only axe/pickaxe gathering
- use-based progression with meaningful specialization choices
- dynamic world threat and replayability

The combat architecture should support the long-term fantasy of a player becoming a powerful portal hunter / rune warrior in a living world affected by portals.

---

# Core combat philosophy

Combat should be:

- direct and readable
- skill-based without becoming a pure Soulslike
- built around timing, positioning and build synergy
- driven by GAS abilities, effects, cues and tags
- modular enough to support melee, ranged, magic, runes and portal-specific mechanics

Core combat should prove:

- attack input triggers the correct ability
- montage / animation plays reliably
- hit detection is authoritative and readable
- damage is applied through GameplayEffects
- hit reactions, stagger and death work
- equipment can grant and remove combat capabilities
- progression can unlock or modify abilities cleanly

---

# Architecture boundaries

## Equipment authority

Keep `URpgEquipmentComponent` or the current equivalent equipment component as the authority for:

- equipped state
- slot conflict resolution
- replicated equipped items
- equipment-driven loose tags
- aggregated ability grants
- aggregated gameplay effects
- active equipment context
- source bindings for attacks and abilities

Equip input abilities such as `URpgGameplayAbility_EquipLoadoutSlot` should stay thin. They should map input to equip requests and delegate actual state changes to the equipment system.

Characters and UI should mirror equipment state for presentation only. They should not own combat truth.

Avoid duplicate managers that overlap with equipment authority. A future weapon manager is only acceptable if it has a narrow role that does not conflict with equipped-state ownership.

---

# Item architecture model

Keep these concepts separate:

- static item definition
- concrete item instance
- equipped runtime state
- loadout preset
- presentation state

Loadouts are presets that describe a target configuration. They are never the final runtime truth of what is equipped.

Inventory owns and supplies item instances. Equipment owns equip rules and combat activation.

Keep `SourceItemHandle` or an equivalent resolver as the inventory-agnostic seam for resolving concrete item instances.

Flag any implementation that bypasses item instances when instance identity matters, especially for:

- generated loot
- affixes
- rune sockets
- upgrades
- durability
- mastery-relevant traits
- instance-specific granted abilities or effects

---

# GAS aggregation model

Combat behavior that depends on equipped setup should flow through GAS, not hard-coded pawn branches.

Aggregate from all currently equipped item instances:

- granted abilities
- granted gameplay effects
- loose gameplay tags
- gameplay cues
- source objects / source handles
- ability input bindings

`InputTag.Weapon.Primary` and `InputTag.Weapon.Secondary` should route through active equipment context, slot occupancy and gameplay tags instead of hard branches per weapon class.

Server authority should own:

- equip / unequip
- grant rebuilds
- conflict resolution
- replicated equipped state
- authoritative damage application

---

# GameFeature boundaries

Use GameFeature plugins for modular gameplay packages, but do not overcomplicate early prototypes.

Recommended long-term features:

- `GF_Combat_Core`
- `GF_Combat_Magic`
- `GF_Combat_Ranged`
- `GF_Runes_Core`
- `GF_Runes_Elemental`
- `GF_Runes_Rift`
- `GF_Portals_Core`
- `GF_Dungeonbreak_System`
- `GF_Harvesting_Magic`
- `GF_Progression`
- `GF_AI_RiftMonsters`

For early testing, it is acceptable to keep simple prototype actors such as a viewport-placed `BP_DamageArea_Test` or `GE_Damage_Instant` in Survival/Core if GameFeature placement prevents fast iteration. Mark these as prototype or base assets and refactor later.

## Core vs feature rule

Put generic reusable logic in Core or `GF_Combat_Core`:

- base damage area
- base melee attack flow
- generic instant damage effect
- generic damage-over-time base
- generic stamina cost effect
- generic hit reaction / stagger effects

Put specializations in their owning feature:

- fire damage area -> `GF_Combat_Magic` or `GF_Runes_Elemental`
- frost / shock / burn effects -> magic or elemental runes
- rift corruption area -> `GF_Dungeonbreak_System`
- rune weapon procs -> rune features
- portal monster bonuses -> rift/portal features

Dependencies should flow from specific features to core, not from core to specific features.

---

# DamageArea guidance

A generic damage area can live in Core or `GF_Combat_Core` if it is broadly reusable.

Recommended base behavior:

- collision / overlap detection
- target collection
- owner / instigator ignore rules
- optional team filtering
- tick interval
- duration
- GameplayEffect class to apply
- optional one-shot vs repeated application
- self-destroy after duration

The base class should not know whether it is fire, poison, frost or rift. It should receive data from exposed variables or a DataAsset.

Example base fields:

```text
DamageEffectClass
Radius
Duration
TickInterval
bApplyOnce
bIgnoreOwner
TargetFilterTags
BlockedTargetTags
```

Specialized data or child assets can live in feature plugins:

```text
GF_Combat_Magic
  DA_DamageArea_FireGround
  GE_Damage_Fire_DOT

GF_Dungeonbreak_System
  DA_DamageArea_RiftCorruption
  GE_RiftCorruption_DOT
```

Spawn damage areas through:

- GameplayAbilities
- AnimNotifies / attack windows
- portal actors
- Dungeonbreak managers
- encounter spawners

Avoid directly placing feature-owned gameplay actors in main maps unless the map is a feature test map or the dependency is intentional.

---

# Abilities with montages

Abilities may live in a GameFeature and reference montages in the same GameFeature.

Example:

```text
GF_Combat_Core
  GA_MeleeAttack
  AM_MeleeAttack_Light
  LAS_Combat_Core
  IA_Attack
  IMC_Combat
```

The Experience or GameFeature action grants the ability set and input mapping. The ability plays the montage and drives attack windows / traces / effects.

Check these when montage abilities fail:

- correct skeleton / compatible skeleton
- AnimBP contains the required Slot node
- montage slot name matches ability expectations
- input config is loaded by the active Experience
- ability is actually granted to the ASC
- activation tags are not blocked

Core should not reference feature-only montages. A feature can reference core/shared character assets.

---

# Weapon and content model

Keep definitions fragment-friendly rather than monolithic weapon classes.

Use tags/data for broad classification and future progression:

- `WeaponTypeTag`: broad type such as melee, ranged, magic
- `WeaponFamilyTag`: sword, axe, spear, bow, staff, shield, etc.
- `EquipmentTraitTags`: block, parry, charge, casting, harvesting, rune socket, utility, etc.

Assume future item instances may add behavior through:

- affixes
- rune sockets
- generated modifiers
- upgrades
- durability
- mastery unlocks
- special source bindings

Do not hard-code weapon behavior only by static weapon class if instance data may matter.

---

# Progression and mastery model

The current preferred model is **hybrid progression**:

- use-based leveling for natural growth
- automatic unlocks for basic capabilities
- milestone choices for build identity
- runes and equipment for additional build shaping

Avoid hard-reset frustration when changing weapons.

Use layered mastery instead of isolated per-weapon grind:

```text
70% general mastery
20% category/style mastery
10% weapon familiarity
```

Example melee progression:

```text
Melee Mastery
  general attacks, stamina control, combat fundamentals

One-Handed / Two-Handed / Shield / Dual Wield
  style-specific skills and modifiers

Sword / Axe / Spear Familiarity
  small family bonuses and flavor skills
```

Changing from sword to axe should keep `Melee Mastery` and `One-Handed` progress. Only small familiarity bonuses should differ.

Skill unlocks should grant or modify:

- AbilitySets
- GameplayEffects
- GameplayTags
- input bindings
- passive bonuses
- ability upgrade tags

Prefer AbilitySets for active skill unlocks.

Passive skills can apply long-duration or persistent GameplayEffects.

Ability upgrades can be represented by GameplayTags that the ability checks at runtime.

---

# Combat MVP priority

When in doubt, prioritize the smallest playable combat loop:

1. equip a test weapon
2. trigger a basic attack ability
3. play montage
4. detect hit
5. apply `GE_Damage_Instant` or equivalent
6. play hit reaction / cue
7. kill enemy
8. grant XP / mastery progress through progression seam

Do not build broad systems before this feels good.

The first combat milestone should support later:

- runes
- magic
- ranged weapons
- portal monsters
- dungeonbreak modifiers
- skill tree unlocks
- item instance modifiers

---

# Drift warnings

Flag proposals that:

- reference stale docs as required truth
- bypass equipment authority for equip state
- make loadouts runtime truth
- put combat truth into UI or character visuals
- couple inventory directly to plugin-only combat types
- hard-code weapon families where tags/data should be used
- block future generated items, affixes, runes or item instances
- implement skill trees that make weapon switching feel like starting from zero
- put special fire/rift/rune logic into generic core assets
- require main maps to hard-reference feature-only actors without clear reason
