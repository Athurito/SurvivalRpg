# Combat turns and landing search: 2026-09-05 review

Follow-up: [the response review](ResponseFollowup20260905.md) records why the earlier passes
did not resolve the user's visible complaints and the corrected reproduction cases.

This review records the narrow presentation corrections in
[PR #122](https://github.com/Athurito/SurvivalRpg/pull/122). It separates visible symptoms,
source evidence, and acceptance still to be completed. Broader medieval movement belongs to
[issue #123](https://github.com/Athurito/SurvivalRpg/issues/123); equipment-dependent dodge belongs to
[issue #124](https://github.com/Athurito/SurvivalRpg/issues/124).
The newly observed jump/prediction investigation is tracked separately in
[issue #125](https://github.com/Athurito/SurvivalRpg/issues/125).

## Evidence and limits

- Project baseline: `ee3dc639`.
- Local GASP CMC reference: `D:/Repos/GameAnimationSample`, commit
  `a9f560350f058462a4d72325bfe73a1e95ea0205`, UE 5.8. The local `SandboxCharacter_CMC_ABP` is modified;
  its inspected graph is not claimed to be an untouched Epic checkout. Its August 13 modification
  predates the August 14/18 graph dumps used for comparison.
- User recording: `2026-09-05 13-40-55.mkv`, 47.47 seconds, 1920 x 1080, captured at 60 FPS.
  The game overlay shows approximately 120 FPS in the inspected combat portion. The separately
  reported 15 FPS multiplayer behavior is a reproduction condition, not the capture frame rate.
- Local evidence includes `Intermediate/GaspTirParityAuditUser`,
  `Intermediate/GaspLandingInputAuditUser`, `Intermediate/GaspJustLandedLifetimeAuditUser`,
  and `Saved/Reviews/Retarget20260905/target_fixed`. These are diagnostic artifacts, not runtime
  dependencies or versioned test results. The candidate source was preserved with SHA-256 hashes in
  `Saved/Reviews/GaspTurnLandingSourceSnapshot20260905/manifest.json` during baseline comparison.

| Recording time | Observation | What it does not establish |
| --- | --- | --- |
| 0–7 s | Rapid combat camera turns with planted feet and late follow-up steps, already visible at approximately 120 game FPS. | Exact input timing, selected turn clip, root offset, or network correction history. |
| 30.4–31.0, 33.4–34.0, 38.2–39.0, 40.4–41.8 s | Landing and continuation windows worth reproducing with runtime diagnostics. | A particular landing asset, a held/released movement key, or a proven network cause. |

The recording motivates the investigation. It cannot independently prove which search result or
input transition produced each pose. Fresh baseline/fixed captures and Pose Search evidence remain
part of acceptance, including the existing visual gate in
[issue #99](https://github.com/Athurito/SurvivalRpg/issues/99).

## Three source findings and the GASP comparison

| Finding | Baseline mechanism and evidence | Bounded correction |
| --- | --- | --- |
| Synthetic turn facing used the wrong basis | `RpgTurnInPlaceRuntime::MakeSyntheticTrajectory` wrote actor-yaw quaternions without the mesh's base rotation. UE's `UPoseSearchTrajectoryLibrary` builds normal facing from mesh orientation, while the project's proxy removes the base offset to obtain actor-facing values. These domains differ for a mesh with a nonidentity base rotation. | Compose normalized actor yaw with the game-thread-captured mesh basis for every synthetic facing sample. Preserve positions and sample times. The coordinate mismatch is source-proven; its effect on the actual winning clip still needs a trace. |
| A selected turn stayed exclusive through its long clip tail | `UpdateTurnInPlaceLatchedPlayback` used the full selected asset duration, and the consumed request could not dispatch a follow-up turn. In the eight inspected clips, root rotation ends around 0.30–0.48 s and foot/ball positions settle near their final pose around 0.67–1.07 s, leaving approximately 0.83–1.27 s of tail. This is not a claim that every tail pose is completely static. Entering `Collecting` also discarded that frame's collection time, adding avoidable frame-rate-dependent delay. GASP instead uses an inclusive 50-degree orientation-intent/root comparison; the project deliberately uses an actor-yaw accumulator and hysteresis. | Add authored per-clip re-entry times and preserve the portion of the entry frame after the collection threshold was crossed. Keep the current thresholds and active root-offset policy. |
| Landing playback lifetime also locked ground search | The selected-landing branch of `UpdateGaspMotionMatching` supplied no new searchable databases with `DoNotInterrupt` until playback completion. Ground candidates therefore could not respond normally during the clip tail. The inspected GASP path clears `JustLanded` after 0.300 s and its landing chooser consumes live gait, idle/moving, and stance. | Separate a short landing-exclusive search window from the selected pose's playback lifetime. Reopen live ground candidates after that window, or after a grounded domain change once a landing selection is latched. |

The local GASP lifetime graph is recorded in
`Intermediate/GaspJustLandedLifetimeAuditUser/Saved/Logs/GameAnimationSample.log`, lines
2025–2028 and 2064–2071 (`true`, 0.300-second delay, `false`). The corresponding live chooser inputs
are recorded in `Intermediate/GaspLandingInputAuditUser/Saved/Logs/GameAnimationSample.log`.
This comparison supports the search-lifetime change; it does not require importing GASP's Blueprint
state or replacing project-owned CMC/GAS lifecycle rules.

## Turn contract

The implementation is owned by [RpgTurnInPlaceRuntime](../../../Source/SurvivalRpg/Animation/RpgTurnInPlaceRuntime.cpp),
[RpgGaspPresentationProfile](../../../Source/SurvivalRpg/Animation/RpgGaspPresentationProfile.h),
and the selection/playback bridge in [RpgAnimInstance](../../../Source/SurvivalRpg/Animation/RpgAnimInstance.cpp).

- `MeshBasisRotation` is captured on the game thread. Synthetic facing uses
  `(ActorYawQuaternion * MeshBasisRotation).GetNormalized()`, including wraparound and arbitrary
  mesh bases. Workers consume the snapshot rather than querying the character.
- `TurnInPlaceClipTimings` stores `{Asset, ReentryTimeSeconds}` in asset seconds. A nonempty list
  must cover exactly the configured turn database, without duplicates, foreign/null/looping clips,
  nonfinite times, or times outside `(0, AssetLength]`. Validation and immutable lookup construction
  occur on the game thread. An empty list retains legacy full-duration playback.
- Forward playback arms completion only when the exact selected asset is observed at or beyond its
  marker. It does not subtract a frame or completion tolerance from authored safety. Reverse playback
  and legacy entries use directional remaining playback time; invalid, paused, or looping playback
  remains bounded by the watchdog.
- Completion enters the existing recovery path and clears the selected clip. Previously accumulated
  actor yaw is not queued again as though it were unconsumed visual rotation. This slice does not add
  a root-feedback endpoint solver or guarantee perfect stop/reversal behavior.
- Collection/activation/cancellation remain 20/30/10 degrees, stability 0.08 s, collection timeout
  0.2 s, recovery 0.15 s, and stationary speed 3 cm/s. Collecting/unselected active turns accumulate
  offset; selected active turns retain `LockOffsetIncreaseAndConsumeAnimation`; inactive/recovering
  turns retain interpolation. No blanket switch to GASP's 50-degree trigger is included.

Candidate authored times for the eight `M_Neutral_Stand_Turn_*` clips are:

| Turn magnitude | Left re-entry (s) | Right re-entry (s) |
| --- | --- | --- |
| 45 degrees | 0.70 | 0.87 |
| 90 degrees | 0.77 | 1.10 |
| 135 degrees | 0.87 | 0.94 |
| 180 degrees | 1.10 | 1.07 |

These values follow the inspected retargeted foot/ball tracks staying within 2 cm of their final
positions, plus a 1/30-second reserve. They are profile tuning candidates with visual acceptance
still pending, not proof that the evaluated graph has planted feet at every marker.

## Landing contract

[RpgLandingRuntime](../../../Source/SurvivalRpg/Animation/RpgLandingRuntime.cpp) owns the pointer-free
search decision; `URpgAnimInstance` owns the selected asset and actual Motion Matching result.

- Physical touchdown still starts the request from the frozen pre-touchdown impact/gait snapshot.
  The new `LandingExclusiveSearchDuration` defaults to 0.3 s of elapsed contact time, independently
  of clip length and play rate. Idle-to-moving landing handoff preserves the same touchdown clock.
- During the exclusive window, an unselected request searches its landing database; a selected
  landing can continue alone. Once selected, a live grounded gait/idle-moving/stance domain change
  can reopen ground search earlier. Initial touchdown is not treated as that domain change.
- `bGroundSearchReleased` stays set for that touchdown. Subsequent frames cannot re-latch landing
  selection or create another landing-only request after release. A new physical touchdown starts
  a new request.
- Releasing search does not immediately mark playback complete or invalidate the continuing pose.
  Normal ground candidates compete while the existing landing can continue. A real ground result
  arms completion; observed playback completion, cancellation, and watchdogs remain fallback exits.
  The outgoing landing retains its existing root-reset protection.

## Preserved content and gameplay scope

This correction changes query coordinates, native lifecycle rules, and profile metadata. It does
not retarget the 190 curated clips again or rewrite their root/bone tracks and curves. The previous
`rpg_no_leg_source_blend_v1` retarget correction remains the basis; foot/ball measurements here are
evidence for release timing, not another skeleton or IK fix.

Character/CMC continues to own facing and physical movement. GAS continues to own attacks, block,
dodge, hits, skills, montage interruption, and root motion. The existing DefaultSlot and
`Root Motion from Montages Only` contract remains in scope for regression verification.

[Issue #123](https://github.com/Athurito/SurvivalRpg/issues/123) owns grounded medieval baseline
movement, sword/shield posture and footwork, and the further stationary root/steering comparison.
The existing moving steering gate below 10 cm/s is not replaced in this slice; any stationary
steering integration needs explicit endpoint, stop, reversal, and invalidation acceptance.
[Issue #124](https://github.com/Athurito/SurvivalRpg/issues/124) owns equipment-based combat options,
including Light roll and Heavy sidestep through existing equipment tiers and predicted GAS seams.
It does not introduce general equipment-load-based acceleration, movement speed, or turning inertia.

## Validation status

The runtime changes are separate commits: `65ae88f7` (Turn/shared playback) and `5908e4a8`
(Landing). The following results are from this candidate, not the earlier migration run.

| Acceptance | Required evidence | Current status |
| --- | --- | --- |
| Editor build | UE 5.8.1, `SurvivalRpgEditor Win64 Development`; final log `Saved/Logs/GaspTurnLandingFixtureBuild20260905.log`. | Passed; existing GetMovementBase deprecation warning remains. |
| Native turn and profile seams | `SurvivalRpg.Animation.TurnInPlace.*`, `SurvivalRpg.Animation.Gasp.TurnInPlaceTimingValidation`: arbitrary mesh bases and +/-180-degree wrap, absolute dispatch latency at 15/30/60/120 FPS, exact marker boundary, invalid metadata and legacy fallback. | Passed. |
| Landing and playback seams | `LandingGroundSearch`, `LandingSelection`, `PhaseAndProceduralGates`, and `SurvivalRpg.Animation.Playback.DirectionalCompletionTime`. | Passed. |
| Complete native selection | `SurvivalRpg.Animation+SurvivalRpg.Character+SurvivalRpg.Health`; report `Saved/Automation/GaspTurnLandingNativeFinal20260905`. | 39 passed: 32 Animation, 6 Character, 1 Health; two tests passed with warnings. |
| Real content | Profile authoring, AnimBP compilation, profile `IsDataValid`, pilot content contract and existing Raw/Compressed retarget pose contracts. | Passed. SHA-256 verification: all 190 animation packages and the AnimBP unchanged; only presentation-profile metadata saved. |
| Rendered diagnostics | Actual evaluated root transforms, MM search set/result, clip/time/rate, contact curves and BlendStack scalar contributions; optional PNGs. | Captured and inspected with the limits below. Final skinning/terrain/foot-slip approval remains #99. |
| Separate-process network | Listen host, autonomous owner, second client with late join; actual IP transport and per-peer 60 +/-10 ms simulated lag plus 10% loss. | Captures completed at all requested caps, including an isolated 120 retry. Missing server jump episodes remain open in #125; this is not full network acceptance. |
| Rendered PIE regressions | `Saved/Automation/GaspTurnLandingPIEVerified20260905`: 9 GASP tests, real remote-melee attack-window/damage/cancellation, and Prototype override. | All 11 passed with warnings, including the actual DefaultSlot/root-motion/correction test. |

Native resolver tests cannot establish rendered foot quality or exclude all multiplayer issues.
The remaining visual and network gates are complementary to this bounded source correction.
The initial PIE run timed out before recording its deliberately offset SavedMove: Engine move
combining could restore a pending move's old start position over the one-shot test offset. The
fixture now marks that existing pending move `bForceNoCombine` before injection; it still sends
the move normally and requires the same server timestamp, real correction and convergence. No
gameplay code, acceptance threshold or timeout was changed for this fixture repair.

## Measured response and remaining boundaries

The baseline capture used the exact `ee3dc639` animation runtime and original profile with the
independent diagnostic harness. Its first 48-second segment is usable; later CSV reader/writer
contention aborted a peer. The harness also initially used an OSS-overwritten player name. Both
recording problems were fixed; baseline observations are selected by the replicated PlayerId 257,
and the baseline run is **not** counted as passed acceptance.

Baseline owner/server ran at approximately 45.49/45.47 FPS. Selected Turn Active spans were
1.95–2.13 s. In the fixed matrix, regular owner spans were approximately 0.79–1.22 s, using the
authored markers at play rate 1.0. Four timed second yaw inputs per segment exercise follow-up and
opposite requests. The fixed character often already plays Idle by those requests; this proves
re-entry input coverage, not an in-flight request inside every clip's last third.

| Requested FPS cap | Measured owner FPS before / after late join | Ground search / selected ground result / scalar ground majority, seconds after regular observed landing |
| --- | --- | --- |
| 15 | 14.51 / 13.07 | 0.304–0.333 / 1.332–1.380 / 1.399–1.458; missing jump episodes excluded and recorded separately. |
| 30 | 23.89 / 23.73 | 0.306–0.340 / 1.305–1.325 / 1.382–1.403. |
| 60 | 46.11 / 46.87 | 0.305–0.325 / 1.283–1.296 / 1.355–1.387; correction-interrupted episodes excluded. |
| 120, isolated retry | 76.96 / 54.11 | 0.301–0.318 / 1.260–1.286 / 1.357–1.381; missing jump episode excluded. |

The earlier owner's ground-search delays were 1.271/1.292/1.276 s for held Run, release, and Walk
input. Search now opens around 0.3 s. **The selected landing pose still commonly wins until about
1.3 s when input remains consistent with it.** Search release is therefore proven; shorter visible
landings under held input are not. The approved continuing-pose competition remains intact rather
than forcing a cut at 0.3 s. Further presentation acceptance belongs to #99/#123.

The requested caps are not achieved render rates. Exact 15/30/60/120-frame-step native contracts
passed, but this machine did not deliver sustained 120-FPS three-process rendering. The first 120
attempt timed out while the late editor spent 210.423 s in its initial AssetRegistry scan; the
isolated retry completed. Neither result establishes packaged performance or Steam transport.

The separate `fixed_visual` run produced 60 PNGs with clip/time metadata. Its first screenshot
coincided with an approximately 10-second frame gap, so that first turn span is excluded from timing
conclusions. The fixture has an invisible collision floor, and the scalar BlendStack weights do
not resolve per-bone blend profiles. Stills and contact curves cannot close terrain/foot-slip review.

The new #125 observation occurs before landing selection: at cap 15, owner 31.240 s is airborne at
Z=10190.64, then 31.307 s is at Z=10092.15 with correction 7→8/history 2→3, while the server never leaves
the ground. Similar missing jumps occur after late join at caps 15, 60 and 120. The first 15-FPS
correction and the 120-retry correction precede their scheduled artificial hitch. The trace cannot
distinguish lost Jump flags, timestamp rejection and gameplay rejection. Short Landing→Airborne
excursions on a simulated proxy are also recorded separately. A cumulative air-to-ground observation
can include a correction and must not be called per-request jump parity.

Local reports (ignored raw evidence):

- `Saved/Reviews/GaspMultiProcess/baseline-20260905-134029-795ebc78/baseline-subject257-summary.json`
- `Saved/Reviews/GaspMultiProcess/fixed_visual-20260905-135621-8e731c1f/fps-60/subject-pose-timing.json`
- `Saved/Reviews/GaspMultiProcess/fixed_matrix-20260905-140524-bc2dedf5/matrix-readout.md`
- `Saved/Reviews/GaspMultiProcess/fixed_120_retry-20260905-142604-1887c396/fps-120`
- `Saved/Reviews/gasp-turn-release-profile20260905.json` and `gasp-turn-landing-immutable-verification20260905.json`

Run [the multi-process launcher](../../../Scripts/Tests/GaspMultiProcess.md) to reproduce captures.
The diagnostic test's successful completion is a recording prerequisite, not blanket scenario
acceptance. #55 still includes dedicated-server, packaged selection, relevancy/correction and
other cutover gates; no merge or issue closure follows solely from this change.
