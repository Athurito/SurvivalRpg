# RNG-03c: Arrows in AI characters, corpse recovery and the NPC recovery budget

Task report for RNG-03c in the [ranged combat roadmap](ranged-combat-roadmap.md), PR pending. Intact arrows now stay in AI enemies: the shafts show on their bones, follow animation and each machine's ragdoll, and the arrows become ordinary corpse loot when the enemy dies, including arrows that hit the corpse afterwards. Looting them removes their shafts. Unlimited NPC fire returns ammunition only for a finite budget per enemy life. Player characters still never keep arrows (RNG-03b), and the bow enemy itself is RNG-05.

## C++ boundary decision

- **Classification:** reusable native mechanism (exactly-once character records, the corpse loot supplement, ordered credits, bone-relative shaft groups, per-life recovery tokens) and designer data (`MaxVisibleArrowsPerCharacter` on the projectile's recovery data, `UnlimitedRecoveryBudget` on the weapon's shot definition, the shaft mesh from the pickup Blueprint).
- **Runtime truth:**
  - **Living enemy:** one server record per intact arrow on the enemy, made before the arrow's damage applies, so a killing arrow counts. Records are pending entitlements, not items.
  - **Corpse:** from the death loot on, the corpse inventory is the only quantity owner. Each record becomes a credit; every decrease of that ammunition's aggregate quantity in the corpse retires the oldest credits.
  - **Shafts:** presentation only, from a replicated array of bone-relative groups.
  - **NPC budget:** recovery tokens per avatar life on the shooter's ability system; a launched projectile keeps its recovery item regardless of what happens to the shooter afterwards.
- **Existing seams:**
  - `ARpgCombatProjectile` (`ResolveImpact`, the break roll from RNG-03b);
  - `URpgLootSourceComponent` (cached payload and unchanged retries);
  - the corpse inventory's post-commit;
  - `URpgCorpseLifecycleComponent` (Completed/Expiring);
  - `ARpgAICharacter`;
  - `FRpgProjectileShotDefinition` and `RpgProjectileShot::CommitShot`;
  - `URpgAbilitySystemComponent`.
- **Native, and why:**
  - Records, loot hooks and credits are server authority and exactly-once quantity.
  - The shafts are a replicated array instead of one actor per arrow. That keeps network cost bounded, rebuilds the shafts for late joiners, and lets every client attach them to its own skeletal mesh, so they follow its local ragdoll.
  - Tokens must sit in the native shot commit to be reserved and returned with it.
- **New native classes:** one, `URpgEmbeddedProjectileHostComponent`, a default subobject of `ARpgAICharacter`.
- **Deviations from the roadmap's class table:**
  - It planned `ARpgEmbeddedProjectilePickup` as presentation on characters. A component that owns only replicated groups does the same job without a replicated actor per shaft; the pickup stays the world path of RNG-03b.
  - It placed the pending recovery records on the loot source. They live in the host, which owns their shafts, and reach the loot through the loot source's new supplement seam.
  - Records are keyed by a host-local record id, not by shot id. One projectile makes at most one record, so the id carries the same exactly-once guarantee.
- **Data placement:** `MaxVisibleArrowsPerCharacter` is read from the incoming arrow's recovery data, like `MaxSeparatePickupsPerHost` in RNG-03b. Mixed ammunition can therefore exceed the smaller cap.
- **Asset work:** none. The shaft mesh and its offset come from the arrow's pickup Blueprint (`BP_EmbeddedPickup_Arrow`), and the starter values equal the native defaults (`MaxVisibleArrowsPerCharacter` 10). The NPC budget is authored with the bow enemy in RNG-05.

## Behavior

### Problem

After RNG-03b, arrows stayed in the world but never in enemies. An arrow that hit an NPC was gone, and nothing bounded the arrows an NPC's unlimited fire could return.

### Change

