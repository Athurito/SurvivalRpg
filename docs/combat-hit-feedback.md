# CMB-04: Hit feedback

Task report for CMB-04 in the [combat roadmap](combat-roadmap.md), [PR #289](https://github.com/Athurito/SurvivalRpg/pull/289). Hits, blocks, perfect blocks, staggers, guard breaks and deaths get gameplay cues with placeholder effects, a light hit gets a short directional flinch that never interrupts, and the camera shakes locally. The rules come from the [hit feedback rules](combat-roadmap.md#hit-feedback-rules-cmb-04) decided with the user on 2026-10-25; there is no hitstop.

## C++ boundary decision

- **Classification:** extension of the existing damage mechanism (`URpgDamageExecution`), the weapon attack (`URpgGameplayAbility_BasicWeaponAttack`), the stagger and death abilities and the attack schema (`FRpgWeaponAttackDefinition`), plus designer content (cue notifies, flinch montage, AnimBP slot, camera shakes, tags).
- **Runtime truth:** the server's damage execution decides what a melee impact was: a hit, a block or a perfect block. The stagger meter on the target's defense set decides stagger and guard break, as before. Cues, the flinch and camera shakes are presentation only and never feed back into gameplay.
- **Existing seams:** the damage execution's block resolution and combat events, the attack ability's trace windows on owner (montage notifies) and server (schedule), GAS gameplay cues routed through each target's ability system, `GameplayCueNotify_Burst` assets in GameFeature `GameplayCues` folders, the stagger and death abilities, and the GASP AnimBP's pose chain.
- **Native, and why:**
  - **Impact outcome:** the damage execution resolves block and perfect block and therefore executes the matching impact cue itself. A blocked hit can still deal reduced damage, so the attack ability alone cannot tell a block from a hit. The block rules move into a shared helper (`RpgCombatImpact`), so the execution and the owner's prediction use the same code.
  - **Owner prediction:**
    - **Why:** the server confirms a hit one round trip after the owner's swing.
    - **Prediction:** the owner also traces its own attack window, for feedback only, and plays the impact cue locally. It reads the outcome from the replicated target state (block tags, block cone, stamina).
    - **The server's copy:** it reaches every machine. On the attacking client, `URpgGameplayCueNotify_Impact` absorbs it when a recent prediction for the same target and attacker exists. Otherwise it plays, late instead of never.
    - **Unpredicted:** perfect blocks, staggers, guard breaks and deaths reach everyone from the server.
  - **Rejected alternative:** executing the server's copy under the attack's activation prediction key also deduplicates. GAS sends that key only to the owning connection, which then skips the multicast. In the full network suite one owner's feedback trace missed a hit the server confirmed, and that owner saw nothing for its own hit. The registry keeps the server's confirmation as a fallback.
  - **Schema:**
    - `FRpgWeaponAttackDefinition::HitCueTag`: the impact cue of a body hit per attack, for example a heavier cue on the last combo hit.
    - `StaggerCueTag` and `GuardBreakCueTag` on the stagger ability: it knows whether the target was blocking when its meter broke.
    - `DeathCueTag` on the death ability.
  - **New native classes:** one, `URpgGameplayCueNotify_Impact`, the parent of the impact cue notifies. The burst effects spawn natively before `OnBurst`, so only a native override can absorb a predicted copy before anything plays. The shared rules and the prediction registry are a namespace of functions next to the combat code.
  - **Tooling:** the skeleton slot tool takes an optional slot group. The engine and Python offer no way to place a slot in its own group, and a flinch slot in the default group would stop the attack and block montages.
- **Content:**
  - Cue notifies `GCN_Combat_*` in `GF_Combat_Core/GameplayCues`.
  - An additive flinch montage with four direction sections, built from the pack's `Hit_Combat_*` clips.
  - A `HitFlinch` slot in its own slot group, applied above the spine in `ABP_RpgGasp_CMC_Manny` and `ABP_Unarmed`.
  - Camera shakes, cue tags, and the sword's per-step hit cues.
  - Swing hooks as gameplay cue notifies in the combo montages.
- **Removed:** `GA_Combat_HitReaction` played a full-body montage through the ability system on every hit and replaced a running recovery. The flinch cue takes over its job without interrupting, so the ability and its montage leave `AS_Combat_Core_Default`.

## Behavior

### Problem

A hit showed nothing: no effect, no sound hook and no camera feedback. The only reaction was `GA_Combat_HitReaction`, a server-only ability in `Exclusive_Replaceable` that played `Montage_HitReaction` through the ability system on every hit. It replaced the target's running recovery, so several attackers could keep a target flinching. Blocks, perfect blocks, staggers, guard breaks and deaths had no cue at all.

### Change

- **One impact cue per melee impact.** An attack opts in by putting its `HitCueTag` on its damage spec. The damage execution resolves the outcome with the shared rules and executes one cue on the target's ability system:
  - **Hit:** the attack's `HitCueTag`. The cue goes out when damage or stagger lands; a dodge's invulnerability or a friendly target plays none.
  - **Block:** `GameplayCue.Combat.Block`, also when reduced damage gets through.
  - **Perfect block:** `GameplayCue.Combat.PerfectBlock`.
  - **Parameters:** the impact point, the surface normal and the attacker as instigator. `RawMagnitude` is the damage, `NormalizedMagnitude` the stagger damage as a fraction of the target's maximum stagger.
- **Owner prediction:**
  - **Remote owner:** traces its own attack window from its montage notifies with the same sampling as the server, for feedback only. Each target gets one prediction per window.
  - **Predicted outcome:** a block when the target's replicated `State.Blocking`, block cone and stamina allow it, otherwise a hit; a perfect-block window is predicted as a block.
  - **Recording:** `RpgCombatImpact::PlayPredictedImpact` plays the cue at once and records target, attacker and time.
  - **The server's copy:** it reaches every machine. On the attacking client, the impact notify absorbs it when a recorded prediction for the same target and attacker is at most 0.75 s old. Without one, because the owner's trace missed, it plays late.
  - **Perfect block:** `GCN_Combat_PerfectBlock` sets `bPredictedByAttacker` off, so it always plays and only clears the block prediction it replaces.
  - **No prediction:** a listen-server host, an AI and a dedicated server's own attacks are not predicted, and their cues reach every machine.
- **Stagger, guard break and death:** the server's stagger ability plays `GuardBreakCueTag` when the character was blocking as its meter broke, otherwise `StaggerCueTag`. The death ability plays `DeathCueTag`. All three reach every machine.
- **Flinch:**
  - **Trigger:** `GCN_Combat_Hit` plays `AM_Combat_HitFlinch` on the hit character's anim instance. The section F, B, L or R comes from the attacker's direction relative to the character's facing.
  - **Additive and masked:** the montage is additive on the `HitFlinch` slot in its own slot group `HitFlinchGroup`, applied above `spine_01`. It never stops a montage in the default group, so attacks, blocks and dodges keep running; legs and feet stay untouched.
  - **Several attackers:** a new hit restarts only the flinch montage with a 0.06 s blend; overlapping hits stay presentation.
- **Camera shake:**
  - **Hit player:** the burst effects shake only that player's camera.
  - **Attacker:** `OnBurst` shakes only the camera of the machine that controls the attacker. Bystanders feel nothing.
- **Swing:** `AN_Combat_Swing`, a child of `AnimNotify_GameplayCue`, fires `GameplayCue.Combat.Swing` locally on every machine at the attack window start of the four combo steps. Its cue is empty until the sound library is curated.
- **Parry:** `GameplayCue.Combat.Parry` is only a reserved tag; no parry mechanic exists.
- **The retired reaction:** `GA_Combat_HitReaction` and `Montage_HitReaction` are deleted, and `GameplayEvent.HitReaction` starts no ability. The attack still sends the event as a hook, for example for CMB-06 AI.

### Content

| Asset | Role |
| --- | --- |
| `GF_Combat_Core/GameplayCues/GCN_Combat_Hit` (parent `URpgGameplayCueNotify_Impact`) | Base impact cue: spark burst (`NS_FX_Tpl_SparkBurst` ×0.4), target-local shake (`CS_FX_RockBurst` ×0.35), flinch (`FlinchMontage`, `FlinchPlayRate` 1.25), attacker shake (`InstigatorShake` ×0.2) |
| `GCN_Combat_Hit_Heavy` (child of `GCN_Combat_Hit`) | `GameplayCue.Combat.Hit.Heavy`: sparks ×0.6, shakes ×0.6 and ×0.35, flinch at rate 1.0 |
| `GCN_Combat_Block`, `GCN_Combat_PerfectBlock` (children of `GCN_Combat_Hit`) | No flinch; sparks ×0.7 and ×1.0, blocker shake ×0.3 and ×0.5, attacker shake ×0.25 and ×0.4 |
| `GCN_Combat_Stagger`, `GCN_Combat_GuardBreak` | Target-local shake ×0.5 and ×0.7; the guard break adds sparks ×0.9 |
| `GCN_Combat_Death`, `GCN_Combat_Swing` | Empty placeholders for sounds |
| `GF_Combat_Core/Animations/HitFeedback/AS_HitFlinch_F/B/L/R` | Local-space additive copies of the pack's `Hit_Combat_*` clips, base frame 0, no root motion |
| `AM_Combat_HitFlinch` | Four unlinked sections, slot `HitFlinch`, blend 0.06 s in and 0.25 s out |
| `SK_Mannequin` | Slot `HitFlinch` in group `HitFlinchGroup`; no bones changed |
| `ABP_RpgGasp_CMC_Manny`, `ABP_Unarmed` | After the final pose (the pose history collector, the control rig): additive identity → `HitFlinch` slot, layered blend from `spine_01` over an additive identity, *Apply Additive* |
| `GF_Combat_Core/Animations/Notifies/AN_Combat_Swing` | Swing cue notify; in `AM_SwordShieldCombo01_Step1`–`4` at 0.13, 0.12, 0.12 and 0.48 s |
| `BP_WeaponInstance_BasicSword` | Steps 3 (shield bash) and 4 (overhead) use `GameplayCue.Combat.Hit.Heavy`; the other attacks keep the default `GameplayCue.Combat.Hit` |
| `GA_Combat_Stagger`, `GA_Combat_Death` | `GameplayCue.Combat.Stagger`, `.GuardBreak` and `.Death` |
| `AS_Combat_Core_Default` | No longer grants `GA_Combat_HitReaction` |
| `Config/DefaultGameplayTags.ini` | Content tags `GameplayCue.Combat.Hit.Heavy`, `.Stagger`, `.GuardBreak`, `.Death`, `.Swing`, `.Parry` with their parameters; `Hit`, `Block` and `PerfectBlock` are native |

## Validation (2026-10-25, on `master` `2994c322` plus this change)

- **Build:** `ue.py build` passed, 0 warnings.
- **Assets:** data validation of the 25 new and changed assets passed (25 valid, 0 warnings), including the asset reference restrictions. The AnimBPs compile up to date.
- **New tests:**
  - `SurvivalRpg.Combat.HitFeedback.PredictionAbsorbsOneServerCopy` and `...OutcomeCues`: the prediction registry and the outcome cue mapping.
  - `SurvivalRpg.Combat.HitFeedback.NativeImpactCuesHaveNotifies`: each natively routed impact cue tag has exactly one project notify.
  - `SurvivalRpg.Network.CombatImpactCuesPIE.TwoRemoteAttackersHitOneTargetAndEachHitPlaysOnceEverywhere`:
    - **Setup:** listen server with two remote clients at 60 ms lag ±10. Both press attack in the same frame, two rounds, on the real stagger ability.
    - **Each round:** each server attack lands one hit. Each hit plays its impact cue exactly once on the server and on both clients.
    - **Stagger:** the shared meter breaks once, with one stagger cue per machine, and the immunity holds through round two.
  - **Counting:** routing observers also see the server copies an impact notify absorbs, so the test counts played cues as routed minus absorbed (`GetAbsorbedImpactCountForTests`). It asserts at least one owner prediction across both rounds.
  - **Fixes during development:** the first version counted routed cues, and the notify contract filtered on the engine parent class; both failed once and passed after the fix.
- **Changed test:** `RemoteClientAttackWindowDamageAndCancellation` now asserts that `GameplayEvent.HitReaction` starts no ability and leaves both exclusive activation groups free.
- **Regression** (`ue.py test`, own editor closed):
  - **`SurvivalRpg.Combat` and `SurvivalRpg.Network`:** 69 / 70 in the final run, every new test passed.
    - **`MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease`** timed out. It passed in the previous full run and 2 / 2 alone.
    - **`RemoteClientAttackWindowDamageAndCancellation`:** in the first full run the server missed the 1.5x attack, the load sensitivity known from CMB-03; it passed in both later full runs.
  - **`SurvivalRpg.GASP.CMC`:** 14 / 14.
  - **`SurvivalRpg.AI`, `.Equipment`, `.Cosmetics`, `.Animation` and `.UI`:** 79 / 79.
  - **`SurvivalRpg.GASP.Mover`:** 66 / 67. `Vault.LateJoinReconstructsTheCurrentVaultState` failed in the suite and passed alone. This is the timing-sensitive traversal pattern recorded in CMB-03, where the suite gave 62 / 67 with changing single failures.
- **Rejected key approach:** in the first full network run, one owner's feedback trace missed a hit the server applied. With the key routing that owner then counted 1 of 2 hit cues. This led to the prediction registry.
- **PIE, standalone, `Lvl_RpgGaspCMC` with a placed `BP_RiftGrunt_Sword`:**
  - **Hit:** a sword hit dealt 25 damage and routed `GameplayCue.Combat.Swing` on the player and `GameplayCue.Combat.Hit` on the grunt.
  - **Flinch:** the cue plays `AM_Combat_HitFlinch` on the grunt (`ABP_Unarmed`). During it the head moved by 9 and 13 cm, while pelvis and feet stayed within 0.2 cm.
- **PIE, listen server with two clients, `Lvl_RpgGaspCMC`:**
  - **Swing:** a swing of client 1 played its swing cue on all three machines.
  - **Flinch timing:** one hit flinched each grunt exactly once per machine, on client 1 from its prediction at 0.554 s, on the server and client 2 at 0.574 to 0.595 s.
  - **Misprediction:** another hit was predicted as a block because the grunt was blocking on client 1, while the server applied it as a hit. Client 1 showed block sparks, the others the hit.

## Playtest

1. Open `Lvl_RpgGaspCMC` and place a `BP_RiftGrunt_Sword` (`GF_AI_RiftMonsters/AI/Characters`) in front of a player start; start PIE with the Paladin.
2. Hit the grunt: sparks at the blade's impact point, a short flinch of its upper body from the side you hit it, and a light shake of your camera. Its legs keep standing; an attack it is swinging continues.
3. Combo hits 3 and 4 (shield bash and overhead): bigger sparks and a stronger flinch and shake.
4. Let the grunt hit you: your camera shakes, your upper body flinches, and your own attack or block is not interrupted.
5. Hit it while it blocks: bigger sparks on the shield, no flinch. Break its guard: a guard-break burst and its stagger.
6. Listen server with two clients: hit the grunt with one client. The other client and the host see the same sparks and flinch, and only the attacker's camera shakes.

## Limits

- **Mispredictions are cosmetic:**
  - **Wrong outcome:** the attacker shows the outcome it read from the replicated block state. When the server decides differently (block just ended, stamina), the attacker sees the predicted cue and the others see the server's.
  - **Hit the server rejects:** the attacker sees sparks without damage.
  - **Lifetime:** a prediction waits 0.75 s for the server's copy. A copy that arrives later, on a round trip above that, plays a second time.
- **Server copy timing:** a hit the attacker did not predict reaches it one round trip late. The registry matches only target and attacker, not the hit, so in rare double mismatches a stale prediction can absorb the next combo hit's copy.
- **Listen-server flinch:** the flinch runs on the server's pose too, so on a listen server an enemy's own weapon trace can shift slightly while it flinches. Dedicated servers play no cues.
- **Placeholders:**
  - **Effects:** the sparks come from `NS_FX_Tpl_SparkBurst` on every surface, also flesh.
  - **Sound:** swing, death and every other cue are silent until the sound library is curated.
  - **Parry:** only a reserved tag.
- **Coverage:**
  - **Swing hook:** only the four sword-and-shield combo steps have it; the two-handed sword and `AM_BasicSword_Attack01` have none.
  - **Flinch:** only `ABP_RpgGasp_CMC_Manny` and `ABP_Unarmed` have the flinch slot; the UEFN Mover pawn was not changed.
  - **Opt-in:** only weapon attacks opt in through `HitCueTag`. Spells, damage areas and other damage play no impact cue yet.
- **Direction sections:** F, B, L and R assume the pack's naming means the side of the impact; the user check confirms the look.
- **No hitstop**, as decided.
