# Harvesting rework roadmap

Status, decisions and tasks for the harvesting rework that replaces camera
aiming with tool swings. Each task is one bounded PR on a
`claude/hrw-<nn>-<slug>` or `codex/hrw-<nn>-<slug>` branch. A task records its
commits, validation, failed attempts and limits in its PR and in its own
section below, and updates only its own row in the task table.

The older [harvesting roadmap](harvesting-roadmap.md) predates the mastery map.
It stays the history and the contract for what still holds: the stock core,
the PCG bridge, persistence and co-op load. Its hit model and its powers are
superseded here and by MST-03b.

## Why

The user stopped the mastery-map playtest on 2026-10-31 after the harvesting
steps. Harvesting felt bad:

- **Aiming:** a swing only harvests when a ray from the camera through the
  screen centre hits the resource within 250 cm of the pawn
  (`FRpgHarvestTargeting::SelectAndEvaluate` in `RpgHarvestTargeting.cpp`).
  Tree trunk proxies are about 55 cm wide (`BPC_HarvestInstances_DeadPine`)
  and weak points need a ray within 35 cm. The "4/4" indicator is only the
  resource's stock.
- **Animation:** axe and pickaxe share one placeholder swing,
  `AM_Harvest_PickaxeSwing`, a copy of an unarmed attack.
- **Talents and runes:** the skills and runes feel weak, and talents that only
  work with a socketed rune feel bad. The shared Harvesting tree for axe and
  pickaxe feels odd.

## Decisions

| Topic | Decision |
| --- | --- |
| Hit model | User decision, 2026-10-31: the axe and pickaxe swing finds its targets with a trace on the tool, like melee weapons. There is no screen-centre aiming. The server traces its own pose and commits through the existing `FRpgHarvestRequest` → `IRpgHarvestableTarget::CommitHarvest` path. |
| Input | User decision, 2026-10-31: holding the button keeps swinging, and the player can move while swinging (upper-body montage over GASP legs). |
| Shared mechanism | Combat and harvesting share one native swing-window and trace mechanism in the core module, extracted from `URpgGameplayAbility_BasicWeaponAttack`. The extraction ships alone without behaviour change. |
| Tools and pawns | Tool swings deal no damage to players or enemies, so co-op players chopping side by side cannot hurt each other. A weak tool attack can come later through the same mechanism. |
| Reach and targets | Swing reach and targets per swing are ability tunings from the start (`Ability.Tuning.Harvest.SwingReach`, `.SwingTargets`), so a spell can extend the trace and hit trees behind the first. |
| Assets | Chop and pickaxe clips come from the 505 Survival Mocap pack in the asset library. The axe uses a library mesh if one fits a work axe; the pickaxe gets a simple self-built mesh, because the library has none. |
| Target picture | Modelled on Enshrouded: a skill-tree choice selects a harvesting spell, harvesting fills a focus meter, and a full meter activates the spell for a limited time. The first spell is Wide Swing (extended trace). This replaces rune-gated talents. |
| Deferred | The skill tree, runes and rune-gated talents are redesigned with HRW-05. Crafting stays as it is for now. |

## What stays

Stock, work, talents, rune follow-ups, reward delivery and persistence keep
working through `FRpgHarvestRequest` and `CommitHarvest`. The rework changes
how a hit is found, not what a hit does.

## Tasks

| ID | Scope | Status |
| --- | --- | --- |
| HRW-01 | Tool animations and meshes: migrate the 505 chop and pickaxe clips, one-cycle montages in the `UpperBody` slot with attack-window notifies, work-axe and pickaxe meshes with trace sockets; hit model unchanged | Planned |
| HRW-02 | Extract the core swing window and trace (`FRpgSwingTraceWindow`) from the melee attack ability without behaviour change | In review: #334 |
| HRW-03 | Tool swing hits: harvest abilities trace the tool swing instead of the camera ray; tunable reach and targets per swing; collision profile for harvestables; weak points and pierce from the impact point | Planned |
| HRW-04 | Hold to keep swinging and a stock indicator that appears after a hit instead of the aim highlight | Planned |
| HRW-05 | Focus meter and harvesting spells chosen in the skill tree, first spell Wide Swing; redesign of the tree and the runes | Planned |

## Task scope

### HRW-01: tool animations and meshes

- Migrate only the UE5 clips the task uses from
  `505_Survival_Mocap_Animations/Animations_UE5`: `IP/Chop/IP_Chop_Loop_01`
  (with `IP_Chop_Start_01` if needed) and `IP/Pickaxe/IP_Pick_axe_hit_01`.
  Follow the [asset library migration](../.agents/skills/unreal-lyra-expert/references/asset-library-migration.md):
  compare the skeleton, re-point to the project `SK_Mannequin`, no
  retargeting. Target: `GF_Harvesting_Magic/Content/Animations/`, with a
  notice entry in [third-party notices](third-party-notices.md).
- `AM_Harvest_AxeChop` and `AM_Harvest_PickaxeStrike`: one section, one swing
  cycle, in place. The commit notify sits at the impact frame, and an attack
  window start/end pair surrounds it and ends before auto blend-out.
- The montages play in the existing `UpperBody` slot. Check first whether its
  blend branch is block-only; if so, make it follow the slot weight without
  changing block behaviour or worker-thread safety.
- Replace the placeholder shapes in `BP_Tool_AxeActor` and
  `BP_Tool_PickaxeActor`. Both meshes get `TracePoint_Base` and
  `TracePoint_Tip` sockets at the tool head.

