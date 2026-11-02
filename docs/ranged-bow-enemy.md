# RNG-05: The bow enemy

Task report for RNG-05 in the [ranged combat roadmap](ranged-combat-roadmap.md), PR pending. A skeleton archer, the Rift Grunt body with a bow, keeps its distance, shoots readable arrows through the player's own bow ability and repositions when a wall or an ally is in the way or a player presses in. Its fire is unlimited, and only five shots per life return arrows. Integrated acceptance and feel tuning are RNG-06.

## C++ boundary decision

- **Classification:** extended reusable mechanisms plus designer content.
  - The AI attack driver gains a ranged shot lifecycle, a line of fire and a ballistic aim point.
  - The StateTree gains a ranged attack task and a line-of-fire condition.
  - The AI controller can aim at a point.
  - A ranged weapon declares its ammunition policy.
  - The bow enemy itself, its weapon, loadout, AnimBP, StateTree and map placement are content.
- **Runtime truth:** the server. The StateTree runs only in the server's `ARpgAIController`. The shot is the same server-authoritative `GA_BowShot` activation a player uses, and the projectile, damage and recovery commit are RNG-01 to RNG-03c. Nothing new replicates; clients see the draw through the replicated ranged state tags, the engine's replicated view pitch and the hold layer that the equipment links on every machine.
- **Existing seams:**
  - `URpgAIAttackDriverComponent`, whose melee sequence already activates the attack-input ability;
  - `URpgAbilityTask_ProjectileShot` and `RpgProjectileShot`, the shot and its Unlimited policy with the RNG-03c recovery budget;
  - the CMB-06 StateTree tasks (`Combat Control`, `Keep Combat Distance`);
  - `URpgEnemyCombatLoadoutComponent` and the archetype tag;
  - the CMB-02 hold layers.
- **Native, and why:** releasing an ability's input, reading the server's draw progress and tracing the projectile channel are engine-facing and server-authoritative. The controller's view pitch can only be kept by overriding `GetFocalPointOnActor`. StateTree nodes are native structs in this project.
- **New native classes:** none. Two new StateTree structs follow the existing task and condition pattern: `Ranged Attack` and `Combat Target In Line Of Fire`.
- **Roadmap reconciliation:** the roadmap asked to remove melee-only restrictions so one bow ability serves both. The melee sequence still requires a melee attack, and ranged shots get their own driver calls, because a held shot and a combo chain end differently. The NPC uses the player's `GA_BowShot` unchanged; its weapon declares the unlimited policy, so no NPC variant of the ability exists.
- **Asset work (Unreal MCP and editor Python):**
  - enemy bow chain and loadout in `GF_Combat_Ranged/AI`;
  - `ABP_Enemy_Archer` in `GF_Combat_Core`;
  - bow enemy, controller, PawnData and StateTree in `GF_AI_RiftMonsters`, built from [combat-enemy-archer-statetree.json](assets/combat-enemy-archer-statetree.json);
  - one archer in `Lvl_CombatSandbox`.

## Behavior

### Problem

After RNG-04 only players could shoot. The AI attack driver accepted only melee weapons, nothing drew, held or released a shot for an AI, and NPC fire had no ammunition source or bounded recovery.

### Change

- **Driver** (`URpgAIAttackDriverComponent`):
  - `StartRangedShot` faces the combat target and activates that weapon's own ability on the attack input of a ranged main-hand weapon.
  - `ReleaseRangedShot` sends the input release a player's mouse sends. `CancelRangedShot` lowers an unreleased draw without cost; it also lowers a release whose ability never ended.
  - `GetRangedDrawFraction` reads the server's draw progress.
  - `HasLineOfFire` traces the projectile channel from the body at the weapon's launch height to the target's body, so an obstacle closer than the muzzle blocks too.
  - `GetRangedAimPoint` solves the low ballistic arc of a full-draw shot.
  - Melee attack sequences and the timer-driven melee attacks are unchanged; ending external control also lowers a drawn shot.
- **Aim** (`ARpgAIController::SetFocusAimPoint`): the view aims above the target by the arc's elevation while the target stays the focus actor, so the engine keeps the pitch. The shot reads that view like a camera. The archer does not lead a moving target; a sidestep during the draw dodges the arrow.
- **Ammunition** (`URpgRangedWeaponInstance::AmmunitionPolicy`):
  - A weapon declares where its shots take their ammunition from. `Start Projectile Shot From Weapon` uses a weapon's Unlimited policy over the ability's own, but only for a shooter without a player inventory: a player who somehow holds the NPC bow still pays arrows.
  - Player bows keep ShooterInventory. `BP_WeaponInstance_EnemyBow` declares Unlimited with `UnlimitedRecoveryBudget` 5 and no release stamina, so the first five released arrows of a life come back as RNG-03b/c recovery and later ones never do.
