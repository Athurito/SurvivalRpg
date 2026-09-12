# GASP contextual mantle integration

This Experience extends the accepted CMC integration with the original GASP mantle query and montage selection on the existing RPG gameplay mesh. Mantle comes first; Vault, Hurdle, other visible skeletons and Mover remain subsequent work. The baseline and accepted CMC Experience retain their material and Foley setup.

## Input and content ownership

Space attempts traversal on Started and requests one ordinary jump if no usable mantle is found. Held input retries traversal, including during that jump. GASP's Down trigger reports Triggered while held; the existing RPG Pressed/Released action reports Ongoing. Both events feed the same retry, gated on a nonzero input value so releasing Space does not start traversal. The shared input assets are unchanged. Active traversal consumes repeated presses. An occupied DefaultSlot also consumes jump in the traversal Experience, matching the source guard against jumping through another montage.

AC_RpgGasp_TraversalQuery copies Shared/AC_TraversalLogic. Its original 228-node TryTraversalAction retains geometry, spline ledges, chooser and pose history. Only the two terminal sample execution calls are removed; the cached result returns immediately. Sample execution/RPC graphs are removed from this copy. The wrapper clears the previous result before each query and requires fresh collider, montage, action and successful failure flags.

The wrapper reproduces source CMC inputs: capsule radius 30 cm, half-height 60 cm and forward reach 75–350 cm mapped from forward velocity 0–500 cm/s. Falling uses reach 75 cm, half-height 86 cm and end offset Z+50 cm. The pawn supplies its gameplay capsule, mesh, MotionWarping component and CMC snapshot through the existing GASP pawn interface. The accepted AnimBP supplies interaction transforms and pose history.

The owned CMC chooser references fifteen owned mantle/climb/catch montage copies. Animation sequences remain shared project-local foundation assets. Original notifies, warp windows, pose-search ranges, root-motion settings, skeleton and DefaultSlot are retained. Foundation and import files stay unchanged.

Their PoseSearchBranchIn notifies reference the owned PSD_RpgGasp_Traversal database. Unreal synchronizes database membership when montage notifies change; keeping these references on Shared/PSD_Traversal would enroll the copies in the shared database. The owned database preserves the source schema and non-selected rows, with the fifteen selected rows synchronized to their new notify identities and original sampling ranges.

## Runtime authority and animation handoff

URpgTraversalQueryComponent defines the Blueprint query/result contract and designer-owned animation eligibility rows. URpgGameplayAbility_Mantle owns GAS prediction, commitment, server validation and cleanup. The concrete GA_RpgGasp_Mantle Blueprint starts the selected montage with PlayMontageAndWait.

The predicted proposal contains only collider identity, approved montage and bounded pose-search entry time, correlated with the GAS activation prediction key. The server reruns the query and physical face, support and capsule-route checks; it does not accept client ledge or landing coordinates. Eligibility allows bounded sampling differences at speed/airborne-height boundaries. Costs and effects are followed by revalidation before movement.

The owning player executes the fresh result sampled during activation through that same native validation/commit path. It does not run an additional stateful pose query after sending its proposal: that redundant query could reject a valid immediate retry after server rejection. The remote authority still samples independently.

Landing derives from the selected clip’s final FrontLedge warp and root motion through its source action handoff. The 1 m running montage forces blend-out at 0.732103 seconds; its later running tail is not the mantle landing. HandoffTime records that source boundary in the eligibility row. Clips with conditional movement-input exits use their natural endpoint for conservative preflight. Original notify conditions and blend profiles remain active. The interrupted montage callback releases traversal, as in GASP.

The ability owns only its temporary collision exception and FrontLedge warp. Movable, non-simulating source cubes are supported, but their transform and support must remain unchanged during the action. The CMC collision lease and MotionWarping targets replicate to simulated proxies; the owner predicts locally. Server movement correction remains enabled. Blocked landings, cancellation, death and collider destruction release state and recover a checked capsule position. Death or another movement owner retains its newer movement mode.

The warp includes GASP’s 0.5 cm ledge offset plus the existing mesh/capsule separation (CapsuleHalfHeight + BaseTranslationOffset.Z). The gameplay mesh transform and montage-only root-motion policy stay unchanged.

Unreal MCP authors and validates the assets. CopyAnimationNotifies is an editor-only helper for copying exact event records and instanced notify objects between identical animation timelines; Python cannot write the protected Notifies array. It changes only the target and does not save automatically.

## Test map

Open /Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle. WorldSettings selects RpgGaspMantleExperience and the isolated test GameMode, with disk persistence disabled. PawnData, ability set, character, query, chooser and montages live under /Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal.

The three platforms inherit the original LevelBlock_Traversable, using its cube, four spline ledges and traversal collision preset. Actor scale (4,4,1) gives 400 × 400 × 100 cm platforms. The corner-based pivots are accounted for; PlayerStarts remain in front of the same lanes.

The floor uses original LevelBlock and M_Grid with the GASP floor color. Original LevelVisuals supplies a matched sun, skylight, cubemap, fog and manual exposure configuration.

Run toward a platform and press Space, approach away from its center, or press Space while touching it. Hold Space well before reaching it to exercise an ordinary jump followed by a traversal retry on the same held key. Check both a single-player listen-server host and a remote client.

## Validation

The previous standing-only fixture teleported the pawn to a narrow valid entry and missed the real input problem. The replacement tests start the saved map with its GameMode, Experience and PlayerStarts and drive mapped W/Space key events, without candidate-based placement or forced velocity.

Focused cases cover running, lateral approach, collision contact, held-input retry and single-player listen-server host input. The network fixture retains server rejection, late join, cancellation, death/respawn, latency and collider destruction using source cubes and dynamic montage selection. Save isolation installs before GameMode initialization. CMC and combat regressions cover the accepted compositions.

Validated on Unreal 5.8.2: the final SurvivalRpgEditor Win64 Development build passed, and the four owned Blueprints compiled with warnings treated as errors. All ten final automation tests passed: six authored-input/composition cases, the complete Mantle network lifecycle case, and three CMC/combat regressions. Gameplay coverage uses the 1 m platforms; the higher/cliff montage copies have asset parity coverage but still need dedicated gameplay cases. No packaged-build or dedicated-server validation is implied.

The fresh asset audit passed for 22 roots, 1,602 dependency packages, all fifteen montage pairs, the original executable query graph and chooser, and the owned database's exact BranchIn identities/ranges. Hash preservation checked all 7,579 existing content/plugin files and seven saves; only the five authorized pilot assets changed. No save was added or changed. Runs also reported voice-interface and temporary-world NetGUID warnings, plus a tick-setting warning on the unchanged respawn widget.

Evidence is in Saved/GaspTraversalFix20260912: final-input-results.json, final-network-results.json, final-regression-results.json, build.log and fix-asset-audit.json. Source snapshots remain in Saved/GaspAssetFoundation20260910. The full temporary PoseSearch recovery asset is retained under Saved and has no runtime reference.
