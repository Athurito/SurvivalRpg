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
| HRW-01 | Tool animations and meshes: migrate the 505 chop and pickaxe clips, one-cycle montages in the `UpperBody` slot with attack-window notifies, work-axe and pickaxe meshes with trace sockets; hit model unchanged | Merged: [#335](https://github.com/Athurito/SurvivalRpg/pull/335) |
| HRW-02 | Extract the core swing window and trace (`FRpgSwingTraceWindow`) from the melee attack ability without behaviour change | Merged: [#334](https://github.com/Athurito/SurvivalRpg/pull/334) |
| HRW-03 | Tool swing hits: harvest abilities trace the tool swing instead of the camera ray; tunable reach and targets per swing; collision profile for harvestables; weak points and pierce from the impact point | In review: [#337](https://github.com/Athurito/SurvivalRpg/pull/337) |
| HRW-04 | Hold to keep swinging and a stock indicator that appears after a hit instead of the aim highlight | In review: [#339](https://github.com/Athurito/SurvivalRpg/pull/339) |
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

#### HRW-03 report

Branch `claude/hrw-03-tool-swing-hits`, PR [#337](https://github.com/Athurito/SurvivalRpg/pull/337): `9826dbe3` implementation, assets and tests, `26da0250` report and skill notes, then this status row. Axe and pickaxe swings now find resources with their tool's trace; nothing aims at the screen centre any more.

- **Strike source:** `URpgGameplayAbility_Harvest` gained `ERpgHarvestStrikeSource` (`ViewRay`, `ToolSwing`), the `SwingTrace` trace parameters, `SwingWindupSteeringRate` and an `FRpgSwingTraceWindow`. `ViewRay` keeps the old path for focused and area powers. The old commit is split: `ExecuteAuthorityCommit` (view ray) and `CommitToolSwingContact` (one swing contact) find the target, and the shared `ResolveSelection` does rewards, `CommitSelection` with the rune follow-ups, cues and On Harvest Resolved. A test ability overrides the new `PlayToolSwing` hook to run the swing without an animated mesh.
- **Hit rule:** the server counts each harvestable contact once per swing, keyed by receiver and instance (`FRpgHarvestTargeting::MakeResourceKey`, so PCG instances count separately), until the tuned `SwingTargets` are reached. Ground, pawns and other actors never count; tools damage no pawn. A counted contact is evaluated against the stock at that moment: the server checks reach (`MaxReachFromAvatar` plus `SwingReach`) itself, and tool, skill and stock through the target. A rejected contact plays the no-yield cue and still uses up the swing; a contact beyond the reach is ignored. Every swing is one root action.
- **Commit timing (decision):** the window's authority hits commit directly at contact, as melee damage does. The `GameplayEvent.Harvesting.Commit` notify stays in both montages as the authored impact frame inside the window (`SwingMontageContract` checks it) but gates nothing for a tool swing. Cancelling before the window yields nothing, and `EndAbility` closes the window without taking its final sample's contacts.
- **Tunings:** `Ability.Tuning.Harvest.SwingReach` (cm, base 0, at most 400) adds to the new `FRpgSwingTraceParams::TipExtension`, which moves the last trace point beyond the tip along the tool axis; `Ability.Tuning.Harvest.SwingTargets` (base 1, 1 to 8). Both resolve through `GetTunedValueForSpec` with the other tunings, on the server and on the swinging client, and are captured when the swing starts. HRW-05's Wide Swing can set them.
- **Weak points and pierce:** `FRpgHarvestRequest::bSwingStrike` marks a swing contact. A weak point then counts only when the impact point lies within its radius (`FRpgHarvestWeakPoint::IsStruckBy(Hit, bImpactOnly)`); a pierce drives on horizontally from the harvester through the impact point instead of along the tool's sideways sweep.
- **Feedback:** the new `GameplayCue.Harvesting.ToolHit` with `GCN_Harvest_ToolHit` (an `Rpg Impact Burst`: smoke puff, light camera shake for the swinger only) is the success cue of both swings. The swinging client traces the same window for feedback and, when its own evaluation would harvest, plays the cue at once through `RpgCombatImpact::PlayPredictedImpact` on its avatar; the notify absorbs the server's copy there, so every machine plays each hit once. Neither swing has a no-yield cue asset yet.
- **Facing:** like a melee attack, the windup may still turn toward the camera at `SwingWindupSteeringRate` (360 deg/s), and the window holds the facing. No aim-assist turn was built.
- **Collision:** new profile `RpgHarvestable` in `Config/DefaultEngine.ini` (WorldStatic, Visibility and Interaction block, weapon channel overlap). The constructors of `URpgHarvestableInstancesComponent` and `URpgHarvestableInstancedMeshComponent` use it, so `BPC_HarvestInstances_*` and the ISM node Blueprints (`BP_HarvestNode_Bush`, `_Mining`, `_Tree`) inherit it. `BP_HarvestNode_StaticMeshBase`'s `ResourceMesh` (and so `BP_HarvestNode_IronVein`) moved from `BlockAllDynamic` to it. The Static Mesh Spawners of `PCG_HarvestIronVeins` and `PCG_HarvestDeadPines` had their own `BlockAll` descriptor; they now use the profile, and the PCG volumes of `Lvl_HarvestPickaxe` and `Lvl_ResourceParity` were regenerated (same 9, 12 and 12 instances) and saved.
- **Assets:** `GA_Harvest_AxeChop` and `GA_Harvest_PickaxeStrike` use `ToolSwing` with a 12 cm sphere sweep between `TracePoint_Base` and `TracePoint_Tip` on the weapon channel and the tool hit cue.
- **Supporting changes:** `GameplayEvent.Weapon.Attack.Window.Start/End` are exported from the core module for GameFeature modules. The PIE world-save isolation of the combat PIE tests moved to `FRpgScopedPIEWorldSaveIsolation` in `RpgCombatNetworkTestTypes`, and the network instance field fixture uses the harvestable profile.

##### Tests

- Native `SurvivalRpg.Harvesting.Swing.*`: `FirstContactCommitsOnce`, `RejectedContactConsumesSwing`, `ReachGuard`, `CancelYieldsNothing`, `WeakPointByImpact`, `PierceFollowsHarvesterToImpact`, `ReachTuningHitsTargetBehind` (a real sphere sweep through two hand-built tool poses in a test world).
- Asset contracts: `AxeToolContract` and `PickaxeToolContract` check the tool swing trace, the sockets on the tool mesh and the impact notify of the hit cue; `SwingMontageContract` checks the window contract and the impact inside it; new `HarvestableCollisionContract`.
- PIE `SurvivalRpg.Network.HarvestSwingPIE`: a remote client swings the real axe at a one-instance field on a listen server with 60 ms lag. One commit per swing, each machine plays the hit once, a swing cancelled before its window takes nothing, and a late joiner sees the stock and replays no hit, then plays the next hit once.

##### Validation

All through `ue.py`, with the MCP server's autostart off for test runs; the MCP editor of this worktree was closed for every build and test.

- `build`: passed, 0 errors, 0 warnings. A first build failed to link: the window event tags were not exported from the core module (fixed).
- `test SurvivalRpg.Harvesting.Swing --null-rhi` before the asset work: 6 / 7; `PierceFollowsHarvesterToImpact` failed because the test's synthetic contact kept `Item` 0 (fixed in the test, noted in the GAS test pitfalls).
- `test SurvivalRpg.Harvesting SurvivalRpg.Interaction.Harvesting SurvivalRpg.Combat --null-rhi`: **165 / 165 passed** (12 with warnings), including the seven `Harvesting.Swing.*` tests, the tool and collision contracts and the unchanged view-ray tests.
- `test SurvivalRpg.Network.HarvestSwingPIE` (rendered): 1 / 1 on its first run, owner predictions 2 of 2 hits.
- `test SurvivalRpg.Network.HarvestSwingPIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.Network.CombatImpactCuesPIE` (rendered, separate call from the native suites): **17 / 17 passed** (all with the usual PIE log warnings), no "no more RPCs" line; owner predictions 2 of 2 tool hits and 4 of 4 melee hits. No rerun was needed.
- `test SurvivalRpg.AI` (enemies attack through the melee ability, which shares the window): 6 / 6 passed.
- Editor (MCP, port 8000): the generated components of both maps report `RpgHarvestable` with 9, 12 and 12 instances, as before; the placed iron veins of `Lvl_HarvestPickaxe` picked up the profile from their parent Blueprint.
- `python Build/Tools/AgentSkills/sync.py` and `--check`: consistent.

##### Limits

- No swing was watched by eye in PIE on the real trees and veins; the PIE test strikes a 100 cm cube with the real axe, character, montage and tool sockets. The axe reaches about 1.1 m, so a player standing farther from a 55 cm trunk now misses where the camera ray used to hit. An aim-assist turn toward the nearest resource in reach is a possible follow-up if the trees are hard to hit; none was built.
- The pickaxe swing is covered by its asset contract only, not by a PIE swing.
- Neither tool has a no-yield cue asset; a rejected contact is silent apart from the swing.
- The targeting preview and highlight still follow the camera ray for tool swings; HRW-04 replaces them with the stock indicator.
- The ISM component constructors now set `RpgHarvestable`, which also changes their object type from WorldDynamic to WorldStatic; no query in the project filters harvestables by object type.
- A dedicated server is not tested separately, as before.

##### Playtest

1. Open `Lvl_HarvestPickaxe`, pick up the axe, walk up to a dead pine until the trunk is about an arm's length ahead and click. Wood arrives at the moment the axe meets the trunk, with a small dust puff; the camera's centre does not need to point at the trunk. From about 2 m away the swing misses and takes nothing.
2. Chop beside a second pine: one swing takes one section of one tree only.
3. Swing the pickaxe at an iron vein near the ground: ore arrives on the strike. Strike a pine with the pickaxe: nothing, and the swing is used up.
4. Start a chop and dodge before the axe lands: nothing is taken.
5. Hit a sword at a pine: the blade passes through the trunk; an enemy behind it still takes the hit.
6. **Multiplayer:** play as listen server with two players. The client chops a pine the host is watching: the host sees the swing and the dust puff once, the client sees its own puff once without delay, and both see the stock drop by one section. Let a third player join late: the pine shows the reduced stock.

### HRW-04: hold to swing and stock indicator

- Both swing abilities activate while the input is held, end at montage
  blend-out, and stop until the next press after a cancel such as a dodge.
- The aim query and highlight go away for tool swings. The stock indicator
  shows the last struck resource for a few seconds.

#### HRW-04 report

Branch `claude/hrw-04-hold-to-swing`, PR [#339](https://github.com/Athurito/SurvivalRpg/pull/339): `5f654ae6` implementation, assets and tests, then this report with the skill notes and the status row. Holding the harvest button keeps swinging the axe or pickaxe while the player moves, and the stock of a struck resource shows for three seconds after each hit instead of an aim marker.

- **Hold loop:** `GA_Harvest_AxeChop` and `GA_Harvest_PickaxeStrike` use the `WhileInputActive` policy, as Block does, so `URpgAbilitySystemComponent::ProcessAbilityInput` starts the next swing while the input is held. A tool swing now ends at its montage's blend-out (`OnBlendOut`): after 1.317 s for the axe and 1.75 s for the pickaxe. The next swing's montage replaces the one blending out in the `UpperBody` slot group, and `ABP_RpgGasp_CMC_Manny` keeps the legs on locomotion while moving (HRW-01). The ability only holds the facing; movement stays free.
- **Cancels:** a cancelled activation of a `WhileInputActive` harvest ability calls `SuppressAbilityInputUntilRelease`, so after a dodge, block, attack, tool switch, stagger or another montage in the slot group the next swing waits for a fresh press. A rejected prediction ends without a cancel and retries in the next frame, as Block does.
- **Activation group (finding):** both swings inherited `Exclusive_Blocking` from `URpgGameplayAbility_Harvest`. That blocks every exclusive activation, so the dodge (`GA_Combat_Dodge`, also exclusive) could not start during a swing at all, contrary to what the HRW-01 and HRW-03 playtest notes assumed; with the hold loop it would have been locked out for as long as the button was held. Both swing assets are now `Exclusive_Replaceable`: a dodge, block or melee attack replaces a swing. Harvesting powers keep `Exclusive_Blocking`.
- **Server authority:** the owning client predicts each activation, and the server validates each one as before (hand role, skill, tool). Every swing is its own activation with its own contact set, `SwingTargets` count and root action, and `EndAbility` closes the window without its final sample, so a held loop commits at most once per resource and swing. A client may end or cancel its own activation (`ServerEndAbility`), which the server honours. The server therefore records in `SetRemoteInstanceHasEnded` when the next swing may start, the swing's start plus its montage cycle minus 0.2 s of jitter tolerance (`ToolSwingCadenceToleranceSeconds`), and `CanActivateAbility` refuses an earlier swing with `Ability.ActivateFail.Harvesting.SwingCadence`. Swings that the server ends itself, at its blend-out or through its own dodge, hold nothing back.
- **Stock indicator:** `URpgHarvestTargetingComponent` runs no view query for a tool swing and marks nothing before a hit. The swinging client calls `NoteStruckTarget` with the struck resource at its predicted contact, a listen server after its commit. The preview then shows that resource for `StruckTargetLingerSeconds` (3 s, designer-tuned on the component), and each further hit restarts the time. The 15 Hz refresh re-evaluates it from the replicated stock without a trace (`URpgGameplayAbility_Harvest::EvaluateStruckTarget`), so the server's commit or rejection corrects what the client shows. View-ray powers keep their aim query and indicators. `WBP_HarvestTargetIndicator` needed no change: outside an aim preview it shows the stock ("3/4" and a conversion suffix) or the status ("Wrong tool", "Depleted"). The widget base reports no weak point for a struck resource and gained the read-only `IsStruckTargetPreview`.

##### Tests

- Native `SurvivalRpg.Harvesting.Swing.*`: `HoldKeepsSwingingUntilRelease` (through the real input processing: a held input restarts the swing at its blend-out, never while it runs; a release lets the running swing finish), `CancelStopsHoldUntilPressedAgain` (a cancel or an interrupting montage stops the hold until release and a new press), `ClientCannotOutpaceTheMontage` (the cycle helper, a client-cut swing holds the next one back until the cycle is over, server-side ends hold nothing back) and `StruckTargetIndicatorLingers` (no aim query or indicator before a hit, the stock appears at the hit, stays 3 s and hides, a predicted contact is corrected by the replicated result, a further hit restarts the time). The existing indicator tests are unchanged and use view-ray abilities.
- Asset contracts: `AxeToolContract` and `PickaxeToolContract` also require `WhileInputActive` and `Exclusive_Replaceable`.
- PIE `SurvivalRpg.Network.HarvestSwingPIE` gained `RemoteHoldSwingsOnceEachUntilDodgeAndPressesAgain`: a remote client holds the real axe's input on a listen server with 60 ms lag and swings three times, each taking one section once. Each held swing starts on the server between the cycle minus the tolerance and the cycle plus 0.35 s after the one before; measured 1.304, 1.327 and 1.273 s against the 1.317 s cycle. Before the first hit the client's preview marks nothing; after it, the indicator shows the struck resource with the replicated stock. A real dodge during the fourth windup cancels that swing on both machines, and no further swing starts for 3 s while the input stays held. After release and a new press, one swing commits once. Finally the client walks away from the field while holding: two more swings run while it moves (up to 600 cm/s), the server moves the character too, and no swing commits twice. The field has six sections now.
- The PIE fixture now keeps the swing that `ED_Tool_Axe` grants with the main-hand slot and asserts it is the only one. It used to grant `AS_Tool_Axe` a second time, which `TryActivateAbility` by handle hid; with a real press both swings activated and the second replaced the first at once.

##### Validation

All through `ue.py`, native and PIE suites in separate calls, with the MCP server's autostart off for test runs; the MCP editor of this worktree was closed for every build and test.

- `build`: passed, 0 errors, 0 warnings, after the C++ changes and after each test fix.
- `test SurvivalRpg.Harvesting.Swing SurvivalRpg.Harvesting.Content.AxeToolContract SurvivalRpg.Harvesting.Content.PickaxeToolContract SurvivalRpg.Harvesting.Ability.TargetStatusAndIndicatorLifecycle --null-rhi`: first 13 / 14. `CancelStopsHoldUntilPressedAgain` failed because the test changed the native test ability's class default at runtime, which the granted instance did not copy (fixed in the test, noted in the GAS test pitfalls). Then **14 / 14 passed**.
- `test SurvivalRpg.Harvesting SurvivalRpg.Interaction.Harvesting SurvivalRpg.Combat --null-rhi`: **169 / 169 passed** (12 with warnings).
- `test SurvivalRpg.Network.HarvestSwingPIE` (rendered): first 1 / 2. The new hold step timed out: a temporary log showed one server activation of the tracked swing, cancelled 63 ms after the press, while the stock still dropped every 1.32 s, because the fixture's second axe swing had replaced it. With the fixture fixed **2 / 2 passed**.
- `test SurvivalRpg.Network.HarvestSwingPIE SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.Combat.Block` (rendered, separate call): **44 / 44 passed** (28 with the usual PIE log warnings), no "no more RPCs" line, no rerun needed. Held swings started 1.318, 1.332 and 1.306 s apart (cycle 1.317 s); owner predictions 4 of 4 and 2 of 2 tool hits.
- Editor (MCP, port 8000): both swing assets read back `WhileInputActive` and `Exclusive_Replaceable` after compile and save. `WBP_HarvestTargetIndicator` was only read; reading its graph dirtied it, so it was saved to close the editor cleanly and restored from git.
- `python Build/Tools/AgentSkills/sync.py` and `--check`: consistent.

##### Limits

- No agent PIE check by eye on the real trees and veins of `Lvl_HarvestPickaxe`; the PIE test strikes a 100 cm cube with the real axe, montage, AnimBP and dodge. The indicator's text is covered through the widget's read model, not a screenshot.
- The pickaxe's hold is covered by its asset contract only, not by a PIE swing.
- The cadence tolerance is 0.2 s, so a client that cuts its swings short can still swing about 15 % faster than the montage (the axe every 1.117 s instead of 1.317 s).
- When the server rejects a held swing's prediction, the client's swing ends without a cancel and the held input tries again in the next frame, as Block does; a lasting mismatch keeps sending activations while the button is held.
- A remote client's indicator shows the resource its own trace struck. When the server's trace struck another one, it shows the predicted resource with its unchanged stock; when only the server's trace struck, that client shows nothing.
- A dodge that starts in the single frame between two held swings cancels no swing, so the held input swings again once the dodge allows it. Not observed.
- `Exclusive_Replaceable` also lets other exclusive abilities, such as block, attack or item use, replace a swing, and a swing press can replace the replaceable recovery of another ability, as a melee attack can.
- The hold step watches the swinger from the listen server only; no second client sees the held swings as a simulated proxy. They replicate as single swings do, which the late joiner of the other `HarvestSwingPIE` case covers.
- A dedicated server is not tested separately, as before.

##### Playtest

1. Open `Lvl_HarvestPickaxe` and pick up the axe. Look anywhere near a dead pine: nothing marks it.
2. Stand an arm's length from the pine and hold the left mouse button: the chops repeat without a pause, each takes one section, and "3/4" (then "2/4" and so on) appears over the pine at each hit. About 3 s after the last hit it disappears.
3. Hold the button and run past a row of pines: the arms keep chopping while the legs run, and each pine in reach shows its stock when it is hit.
4. Release the button during a chop: that chop finishes, and no further one starts.
5. Hold the button and dodge during a chop's wind-up: the dodge starts at once, that chop takes nothing, and no new chop starts while the button stays held. Release it and press again: chopping resumes.
6. Pick up the pickaxe and hold the button at an iron vein: strikes repeat about every 1.75 s, and the ore's stock shows after each strike. Strike a pine with it: "Wrong tool" shows over the pine.
7. **Multiplayer:** play as listen server with two players. The client holds the button at a pine while the host watches: both see every chop once with one dust puff, the stock drops by one section per chop on both screens, and the client's own "x/4" appears at its hit. The client dodges while holding: on both screens the chopping stops until the client presses again.

### HRW-05: focus meter and harvesting spells

- Starts with a short design round with the user: fill rate, key, duration and
  further spells.
- The focus value is server-authoritative runtime state replicated to its
  owner; the UI only reads it. Spells are `GA_*` Blueprints with timed
  GameplayEffects. Ability tunings must also come from active timed effects,
  not only from the skill tree.
- Covers the "magical harvesting ability" of the first playable in
  [game-vision.md](game-vision.md) §12, open since Rift Grip was retired.

## HRW-01 report

Branch `claude/hrw-01-tool-animations`. The hit model is unchanged: swings still commit through the camera-aim query.

### Assets

- **Clips:** `IP_Chop_Loop_01` and `IP_Pick_axe_hit_01` from `505_Survival_Mocap_Animations/Animations_UE5/IP` in `GF_Harvesting_Magic/Content/Animations/505_Survival_Mocap`. The pack skeleton has the same 161 bones, order and reference pose as the project's `SK_Mannequin`, so the clips were re-pointed without retargeting; the pack skeleton and mesh stayed in the library. Both clips keep the root at the origin, so no root lock is needed.
- **Chop direction:** the mocap chop lands 54.5° to the right of the character's forward (a side-on stance at the trunk). A tool trace in HRW-03 would miss a tree the player faces, so the chop's root keys were turned by -54.5°: the head now strikes 1.05 m straight ahead at hip height. The standing pose keeps the side-on stance.
- **Montages** (one `Default` section at 0, slot `UpperBody`, blend 0.25/0.25 s, one cycle cut from the clip with the new `trim_montage_range` tool):

  | Montage | Clip range | Length | Window start | Commit | Window end |
  | --- | --- | --- | --- | --- | --- |
  | `AM_Harvest_AxeChop` | 0.600-2.167 s (frames 18-65; starts and ends in the same wind-up pose, so repeated swings loop) | 1.567 s | 0.367 s | 0.500 s (axe enters the trunk) | 0.567 s |
  | `AM_Harvest_PickaxeStrike` | 0.667-2.667 s (frames 20-80) | 2.000 s | 0.733 s | 1.033 s (pick stops in the rock) | 1.100 s |

  The window notifies are queued, 100 %, server-delivered and end at least 0.9 s before the auto blend-out. `GA_Harvest_AxeChop` and `GA_Harvest_PickaxeStrike` use them; `AM_Harvest_PickaxeSwing` and `MM_Harvest_PickaxeSwing` had no other referencers and are deleted.
- **Moving while swinging:** `ABP_RpgGasp_CMC_Manny` had no `UpperBody` slot (block uses linked layers since GASP-06), so it got one between the block layer and `DefaultSlot`: cached poses `RpgPreUpperBodyMontage` and `RpgUpperBodyMontage`, the `UpperBody` slot (source kept updating), a layered blend from `spine_01` with mesh-space rotation, and a bool blend on the AnimBP's thread-safe `IsMoving()` (0.2 s): moving keeps the locomotion legs, standing plays the whole montage. Without an `UpperBody` montage the pose passes through unchanged, so block, `DefaultSlot` priority and the snapshots stay as they were. `UpperBody` shares `DefaultGroup` with `DefaultSlot`, so a dodge still interrupts a swing. The harvest ability only holds the facing during the swing; it never locked movement. The Mover AnimBP runs on the UEFN skeleton, which these montages do not target, and is unchanged.
- **Tool meshes:** the axe is the library's `SM_Axe_02` (single-bit head, straight wooden handle) with its material in `Equipment/Tools/SkeletonCharacter`; the master's texture defaults now point at the axe textures, so the sword textures stayed in the library. The pickaxe `SM_Tool_Pickaxe` is built with Geometry Script (wooden handle, iron head with pick and blunt back, 192 vertices) with `M_Tool_Basic` and two instances in `Equipment/Tools/Meshes`. Both have `TracePoint_Base` and `TracePoint_Tip`: the axe along its cutting edge (z 50 to 60 cm on the edge), the pickaxe from the head centre to the pick point.
- **Grip:** both tool actors now hold one `ToolMesh` instead of the cube/cylinder placeholders, under `Grip_OneHand_R` as before. Its rotation puts the handle through the left hand's grip of the clip (26° and 42° from the socket's Z) with the edge or pick leading at impact, and the right palm 10 or 13 cm up the handle.
- **Editor tooling:** `RpgAnimationAssetTools` gained `TrimMontageToRange`, `RotateSequenceRoot` and `CreateSave/UseCachedPoseNode`, with `AnimationAssetTools` wrappers; `SurvivalRpg.Animation.TrimMontageToRange.KeepsTheRangeAndItsNotifies` tests the trim.

### Validation

- `ue.py build`: passed, 0 errors, 0 warnings (after the tool additions and after the contract test).
- `ue.py test SurvivalRpg.Harvesting SurvivalRpg.Animation SurvivalRpg.Interaction.Harvesting SurvivalRpg.Combat.Block SurvivalRpg.GASP.CMC SurvivalRpg.Movement.CMC --null-rhi`: **169/169 passed** (21 with warnings), including `Harvesting.Ability.CommitDelayFromAuthoredNotify`, the axe and pickaxe tool contracts, `Animation.TrimMontageToRange` and `RetimeMontageRange`.
- `ue.py test SurvivalRpg.Harvesting.Content SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.GASP.CMC SurvivalRpg.GASP.Mantle SurvivalRpg.GASP.Vault SurvivalRpg.GASP.Hurdle SurvivalRpg.Combat.BlockLocomotion SurvivalRpg.Combat.WeaponHold` (rendered): **97/98 passed** (69 with warnings), including the new `Harvesting.Content.SwingMontageContract`, both harvest network tests, the CMC experience, host-view and sprint tests, mantle, vault, hurdle, block locomotion and weapon hold. Failed: `CombatRemoteMeleePIE.RemoteClientRecoveryYieldsToBlockAndDodgeButNotTheSwing`, timed out after 90 s waiting for the owner's sword swing to open its attack window. Rerun alone (`ue.py test SurvivalRpg.Network.CombatRemoteMeleePIE`): **5/5 passed**, that case included. The GASP handoff already lists this test as time-dependent; the step polls for a short window.
- After merging `master` with HRW-02 (`d5f2da83`): `ue.py build` passed (0 errors, 0 warnings); `ue.py test SurvivalRpg.Harvesting SurvivalRpg.Animation --null-rhi` **97/97 passed**; `ue.py test SurvivalRpg.Network.LootHarvestPIE SurvivalRpg.Network.HarvestCuePIE SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.Combat.Weapon` **22/22 passed**. A first attempt with all of these filters in one invocation crashed after 43 s: a `SurvivalRpg.Harvesting.Persistence` test leaves its `HarvestRenewableAreaTest` world referenced (`RpgHarvestAutomationInstancesActor` → its profile), and the next PIE test's `NewMap` fails the engine's world-leak check. The test file is unchanged on this branch; split such runs.
- The Mover tests did not run: the Mover AnimBP and its UEFN skeleton are unchanged and do not play these montages.
- **Agent PIE check** in `Lvl_HarvestPickaxe`, listen server with two players in one process, driven by editor Python and `set_pie_input_key`, sampled every frame:
  - Host with the axe, client with the pickaxe (picked up with F after clearing the sword and shield); both tools sit in the right hand on both machines.
  - Host chopping while running: 375 cm/s through the whole swing, `UpperBody` weight 1, `DefaultSlot` 0, the feet kept stepping; the client's copy played `AM_Harvest_AxeChop` at the same time. Screenshot: running legs under the chop.
  - Client striking while blocked by a vein (speed 0): the full-body overhead strike; the host's copy played it too.
  - Yield through the unchanged camera aim: the client's pickaxe took 6 ore from a vein, then 6 more on a strike while walking in; the host's chop, run in at a dead pine with the trunk at the screen centre, took 5 wood (the pine fell), and a standing chop on another pine took one section (4/4 to 3/4) with the wood toast. From the side, the client's screen showed the host's standing chop with the axe straight ahead.
  - Off-centre aim showed "Out of reach" for a trunk beside the screen centre; that is the HRW-03 problem, not a regression.
  - Under screenshot stalls of the game thread, the client's copy of a swing once ran 0.36 s ahead and then resynced to the server's position.

### Limits

- The axe hit still comes from the camera ray: from 2.2 m the player harvests although the axe stops short of the trunk, and a trunk off the screen centre reads "Out of reach". HRW-03 replaces this with the tool trace.
- The pickaxe and chop speeds are the mocap's (commit after 0.5 s and 1.03 s); `MontagePlayRate` on the abilities stays 1.
- The carry pose holds the tool tilted to fit the two-handed swing grip; a dedicated tool hold layer is not part of this task.
- Standing to moving switches the leg source over 0.2 s. The standing chop stance is side-on, so its blend-out turns the feet back to the locomotion pose; how that reads was not judged by eye.
- A dodge interrupting a swing follows from the shared slot group but was not checked in PIE.
- Mantle and jump do not check the `UpperBody` slot (they check `DefaultSlot`), so a traversal can start during a swing and overrides it visually.
- The world pickups `BP_Pickup_Axe` and `BP_Pickup_Pickaxe` still show the lying cylinder; their display mesh also carries the interaction collision.
- No comment boxes in the AnimGraph: the 5.8 tools cannot author them; node names carry the meaning.

### Playtest

1. Open `Lvl_HarvestPickaxe` and pick up the axe (look at it, F). It sits in the right hand with the head up.
2. Run at a dead pine and click while running: the arms chop while the legs keep running; the swing lands straight ahead. Stand still and click: the whole body takes the side-on chop stance. With the trunk at the screen centre, wood arrives on the strike.
3. Pick up the pickaxe instead, walk at an iron vein and click: an overhead strike while walking; ore arrives on the strike.
4. Dodge during a swing: the dodge interrupts it.
5. **Multiplayer:** play as listen server with two players. The host chops a pine while running, the client strikes a vein. Each screen shows the other player's swing, running legs while moving and the full stance while standing.

## Order

- HRW-01 and HRW-02 share no files and can run in parallel.
- HRW-03 starts when both are merged; HRW-04 follows HRW-03, and HRW-05 follows
  HRW-04.
- Every gameplay task runs the listen-server PIE tests and adds a two-player
  step to its playtest.
