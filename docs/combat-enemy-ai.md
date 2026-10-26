# CMB-06: Enemy AI

Task report for CMB-06 in the [combat roadmap](combat-roadmap.md), [PR #295](https://github.com/Athurito/SurvivalRpg/pull/295). The skeleton warrior from [CMB-05](combat-skeleton-warrior.md) gets real behavior: it approaches, keeps its distance and circles, telegraphs its attacks so the perfect block can read them, chains one to four hits, blocks the player's attacks, recovers from stagger and backs off after a combo. The behavior is a StateTree on the existing AI drivers and loadouts.

## Design decisions (user, 2026-10-26)

- **Difficulty:** medium, Souls-like. The warrior circles at 3 to 4 m, attacks every 2 to 4 s with one to three hits, blocks about 40 % of the player's attacks and retreats after some combos. Dangerous alone, but readable.
- **Telegraph:** the enemy's own attack montages hold the wind-up, so the hit lands about 0.5 s after the swing starts, and a short glow plays at the start (placeholder effect until the [VFX roadmap](vfx-roadmap.md) replaces it). The player's montages stay unchanged.
- **Target choice in co-op:** configurable per enemy: the nearest player, or the last attacker with a switch cooldown against ping-pong. A threat meter ("aggro") for a later tank role is recorded under "Later" in the roadmap and plugs in as a further policy.
- **Hyper armor:** only the heavy finisher (combo step 4) cannot be staggered by the meter; perfect block and dodge stay the answers to it.

## C++ boundary decision

- **Classification:** extension of the existing AI mechanism (StateTree AI tasks, `URpgAIAttackDriverComponent`, `URpgAIBlockDriverComponent`) and of the attack schema (`FRpgWeaponAttackDefinition`), two editor-only authoring tools for confirmed MCP gaps, and designer content (StateTree, enemy weapon, montages, cue, movement profile, tuning).
- **Runtime truth:** the server. The StateTree runs only in the server's `ARpgAIController`; attacks, blocks, stagger and death stay server-authoritative GAS abilities whose montages the ability system replicates; the movement component replicates movement and gait. Nothing new is replicated, and late joiners see the warrior through the existing montage and movement replication.
- **Existing seams:**
  - `URpgAIPawnData::StateTree` and the controller's `UStateTreeAIComponent`, with one native task, `Find Hostile Target`, that chases the nearest hostile and hands it to the attack driver.
  - The attack driver presses `InputTag.Weapon.Primary` on timers. It only activates the ability, so the warrior never chains: the combo (CMB-03) chains on a press while a step runs.
  - The block driver blocks after damage, by chance.
  - The stagger meter, `GA_Combat_Stagger` and `State.StaggerImmune`, which the damage execution, the defense set and the stagger ability already respect.
  - `URpgMovementProfile` with walk and run gaits on `URpgCharacterMovementComponent`.
  - Montage gameplay cue notifies, like `GameplayCue.Combat.Swing` from CMB-04.
- **Native, and why:**
  - **Driver control (mechanism):** the StateTree, not the timers, decides when the warrior attacks and blocks. The attack driver gets a StateTree mode with an attack sequence of N combo steps: it presses once to activate, then once per running step until N steps have started, which needs the server's ability spec and the attack ability's step. The block driver gets a timed hold. The timer modes stay the default, so other pawns and the existing tests are unchanged.
  - **Target policy (mechanism):** the attack driver already owns the combat target and sees damage instigators. It gets the policy (nearest or last attacker) and the switch cooldown, so the target has one owner and a threat policy can join later.
  - **StateTree nodes (mechanism):** generic server tasks and conditions that every melee enemy can compose: keep a distance band to the combat target while walking, running, strafing or backing off; run an attack sequence; hold block; wait out stagger; and conditions on the target's distance and on the owner's tags. A global task forwards the target's attack activations (abilities tagged `Ability.Type.Attack`) as a StateTree event, so the StateTree can react once per attack. They are engine-facing (navigation, focus, ability system, StateTree events) and tick for every enemy, so they are native USTRUCT nodes like the existing task.
  - **Hyper armor (schema):** `FRpgWeaponAttackDefinition` gets tags the attacker holds while that step runs, applied on the server. The finisher holds `State.HyperArmor`, which the stagger gates treat like stagger immunity. A separate tag, because the post-stagger immunity timer sets the count of `State.StaggerImmune` and would clear a shared tag.
  - **New native classes:** no UCLASS at runtime; the StateTree nodes are USTRUCTs. Editor-only: a StateTree builder that writes states, tasks, conditions and transitions from JSON, and a montage tool that slows a time range of a montage and moves its notifies, because the engine StateTree toolset only inspects and montage segments cannot be edited through MCP. Both are reusable for later enemies. The builder replaces hand-written C++ per StateTree.
- **Content (Unreal MCP):**
  - The warrior's StateTree.
  - An enemy sword: item, equipment definition and weapon instance derived from the Basic Sword, with four telegraphed montages and the glow cue notify.
  - The SwordShield loadout pointing to it.
  - A movement profile with the pack's walk and run speeds.
  - `Ability.Type.Attack` on `GA_BasicWeaponAttack`.
  - Driver settings on `BP_RiftGrunt_Sword`.
- **Tests:** the native seams once:
  - the attack sequence chains exactly N steps;
  - the target policies and the switch cooldown;
  - the timed block;
  - attack-step tags are added and removed, and hyper armor stops the stagger;
  - the StateTree builder compiles a tree from JSON;
  - the montage tool keeps notify order and length.

  The behavior itself is checked in PIE, standalone and on a listen server.

## Behavior

### Problem

The warrior ran straight at the nearest player and swung the first combo hit every 1.25 s from the attack driver's timers; the driver never chained a second step. It raised its shield by chance after taking damage. Its hit landed 0.13 s after the swing began, too fast to read for the 0.25 s perfect-block window.

### Change

- **StateTree `ST_SkeletonWarrior`** (`GF_AI_RiftMonsters/AI/StateTrees`), built from [its description](assets/combat-enemy-ai-statetree.json) with the new builder:

  | State | Enters when | Does | Then |
  | --- | --- | --- | --- |
  | Idle | No hostile within 30 m | Waits 0.5 s | Selects again |
  | Recover | `State.Staggered` or `State.GuardBroken` | Stands still until the tags are gone | Selects again |
  | Approach | Target farther than 6.5 m | Runs until 4.5 m | Selects again |
  | Circle | Otherwise | Walks sideways around the target at 2.5 to 4 m for 1.5 to 3.5 s, flipping direction now and then | Attack; Defend on a target attack (see below); Approach when the target gets away |
  | Attack | After circling or a block | Closes in, then chains 1, 2, 3 or 4 combo steps (weights 35, 35, 20 and 10 %) | Retreat (50 %), else Circle |
  | Defend | The target starts an attack within 4 m while circling, 40 % chance | Holds the shield 0.6 to 1.0 s, facing the target | Counterattack (50 %), else Circle |
  | Retreat | After some attacks | Walks backward to 4.5 to 5.5 m, at most 2 s | Circle |

  A stagger interrupts Approach and every Engage state at once.
- **Target choice:** the global combat control task hands the drivers to the StateTree and picks the target by policy. The warrior uses the last attacker with a 4 s switch cooldown and drops a target beyond 40 m; the policy is a parameter of the task, so other enemies can choose the nearest player.
- **Telegraphs:** the enemy sword attacks with copies of the combo montages whose wind-up is played slower (`retime_montage_range`):

  | Step | Montage | Hit window opens | Player montage | Wind-up range, play rate |
  | --- | --- | --- | --- | --- |
  | 1 | `AM_EnemySwordShieldCombo01_Step1` | 0.50 s | 0.13 s | 0.00 to 0.12 s at 0.245 |
  | 2 | `_Step2` | 0.42 s | 0.12 s | 0.00 to 0.11 s at 0.268 |
  | 3 (shield bash) | `_Step3` | 0.42 s | 0.12 s | 0.00 to 0.11 s at 0.268 |
  | 4 (finisher) | `_Step4` | 0.85 s | 0.48 s | 0.05 to 0.45 s at 0.52 |

  Combo windows and the swing cue moved with the hit window. At 0.02 s each step plays `GameplayCue.Combat.Telegraph` through `AN_Combat_Telegraph`, which flashes the spark template at the warrior's head on every machine (placeholder effect).
- **Hyper armor:** the finisher holds `State.HyperArmor` on the server. Hits still fill the stagger meter, but it waits one point below the break until the finisher ends; the next hit then breaks it.
- **Movement:** the warrior uses the shared movement profile (`DA_RpgGasp_MovementProfile`): it walks at 165 and runs at 375 cm/s instead of always running at 600. It turns toward its focus at 720°/s instead of snapping to the controller, so it no longer tracks the player during a root-motion swing.
- **Drivers:** without the StateTree's combat control task both drivers keep their timers, so other AI pawns are unchanged.

### Content

| Asset | Role |
| --- | --- |
| `GF_AI_RiftMonsters/AI/StateTrees/ST_SkeletonWarrior` | The warrior's StateTree; `DA_RiftGrunt_Sword_PawnData` points to it |
| `GF_AI_RiftMonsters/AI/Characters/BP_RiftGrunt_Sword` | Movement profile, rotation toward the controller's desired rotation |
| `GF_Combat_Core/AI/EnemyWeapons/ID_EnemySword`, `ED_EnemySword`, `BP_WeaponInstance_EnemySword` | Enemy sword: the Basic Sword's item, equipment and instance with the enemy montages; the finisher holds `State.HyperArmor` |
| `GF_Combat_Core/AI/CombatLoadouts/DA_EnemyLoadout_SwordShield` | Main hand is the enemy sword |
| `GF_Combat_Core/Animations/Combo/SwordShield/Enemy/AM_EnemySwordShieldCombo01_Step1` to `_Step4` | Telegraphed combo steps |
| `GF_Combat_Core/Animations/Notifies/AN_Combat_Telegraph`, `GF_Combat_Core/GameplayCues/GCN_Combat_Telegraph` | Telegraph notify and its placeholder cue (spark template at the `head` socket, scale 0.35) |
| `GF_Combat_Core/GAS/Abilities/GA_BasicWeaponAttack` | Asset tag `Ability.Type.Attack` |
| `Config/DefaultGameplayTags.ini` | `GameplayCue.Combat.Telegraph` |

## Validation

Run on 2026-10-26 on `master` `9da1389a` plus this change.

- **Build:** `ue.py build` passed, 0 errors, 0 warnings.
- **Assets:** data validation of the 15 new and changed assets, including the enemy weapon instance's combo checks: 15 valid, 0 warnings.
- **New tests**, all passed:
  - `SurvivalRpg.AI.AttackDriver.ExternalControlStopsTheTimersAndKeepsTheLastAttackerAfterTheCooldown`
  - `SurvivalRpg.AI.AttackDriver.CountsOnlyTheTargetsAttackActivations`, which also holds `GA_BasicWeaponAttack` to its `Ability.Type.Attack` tag
  - `SurvivalRpg.AI.MeleeAttack.StepCountFollowsTheWeights`
  - `SurvivalRpg.AI.StateTreeBuilder.BuildsCompilesAndReportsErrors`
  - `SurvivalRpg.Combat.HyperArmor.MeterWaitsBelowTheBreakUntilTheTagIsGone`
  - `SurvivalRpg.Animation.RetimeMontageRange.StretchesTheRangeAndMovesLaterNotifies`
- **Regression** (`ue.py test`, own editor closed):

  | Suite | Result |
  | --- | --- |
  | `SurvivalRpg.AI`, `.Combat`, `.Animation`, `.Equipment`, `.Cosmetics` | 80 / 81; the failure is below |
  | The failed `BlockLocomotionPIE.CMCActivationSnapshotsCapAndZeroCapPreservesNormalSpeed`, again with the test editor's MCP server off | 1 / 1 |
  | `SurvivalRpg.Network` (MCP server off) | 28 / 28 |
  | `SurvivalRpg.GASP.CMC` (`np.ForceReconcile 0`, MCP server off) | 14 / 14 |

  The block test failed only on `Call to unknown method "server/discover"`: an outside MCP client reached the test editor's own MCP server, the known pitfall in the [editor session reference](../.agents/skills/unreal-lyra-expert/references/editor-session-and-pie.md).
- **PIE, standalone, `Lvl_CombatSandbox`, 30 s probe with the player attacking every 2.5 s:**
  - The warrior ran in at 375 cm/s, switched to walking at 165 cm/s near 4 m and circled with Direction ±73 to 80° (sideways loops).
  - It attacked with the enemy montages in chains of one, two and three steps.
  - After some combos it walked backward (Direction 180°) to about 4.5 m.
  - Once it raised its shield (`State.Blocking`) right after a player attack.
- **Telegraph cue:** executed on the warrior in PIE, `GCN_Combat_Telegraph` spawned the spark template on the warrior mesh's `head` socket at scale 0.35. During attacks, sparks on that socket appeared at montage time 0.01 and 0.05 s.
- **PIE, listen server with two clients, 25 s:**
  - Server and both simulated proxies showed the same montage in every sample, including one full four-step chain with the finisher.
  - They showed the same speed (proxies trailed by up to 130 cm/s during root-motion steps) and the same Direction (±180° counted as equal).
- **Failed attempts:**
  - **StateTree failed at start:** the first PIE run left the warrior standing, its StateTree `Failed`. The combat control task failed when the drivers did not exist yet, because `GF_Combat_Core` adds them after possession. It now takes control once they appear.
  - **Live Coding crash:** a Live Coding patch of that fix crashed the editor in PIE. The patch relinked the attack driver's debug check, whose file-static console variable was never constructed in the patch. A full build fixed it; the pitfall is in the [authoring reference](../.agents/skills/unreal-lyra-expert/references/unreal-mcp-asset-authoring.md).
  - **Tag INI rewritten:** adding the cue tag with `GameplayTagsToolset.AddTag` rewrote `DefaultGameplayTags.ini` and escaped every apostrophe. The file was restored and only the new line kept.

## Playtest

1. Open `GF_Dev_Sandbox/Maps/Lvl_CombatSandbox` and start PIE. The skeleton runs toward you and slows to a walk about 4 m away.
2. It circles you sideways at 2.5 to 4 m, changing direction now and then, and after 1.5 to 3.5 s it closes in and attacks.
3. Before every swing it visibly holds back for about half a second and a spark flashes at its head; block just before the hit lands to get a perfect block. The finisher (overhead swing, fourth hit) holds back longest.
4. It chains one to four hits. During the finisher a stagger build-up does not stop it; the first hit afterwards does.
5. Attack it while it circles: now and then it raises its shield, and sometimes it counterattacks right after.
6. Break its stagger meter: it stops, plays the stun or stumble, and then resumes the fight.
7. After some combos it walks backward to about 5 m before circling again.
8. **Multiplayer:** listen server with two clients. Both clients see the same circling, attacks, blocks and retreats. Hit it with the second player: after a few seconds it turns to that player.

## Limits

- **Placeholder telegraph effect:** the glow is the spark template at the head. It spawns on the warrior's `head` socket when the cue runs, but no capture shows it clearly, so how readable it is still needs the playtest. A proper effect belongs to the [VFX roadmap](vfx-roadmap.md).
- **Wind-up by slowing the clip:** the held wind-up plays the first frames of the pack clips at a quarter speed; it may read as a slow lean rather than a deliberate raise.
- **One enemy:** several warriors do not take turns; each attacks on its own schedule. Attack tokens, and the threat policy for a tank, are later work ("Later" in the roadmap).
- **Movement:** the shared profile runs at 375 cm/s, slightly slower than the run loops (397 to 415), so the feet slide a little; there is still no turn-in-place.
- **Derived boss:** `BP_RiftBoss_Prototype` (`GF_Portals_Core`) derives from the warrior Blueprint and controller, so it now fights the same way.
- **Not checked:** late join; a stagger applied during the finisher in PIE (the native test covers the rule); `SurvivalRpg.GASP.Mover`.
