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
5. Interruptions: during a draw, dodge, jump, or start a sprint. The draw ends without a shot or arrow cost; while the buttons stay held nothing restarts. Release and press again to aim. Press aim during a dodge or jump: the stance begins when it ends.
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
