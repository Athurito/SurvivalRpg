# CMB-06: Enemy AI

Task report for CMB-06 in the [combat roadmap](combat-roadmap.md). The skeleton warrior from [CMB-05](combat-skeleton-warrior.md) gets real behavior: it approaches, keeps its distance and circles, telegraphs its attacks so the perfect block can read them, chains one to four hits, blocks the player's attacks, recovers from stagger and backs off after a combo. The behavior is a StateTree on the existing AI drivers and loadouts.

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
