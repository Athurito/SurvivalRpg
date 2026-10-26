# Ranged combat roadmap

Updated: 2026-10-26. Status: **Deferred; implementation start not scheduled**.

This plan records the agreed first bow system: a player bow and a bow enemy using shared combat mechanisms, including visibly embedded arrows, breakage, and partial ammunition recovery. It also defines the extension boundary for later magic and abilities such as Rain of Arrows. This is a design and implementation handoff, not a report of implemented or tested gameplay.

## Scheduling and scope

- Ranged combat is an independent workstream with its own implementation start; this plan has no prerequisite to finish another roadmap.
- Preserve the scheduling of other workstreams, including the [mastery map roadmap](mastery-map-roadmap.md). This document does not select when ranged implementation starts.
- Every RNG task below remains Deferred until the user selects its start. Use one bounded task per branch/PR, named `codex/rng-<id>-<slug>` or `claude/rng-<id>-<slug>`.
- The first delivery is one bow, one standard arrow type, and one bow enemy. Embedded arrows and recovery are required for that delivery, not later polish.
- The player presentation targets CMC/Manny. Do not incidentally add a Mover bow presentation; regression-check shared mechanisms on Mover.
- RNG-01 is the only projectile foundation for arrows and spells: the "Projectile foundation" listed under "Later" in the [combat roadmap](combat-roadmap.md). There is no second, spell- or arrow-specific projectile. If combat magic needs a projectile before ranged work starts, implement RNG-01 from this roadmap first.
- Magic gameplay, Rain of Arrows, special ammunition, homing, penetration, headshot bonuses, and a full Archery skill tree remain later work.

Bow combat extends the [game vision](game-vision.md) beyond the original sword-and-shield first-playable minimum. Deliver this bounded loop before expanding the weapon or spell catalog.

## Player experience

### Aiming and firing

- Use free third-person shoulder aiming with an ordinary crosshair, arrow travel time, and gravity.
- Holding primary attack automatically enters the aim stance and draws the bow. Releasing it fires.
- Holding secondary aim allows aiming before drawing and preserves the aim stance between shots.
- After a short nocking phase, an early release fires a weaker arrow. Configurable draw curves determine damage and launch speed. Releasing before the minimum nocking time cancels without an arrow or release cost.
- The camera supplies aiming intent, while the arrow launches from the bow. Nearby walls and other obstructions must block the actual shot even when the camera can see around them.
- Normal hits use the existing damage, critical-hit, armor, stagger, faction, and dodge rules. There are no damage zones in this slice.

### No trajectory preview

**The bow must never display a predicted flight path or landing point.**

Do not add a white trajectory tube, curved aiming ribbon, arc, dotted path, projected arrow flight, predicted impact marker, or a ballistic correction of the crosshair. The player estimates distance, arrow drop, and lead. The only bow aiming aids are the ordinary crosshair and a draw-strength display. The latter describes draw progress, not where the arrow will land.

Internal aiming and collision calculations do not authorize a player-visible trajectory visualization. Acceptance explicitly checks its absence.

### Stamina, movement, and interruption

- Make draw duration, release stamina cost, optional hold drain, and movement while aiming configurable and adjustable by passive skills.
- The starter profile uses a stamina cost on release, no hold drain, and slow movement while aiming. These are content defaults, not hard-coded rules.
- Primary draw and held secondary aim have separate activation tokens. Ending either must not clear the other; one effective camera owner reflects the combined aim state.
- Dodge, sprint, jump/traversal, stagger, death, equipment removal, and avatar teardown cancel an unfinished shot. Require fresh input after interruption rather than restarting from a still-held button.
- Exhaustion during configured hold drain lowers the bow and cancels; it never automatically fires. Already paid hold drain is not refunded.
- Resolved tuning is fixed for the activation. Passive changes during that activation apply to the next one, avoiding different owner/server timing rules.
- A released arrow survives subsequent weapon switching, shooter death, and the end of its ability.

### Defense

Shield block works against ordinary arrows within the block angle, using configured damage reduction and stamina cost. Perfect Block prevents the arrow's damage and costs less stamina, but does not reflect the arrow or apply posture damage to the distant shooter.

