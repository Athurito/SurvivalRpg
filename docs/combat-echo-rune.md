# CMB-07: Echo rune

Task report for CMB-07 in the [combat roadmap](combat-roadmap.md). The first rune of the [game vision](game-vision.md) (section 8.4, "every third attack produces a ghost strike") becomes gameplay: a test sword carries the Echo rune, every third swing that lands a hit charges a ghost strike, and 0.2 s later the swing repeats as an ash afterimage that deals damage. The [VFX-02](vfx-echo-rune.md) presentation, so far triggered by hand in the gallery, now follows the real charge and strike.

## Design decisions (user, 2026-10-26)

- **Damage:** about 25 per ghost strike, its own value, independent of the triggering hit.
- **Targets:** the ghost strike replays the third swing's trace on the same path and hits everything that trace would hit, several enemies included, each once.
- **Charge:** only swings that land a hit count; a swing into the air does not. Several enemies hit by one swing count once.
- **Item:** the rune sits fixed on a separate test sword; the basic sword is unchanged. Rune slots come later.
- **Who:** players only; enemies get no runes.
- **Defaults** (proposed and accepted): the charge expires after 5 s without a hit and when the weapon is unequipped. The test sword lies as a pickup next to the player starts of `Lvl_CombatSandbox`.

## C++ boundary decision

