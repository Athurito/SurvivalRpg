# RNG-01: Shared combat projectile

Task report for RNG-01 in the [ranged combat roadmap](ranged-combat-roadmap.md), [PR #364](https://github.com/Athurito/SurvivalRpg/pull/364) and its review follow-up FOLLOWUP_PR. One native, server-authoritative projectile carries arrows and spells: the combat roadmap's "Projectile foundation". It flies on every machine, hits exactly once on the server, applies its damage through a GameplayEffect, blocks on a shield from the incoming flight direction, and plays one impact cue on every machine. Any ability, NPC or system can launch it without equipment. The VFX-05 fireball ([vfx-fireball.md](vfx-fireball.md)) is its first content. Bow, ammunition, aiming and recovery follow in RNG-02 to RNG-06.

## C++ boundary decision

- **Classification:** a new reusable native mechanism (projectile authority, replication and lifecycle), an extension of the damage mechanism (`URpgDamageExecution`, `RpgCombatImpact`) and of the block ability, a new collision channel, and designer content (the fireball projectile, the shield's blockable damage types).
- **Runtime truth:**
  - **Impact:** the server's flight sweep decides the one impact. Clients never collide.
  - **Defense:** the server's damage execution decides hit, block or perfect block, as for melee.
  - **Read state:** the replicated `ImpactState` (result, location, normal) only stops the presentation on every machine.
- **Existing seams:** `URpgDamageExecution` and the shared block rules in `RpgCombatImpact`, `FRpgGameplayEffectContext`, `URpgGameplayAbility_Block` with `FRpgWeaponBlockDefinition::BlockableDamageTypeTags`, `URpgFactionSubsystem`, the ability system of `ARpgGameStateBase`, and the engine's `UProjectileMovementComponent`.
- **Native, and why:**
  - **Authority and lifecycle:** exactly one impact, damage that outlives the shooter's pawn, a replicated stop that also reaches late joiners, and a projectile that never obstructs pawn movement. These are invariants that every arrow and spell shares.
  - **Server sweep:** each frame the server sweeps the segment the movement covered on `Rpg_TraceChannel_Projectile` (`ECC_GameTraceChannel10`), with filtering that Blueprint collision callbacks cannot order (the shooter, dodging targets and the shooter's other avatars pass through, at most eight per segment).
  - **Launch snapshot:** the damage spec is built at launch from the launching ability's context. The projectile becomes the effect causer and keeps the shooter's faction (`IGenericTeamAgentInterface`), so a weapon swap, death or respawn never changes a projectile in flight.
  - **Projectile defense:** the damage execution reads the incoming direction from the hit's trace segment, measures range from the launch origin, and blocks a projectile only while the active block's source lists `Damage.Type.Projectile`.
- **New native classes:** one, `ARpgCombatProjectile`, as planned in the roadmap. `ERpgProjectileImpactRecovery` and `FRpgProjectileLaunchParams` are its schema; recovery itself acts on the policy in RNG-03b.
- **Deviation from the roadmap's class table:** the impact cue goes through the GameState's ability system, not the instigator's. The GameState is always relevant and keeps its avatar, while the shooter's ability system drops cues on machines where the shooter's pawn is dead or not relevant.
- **Content:** concrete projectiles are Blueprint subclasses that set class data and attach their presentation under `VisualRoot`; no native leaf per projectile.

## Behavior

### Problem

There was no projectile. Spells and the planned bow had no way to fly, hit, damage or play an impact; the VFX-05 fireball only existed as a local gallery demo. Blocking knew only melee: the block cone used the attacker's current position, `BlockableDamageTypeTags` on weapon block definitions was read by nothing, and a perfect block staggered the attacker regardless of distance.

### Change

- **Launch:**
  - `LaunchFromAbility` (BlueprintCallable, server only) builds the damage spec from the projectile class's `DamageEffectClass` with the calling ability's context. Any ability works; no equipment or inventory is needed.
  - `LaunchWithDamageSpec` (native) takes a spec the caller built, for launches outside an ability.
  - `FRpgProjectileLaunchParams` carries origin, direction, speed (zero uses the class's `DefaultSpeed`), `SetByCaller.Damage`, `SetByCaller.StaggerDamage` and an optional shot identity, replicated once for later predicted cosmetics.
  - The spec always gets `Damage.Type.Projectile` plus the class's `DamageTypeTags` and optional body-hit cue.
  - The projectile is owned by the ability system's owner (the PlayerState for players), so it stays net-owned by the shooter after the pawn dies.
- **Flight:**
  - `UProjectileMovementComponent` integrates the ballistic flight on every machine, without collision (`bSweepCollision` off, `NoCollision` root).
  - Clients smooth server corrections through `VisualRoot` with the movement component's interpolation.
  - The flight expires after `MaxFlightSeconds` without an impact.
  - The launch point replicates once. A launch sound plays there on every machine that receives the projectile within 0.5 s of its flight (distance from the launch point over the current speed); a late joiner hears none.
- **Impact (server only):**
  - The first valid hit claims the impact, freezes the flight and applies the spec once through `TargetASC->ApplyGameplayEffectSpecToSelf`, so it works after the shooter died.
  - A dodging target (`State.Evading`), the shooter's pawn and any other avatar of the shooter's ability system are passed through.
  - `PreApplyImpactDamage` is a native hook before a potentially lethal hit, for RNG-03c's recovery records.
  - The damage execution reports its defense outcome in the server-only `FRpgGameplayEffectContext::ImpactOutcome`; the projectile maps it to `ERpgProjectileImpactResult` (Hit, Blocked, PerfectBlocked, Unaffected).
  - The impact cue (`ImpactCueTag`, with `ImpactCueMagnitude` as `RawMagnitude`) executes through the GameState's ability system with a cleared prediction key, on world hits always and on targets when `bImpactCueOnTargets` is set.
- **Stop on every machine:** the replicated `ImpactState` places the projectile at the impact, deactivates every FX system, fades every audio component over `AudioFadeOutSeconds` and calls `On Flight Stopped`. The actor lingers `ImpactLingerSeconds`, then the server destroys it. A late joiner, a client that gains relevancy during the linger, or a point-blank impact receives the projectile already stopped and plays no flight. Such a client stops it in BeginPlay, after its component ticks are registered, and deactivates the movement, so a stopped arrow stays where it hit and `On Flight Stopped` follows the Blueprint's BeginPlay.
- **Projectile defense:**
  - `URpgGameplayAbility_Block` publishes its source's `BlockableDamageTypeTags` on the server's ability system while the block state applies and clears them with it.
  - A projectile meets only a block whose source lists `Damage.Type.Projectile`. The block cone faces the incoming flight (`RpgCombatImpact::ResolveProjectileOutcome`, `GetIncomingDirection`), never the shooter's position.
  - A perfect block stops all damage and never staggers the distant shooter. Block events and hit cues name the projectile as their source.
  - Melee blocking is unchanged.
- **Collision:** `Rpg_TraceChannel_Projectile` blocks by default, so world geometry, character meshes and ragdolls stop projectiles. Pawn capsules (`RpgPawnCapsule`, `CharacterCapsule`, `Pawn`), triggers and the overlap profiles ignore it, so projectiles hit the visible body and never stop on invisible volumes. The projectile disables its own actor collision, so meshes a subclass attaches never block pawns, cameras or other projectiles. Slot 10 had no name before, so components saved its responses as `GameTraceChannel10`, such as the Ignore of the PCG volume brush in `Lvl_PortalRealm_RootGrove`; a `CollisionChannelRedirects` entry keeps them on the projectile channel, so that invisible brush does not stop projectiles.
- **Dev command:** `Rpg.Combat.LaunchProjectile <ClassPath> [Speed] [Damage]` launches a projectile along the host's view without equipment (server only, non-shipping builds).

### Content

| Asset | Role |
| --- | --- |
| `GF_Combat_Magic/Projectiles/BP_Projectile_Fireball` (parent `ARpgCombatProjectile`) | `NS_Spell_Fireball_Projectile` and `SW_Fireball_FlightLoop_01` under `VisualRoot`; `GE_Damage_Instant`, `Damage.Type.Magic`; speed 2500 cm/s, gravity 0, radius 20 cm; impact cue `GameplayCue.Spell.Fireball.Impact` with magnitude 300; three launch sounds `SW_Fireball_Launch_01`–`03`; consumed on impact |
| `BP_WeaponInstance_BasicShield` | Lists `Damage.Type.Projectile` in its blockable damage types; swords do not |
| `Config/DefaultEngine.ini` | The trace channel `Rpg_TraceChannel_Projectile`, the ignore responses of the capsule, trigger and overlap profiles, and the redirect of the saved `GameTraceChannel10` responses |

## Validation (2026-11-01)

### Review follow-up, on `master` `1d7a77de` plus the follow-up

- **Review:** the coordinating session's four-lens review (network and lifecycle, GAS damage and defense, collision config, tests and conventions), each finding checked by an adversarial verifier, confirmed six findings and rated one plausible. The follow-up fixes all of them:
  - **Saved slot-10 responses (high):** naming slot 10 dropped the saved Ignore of the PCG volume brush in `Lvl_PortalRealm_RootGrove` (about 50 x 50 x 20 m over the realm floor), so the invisible brush would have stopped every projectile. Fixed by the channel redirect. The brushes in `Lvl_ResourceParity` and `Lvl_HarvestPickaxe` save `NoCollision` and were not affected.
  - **Stopped arrival (medium):** a client that received the projectile already stopped ran `OnRep_ImpactState` before BeginPlay; BeginPlay's tick registration then restarted the movement, so a stopped arrow fell under gravity there for the whole linger, and `On Flight Stopped` fired before the Blueprint's BeginPlay. Fixed in BeginPlay, with the movement deactivated.
  - **Tests (medium and low):** clients' stop, the block ability's publication, the shield asset contract, world impacts and flight expiry had no automated check; the tests below add them.
  - **Docs (low):** the roadmap's class table and the fireball report still named the instigator's ability system as the cue sender.
  - **Launch sound (plausible):** clients played it at the projectile's current position; the launch point now replicates.
  - **Refuted:** a missing `GameplayEvent.HitReaction` (nothing consumes it since CMB-04), damage on corpses (RNG-03c owns post-death hits), blocking volumes stopping projectiles (engine behavior, no such volume in the project), and tests for the shared stamina rules and the respawned-pawn pass-through.
- **Red run first:** with the new tests on the unchanged projectile, `CollisionProfiles` failed on the saved slot-10 response (Block instead of Ignore), and `PointBlankAndOrphanedProjectilesStillHitOnce` failed because the falling point-blank copy drifted on the clients. Every other new check passed.
- **Build:** `ue.py build` passed, 0 errors, 0 warnings.
- **Native and asset tests:** `ue.py test SurvivalRpg.Combat.Block.Lifecycle SurvivalRpg.Combat.HitFeedback SurvivalRpg.Combat.Dodge SurvivalRpg.Combat.ReactionProfile SurvivalRpg.Combat.Runes SurvivalRpg.Combat.SwingTrace SurvivalRpg.Combat.AbilityContent SurvivalRpg.Combat.HyperArmor SurvivalRpg.AbilitySystem SurvivalRpg.Combat.Projectile SurvivalRpg.Combat.WeaponBlockAssetContract SurvivalRpg.Combat.WeaponAttackAssetContract SurvivalRpg.Combat.StarterEquipmentAssetContract --null-rhi` passed 54 / 54. New:
  - `Projectile.WorldImpact`: a hit on an actor without an ability system resolves as World, damages nobody, stops at the sweep location, lingers, and plays the impact cue once at the impact point through the GameState's ability system; on a target the cue plays only with `bImpactCueOnTargets`.
  - `Projectile.FlightExpiry`: without an impact the flight expires after `MaxFlightSeconds`, and not before.
  - `Projectile.CollisionProfiles`: also a response saved as `GameTraceChannel10` applies to the projectile channel.
  - `Block.Lifecycle.PublishesBlockableDamageTypesWhileBlocking`: the concrete `GA_Combat_Block` publishes exactly its source's blockable damage types and clears them on release.
  - `WeaponBlockAssetContract`: `BP_WeaponInstance_BasicShield` blocks projectiles, the basic and two-handed swords do not; all three block melee.
- **PIE network tests:** the same command as below passed 10 / 10. Each projectile shot now also checks every client: it shows the shot stopped within 1 m of the target and watches the copy for 0.5 s with less than 1 cm drift. The point-blank shot uses full gravity and reaches the clients already stopped. A client in flight also receives the launch point. Only the baseline warnings appeared.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.

### RNG-01, on `master` `0e45c7c3` plus PR #364

Final runs in the task worktree, on the code of PR #364:

- **Build:** `ue.py build` passed, 0 errors, 0 warnings.
- **Native tests:** `ue.py test SurvivalRpg.Combat.Projectile --null-rhi --expect-tests 6` passed 6 / 6:
  - `DirectionalBlock`: the block cone faces the incoming flight, not the shooter; perfect-block window; all-around block; the incoming direction from the hit segment or from origin to impact.
  - `ImpactAppliesOnce`: launch without equipment or an ability; faction, shot identity and effect causer are kept; the projectile's own components never collide; a second impact report deals no second damage; the launch origin survives `AddHitResult`.
  - `ShieldBlock`: a shield blocks a frontal projectile with reduced damage, a shot into the back hits although the shooter stands in front, a melee-only block takes full damage, a perfect block takes none and never staggers the shooter.
  - `LaunchState`: damage uses the shooter's stats from the launch, after the stats changed and the shooter was destroyed.
  - `FlightSweep`: the sweep passes the shooter, hits the target, flies through a dodging target and keeps it passed.
  - `CollisionProfiles`: world, mesh, ragdoll and harvestable profiles block the projectile channel; capsule, pawn, trigger and overlap profiles ignore it.
- **Regression:** `ue.py test SurvivalRpg.Combat.Block.Lifecycle SurvivalRpg.Combat.HitFeedback SurvivalRpg.Combat.Dodge SurvivalRpg.Combat.ReactionProfile SurvivalRpg.Combat.Runes SurvivalRpg.Combat.SwingTrace SurvivalRpg.Combat.AbilityContent SurvivalRpg.Combat.HyperArmor SurvivalRpg.AbilitySystem SurvivalRpg.Combat.Projectile --null-rhi` passed 48 / 48.
- **PIE network tests:** `ue.py test SurvivalRpg.Network.CombatProjectilePIE SurvivalRpg.Network.CombatRemoteMeleePIE SurvivalRpg.Network.CombatImpactCuesPIE SurvivalRpg.Network.KillXPCuePIE --expect-tests 10 --ue-arg=-ini:Engine:[ConsoleVariables]:TestFramework.CQTest.CommandTimeout.Network=90` passed 10 / 10. The projectile tests run a listen server with two clients:
  - `ReplicatedFlightHitsOnceWithOneImpactCuePerMachine`: every client sees the projectile in flight, the target takes one hit, and each machine plays exactly one impact cue at the target.
  - `PointBlankAndOrphanedProjectilesStillHitOnce`: a projectile that hits before it first replicates, and one whose shooter is destroyed in flight, each hit once with one cue per machine and leave every machine.
  - `PredictedAbilityLaunchesWithoutEquipment`: a client-predicted ability without equipment launches through `LaunchFromAbility`; the hit names the predicting player as instigator, and the predicting client plays the impact cue too.
  - The warnings of every test are the baseline `LogNetPackageMap` package-map messages and, in the melee and impact-cue tests, `AdvancedFriendsInterfaceLog` without a voice interface; none comes from the projectile.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.
- **Earlier runs:** the same suites passed in the master checkout before the work moved to this worktree (48 / 48, 3 / 3 and 7 / 7), on the code before the collision hardening below.
- **Manual PIE, `Lvl_ThirdPerson`** (master checkout, before the collision hardening): `Rpg.Combat.LaunchProjectile` with `BP_Projectile_Fireball` flew along the view and exploded at the wall with the fireball impact cue. Captures: `Saved/RNG01/fireball-flight.png`, `fireball-impact.png` (ignored, local evidence).
- **Review:** the task session's own focused review, run while the four-lens review was still pending, found no correctness defect; the four-lens review above found the defects its follow-up fixes. The focused review added the native collision hardening (`SetActorEnableCollision(false)`, so content meshes under `VisualRoot` never block pawns, cameras or other projectiles) and recorded the impact cue budget and the client launch-sound position as limits.

## Playtest

1. Open `Lvl_ThirdPerson` and start PIE as a single player. In the console run `Rpg.Combat.LaunchProjectile /GF_Combat_Magic/Projectiles/BP_Projectile_Fireball.BP_Projectile_Fireball_C 900 10`. A fireball with its launch sound and flight loop flies along your view and explodes at the first wall or character with the fireball impact; it never stops on your own character.
2. **Multiplayer:** set Play to 2 players, *Play As Listen Server*, and run the command in the host's window. The client sees the same flight and exactly one explosion at the same place.
3. **Shield:** the client equips sword and shield, faces the host and holds block. A fireball from the host into the shield is blocked (no health lost, block feedback); walk around the client and shoot it in the back: it hits.

## Limits

- **No lag compensation:** the server sweeps against its own current poses. A remote client's shot hits where the target is on the server, and Mover proxies are interpolated about 100 ms behind, so a near miss on a client can hit on the server and the other way round.
- **Legacy replication only:** Iris is inactive in this project. `ImpactState` and the launch data were exercised only with the legacy replication path.
- **Client overshoot:** client copies never collide. Until the replicated stop arrives, a client's projectile can fly a little past a wall or through a character, then snaps to the impact.
- **Impact cue budget:** every projectile impact cue goes through the one GameState ability system. Unreliable multicasts are limited by `net.MaxRPCPerNetUpdate` (10 in `Config/DefaultEngine.ini`) per function and net update, so more than ten impacts with cues within one GameState net update drop the rest on clients. One bow or spell stays far below; a mass effect such as Rain of Arrows must not send one cue per visible arrow.
- **Launch sound timing:** the flight age that decides a late receiver's launch sound is estimated from distance and current speed; a strongly slowed ballistic flight counts as older than it is.
- **Not covered by a test:** the rule that a respawned pawn of the shooter is passed through rests on comparing ability systems in `ShouldPassThrough`; no test respawns a shooter mid-flight.
- **Later tasks:** launch-obstruction validation, ammunition and the shot commit (RNG-02), the predicted cosmetic arrow (RNG-02/03, `ShotId` is only carried), and the break roll, embedding and recovery (RNG-03b/03c, `ImpactRecovery` and `PreApplyImpactDamage` are only the seams) are not part of RNG-01.
- **AI retarget gap:** a player's projectile names the player's PlayerState as instigator, as melee attacks do. The AI's last-attacker retarget does not resolve a PlayerState instigator to its pawn; this inherited gap stays a separate fix.
- **No user check yet:** the user validates the whole ranged roadmap at the end (RNG-06).
