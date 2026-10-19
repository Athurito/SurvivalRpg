# GASP-17 – Paladin in the production CMC Experiences

Status: **In progress** since 2026-10-19 on `claude/gasp-17-paladin-production`. Defined in [#267](https://github.com/Athurito/SurvivalRpg/pull/267) and marked Ready by the user. [`GASP-16`](gasp-character-armor-pilot.md) is merged ([#266](https://github.com/Athurito/SurvivalRpg/pull/266)). This report is the task report.

## Goal

The production CMC pawns show the MetaHuman Paladin from `GASP-16`, and its armor comes from the inventory as on the pilot map.

The Paladin arrives through one Experience action set, which can be removed again:
- Removing the action set from an Experience restores today's look: the plain Manny without armor visuals.
- The pawn Blueprints, PawnData, animation and gameplay stay unchanged.
- No new native code is needed.

## Why

- The user checked the pilot on 2026-10-19 and decided to keep the Paladin.
- The user also asked for a way back before the production pawns depend on it.
- Lyra composes the player's cosmetic character in the Experience: an `AddComponents` action gives each controller `B_PickRandomCharacter`. The pilot already works this way, and a later character editor plugs into the same controller component.
- PawnData is the wrong owner:
  - `URpgPawnData` has no field for character parts, so it would need a new native field and init code.
  - The look would hang on the pawn type instead of the player.

## Current state (checked 2026-10-19 on `claude/gasp-16-character-armor-pilot` `dce5c9d4`)

| Area | State |
| --- | --- |
| Pilot | `RpgGaspPaladinExperience` is the Mantle Experience plus one `AddComponents` action. The action adds two components: `BPC_RpgPawnParts_GaspManny` (pawn parts component; switches the gameplay mesh to `SKM_Manny_Simple_GaspLook_Invis` while `Cosmetic.Body` is present) on the CMC Mantle pawn, and `BPC_RpgControllerParts_PaladinBody` (adds `B_RpgPart_MetaHumanPaladin`) on the controller. Map `GF_Dev_Sandbox/Maps/Lvl_RpgGaspPaladin` with a 6×6 chest of the five Paladin pieces. |
| CMC Experiences | `RpgGaspCMCExperience`, `RpgGaspMantleExperience` and `RpgInventoryStorageExperience` (see below). |
| Other Experiences | Mover and Mover-Ragdoll play UEFN pawns. Baseline and Prototype play `BP_Rpg_Character`. Enemies have their own PawnData. |
| Armor items | `ID_BasicHelmet` (now with the Paladin helmet part) and `ID_PaladinChest`, `Bracers`, `Pants`, `Boots` in `GF_Combat_Core`. Without a pawn parts component they show nothing. |
| Action sets | The project already composes Experiences from `URpgExperienceActionSet` assets, for example `LAS_RpgGasp_Mantle` and `LAS_Rpg_StandardUI`. |
| Tests that read the gameplay mesh | `RpgGaspCMCIntegrationTests`, `RpgBlockLocomotionTests` (CMC variant) and `RpgRuntimeRetargetTests` expect `SKM_Manny_Simple_GaspLook`. With the body part, `GetMesh()` plays the invisible copy. The character-parts PIE tests use `RpgGaspPaladinExperience`. |

CMC Experiences, taken from `docs/gasp-manny-gameplay-mesh.md`. They are re-checked in the editor at the start, together with every map that selects them:
- `RpgGaspCMCExperience` uses `BP_RpgGasp_CMC` (`Lvl_RpgGaspCMC`).
- `RpgGaspMantleExperience` uses its child `BP_RpgGasp_MantleCharacter` (`Lvl_RpgGaspMantle`, `GF_Dev_Sandbox` `Lvl_ResourceParity`).
- `RpgInventoryStorageExperience` uses the Mantle pawn as well (`Lvl_RpgInventoryStorage`).

## Scope

1. **Action set.**
   - Create `LAS_RpgCharacter_Paladin` in `System/Experiences/ActionSets`, the only folder where the asset manager registers Experience action sets. It holds the pilot's `AddComponents` entries:
     - the pawn parts component on `BP_RpgGasp_CMC`, so its Mantle child is covered;
     - the controller parts component on the player controller, with the same client/server flags as the pilot.
   - Switch `RpgGaspPaladinExperience` to the action set. The pilot map then proves that it behaves exactly as before.
2. **Production Experiences.**
   - Add the action set to `RpgGaspCMCExperience`, `RpgGaspMantleExperience` and `RpgInventoryStorageExperience`.
   - Leave the Mover, Mover-Ragdoll, Baseline and Prototype Experiences unchanged.
3. **Retire the pilot Experience.**
   - `RpgGaspPaladinExperience` is now the Mantle Experience plus the action set, so remove it.
   - `Lvl_RpgGaspPaladin` keeps its chest and moves to `RpgGaspMantleExperience`.
   - The character-parts PIE tests move to the Mantle Experience.
4. **Tests.**
   - The CMC, block and retarget tests above check the gameplay mesh's skeleton and AnimBP, or accept the invisible copy while a body part is present, instead of one mesh asset.
   - Add a check that a Mover pawn gets no parts component.
5. **Validation and user check.** See [Acceptance criteria](#acceptance-criteria).

## Out of scope

- The Mover and Mover-Ragdoll pawns, which stay on UEFN.
- Baseline and Prototype pawns (`BP_Rpg_Character`), and AI enemies.
- Character editor, male MetaHuman player variants (separate package), sound and armor Foley.
- Where players find the Paladin armor in the world (loot, crafting, vendors), armor stats and balance.
- The ragdoll trail of one physics frame and the render-thread visibility wait from `GASP-16`.

## Decisions

- 2026-10-19 (user): keep the MetaHuman Paladin from `GASP-16`.
- 2026-10-19 (user): adopt it in the production CMC Experiences as a separate task. Keep it separable through an Experience action set rather than the pawn Blueprint or PawnData. Define this task and mark it Ready.

## C++ boundary

- **Classification:** designer-owned composition. It reuses the native Character Parts mechanism from `GASP-16`.
- **Runtime truth:**
  - The server-owned part lists on pawn and controller, and the equipment state, as in `GASP-16`.
  - The Experience decides only which components exist.
- **Existing seams:**
  - `URpgExperienceDefinition::ActionSets` and `URpgExperienceActionSet`;
  - the `AddComponents` GameFeature action;
  - `BPC_RpgPawnParts_GaspManny` and `BPC_RpgControllerParts_PaladinBody`.
- **Ownership:**
  - Assets: the action set and the Experience edits.
  - C++: test code only.
- **New native classes:** none.
- **Asset work:**
  - Unreal MCP authoring of the action set and the Experiences;
  - compile and save;
  - an asset-registry referencer check for the retired pilot Experience;
  - PIE probes.

## Ownership

| Concern | Owner |
| --- | --- |
| Which Experiences show the Paladin | `LAS_RpgCharacter_Paladin` in each Experience's action sets |
| Parts, body, armor visuals | Unchanged from `GASP-16` |
| Pawn Blueprints, PawnData, AnimBPs, movement | Unchanged |
| Verification | Existing network and integration suites on the production Experiences, adjusted mesh checks, one Mover check |

## Acceptance criteria

| Area | Criterion |
| --- | --- |
| Look | On `Lvl_RpgGaspCMC`, `Lvl_RpgGaspMantle` and `Lvl_RpgInventoryStorage`, the player is the MetaHuman Paladin. Armor equipped from the inventory shows as on the pilot map. |
| Way back | Removing the action set from an Experience restores today's Manny look without other changes. This is shown once in PIE and then restored. |
| Pilot | `Lvl_RpgGaspPaladin` behaves as before on the Mantle Experience. `RpgGaspPaladinExperience` is removed and nothing references it. |
| Untouched | Mover, Mover-Ragdoll, Baseline and Prototype pawns have no parts component and look as before. |
| Network | On a production map: owner, listen server, proxy and late joiner see the same body and armor after equip, unequip, death and respawn. A dedicated server spawns no part actors. |
| Regression | The `GASP-16` regression set passes: Cosmetics, Equipment, Inventory, Animation, GASP.CMC, Combat.BlockLocomotion, GASP.Mantle, Vault, Hurdle, VariantComparison, Network and GASP.Mover. Exceptions are only documented timing cases. |
| User check | The user plays a production CMC map alone and as listen server with a client. |

## Verification

- Build and test through `Build/Tools/Unreal/ue.py`.
- Raw evidence goes to `Saved/Gasp17`.
- This report and the PR carry the summary.
- `GASP-16`'s cost measurements still apply: one Paladin per player on these maps.

## Repro (after implementation)

1. Open `Lvl_RpgGaspMantle` and start PIE: the player is the Paladin.
2. Equip armor from the inventory, fight, die and respawn.
3. Repeat as a listen server with a client that joins late.
4. Open `Lvl_RpgGaspMover` and start PIE: the UEFN Mover pawn looks as before.

## Results

### Composition (2026-10-19)

**Checked in the editor first**
- The three CMC Experiences and their pawns match the definition.
- Maps that select them, from the asset registry:
  - `RpgGaspCMCExperience`: `Lvl_RpgGaspCMC`.
  - `RpgGaspMantleExperience`: `Lvl_RpgGaspMantle`, plus `GF_Harvesting_Magic` `Lvl_HarvestPickaxe` and `GF_Dev_Sandbox` `Lvl_ResourceParity` and `Lvl_VfxGallery`.
  - `RpgInventoryStorageExperience`: `Lvl_RpgInventoryStorage`.
- The harvesting test map and the VFX gallery therefore show the Paladin too. The harvesting suite joins the regression.
- The pilot's `AddComponents` action had two entries:
  - `BPC_RpgPawnParts_GaspManny` on `BP_RpgGasp_MantleCharacter`, for client and server;
  - `BPC_RpgControllerParts_PaladinBody` on `RpgPlayerController`, server only.

**Built**
- `LAS_RpgCharacter_Paladin` (`System/Experiences/ActionSets`) with one `AddComponents` action:
  - the pawn parts component now targets `BP_RpgGasp_CMC`, so its Mantle child is covered;
  - the controller entry is unchanged.
- The CMC, Mantle and inventory storage Experiences list it after `LAS_Rpg_StandardUI`. The values survive a Blueprint compile.
- The pilot map:
  - `Lvl_RpgGaspPaladin` now runs on `RpgGaspMantleExperience`.
  - Listen-server PIE on that map: the player wears the body; five inventory equips fill all five slots and show the body and five armor parts; the client's own pawn wears the body as well.
  - `RpgGaspPaladinExperience` was then deleted after the asset registry showed no referencer.
- No pawn Blueprint, PawnData or AnimBP changed, and there is no new native code.

**Failed attempt**
- The action set was first created next to the MetaHuman parts.
- The asset manager registers `RpgExperienceActionSet` only under `/Game/SurvivalRpg/System/Experiences/ActionSets` (`Config/DefaultGame.ini`). So the Experience manager logged `Invalid Primary Asset Id RpgExperienceActionSet:LAS_RpgCharacter_Paladin` and skipped the set's bundle state. The components still arrived.
- The first regression run showed the warning and was stopped.
- The set was recreated in the scanned folder, the three Experiences were pointed at it, and the old asset was deleted. No redirector was created.

**Tests**
- `RpgGaspCMCIntegrationTests` (respawn check) and `RpgBlockLocomotionTests` (pawn readiness) now check that the live gameplay mesh plays the expected skeleton.
  - The body part swaps the asset for `SKM_Manny_Simple_GaspLook_Invis` on the same skeleton.
  - The pawn class-default checks keep the visible asset, because the pawn Blueprints are unchanged.
- The character-parts PIE tests run on `RpgGaspMantleExperience`.
- New `SurvivalRpg.Cosmetics.CharacterParts.PaladinExperienceComposition`: the three CMC Experiences compose the action set, and the Mover and Mover-Ragdoll Experiences do not.

### Validation (2026-10-19, on `f385db0d`)

**Build:** `ue.py build` passed.

**Regression, each suite on its own through `ue.py test`**

| Suite | Result |
| --- | --- |
| `SurvivalRpg.Cosmetics`, `SurvivalRpg.Equipment`, `SurvivalRpg.Inventory` | 198 / 198 (one more than `GASP-16`: the new composition test) |
| `SurvivalRpg.Animation` | 4 / 4 |
| `SurvivalRpg.GASP.CMC` | 14 / 14 |
| `SurvivalRpg.Combat.BlockLocomotion` | 16 / 16 |
| `SurvivalRpg.GASP.Mantle` | 13 / 13 |
| `SurvivalRpg.GASP.Vault` | 8 / 9, then 9 / 9 on the rerun without the MCP server |
| `SurvivalRpg.GASP.Hurdle` | crashed, then 11 / 12, then 12 / 12 (see below) |
| `SurvivalRpg.GASP.VariantComparison` | 3 / 3 |
| `SurvivalRpg.GASP.RuntimeRetarget` | 1 / 1 |
| `SurvivalRpg.Network` | 23 / 23 |
| `SurvivalRpg.Harvesting` (its test map now shows the Paladin) | 60 / 60 |
| `SurvivalRpg.GASP.Mover` (`np.ForceReconcile 0`, `t.MaxFPS 30`) | 66 / 67, then `SurvivalRpg.GASP.Mover.Ragdoll` 7 / 7 on the rerun without the MCP server |

The warnings are the known PIE network and editor noise. The `Invalid Primary Asset Id` warning of the first run is gone.

**Failures and how they were checked**
- **MCP noise (Vault, Mover-Ragdoll):**
  - `NegativeAngledVaultAlignsWithoutTurningTheView` and `CancelledGetupCannotCompleteTheNextRagdollEpisode` failed only on `LogModelContextProtocol: Call to unknown method "server/discover"`.
  - That is an outside MCP client calling the test editor's own MCP server on port 8000.
  - Rerun with `--ue-arg=-ini:EditorPerProjectUserSettings:[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]:bAutoStartServer=False`, both suites passed.
- **Hurdle teardown crash:**
  - The first Hurdle run crashed after all steps of `WalkingHurdleUsesMappedAnalogInputAndBackFloor` had passed.
  - The crash is `Assertion failed: IsInGameThread()` in `FSceneViewport::Destroy`, released from `FSlateRHIRenderer::DrawWindow_RenderThread` while a client window closed.
  - It is the same family as the `GASP-16` teardown crash in `SLevelViewport::EndPlayInEditorSession`. It did not recur in the next two Hurdle runs.
- **Hurdle rotation check:**
  - `CancelledHurdleReleasesMovementCollisionAndRotation` failed once, at `RpgHurdleIntegrationTests.cpp:409`. After the server cancels the hurdle, the owner's yaw must be within 5° of the approach yaw.
  - Comparison of that test alone, without the MCP server:
    - 5 / 5 passed with the Paladin;
    - 5 / 5 passed with the Mantle Experience restored to `master` (no Paladin).
  - Over all runs the test passed 7 of 8 times with the Paladin. Its history shows no earlier failure at this line.
  - It is recorded as a rare timing case, not as a defect of the Paladin.

**PIE, listen server with two players, scripted probe** (`Saved/Gasp17`)
- `Lvl_RpgGaspMantle` and `Lvl_RpgGaspCMC`: on the server and the client, every pawn has `BPC_RpgPawnParts_GaspManny`, wears `B_RpgPart_MetaHumanPaladin` and plays `SKM_Manny_Simple_GaspLook_Invis`. This covers authority, autonomous proxy and simulated proxy.
- `Lvl_RpgGaspMover`: no pawn has a parts component, and all play `SKM_UEFN_Mannequin`.
- Way back:
  - The action set was removed from `RpgGaspMantleExperience` in memory only. In PIE every pawn on every role was the plain `SKM_Manny_Simple_GaspLook` again, with no parts component.
  - The package was then reloaded from disk without saving. The action set was back and nothing was dirty.
- Late join, death, respawn, inventory equips and the dedicated server are covered by the character-parts PIE tests on `RpgGaspMantleExperience`, in the Cosmetics suite above.

**Limits**
- The Hurdle rotation timing case and the engine teardown crash above.
- `Lvl_HarvestPickaxe`, `Lvl_ResourceParity` and `Lvl_VfxGallery` use the Mantle Experience and now show the Paladin. That includes the VFX gallery other tasks use for their captures.
- The `GASP-16` limits still apply: the ragdoll trail, the cost per Paladin and the render-thread visibility wait.
- Open: the user check on a production map.