- **Classification:** new item semantics (runes engraved on a weapon) and a new reusable combat mechanism (a delayed replay of a swing's recorded traces), plus designer content (the Echo rune, the test sword, its cues and presentation).
- **Runtime truth:** the server.
  - The attack ability already owns traces and damage there. It records the traces of each swing and reports the swing's outcome to its weapon instance.
  - The weapon instance, the runtime object of the equipped item instance, owns the rune charge and the ghost strike. The charge replicates to every client for the rune glow; the ghost strike's damage goes through the owner's ability system like any hit.
  - Presentation is cosmetic: the glow from the replicated charge, flare and afterimage from a server gameplay cue.
- **Existing seams:**
  - Native inventory fragments on item definitions.
  - The equipment instance's replicated instigator (the item instance) and spawned actors.
  - `URpgGameplayAbility_BasicWeaponAttack`'s authority attack windows, their trace queries, and its damage spec path (SetByCaller damage, hit cue tag, hit result in the context).
  - Gameplay cues as in CMB-04.
  - The VFX-02 assets in `GF_Runes_Core` (`MI_Rune_Echo_Overlay`, `NS_Rune_Echo_Smoulder`, `NS_Rune_Echo_Flare`, `NS_Rune_Echo_Ghost`).
- **Native, and why:**
  - **Schema:**
    - `URpgWeaponRuneDefinition`, a DataAsset for one rune. Its effects are optional settings blocks; the Echo block holds hits to trigger, delay, damage, damage effect, charge timeout and strike cue. Later runes add blocks; no rune has its own class.
    - `URpgInventoryFragment_WeaponRunes`, the runes engraved on an item definition. Rune slots on the item instance can add to it later.
  - **Mechanism:**
    - The attack ability records the trace queries of each authority window with their time offsets and reports them to the weapon instance when the window closes.
    - The weapon instance counts charge per swing that landed a hit, expires it, and replays the third swing's queries 0.2 s later at their original offsets, with its own damage spec and its own once-per-target set.
    - These are authority, timing and trace paths inside an existing native ability; the replay must outlive the attack ability, which a dodge can cancel during the delay.
  - **Presentation mechanism:** `URpgWeaponSwingRecorderComponent` keeps the weapon mesh's recent transforms on each client and plays an afterimage effect along them with a delay. The ghost then follows each client's own animation of the swing; a per-frame transform history is engine-facing and reusable.
  - **New native classes:** two schema classes, one presentation component, plus plain structs for the recorded swing and the charge rule. No GameplayAbility or actor leaf.
- **Content (Unreal MCP, in `GF_Runes_Core`):**
  - `DA_Rune_Echo`.
  - The test sword: item, equipment definition, weapon instance and actor, derived from the basic sword. The actor carries the rune overlay, the smoulder effect and the recorder; the instance sets the glow from the charge.
  - The strike cue with flare and afterimage, and a GameplayCue path action.
  - A pickup in `Lvl_CombatSandbox`.
  - `GF_Runes_Core` was to be activated by the Experiences that activate `GF_Combat_Core`. During implementation that proved unnecessary; see [Content](#content).
- **Tests:**
  - the charge rule: only hitting swings count, triggering at three, timeout and reset;
  - the replay schedule: queries come due at their offsets after the delay;
  - one listen-server PIE test: a remote client's third hitting swing damages the target a second time after the delay;
  - an asset contract: the test sword carries a rune with an Echo block and a damage effect.

## Behavior

### Problem

The Echo rune existed only as the VFX-02 gallery demo. No weapon could carry a rune, and no gameplay counted hits or dealt a ghost strike.

### Change

- **Charge:** with an Echo rune on the weapon, every authority attack window counts as one swing.
  - A swing that deals health damage to at least one target adds one charge; several targets still count once. A swing into the air adds nothing.
  - The server owns the charge on the weapon instance (`URpgWeaponInstance::EchoCharge`) and replicates it to every machine for the rune glow.
- **Trigger:** the third hitting swing empties the charge and triggers the ghost strike.
- **Ghost strike:** 0.2 s after that swing's attack window opened, the server replays the window's recorded trace queries (line or shape sweeps on the weapon channel) at their original offsets.
  - Every target the queries hit takes 25 damage, once per strike, through `GE_Damage_Instant` with `Damage.Type.Melee` (so blocks apply) and the `GameplayCue.Combat.Hit` impact cue.
  - The weapon instance runs the replay, so a dodge that cancels the attack during the delay does not stop it.
  - The replay hits whatever stands in the recorded path when it runs.
- **Expiry:** 5 s without a hitting swing, or unequipping the weapon, empties the charge.
- **Presentation:**
  - **Glow:** the test sword's weapon instance passes the replicated charge to its sword actor, which eases the rune overlay (Custom Primitive Data 0) and the ember rate toward it, as in the VFX-02 demo.
  - **Strike cue:** the server executes `GameplayCue.Rune.Echo.Strike` on the wielder. `GCN_Rune_Echo_Strike` then plays the rune flare (Custom Primitive Data 1 and the flare burst) on the sword and the ash afterimage.
  - **Afterimage:** `URpgWeaponSwingRecorderComponent` on the sword actor keeps the last second of the blade's transforms on each machine. The cue moves a follower along that history, as far behind as the ghost runs (at least 0.2 s), and attaches `NS_Rune_Echo_Ghost` to it. Every machine therefore repeats its own animation of the swing; no path is replicated.
- **Item:** the rune sits on the test sword's item definition (`URpgInventoryFragment_WeaponRunes`); the weapon instance reads it from its item instance on equip. The basic sword is unchanged. Rune slots on item instances are later work.

### Content

| Asset | Content |
| --- | --- |
| `GF_Runes_Core/Runes/DA_Rune_Echo` | Echo rune: 3 hitting swings, 0.2 s delay, 5 s timeout, 25 damage, no stagger, `GE_Damage_Instant`, `Damage.Type.Melee`, hit cue `GameplayCue.Combat.Hit`, strike cue `GameplayCue.Rune.Echo.Strike` |
| `GF_Runes_Core/Weapons/ID_EchoTestSword` | Item "Echo Test Sword": equippable (`ED_EchoTestSword`), UI, trait and grid values of the basic sword, and the runes fragment with `DA_Rune_Echo` |
| `ED_EchoTestSword` (child of `ED_BasicSword`) | Same slots and ability sets; spawns `BP_EchoTestSwordActor` and uses `BP_WeaponInstance_EchoTestSword` |
| `BP_WeaponInstance_EchoTestSword` (child of `BP_WeaponInstance_BasicSword`) | Same attacks; `On Echo Charge Changed` forwards the charge to its sword actor |
| `BP_EchoTestSwordActor` (child of `BP_BasicSwordActor`) | `MI_Rune_Echo_Overlay` on the blade, `NS_Rune_Echo_Smoulder`, the swing recorder; eases the glow (`SetEchoCharge`, `FlareRunes`, `UpdateRuneGlow`) |
| `BP_EchoTestSwordPickup` (child of `BP_BasicSwordPickup`) | The item, shown as the sword with lit runes instead of the placeholder cube |
| `GF_Runes_Core/GameplayCues/GCN_Rune_Echo_Strike` | Burst cue: flare on the sword, afterimage on the recorder's follower |
| `GF_Runes_Core` GameFeatureData | Gameplay cue path `/GameplayCues` |
| `GF_Runes_Core.uplugin` | Depends on `GF_Combat_Core` |
| `GF_Dev_Sandbox/Maps/Lvl_CombatSandbox` | One pickup in front of each of the three player starts |
| `Config/DefaultGameplayTags.ini` | `GameplayCue.Rune.Echo.Strike` |

Changes to the plan:
- **No Experience activation:** `GF_Runes_Core` has no action that needs the Active state. Its cue path registers when the feature registers, its built-in initial state, and `GF_Dev_Sandbox` already depends on it.
- **Item definition:** `ID_EchoTestSword` is built from the native item definition with the basic sword's fragment values, not as a Blueprint child of `ID_BasicSword`, which cannot be saved (see Failed attempts). It has no itemization fragment, so the test sword rolls no stats.
- **Three pickups** instead of one, so every player of a multiplayer test can take a sword.

## Validation

Run on 2026-10-26 on `master` `9cb6f18d` plus this change.

- **Build:** `ue.py build` passed, 0 errors, 0 warnings.
- **Assets:** data validation of all 12 `GF_Runes_Core` assets and `Lvl_CombatSandbox`: 13 valid, 0 warnings. Their package dependencies reach only `GF_Combat_Core`, core content and `GF_Runes_Core` itself.
- **New tests**, all passed:
  - `SurvivalRpg.Combat.Runes.Echo.ChargeCountsOnlyHittingSwings`
  - `SurvivalRpg.Combat.Runes.Echo.ChargeExpiresAfterTheTimeout`
  - `SurvivalRpg.Combat.Runes.Echo.ReplayRunsTracesAtTheirOffsets`
  - `SurvivalRpg.Combat.Runes.Echo.DefinitionNeedsEffectAndDamage`
  - `SurvivalRpg.Combat.Runes.EchoTestSwordAssetContract`: the test sword carries an enabled Echo rune with a damage effect, damage and a strike cue, equips as a weapon instance, and its actor has the swing recorder.
  - `SurvivalRpg.Network.CombatRemoteMeleePIE.RemoteClientEchoRuneReplaysEveryThirdHittingSwing`: on a listen server, a remote client's swings with a test rune give charge 1, 2, then 0, which the owner receives each time. The third swing damages the target twice; the second drop is exactly the rune's 25 plus the source's base damage. A fourth swing starts a new charge.
- **Regression** (`ue.py test`, own editor closed, test editor's MCP server off):

  | Suite | Result |
  | --- | --- |
  | `SurvivalRpg.Combat` (with the new rune tests), `.Interaction` | 81 / 81 |
  | `SurvivalRpg.Inventory.ItemDefinitions`, `.Equipment` | 22 / 22 |
  | `SurvivalRpg.Network.CombatRemoteMeleePIE`, before the content | 4 / 5, rerun 5 / 5 (see Failed attempts) |
  | `SurvivalRpg.Network`, after the content | 28 / 29; the failed combo test passed alone 3 / 3 (see Failed attempts) |

- **PIE, standalone, `Lvl_CombatSandbox`:**
  - **Pickup:** with the basic sword in hand, `F` at the pickup showed "1× Echo Test Sword" and removed the pickup; the item stayed in the inventory. Equipped to the main hand through the inventory's equipment intent, the hand held `BP_WeaponInstance_EchoTestSword` with `BP_EchoTestSwordActor`.
  - **Three swings at the frozen warrior:** charge 1, 2, 0; the warrior's health went 100, 75, 50, then 0. The log showed "strike triggered: 228 traces replay … 0.200 s after the swing", and 56 ms later "ghost hit BP_RiftGrunt_Sword_C_0".
  - **Captures of the third swing:** flare sparks on the blade, ash at the warrior and an ash trail along the arc, then the warrior collapsing.
  - **Glow and timeout:** after one hit the sword actor's target charge was 1 and its shown glow eased to 1.0. About 5 s after the hit the charge and the glow returned to 0.
- **PIE, listen server with two clients:** the remote client picked up the sword with `F` and swung with the left mouse button.
  - The charge read 1, 2, 0 on the server, on the owner (autonomous proxy) and on the other client (simulated proxy).
  - The warrior's health on all three went 100, 75, 50, 25, 0; the last 25 was the ghost strike.
  - After the trigger the sword actor on all three machines had the afterimage follower with the ghost effect attached, so the strike cue played everywhere; the effect was released afterwards.
- **Failed attempts:**
  - **Item definition as a Blueprint child:** `ID_EchoTestSword` as a child of `ID_BasicSword` failed to save with "Illegal reference to private object". Its inline fragments kept the parent's fragments as archetypes, which are private objects in another package. Reparented to the native item definition and rebuilt with new fragments; the pitfall is in the [authoring reference](../.agents/skills/unreal-lyra-expert/references/unreal-mcp-asset-authoring.md).
  - **Remote melee test, first run:** `RemoteClientAttackWindowDamageAndCancellation` timed out on its 21st attack, the one at 1.5× montage speed: the window traced 8 samples and hit nothing, not even the floor. That attack runs without a rune. The rerun of the class passed 5 / 5; the test was already known to fail now and then ([test floor race](gasp-test-floor-race.md)).
  - **Combo test in the full network suite:** `RemoteClientComboChainsEveryStepOnOwnerAndServer` timed out waiting for the owner's fourth step. The owner's third-step montage ended right after it started (`MontageEnded.ClientNotifyStartMissing` at position 0), which ended the combo. No rune is involved there, and the recording returns at once without one. The test passed in both class runs and alone 3 / 3.
  - **Freezing the warrior for probes:** stopping its StateTree right after the PIE start did not hold, and the warrior killed the player twice. A stop a few seconds later plus removing its attack drivers worked; unpossessing it or giving the player `State.Evading` did not. Recorded in the [editor session reference](../.agents/skills/unreal-lyra-expert/references/editor-session-and-pie.md).
  - **Graph cleanup:** writing the actor's tick event left the template's `Parent: Tick` node behind without an exec input; it was deleted.

## Playtest

1. Open `GF_Dev_Sandbox/Maps/Lvl_CombatSandbox` and start PIE. In front of each player start lies a sword with three glowing runes. Look at it from about 2 m and press `F`: "1× Echo Test Sword".
2. Open the inventory (`Tab`) and put the Echo Test Sword into the main hand; it stays in the inventory while the basic sword holds the hand.
3. Hit the skeleton: each hit lights one more rune, and embers rise off the blade. Swings into the air do not light a rune.
4. The third hit flares the runes with sparks. 0.2 s later an ash afterimage repeats the swing, and the skeleton takes a second hit of 25 with a hit reaction.
5. Land one or two hits and wait 5 s: the runes go dark again, and the next third hit needs three new hits.
6. **Multiplayer:** listen server with two clients. Pick up and equip the sword as a client and hit the skeleton three times. The other client sees the glow, the flare and the afterimage on your sword, and both see the skeleton's health drop twice on the third hit.

## Limits

- **Afterimage under latency:** every machine replays its own animation, behind by the ghost's lag measured on the server. With latency the cue reaches the owner about one round trip after its predicted swing, so the owner's afterimage starts later in the arc; this was not measured with packet lag.
- **Glow after a late spawn:** a client that receives the charge before the sword actor, for example after late join, shows no glow until the next charge change.
- **Fixed ghost damage:** 25 plus the wielder's base damage, independent of the combo step and of rolled stats. The ghost can be blocked like any melee hit.
- **Flare for other weapons:** the cue sets the rune flare on `BP_EchoTestSwordActor`; a second Echo weapon needs a shared Blueprint parent for the glow. The afterimage works on any weapon actor with the recorder.
- **Strike cue delivery:** executed cues travel as unreliable multicasts, so under packet loss a machine can miss the flare and afterimage; damage is unaffected.
- **Graph comments:** the Blueprint graphs are split into named functions with variable tooltips; the 5.8 tools cannot add comment boxes.
- **Not checked:** late join, packet lag or loss, a dedicated server, the Mover variant (`SurvivalRpg.GASP.Mover`), and a cook of the sandbox map.
