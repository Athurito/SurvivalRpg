# RNG-04: Bow crosshair, draw strength and ammunition HUD

Task report for RNG-04 in the [ranged combat roadmap](ranged-combat-roadmap.md), [PR #369](https://github.com/Athurito/SurvivalRpg/pull/369). While the player aims a bow, a crosshair shows around the screen centre, with a draw-strength bar below while drawing and a mark at full draw. A slot right of the Q/E/R abilities shows the ammunition and its count while a ranged weapon is equipped, and marks it when empty. Nothing shows a trajectory, an arc, a landing point or a ballistic correction.

## C++ boundary decision

- **Classification:** native read model (`URpgRangedCombatViewModel`, the owner's local draw timing on the ability system) and designer-owned HUD content (two CommonUI widgets, their registration and the crosshair slot layout).
- **Runtime truth:**
  - **Aim and draw:** the owner's ability system. Aim tokens from RNG-03; the local draw flag and, new here, its draw timing, which the shot task sets from the activation's resolved tuning.
  - **Weapon:** the `URpgRangedWeaponInstance` equipped on the pawn, whose shot definition names the ammunition the shot spends.
  - **Ammunition:** the player state's inventory, the same one the shot consumes from, carried bags included.
  - The HUD only reads.
- **Existing seams:**
  - `URpgUiSubsystem` and `URpgLocalPlayerViewModelResolver` (per-local-player view models, bound across pawns and ability system changes);
  - the inventory `StackChanged` message;
  - `URpgEquipmentManagerComponent`;
  - the HUD slots `UI.HUD.Slot.Crosshair` and `UI.HUD.Slot.AbilityResource`;
  - the GameFeature action Add Rpg Widgets.
- **Native, and why:**
  - The view model binds lifecycle-safe to the local player, pawn, ability system and inventory messages, and ticks only while bound.
  - The draw timing is owner-local prediction state that only the shot task knows; Blueprint cannot read it without a native seam.
- **New native classes:** one, `URpgRangedCombatViewModel`, as planned in the roadmap.
- **Content:** `CUI_RangedCrosshair`, `CUI_AmmunitionCounter`, the GameFeature registration and the crosshair slot change are assets.
- **Asset work:** Unreal MCP (`UMGToolSet`, `MVVMToolset`, `ObjectTools`) plus editor Python for the GameFeature action and one function binding; validated by `SurvivalRpg.UI.RangedCombat.HudAssets` and in PIE.

## Behavior

### Problem

RNG-03 gave the bow an aim stance and a shoulder camera, but the HUD showed only the interaction dot. The player could not see the draw strength, the ammunition left, or that the bow was empty.

### Change

- **View model:** `URpgRangedCombatViewModel`, one per local player in `URpgUiSubsystem`. Its fields:
  - `bHasRangedWeapon`, `AmmunitionCount` and `AmmunitionCountText`, `bOutOfAmmunition`, `AmmunitionName`, `AmmunitionIcon`, `bShotAvailable`;
  - `bAiming`, `bDrawing`, `DrawFraction`, `NockFraction`, `bNocked` and `bFullyDrawn`.
- **Updates:**
  - **Ammunition:** follows inventory messages for the player's own inventory.
  - **Weapon:** while bound to a pawn's ability system, the view model checks the pawn's ranged weapon instance every 0.2 s. The equipment manager sends no change notification.
  - **Aim and draw:** read every frame, but only while a ranged weapon is equipped.
- **Draw strength:**
  - The shot task gives the owner's ability system the draw start and the resolved full-draw and nock times (`SetLocalRangedDrawTiming`).
  - `GetLocalRangedDrawProgress` returns the same fraction the shot computes from its elapsed draw time.
  - Remote machines never read it: the HUD is local only.
- **Crosshair (`CUI_RangedCrosshair`):**
  - four bone-coloured ticks around the free centre while aiming, so the interaction dot stays visible in the middle;
  - a 64 × 6 bar below while drawing, `Percent` = `DrawFraction`;
  - a diamond at full draw.
  - It is registered in `UI.HUD.Slot.Crosshair`.
- **Ammunition counter (`CUI_AmmunitionCounter`):**
  - a 64-unit gear slot with the ammunition icon, the count bottom right, and the forbidden glyph when empty;
  - visible only while a ranged weapon is equipped;
  - registered in `UI.HUD.Slot.AbilityResource`, right of Q/E/R.
- **Crosshair slot:** `CUI_RpgHudLayout`'s `CrosshairExtensionPoint` was a horizontal box. It now stacks its entries centred: overlay, centred alignment, one `(0, 0)` spacing pattern. Otherwise the bow crosshair would push the interaction dot aside.
- **Registration:** `GF_Combat_Ranged` registers both widgets through an Add Rpg Widgets action. They appear in every Experience that activates the feature (Baseline, GaspCMC, GaspMantle, InventoryStorage, Prototype), not on the Mover maps, which have no bow presentation.

## Validation (2026-11-02)

- **Review:** two review agents worked on separate lenses: (1) runtime, lifecycle and performance; (2) contract, architecture boundary and tests. I checked each finding against the code and engine source. Confirmed and fixed (commit `2f34a04a`):
  - **Weapon:** the lookup took any ranged instance; it now takes the main hand, else the off hand. `bHasRangedWeapon` depended on the weapon naming ammunition.
  - **Inventory:** the count read the controller's player state instead of the inventory the shot spends from.
  - **Lifecycle:** the inventory listener could survive an unbind after the controller was gone.
  - **Assets:** the interaction HUD authoring tool would rebuild the crosshair slot as a horizontal box. The asset test froze the number of view model sources and the spacing pattern length.
  - **PIE gaps:** the observer step passed without a bound view model, and nothing covered the last arrow on a client or putting the bow away.

  The contract reviewer also suspected a stale count when a client loses its last stack, because `PreReplicatedRemove` broadcasts before the entry leaves the list. The runtime reviewer traced the following `OnRep_InventoryRevision` broadcast, which recounts in the same update. The new PIE step that removes the client's last arrows confirms the HUD shows 0. Kept as limits: the HUD runs one frame behind (core ticker after Slate), and there is no listen-server host HUD step.
- **Build:** `python Build/Tools/Unreal/ue.py build` passed (0 errors, 0 warnings).
- **Native tests** (`--null-rhi`): `SurvivalRpg.UI.RangedCombat SurvivalRpg.Combat.ProjectileShot SurvivalRpg.Interaction` passed 50 / 50 after the fixes, including:
  - `UI.RangedCombat.BowShootsTheStandardArrow`: the bow equipped through the equipment manager names its ammunition, icon and name; putting it away hides the HUD.
  - `AmmunitionFollowsTheInventory`: the count follows the player inventory only, the empty mark, and the hidden HUD without a weapon.
  - `DrawStrengthFollowsTheLocalDraw`: draw fraction, nock, full draw, reset, and the ticker lifecycle.
  - `HudAssets`: both widgets are registered in their slots, resolve the view model and have bindings; the crosshair slot stacks and centres its entries.
  - `ProjectileShot.PresentationTagsFollowTheShot`: the shot's draw progress on the owner, cleared after the release.
- **Native regression:** `SurvivalRpg.UI SurvivalRpg.Combat SurvivalRpg.AbilitySystem SurvivalRpg.Inventory SurvivalRpg.Interaction SurvivalRpg.GameFeatures SurvivalRpg.Equipment` passed 401 / 401 before the review fixes, with the 16 baseline warnings. The changed paths were rerun after the fixes, as listed above.
- **PIE network tests** (listen server, 2 clients, 60 ms lag): `RangedAimPIE CombatProjectilePIE` passed 8 / 8. The extended `RemoteAimDrawAndReleaseShowEverywhereAndCapMovement` checks:
  - the archer client's HUD: the bow and 10 arrows; then crosshair, draw, nock and full draw; 9 arrows after the release;
  - 0 arrows and no shot once the server removes the rest;
  - the HUD hidden after the server unequips the bow;
  - the unarmed observer's bound HUD without a ranged weapon.
- **Manual PIE** on `Lvl_RpgGaspCMC`, standalone, with screenshots (not committed):
  - the counter beside Q/E/R with the arrow icon and count;
  - the crosshair ticks around the centred interaction dot while aiming;
  - the gold draw bar and the full-draw diamond;
  - count 0 with the forbidden mark after three shots.

  No trajectory, arc, landing marker or ballistic offset appears. An earlier pass showed the interaction dot pinned top left beside the crosshair, which led to the `SpacingPattern` fix.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

## Playtest

1. Open `Lvl_RpgGaspCMC` and start PIE in standalone.
2. In the console, run:
   - `Rpg.Inventory.GrantItem /Game/SurvivalRpg/Inventory/Items/Test/ID_TestBackpack.ID_TestBackpack_C 1 equip`
   - `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C 1 equip`
   - `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C 3`
3. Expected: a slot right of Q/E/R shows the arrow icon and "3"; no crosshair yet, only the interaction dot.
4. Hold the right mouse button. Expected: four ticks appear around the dot.
5. Hold the left mouse button. Expected: a gold bar below the crosshair fills over the draw; a diamond appears at full draw. Release: one arrow flies, the count drops to 2, the bar disappears.
6. Shoot the remaining arrows. Expected: the count shows 0 with a red forbidden mark; the bow can no longer draw.
7. Unequip the bow. Expected: the slot disappears.
8. Multiplayer: start PIE as a listen server with two players and give only one the bow and arrows. Expected: only that player sees the ammunition slot and, while aiming, the crosshair; the other player's HUD stays unchanged.

## Limits

- **One frame behind:** the view model ticks on the core ticker, which runs after Slate paints, so the HUD shows the draw one frame late.
- **Polling:** the pawn's weapon is checked every 0.2 s, because the equipment manager sends no change notification. A weapon swap can show up to 0.2 s late.
- **Shot availability:** `bShotAvailable` checks only the ammunition; stamina for the release shows on the stamina bar. No widget binds it yet.
- **Not tested directly:**
  - the listen-server host's own HUD;
  - rebinding after death and respawn (it rides on the `URpgUiSubsystem` rebinding the character stats already use);
  - ammunition removed while drawing.
- **Nock point:** the crosshair does not mark the nock on the bar; `NockFraction` and `bNocked` are there for styling in RNG-06.
- **Mover maps:** the HUD appears only in Experiences that activate `GF_Combat_Ranged`, so not on the Mover maps, which have no bow presentation either.
- **Tuning:** sizes, colours and the bar fill are starter styling for RNG-06.
