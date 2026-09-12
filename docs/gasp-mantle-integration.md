# GASP contextual mantle integration

This Experience extends the accepted CMC integration with the original GASP mantle query and montage selection on the existing RPG gameplay mesh. The subsequent [grounded Vault extension](gasp-vault-integration.md) uses this same composition. Hurdle, other visible skeletons and Mover remain subsequent work. The baseline and accepted CMC Experience retain their material and Foley setup.

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

The ability owns temporary traversal movement, its collision exception and FrontLedge warp. Movable, non-simulating source cubes are supported, but their transform and support must remain unchanged during the action. The CMC collision lease and MotionWarping targets replicate to simulated proxies; the owner predicts locally. Server movement correction remains enabled. Blocked landings, cancellation, death and collider destruction release state and recover a checked capsule position. Death or another movement owner retains its newer movement mode.

The approach retains CMC velocity and acceleration when root motion takes over. A normal montage handoff also retains momentum when the same montage has finished its final warp, the capsule is clear, and the traversed collider provides walkable support. The concrete Ability routes the source notify's OnInterrupted callback to EndAbility; OnCancelled retains CancelAbility. Gameplay cancellation is recorded before GAS montage callbacks can report an ordinary interruption and remains a cancellation for GAS observers; cancellation, recovery and unsafe exits retain the previous stop behavior. Playback rate, source notifies and blend profiles remain unchanged.

During validated mantle, the existing movement lease also gives body rotation to root motion. ARpgCharacter gates controller-driven FaceRotation and its CMC gates PhysicsRotation until that lease releases. Original GASP disables pawn controller yaw and physics rotation during animation root motion; the RPG pawn normally enables controller yaw, which otherwise repeatedly overwrites the authored Slerp turn toward the FrontLedge normal during an angled approach. The scoped native guards preserve ordinary RPG rotation settings and ControlRotation. The existing predicted/server/simulated lease supplies the lifetime and cleanup without another replicated state or mutable flag restoration. Motion Warping retains the source rotation windows and performs the turn; no explicit snap to the ledge is added.

## Camera presentation

The user's approved feel keeps GASP animation tempo and softens camera and movement transitions. CM_RpgGasp_Traversal inherits the existing RPG third-person camera and is selected only by the Mantle PawnData. Its designer-owned PositionDampingFactor is 20 rad/s, matching the uniform position damping in GASP's free-camera rigs. The existing RPG camera mode provides the reusable critical-spring mechanism. It follows the moving pivot while leaving view/control rotation immediate and applying camera collision afterward.

The shared camera defaults to zero damping. Activation, target changes, explicit interpolation resets and target displacements greater than the configured 500 cm reset distance discard camera history. This is cosmetic state; it does not drive character movement, replication or montage speed.

The warp includes GASP’s 0.5 cm ledge offset plus the existing mesh/capsule separation (CapsuleHalfHeight + BaseTranslationOffset.Z). The gameplay mesh transform and montage-only root-motion policy stay unchanged.

Unreal MCP authors and validates the assets. CopyAnimationNotifies is an editor-only helper for copying exact event records and instanced notify objects between identical animation timelines; Python cannot write the protected Notifies array. It changes only the target and does not save automatically.

## Test map

Open /Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle. WorldSettings selects RpgGaspMantleExperience and the isolated test GameMode, with disk persistence disabled. PawnData, ability set, character, query, chooser and montages live under /Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal.

The three platforms inherit the original LevelBlock_Traversable, using its cube, four spline ledges and traversal collision preset. Actor scale (4,4,1) gives 400 × 400 × 100 cm platforms. The corner-based pivots are accounted for; PlayerStarts remain in front of the same lanes.

The floor uses original LevelBlock and M_Grid with the GASP floor color. Original LevelVisuals supplies a matched sun, skylight, cubemap, fog and manual exposure configuration.

This approved setup is the starting point for further prototype/test maps, as recorded in AGENTS.md. It is a readable test presentation, not the final dark-fantasy art direction.