Use the incoming arrow direction for the block test, not the shooter's current position. Preserve existing melee defense behavior. A blocked intact arrow drops as a recoverable world pickup on the ground near the contact, never attached to the blocker; it does not continue as a reflected damaging projectile. An arrow consumed on impact leaves no pickup.

## Embedded arrows and recovery

### Impact recovery policy

Each ammunition or projectile payload selects one impact recovery policy:

| Policy | Impact behavior | Use |
| --- | --- | --- |
| Recoverable | One server break roll; an intact arrow may embed and be recovered | Standard arrow |
| Consumed on impact | No break roll, no embedding, no pickup and no recovery credit; the impact cue shows the arrow burning, shattering, or dissolving | Elemental and other magic arrows, every spell projectile |

The first delivery authors only the standard arrow, but RNG-03b implements both policies, so later special arrows need no new mechanism. A consumed-on-impact NPC shot reserves no recovery token.

### Impact result and presentation

- The server resolves intact versus broken exactly once at the impact of a recoverable arrow. Arrow configuration, the hit surface, and snapshotted passive tuning determine the break chance.
- This is a probability per arrow, not a guaranteed recovery quota. A short sequence can yield no intact arrows.
- Replication, loot retries, and collection never reroll the result.
- Intact arrows remain visibly embedded in suitable world surfaces. They follow moving hit components rather than remaining at a stale world transform.
- **Player characters never keep arrows.** A hit on a player plays only the impact feedback; the arrow is gone and creates no attachment, pickup, or recovery credit. This covers hostile and co-op hits, downed players, death, and respawn, so players never walk around with dozens of arrows in them.
- Only AI characters, which own a loot source and a corpse lifecycle, embed arrows. On them, retain the hit component, bone, and bone-local transform. Obtain a skeletal contact or refine a capsule contact; attaching every arrow to the character root would visibly float.
- Arrows follow animation and each client's local ragdoll pose. Bone selection is presentation and does not introduce headshot damage.
- Broken arrows grant no ammunition. A short broken-shaft or impact effect may remain, then expire.

### Embedded-arrow budget

Embedded arrows are bounded so long fights cannot flood a target or the world with attachments and replicated actors. Grouping changes only presentation and actor count; it never loses or duplicates recoverable quantity.

- **On an AI character:** at most `MaxVisibleArrowsPerCharacter` shafts are visible. A further intact arrow joins the nearest visible group by bone and raises that group's count instead of adding a shaft. Corpse looting retires groups in credit order as the aggregate quantity decreases; a group's shaft disappears when its count reaches zero.
- **In the world:** a hit component holds at most `MaxSeparatePickupsPerHost` separate pickups. Once it is full, a further intact arrow joins the nearest pickup of the same ammunition on that component within `GroupRadius` and raises its stack without a new actor or shaft; the group keeps the later of both recovery deadlines. With no group in range, the arrow gets its own pickup.
- **Hard world cap:** at most `MaxWorldArrowPickups` temporary arrow pickups exist per world. A new pickup beyond the cap expires the oldest one early, exactly like its normal expiry.
- The interaction prompt shows a group's quantity. Collecting a group transfers its stack through the same atomic path as a single arrow; a full inventory leaves the remainder in the group.

A hundred arrows in one enemy therefore appear as at most ten shafts that together carry every intact arrow. Normal play should rarely reach these limits; they are a safety net, not a target.

### Collection

- Collect intact world arrows manually through the existing Lyra Interaction path.
- Arrows lodged in living enemies cannot be extracted yet. On death, recoverable arrows become part of the corpse's ordinary loot.
- Remove the corresponding embedded visuals or group counts as the arrows are looted, including partial collection.
- A full player inventory leaves the uncollected arrows available. Concurrent players cannot receive the same arrow twice.
- Recovered standard arrows are normal stackable ammunition. This slice does not track persistent per-arrow durability.

### Suggested initial tuning

The user selected configurable breakage, temporary recovery, a finite NPC recovery budget, no arrows on players, grouping of embedded arrows beyond a budget, and no embedding for magic arrows. The numbers below are suggested starting values for playtesting, not user-specified balance requirements.

