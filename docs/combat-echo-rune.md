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
  - `GF_Runes_Core` is activated by the Experiences that activate `GF_Combat_Core`.
- **Tests:**
  - the charge rule: only hitting swings count, triggering at three, timeout and reset;
  - the replay schedule: queries come due at their offsets after the delay;
  - one listen-server PIE test: a remote client's third hitting swing damages the target a second time after the delay;
  - an asset contract: the test sword carries a rune with an Echo block and a damage effect.
