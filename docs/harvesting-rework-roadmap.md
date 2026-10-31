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
| HRW-02 | Extract the core swing window and trace (`FRpgSwingTraceWindow`) from the melee attack ability without behaviour change | Planned |
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