| Setting | Suggested starter value |
| --- | --- |
| Base break chance | 30%, modified by authored surface rules and passive tuning |
| World-arrow recovery lifetime | 10 minutes after impact |
| NPC recovery budget | Five recovery-eligible successful shots per enemy life |
| `MaxVisibleArrowsPerCharacter` | 10 |
| `MaxSeparatePickupsPerHost` | 10 |
| `GroupRadius` | 1 m |
| `MaxWorldArrowPickups` | 100 per world |

NPCs can keep firing after their recovery budget is exhausted. Reserve a recovery eligibility token at a successful authoritative shot commit; failed commits consume none. Once fired, a token is not refunded by breakage, a miss, collection, or expiry. This gives an upper bound on recoverable NPC arrows without requiring NPC combat inventories. Carry eligibility in the shot snapshot so the shooter's death cannot reset or invalidate it. Target changes, disengagement, loadout refreshes, and equipment changes do not refill the budget.

Later NPC arrows remain visually usable for combat feedback but grant no recovery item. Player arrows instead derive eligibility from an actually consumed inventory arrow. Missing inventory must never silently select unlimited ammunition.

### Lifetime and persistence

- Embedded arrows and their uncollected recovery state are temporary, session-only combat objects. Only successfully collected items enter normal player or storage persistence.
- The arrow recovery deadline never extends its host corpse's lifetime. The inspected corpse implementation has a 120-second hard-lifetime default; confirm that value again before implementation.
- On corpse expiry/destruction, remove remaining embedded visuals and their recovery state/items. Do not recreate a second world drop from expired corpse arrows.
- If a non-character attachment host disappears, detach the existing world pickup at its last valid world transform for the remainder of its original lifetime. Do not duplicate the payload or restart the timer.
- A character that disappears without a corpse transition loses its pending embedded-arrow state. Map teardown and feature deactivation clean up temporary actors and records.

## Architecture and ownership

The shared flow is:

```text
Player input / AI target choice
    -> validated aiming and release intent
    -> authoritative shot commit
    -> projectile movement and collision
    -> GameplayEffect damage and GameplayCue feedback
    -> optional embedded representation and ammunition recovery
```

Target choice, delivery, and effect payload remain separate. Player and enemy bows use the same shot ability and mechanisms; only their input/target source and configured ammunition policy differ. A later spell can use the projectile mechanism without a weapon or inventory.

### C++ boundary decision

- **Classification:** reusable native schemas and mechanisms for authority, replication, prediction, lifecycle, and exactly-once resource/recovery commits; designer-owned concrete combat content.
- **Runtime truth:** GAS and the server own activation, resource costs, and damage. Inventory owns physical ammunition and committed loot. Equipment owns equipped state, source binding, and grants. Existing loot sources own pending corpse recovery until materialization.
- **Existing seams:** `URpgRangedWeaponInstance`, `URpgGameplayAbility_FromEquipment`, `GetTunedValue`, the ASC, `RpgDamageExecution`, inventory transfers, `URpgLootSourceComponent`, `ARpgDroppedInventoryActor`, AI drivers, movement, and CommonUI/MVVM.
- **Ownership:** C++ owns validated intent, shot/recovery lifecycle, native configuration structures, and replicated read state. Blueprint/DataAsset content owns bow identity, sequencing, curves, montage/cue selection, tuning, presentation, and AI behavior composition.
- **New native classes:** five planned classes, listed below. Names are proposed and must be reconciled with the actual implementation baseline; reuse an existing equivalent rather than duplicating it.
- **Asset work:** use Unreal MCP to inspect, author, compile, save, and validate concrete Blueprint, animation, equipment, effect, cue, and WBP assets. No native ability leaf substitutes for asset authoring.

