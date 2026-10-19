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
   - Create `LAS_RpgCharacter_Paladin` next to the Paladin parts. It holds the pilot's `AddComponents` entries:
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
