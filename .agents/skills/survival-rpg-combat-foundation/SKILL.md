---
name: survival-rpg-combat-foundation
description: Combat equipment and item architecture guide for the SurvivalRpg repository. Use when Codex needs to design, review, document, or implement GAS-based equip/loadout, item-instance handling, slot rules, aggregated combat grants, or future inventory, loot, affix, rune, and mastery seams while staying aligned with the existing equipment stack.
---

# SurvivalRpg Combat Foundation

Start from the current repository truth.

- Read `docs/game-vision.md` first.
- Read `docs/combat/phase-1-weapon-equip-foundation.md` next.
- Inspect `Source/SurvivalRpg/Equipment`, `Source/SurvivalRpg/AbilitySystem`, `Source/SurvivalRpg/Core/Character`, and any active inventory-facing code before proposing changes.

Preserve the architecture boundaries.

- Treat the equipped object conceptually as an item instance, even if the current implementation still uses simplified runtime containers plus `SourceItemHandle`.
- Keep `URpgEquipmentComponent` as the authority for equipped state, slot conflict resolution, aggregated combat grants, granted gameplay effects, replicated equipped items, and equipment-driven loose tags.
- Keep equip input abilities such as `URpgGameplayAbility_EquipLoadoutSlot` as thin entry points that map input to equip operations and delegate state changes to the equipment system.
- Keep `ARpgCharacter` and UI presentation-only: they mirror equipment state but do not own combat truth.
- Keep `UWeaponManager` deferred unless a later task gives it a narrow responsibility that does not overlap with equipment authority.

Use the item architecture model.

- Separate static item definitions, concrete item instances, equipped runtime state, loadout presets, and presentation state.
- Treat loadouts as presets that describe a target configuration, never as the source of truth for what is actually equipped.
- Keep inventory as the owner and supplier of item instances, but not the owner of equip or combat rules.
- Keep `SourceItemHandle` or an equivalent resolver as the inventory-agnostic seam used to resolve a concrete item instance.

Follow the GAS aggregation model.

- Bind combat logic that depends on equipped setup through GAS instead of hard-wiring it on the pawn.
- Aggregate ability grants, gameplay effects, tags, and source bindings from all currently equipped item instances.
- Route `InputTag.Weapon.Primary` and `InputTag.Weapon.Secondary` through the active equipment context, slot occupancy, and gameplay tags rather than through hard branches per class.
- Preserve server authority for equip, unequip, grant rebuilds, and conflict resolution.

Keep the content model fragment-friendly.

- Keep definitions readable as modular fragments or fragment-like data blocks instead of monolithic weapon classes.
- Use `WeaponTypeTag` for broad melee/ranged/magic classification.
- Use `WeaponFamilyTag` for mastery, skill trees, unlocks, and family-specific mechanics.
- Use `EquipmentTraitTags` and future instance-driven trait data for blocking, parry, charge, casting, harvesting, runes, utility, and similar modular behavior.
- Assume future loot, affixes, rune sockets, generated modifiers, and upgrades may contribute instance-specific combat behavior.

Flag drift early.

- Flag combat work that bypasses item instances and relies only on static definitions when instance identity matters.
- Flag loadout-driven logic that starts acting as runtime truth.
- Flag inventory-driven combat logic that couples directly to plugin-only types.
- Flag solutions that move combat state into widgets, character visuals, or duplicate managers.
- Flag proposals that block future generated items, affixes, runes, or fragment-based inventory integration.
