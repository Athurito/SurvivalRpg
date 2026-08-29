# GASP combat upper-body presentation contract

This document defines the issue #75 presentation boundary and the issue #113 feature-lifecycle
boundary for combining project-owned GASP CMC locomotion with
weapon-specific upper-body presentation. The layer is cosmetic. It must not become a second owner
for equipment, combat state, rotation policy, montage execution, movement, or replication.

## Ownership

| Concern | Authoritative owner | Animation consumption |
| --- | --- | --- |
| Equipped items and active `MainHand` / `OffHand` loadout | `URpgEquipmentManagerComponent` and its replicated equipment list | On the game thread, the AnimInstance proxy projects `EquipmentTraitTags` from active hand `URpgWeaponInstance` objects into a value snapshot. |
| Attack, block, dodge, hit reaction, death, combat gates, montage windows, notifies, damage, and root motion | GAS and the existing combat abilities | The AnimBP only displays the resulting mapped tags and `DefaultSlot` montages. It does not activate, cancel, or advance gameplay. |
| `Free`, `CombatStrafe`, and `Aim` rotation policy | `ARpgCharacter` and its replicated rotation-mode contract | `Free` selects the equipped/relaxed pose; `CombatStrafe` and `Aim` select the combat-ready pose. |
| Capsule motion, gait, stance, airborne state, floor, and movement correction | CharacterMovement and the existing character lifecycle | GASP Motion Matching and procedural nodes consume the established proxy snapshot. |
| Upper-body sequences, mask, blend timings, and loadout-to-profile mapping | Designer-owned `URpgCombatAnimationProfile` asset inside `GF_Combat_Core` | A feature-owned provider component supplies the active profile; `URpgAnimInstance` resolves and snapshots presentation values; the AnimGraph blends them. |

The AnimBP is never gameplay truth. It must not infer a weapon from a montage name, set rotation
mode, grant abilities, mutate equipment, or replicate a cosmetic profile. UI may read this state for
diagnostics but must not author it.

Equipment and character UObject access is confined to game-thread proxy collection. Parallel
animation update and evaluation consume only copied values, already loaded animation pointers, and
immutable lookup data. They must not query the ASC or equipment components, inspect asset paths,
dynamically load content, or read mutable DataAsset arrays.

## AnimGraph composition

The pilot GASP graph uses this ordering:

```text
GASP lower-body locomotion
  Motion Matching -> Offset Root Bone
    -> masked weapon upper-body overlay
      -> DefaultSlot
        -> Foot Placement -> Leg IK
          -> Pose History -> Output Pose
```

The contract at each boundary is:

1. **GASP lower body** supplies starts, stops, pivots, reversals, gait changes, jumps, landings, and
   the current locomotion pose. It remains the base pose of the upper-body blend.
2. **Weapon overlay** blends the selected relaxed or combat-ready looping sequence with a
   `Layered Blend per Bone`. Use mesh-space rotation blending and the project-owned
   `UpperBodyMask`. `CombatAnimationOverlayAlpha` controls equip/profile blending;
   `bCombatAnimationReady` and `CombatModeBlendTime` control relaxed-to-ready blending. Optional
   weapon additives belong inside this presentation stage and must not overwrite gameplay
   montages.
3. **`DefaultSlot`** remains after the overlay and stays the single authoritative GAS montage
   route. Attack, block, dodge, hit, death, and root-motion montages can therefore replace the
   composed locomotion/weapon pose without double posing. Existing montage sections, notifies,
   ability tasks, and root-motion authority remain unchanged.
4. **Foot Placement and Leg IK** run after `DefaultSlot`, using their existing montage and
   procedural gates. The weapon overlay must not take ownership of root, pelvis, or legs.
5. **Pose History** records the final composed pose at its existing location. No parallel combat
   history or second Motion Matching owner is introduced.