- **Records before the damage:**
  - When a recoverable, paid arrow hits a character with `URpgEmbeddedProjectileHostComponent` (every AI character), the projectile makes its one break roll and, if intact, records the arrow before applying its damage.
  - If the impact turns out blocked, the record is withdrawn and the arrow drops to the ground as in RNG-03b. A record that the block's own damage already put into the corpse loot, by killing at once, stays there, so the arrow never exists twice.
  - Player characters have no host and keep nothing. Neither does a character whose loot source builds no death loot, such as the harvest-only `BP_PlaceholderBeast` (`URpgLootSourceComponent::HasDeathLoot`): its arrows behave like player hits.
- **Shaft placement:**
  - A shaft sits on the hit bone. For a capsule hit it sits on the bone nearest the contact, where the flight passes closest to it, `EmbedDepth` inside.
  - Its location and rotation are stored relative to that bone.
  - Without posed bones the root bone is used.
- **Visible maximum:**
  - At most `MaxVisibleArrowsPerCharacter` (10) shafts show.
  - Further arrows join the nearest shaft, preferring the same bone, and raise its count.
  - Every arrow keeps its own record.
- **Presentation:** the groups replicate. Every machine, the listen server included but not a dedicated server, attaches one static mesh per group (the pickup class's display mesh and offset) to its own skeletal mesh at the bone. The shafts follow animation and that machine's ragdoll, also for late joiners.
- **Corpse loot:**
  - When the enemy's death loot payload is first built, `URpgLootSourceComponent` broadcasts `OnGatherSupplementalLoot`, and every pending arrow joins the payload once. A failed delivery retries the same payload.
  - On success the arrows are credited.
  - An arrow that hits the corpse later (its ability system is gone, so the impact is a world hit on a pawn) goes through `AddSupplementalLoot`. It joins the payload if the payload is not committed yet; otherwise it is added to the corpse inventory at once. If the built loot cannot take it, the arrow is lost with its shaft.
  - A completed or expiring corpse takes no arrows.
- **Looting:** arrows of one kind are interchangeable. Whenever the corpse holds fewer of an ammunition than it has credits for, the oldest credits retire with their shaft share; a shaft goes at zero. Moves, splits and merges inside the corpse, taking other loot, or taking the loot table's own arrows of the same kind first retire nothing. When the corpse goes, its arrows and shafts go with it; nothing drops into the world.
- **NPC budget:**
  - `FRpgProjectileShotDefinition::UnlimitedRecoveryBudget` (default 0) gives unlimited fire a number of shots per shooter life whose intact arrow still returns the ammunition.
  - Each successful release of recoverable ammunition reserves a token on the shooter's ability system; ammunition consumed on impact reserves none. A failed release (spawn failure, stamina) gives the token back. Misses, breaks and collection never give it back.
  - A new avatar starts with a fresh budget; its first budgeted shot fixes the budget for that life. Target changes, loadout refreshes and equipment changes refill nothing, and a weapon with a bigger budget does not raise it.
  - The projectile keeps its recovery item, so the shooter's death changes nothing.

## Validation (2026-11-02)

### Commits

- `9ba1c8bc`: the host, corpse supplements, the budget and the native tests.
- `35389429`: the PIE test.
- `7ab59e56`: the review fixes.

### On `35389429`

- **Build:** `python Build/Tools/Unreal/ue.py build` passed.
- **Focused tests:** `SurvivalRpg.Combat.Projectile.Recovery` and `SurvivalRpg.Combat.ProjectileShot`, 24/24.
- **New PIE test:** passed.
- **Broad regression:**
  - Native: `SurvivalRpg.Combat`, `AbilitySystem`, `Inventory`, `Crafting`, `Equipment`, `Harvesting`, `Input`, `Interaction`, `UI`, `Save`, `AI` and `Progression`, 602/603. The one failure is `BlockLocomotionPIE.MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease`, which timed out at the default 45 s command timeout because the run lacked the 90 s network override. Rerun alone with the override, it passed.
  - `SurvivalRpg.Network`: 46/46.

### Review

Two review agents read the diff, one with an authority, exactly-once and lifecycle lens and one with a contract and test lens. Each finding was checked against the code.

**Fixed in `7ab59e56`:**
- consumed-on-impact NPC shots reserved a token, and the old test asserted that;
- harvest-only corpses kept arrows that could never be looted;
- a blocked arrow whose damage killed at once could both stay in the corpse loot and drop;
- an arrow that a built corpse could not take kept its shaft;
- the loot table's own arrows of the same kind retired shafts;
- a weapon with a bigger budget raised the life's budget;
- missing tests for the record before the damage, the AI character's host, and same-bone grouping.

**Recorded as limits:** the shared death payload, ragdoll and late join in PIE, and the cap read from the incoming arrow.

### On `7ab59e56`

- **Build:** passed.
- **Focused native tests:**
  - `SurvivalRpg.Combat.ProjectileShot` and `SurvivalRpg.Combat.Projectile`: 32/32.
  - `SurvivalRpg.Combat.Projectile.Recovery`: 9/9 without warnings.
  - `SurvivalRpg.Loot` and `SurvivalRpg.Interaction.Core.DeathLootVisibility`: 9/9.
- **PIE:** `SurvivalRpg.Network.CombatProjectilePIE` and `SurvivalRpg.Network.RangedAimPIE` with the 90 s override, 9/9.
- **Regression:** `SurvivalRpg.Combat`, `AbilitySystem`, `Inventory`, `AI`, `Interaction`, `Loot` and `Save` with the 90 s override, 345/345.
- **Skills:** `python Build/Tools/AgentSkills/sync.py --check` passed.
- **Not rerun after the fixes:** `Crafting`, `Equipment`, `Harvesting`, `Input`, `UI`, `Progression` and the rest of `SurvivalRpg.Network`. Their code was not changed.
- **No manual playtest** in the editor.

## Playtest

1. Open `GF_Dev_Sandbox/Maps/Lvl_CombatSandbox` (it has an enemy and a navmesh) and start PIE in standalone.
2. In the console, run:
   - `Rpg.Inventory.GrantItem /Game/SurvivalRpg/Inventory/Items/Test/ID_TestBackpack.ID_TestBackpack_C 1 equip`
   - `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Bow.ID_Bow_C 1 equip`
   - `Rpg.Inventory.GrantItem /GF_Combat_Ranged/Items/ID_Arrow_Standard.ID_Arrow_Standard_C 20`
3. Shoot an enemy several times. Expected: intact arrows stay in its body, on the body part they hit, and move with its animation; broken ones vanish.
4. Kill it. Expected: the shafts stay in the ragdoll, also the killing arrow. Shoot the corpse once more: that arrow stays too.
5. Loot the corpse. Expected: the loot lists the arrows (about 70 % of the hits); taking some removes as many shafts, oldest first.
6. Multiplayer: start PIE as a listen server with two players. One player shoots an enemy. Expected: both see the same shafts on the same body parts, also on the ragdoll; only one player can take each arrow from the corpse.

## Limits

- **Not observed in a running game yet.** No test watched a real AI enemy die from an arrow and drop its corpse loot. The native tests drive hits, death loot and looting on fixture pawns, and the PIE test drives a replicated stand-in body directly through the host. The killing arrow is covered by the rule that the record exists before the damage, which a test proves. Playtest steps 3 to 6 and RNG-06 cover the whole chain.
- **No automated check of these:**
  - shafts following animation and each machine's ragdoll;
  - late joiners rebuilding the shafts;
  - a real player transfer out of the corpse with two looters at once;
  - corpse expiry with arrows inside.

  The PIE test proves that every client attaches each shaft to its own skeletal mesh at the group's bone; following the bone is then the engine's attachment.
- **Retry after a failed death loot:** a failed population followed by its retry with queued arrows is covered only by the unchanged-retry contract of `URpgLootSourceComponent`.
- **Same-bone grouping:** proven natively with named bones on a mesh without an asset, not on Manny.
- **One payload for loot and arrows:** the death loot commits the arrows together with the loot table. A corpse inventory too small for both would fail the whole population. AI loot inventories use merged arrow stacks in their default grid, so this needs a nearly full corpse.
- **Full corpse:** an arrow that a full, already-built corpse cannot take is lost with its shaft. Looting does not offer it again.
- **Budget scope:** the NPC recovery budget is per avatar life on the ability system, shared by every weapon of that life. A shot through a different ability system, such as a turret, has its own budget. The real budget for the bow enemy (5) is authored with RNG-05.
