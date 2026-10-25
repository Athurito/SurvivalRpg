# CMB-03: Player combo

Task report for CMB-03 in the [combat roadmap](combat-roadmap.md), [PR #282](https://github.com/Athurito/SurvivalRpg/pull/282). The sword's primary attack becomes a four-hit sword-and-shield combo from the pack's `Combo_Attack_01`, with an input buffer, a chaining window per hit, and dodge and block canceling the recovery. On the user's request (2026-10-24) the task also makes the dodge playable: a Souls-style eight-way roll or step on its own key, with invulnerability frames and a stamina cost.

## C++ boundary decision

- **Classification:** extension of the existing weapon attack mechanism (`URpgGameplayAbility_BasicWeaponAttack`) and schema (`FRpgWeaponAttackDefinition`), plus designer content (montages, combo data on the weapon instance, tags).
- **Runtime truth:** the server's attack ability. It accepts each chain step and owns damage, as for single attacks. The owner predicts the chain; animation only presents it.
- **Existing seams:** the attack definitions keyed by `Weapon.Attack.*` on the weapon instance, the attack-window notifies with their authority schedule, Lyra's activation groups (`ChangeActivationGroup`), the ASC's replicated input events and the facing commitment.
- **Native, and why:**
  - **Schema:** `FRpgWeaponAttackDefinition::ComboNextAttack`, the attack that follows when the input arrives in time, and `UAnimNotifyState_RpgComboWindow`, which marks in a montage when the next hit may start. Designers build chains from these; every chain step is an ordinary attack definition with its own montage, damage and trace.
  - **Mechanism:** the chain itself is native because it is a predicted, server-validated transition inside one activation: the owner buffers the press, starts the next hit inside a new prediction window and tells the server; the server checks the step timing, plays the same montage under the owner's prediction key and reschedules its damage window. The recovery after each hit's damage window switches the ability to `Exclusive_Replaceable`, so dodge and block can cancel it through the existing activation groups.
  - **Validation:** the weapon instance's data validation checks every chain (targets exist, no cycles, one combo window after the damage window and before auto blend-out).
- **No new classes for content:** no new ability class; `GA_BasicWeaponAttack` stays the concrete Blueprint. Montages, chain order, timing, damage and traces are assets and data on `BP_WeaponInstance_BasicSword`.
- **Dodge (added 2026-10-24):** extends the existing `URpgGameplayAbility_Dodge` base, whose Blueprint `GA_Dodge` had no montage, no grant and no input.
  - **Runtime truth:** the server's dodge: it receives the owner's direction, pays the cost and owns the invulnerability window; damage reads the window only on the server.
  - **Native, and why:** the direction must be the same on owner and server, so the owner picks it from its movement input and sends it as replicated target data with the predicted activation (`FRpgDodgeDirectionTargetData`, as the mantle sends its proposal); the server waits for it. The commit moves to native code so the invulnerability tag (`State.Evading`, a replicated timed loose tag) is only granted after the cost is paid. The damage execution ignores hits on an evading target. A grounded check and a recovery cancel time make the roll committed until its recovery, like the attack. Data validation checks every dodge montage for its eight direction sections.
  - **Tooling:** `URpgAnimationAssetTools::SetMontageSectionSequences` with the toolset tool `set_montage_section_sequences` builds a montage with one unlinked section per sequence; montage sections were not reachable from Python or MCP.
  - **Content:** `GA_Combat_Dodge` (child of `GA_Dodge`) in `GF_Combat_Core` holds montages per load tier, tuning, cost, blockers and timing; the core keeps only the generic flow and the controller's tier profile names.

## Behavior

### Problem

The sword had one primary attack (`AM_Combo_Attack_03_01_Seq_Montage`, a single hit). Pressing again during the attack did nothing, and dodge and block could not interrupt it: the attack held `Exclusive_Blocking` until its montage ended.

### Change

- **Chain inside one activation.** Pressing attack while the ability runs reaches the owner as the ASC's `InputPressed` event (the Lyra input path already forwards it). Before the step's Combo Window the press is buffered; at the window's start, or immediately inside it, the owner starts the next step: it opens a new prediction window, sends `GameCustom1` to the server and plays the next montage. A press after the window, or on the last step, is ignored.
- **Server acceptance.** The server listens for `GameCustom1` and starts the same step under the owner's prediction key. A remote owner's window opens on the server `ComboServerLeadTolerance` (0.15 s, on the ability) ahead of the server's own montage time, which absorbs jitter between the two montage clocks. An earlier request waits for that point; the server never closes the window for a request already sent, it only stops at the step's end. Host players chain locally without a request.
- **Per step.** Each step is an ordinary attack definition: own montage, damage, stagger, trace and camera mode, own authority damage window, and a fresh facing commitment (the windup may re-aim toward the camera). An item's rolled weapon damage replaces the entry attack's damage, and later steps keep their authored ratio to it.
- **Recovery cancel.** When a step's attack window ends (on the server again 0.15 s earlier for a remote owner), the ability switches to `Exclusive_Replaceable`. Any exclusive ability then replaces it: a held block takes over at once, and a dodge activated in the recovery cancels it. The next step switches back to `Exclusive_Blocking`; the instance always starts blocking again.
- **Montage hand-over.** Before a step replaces the running montage, the ability unbinds and ends the previous montage task, so the interruption does not end the ability.

### Content

- **Tags** (`Config/DefaultGameplayTags.ini`): `Weapon.Attack.Primary.Combo2`, `.Combo3`, `.Combo4`.
- **Montages** (`GF_Combat_Core/Animations/Combo/SwordShield/AM_SwordShieldCombo01_Step1` to `Step4`) from `Combo_Attack_01_01` to `_04`, slot `DefaultSlot`, blend in 0.12 s, blend out 0.25 s. Root motion is now enabled on the four pack sequences, like on `Combo_Attack_03_01`.
- **Windows.** The attack windows follow the real blade: the sword's blade sockets, placed on the CMB-01 grip, sampled over every clip. The Combo Windows start where the pack's own chained clip `Combo_Attack_01_All` starts the next hit.

  | Step | Montage length | Attack window | Combo Window | Damage / stagger | Trace |
  | --- | --- | --- | --- | --- | --- |
  | 1 (`Weapon.Attack.Primary`) | 1.167 s | 0.13–0.30 | 0.33–0.70 | 25 / 20 | blade line |
  | 2 (`.Combo2`) | 1.167 s | 0.12–0.30 | 0.47–0.82 | 25 / 20 | blade line |
  | 3 (`.Combo3`) | 0.883 s | 0.12–0.28 | 0.35–0.60 | 15 / 40 | sphere 30 cm from `lowerarm_l` to `Grip_Shield_L` |
  | 4 (`.Combo4`) | 1.500 s | 0.48–0.62 | none | 40 / 45 | blade line |

  Step 3 is the pack's shield bash: the left hand thrusts while the blade stays out of the front, so it traces the forearm and the shield grip.
- **Weapon data.** `BP_WeaponInstance_BasicSword` keeps its secondary attack; its primary entry now starts the chain.
- **Notice.** The pack had no row in the [third-party notices](third-party-notices.md); this task adds it, with the Fab Standard License stated by the user.

### Dodge

- **Key.** `IA_Dodge` on Left Alt and gamepad B (east face button), the Souls-like dodge button. On the keyboard the space bar stays jump and traversal, so mantle, vault and hurdle keep their key. Bound in `IMC_Combat` and `DA_InputConfig_Combat` to `InputTag.Ability.Dodge`; the combat GameFeature grants `GA_Combat_Dodge` through `AS_Combat_Core_Default`.
- **Direction.** Eight directions relative to the facing (F, FR, R, BR, B, BL, L, FL), from the owner's last movement input; without input the dodge goes backward, like a Souls backstep. In the combat stance the character faces the camera, so the dodge goes where the stick or WASD points.
- **Load tiers** (existing equipment load, `Equipment.Load.*`): Light rolls (`Roll_Combat`, root motion at 85 %, about 3.6 m), Medium rolls shorter (70 %), Heavy steps (`Dodge_Combat`, 50 %, about 2.4 to 3.3 m). The controller's tier profiles carry only the names; `GA_Combat_Dodge` maps them to its montages.
- **Commitment.** Dodge is `Exclusive_Blocking`: an attack or block pressed during the roll is ignored. After 0.55 s of montage time the recovery becomes replaceable, so an attack or block takes over. From an attack's recovery, the dodge cancels the attack as before.
- **Invulnerability and cost.** For 0.35 s from the start the server ignores incoming damage, stagger included, and no hit reaction plays. Each dodge costs 15 stamina (`GE_Dodge_Cost`); without enough stamina it does not start. Dead, staggered or guard-broken characters cannot dodge, nor can airborne ones.
- **Montages.** `GF_Combat_Core/Animations/Dodge/SwordShield/AM_SwordShieldDodge_Roll` and `_Step`, one section per direction from the pack's eight clips, blend in 0.1 s, blend out 0.2 s; root motion enabled on the 16 pack sequences.

## Validation (2026-10-20, on `master` `da8bacab` plus this change)

- **Build:** `ue.py build` passed, 0 warnings.
- **Assets:** the four step montages and `BP_WeaponInstance_BasicSword` pass data validation (5 / 5, 0 warnings), including the new combo checks.
- **New tests:**
  - `SurvivalRpg.Combat.WeaponCombo.Validation.ChainsNeedTargetsWindowsAndAnEnd` passed. Validation accepts a chain into an ending attack and rejects a missing target, a cycle, a chaining attack without a Combo Window, a window that opens inside the attack window, and one that ends inside the auto blend-out. The Basic Sword's combo data is valid.
  - `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientComboChainsEveryStepOnOwnerAndServer` passed (listen server, remote client, 60 ms lag with 10 ms variance). The owner's second press during the first swing is buffered; the owner predicts and the server accepts every step of the authored chain with the same attack and montage; one activation opens and closes exactly one authority window per step and ends uncanceled, in its blocking group.
  - `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientRecoveryYieldsToBlockAndDodgeButNotTheSwing` passed. Block held from the start does not replace the swing while its attack window is open, then takes over the recovery on the owner and the server, which ends the attack canceled; a dodge granted by the test cancels a second attack's recovery on both.
  - The server log of these runs shows three accepted steps on each side, no deferred and no rejected request.
- **Recalibrated test:** `RemoteClientAttackWindowDamageAndCancellation` places its target on the path of the old primary montage, as its fixture comment asks to recalibrate. It failed with the combo's first hit (target missed, 90 s timeout). The new offset (160, 20, 0) lies on the sampled blade path of step 1, 19 cm inside the target capsule; the old one sat at the capsule's edge on the old montage, which may explain the occasional misses the [handoff](gasp-integration-handoff.md) records for this test. Afterwards it passed.
- **Regression** (`ue.py test`, test editor MCP off): `SurvivalRpg.Combat` 38 / 38, `SurvivalRpg.Network` 26 / 26, `SurvivalRpg.GASP.CMC` 14 / 14, `SurvivalRpg.Equipment`, `.Cosmetics`, `.Animation` 28 / 28. `SurvivalRpg.GASP.Mover` (`np.ForceReconcile 0`, `t.MaxFPS 30`) 64 / 67:
  - `Mover.Hurdle...CancellationReleasesAllWarpTargetsAndColliderOwnership` and `...MaximumDepthHurdleLandsBeyondRearEdge` timed out in the 990 s suite run; `SurvivalRpg.GASP.Mover.Hurdle` alone passed 15 / 15. The first does not use the attack; the second is a known timeout case in the [handoff](gasp-integration-handoff.md).
  - `Mover.Vault...EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection` failed with its known `GASP-VAL-03` precondition message ("Equipment replacement overlapped the observer's old active NP traversal"). This fixture presses the real primary attack, so it plays the new step 1 montage. A/B on this change's code: with the combo data it passed 4 of 7 isolated runs, with the sword asset swapped back to its `master` state 3 of 4; the handoff records about half of all runs failing. The precondition races the ASC's montage replication against the observer's NP traversal history; the combo does not change that race.
- **Multiplayer probe, `Lvl_RpgGaspCMC` (Paladin), listen server with two clients:** client 1 mashed attack. Client 1 (autonomous proxy), the server (authority) and client 2 (simulated proxy) all showed step 1 to 4 in order, within 0.04 s of each other, and the presses during step 4 started a fresh single attack after the combo ended.
- **PIE, standalone, `Lvl_RpgGaspCMC` (Paladin):** mashing attack plays all four steps; with the editor unthrottled, steps start 0.34, 0.48 and 0.37 s into the previous one, at the Combo Windows; the ability ends after step 4's blend-out; about 6.1 m travel. Block pressed in step 1's recovery cancels it at once; block held from the swing takes over at the end of the attack window. Slow-motion captures show the four hits.

### Dodge validation (2026-10-24, on `master` `8461d3cd` plus this change)

- **Build:** `ue.py build` passed, 0 warnings.
- **Assets:** `GA_Combat_Dodge`, `GA_Dodge`, `GE_Dodge_Cost`, both dodge montages, `AS_Combat_Core_Default`, `IA_Dodge`, `IMC_Combat`, `DA_InputConfig_Combat` and `BP_Rpg_PlayerController` pass data validation (10 / 10, 0 warnings), including the eight-section check.
- **New tests:**
  - `SurvivalRpg.Combat.Dodge.DirectionFollowsMovementInput` passed: eight sectors around two facings, backward without or with tiny input, distinct section names, and every direction survives net serialization.
  - `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientDodgesInTheInputDirectionWithServerInvulnerability` passed (remote client, 60 ms lag). The owner holds D and dodges: owner and server both play section R, the server from the owner's target data, and the server is evading. A hit from the hostile fixture inside the window leaves health unchanged, and stamina has dropped; after the window the same hit lands. Without input the next dodge is a backstep on both.
  - `...RemoteClientRecoveryYieldsToBlockAndDodgeButNotTheSwing` now uses the granted dodge through its input instead of a test grant, and passed.
- **Regression** (`ue.py test`, test editor MCP off, own editor closed): `SurvivalRpg.Combat` 39 / 39, `SurvivalRpg.Network` 27 / 27, `SurvivalRpg.GASP.CMC` 14 / 14, `SurvivalRpg.AI`, `.Equipment`, `.Cosmetics`, `.Animation`, `.UI` 79 / 79.
- **Mover:** `SurvivalRpg.GASP.Mover` 62 / 67 in the suite run. Alone, `Mover.Mantle` 16 / 17 and `Mover.Vault` 15 / 17 and 16 / 17, each time with other single traversal timeouts or fixed-correction failures. A/B with `AS_Combat_Core_Default` back on `master` (no dodge grant, same code): `Mover.Vault` 16 / 17 twice, again with a different single failure. The Mover traversal cases are timing-sensitive on this machine independently of the dodge.
- **Load sensitivity:** `RemoteClientAttackWindowDamageAndCancellation` failed 3 of 3 runs while this worktree's editor with the MetaHuman map ran in parallel (a random attack missed its target) and passed 2 of 2 with it closed.
- **PIE, standalone, `Lvl_RpgGaspCMC` (Paladin):** D plus dodge plays section R, no input plays B, W plus A plays FL. Dodge in an attack's recovery cancels the attack; dodge during the swing does nothing; an attack 0.3 s into a roll does nothing, one at 0.7 s replaces the roll.
- **Multiplayer probe, listen server with two clients:** client 1 dodged with A held and with S plus D held; client 1 (autonomous proxy), the server and client 2 (simulated proxy) all played L and BR respectively.

### Heavy dodge and user check (2026-10-25)

- **Heavy tier, PIE standalone, `Lvl_RpgGaspCMC`:** every equipment definition has an `EquipLoadWeight` of 0 kg, so a player is always Light. With `ED_BasicSword` set to 25 kg in memory only (discarded afterwards by reloading the package), the loadout reported Heavy at 25 kg, and a dodge without input played `AM_SwordShieldDodge_Step` section B.
- **User playtest:** the user checked combo and dodge in PIE, including the heavy step, and accepted them for now; the presentation is not final.

## Playtest

1. Open `Lvl_RpgGaspCMC` and start PIE (Paladin with sword and shield).
2. Press attack once: one hit, then the character recovers.
3. Press attack repeatedly: four hits in a row, the third a shield bash, the fourth a heavy overhead swing. Pressing during a swing queues the next hit, which starts at the natural point of the chain.
4. Press attack late in a hit's recovery: the combo ends.
5. Hold block before or during a swing: the swing finishes its hit, then block takes over. Press block after a hit: block cancels the recovery at once.
6. Listen server with two clients: both players see each other's full combo; a client's combo plays every hit on its own screen without hitches.
7. Dodge with Left Alt or gamepad B: with W, A, S or D (or the stick) held, the character rolls in that direction; without input it backsteps. Attack or block during the roll do nothing; from the end of the roll they take over.
8. Dodge out of an attack's recovery: the attack stops and the roll starts.
9. Heavy load (Gear and Carry weight of 23 kg or more): the dodge is a short step instead of a roll. No equipment has a load weight yet, so give an equipped item a temporary `EquipLoadWeight` first, for example 25 kg on `ED_BasicSword`, and do not save it.
10. Listen server with two clients: each player sees the other's rolls in the right direction.

## Limits

- **Inputs are not buffered across abilities.** A dodge pressed during a swing, or an attack pressed during a roll, is dropped; only a held block waits for the recovery.
- **Dodge out of block.** Block is `Exclusive_Blocking`, so a dodge pressed while holding block does nothing; release block first.
- **Load-dependent dodge ([#124](https://github.com/Athurito/SurvivalRpg/issues/124)).** Light, Medium and Heavy are configured, but every equipment definition still has 0 kg `EquipLoadWeight`, so players are always Light in play. Real item weights wait until the load system itself is settled (user, 2026-10-25). The issue's remaining acceptance (equipment changes right before a dodge under lag and loss, respawn and late join) is not covered here.
- **Invulnerability scope.** `State.Evading` ignores every damage through the damage execution, not only attacks; it lasts 0.35 s per dodge.
- **Root motion.** The pack's combo travels about 6 m forward over four hits, as authored; tuning it would mean scaling root motion per step.
- **Server lead.** When a remote owner's request or cancel reaches a server that runs behind, the server shortens its own damage window by up to the lead tolerance to stay in step with the owner, who has already finished that window.
- **Other loadouts.** The sword without a shield plays the same combo, including the shield bash with an empty hand. The two-handed sword keeps its single attack. Enemies with the Basic Sword now open with step 1 and never chain; enemy combos are CMB-06.
- **Mover.** Like the previous primary montage, the steps target the Manny skeleton; the UEFN Mover pawn was not checked.
- **No hit feedback** (CMB-04) and no sound yet.
- **Pack licence.** The Sword and Shield pack now has a [third-party notice](third-party-notices.md) row with the Fab Standard License, as the user stated.