`UpperBodyMask` is a skeleton `BlendMask`, not a transition blend profile. On `SK_Mannequin` it
keeps root, pelvis, and legs at their default weight `0`, ramps `spine_01` through `spine_05` from
`0.10` to `0.50`, uses `0.65` on clavicles, approximately `0.75` to `0.85` through arms and
forearms, and reaches `1.0` on hands, fingers, and weapon bones. This is the default issue #75
mask because it preserves lower-body GASP ownership. `UpperBodyLowerBodySplitMask` is not an
automatic substitute: its small pelvis contribution and full arm weights require an explicit
rendered A/B review before adoption.

The overlay must fail safely to a valid base pose when its profile or animation is absent. A
missing or unknown profile must never produce a reference pose, stale weapon pose, or zeroed base
pose.

## Designer data and deterministic resolution

`URpgCombatAnimationProfile` is the project-local presentation DataAsset. A concrete
`URpgCombatAnimationProfileProviderComponent` Blueprint in `GF_Combat_Core` hard-references it;
the core GASP AnimBP does not. The asset owns:

- `TargetSkeleton` and `UpperBodyBlendMaskName`;
- one `UnarmedFallback`;
- designer-authored `WeaponProfiles`;
- for every entry, a stable `ProfileName`, required and blocked equipment traits, relaxed and
  combat-ready looping sequences, and equip/combat blend times.

Runtime input is the union of `EquipmentTraitTags` from the replicated active `MainHand` and
`OffHand` weapon instances. Weapon family alone is not a sufficient presentation identity: both
one-handed and two-handed swords can share `Weapon.Family.Sword`. Concrete equipment assets use
the dedicated cosmetic traits under `Equipment.AnimationProfile.*`.

Resolution is deterministic:

1. An entry matches only when all `RequiredEquipmentTraits` are present and none of its
   `BlockedEquipmentTraits` are present.
2. Among matching entries, the unique entry with the greatest number of required traits wins.
   This is the **most-specific** rule.
3. No match, an empty loadout, or a tie at the highest specificity resolves to the authored
   `UnarmedFallback`. If the DataAsset itself fails validation, runtime lookup rejects the complete
   asset and uses its own empty `Unarmed` fallback instead.
4. The authored fallback has no required or blocked traits. It may provide a complete relaxed/ready
   pair, or no overlay pair; no pair means pure GASP presentation with overlay alpha `0`.
5. Replacing a profile first blends the active overlay out, swaps the immutable selection at zero,
   and blends the new overlay in. It does not crossfade unrelated weapon poses at full weight.

Data Validation rejects missing or mismatched skeletons, a missing/non-mask `UpperBodyMask`, a
missing `DefaultSlot`, incomplete animation pairs, non-looping/additive/root-motion sequences,
invalid blend times, ambiguous profile definitions, and forbidden dependencies on excluded GASP
sample stacks. Runtime lookup uses the active provider's hard reference and must fail closed to
Unarmed.

### First-playable profile

The active first-playable loadout is `BP_BasicSwordShieldStarterLoadout`: Basic Sword in
`MainHand` plus Basic Shield in `OffHand`. Its composed profile is:

| Field | Value |
| --- | --- |
| Profile | `SwordShield` |
| Required traits | `Equipment.AnimationProfile.OneHandSword`, `Equipment.AnimationProfile.Shield` |
| Blocked trait | `Equipment.AnimationProfile.TwoHandedSword` |
| Equipped/relaxed sequence | `/GF_Combat_Core/Animations/Sword_and_Shield/Animations/Sequence2/01_Idle/Idle_Seq` |
| Combat-ready sequence | `/GF_Combat_Core/Animations/Sword_and_Shield/Animations/Sequence2/01_Idle/Idle_Combat_Seq` |

The existing `Idle_to_Idle_Combat_Seq` and `Idle_Combat_to_Idle_Seq` assets are reference content,
not new gameplay transitions. The first slice uses the profile's authored crossfade unless a
rendered review proves a dedicated transition is necessary.

The authored sibling definitions remain unambiguous, and the reserved two-handed trait fails closed:

- `OneHandSword` requires `Equipment.AnimationProfile.OneHandSword` and blocks Shield and
  TwoHandedSword.
- `SwordShield` is more specific than OneHandSword because it requires both hand traits.
- `TwoHandedSword` is tagged separately but intentionally has no authored profile in this slice, so
  it resolves to `Unarmed` until a curated two-handed overlay pair is visually accepted.

