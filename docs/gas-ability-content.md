# GAS ability content boundary

Issue [#164](https://github.com/Athurito/SurvivalRpg/issues/164) separates the
15 concrete native abilities named in the issue from their designer-owned
content. Gameplay behavior, equipment ownership and the existing Experiences
remain the compatibility baseline.

## Ownership decision

- **Runtime truth:** GAS owns specs, activation and prediction. Inventory,
  equipment, interaction providers and health components retain their existing
  authoritative state and lifecycle.
- **C++:** reusable source lookup, authoritative transactions, validation,
  activation leases, replicated state cleanup and native engine integration.
  Fourteen existing ability mechanisms become abstract. No new runtime class
  is introduced.
- **Blueprint/DataAsset:** concrete ability identity, triggers, tuning,
  montages and grants. Hit reaction is entirely Blueprint flow. Dodge's
  Blueprint owns commit and montage tasks; its native base snapshots the
  equipment profile once per activation and clears it on end.
- **Presentation:** existing montages and UI remain consumers of gameplay
  state. No AnimBP, skeleton, movement simulation or UI authority changes.
- **Editor:** existing Unreal MCP Blueprint/Object/Asset tools author,
  compile, save and read back assets. No new editor tooling is required.
- **Tests:** native lifecycle/authority regressions plus shared asset
  contracts for parentage, compilation, grants and cooked dependencies.

## Class decisions

| Native suffix | Native responsibility | Concrete content |
| --- | --- | --- |
| `FromEquipment` | Spec source and equipment context; abstract family only | Existing attack/block children |
| `ApplyItemEffects` | Server item-use transaction and deferred consumption cleanup | `GA_ApplyItemEffects` |
| `BasicWeaponAttack` | Authoritative attack windows, traces and cancellation | Existing `GA_BasicWeaponAttack` |
| `Block` | Predicted source lease, attribute snapshot and restoration | Existing `GA_Combat_Block` |
| `Death` | Exclusive uncancelable health lifecycle and terminal cleanup | Existing `GA_Combat_Death` |
| `Dodge` | Per-activation equipment profile snapshot and lifecycle cleanup | `GA_Dodge` |
| `HitReaction` | Removed: guards, commit and montage/delay tasks are Blueprint-accessible | Existing `GA_Combat_HitReaction`, directly parented to `RpgGameplayAbility` |
| `Revive` | Authority reservation, repeated target validation and cleanup | `GA_Revive` |
| `SelfRevive` | Authoritative health reset and native attribute mutation | `GA_SelfRevive` |
| `Stagger` | Replicated state-tag lifecycle, attribute reset and activation-group restoration | Existing `GA_Combat_Stagger` |
| `Collect` | Inventory graph transfer, consumption and equipment transaction | Existing `GA_InteractionCollect` and `GA_Combat_CollectBasicSword` |
| `ExecuteInteraction` | Authoritative revalidation and native interaction-provider commit | `GA_ExecuteInteraction` |
| `OpenStorageContainer` | Server access checks and owning-client storage RPC | `GA_OpenStorageContainer` |
| `OpenCraftingStation` | Server station access checks and owning-client crafting RPC | `GA_OpenCraftingStation` |
| `OpenBaseStorageStation` | Server base storage access checks and owning-client RPC | `GA_OpenBaseStorageStation` |

Core interaction abilities live under
`/Game/SurvivalRpg/Interaction/Abilities`. Item use, self revive and dodge live
under `/Game/SurvivalRpg/AbilitySystem/Abilities`. Existing combat assets stay
in `GF_Combat_Core`; Core gains no dependency on a combat feature asset.

Native execution/instancing policies and gameplay state tags used to enforce
mechanism invariants stay native. Concrete event triggers, attack fallback
selection, block fallback tuning, revive/stagger timings, health restoration
fraction and dodge UI text move to assets. Identity multipliers and numerical
safety clamps remain schema defaults. Weapon-specific tuning already belongs
to equipment definitions/instances and is preserved.

## Grant audit and migration

The initial MCP audit inspected all 16 registered project AbilitySets. Their
18 ability entries already referenced Blueprint classes. Equipment and
Experience composition therefore keeps those AbilitySet references.

Two explicitly serialized native references required replacement:

- `ID_TestHealthPotion`'s usable-item fragment now selects
  `GA_ApplyItemEffects`.
- `BP_Rpg_PlayerController`'s existing revive grant node now selects
  `GA_Revive`.

Native interaction defaults existed in world pickups, dropped inventory,
containers, crafting/base stations, doors, downed characters and instanced
harvesting. They now resolve six soft class references in `DA_RpgGameData`
when gathering options. Explicit provider overrides take precedence. Resolving
at runtime avoids loading designer assets while native constructors/CDOs are
being created. Native-spawned dropped actors use the same collect fallback.
The existing AssetManager keeps resolved classes resident, and GameData's
recursive cook rule includes its soft dependencies.

Existing AbilitySet, equipment and Experience composition is unchanged.
`GA_Dodge` and `GA_SelfRevive` provide concrete configurable content for the
existing mechanisms; this migration does not add new player input or grants.
Feature-native portal and harvesting abilities outside the issue's 15-class
inventory are outside this change.

## Validation

Executed on 2026-10-02 with UE 5.8.2:

- Editor build passed. The final source build (`20261002-203506-build`)
  reported zero errors and zero warnings. The first full build passed with
  one existing `ChaosMover` deprecated-API warning.
- MCP compiled all 15 concrete ability assets with warnings treated as
  errors. After reloading their saved packages, all captured reflected
  defaults matched exactly. The six GameData references, potion fragment
  and controller grant were read back as concrete Blueprint classes.
- MCP inspected 14 equipment/Experience definition classes in addition to
  the 16 AbilitySets. A tracked-package name scan covered actor/component
  and map overrides; the two explicit native references are listed above.
- Focused native/assets run: **27/27 passed**, no test warnings
  (`20261002-203546-test`). Includes the three shared content contracts,
  ten block lifecycle tests, two dodge tests, interaction authority/grants,
  item-use routing and health lifecycle regressions.
- Rendered network run: **10/10 passed**, all with warnings
  (`20261002-203652-test`). Covers remote melee plus the real granted hit
  reaction, CMC/Mover block and stagger, two Mover death/respawn/late-join
  cases, loot/harvest and physical storage. Used `np.ForceReconcile 0`,
  `t.MaxFPS 30`, and a 90-second CQTest network timeout.
  Warnings include PIE NetGUID/package-map messages, unavailable voice,
  repeated console lookups, asynchronous PoseSearch builds and
  NetworkPrediction rollback diagnostics. The editor startup also logged
  two known condition-check errors outside the test results.
- Agent skill/instruction consistency check passed.
- Windows cook of `Lvl_RpgGaspMantle` passed in 99 seconds with zero errors
  (`20261002-204540-cook`). Fresh cooked packages contain all six GameData
  defaults, item use and the active combat abilities. Four warnings cover
  disabled Steam, the existing broad GameplayCue search, the MCP plugin
  notice and the existing `CUI_RespawnScreen` native-tick setting.
- Independent source/test review and `git diff --check` passed.

The Dodge regression covers GAS's deferred end under a target-list lock,
snapshot cleanup, rejection of reads during end callbacks, and reuse of the
same ability instance. The helper has no designer callbacks, so profile
resolution cannot synchronously end/restart its own activation.

The runtime checks use local uncooked PIE; the cook verifies asset processing
and dependency inclusion. They do not establish packaged/WAN or
dedicated-server behavior. Unassigned dodge and self-revive content has MCP
compile/reload coverage and is not pulled into the map cook. The newly
authored item-use and interaction assets
have grant/parent/compile coverage; this work does not claim a separate
end-to-end activation test for every collect, station, revive or self-revive
variant. Existing server-only reaction owner-montage limits remain.

The original inherited defaults were captured through MCP before rebuilding.
They must be reapplied after the native defaults change: writing a value equal
to the old parent default alone does not guarantee that Unreal serializes an
override. Asset readback after a fresh editor load verifies the final values.

Local raw evidence lives under `Saved/Issue164` and `Saved/ToolRuns`; these
directories are ignored and are not available in another checkout.
