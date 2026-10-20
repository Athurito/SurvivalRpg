# GASP movement plan (CMC)

Status: planning document, 2026-10-11. `GASP-08` is merged ([#228](https://github.com/Athurito/SurvivalRpg/pull/228)); the user decided **Pass** on 2026-10-11. Every other package here is **Planned**. Each package gets its own task report and PR when it starts. This page holds the shared findings, the design rules, the package outlines, and their order.

## Design rules

- CMC is the movement variant going forward. Mover stays as an accepted variant but receives no new feel work from this plan.
- Movement must feel heavy and grounded (dark-fantasy medieval), not athletic or parkour.
- **Load-independent movement.** Equipment load never changes speed, acceleration, braking, rotation, jump, sprint speed or stamina cost, gait availability, or traversal ([game vision](game-vision.md)). The load tier only selects the dodge: Light a full roll, Medium a shorter roll, Heavy a sidestep.
- **Shared heavy feel.** The heavy feel is one shared base tuning for every character.
- **Speeds match the animation set.** Target speeds stay inside the Motion Matching play-rate band (0.85–1.15) of the animation set in use. Heavy movement is never faked by slower playback.
- **Prediction-safe movement.** Gameplay movement values live in prediction-safe overrides of `URpgCharacterMovementComponent`. The original GASP Blueprint changes `MaxWalkSpeed` and friends every tick; that pattern is not copied.

## Findings from the 2026-10-11 review

Measured on `master` `59ecdec4` with UE 5.8.2. Measurements were a standalone PIE probe on `Lvl_RpgGaspCMC` at about 120 FPS, plus root-motion speed of the loops. Original values come from the hash-verified export of `SandboxCharacter_CMC` (GASP sample, 2026-09-28) and the project-owned copies of the original assets.

1. **Rotation mode conflict.**
   - `ARpgCharacter` sets `bUseControllerRotationYaw = true`, and `BP_Rpg_Character` adds `bOrientRotationToMovement = true` at 500°/s. The capsule follows the camera: strafing runs 86° off the facing, backpedaling 176° off, at full speed. Orient-to-movement adds a frame-rate-dependent nudge of about 4° at 120 FPS.
   - The AnimBP is told `OrientToMovement` and `WantsToStrafe = false`, so aim offset and strafe-specific logic stay off.
   - Sprint has no direction gate, so sprinting backwards is possible. The original allows sprint only within 50° of the facing in strafe mode.
   - The original uses `bUseControllerDesiredRotation` for strafe or aim and orient-to-movement otherwise; capsule rotation is instant on the ground and 200°/s in the air.
2. **Speeds do not match the animation set.**

   | Set (loops, cm/s) | Walk F / side / B | Run F / side / B | Sprint F |
   | --- | --- | --- | --- |
   | Neutral, used by the CMC Dense databases | 200 / 180 / 150 | 500 / 350 / 300 | 700 |
   | Relaxed, used by the Mover chooser | 165 / 165 / 165 | 375 / 375 / 375 | 585 |
   | CMC today | – (no walk) | 375 / 375 / 375 | 585 |

   - Forward running is 0.75× the authored loop and backpedaling 1.25×, both outside the 0.85–1.15 play-rate band, so the feet slide.
   - Crouch walk is 300 cm/s against an authored 225 / 200 / 180.
3. **Acceleration and braking are much snappier than the original.**

   | | Original CMC | CMC today |
   | --- | --- | --- |
   | `MaxAcceleration` (cm/s²) | 800; sprint tapers from 800 to 300 with speed | 1500 |
   | Braking without / with input (cm/s²) | 2000 / 500, friction factor 0 | 2000 plus separate friction 6 |
   | Ground friction | 5; sprint tapers to 3 | 8 |
   | Start to 95 % run speed | ≈ 0.6 s | 0.24 s, measured |
   | Stop from run | 0.25 s / 62 cm | 0.125 s / 18.5 cm, measured |

4. **Other gaps.**
   - No walk gait; partial analog input stays in the Run gait.
   - Air control is 0.35 with 1500 falling deceleration, against 0.25 and 0 in the original.
   - Movement defaults live twice: the C++ constructor sets 2400 / 1400 / 720, and `BP_Rpg_Character` overrides them with 1500 / 2000 / 500.
   - Camera turns rotate the character during attack montages, because `FaceRotation` is only suppressed for mantle and block (derived from code, not measured).

The AnimBP itself matches the original except for the block layer and the disabled notify filter, so these gaps come from character and movement settings, not from GASP content.

## Packages

### GASP-08: Manny skeleton pilot

**Merged** with the user's **Pass** decision of 2026-10-11, in [#228](https://github.com/Athurito/SurvivalRpg/pull/228); see the [pilot report](gasp-manny-skeleton-pilot.md). It decides whether humanoid characters standardize on the UE5 Manny skeleton by baking GASP clips offline with the official retargeter. It touches only pilot copies.

The automated evaluation supported the pass:
- 144/144 clips are preserved and root speed is unchanged.
- Foot contact matches the runtime follower.
- Motion Matching agrees with the eight-database control in 98.9 % of frames.
- Animation cost is lower than UEFN plus follower.

Its recorded limits carry into GASP-14 and GASP-15:
- the bake targets `SKM_Manny_Simple` without corrective bones;
- the pelvis sits 3.09 cm forward;
- the normalization is pilot-only;
- multiplayer and gameplay on a Manny mesh are untested.

### GASP-09: Rotation modes and combat stance

**Merged** in [#232](https://github.com/Athurito/SurvivalRpg/pull/232) after the user's check; see the [task report](gasp-rotation-modes.md). The user decided that every hand item, tools included, triggers the stance; attacks steer during the windup and hold from the hit window; the stance turns instantly.

- **Goal:** two explicit modes, reported correctly to the AnimBP.
  - **Free movement:** hands holstered, out of combat. The character turns toward its movement.
  - **Combat stance:** a weapon in hand, blocking, lock-on, or aiming. The character faces the camera and strafes, as in New World.
    Standing still it keeps its facing while the camera orbits, as in Souls-likes and Valheim, until it moves, blocks or attacks (user choice 2026-10-16 in `GASP-13`).
- **Scope:**
  - The trigger comes from existing equipment state: active hand versus holstered in `URpgEquipmentLoadoutComponent`.
  - Correct `RotationMode` and `WantsToStrafe` in `BP_RpgGasp_CMC`.
  - Remove the camera-yaw and orient-to-movement conflict.
  - Sprint direction gate in combat stance.
  - Decide with the combat foundation whether attacks keep their facing during the montage.
- **Touches:** `RpgCharacter`, `URpgCharacterMovementComponent`, `BP_RpgGasp_CMC`, and possibly `BP_Rpg_Character`. These overlap GASP-10 to GASP-12, so those run sequentially, not in parallel.
- **Acceptance:**
  - Free mode: facing follows velocity with no frame-rate-dependent offset.
  - Combat stance: strafe mode is reported and the aim offset is active.
  - Backward sprint is impossible in stance.
  - Owner, authority, simulated proxy, correction, and late join are checked.
  - Block keeps its camera alignment.

### GASP-10: Animation set and speed parity

**Merged** in [#235](https://github.com/Athurito/SurvivalRpg/pull/235) after the user's check; see the [task report](gasp-animation-set.md). The user chose the Relaxed set and kept the Neutral traversal montages. The CMC now plays the Relaxed databases at matching speeds; crouch drops to 200 cm/s.

- **Goal:** make every target speed fit the animation set actually played.
- **Scope:** choose one of two options.
  - **Relaxed set (recommended):** switch the CMC chooser to the Relaxed databases the Mover already uses. Run 375, sprint 585, and walk 165 cm/s then match exactly.
  - **Neutral set:** keep it with directional speeds (run 500 / 350 / 300), which is more athletic.

  In either case, align crouch speeds to the measured crouch loops of the chosen set.
- **Depends on:** GASP-09. The choice also fixes which databases GASP-14 bakes.
- **Acceptance:**
  - Every loop's target speed lies within 0.85–1.15 of its measured root speed.
  - Foot sliding is checked on starts, loops, stops, and turns.
  - Mover is unaffected.

### GASP-11: Shared movement profile

**Merged** in [#238](https://github.com/Athurito/SurvivalRpg/pull/238) after the user's check; see the [task report](gasp-movement-profile.md). The user chose a jump Z of 450 cm/s and instant free-movement turning on the ground; the combat stance keeps turning instantly (GASP-09 decision 3). The CMC pawn now takes its movement from `DA_RpgGasp_MovementProfile`: about 0.48 s to run speed and a 0.29 s / 51 cm stop at 60 FPS.

- **Goal:** one designer-tuned movement profile asset for every character, applied through prediction-safe CMC overrides.
- **Profile contents:** speed per gait and direction, acceleration (tapering during sprint build-up), braking with and without input, friction, rotation rates for ground, sprint, and air, air control, and jump. The C++ and Blueprint defaults collapse into this single source.
- **Starting values** (to tune by feel, Relaxed set assumed):

  | Value | Start |
  | --- | --- |
  | Acceleration | 750, tapering to about 350 at sprint speed |
  | Braking without / with input | 1300 / 500 |
  | Braking friction factor | 0 |
  | Ground friction | 4, tapering to 3 when sprinting |
  | Rotation rate ground / sprint / air | 400 / 220 / 150 °/s |
  | Air control | 0.15 |
  | Falling deceleration | 0 |
  | Jump Z | 420–450 |

  This gives roughly 0.47 s to run speed and a stop of about 0.29 s over about 54 cm.
- **Depends on:** GASP-10.
- **Acceptance:**
  - Measured start and stop times match the profile.
  - Saved-move replay and server correction are verified.
  - Simulated proxies and late join are covered.
  - Equipment changes across every load threshold leave movement unchanged.

### GASP-12: Walk gait and analog input

**Merged** in [#241](https://github.com/Athurito/SurvivalRpg/pull/241) after the user's check; see the [task report](gasp-walk-gait.md). The user chose Caps Lock for the walk toggle, with crouch staying on Ctrl, and walking below the 0.7 stick threshold. The pawn walks at 165 cm/s on the Relaxed walk databases; every gait moves at a fixed speed, and simulated proxies receive the gait.

- **Goal:**
  - a walk toggle and an analog walk/run threshold, as in the original `GetDesiredGait`;
  - the gait (Walk / Run / Sprint) reported to the AnimBP and to simulated proxies.
- **Depends on:** GASP-11.
- **Acceptance:**
  - Toggle and gamepad threshold both select Walk.
  - Gait transitions stay inside the play-rate band.
  - Proxies and late join show the same gait.

### GASP-13: Presentation polish

**Merged** in [#247](https://github.com/Athurito/SurvivalRpg/pull/247) after the user's checks; see the [task report](gasp-presentation-polish.md). The user deferred a remaining problem when block is released; GASP-STAB-05 addressed it with exit steps, and the user parked it on 2026-10-17 because the press no longer felt snappy (draft [#249](https://github.com/Athurito/SurvivalRpg/pull/249)). On 2026-10-14 the user added the standing turn in place in the combat stance, found in GASP-STAB-04, and moved armor Foley to GASP-16 or its own audio package.

- **Goal:** visual weight without gameplay change; a standing character in the combat stance turns its feet with the camera.
- **Scope:**
  - Standing turn in place in the combat stance, first.
  - Offset Root Bone rotation half-life: 0.1 → 0.15–0.2.
  - Motion Matching `PoseReselectHistory`: 0.3 → 0.4–0.5.
  - Steering `ProceduralTargetTime`: 0.4 → 0.5–0.6.
  - Cost bias against spin transitions.
  - Camera lag.
- **Depends on:** GASP-11 and GASP-STAB-04.
- **Acceptance:** user visual check. No movement value changes.

### GASP-14: Full CMC bake to Manny (only if GASP-08 passes)

**Merged** in [#251](https://github.com/Athurito/SurvivalRpg/pull/251) after the user's check on 2026-10-17; see the [task report](gasp-manny-full-bake.md). The user chose `SKM_Manny_Simple` as the target mesh. GASP's traversal `attach` bone is carried on Lyra's `interaction` bone, so `SK_Mannequin` keeps its bones and GUID.

- **Goal:** bake the whole CMC closure the game uses onto the chosen Manny skeleton with the official retargeter, using the pilot's recorded retargeter settings.
- **Scope:**
  - the animation set chosen in GASP-10: the Relaxed databases instead of Dense and Sparse if Relaxed wins;
  - traversal, choosers, and Pose Search databases;
  - a production normalization set;
  - the target mesh: `SKM_Manny_Simple` or a mesh with corrective bones, decided from the GASP-16 character choice.
- **Depends on:** GASP-08 pass and the GASP-10 set decision. The #228 handoff proposes the full bake as the next step after a pass; settle the animation set first, so the bake does not cover databases the game will not use.

### GASP-15: Manny gameplay mesh (only if GASP-08 passes)

**Merged** in [#254](https://github.com/Athurito/SurvivalRpg/pull/254) at the user's request on 2026-10-17; see the [task report](gasp-manny-gameplay-mesh.md). The user limited it to the CMC pawns (the Mover stays on UEFN with its follower, and block presentation is selectable per skeleton) and chose to remove the GASP-08 pilot. Block turning findings from its user check, which predate it, are planned as `GASP-STAB-06` ([definition](gasp-block-turn.md)).

- **Goal:** make the Manny-skeleton character the `GetMesh()` of the CMC pawn.
- **Scope:**
  - Move sockets, physics asset, ragdoll, montage slots, and gameplay montages over.
  - Retire the runtime follower for gameplay characters.
- **Depends on:** GASP-14.

### GASP-16: Character and armor pilot

**Merged** on 2026-10-19 ([#266](https://github.com/Athurito/SurvivalRpg/pull/266)); see the [task report](gasp-character-armor-pilot.md). The user keeps the Paladin; the production CMC pawns adopt it in `GASP-17`.

- **Goal:** prove a marketplace character on the chosen skeleton with one modular armor set driven by Leader Pose, including hiding body parts under armor.
- **Character:** the user chose the Polyphoria MetaHuman Paladin, with Knight_01 as a comparison. A character editor comes later; GASP-16 keeps the MetaHuman route open.
- **Design:** after Phase A the user chose Lyra's Character Parts on 2026-10-18. The visible body copies the pose and drives its corrective bones, and the armor follows the body by Leader Pose. Migrated textures are capped at 2K.
- **Armor Foley:** moved here from GASP-13 on 2026-10-14. On 2026-10-18 the user deferred all sound, so it is out of GASP-16 and waits for a later audio package. Its layers may differ by load, because Foley keeps the pace and does not change movement.
- **Depends on:** GASP-15 (merged).

### GASP-17: Paladin in the production CMC Experiences

**Merged** on 2026-10-19 ([#268](https://github.com/Athurito/SurvivalRpg/pull/268)); see the [task report](gasp-paladin-production.md).

- **Goal:** the production CMC Experiences show the MetaHuman Paladin from GASP-16 through one Experience action set that can be removed again. Pawn Blueprints, PawnData, animation and gameplay stay unchanged.
- **Out:** Mover, Baseline, Prototype and enemy pawns; character editor; male MetaHuman variants; sound.
- **Depends on:** GASP-16 ([#266](https://github.com/Athurito/SurvivalRpg/pull/266)).

### Related combat work

[#124](https://github.com/Athurito/SurvivalRpg/issues/124), the load-dependent dodge, may build its skeleton-independent parts at any time: grant, input, cost, blockers, tier snapshot contract, and tests. Its final roll and sidestep montages follow the GASP-08 decision. With GASP-15 they need no retargeting, because the sources are Manny clips. Directional dodges assume the GASP-09 combat stance.

## Order and parallelism

1. **GASP-08** to **GASP-12** are merged, and so are **GASP-14** ([#251](https://github.com/Athurito/SurvivalRpg/pull/251)) and **GASP-15** ([#254](https://github.com/Athurito/SurvivalRpg/pull/254)).
2. **GASP-STAB-04**, the host-view jitter found in the GASP-12 check, came first at the user's choice and is **merged** in [#244](https://github.com/Athurito/SurvivalRpg/pull/244) after the user's check ([task report](gasp-host-view-turn-jitter.md)); **GASP-13** is **merged** in [#247](https://github.com/Athurito/SurvivalRpg/pull/247) ([task report](gasp-presentation-polish.md)). Both concern the CMC AnimBP's Offset Root Bone path.
3. After a GASP-08 pass: **GASP-14 → GASP-15**, then **GASP-16**, then the final dodge montages for #124.
4. If GASP-08 fails: GASP-14 and GASP-15 are dropped. GASP-16 then targets the runtime follower path, and the dodge montages are retargeted to UEFN like the block set.

`GASP-VAL-02` (cooked, packaged, and emulated-network validation) is **merged** ([#271](https://github.com/Athurito/SurvivalRpg/pull/271)); see the [task report](gasp-packaged-network-validation.md). Real WAN stays deferred.