- **StateTree** (`ST_SkeletonArcher`):
  - **Idle** without a target.
  - **Recover** while staggered.
  - **Evade** when a target comes within 4.5 m: run back toward 8 to 12 m for up to 2.5 s, then shoot from where it stands.
  - **Approach** beyond 16 m: run toward 13 m for up to 3 s, then shoot, so a target it cannot reach is still shot at.
  - **Engage**:
    - **Reposition** without a line of fire: strafe at 8 to 13 m for 1 to 2 s.
    - **Shoot** with the `Ranged Attack` task: stand, draw, hold the full draw for 0.4 to 0.8 s, release when the line of fire is clear. It lowers the bow after 1 s without a line of fire or after 6 s, and a released shot whose ability does not end within 1 s.
    - **Circle** at 8 to 13 m for 1.5 to 3 s, which also falls back to Evade under close pressure, then shoot again.
- **Presentation:** `ABP_Enemy_Archer` is `ABP_Enemy_SwordShield` with the `ALI_RpgMainHandHold` layer before the block layer, so the bow's hold layer from RNG-03 links on every machine: lowered bow, aim and full draw by pitch, release. The warrior keeps `ABP_Enemy_SwordShield` unchanged.

### Content

| Asset | Role |
| --- | --- |
| `GF_Combat_Ranged/AI/EnemyWeapons/BP_WeaponInstance_EnemyBow` | Child of `BP_WeaponInstance_Bow`: same shot and aim; `AmmunitionPolicy` Unlimited, `UnlimitedRecoveryBudget` 5, release stamina 0 |
| `GF_Combat_Ranged/AI/EnemyWeapons/ED_EnemyBow`, `ID_EnemyBow` | `ED_Bow`'s actor, grip, `AS_Bow` and hold layer with the enemy instance; an item only enemy loadouts equip |
| `GF_Combat_Ranged/AI/CombatLoadouts/DA_EnemyLoadout_Bow` | `Enemy.Archetype.Bow`: the enemy bow in the main hand |
| `GF_Combat_Core/Animations/Enemy/ABP_Enemy_Archer` | Enemy AnimBP with the main-hand hold layer |
| `GF_AI_RiftMonsters/AI/Characters/BP_RiftGrunt_Bow` | Child of `BP_RiftGrunt_Sword`: Bow archetype and loadout, `ABP_Enemy_Archer`, `BP_AIC_RiftGrunt_Bow` |
| `GF_AI_RiftMonsters/AI/Controllers/BP_AIC_RiftGrunt_Bow`, `PawnData/DA_RiftGrunt_Bow_PawnData`, `StateTrees/ST_SkeletonArcher` | The archer's controller, PawnData and tree |
| `GF_Dev_Sandbox/Maps/Lvl_CombatSandbox` | `SkeletonArcher_01` at (1800, 2400), facing the player starts |
| `Config/DefaultGameplayTags.ini` | `Enemy.Archetype.Bow` |
| `GF_AI_RiftMonsters.uplugin` | Now depends on `GF_Combat_Ranged` |

## Validation (2026-11-02)

- **Build:** `python Build/Tools/Unreal/ue.py build` passed on every step, the last after the review fixes.
- **New tests:**
  - `SurvivalRpg.Combat.ProjectileShot.LaunchDirectionArcsOntoTheTarget`: the low-arc solver's launch passes the target analytically within 1 cm; out of reach, no gravity and straight-up cases.
  - `SurvivalRpg.Combat.Ranged.EnemyBowAssetContract`:
    - the enemy bow grants exactly the player bow's abilities;
    - the player bow consumes arrows, while the enemy bow is Unlimited with a positive budget, the standard recoverable arrow and a Manny hold layer;
    - the bow enemy's archetype loadout equips it;
    - its AnimBP implements `ALI_RpgMainHandHold`;
    - its controller's tree contains the `Ranged Attack` task.
  - `SurvivalRpg.Network.RangedEnemyPIE.BowEnemyShootsThePlayerThroughTheSharedBowShot`: a listen server with two clients and 60 ms simulated lag spawns `BP_RiftGrunt_Bow` 10 m from a player, behind a wall.
    - Its StateTree controls the driver with the Unlimited bow and targets the player. While the wall blocks the line of fire it draws nothing for 3 s.
    - After the wall goes, it releases a shot that takes a recovery token, and the arrow hits the player.
    - The other client shows the bow's hold layer on the archer and sees the replicated draw.
    - It spends all five tokens, draws again and hits once more without a sixth token.
- **Final regression on the review fixes:**
  - native, with `--null-rhi`: `SurvivalRpg.AbilitySystem`, `Inventory`, `AI`, `Interaction`, `Equipment`, `Loot`, `Save`, `Input`, `UI`, `Crafting`, `Harvesting` and `Progression`, 511/511;
  - `SurvivalRpg.Combat` with rendering and the 90 s network command timeout, 102/102, including the block-locomotion and weapon-hold PIE tests;
  - `SurvivalRpg.Network`, 47/47, including the melee, projectile and ranged aim PIE tests.
