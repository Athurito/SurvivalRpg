# RNG-03: Bow aim, camera, aim movement and presentation

Task report for RNG-03 in the [ranged combat roadmap](ranged-combat-roadmap.md), [PR #367](https://github.com/Athurito/SurvivalRpg/pull/367). Holding the primary attack with the bow now enters the aim stance and draws; releasing it shoots. Holding the aim input (right mouse button) aims before drawing and keeps the stance between shots. The stance turns the character to the camera, slows it, uses a shoulder camera and shows a ready stance, the full draw by aim pitch and the release on every machine. Dodge, sprint, leaving the ground, stagger, attacks, death, equipment removal and avatar teardown interrupt an unfinished shot; a held button then needs a fresh press. RNG-03b adds embedding and recovery, RNG-04 the HUD with draw strength and ammunition.

## C++ boundary decision

- **Classification:** reusable native mechanism (aim tokens, the CMC aim lease, the aim ability task with its interruption rule, the camera read model), native schema (`FRpgRangedAimDefinition`, replicated ranged state tags, the anim instance snapshot) and designer content (`GA_BowAim`, the `GA_BowShot` graph, `CM_Bow_Aim`, the bow hold layer and its pose blend, the grip socket, the migrated archer clips).
- **Runtime truth:**
  - **Stance:** every GAS activation that aims (held aim, drawn shot) holds one token on the ability system, on the owner and on the server. Tokens stack: the stance lasts while any is live, its cap is the lowest, its camera the oldest token's. The authority's `State.Ranged.Aiming` count reaches simulated proxies and late joiners.
  - **Movement:** the CMC samples the stance and its cap into each saved move, so prediction, replay and the server's own move agree; the client never sends a cap.
  - **Presentation state:** `State.Ranged.Drawing` (from activation until the shot ends) and `State.Ranged.Releasing` (a short presentation time after a release) are set on owner and authority and replicate to everyone else.
- **Existing seams:** the block, sprint and facing leases on `URpgAbilitySystemComponent` and the CMC's saved moves; `URpgPawnGameplayComponent::DetermineCameraMode`; `URpgCameraMode_ThirdPerson`; the CMB-02 hold layers (`HoldPresentations`, `ALI_RpgMainHandHold`, `ABP_RpgGasp_CMC_Manny`); the `URpgAnimInstance` game-thread snapshot; the Lyra activation groups; `SuppressAbilityInputUntilRelease`; the persistent center reticle.
- **Native, and why:**
  - **Tokens:** two abilities (aim and shot) must hold one stance without either end clearing the other, and the camera must have one owner reflecting both. GAS has no per-activation stacking state that Blueprint could keep consistent through prediction, cancellation and avatar changes.
  - **Aim lease:** movement speed and facing are predicted movement; they belong in the saved moves next to the block lease, not in an attribute (`URpgMobilitySet::MovementSpeed` is read by nothing).
  - **Aim task:** the interruption rule watches exclusive activation groups, sprint leases and the CMC movement mode on owner and authority each tick, and suppresses a held input after a cancel; Blueprint cannot reach those seams safely.
  - **Snapshot:** worker-thread animation graphs must not query the ability system; the anim instance captures the ranged state and aim pitch on the game thread.
- **New native classes:** one, `URpgAbilityTask_RangedAim`. Everything else extends existing types.
- **Content:** `GA_BowAim` is a Blueprint on `URpgGameplayAbility`; the hold layer, blend space, camera mode and grip are assets. No native leaf.
- **Asset work:** Unreal MCP and editor Python; validated by asset contract tests, data validation and PIE.

## Behavior

### Problem

The bow could shoot (RNG-02), but only from the primary input without an aim stance: no shoulder camera, no aim movement, no aim posture, no grip and no draw or release animation, and nothing interrupted a shot except the ability system's ordinary cancels.

### Change

- **Input:** `GA_BowShot` stays on the primary attack (`InputTag.Weapon.Primary`); the new `GA_BowAim` holds the aim on `InputTag.Weapon.Block` (right mouse button), which the bow cannot use for blocking. `GA_BowAim` is `WhileInputActive` in the independent group; the shot stays `Exclusive_Replaceable`. Both carry `Ability.Type.RangedAim`.
- **Aim stance (`URpgAbilityTask_RangedAim`, `Hold Ranged Aim From Weapon`):** both bow abilities hold the stance with the weapon's `AimDefinition`. The movement cap resolves once per activation through `Ability.Tuning.Ranged.AimMovement` (cm/s). Ending the shot keeps a held aim's stance, ending the aim keeps a drawn shot's stance.
- **Movement:** while aiming the CMC faces the camera (also standing), caps the speed at the weapon's authored cap (the bow 180 cm/s, a starter value; the native default is no cap), never sprints, and records both in its saved moves. The ordinary movement profile and the equipment load rules are unchanged. The Mover does not read the stance.
- **Camera:** `URpgPawnGameplayComponent::DetermineCameraMode` uses the stance's camera, `CM_Bow_Aim`: a closer shoulder camera (220 cm behind, 75 cm right, 20 cm up at level pitch, field of view 70°). The existing center reticle is the crosshair: the shot aims through the camera's center. There is no trajectory, arc, impact marker or ballistic correction anywhere.
- **Interruptions:** on owner and authority the task cancels the stance's ability when the avatar leaves the ground (jump, fall, traversal), a sprint starts after the stance began, or an ability of an exclusive activation group runs that is not a ranged aim ability (dodge, attacks, stagger, death). The shot is also cancelled by its own exclusive group. Equipment removal and avatar changes end the abilities as before. A cancelled ability whose input is still held suppresses that input until it is released. A press made while such a state already lasts (during a dodge, a stagger, a jump) is not cancelled: the stance waits and begins when the state ends, so that press counts as the fresh input. Exhaustion during hold drain still lowers the bow (RNG-02).
- **Presentation:** `ABP_Hold_Bow_Manny`, the bow's main-hand hold layer on the Manny skeleton (CMB-02 mechanism), reads the snapshot:
  - not aiming: the left arm carries the bow lowered (`AS_Idle_Combat_Seq`, frozen);
  - aiming: the ready stance from `spine_01` up;
  - drawing: the full draw by aim pitch (`BS_Bow_AimPitch` over `AS_Idle_Aim_Down/0/Up`), blended in over 0.35 s;
  - releasing: `AS_Aim_the_Target_End_Seq` from the start. A new draw clears the release state, so rapid shots restart it on every release.
  Simulated proxies read the replicated tags and the replicated view pitch, so they show the same.
- **Grip:** the bow attaches at the new skeleton socket `Grip_Bow_L` on `hand_l` of both gameplay skeletons, at the archer pack's rigid bow bone transform.

### Content

| Asset | Role |
| --- | --- |
| `GF_Combat_Ranged/GAS/Abilities/GA_BowAim` | Held aim: Hold Ranged Aim From Weapon, then Wait Input Release, then End Ability |
| `GF_Combat_Ranged/GAS/Abilities/GA_BowShot` | Now also holds the aim stance; tag `Ability.Type.RangedAim` |
| `GF_Combat_Ranged/GAS/AbilitySets/AS_Bow` | Adds `GA_BowAim` on `InputTag.Weapon.Block` with the id `Ability.Ranged.BowAim` |
| `GF_Combat_Ranged/Equipment/Bow/BP_WeaponInstance_Bow` | `AimDefinition`: 180 cm/s, `CM_Bow_Aim` |
| `GF_Combat_Ranged/Equipment/Bow/CM_Bow_Aim` | Shoulder camera, child of `CM_RpgGasp_Traversal` |
| `GF_Combat_Ranged/Equipment/Bow/ED_Bow`, `BP_BowActor` | Attach at `Grip_Bow_L`, hold layer for the Manny skeleton, bow mesh rotation (roll 90, yaw -90) |
| `GF_Combat_Ranged/Animations/Archer` | Five migrated archer clips, `BS_Bow_AimPitch`, `ABP_Hold_Bow_Manny` |
| `SK_Mannequin`, `SK_UEFN_Mannequin` | Socket `Grip_Bow_L`; no bone or hierarchy change |
| `Config/DefaultGameplayTags.ini` | `Ability.Ranged.BowAim` |

## Validation (2026-11-02)

Runs in the task worktree on `master` `499ba982` plus this branch:

- **Review:** two independent review agents read the diff, one for authority, prediction, lifecycle and replication of the stance, one for architecture boundaries, content wiring and test quality. Each finding was checked against the code and, for tag replication and activation groups, the engine source before fixing:
  - **Sprint interrupt one-shot (medium, confirmed):** a sprint held when the stance began disarmed the sprint interrupt for the whole stance, so releasing and pressing sprint again kept drawing at aim speed. The task now re-arms once that sprint ended; `InterruptionsCancelAndNeedFreshInput` covers it.
  - **Owner tags overwritten (low, confirmed):** owner and server both wrote `State.Ranged.Drawing` and `State.Ranged.Releasing` with replicated counts, and a late server removal dropped the owner's predicted count in quick follow-up shots. Only the authority writes the tags now; the owner reads its own local draw and release flags, as `State.Blocking` is authority-only.
  - **Avatar change never detected (low, confirmed):** the task compared the ability's avatar with the ability system's avatar, which read the same actor info. It now compares with the avatar captured at activation.
  - **Press during a dodge eaten (medium, confirmed):** the independent aim started during a dodge, stagger or jump, was interrupted on its first tick and suppressed the held button, so the fresh press the spec asks for needed a third press. A press during such a state now waits and begins the stance when the state ends; only an avatar change cancels a waiting stance.
  - **Release clip not restarted (low-medium, confirmed):** with 0.3 s nock time and 0.5 s release presentation, a second release inside the window kept `State.Ranged.Releasing` set, so the release clip never restarted. A new draw now clears the release state on owner and authority.
  - **Tests (medium, confirmed):** the primary input alone, a group switch after start (stagger), ability removal, death-like cancel of all abilities, avatar change and the fixed aim tuning had no test, and deleting the stance node from `GA_BowShot` passed every test. The tests below add them; the anim snapshot stays untested (Limits).
  - **Aim cap only native (low, confirmed):** 180 cm/s was the struct default and the PIE test froze it. The native default is now no cap, `BP_WeaponInstance_Bow` authors 180, and `RangedAimPIE` reads the bow's authored value.
  - **Kept as limits:** owner and server resolve the aim tuning at their own activation, and the Mover reads no stance (see Limits).
- **Build:** `python Build/Tools/Unreal/ue.py build` passed, 0 errors, 0 warnings.
- **Native and asset tests:** `ue.py test SurvivalRpg.Combat.Ranged SurvivalRpg.Combat.ProjectileShot --null-rhi` passed 24 / 24 after the review fixes. New:
  - `Ranged.Aim.TokensHoldTheCombinedStance`: aim and draw hold separate tokens; ending either keeps the other's stance, the lowest cap and the oldest camera; the end of a stale token leaves the new aim's stance and camera.
  - `Ranged.Aim.CMCLeaseCapsSpeedAndFacesTheCamera`: the stance caps `GetMaxSpeed`, records the cap in the saved move, faces the control rotation and survives a saved-move replay with the recorded values.
  - `Ranged.Aim.InterruptionsCancelAndNeedFreshInput`: an exclusive ability, a new sprint (also after a sprint held at the start ended) and leaving the ground cancel the stance; the held button stays suppressed until released.
  - `Ranged.Aim.PressDuringABlockingStateWaits`: an aim pressed during a dodge-like ability or in the air keeps the press and aims once the state ends.
  - `Ranged.Aim.StaggerRemovalDeathAndAvatarChangeEndTheStance`: an ability turning exclusive after it started, removing the ability, cancelling every ability and a new avatar end the stance with its tag and camera.
  - `Ranged.Aim.MovementTuningIsFixedPerActivation`: a passive halving `Ability.Tuning.Ranged.AimMovement`, gained while aiming, applies to the next stance only.
  - `ProjectileShot.PresentationTagsFollowTheShot`: drawing and releasing state on owner and authority through the shot; `ProjectileShot.BowContentShootsTheStandardArrow` now asserts that the primary input alone holds the stance and that the release ends it.
  - `Ranged.BowAssetContract`: both bow abilities, their ids, inputs and tags, `GA_BowAim` held and independent, the bow's aim cap and camera, and the Manny hold layer.
- **Wider native regression after the review fixes:** `ue.py test SurvivalRpg.Combat.Ranged SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.EquipmentAttachSocketContract SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.WeaponBlockAssetContract SurvivalRpg.Combat.WeaponAttackAssetContract SurvivalRpg.Combat.StarterEquipmentAssetContract SurvivalRpg.Combat.Block.Lifecycle SurvivalRpg.Combat.HitFeedback SurvivalRpg.Combat.Dodge SurvivalRpg.Combat.ReactionProfile SurvivalRpg.Combat.Runes SurvivalRpg.Combat.SwingTrace SurvivalRpg.Combat.AbilityContent SurvivalRpg.Combat.HyperArmor SurvivalRpg.AbilitySystem SurvivalRpg.Inventory SurvivalRpg.Crafting SurvivalRpg.Equipment SurvivalRpg.Harvesting SurvivalRpg.Input --null-rhi` passed 428 / 428 and `ue.py test SurvivalRpg.Movement SurvivalRpg.Animation SurvivalRpg.GASP.CMC --null-rhi` 46 / 46. The warnings are the known inventory baseline (request-id collisions, Mover example components, controller action rows) and the GASP baseline.
- **One combined native run crashed:** running both batches above in one editor crashed after `RpgMovementProfileTests`: its fixture's garbage collection destroyed the initialized `BootMenu` World Partition, and a later test used it. Split, both batches pass. It was not reproduced on `master`, so it may predate this branch.
- **PIE network tests:** listen server with two clients and 60 ms packet lag, `--ue-arg=-ini:Engine:[ConsoleVariables]:TestFramework.CQTest.CommandTimeout.Network=90`. After the review fixes, `ue.py test SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.Network.CombatImpactCuesPIE SurvivalRpg.Network.KillXPCuePIE` passed 14 / 14:
  - `RangedAimPIE.RemoteAimDrawAndReleaseShowEverywhereAndCapMovement`: the archer's held aim gives server and archer the stance and the bow's authored cap, and the observer sees the aim; a draw shows on server and observer; the release launches one arrow and shows on the observer; the held aim keeps the stance after the shot; releasing aim lowers the bow everywhere and lifts the cap.
  - `RangedAimPIE.DodgeInterruptsTheDrawAndNeedsAFreshPress`: a dodge ends aim and draw on both machines without a shot or arrow cost; the archer stays lowered for a second while both buttons are held; a fresh press aims again.
  - The RNG-01/02 projectile and shot tests, melee, impact cues and kill XP pass unchanged. Warnings are the baseline `LogNetPackageMap` and `AdvancedFriendsInterfaceLog` messages.
- **GASP and combat regression in PIE:** `ue.py test SurvivalRpg.Combat.BlockLocomotion SurvivalRpg.GASP.CMC.Sprint SurvivalRpg.Combat.WeaponHold SurvivalRpg.GASP.Mover.Gameplay` passed 29 / 29 after the review fixes: block locomotion, CMC sprint, the weapon hold layers and the Mover gameplay regression of the shared seams. Warnings are the GASP and Mover baseline (NetworkPrediction rollback notes, Pose Search index builds, out-of-date pose assets).
- **Manual PIE with screenshots:** `Lvl_RpgGaspCMC`, standalone, bow and arrows granted after `ID_TestBackpack`. Captures of the editor window only, kept in the task worktree's ignored `Saved/RNG03/` (`rng03_final_carry.png`, `_ready`, `_draw`, `_draw_up`, `_release`): the lowered carry, the ready stance with the shoulder camera, the full draw level and pitched up, and the release. The center reticle is the crosshair; no trajectory, arc or impact marker appears.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

## Playtest

1. Open `Lvl_RpgGaspCMC` and start PIE (standalone). The bow presentation needs the GASP CMC Manny; `Lvl_ThirdPerson` uses `ABP_Unarmed` without hold layers.
2. In the console: `Rpg.Inventory.GrantItem /Game/SurvivalRpg/Inventory/Items/Test/ID_TestBackpack.ID_TestBackpack_C 1 equip`, then `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C 1 equip` and `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C 20`. The backpack comes first; without it the bow does not fit.
3. Expected: the bow hangs lowered in the left hand. Hold the right mouse button: the camera moves closer over the shoulder, the character turns with the camera and walks slowly, and the bow comes up ready. Hold the left mouse button: the character draws; looking up and down tilts the draw. Release: one arrow flies through the screen center and the release plays. Release the right mouse button: the bow lowers and the normal camera and speed return.
4. Hold only the left mouse button: the same stance and draw; releasing shoots and lowers the bow.
5. Interruptions: during a draw, dodge, jump, or start a sprint. The draw ends without a shot or arrow cost; while the buttons stay held nothing restarts. Release and press again to aim. Press aim during a dodge or jump: the stance begins when it ends. A stagger from enemy hits ends the draw the same way, and a press made during the stagger draws when it ends (see [presses during a stagger](#follow-up-presses-during-a-stagger-2026-11-04)).
6. Multiplayer: Play As Listen Server with two players. Equip the bow on one player as above. On the other player, watch the archer aim, draw (by pitch) and release, and see the bow lower when the archer lets go.

## Limits

- **Mover:** the Mover pawn reads no stance: no speed cap, facing or airborne interrupt. Bow presentation is CMC/Manny only, as scoped; the Mover gameplay regression passes.
- **Presentation scope:** only the GASP CMC Manny (`ABP_RpgGasp_CMC_Manny`) hosts the bow hold layer. `BP_Rpg_Character` with `ABP_Unarmed` and the UEFN Manny show no bow pose; the UEFN skeleton has the socket but no hold layer.
- **Tuning timing:** owner and server resolve `Ability.Tuning.Ranged.AimMovement` at their own activation. A tuning effect that changes between the two can give them different caps for one stance, with corrections while moving. Likewise, a server that still runs a dodge when a client's aim press arrives begins the stance up to one trip later than the predicting owner.
- **No anim snapshot test:** `URpgAnimInstance::CaptureRangedAnimationState` and the proxy aim pitch are verified only visually in PIE and through the tags it reads.
- **No predicted arrow:** the owner sees its arrow only when the server's projectile replicates; a predicted cosmetic arrow is deferred.
- **Real jump input and late join:** the airborne interrupt is tested through the movement mode, not a jump input, and no test joins a client late while someone aims; the stance tag and drawing state are replicated loose tags, which reach late joiners like `State.Blocking`.
- **Combined native run crash:** see Validation.
- **Starter tuning:** the aim cap, camera offsets, blend times and the release presentation time (a native default of 0.5 s) are starting points for RNG-06.
- **No user check yet:** the user validates the whole ranged roadmap at the end (RNG-06).

## Follow-up: presses during a stagger (2026-11-04)

User report from a single-player playtest on `Lvl_CombatSandbox` (`master` `0db9a9e6`), with the sword warrior and the archer active: „wenn ich links gedrückt halte fürs schießen bricht er random mal ab also er schießt nicht“. The bow sometimes stopped while the left mouse button was held, and no arrow came.

### C++ boundary decision

- **Classification:** a fix in the native input policy of the ability system (`URpgAbilitySystemComponent`), plus logging in the shot and aim tasks. No new class, asset or replicated field.
- **Native, and why:** which press activates which ability, and when the owner predicts, are owner-local input and prediction rules of the ability system. The server keeps validating every activation; no authority rule changes.

### Cause

Reproduced in single-player PIE on `Lvl_CombatSandbox` with scripted inputs and the new log lines:

- **Ruled out:**
  - Walking and strafing over the 20 cm step and the 40 cm platform while drawing: no falling frame, every draw shot.
  - Drawing with the primary button alone: every draw shot.
  - Repeated draws while the warrior attacked: every draw that started also shot.
  - The release hand-off and speed curve of PR #379 and PR #380: every release launched an arrow.
- **Found:** one press in a fight run did nothing. Staggers forced through the real `GA_Combat_Stagger` showed why:
  - Hits from the warrior and the archer's arrows fill the player's stagger meter (100 by default). When it breaks, the server-only `GA_Combat_Stagger` cancels the running exclusive abilities. That ends the drawing `GA_BowShot`, which is in `Exclusive_Replaceable`. The stagger then holds `Exclusive_Blocking` for its duration (0.7 s by default).
  - During that time every bow press was refused with `Ability.ActivateFail.ActivationGroup`. `GA_BowShot` activates on the press only, so the held button never tried again.
    - A press held for 3 s, far past the stagger, drew nothing, and its release shot nothing.
    - After a stagger that interrupted a draw, the quick second press fell into the stagger as well. Only a third press drew.
  - The player's pawn data `DA_PawnData_GaspCMC` has no combat reaction profile, so the player's stagger plays no montage. Only the stagger cue's light camera shake marks it. A stagger looks like any other hit, so the bow seemed to lower at random.
  - None of this reached the log: the shot logged its cancel reasons at Verbose, and nothing logged a refused press or a cancel through an activation group.

### Change

- **A held press waits:**
  - `URpgAbilitySystemComponent` keeps a refused, still held press of an ability that is tagged `Ability.Type.RangedAim` and activates on input trigger, which is the bow shot.
  - The press activates as soon as nothing blocks it:
    - **Authority:** when `CanActivateAbility` passes again. It is checked without failure notifications, so a blocked press waits quietly.
    - **Remote owner:** it never runs the server's stagger, so it also waits for the replicated `State.Staggered` and `State.GuardBroken` to clear. While they are set, it does not predict a draw that the server would refuse.
    - **Server refusals:** a press that the server refuses anyway waits 0.2 s before its next try. It stops waiting after three server refusals.
  - Releasing the button, a cancelled hold's suppression, removing the ability or a new avatar ends the wait. The server validates each activation as before.
- **Unchanged rules:**
  - A stagger, dodge, jump, sprint or attack still ends a running draw without cost. The stance, the draw pose and the shoulder camera end, so the bow visibly lowers.
  - The button held through the interruption still never restarts the draw: release and press again. That fresh press now draws as soon as the stagger ends, also when it was made during the stagger.
  - Melee presses are still dropped during a stagger.
- **Logging:** every ranged line goes to `LogRpgAbilitySystem` at `Log` verbosity and starts with "Ranged". The next report can be read straight from the editor log:

  | Line | When |
  | --- | --- |
  | `Ranged shot of <ability> on <avatar> (<machine>) released after <s> s at draw <fraction> (<projectile>)` | A shot released; the machine is authority owner, server or owning client |
  | `Ranged shot ... cancelled without a projectile: <reason> after <s> s drawn` | The shot ended with a reason such as `EarlyRelease`, `InsufficientStamina`, `NoAmmunition` or `TimedOut` |
  | `Ranged shot ... ended with its ability before the release, after <s> s drawn` | Its ability was cancelled, for example by a stagger, a dodge or the aim stance |
  | `Ranged aim of ... interrupted: <reason>` | The stance ended for `Airborne`, `Sprint`, `ExclusiveAbility (<ability>)` or `AvatarChanged` |
  | `Ranged aim of ... cancelled; its held input waits for a release` | The stance's ability was cancelled while its button was held |
  | `Ranged ability ... cancelled by <ability>, which takes its activation group` | An exclusive ability, such as the stagger, cancelled it |
  | `Ranged ability ... could not activate: <failure tags>; active exclusive: <abilities>` | A press was refused; at most once a second per ability |
  | `Ranged ability ... keeps its held press waiting: <why>` and `... activates from its held press, which was refused before` | A held press waits, then activates |

### Validation

- **Build:** `python Build/Tools/Unreal/ue.py build` passed, 0 errors, 0 warnings.
- **New native test** `SurvivalRpg.Combat.ProjectileShot.BowPressHeldThroughAStaggerDrawsWhenItEnds`: the real `GA_BowShot` from `AS_Bow` with a test ability that does what the stagger does (cancel the exclusive replaceable group, then hold the blocking group).
  - A press during it is refused but waits; it neither aims nor draws while the block lasts, and starts aim and draw at its end with draw progress 0. The release launches one arrow.
  - A block during a draw lowers the bow without cost; the button held through it does not restart the draw. A fresh press during the next block draws at its end and shoots.
  - With only the replicated `State.Staggered` set, as a remote owner sees the server's stagger, a press predicts no draw, waits, and draws when the tag clears.
  - A press released while it waits starts nothing.
  - **Mutation check:** with the retry call removed, the test fails at "The held press starts the shot when the stagger ends"; restored, it passes.
- **New PIE test** `SurvivalRpg.Network.RangedAimPIE.AShotPressedDuringTheServersStaggerDrawsWhenItEnds` (listen server, two clients, 60 ms lag, the real `GA_Combat_Stagger` on the server with a 2 s duration):
  - the remote archer presses during its stagger and holds; the server never draws during the stagger, draws after it and launches one arrow on the release;
  - a stagger during the second draw lowers the bow on the client; its release and a fresh press during the stagger draw after the stagger and launch the second arrow.
- **Native suites:** `ue.py test SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Combat.Ranged SurvivalRpg.AI SurvivalRpg.UI.RangedCombat SurvivalRpg.AbilitySystem SurvivalRpg.Input --null-rhi`, 75/75.
- **PIE suites:** `ue.py test SurvivalRpg.Network.RangedAimPIE SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.RangedEnemyPIE SurvivalRpg.Network.CombatImpactCuesPIE`, 14/14. The warnings are the known baseline: `LogNetPackageMap`, `AdvancedFriendsInterfaceLog`, the late-join arrow body's socket lookups and one teardown `SpawnActor failed`.
  - A first run failed only in the new test's last step, which counted two flying arrows when the first had already landed and become a pickup. The step now checks the spent arrows.
- **Two-player listen-server PIE** on `Lvl_CombatSandbox`, own editor, real GASP pawns, enemies removed, the host as archer and the client watching:
  - **Press during a stagger:** the host's press waited (`keeps its held press waiting: staggered`), activated after the stagger and launched one arrow, seen on server and client.
  - **Stagger during a draw:** the host's bow lowered at once and on the client one frame later. A fresh press drew again on both and launched one arrow on both.
  - The remote archer's own stagger is covered by the PIE test above. `Rpg.Inventory.GrantItem` equips only the host.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

### Limits

- **The player's stagger stays invisible.** It has no montage; only the lowered bow and the stagger cue's light camera shake show it. A stagger reaction for the player, such as a combat reaction profile on `DA_PawnData_GaspCMC`, is combat content outside this fix.
- **Only ranged aim presses wait.** A melee press during a stagger is still dropped.
- **Other server-only blocks on a remote owner.** The owner waits for the replicated stagger state only. Any other block that only the server sees is found through the server's refusal: up to three tries 0.2 s apart, each showing the draw for about one round trip, after which the press stops waiting and needs a fresh press. No current content has such a block.
- **A button held through an interruption** still needs a release and a fresh press, as the RNG-03 rule says.
- **Not covered by a test:** the real warrior's hits breaking the meter. The tests stagger through the stagger ability or a blocking test ability. The same wait now also covers a press during a dodge's blocking recovery, which the native test covers only through its generic blocking ability.
