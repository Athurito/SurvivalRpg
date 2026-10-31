# GASP CMC traversal: Mantle, Vault and Hurdle

The CMC traversal Experience gained its three grounded actions in three slices: contextual Mantle, then Vault, then Hurdle. Each section below is the report of one slice; they share the traversal Ability, query, chooser and test map `Lvl_RpgGaspMantle`.

## Mantle

This Experience extends the accepted CMC integration with the original GASP mantle query and montage selection on the existing RPG gameplay mesh. The subsequent [Vault](#vault) and [Hurdle](#hurdle) slices use this same composition. Hurdle, other visible skeletons and Mover remain subsequent work. The baseline and accepted CMC Experience retain their material and Foley setup.

### Input and content ownership

Space attempts traversal on Started and requests one ordinary jump if no usable mantle is found. Held input retries traversal, including during that jump. GASP's Down trigger reports Triggered while held; the existing RPG Pressed/Released action reports Ongoing. Both events feed the same retry, gated on a nonzero input value so releasing Space does not start traversal. The shared input assets are unchanged. Active traversal consumes repeated presses. An occupied DefaultSlot also consumes jump in the traversal Experience, matching the source guard against jumping through another montage.

AC_RpgGasp_TraversalQuery copies Shared/AC_TraversalLogic. Its original 228-node TryTraversalAction retains geometry, spline ledges, chooser and pose history. Only the two terminal sample execution calls are removed; the cached result returns immediately. Sample execution/RPC graphs are removed from this copy. The wrapper clears the previous result before each query and requires fresh collider, montage, action and successful failure flags.

The wrapper reproduces source CMC inputs: capsule radius 30 cm, half-height 60 cm and forward reach 75–350 cm mapped from forward velocity 0–500 cm/s. Falling uses reach 75 cm, half-height 86 cm and end offset Z+50 cm. The pawn supplies its gameplay capsule, mesh, MotionWarping component and CMC snapshot through the existing GASP pawn interface. The accepted AnimBP supplies interaction transforms and pose history.

The owned CMC chooser references fifteen owned mantle/climb/catch montage copies. Animation sequences remain shared project-local foundation assets. Original notifies, warp windows, pose-search ranges, root-motion settings, skeleton and DefaultSlot are retained. Foundation and import files stay unchanged.

Their PoseSearchBranchIn notifies reference the owned PSD_RpgGasp_Traversal database. Unreal synchronizes database membership when montage notifies change; keeping these references on Shared/PSD_Traversal would enroll the copies in the shared database. The owned database preserves the source schema and non-selected rows, with the fifteen selected rows synchronized to their new notify identities and original sampling ranges.

### Runtime authority and animation handoff

URpgTraversalQueryComponent defines the Blueprint query/result contract and designer-owned animation eligibility rows. URpgGameplayAbility_Mantle owns GAS prediction, commitment, server validation and cleanup. The concrete GA_RpgGasp_Mantle Blueprint starts the selected montage with PlayMontageAndWait.

The predicted proposal contains only collider identity, approved montage and bounded pose-search entry time, correlated with the GAS activation prediction key. The server reruns the query and physical face, support and capsule-route checks; it does not accept client ledge or landing coordinates. Eligibility allows bounded sampling differences at speed/airborne-height boundaries. Costs and effects are followed by revalidation before movement.

The owning player executes the fresh result sampled during activation through that same native validation/commit path. It does not run an additional stateful pose query after sending its proposal: that redundant query could reject a valid immediate retry after server rejection. The remote authority still samples independently.

Landing derives from the selected clip’s final FrontLedge warp and root motion through its source action handoff. The 1 m running montage forces blend-out at 0.732103 seconds; its later running tail is not the mantle landing. HandoffTime records that source boundary in the eligibility row. Clips with conditional movement-input exits use their natural endpoint for conservative preflight. Original notify conditions and blend profiles remain active. The interrupted montage callback releases traversal, as in GASP.

The ability owns temporary traversal movement, its collision exception and FrontLedge warp. Movable, non-simulating source cubes are supported, but their transform and support must remain unchanged during the action. The CMC collision lease and MotionWarping targets replicate to simulated proxies; the owner predicts locally. Server movement correction remains enabled. Blocked landings, cancellation, death and collider destruction release state and recover a checked capsule position. Death or another movement owner retains its newer movement mode.

The approach retains CMC velocity and acceleration when root motion takes over. A normal montage handoff also retains momentum when the same montage has finished its final warp, the capsule is clear, and the traversed collider provides walkable support. The concrete Ability routes the source notify's OnInterrupted callback to EndAbility; OnCancelled retains CancelAbility. Gameplay cancellation is recorded before GAS montage callbacks can report an ordinary interruption and remains a cancellation for GAS observers; cancellation, recovery and unsafe exits retain the previous stop behavior. Playback rate, source notifies and blend profiles remain unchanged.

During validated mantle, the existing movement lease also gives body rotation to root motion. ARpgCharacter gates controller-driven FaceRotation and its CMC gates PhysicsRotation until that lease releases. Original GASP disables pawn controller yaw and physics rotation during animation root motion; the RPG pawn normally enables controller yaw, which otherwise repeatedly overwrites the authored Slerp turn toward the FrontLedge normal during an angled approach. The scoped native guards preserve ordinary RPG rotation settings and ControlRotation. The existing predicted/server/simulated lease supplies the lifetime and cleanup without another replicated state or mutable flag restoration. Motion Warping retains the source rotation windows and performs the turn; no explicit snap to the ledge is added.

### Camera presentation

The user's approved feel keeps GASP animation tempo and softens camera and movement transitions. CM_RpgGasp_Traversal inherits the existing RPG third-person camera and is selected only by the Mantle PawnData. Its designer-owned PositionDampingFactor is 20 rad/s, matching the uniform position damping in GASP's free-camera rigs. The existing RPG camera mode provides the reusable critical-spring mechanism. It follows the moving pivot while leaving view/control rotation immediate and applying camera collision afterward.

The shared camera defaults to zero damping. Activation, target changes, explicit interpolation resets and target displacements greater than the configured 500 cm reset distance discard camera history. This is cosmetic state; it does not drive character movement, replication or montage speed.

The warp includes GASP’s 0.5 cm ledge offset plus the existing mesh/capsule separation (CapsuleHalfHeight + BaseTranslationOffset.Z). The gameplay mesh transform and montage-only root-motion policy stay unchanged.

Unreal MCP authors and validates the assets. CopyAnimationNotifies is an editor-only helper for copying exact event records and instanced notify objects between identical animation timelines; Python cannot write the protected Notifies array. It changes only the target and does not save automatically.

### Test map

Open /Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle. WorldSettings selects RpgGaspMantleExperience and the isolated test GameMode, with disk persistence disabled. PawnData, ability set, character, query, chooser and montages live under /Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal.

The three platforms inherit the original LevelBlock_Traversable, using its cube, four spline ledges and traversal collision preset. Actor scale (4,4,1) gives 400 × 400 × 100 cm platforms. The corner-based pivots are accounted for; PlayerStarts remain in front of the same lanes.

The floor uses original LevelBlock and M_Grid with the GASP floor color. Original LevelVisuals supplies a matched sun, skylight, cubemap, fog and manual exposure configuration.

This approved setup is the starting point for further prototype/test maps, as recorded in AGENTS.md. It is a readable test presentation, not the final dark-fantasy art direction.

Run toward a platform and press Space, approach away from its center, or press Space while touching it. Hold Space well before reaching it to exercise an ordinary jump followed by a traversal retry on the same held key. Check both a single-player listen-server host and a remote client.

### Validation

The previous standing-only fixture teleported the pawn to a narrow valid entry and missed the real input problem. The replacement tests start the saved map with its GameMode, Experience and PlayerStarts and drive mapped W/Space key events, without candidate-based placement or forced velocity.

Focused cases cover running, lateral approach, collision contact, held-input retry and single-player listen-server host input. The network fixture retains server rejection, late join, cancellation, death/respawn, latency and collider destruction using source cubes and dynamic montage selection. Save isolation installs before GameMode initialization. CMC and combat regressions cover the accepted compositions.

Additional handoff cases hold W through the action on a remote owner and listen-server host. They sample velocity and acceleration directly from the ability-ended delegate, before a later movement tick could conceal a full stop, then require continued grounded forward movement. Pure camera tests cover direct-follow compatibility, resets, convergence, frame-rate agreement, a hitch and pause/resume.

Validated on Unreal 5.8.2: the final SurvivalRpgEditor Win64 Development build passed. The preceding camera/Ability authoring compiled with warnings treated as errors. Eighteen automation tests passed: ten authored-input scenarios, three camera tests, two asset-composition checks, the complete Mantle network lifecycle, and two CMC/combat gameplay regressions. Gameplay coverage uses the 1 m platforms; the higher/cliff montage copies have asset parity coverage but still need dedicated gameplay cases. No packaged-build or dedicated-server validation is implied.

The angled-input regression runs remote approaches at +35 and -35 degrees with a second client observing, plus a +35-degree listen-server host. It retains the player's view heading, samples the actual FrontLedge target during the final warp and through the remaining active action, then checks landing, lease release and ordinary facing. It does not resample the stateful query for diagnostics. All three tests failed against runtime 41422a1d: the body reached the ledge normal and then reverted to controller yaw, producing 35 degrees of post-warp error on every role. The identical tests passed with scoped rotation ownership, recording 0.00 degrees of post-warp error and unchanged control yaw. Cancellation coverage also verifies that normal controller-facing resumes after release.

The standing network fixture now synchronizes the server and owning controller's view heading with its prepared entry and waits for alignment. A pawn teleport alone did not reset the owner's view. An earlier run landed at the edge after respawn; the final run passed immediately after all seven input scenarios, with the original landing geometry assertions retained. Transform diagnostics record entry facing and warp targets without resampling the stateful query.

The original asset audit passed for 22 roots, 1,602 dependency packages, all fifteen montage pairs, the original executable query graph and chooser, and the owned database's exact BranchIn identities/ranges. The transition follow-up checked hashes of 7,596 existing content/plugin files and seven saves: only the Mantle PawnData and Ability changed, with one new camera asset. Original animations, import content, the accepted baseline and saves remain unchanged. Runs also reported voice-interface and temporary-world NetGUID warnings, plus a tick-setting warning on the unchanged respawn widget.

Transition evidence is in Saved/GaspMantleFeel20260912: input-final-results.json, input-camera-final-results.json, network-final-results.json, regression-final-results.json, build-final.log and preservation-final.json. The initial handoff failure and network reproduction are retained there. Original migration evidence remains in Saved/GaspTraversalFix20260912/fix-asset-audit.json and Saved/GaspAssetFoundation20260910. The full temporary PoseSearch recovery asset is retained under Saved and has no runtime reference.

Alignment evidence is in Saved/GaspMantleAlignment20260912: before-results.json and before-editor.log retain the previous-runtime failure; validation-summary.json records all eighteen final tests and the measured alignment comparison. build-final.log, final-editor.log and the individual final result files retain execution evidence. Fresh hashes verified all 7,597 content/plugin files and seven saves unchanged; this follow-up changes native rotation guards, tests and documentation only.

## Vault

This slice extends the existing CMC traversal Experience with the three original grounded Vault variants. The gameplay mesh remains the UEFN mannequin. Grounded Hurdle follows in [its own slice](#hurdle); additional visible skeletons and Mover remain separate steps.

### Source behavior and ownership

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

### Test map

Open `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle` with its saved WorldSettings Experience. The three existing 1 m Mantle platforms and PlayerStarts remain in place. The floor boundary extends north to contain two new Vault lanes, retaining original GASP LevelBlock materials and LevelVisuals lighting/exposure.

Each new lane has five 20 cm steps, a platform 1 m above the main floor, and a narrow prepared barrier at its far edge. The barrier is another 1 m above the platform. One barrier is 50 cm deep and the other 30 cm deep; both use the original traversable cube and ledge splines. The lower floor behind them produces the source Vault query conditions. Walk up the stairs, face the barrier and press Space from standing or while approaching. Continue toward the lower floor after crossing. Test angled approaches and both a listen-server host and remote client.

The obstacle actors carry `Rpg.TraversalTest.Vault`; approaches carry `Rpg.TraversalTest.Vault.Approach`. Existing actor class names containing Mantle are retained for compatibility.

### Validation

Execution evidence is recorded under `Saved/GaspVault20260912`. The asset audit compares all three Vault montage copies against their source, the unchanged executable query and chooser logic, exact BranchIn membership and preserved database records, and the project-local dependency closure. Preservation hashes cover all pre-existing tracked Content/Plugins files and save files.

The new authored-map fixture starts at the saved PlayerStarts and drives mapped movement/Space input, including the stairs. The walking-speed case supplies the existing gamepad move axis and verifies the actual 100–250 cm/s speed and walking montage on all roles. Assertions distinguish an active Vault montage, rear-edge crossing, released traversal state while falling and landing on the lower floor beyond the barrier. All successful variants require forward momentum at the owner and server's ability-end delegates. Standing additionally checks replicated BackLedge ownership. No candidate-based teleport or forced approach velocity is used.

Both authored traversal fixtures temporarily ignore physical mouse/stick look on their own local controller. The balanced scope leaves direct SetControlRotation calls observable, so ability/camera bugs still fail the view-preservation assertions. This removes external input interference observed during a previous angle test; the approach, landing and angle assertions are unchanged.

The first cold-editor attempt encountered the source chooser's asynchronous PoseSearch index build and skipped its initial search. That attempt is retained in the evidence; gameplay checks ran after the index completed.

The final SurvivalRpgEditor Win64 Development build passed on Unreal 5.8.2. All 26 tests passed in one 196.17-second run: eight Vault scenarios and the eighteen existing Mantle, CMC, combat and camera cases. The shared network lifecycle regression includes late join, latency, cancellation, death/respawn and collider destruction. Vault-specific gameplay covers standing, walking, running, both angled approaches, listen host, blocked authority exit and server cancellation. These are editor PIE tests; no packaged-build or dedicated-server validation is implied.

The final asset audit passed for 25 roots and 1,606 dependency packages, including source query/chooser preservation, all three Vault montage copies and eighteen owned BranchIn records. Hashes checked all 7,597 pre-existing Content/Plugins files: only the query, owned chooser/database and test map changed; the other 7,593 files and all seven saves remained unchanged. Three new montage packages are the only added content. Source animations, foundation/import assets and baseline composition remain unchanged.

`Saved/GaspVault20260912/validation-summary.json`, `all-final-results.json`, `build-final.log`, `final-editor.log`, `vault-asset-audit.json` and `preservation-final.json` retain the final evidence. Earlier failures are preserved separately. Existing voice-interface, temporary-world NetGUID and respawn-widget tick warnings remain visible in the results.

## Hurdle

This slice extends the existing CMC traversal Experience with original grounded Hurdle selection and animation. Space still uses the existing contextual traversal Ability. The UEFN gameplay mesh, source montage tempo, approved camera damping, existing Mantle/Vault content and map presentation remain in place. Additional visible skeletons and Mover remain separate steps.

### Source content and ownership

GASP action 1 is Hurdle: front and back ledges, a detected back floor, obstacle depth up to 59 cm and a back ledge at least 50 cm above that floor. The selected grounded rows allow obstacle heights up to 125 cm. Vault remains action 2 with no nearby back floor; Mantle remains action 3. The authority independently validates physical geometry and walkable support.

Ten Neutral montages are copied from `/Game/SurvivalRpg/Characters/GASP/Shared/Characters/UEFN_Mannequin/Animations/Traversal/Hurdle` to `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/Traversal/Animations/Hurdle`, keeping their filenames. Each name has prefix `AM_M_Neutral_Traversal_Hurdle_1_0_`:

| Filename suffix | Depth (cm) | Speed (cm/s) | Pose-search entry (s) | Source blend-out begins (s) |
| --- | --- | --- | --- | --- |
| stand_F_V2_Lfoot | 0–25 | 0–100 | 0–0.07 | 1.017674, with movement input |
| walk_F_Lfoot | 0–25 | 100–250 | 0–0.73 | 1.836459 |
| walk_F_Rfoot | 0–25 | 100–250 | 0–0.84 | 2.000074 |
| run_F_Lfoot | 0–25 | 250+ | 0–0.49 | 1.122898 |
| run_F_Rfoot | 0–25 | 250+ | 0–0.48 | 1.065964 |
| stand_F_V2_Rfoot | 25–60 | 0–100 | 0–0.12 | 0.959538, with movement input |
| walk_F_V2_Lfoot | 25–60 | 100–250 | 0–0.50 | 1.599901 |
| walk_F_V2_Rfoot | 25–60 | 100–250 | 0–0.50 | 1.536383 |
| run_F_V2_Lfoot | 25–60 | 25+ | 0–0.66 | 1.395191 |
| run_F_V2_Rfoot | 25–60 | 25+ | 0–0.67 | 1.432122 |

The wider running rows intentionally preserve the source's 25 cm/s lower bound. Their overlap with walking rows is resolved by the original PoseSearch chooser. The outer chooser still limits physical depth to 59 cm. Native unbounded speed rows use the existing finite 100,000 cm/s convention.

The owned chooser and traversal PoseSearch database select these copies. Sequence, skeleton, root motion, slots, complete notify settings and warp modifiers remain source-authored; only the BranchIn database reference points to the existing owned database. The source/foundation packages remain unchanged. `AllowedHurdleAnimations` records the ten eligibility rows on the existing query Blueprint; it defaults to empty. New depth and conditional handoff fields preserve the previous Mantle/Vault defaults.

### Warping and handoff

FrontLedge retains the source hand offset and facing alignment. BackLedge is installed only for montages that use it. BackFloor uses the original `Distance_From_Ledge` curve through Unreal's AnimationWarping library, including animation-segment curves. Its horizontal position is the back edge plus its outward normal multiplied by the absolute difference between the curve values at the first BackLedge and BackFloor window ends. Without a BackLedge window, that first distance remains zero. BackFloor height comes from the queried floor; targets account for the existing gameplay mesh/capsule feet offset.

The wider right-foot walking montage contains an original 3.277 ms overlap between BackLedge and BackFloor warp windows. This authored overlap is retained. Front-facing alignment finishes before the translation-only landing phase; the view direction remains independent.

Original montage notifies own animation handoff. Walking and running force their source blend-out; standing waits for movement input inside its conditional window or reaches the natural montage end. Hurdle returns to ordinary CMC walking on the validated floor behind the obstacle. Native authority, collision/rotation ownership, cancellation and support-lifecycle checks extend the existing traversal seams.

Activation and ongoing clearance checks both place their check capsule on the measured support plane. The source animation's small vertical root residual is separately bounded against that plane; it must not lower the clearance capsule into an otherwise valid floor. The support component must remain registered, walkable, query-enabled and at its accepted transform throughout traversal.

Environmental failures and the existing traversal timeout use GAS's dedicated cancellation path. In this engine version, marking an ordinary `EndAbility` as locally cancelled still replicates a normal end to the other peer. Dedicated cancellation preserves the reason on the owning client and server, including synchronous montage interruption callbacks. Internal safety failures retain their forced cleanup behavior.

### Test map

Open `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle` with its saved Experience. Three prepared original GASP cubes stand in the free eastern area, beyond the existing Mantle platforms: 100 cm high, 400 cm wide and 20, 40 or 59 cm deep. They have the tag `Rpg.TraversalTest.Hurdle` and front faces at X=1500, with lane centers Y=-1200, -400 and 400. Approach their front faces from the west and press Space. Test standing, walking, running and angled approaches, with both the listen-server host and a remote client.

The floor is at the same height before and after these barriers. All 28 previous actors, saved WorldSettings and PlayerStarts are preserved. The original GASP grid materials and LevelVisuals lighting/exposure remain the map's presentation reference.

### Validation

Evidence is collected under `Saved/GaspHurdle20260912`. The source manifest records exact chooser rows, original warp flags, handoff conditions and graph links. The asset audit checks source/copy parity, unchanged query and chooser logic, exact BranchIn membership, preserved database rows and project-local dependencies. Preservation hashes cover all 7,600 pre-existing Content/Plugins files and seven save files.

The final SurvivalRpgEditor Win64 Development build passed on Unreal 5.8.2. All 37 tests passed in one 332.92-second run: eleven Hurdle cases and the 26 existing Mantle, Vault, CMC, camera and combat regressions. Tests use real mapped input in isolated listen-server PIE worlds. Hurdle-specific coverage includes standing natural and conditional exits, analog walking, running, both approach angles, host, maximum depth, blocked authority landing, explicit cancellation and support loss during traversal. Existing lifecycle tests cover late join, latency, death/respawn and collider changes. No packaged-build or dedicated-server validation is implied.

The final asset audit passed for 35 roots and 1,616 dependency packages, all ten source/copy montage pairs, unchanged executable query/chooser logic, all 28 selected owned BranchIn records and 24 unaffected database rows. Preservation hashes checked all 7,600 pre-existing Content/Plugins files: only the query, owned chooser/database and test map changed. The other 7,596 files and all seven saves remained unchanged. The ten Hurdle montage packages are the only new content.

`validation-summary.json`, `all-final-results.json`, `build-final.log`, `final-editor.log`, `hurdle-asset-audit.json` and `preservation-final.json` retain final evidence in `Saved/GaspHurdle20260912`. Earlier diagnostic failures remain separately recorded. Existing voice-interface, temporary-world NetGUID and respawn-widget warnings remain visible in test results.
