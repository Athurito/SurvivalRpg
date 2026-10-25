# Asset library migration

Purchased content lives in the asset library listed under "Local environment" in `AGENTS.md`. Read this before bringing sounds, effects, animations, or characters from it into SurvivalRpg. Correct this file in the same PR when a step changes.

## Contents

- [Rules](#rules)
- [Reading and copying assets](#reading-and-copying-assets)
- [Animations](#animations)
- [Sounds](#sounds)
- [MetaHumans](#metahumans)

## Rules

- The asset projects are the full library. Migrate only the curated subset a roadmap step uses, and add a `docs/third-party-notices.md` entry per pack.
- Licences: packs that Epic Games publishes, such as the Niagara examples, are cleared by the project owner. The owner also cleared the sound packs in `Assets_Sounds` for use in the game. Third-party seller packs still need their licence noted.
- The SurvivalRpg MCP configuration knows only port 8000. Driving an asset project's editor (ports 8001-8004) needs a local MCP client entry or a small HTTP client. The asset projects have no free Python tool; launch one with `-ExecutePythonScript=<boot script>` that calls `unreal.EditorPythonScripting.set_keep_python_script_alive(True)` and registers a scratch toolset.

## Reading and copying assets

- Read an asset project headlessly instead of opening another editor: `UnrealEditor-Cmd.exe <asset project>.uproject -run=pythonscript -script=<file> -unattended -nullrhi` exports textures (PNG, or EXR for 16-bit float sources) and materials (T3D), and walks dependency closures with `AssetRegistry.get_dependencies`.
- Migrate by copying `.uasset` files to the same `/Game` path in SurvivalRpg, then move them into the target folder with `AssetTools.rename_assets`; this leaves no redirectors.
- Check the output classes afterwards and replace legacy types as unreal-gasp-expert describes.

## Animations

- Some packs ship UE4 and UE5 variants: in Sword_and_Shield, Sword_Animations, and Mage the UE5 set is `Sequence2`; Archer and Spear are UE5 only; 505_Survival_Mocap keeps UE5 under `Animations_UE5`.
- Migrate only UE5 variants and re-point them to SurvivalRpg's `SK_Mannequin`, so demo skeletons and meshes stay behind.

## Sounds

- Pick sounds per needed sound event, not by browsing packs.
- Copy candidates into a temporary `/Game` folder and read their `duration`. For impacts, export them with `AssetExportTask` and `SoundExporterWAV` and measure the attack, the share of energy below 150 Hz, and the spectral centroid: booms have a fast attack and more than 85 % low energy, cracks reach full scale within 20 ms.
- Move the chosen waves into `Content/SurvivalRpg/Audio/Sounds/<Topic>/SW_*` with the `SFX_Effects` sound class and `ATT_FX_World` attenuation.

## MetaHumans

Proven for the Paladin body (`docs/gasp-character-armor-pilot.md`); repeat it for further player variants.

1. Build in `Assets_3DAssets`, where the `MetaHumanCharacter` plugin is enabled: `MetaHumanCharacterEditorSubsystem.build_meta_human` with pipeline OPTIMIZED and quality MEDIUM. "Cannot break link" control-rig errors during the build are harmless.
2. Before migrating, in the asset library:
   - Clear the face mesh's post-process ABP, its `DNAAssetUserData`, and its physics asset.
   - Clear `default_animating_rig` on body and face.
   - Give the body a copy of `ABP_Body_PostProcess` with the ControlRig node deleted and Enable Body Correctives on, so only RigLogic runs.
   - Otherwise the migration pulls in the face and body control rigs, whose evaluation crashes the SurvivalRpg editor during thumbnails.
3. Migrate the body mesh, the face mesh, and everything under `Grooms`, then move them to `/Game/SurvivalRpg/Characters/MetaHumans` (`Common` shared, `MH_<Name>` per character) and resave. Watch for overwrite prompts when the `Common` paths migrate again.
4. At runtime each character is a data-only child of `B_RpgPart_MetaHumanBody`. The face follows by Leader Pose. The grooms need a `LODSync` component (body and face drive; grooms passive with mapping 3/5/7 and 3 LODs) and the attachment `FACIAL_C_FacialRoot`; without it the Optimized grooms are invisible in PIE although the editor viewport shows them.
5. MetaHuman skin instances are chained (body, baked head instance, unified skin material). Put only the root on `M_MetaHumanSkin_CoverMask` and restore its static switches after the reparent, as [Niagara and material pitfalls](mcp-niagara-pitfalls.md#materials) describes.
6. Cap textures at 2K and record the package in the asset manifest under `docs/assets`.
