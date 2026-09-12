# GASP grounded Vault integration

This slice extends the existing CMC traversal Experience with the three original grounded Vault variants. The gameplay mesh remains the UEFN mannequin. Hurdle, additional visible skeletons and Mover remain separate integration steps.

## Source behavior and ownership

GASP's action values are 1 Hurdle, 2 Vault and 3 Mantle. Its grounded Vault chooser requires front and back ledges, no detected back floor, obstacle height up to 125 cm and depth up to 59 cm. A thin obstacle with a nearby floor behind it normally selects Hurdle. This distinction is retained; ordinary same-floor barriers are not relabeled as Vault.

The three copies live under `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/Animations/Vault`. Their original standing, walking and running montages retain sequences, skeleton, root motion, DefaultSlot, warp windows and notifies. Only their PoseSearchBranchIn database reference changes to the existing owned `PSD_RpgGasp_Traversal`. The existing chooser now selects these copies. The owned database holds the eighteen selected Mantle/Vault rows with the correct identities of their copied BranchIn notifies; unrelated rows remain intact.

Each copy keeps its source filename from `/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Traversal/Vault`:

| Montage filename | Speed row (cm/s) | Pose-search entry range (s) | Source handoff (s) |
| --- | --- | --- | --- |
| AM_M_Neutral_Traversal_Vault_1_0_stand_F_Lfoot | 0–100 | 0–0.08 | 0.7294442654 |
| AM_M_Neutral_Traversal_Vault_1_0_walk_F_Rfoot | 100–250 | 0–0.51 | 1.3141385317 |
| AM_M_Neutral_Traversal_Vault_1_0_run_F_Lfoot | 250+ | 0–0.61 | 1.3247057199 |

The native running row uses a finite 100,000 cm/s upper bound, following the existing eligibility contract. The existing bounded client/server sampling tolerance applies at gait boundaries.

`AllowedVaultAnimations` on the existing query component records the three designer-owned eligibility rows. It defaults to empty. The historical Mantle native class and concrete Blueprint Ability retain their serialized names and serve both supported actions, avoiding another input binding or Experience. The proposal still contains only collider, montage and entry time; the authority independently queries and validates geometry.

All three montages use the original FrontLedge warp. Standing also uses the source translation-only BackLedge warp, targeting the actual back edge without the front target's 0.5 cm hand offset. Both targets account for the existing gameplay mesh/capsule feet separation. The source BackFloor target and distance-curve calculation belong to Hurdle and are not used for Vault.

Exit validation derives capsule feet from the last applicable warp and remaining root motion to the source handoff. Physical front/back faces, depth, clear capsule route and exit are checked before commitment. The accepted collider must remain unchanged throughout traversal. Successful completion releases owned targets and the existing collision/rotation lease, preserves momentum, and switches to normal CMC falling. Collision, gravity and landing resume their ordinary behavior. Cancellation, recovery, death and a newer movement owner retain the existing lifecycle protections. Mantle still requires its supported landing on the traversed collider.

The source handoffs are approximately 0.729444 s standing, 1.314139 s walking and 1.324706 s running. Original forced blend-out notifies perform the transition; montage speed and camera damping are unchanged.

An owning client's normal GAS completion message can arrive before the server's final movement/pose update. For an active Vault only, the authority defers that early normal completion while its exact montage instance is still playing before its own source handoff. Its existing montage task then ends through the authoritative notify. This prevents prematurely stopping the server montage and treating the artificial interruption as a reason to discard momentum. The client supplies no completion position or trusted timestamp; geometry checks, cancellation and the existing duration timeout remain active.

## Test map

Open `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle` with its saved WorldSettings Experience. The three existing 1 m Mantle platforms and PlayerStarts remain in place. The floor boundary extends north to contain two new Vault lanes, retaining original GASP LevelBlock materials and LevelVisuals lighting/exposure.

Each new lane has five 20 cm steps, a platform 1 m above the main floor, and a narrow prepared barrier at its far edge. The barrier is another 1 m above the platform. One barrier is 50 cm deep and the other 30 cm deep; both use the original traversable cube and ledge splines. The lower floor behind them produces the source Vault query conditions. Walk up the stairs, face the barrier and press Space from standing or while approaching. Continue toward the lower floor after crossing. Test angled approaches and both a listen-server host and remote client.

The obstacle actors carry `Rpg.TraversalTest.Vault`; approaches carry `Rpg.TraversalTest.Vault.Approach`. Existing actor class names containing Mantle are retained for compatibility.

## Validation

Execution evidence is recorded under `Saved/GaspVault20260912`. The asset audit compares all three Vault montage copies against their source, the unchanged executable query and chooser logic, exact BranchIn membership and preserved database records, and the project-local dependency closure. Preservation hashes cover all pre-existing tracked Content/Plugins files and save files.

The new authored-map fixture starts at the saved PlayerStarts and drives mapped movement/Space input, including the stairs. The walking-speed case supplies the existing gamepad move axis and verifies the actual 100–250 cm/s speed and walking montage on all roles. Assertions distinguish an active Vault montage, rear-edge crossing, released traversal state while falling and landing on the lower floor beyond the barrier. All successful variants require forward momentum at the owner and server's ability-end delegates. Standing additionally checks replicated BackLedge ownership. No candidate-based teleport or forced approach velocity is used.

Both authored traversal fixtures temporarily ignore physical mouse/stick look on their own local controller. The balanced scope leaves direct SetControlRotation calls observable, so ability/camera bugs still fail the view-preservation assertions. This removes external input interference observed during a previous angle test; the approach, landing and angle assertions are unchanged.

The first cold-editor attempt encountered the source chooser's asynchronous PoseSearch index build and skipped its initial search. That attempt is retained in the evidence; gameplay checks ran after the index completed.

The final SurvivalRpgEditor Win64 Development build passed on Unreal 5.8.2. All 26 tests passed in one 196.17-second run: eight Vault scenarios and the eighteen existing Mantle, CMC, combat and camera cases. The shared network lifecycle regression includes late join, latency, cancellation, death/respawn and collider destruction. Vault-specific gameplay covers standing, walking, running, both angled approaches, listen host, blocked authority exit and server cancellation. These are editor PIE tests; no packaged-build or dedicated-server validation is implied.

The final asset audit passed for 25 roots and 1,606 dependency packages, including source query/chooser preservation, all three Vault montage copies and eighteen owned BranchIn records. Hashes checked all 7,597 pre-existing Content/Plugins files: only the query, owned chooser/database and test map changed; the other 7,593 files and all seven saves remained unchanged. Three new montage packages are the only added content. Source animations, foundation/import assets and baseline composition remain unchanged.

`Saved/GaspVault20260912/validation-summary.json`, `all-final-results.json`, `build-final.log`, `final-editor.log`, `vault-asset-audit.json` and `preservation-final.json` retain the final evidence. Earlier failures are preserved separately. Existing voice-interface, temporary-world NetGUID and respawn-widget tick warnings remain visible in the results.
