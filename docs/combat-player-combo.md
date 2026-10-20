# CMB-03: Player combo

Task report for CMB-03 in the [combat roadmap](combat-roadmap.md). The sword's primary attack becomes a four-hit sword-and-shield combo from the pack's `Combo_Attack_01`, with an input buffer, a chaining window per hit, and dodge and block canceling the recovery.

## C++ boundary decision

- **Classification:** extension of the existing weapon attack mechanism (`URpgGameplayAbility_BasicWeaponAttack`) and schema (`FRpgWeaponAttackDefinition`), plus designer content (montages, combo data on the weapon instance, tags).
- **Runtime truth:** the server's attack ability. It accepts each chain step and owns damage, as for single attacks. The owner predicts the chain; animation only presents it.
- **Existing seams:** the attack definitions keyed by `Weapon.Attack.*` on the weapon instance, the attack-window notifies with their authority schedule, Lyra's activation groups (`ChangeActivationGroup`), the ASC's replicated input events and the facing commitment.
- **Native, and why:**
  - **Schema:** `FRpgWeaponAttackDefinition::ComboNextAttack`, the attack that follows when the input arrives in time, and `UAnimNotifyState_RpgComboWindow`, which marks in a montage when the next hit may start. Designers build chains from these; every chain step is an ordinary attack definition with its own montage, damage and trace.
  - **Mechanism:** the chain itself is native because it is a predicted, server-validated transition inside one activation: the owner buffers the press, starts the next hit inside a new prediction window and tells the server; the server checks the step timing, plays the same montage under the owner's prediction key and reschedules its damage window. The recovery after each hit's damage window switches the ability to `Exclusive_Replaceable`, so dodge and block can cancel it through the existing activation groups.
  - **Validation:** the weapon instance's data validation checks every chain (targets exist, no cycles, one combo window after the damage window and before auto blend-out).
- **No new classes for content:** no new ability class; `GA_BasicWeaponAttack` stays the concrete Blueprint. Montages, chain order, timing, damage and traces are assets and data on `BP_WeaponInstance_BasicSword`.

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
- **Notice.** The pack had no row in the [third-party notices](third-party-notices.md); this task adds it.

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

## Playtest

1. Open `Lvl_RpgGaspCMC` and start PIE (Paladin with sword and shield).
2. Press attack once: one hit, then the character recovers.
3. Press attack repeatedly: four hits in a row, the third a shield bash, the fourth a heavy overhead swing. Pressing during a swing queues the next hit, which starts at the natural point of the chain.
4. Press attack late in a hit's recovery: the combo ends.
5. Hold block before or during a swing: the swing finishes its hit, then block takes over. Press block after a hit: block cancels the recovery at once.
6. Listen server with two clients: both players see each other's full combo; a client's combo plays every hit on its own screen without hitches.

## Limits

- **No dodge input.** `GA_Dodge` is not granted or bound to an input in any Experience (see [GAS ability content](gas-ability-content.md)). The recovery cancel works for any exclusive ability and is tested with a dodge granted by the test, but players can only cancel into block until a dodge input exists, for example with the load-dependent dodge ([#124](https://github.com/Athurito/SurvivalRpg/issues/124)).
- **Dodge input is not buffered.** A dodge pressed during a swing is dropped; only a held block waits for the recovery.
- **Root motion.** The pack's combo travels about 6 m forward over four hits, as authored; tuning it would mean scaling root motion per step.
- **Server lead.** When a remote owner's request or cancel reaches a server that runs behind, the server shortens its own damage window by up to the lead tolerance to stay in step with the owner, who has already finished that window.
- **Other loadouts.** The sword without a shield plays the same combo, including the shield bash with an empty hand. The two-handed sword keeps its single attack. Enemies with the Basic Sword now open with step 1 and never chain; enemy combos are CMB-06.
- **Mover.** Like the previous primary montage, the steps target the Manny skeleton; the UEFN Mover pawn was not checked.
- **No hit feedback** (CMB-04) and no sound yet.
- **Pack licence.** The Sword and Shield pack now has a [third-party notice](third-party-notices.md) row, but its licence is not recorded yet.