Adding another weapon family is designer content plus validation and visual acceptance; it is not
a reason to add weapon-specific branches to `URpgAnimInstance`.

## Experience and PawnData switchability

Lyra-style Experiences remain the composition root. Issue #75 does not change the default pawn,
GameMode, Experience selection, PawnData schema, or character authority:

- `RpgPrototypeExperience` continues to select `DA_PawnData` and the prototype character/AnimBP
  path.
- `RpgGaspPilotExperience` continues to select `DA_PawnData_GASP`,
  `BP_Rpg_Character_GASP`, and `ABP_RpgCharacter_GASP`.
- The GASP PawnData continues to differ only through its isolated character/AnimBP and established
  locomotion defaults. Input config, ability sets, camera, inventory layout, TeamId, Experience
  actions, and GameFeature order remain unchanged.
- `GF_Combat_Core` injects `BP_RpgCombatAnimationProfileProvider` only into
  `BP_Rpg_Character_GASP` through the existing GameFeature Add Components action. The provider is
  client-world presentation content, not a global singleton, and does not force the prototype
  Experience or AI characters onto GASP presentation.

Changing Experience/PawnData must therefore remain a reversible content choice. No issue #75
class or asset may make Epic's GASP sample project a runtime dependency.

## GameFeature lifecycle

The core `URpgAnimInstance` knows only the native provider contract. It has no asset path, soft
reference, or hard reference into `/GF_Combat_Core`. On each game-thread proxy `PreUpdate`, it
observes the provider attached to the current pawn and rebuilds the immutable lookup only when the
provider or profile identity changes. This covers an already existing pawn when the feature is
activated, a newly spawned or swapped pawn, AnimInstance reinitialization, and late join.

The active AnimInstance retains one transient GC-strong reference to the provider's profile because
the lookup and proxy intentionally contain raw `UAnimSequence` pointers. On feature removal or an
invalid/missing provider, cleanup is immediate and ordered: proxy and public animation pointers are
reset to empty `Unarmed`, the lookup is cleared, then the strong profile reference is released.
Immediate neutralization is deliberate; a delayed visual fade would retain feature-owned assets
past deactivation and require a separate two-phase unload protocol.

Feature removal does not rely on a future animation tick. The provider's `OnUnregister` first
finishes any in-flight parallel mesh evaluation, then synchronously asks every owning
`URpgAnimInstance` to perform the ordered cleanup before the component and feature content are
released. Hidden, culled, or explicitly non-ticking meshes therefore cannot retain stale profile or
sequence references across deactivation.

The provider component never ticks or replicates. It is added in graphical worlds (including a
listen server) and omitted from dedicated servers. Equipment and rotation replication remain the
only multiplayer inputs; the provider carries static local presentation content and never becomes
gameplay truth.

## Multiplayer and late-join contract

No new gameplay replication, RPC, saved state, or animation-authoritative state is added.

- The server remains authoritative for equipment, GAS state, montage execution, and rotation mode.
- Every graphical world receives the same feature-owned provider for the GASP pawn through the
  GameFeature component manager; no provider RPC or replicated profile pointer is introduced.
- Existing replicated equipment reconstructs the active hand instances and their static
  presentation traits. Existing rotation-mode replication reconstructs relaxed versus combat-ready
  intent.
- Authority, autonomous proxies, simulated proxies, relevancy returns, and late joiners resolve the
  same profile name from the same replicated gameplay state. A client may temporarily display the
  safe Unarmed fallback while replication is incomplete, then blend into the resolved profile.
- Overlay alpha and local crossfade time are transient presentation values. They are reconstructed
  locally and are not replicated; the deterministic target profile and ready state are what must
  converge.
- `DefaultSlot` montage replication, root motion, gameplay notifies, and ability lifecycle are not
  replaced by the overlay. A montage that starts during an equip or combat-mode blend still owns
  the final full-body slot pose.
- Worker threads receive only the proxy snapshot. There are no worker-thread ASC/equipment reads,
  path-based classification, dynamic loads, or mutable profile access.

