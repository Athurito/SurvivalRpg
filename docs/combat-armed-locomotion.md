# CMB-02: Armed locomotion

Task report for CMB-02 in the [combat roadmap](combat-roadmap.md). Held weapons now bring a hold pose for their arm, layered over the GASP locomotion, so the sword and shield stay at the character's sides instead of swinging through the body.

## Behavior

### Problem

The GASP locomotion is unarmed: the arms swing freely in every gait, and a held sword or shield passes through the legs and torso (user report 2026-10-13). Nothing in the pawn's AnimBP knew that a weapon was in hand.

### Change

- **Per-hand hold layers.** Two Anim Layer Interfaces in core, one per hand, so the sword's and the shield's layers never overwrite each other:
  - `Characters/GASP/Shared/RPG/WeaponHold/ALI_RpgMainHandHold` with the layer `MainHandHold` (input pose `MainHandBasePose`);
  - `.../ALI_RpgOffHandHold` with `OffHandHold` (input pose `OffHandBasePose`).

  `LinkAnimClassLayers` binds every interface a class implements, so one interface per hand is what lets two items link side by side.
- **Host.** `ABP_RpgGasp_CMC_Manny` implements both interfaces with pass-through defaults. Two linked-layer nodes sit after foot placement and leg IK and before the block layer, the `DefaultSlot` montage slot and pose history. Montages, block and traversal still override the arms.
- **Content (`GF_Combat_Core/Animations/WeaponHold`).** `ABP_Hold_SwordShield_MainHand_Manny` and `ABP_Hold_SwordShield_OffHand_Manny` on the Manny skeleton. Each blends one frame of the pack's `Walk_Combat_Loop_F_0_Seq` onto its arm in local space: the sword arm fully from `clavicle_r`, the shield arm from `upperarm_l` at 70 % weight (see "Shield arm tuning").
- **Schema (native).** `URpgEquipmentDefinition::HoldPresentations`: per gameplay skeleton a `MainHandLayer` and an `OffHandLayer` (`FRpgEquipmentHoldPresentation`), resolved through `ResolveHoldLayer(Skeleton, Slot)`. Data validation rejects a missing or repeated skeleton and any layer that is not a concrete compiled Anim Blueprint with an implemented layer for that skeleton.
- **Mechanism (native).** `URpgEquipmentManagerComponent` links the hold layer of every item in a hand on the gameplay mesh's main AnimInstance, on every machine, from the replicated equipment list. It reuses the block layer's lifecycle: equip and unequip through the FastArray callbacks and the tick, gameplay-mesh change, AnimInstance replacement and reinitialization, `EndPlay` and `UninitializeComponent`. Unlike block it needs no ability system. `GetWeaponHoldLayerInstance(Slot)` exposes the linked instance for tests and debugging.
- **Data.** `ED_BasicSword` and `ED_BasicTwoHandedSword` use the main-hand layer, `ED_BasicShield` the off-hand layer, all for the Lyra `SK_Mannequin`.

### Choosing the pose

| Try | Result |
| --- | --- |
| `Idle_Combat_Seq`, arm chain, local space | Arms spread out sideways. The pack's combat idle stands side-on (hips and chest turned 33° and 41° against its plain idle), so its arms do not fit the frontal GASP torso. |
| `Idle_Combat_Seq`, mesh-space rotation | Front view plausible, but from the side the sword arm reaches behind the body. |
| `Walk_Combat_Loop_F_0_Seq`, arm chain, local space | Nearly frontal stance (hips and chest within 12° of the plain idle). Sword held low at the right side, shield at the left side; no clipping seen standing or running. |

The pose is frozen at its first frame (`PlayRate 0`), so the arms do not swing to the walk rhythm while standing.

### Shield arm tuning

The user's check on 2026-10-20 found the shield arm stiff and too far from the body. Upper-arm abduction from the body, measured in PIE in the pawn's frame (`Lvl_RpgGaspCMC`, standalone, Paladin with sword and shield):

