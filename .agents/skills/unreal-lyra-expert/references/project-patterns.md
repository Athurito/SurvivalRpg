# Lyra-Derived Project Patterns

Project-specific conventions only; general Unreal knowledge is assumed.

## Contents

- Adopted systems and how to extend them
- Not adopted without evidence or request
- Decision order
- Anti-patterns to flag
- Verification by area

## Adopted systems and how to extend them

This project uses Lyra as a proven baseline, not as a sample to clone. Extend the adopted system instead of adding a parallel manager, pawn-owned state path, or widget-owned gameplay truth.

| System | Use for | Extend through | Flag |
| --- | --- | --- | --- |
| Lifecycle / init state | Features with initialization dependencies | Modular component init-state progression | `BeginPlay` ordering chains and hidden startup side effects |
| Experiences | Mode rules, pawn setup, ability and action grants, mode input, Game Feature activation | Experience and action-set assets | Custom mode-composition managers; avoiding Experiences because they are "Lyra-specific" |
| Game Features | Modular gameplay slices and optional content | One plugin per feature with narrow dependencies | Moving feature logic into the core module for convenience |
| GAS | Capabilities, costs, cooldowns, blockers, combat and status state | Abilities, effects, attributes, gameplay tags | Parallel cooldown or state managers; booleans that duplicate tags |
| PawnData / AbilitySets | Pawn class, input setup, granted data, pawn defaults | Data assets | A second pawn setup path |
| Attribute sets | Health-like state | HealthSet-style attribute ownership | Health or damage logic spread across pawn, controller, UI, and helpers |
| Interaction (adopted ~1:1) | Pickups, containers, crafting stations, harvest nodes, doors, NPCs, vendors, loot | Interaction options with tag, ability, or item gating | One-off trace, prompt, and execute logic per interactable type |
| Inventory / Equipment (Lyra root plus RPG layer) | Items, equipment, equipment grants | Definitions and native fragments, item and equipment instances, FastArray replication with intentional `OnRep`, server-authoritative mutation, save data as reconstruction source | Parallel inventory managers, pawn-owned item arrays, widget-owned equipment truth, reverting RPG code to Lyra sample behavior |
| CommonUI | Screens, layers, focus and input routing | Activatable Widget Blueprints with MVVM | Ad hoc widget stacks; gameplay truth in widgets |
| Enhanced Input | Gameplay input | Input configs and mapping contexts granted by Experience, PawnData, or Game Feature | Hard-coded bindings; gameplay input in widgets |
| Enemy AI | Enemy behavior | A StateTree on `URpgAIPawnData`, composed of the `Rpg|AI` StateTree nodes under the combat control task; the AI drivers own the target and press the ability inputs | Behavior on driver timers or in Blueprints; tasks that set focus or stop movement on their own |

Predicted presentation of a server-decided outcome follows the CMB-04 impact cues ([hit feedback](../../../../docs/combat-hit-feedback.md)). The remote owner plays its local copy and records it. The server's copy reaches every machine, and a native notify on the owner absorbs it only when a matching recent prediction exists (`RpgCombatImpact`, `URpgGameplayCueNotify_Impact`). Do not use the owner's activation prediction key to skip the server's copy. GAS then hides the confirmation whenever the owner's prediction missed, and a key from a listen-server host or an AI reaches every connection, so every client skips it.

Montage-timed swing traces, melee attacks and tool swings alike, run through `FRpgSwingTraceWindow` (`Source/SurvivalRpg/Combat/RpgSwingTraceWindow.h`, HRW-02): the window contract, the authority schedule, the sampling of the equipment's trace points, the arc sub-steps and the ribbon traces. The owning ability keeps hit de-duplication, damage or harvest commits and swing records in its `FRpgSwingTraceSink`. A hit callback may end the ability, so `EndAbility` closes and resets the window from inside the window's own trace loop; the window keeps its owner and sink through `Reset` and stops tracing once closed, and a sink must tolerate the same re-entry. `UGameplayAbility::EndAbility` clears every timer bound to the ability (`AbilitySystem.ClearAbilityTimers`), so the window binds its timers weakly to the ability, never to the equipment instance.

RPG adaptations layered on the inventory/equipment root include rarity, item level, affixes, sockets, durability, class or stat requirements, generated loot, crafting data, vendor and economy data, stat aggregation, progression scaling, and persistence mapping.