### HRW-02: core swing window

- New `Source/SurvivalRpg/Combat/RpgSwingTraceWindow.h/.cpp` with
  `FRpgSwingTraceParams` and `FRpgSwingTraceWindow`. It takes over the window
  validation, the authority schedule (notify, montage poll, clock), sampling,
  the arc sub-steps and the ribbon traces, unchanged.
- The melee ability keeps combos, damage, its per-window hit set, echo records
  and facing. `FRpgWeaponAttackDefinition` keeps its fields, so no weapon asset
  changes.

#### HRW-02 report

- **Result:** `FRpgSwingTraceWindow` now holds the window contract
  (`ResolveWindowTiming`, `IsWindowEndBeforeAutoBlendOut`), the authority
  schedule, sampling from the equipment's spawned actors or the avatar mesh,
  the arc sub-steps, the line and sweep ribbons on `FRpgSwingTraceParams::TraceChannel`
  (default weapon channel), the debug draw and the trace counters. An ability
  drives it with `Begin`, `HandleStartNotify`/`HandleEndNotify`, `StopTracing`
  and `Reset`, and receives hits, trace queries, window open/close and
  lifecycle records through `FRpgSwingTraceSink` delegates with a role
  (`Authority`, `OwnerFeedback`, `None`). The window neither de-duplicates nor
  damages. `URpgGameplayAbility_BasicWeaponAttack` keeps combos, step tags,
  damage, its per-window hit set, Echo records, facing and lifecycle logging;
  its `…ForTests` getters forward. `FRpgWeaponAttackDefinition` keeps every
  field and gains `MakeSwingTraceParams()`. `ERpgWeaponAttackTraceMode` moved
  unchanged into the new header (same module and name, so assets load as
  before).
- **Decisions:** the facing hold moved from every open attempt to the moment
  the window opens; a repeated attempt was a no-op and a refused one cannot
  happen, because closing clears the schedule, the poll and the start timer.
  The per-sample `CanApplyDamage()` guard left the trace, since a step that
  cannot apply damage never starts. The sink survives `Reset`, because a hit
  may end the ability inside the window's own trace loop. Three warnings now
  read `SwingTraceWindow[…]` instead of `BasicWeaponAttack[…]`; the lifecycle
  stages are unchanged.
- **Commits:** `2a4da481` refactor and tests, `52c461f8` report and skill notes, then this status row (PR #334).
- **Validation** (`ue.py`, headless, no MCP editor of this worktree; the
  HRW-01 editor shared the GPU at about 80 %):
  - `build`: 0 errors, 0 warnings, before and after the change.
  - `test SurvivalRpg.Combat --null-rhi`: 56 / 56, including the new
    `SwingTrace.WindowTiming` and `SwingTrace.RibbonHitsBetweenSamples`,
    `WeaponTrace.*`, `Runes.Echo.*` and `WeaponAttackAssetContract`.
  - `test SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.Network.CombatImpactCuesPIE`
    (rendered, 1/60 s frame cap): 6 / 6 on the first run. The remote client's
    authority windows still traced 4 to 9 sub-steps and landed their hits.
  - `test SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage SurvivalRpg.GASP.Mover.Gameplay SurvivalRpg.GASP.Mover.Lifecycle`
    with `np.ForceReconcile 0` and `t.MaxFPS 30`, the tests that read the
    attack counters: 9 / 9.
  - `test SurvivalRpg.AI` (enemies attack through the same ability): 6 / 6.
- **Limits:** no manual two-player playtest; the listen-server PIE tests cover
  owner, authority and remote client. A dedicated server is not tested
  separately, as before. Three warnings changed their prefix and the scene
  query stat is now `RpgSwingTraceWindow`. No harvest ability uses the window
  yet (HRW-03).

### HRW-03: tool swing hits

- `URpgGameplayAbility_Harvest` gets a strike source (`ViewRay` or
  `ToolSwing`) and the swing window. View-ray targeting stays for focused
  powers.
- Every harvestable contact commits once per swing on the server until
  `SwingTargets` is reached; the default is the first contact. Hits are
  de-duplicated per receiver and instance. The server rechecks reach (grown by
  `SwingReach`), tool tag and stock.
- A collision profile lets harvestables overlap the weapon trace channel, so
  swords do not stop at trunks.
- Weak points count the impact point inside their radius; pierce runs from the
  harvester through the impact point.

### HRW-04: hold to swing and stock indicator

- Both swing abilities activate while the input is held, end at montage
  blend-out, and stop until the next press after a cancel such as a dodge.
- The aim query and highlight go away for tool swings. The stock indicator
  shows the last struck resource for a few seconds.

### HRW-05: focus meter and harvesting spells

- Starts with a short design round with the user: fill rate, key, duration and
  further spells.
- The focus value is server-authoritative runtime state replicated to its
  owner; the UI only reads it. Spells are `GA_*` Blueprints with timed
  GameplayEffects. Ability tunings must also come from active timed effects,
  not only from the skill tree.
- Covers the "magical harvesting ability" of the first playable in
  [game-vision.md](game-vision.md) §12, open since Rift Grip was retired.

## Order

- HRW-01 and HRW-02 share no files and can run in parallel.
- HRW-03 starts when both are merged; HRW-04 follows HRW-03, and HRW-05 follows
  HRW-04.
- Every gameplay task runs the listen-server PIE tests and adds a two-player
  step to its playtest.