| Shield-arm blend | Abduction standing | Running |
| --- | --- | --- |
| None (free GASP arm) | 15° | shield swings through the body |
| From `clavicle_l`, 100 % (first version) | 39° | static, far out |
| From `upperarm_l`, 100 % | 32° | |
| From `clavicle_l`, 70 % | 32° | |
| **From `upperarm_l`, 70 % (kept)** | **25°** | arm keeps some swing, shield stays beside the leg |
| From `upperarm_l`, 50 % | 23° | shield swings in front of the leg, hand down to about 9 cm from the thigh |

- About half of the spread came from blending from the clavicle, which carried over the pack's raised shoulder. The rest is the pack's own carry: sampled body-relative, it holds the shield arm about 30° out in every idle, walk and run clip, its relaxed `Idle_Seq` included, so another clip would not help.
- Starting at the upper arm keeps the GASP shoulder motion; 70 % weight lets 30 % of the locomotion swing through. The sword arm is unchanged.

## C++ boundary decision

- **Classification:** reusable schema (hold presentation on the equipment definition) and mechanism (linking lifecycle), plus designer content (interfaces, host nodes, hold layers, definition data).
- **Runtime truth:** the replicated equipment list (definition and equipped slot per entry). Animation only presents it.
- **Existing seam:** the block layer's linking in `URpgEquipmentManagerComponent::RefreshBlockLocomotionLayer` and the Lyra pattern of item anim layers.
- **New native classes:** none. One struct, one property and resolver on the definition, and private linking state on the manager. Native because the binding must follow replication, mesh and AnimInstance replacement on every role, exactly like the block layer.
- **Asset work:** interfaces, host graph and hold layers authored through Unreal MCP and the project animation toolset; definitions edited on their class defaults; all compiled without warnings and validated.

## Validation (2026-10-20, on `master` `8cc6fed9` plus this change)

- **Build:** `ue.py build` passed.
- **Assets:** the two interfaces, both hold layers and the host compile without warnings; data validation of the three definitions, both layers, both interfaces and the host: 8 / 8 valid, 0 warnings (including the asset reference restrictions).
- **New tests:**
  - `SurvivalRpg.Combat.WeaponHold.Validation.PresentationResolvesPerSkeletonAndHand` passed: resolution per skeleton and hand, rejection of a native AnimInstance, a foreign skeleton, a missing and a repeated skeleton.
  - `SurvivalRpg.Combat.WeaponHold.WeaponHoldListenServerPIE.HeldItemsLinkHoldLayersOnEveryRoleAndFollowTheHands` passed (listen server, one client, a late joiner): both layers linked for the armed owner, on the server for the remote pawn and the host, and on the late joiner's simulated proxy; holstering unlinks them on all three; drawing the sword relinks the main hand on all three; replacing the owner's main AnimInstance relinks it.
- **Regression**, `ue.py test` with the test editor's MCP server off:

  | Suite | Result |
  | --- | --- |
  | `SurvivalRpg.Combat` (incl. the new tests and block locomotion) | 36 / 36 |
  | `SurvivalRpg.GASP.CMC` | 14 / 14 |
  | `SurvivalRpg.GASP.Mantle` | 13 / 13 |
  | `SurvivalRpg.GASP.Vault` | 9 / 9 |
  | `SurvivalRpg.GASP.Hurdle` | **11 / 12**, see "Hurdle cancel test" |
  | `SurvivalRpg.Equipment`, `SurvivalRpg.Cosmetics` | 24 / 24 |
  | `SurvivalRpg.Network` | 23 / 23 |
  | `SurvivalRpg.Animation` | 4 / 4 |
  | `SurvivalRpg.GASP.Mover.Gameplay`, `.Lifecycle` (`np.ForceReconcile 0`, `t.MaxFPS 30`) | 8 / 8 |

- **PIE, standalone, `Lvl_RpgGaspCMC`:** without manual linking, the equipment manager links `ABP_Hold_SwordShield_MainHand_Manny` for the main hand and the off-hand layer for the shield; the linked instance stayed the same over about 200 frames (no relinking per tick). Idle and running captures show both weapons beside the body.

- **After the user's review (shield arm at `upperarm_l`, 70 %; reworked hurdle test):** `ue.py build` passed (0 errors, 0 warnings); data validation of the same 8 assets: 8 / 8 valid, 0 warnings; `SurvivalRpg.Combat.WeaponHold` 2 / 2; `SurvivalRpg.GASP.Hurdle` 12 / 12; the cancel test alone 9 / 9.

