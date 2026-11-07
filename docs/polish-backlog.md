# Polish backlog

Uncategorized polish and follow-up items that belong to no active roadmap,
collected at the user's request on 2026-11-07. The list has no order. Each
item becomes its own bounded task and PR, with its ID in the branch name, for
example `claude/pol-01-crosshair-settings`. An item that grows into a feature
area moves into a roadmap of its own. Record the PR in the status column when
an item is done.

## Items

| ID | Item | Source | Status |
| --- | --- | --- | --- |
| POL-01 | **Crosshair settings in the settings menu.** A HUD category in the Game tab: presets (small, medium, large, custom), stroke length, thickness, gap, outline, colour, opacity, centre point, draw bar and full-draw diamond on or off, with a live preview. The choice is local presentation, saved per user like the controller glyph choice (`URpgControllerGlyphSubsystem`, [ui-roadmap.md](ui-roadmap.md#settings-menu-ui-07)), never replicated and never read by gameplay; the crosshair reads it through its view model. | User, 2026-11-06. [#383](https://github.com/Athurito/SurvivalRpg/pull/383) already exposes every size as a variable in `CUI_RangedCrosshair`. | Open |
| POL-02 | **Visible player stagger.** The player pawn has no reaction profile in `DA_PawnData_GaspCMC`, so a stagger looks like an ordinary hit. Melee presses during a stagger are still dropped, while bow presses now wait. | [#381](https://github.com/Athurito/SurvivalRpg/pull/381) | Open |
| POL-03 | **Late join during a draw.** No test or PIE run has shown a late joiner the drawing posture, pulled string and nocked arrow of a player who is already drawing. | [ranged-acceptance.md](ranged-acceptance.md), [#382](https://github.com/Athurito/SurvivalRpg/pull/382) | Open |
| POL-04 | **Second bow model.** Run the "Add a bow" recipe in [ranged-bow-draw-presentation.md](ranged-bow-draw-presentation.md) once with a second bow mesh from the asset library, to prove `rig_bow_mesh` and `BowFamilyContract` on more than one bow. | [#382](https://github.com/Athurito/SurvivalRpg/pull/382) | Open |
| POL-05 | **Bending bow limbs.** Only the string bends today. Limb bending needs limb bones in the shared `SKEL_Bow`, which affects every bow. | [#377](https://github.com/Athurito/SurvivalRpg/pull/377), [#382](https://github.com/Athurito/SurvivalRpg/pull/382) | Open |
| POL-06 | **Dedicated-server check of the bow.** Never run: shafts are placed from the server's pose there, and hosts attach no meshes. | Ranged validation, [ranged-acceptance.md](ranged-acceptance.md) | Open |
| POL-07 | **Design question: arrows and carried shields.** A shield that is not actively blocking lets arrows pass and the shaft sticks in the arm behind it. Decide whether a carried shield should catch arrows. | Ranged validation (observation O1) | Open |
| POL-08 | **Design question: downed players as targets.** The archer and the warrior keep attacking a downed player. Decide whether enemies spare downed players so they can be revived. | [ranged-acceptance.md](ranged-acceptance.md) | Open |

## Tracked elsewhere

- **Predicted cosmetic arrow:** an open row under "Later extensions" in
  [ranged-combat-roadmap.md](ranged-combat-roadmap.md#later-extensions).
- **Low findings of the ranged validation:** listed as still open in
  [ranged-acceptance.md](ranged-acceptance.md).