| Proposed class | Native responsibility |
| --- | --- |
| `ARpgCombatProjectile` | The shared projectile for arrows and spells: replicated movement/collision, retained launch payload and attribution, impact recovery policy, one authoritative impact, and teardown. It carries flight presentation per the [fireball carrier contract](vfx-fireball.md#flight-system) and executes the impact cue on authority through the instigator's ability system. It does not require equipment or arrow-specific content. |
| `URpgAbilityTask_ProjectileShot` | Activation-bound validation of aiming/release data, charge duration, optional hold-resource drain, exactly-once release/payment/spawn, and cancellation cleanup. It accepts trusted configuration from an ability without requiring an equipment ability. |
| `ARpgEmbeddedProjectilePickup` | Reusable attachment, grouping, expiry, and world recovery using the existing dropped-inventory foundation. On characters it is presentation for separately owned corpse recovery, never a second loot inventory for the same arrow. |
| `URpgEmbeddedProjectileSubsystem` | Server-side world subsystem that only orders temporary arrow pickups and enforces `MaxWorldArrowPickups`. It owns no inventory, recovery, or combat state. |
| `URpgRangedCombatViewModel` | Read-only local-player aim, draw fraction, ammunition count, and shot availability; lifecycle-safe binding across equipment/avatar changes. |

Extend existing types with native shot/recovery configuration structures, source snapshots, projectile defense, aim activation tokens, and the required movement/network state. Do not grow the melee trace/combo definition into a universal attack definition. No new native GameplayAbility leaf or separate combat/inventory/skill manager is planned.

### Content and feature placement

- Put the bow, standard arrow, `GA_BowAim`, `GA_BowShot`, equipment/AbilitySet content, bow camera, animation, and HUD assets in the appropriate `GF_Combat_Ranged` content boundaries.
- Keep shared native mechanisms in the existing project runtime and generic shared effects/cues in Combat Core. `GF_Combat_Ranged` depends on Core; Core never depends on ranged or magic content.
- Keep enemy archetype and StateTree composition with the existing enemy feature, depending on ranged content where required. Activate dependencies through Experiences/Game Features.
- The first bow enemy reuses the Manny-native skeleton body of the CMB-05 Rift Grunt ([skeleton warrior](combat-skeleton-warrior.md)) with a bow. The asset library's `SkeletonArmy` archer uses a UE4 skeleton and waits for the UE4-enemy pipeline pilot listed under "Later" in the [combat roadmap](combat-roadmap.md).
- Sandbox recipes and test maps may reference production ranged items. Production ranged content must not depend on `GF_Dev_Sandbox`.
- The asset library contains an Archer animation pack, but skeleton compatibility and visual quality have not been validated. Inspect it before migrating only the required assets, record pack attribution, and preserve the project-owned Manny skeleton and montage/layer contracts.

### Configuration and passives

Use the existing ability skill-tuning resolver around `GetTunedValue` for draw time, release cost, optional hold drain, aim movement, and break chance. Keep damage and launch-speed curves authored in the bow's content. Resource mutations remain GAS-owned.

The inspected tuning resolver discovers an equipment item's skill tree; NPC equipment currently can lack an item instance and then uses base tuning. Do not assume it already supports arbitrary global buffs or a future magic progression source. Re-inspect the current equipment and progression contracts before extending that resolver. Prove passive adjustments with focused test content rather than adding a production Archery tree.

Aim movement must use the real movement mechanism and its prediction snapshots. The inspected `URpgMobilitySet::MovementSpeed` is not consumed by the current movement calculation; merely modifying that attribute would not implement aim mobility. Add a distinct aim lease/state through the existing ASC and CMC seams instead of pretending aiming is blocking. Keep the ordinary movement profile and equipment-load rules unchanged.

Document units, valid ranges, server/client ownership, replication, persistence, and static versus runtime values on the new designer-facing APIs.

## Authoritative contracts

### Shot commit and networking

- Revalidate equipment/action availability, charge, ammunition, stamina, aim, and launch obstruction on authority. Clients submit intent, never accepted damage, cost, or recovery outcomes.
- A successful release publishes one projectile and pays one ammunition/release-cost commit. Failure publishes none and pays neither. Previously paid optional hold drain is separate.
- Cover ammo removed during draw, insufficient release stamina, spawn failure, duplicate release messages, and cancellation races. The existing `URpgAbilityCost::ApplyCost` returns no success result; it alone cannot enforce this commit contract.
- Retain resolved launch damage, source tags, source identity/faction, origin, charge, and recovery policy. Live equipment/source pointers must not change a projectile after a weapon swap, death, or avatar replacement.
- Use launch origin and incoming impact direction for attenuation, defense, and feedback as applicable, not a new pawn's position or the projectile's final position as its own range origin.
- Predict local draw/release presentation and a cosmetic arrow, with no client-authoritative collision or damage. Authoritative confirmation/rejection reconciles by shot identity.
- Keep acknowledgement in a longer-lived ASC/owner seam with bounded cosmetic cleanup. Immediate impact may destroy the projectile before its replicated actor appears, and the ability task may already have ended.
- Keep launch data on the projectile and pass at most a shot identity through the effect context. `FRpgGameplayEffectContext` currently forwards Iris to the engine's context serializer; any new `NetSerialize` field needs a custom Iris NetSerializer beside the legacy path. Do not reuse the melee-only target/instigator cue suppression as an assumed per-projectile identity.

### Exactly-once recovery

For a world impact, the embedded pickup's existing replicated inventory is canonical. Configure its container as withdraw-only, unavailable to crafting, and nonpersistent before initialization; ordinary storage defaults must not accidentally persist temporary arrows. Reuse the shared atomic pickup/transfer path and empty-source cleanup.

For a living enemy, register a pending recovery record keyed by shot identity **before applying potentially lethal damage**. The record contains ammunition definition, the fixed intact/broken result, expiry, and attachment metadata. It is a pending entitlement, not an additional inventory item. Grouping on a character merges only visuals; each shot keeps its own pending record until materialization.

Extend `URpgLootSourceComponent` to append eligible pending arrows to its cached death-loot payload and commit through the existing inventory batch path. Preserve unchanged retries after a failed population. Successful materialization retires the pending entitlement; the corpse inventory becomes the sole quantity owner.

Post-death impacts use an explicit idempotent supplemental-loot path. Calling the current `PopulateLoot()` again is insufficient because it returns after initial population. Reject new corpse credits once the corpse is completed or expiring.

For stackable ammunition, track ordered recovery credits per definition against actual aggregate corpse-quantity decreases. Internal stack merges, splits, and moves do not constitute collection. Retire corresponding visuals when quantities are taken; expiry consumes only credited units still in that corpse, never arrows already transferred to a player.

### Enemy integration

Extend the existing enemy AI and its action-selection seam. The StateTree/configuration owns preferred distance, line-of-fire checks, readable draw timing, release decisions, and repositioning under obstruction or close pressure.

The attack driver inspected at the original planning commit requires a melee primary attack definition and has no ranged target-data/draw lifecycle. Check the current implementation before extending it: reuse any available generic action/targeting contract, and remove remaining melee-only restrictions where necessary so the same granted bow ability can receive player camera intent or a server-selected AI target. Preserve melee behavior and existing loadout grants.

Unlimited NPC ammunition is an explicit content policy. The existing NPC inventory is corpse loot storage, not an implicit combat quiver. Keep the finite recovery budget separate from that inventory and from the number of attacks the AI may perform.

## Items, crafting, and compatibility

- Create productive bow and standard-arrow item definitions through the existing inventory/equipment architecture. Standard arrows use ordinary stack rules; no dedicated quiver slot or ammo-selection UI is needed in the first slice.
- The bow is a two-handed item under the existing loadout rule (`IsTwoHandItem`), so it cannot be paired with a shield. A bow user defends by dodging and spacing.
- Count and consume arrows from the shooter's own inventory graph, including carried nested containers, through the existing atomic by-definition intents (`GetTotalItemCountByDefinition`, `ConsumeItemsByDefinition`). Base storage and chests never feed the bow, and the HUD count reads the same total. Confirm the nested-container coverage in RNG-02.
- Update the resource-parity recipes to produce these definitions, preserving the shared material-pool contract and existing recipe tuning unless separately changed.
- Replace the sandbox placeholders `ID_Placeholder_Bow` and `ID_Placeholder_Arrow` in `GF_Dev_Sandbox` with the production definitions, then delete the placeholder assets in RNG-02. Nothing is in production, so there is no save migration: dev saves that still hold the placeholders may lose those items, or can be deleted.
- Update the resource-parity content tests to validate the productive items. Temporary world arrows and pending corpse credits are not added to world-save data.

## Implementation roadmap

All tasks are **Deferred** because their implementation start has not been selected. The order below is internal to this plan and does not reschedule other workstreams. Preserve these IDs, including the added recovery steps `RNG-03b` and `RNG-03c`. The dependencies are real prerequisites: RNG-04 can run beside RNG-03b and RNG-03c.

| ID | Deliverable | Dependencies | Status |
| --- | --- | --- | --- |
| RNG-01 | The shared projectile for arrows and spells (the combat roadmap's "Projectile foundation"), retained source context, authoritative impacts, projectile defense, the fireball carrier contract, and proof that launching does not require equipment | Baseline refresh | Deferred |
| RNG-02 | Bow/arrow items, shot task, draw curves, inventory consumption, configurable costs/passive seams, recipes, and placeholder removal | RNG-01 | Deferred |
| RNG-03 | Player input, shoulder camera, aim movement, grips, and draw/release animations on CMC/Manny | RNG-02 | Deferred |
| RNG-03b | Impact recovery policies, the break roll, embedded world arrows and pickups, world grouping and cap, the player-hit rule, and salvage lifetime | RNG-03 | Deferred |
| RNG-03c | Embedded arrows on AI characters with grouping, corpse recovery including post-death hits, and the finite NPC recovery-budget mechanism | RNG-03b | Deferred |
| RNG-04 | Crosshair, draw-strength and ammunition HUD through CommonUI/MVVM; no trajectory or impact prediction display | RNG-03 | Deferred |
| RNG-05 | One bow enemy extending existing AI action/spacing behavior, common bow execution, and configured unlimited fire with bounded recovery | RNG-03c | Deferred |
| RNG-06 | Integrated acceptance, network/regression checks, and final first-bow feel tuning | RNG-04, RNG-05 | Deferred |

Each task records its ownership boundary, changed assets, actual validation, failed attempts, remaining limits, PR, and next action. Do not defer the focused checks for early tasks until RNG-06. No implementation task is marked Ready by this document.

### Baseline refresh before RNG-01

1. Verify current branch, working tree, PR state, and the latest runtime baseline. The original planning checkout was clean at detached commit `17f16045`; this is historical inspection evidence, not the future implementation base.
2. Re-read current AI action/targeting, equipment grants, damage/defense, rune/item-instance, skill-tuning, save schemas, and corpse contracts. Reuse existing foundations and account for changes since the original inspection rather than duplicating them.
3. Confirm asset/editor ownership against the [GASP handoff](gasp-integration-handoff.md) and coordinate binary assets and shared MCP sessions. Run only one PIE network process at a time.
4. Verify required local engine overrides and asset availability before builds/tests. Inspect the Archer pack's usable skeleton/clip subset before relying on it.

## Validation and acceptance

Run builds, tests, and cooks through `Build/Tools/Unreal/ue.py`. Build after native/module changes; render-dependent PIE tests must not use NullRHI. Validate stable native mechanisms and asset contracts without freezing cosmetic widget trees or graph layouts.

### Focused contracts

- Nocking cancellation, partial/full draw, repeated shots, recovery timing, and every interruption route.
- Simultaneous primary/secondary aim ownership, camera cleanup, fresh-input suppression, passive changes across activations, and avatar/equipment rebinding.
- Last-arrow consumption, ammo removed while drawing, insufficient stamina, spawn failure, duplicate release data, and no partial resource commits.
- Projectile impact against world and characters, shield block, cheaper Perfect Block without reflection/remote stagger, and dodge immunity.
- The fireball carrier contract on the shared projectile: flight system and loop on every machine, stop and fade, and one authoritative impact cue.
- Player hits leave no attachment, pickup, or recovery credit, including co-op hits, downed players, death, and respawn.
- Consumed-on-impact test ammunition: no break roll, embedding, pickup, recovery credit, or NPC token.
- Grouping on AI characters and world hosts, collecting a group in full and in part, the merged deadline, and the world cap expiring the oldest pickup, all without lost or duplicated quantity.
- Launch-state stability after weapon switching, shooter death, and avatar replacement.
- One break roll, fixed outcomes on retries, lifetime expiry, missing attachment hosts, and late-join attachment reconstruction.
- Embedded arrows following live animation and local ragdolls, including the killing arrow and supplementary hits on corpses.
- Full inventory, two concurrent looters, partial-stack pickup, stack rearrangement, expiry during transfer, and failed death-loot population retries without duplication.
- NPC fire exceeding the recovery budget, including missed/broken arrows, shooter death, and AI/loadout refreshes; no refill or unlimited recovery farming.
- Normal persistence of collected arrows, and absence of temporary arrow state from world saves.
- Explicit visual check that there is no flight-path tube, line, arc, dotted guide, predicted impact marker, or ballistic crosshair correction.

### Integration and playtest

- Prove one player's bow and one enemy's bow use the same mechanism without a player-controller requirement for the AI.
- Exercise aim/draw/release, line-of-fire obstruction, target loss/death during draw, spacing, and retreat/repositioning.
- Use a listen server with two clients, latency, late join, simultaneous shots, and point-blank impacts. Inspect remote draw, release, aim posture, embedded arrows, and ragdoll attachments.
- Regression-check melee combo/block/dodge, equipment grants/removal, inventory, source attribution, GASP montage/layer behavior, death/corpse handling, and shared Mover foundations.
- Compile affected assets, check parentage/skeletons/dependency closure, and cook the relevant test Experience/map.
- Provide simple single-player and multiplayer reproduction steps for every observable gameplay task. No historical validation counts as a newly executed check.

## Later extensions

| Content | Reused foundation | Later addition |
| --- | --- | --- |
| Fireball | Validated aiming/launch, the RNG-01 projectile with its carrier contract, effect payload, and cues | Magic ability assets, mana costs, and fire effects in `GF_Combat_Magic` |
| Special arrow | Bow activation, projectile, recovery policy, and item/equipment source | Ammunition variants and additional effects; elemental arrows use the consumed-on-impact policy |
| Rain of Arrows | GAS activation, server target validation, and shared damage/feedback | Ground targeting, telegraph, and timed area-effect mechanism |
| Other enemy spells | Common ability/target contract | StateTree action selection and feature-owned spell content |

Rain of Arrows receives its own gameplay plan. It need not simulate one damaging replicated projectile for every visible arrow. Its decorative arrows must not accidentally create inventory ammunition. A future area-targeting presentation does not relax the bow's prohibition on flight-path or predicted-impact previews.

Do not build a universal spell editor, broad ability catalog, or speculative projectile framework before the first bow loop is accepted.

## Planning evidence and limits

This plan uses survival-rpg-project for scope, survival-rpg-combat-foundation for combat/equipment ownership, unreal-lyra-expert for GAS/inventory/UI/network seams, unreal-gasp-expert for aim animation and movement, and survival-rpg-orchestrator for native/content boundaries.

Inspection found an existing ranged weapon stub, existing atomic inventory/loot paths, and the current melee damage/AI/animation foundations. It did not find a completed shared projectile or damage-area gameplay implementation. The Archer pack was located by filename only (`Assets_Animations/Content/Archer`). Runtime, animation quality, networking, and final balance remain unvalidated until the corresponding RNG tasks run.

A review on 2026-10-26 re-checked the cited seams against `master` at `0e00284e` (ranged stub, `ApplyCost` without a result, unused `MovementSpeed`, early-returning `PopulateLoot`, the 120-second corpse default, the melee-only AI attack driver, and the effect-context Iris forwarding). It merged the projectile with the combat roadmap's foundation and added the player-hit rule, the embedded-arrow budget, the recovery policies, and the RNG-03b/RNG-03c split from the user's decisions. The user also dropped the placeholder save migration, because no save is in production yet.

Saving this plan adds documentation only. It does not create runtime classes, assets, save migrations, or a started roadmap task.