Automated tests may compare resolved profile identity, fallback state, ready state, and bounded
blend convergence across network roles. Those value checks do not replace rendered multiplayer
acceptance.

## Manual rendered test matrix

Run with the real `RpgGaspPilotExperience` in one-process PIE as a listen server with two external
clients. Join the second client after the observed pawn is already equipped and in combat-ready
mode. Record host, owning-client, simulated-proxy, and late-join views.

| Scenario | Actions | Required visual result |
| --- | --- | --- |
| Unarmed fallback | Spawn without hand equipment; move through every gait | Pure GASP locomotion; no T-pose, stale weapon arms, or overlay pop. |
| Equip first-playable | Equip Basic Sword plus Shield while idle, then while moving | `SwordShield` blends in once; weapon meshes and hands agree; lower-body cadence and facing remain stable. |
| Partial/unknown loadout | Remove Shield, remove Sword, equip an unmapped fixture | Unique valid profile or deterministic Unarmed fallback; never reuse the previous profile. |
| Combat-mode toggle | Toggle middle mouse repeatedly in idle, walk, and run | Relaxed and combat-ready upper body crossfade smoothly; replicated facing policy remains authoritative; no arm rowing or torso snap. |
| Locomotion stress | Start, stop, 180-degree reverse, pivot, strafe, walk/run transitions | GASP lower-body selection remains stable under the masked overlay; feet do not skate because of weapon-pose pelvis/root motion. |
| Airborne | Jump forward/backward in relaxed and ready modes; land idle and moving | No upper-body pop at takeoff or touchdown; landing and Foot Placement/Leg IK remain coherent. |
| Attack | Use primary and secondary attacks during idle and movement | `DefaultSlot` montage cleanly overrides the composed base; attack windows/notifies fire once; no double pose or post-montage stale pose. |
| Block | Enter block, hold loop, receive block hit, release | Start/loop/hit/end montages retain current GAS ownership and return smoothly to the correct relaxed/ready profile. |
| Other montage gates | Dodge, hit reaction, guard break, death where available | Montage/root-motion result wins; procedural controls do not fight it; recovery returns to the correct profile. |
| Equip lifecycle | Unequip, re-equip, and holster/draw through the existing equipment flow | Overlay follows the replicated active hand loadout, blends out before replacement, and never invents a sheath or hidden equipment owner. |
| Feature lifecycle | Deactivate and reactivate `GF_Combat_Core` with an existing GASP pawn; repeat after respawn | Deactivation immediately produces `Unarmed`, alpha `0`, and no overlay sequences; reactivation restores the equipped profile once, with no stale or duplicate component. |
| Late join | Join while Sword/Shield is equipped and CombatStrafe/Aim is active; repeat during movement | Late joiner converges to `SwordShield` and ready pose without a reference pose, permanent fallback, or different facing mode. |
| Relevancy/correction | Leave and regain relevancy; exercise normal simulated smoothing and one deliberate correction | Reconstructed profile remains correct; no extra animation-history reset, mask pop, or divergent montage authority. |

For every row, inspect the lower body and upper body separately, then the final silhouette. Capture
at least idle, start, reversal, jump, landing, attack, block, equip/unequip, and late join from host
and one simulated-proxy view. Passing criteria are no T-pose, snap, double pose, rowing, leg jitter
introduced by the overlay, foot-placement conflict, montage/root-motion regression, animation-thread
warning, or disagreement in final profile identity after replicated state converges.

## Verification boundary

Issue #75/#113 acceptance requires all of the following:

- editor build and Data Validation;
- focused profile resolver, provider-lifecycle, feature-composition, and asset-contract tests;
- GASP graph-contract coverage for mask mode, mask name, mesh-space rotation blending, and ordering
  before `DefaultSlot` and Foot Placement/Leg IK;
- real-network coverage for equip/unequip, rotation-mode toggle, simulated proxies, relevancy return,
  equipped late join, and real GameFeature deactivate/reactivate with the subject mesh tick disabled;
- the rendered manual matrix above.

Value-only automation proves ownership and deterministic reconstruction. It cannot prove that a
sword grip, shield silhouette, torso blend, leg motion, or transition is visually acceptable.