Run toward a platform and press Space, approach away from its center, or press Space while touching it. Hold Space well before reaching it to exercise an ordinary jump followed by a traversal retry on the same held key. Check both a single-player listen-server host and a remote client.

## Validation

The previous standing-only fixture teleported the pawn to a narrow valid entry and missed the real input problem. The replacement tests start the saved map with its GameMode, Experience and PlayerStarts and drive mapped W/Space key events, without candidate-based placement or forced velocity.

Focused cases cover running, lateral approach, collision contact, held-input retry and single-player listen-server host input. The network fixture retains server rejection, late join, cancellation, death/respawn, latency and collider destruction using source cubes and dynamic montage selection. Save isolation installs before GameMode initialization. CMC and combat regressions cover the accepted compositions.

Additional handoff cases hold W through the action on a remote owner and listen-server host. They sample velocity and acceleration directly from the ability-ended delegate, before a later movement tick could conceal a full stop, then require continued grounded forward movement. Pure camera tests cover direct-follow compatibility, resets, convergence, frame-rate agreement, a hitch and pause/resume.

Validated on Unreal 5.8.2: the final SurvivalRpgEditor Win64 Development build passed. The preceding camera/Ability authoring compiled with warnings treated as errors. Eighteen automation tests passed: ten authored-input scenarios, three camera tests, two asset-composition checks, the complete Mantle network lifecycle, and two CMC/combat gameplay regressions. Gameplay coverage uses the 1 m platforms; the higher/cliff montage copies have asset parity coverage but still need dedicated gameplay cases. No packaged-build or dedicated-server validation is implied.

The angled-input regression runs remote approaches at +35 and -35 degrees with a second client observing, plus a +35-degree listen-server host. It retains the player's view heading, samples the actual FrontLedge target during the final warp and through the remaining active action, then checks landing, lease release and ordinary facing. It does not resample the stateful query for diagnostics. All three tests failed against runtime 41422a1d: the body reached the ledge normal and then reverted to controller yaw, producing 35 degrees of post-warp error on every role. The identical tests passed with scoped rotation ownership, recording 0.00 degrees of post-warp error and unchanged control yaw. Cancellation coverage also verifies that normal controller-facing resumes after release.

The standing network fixture now synchronizes the server and owning controller's view heading with its prepared entry and waits for alignment. A pawn teleport alone did not reset the owner's view. An earlier run landed at the edge after respawn; the final run passed immediately after all seven input scenarios, with the original landing geometry assertions retained. Transform diagnostics record entry facing and warp targets without resampling the stateful query.

The original asset audit passed for 22 roots, 1,602 dependency packages, all fifteen montage pairs, the original executable query graph and chooser, and the owned database's exact BranchIn identities/ranges. The transition follow-up checked hashes of 7,596 existing content/plugin files and seven saves: only the Mantle PawnData and Ability changed, with one new camera asset. Original animations, import content, the accepted baseline and saves remain unchanged. Runs also reported voice-interface and temporary-world NetGUID warnings, plus a tick-setting warning on the unchanged respawn widget.

Transition evidence is in Saved/GaspMantleFeel20260912: input-final-results.json, input-camera-final-results.json, network-final-results.json, regression-final-results.json, build-final.log and preservation-final.json. The initial handoff failure and network reproduction are retained there. Original migration evidence remains in Saved/GaspTraversalFix20260912/fix-asset-audit.json and Saved/GaspAssetFoundation20260910. The full temporary PoseSearch recovery asset is retained under Saved and has no runtime reference.

Alignment evidence is in Saved/GaspMantleAlignment20260912: before-results.json and before-editor.log retain the previous-runtime failure; validation-summary.json records all eighteen final tests and the measured alignment comparison. build-final.log, final-editor.log and the individual final result files retain execution evidence. Fresh hashes verified all 7,597 content/plugin files and seven saves unchanged; this follow-up changes native rotation guards, tests and documentation only.