- **After the last test change:**
  - `SurvivalRpg.Network.RangedEnemyPIE`, 1/1;
  - `SurvivalRpg.Combat.Ranged`, `ProjectileShot`, `Projectile` and `SurvivalRpg.AI` with `--null-rhi`, 49/49.
- **GPU crash:** two earlier runs of all native suites in one rendered process stopped with a GPU driver crash (`DXGI_ERROR_DRIVER_INTERNAL_ERROR`), both at `SurvivalRpg.Inventory.TransferDelta.RejectedLegacyPartialTransferIsSideEffectFree` after about 295 tests. That suite passed alone, with rendering 5/5 and with `--null-rhi` as part of 227/227, so the final regression split the rendered suites from the rest.
- **Manual PIE in `Lvl_CombatSandbox`** (editor, standalone, scripted through MCP; window captures not committed):
  - the archer noticed the player at about 17 m, ran closer and shot;
  - it walked sideways between shots, and when teleported to 2.7 m it ran about 5 m back before shooting;
  - full-draw arrows took 30 health each;
  - the capture showed the raised, drawn bow and the lowered bow after the release.
- **Review:** two review agents read the diff, one on authority and lifecycle and one on contracts and tests, and each finding was checked against the code.
  - **Fixed:**
    - an NPC weapon's Unlimited policy reached players too;
    - Approach never ended against an unreachable target in sight;
    - a release whose ability never ended held the bow forever;
    - the driver could draw another ability on the attack input;
    - the line of fire missed obstacles closer than the muzzle;
    - an uninitialized aim point;
    - the PawnData's pawn class pointed at the sword grunt;
    - the gameplay tag INI churn;
    - the budget assertion could not fail;
    - doc gaps.
  - **Limits:** the remaining findings are listed below.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

## Playtest

1. Open `GF_Dev_Sandbox/Maps/Lvl_CombatSandbox` and start PIE in standalone. The skeleton warrior attacks first; the archer stands at the far northeast end of the arena.
2. Walk toward the archer until it notices you, about 30 m away. **Expected:** it runs closer to about 13 m, stops, raises the bow, draws for about a second, holds briefly and shoots. You can see the draw coming.
3. Sidestep during the hold. **Expected:** the arrow misses; standing still, it hits for about 30.
4. Stay at range. **Expected:** between shots it walks sideways around you at 8 to 13 m, then shoots again.
5. Put the low platform or a wall between you. **Expected:** it moves sideways until it sees you again instead of shooting into the cover.
6. Run at it. **Expected:** closer than about 4.5 m it runs back, then shoots from where it stands.
7. Kill it after it shot a few times. **Expected:** arrows that stayed intact lie in the world or in bodies as in RNG-03b/c. After five shots its arrows no longer leave anything.
8. **Multiplayer:** listen server with two players. **Expected:**
   - both see the archer raise, draw and release the bow;
   - its arrow hits the player it targets and is seen by both;
   - the second player can draw its attention by hitting it.

## Limits

- **Feel:** the enemy bow uses the player bow's draw, damage and speed curves. A full-draw arrow takes 30 health, so four arrows down a fresh player who stands still. Cadence, damage and hold time are starter values for RNG-06.
- **Aim:**
  - The archer aims at the target's current position with ballistic elevation and never leads, so sidestepping during the hold dodges the arrow.
  - Its view sits about 30 cm above the muzzle, so an arrow can land up to about 30 cm above the body center.
- **Inherited from the warrior:** `BP_RiftGrunt_Bow` is a child of `BP_RiftGrunt_Sword`. It keeps the warrior's body, reactions, defense profile, corpse loot and death. `ABP_Enemy_Archer` is a copy of `ABP_Enemy_SwordShield`, so a change to one does not reach the other, and the archer's legs use the sword-and-shield locomotion.
- **Not tested automatically:**
  - repositioning movement, Evade and Approach (the PIE test map has no navmesh; they were only seen in the sandbox);
  - an ally in the line of fire (only the trace's channel setup speaks for it);
  - the shooter's death mid-draw;
  - late join;
  - a corpse's recovered arrows from NPC shots end to end;
  - the release timeout after a blocked input.
- **Stamina:** the enemy bow costs no stamina, so the AI's stamina never stops its fire. Whether AI stamina regenerates was not inspected.
- **Graph comments:** the new hold-layer node in `ABP_Enemy_Archer` has no comment box, which the 5.8 tools cannot create; its node title names the layer.
- **Placement:** `ABP_Enemy_Archer` lives in `GF_Combat_Core` beside `ABP_Enemy_SwordShield`, because it references only core content.
- **Melee sequence:** it still requires a melee primary attack. Ranged shots use their own driver calls instead of a shared attack sequence.