AI movement looks jerky when it stops dead, which `AAIController::MoveTo` does more often than it seems (CMB-06, [enemy AI](../../../../docs/combat-enemy-ai.md)):
- **Immediate finish:** a request that fails, for example on a goal off the navmesh, or that counts as already reached, measured with the capsule radius added, finishes at once and zeroes the velocity. So does every `StopMovement`.
- **Stop on finish:** every abort sets the path following's stop-on-finish flag again, so a reached goal stops dead after the next stop.
- **The fix:** route combat moves through `RequestCombatMove` in `RpgStateTreeCombatTasks.cpp`. It projects the goal first, skips goals closer than 40 cm, tests the reach without the capsule, and clears stop-on-finish with every request.
- **Ownership:** the global combat control task holds the focus for all combat states, and tasks leave the movement to the next state instead of stopping on exit.

Native widgets that place authored entries outside a list view (`URpgViewModelEntryBox`, `URpgSkillTreeGridWidget`, `URpgSkillTreeMapWidget`; MST-01, [mastery map foundation](../../../../docs/mastery-map-foundation.md)):
- **Item handoff:** `IUserObjectListEntry::Execute_OnListItemObjectSet` reaches only Blueprint implementations. A native `NativeOnListItemObjectSet` override, as in `URpgMvvmListEntryWidget`, needs `SetEntryItem`. Hand items over the way `URpgViewModelEntryBox::AppendEntry` does.
- **Entry creation:** `CreateWidget(UWidget*, ...)` finds its owner through the widget's `UWidgetTree`; for a widget outside one it ensures and returns null. Tests build the host widget's tree and use `WidgetTree->ConstructWidget`.
- **Drag surfaces:** take presses in `OnPreviewMouseButtonDown`, so an entry's button never clicks at the end of a drag. Slate releases mouse capture and calls `OnMouseCaptureLost` when window activation changes or the captor leaves the visible hierarchy, for example when its screen closes. End gestures there.

## Not adopted without evidence or request

- Lyra subsystems not already present in this project
- Lyra spawning or possession changes outside the adopted Experience/PawnData path
- Shooter-specific assumptions that do not fit an RPG
- Lyra sample UI flows that conflict with the RPG UI
- Lyra sample inventory/equipment behavior that would remove the RPG item model
- New framework layers, singleton managers, or plugin boundaries not aligned with the current structure

Name a Lyra concept only to anchor a recommendation, never as "Lyra does X, so do X".

## Decision order

1. Can this stay a small local feature?
2. Does it belong in the lifecycle / init-state flow?
3. Should an Experience configure or activate it?
4. Does it belong in an existing or new Game Feature plugin?
5. Is it a GAS concern, and should its state live in attributes, effects, or tags?
6. Should PawnData or AbilitySets configure it?
7. For world interaction: does the adopted Interaction path fit?
8. For items or equipment: does it extend the Lyra-rooted RPG inventory/equipment architecture?
9. For UI: does it fit CommonUI screen flow with gameplay truth outside widgets?

If none apply, use normal Unreal patterns and keep the solution simple.

## Anti-patterns to flag

- Bypassing an adopted system with a local framework (custom mode manager, ad hoc plugin activation, one-off interaction traces, pawn-owned inventory arrays)
- Recommending a Lyra subsystem only because Lyra has it
- Lifecycle bypass through implicit ordering or manual cross-object assumptions
- GAS duplication through custom cooldowns, combat-state managers, or shadow booleans
- Scattered health ownership
- Forking inventory or equipment beside the Lyra-rooted RPG path, or monolithic item classes where fragment composition fits
- Widget-driven gameplay or widget-owned authoritative state
- Replicated convenience variables instead of authoritative reconstructed state
- Undocumented designer-facing configuration

## Verification by area

- Inventory/equipment: server mutation, replicated item state, equipment grant lifecycle, ability and effect removal, persistence reconstruction, UI refresh.
- Interaction: authority handoff, prompt ownership, target lifetime safety, tag and ability gating.
- Experiences and Game Features: activation and deactivation, plugin dependencies, data asset registration, load and cook implications.
- Blueprint-facing APIs: reflection macros, categories, metadata, null safety.
- Asset-driven systems: soft references, Primary Asset usage, config registration, cook and load implications.
- Replicated gameplay: authority path, replicated state, `OnRep` side effects, late join, prediction assumptions.
- Enemy AI movement: sample speed, yaw and the AnimBP's Direction in PIE at 30 Hz or more, with the player standing and moving. Count hard stops (above 100 to below 30 cm/s in one sample outside montages), gait switches and turns faster than 400°/s before judging feel; a sample every 0.3 s hides the stutter.