### Hurdle cancel test

`SurvivalRpg.GASP.Hurdle.GaspHurdleAuthoredMapPIE.CancelledHurdleReleasesMovementCollisionAndRotation` failed at `RpgHurdleIntegrationTests.cpp:409`: after the server cancels a 35° running hurdle, the owner's yaw had to be within 5° of the approach yaw.

- **Rate:** with the hold layers it passed 1 of 4 runs on its own (plus the failure in the suite run). With the three definitions swapped back to their `master` state (no hold layers, same code) it passed 4 of 4, and 3 of 3 more with diagnostics.
- **Mechanism (temporary log at the assertion, removed again):** nothing turns the owner back after the cancel. The test releases W, and the owner coasts to a stop. While it moves, the combat stance turns it towards the view; once it stands, the stance holds its facing (`URpgCharacterMovementComponent::IsCombatStanceHoldingFacingForMove`). The yaw at standstill therefore depends on how far the hurdle's warp turned the owner before the cancel and how long it coasted. Without the layers the owner ended at 31.8°, 35.0° and 35.0°; with them at 29.9°, 29.4° and 32.0°. One frame of warp turn is about 3°, so the 5° tolerance allowed at most one frame.
- **Not the cause:** no relinking per tick (stable instance), no root motion from the layers (the AnimBP extracts root motion from montages only), same traversal montage and montage times in passing and failing runs.
- **Reading:** the layers' extra animation work shifts the owner's frames by about one frame relative to the server's cancel. GASP-17 already recorded one failure at the same line. The gameplay result (a few degrees more turn before a cancelled hurdle) is not a defect.
- **Fix (user decision 2026-10-20, GASP-owned test changed in this PR):** the standstill yaw check is gone. After the existing cleanup checks the test holds W again and waits up to 5 s until the owner and the server both move faster than 30 cm/s and face the unchanged view within 5°. It then releases W and asserts that the renewed input started no second traversal and left the view unchanged. This proves what the name promises: the cancel hands the rotation back to ordinary movement. The input report line now also logs the owner's yaw.
- **Result:** 9 / 9 runs on its own and 1 / 1 in the suite run passed. The logged standstill yaw was 0.0° in two runs, 34.1° and 35.0° in the others; the old check would have failed the two runs at 0°. Renewed input turned the owner back within 41 to 212 ms. In one run the owner pressed Space at 0.6° instead of about 35° (the cancel case does not assert the press yaw, as before); the renewed input still had to turn it by 35°.

## Playtest

1. Open `Lvl_RpgGaspCMC` and start PIE (Paladin with sword and shield).
2. Stand still: the sword hangs low at the right side, the shield close beside the left leg.
3. Walk, run and sprint, also sideways and backwards: neither weapon passes through the legs or torso.
4. Attack, block and mantle: those animations take over the arms as before.
5. Holster both hands (press the active weapon's action-bar slot again): the arms swing freely again. Draw the sword: the right arm holds it again.
6. Listen server with two clients: every player sees every armed character with the same hold pose, also after one player holsters.

## Limits

- **Static arms.** The hold pose is a single frame: the sword arm does not swing while running, the shield arm keeps only 30 % of the locomotion swing. A pose that moves with the gait would need the pack's combat locomotion as a Motion Matching database or a gait-synced layer (a GASP task).
- **Tools.** The pickaxe and axe have no hold layer: their GameFeature (`GF_Harvesting_Magic`) may not reference `GF_Combat_Core` assets. A tool hold layer needs a pose inside `GF_Harvesting_Magic`, for example from the survival mocap pack.
- **Two-handed sword.** It borrows the one-handed main-hand pose.
- **Mover.** The UEFN Mover host has no hold nodes, and the definitions carry only a Manny presentation; the Mover keeps free arms.
- **Equip pop.** Linking switches the arm pose in one frame when an item enters or leaves a hand.
- **Not checked visually:** strafing and backpedaling, sprinting, the armored Paladin with the hold pose, and the two-handed sword.
- **Grip check.** The user's visual check of the CMB-01 grips is deferred to this task.
