# Combat & Mastery Reference

Dieses Dokument beschreibt die aktuelle Combat- und Progression-Idee für das SurvivalRpg-Projekt. Es ist als Referenz für Codex/Agenten gedacht.

---

## 1. Combat-Ziel

Combat soll direkt, lesbar und skillbasiert sein. Der Spieler soll durch Timing, Positionierung, Ausrüstung, Runen und Mastery-Entscheidungen stärker werden, nicht nur durch höhere Itemwerte.

Kerngefühl:

- klassisches Medieval-Fantasy-Combat
- Nahkampf, Bogen und Magie
- Block, Perfect Block, Parry, Dodge
- Stagger / Poise
- Waffen mit eigenem Spielgefühl
- Runen, die Waffenverhalten verändern
- Portal-/Dungeonbreak-Gegner als besondere Bedrohung

---

## 2. First Combat Loop

Der erste spielbare Combat-Loop sollte klein bleiben:

1. Spieler rüstet Testwaffe aus.
2. Input triggert `GA_MeleeAttack`.
3. Montage wird abgespielt.
4. Attack Window startet per AnimNotify oder Ability Task.
5. Trace/Hitbox erkennt Ziel.
6. `GE_Damage_Instant` oder vergleichbarer Damage Effect wird angewendet.
7. Ziel spielt Hit Reaction.
8. Ziel stirbt bei 0 HP.
9. Progression erhält XP/Mastery Event.

Erst wenn dieser Loop gut funktioniert, sollten Runen, Skilltrees, Portale und Spezialwaffen erweitert werden.

---

## 3. Lyra/GAS Architektur

Combat soll über GAS laufen:

- GameplayAbilities für aktive Aktionen
- GameplayEffects für Damage, Kosten, Buffs, Debuffs
- GameplayCues für Trefferfeedback, VFX und SFX
- GameplayTags für Zustände, Input, Waffentypen, Skill-Upgrades
- AbilitySets für Equipment- und Skilltree-Grants

Equipment sollte Fähigkeiten und Effekte aggregieren. Der Character soll nicht die Combat-Wahrheit besitzen.

---

## 4. Equipment als Combat-Quelle

Equipped Items können beitragen:

- Granted Abilities
- Granted Effects
- Granted Tags
- Source Objects
- Damage Data
- Rune Slots
- Trait Tags

Wichtige Tags:

```text
Weapon.Type.Melee
Weapon.Type.Ranged
Weapon.Type.Magic
Weapon.Family.Sword
Weapon.Family.Axe
Weapon.Family.Spear
Weapon.Family.Bow
Trait.Block
Trait.Parry
Trait.ChargedAttack
Trait.RuneSocket
Trait.Harvesting
```

Input sollte über aktive Equipment-Kontexte laufen:

```text
InputTag.Weapon.Primary
InputTag.Weapon.Secondary
InputTag.Ability.Block
InputTag.Ability.Dodge
```

Nicht pro Waffenklasse hart verzweigen, wenn Tags und Daten ausreichen.

---

## 5. Core Combat Abilities

Für `GF_Combat_Core` oder Core Combat:

```text
GA_MeleeAttack_Light
GA_MeleeAttack_Heavy
GA_Block
GA_PerfectBlock
GA_Dodge
GA_HitReact
GA_Death
```

Später:

```text
GA_ChargedAttack
GA_ParryCounter
GA_Execution
GA_WeaponSkill_Primary
GA_WeaponSkill_Secondary
```

---

## 6. Core GameplayEffects

Generisch:

```text
GE_Damage_Instant
GE_Damage_Physical
GE_Damage_OverTime_Base
GE_StaminaCost_Attack
GE_StaminaCost_Block
GE_Stagger_Base
GE_HitReact_Base
GE_PoiseDamage_Base
```

Spezialeffekte gehören in ihre Features:

```text
GE_Damage_Fire_DOT -> GF_Combat_Magic oder GF_Runes_Elemental
GE_Freeze -> GF_Combat_Magic
GE_RiftCorruption -> GF_Dungeonbreak_System
GE_Rune_FlameWeapon -> GF_Runes_Elemental
```

---

## 7. DamageArea Modell

Für Tests darf eine simple `BP_DamageArea_Test` im Core liegen, wenn sie im Viewport platziert werden muss.

Langfristig:

```text
BP_DamageArea_Base
  DamageEffectClass
  Radius
  Duration
  TickInterval
  bApplyOnce
  bIgnoreOwner
  TargetFilterTags
```

Die Base kennt kein Feuer/Rift/Gift. Spezialisierungen kommen aus Features oder DataAssets.

Spawn über:

- GameplayAbility
- AnimNotify
- Portal Actor
- Dungeonbreak Manager
- Encounter Spawner

---

## 8. Combat Mastery Grundidee

Progression ist hybrid:

- Nutzung gibt XP in passenden Masteries.
- einfache Fähigkeiten werden automatisch freigeschaltet.
- Meilensteine geben echte Entscheidungen.
- Runen und Equipment formen Builds zusätzlich.

Wichtig: Waffenwechsel darf nicht bedeuten, bei null anzufangen.

Empfohlene Gewichtung:

```text
70% General Mastery
20% Style / Category Mastery
10% Weapon Familiarity
```

Beispiel:

```text
Melee Mastery Level 20
One-Handed Mastery Level 14
Sword Familiarity Level 6
```

Wechselt der Spieler von Schwert zu Axt, bleiben Melee und One-Handed erhalten. Nur kleine Schwertboni fehlen.

---

## 9. Combat Mastery Tree Beispiel

### Level 1

Light Attack

- einfache Nahkampfangriffe verfügbar

### Level 3

Heavy Attack

- schwerer Angriff verfügbar

### Level 5: erste Entscheidung

Wähle eine Richtung:

**Duelist**

- schneller, mobiler Stil
- nach Dodge mehr Schaden
- später Dodge Counter

**Guardian**

- Block / Perfect Block Fokus
- weniger Staminakosten beim Block
- später Parry Counter

**Berserker**

- aggressiver Stil
- mehr Schaden bei niedrigem Leben
- Kills geben Stamina zurück

### Level 10: Waffenstil

Wähle eine Richtung:

**One-Handed Specialist**

- schnellere Einhandwaffen
- bessere Combo-Finisher
- höhere Rune-Proc-Chance

**Two-Handed Specialist**

- mehr Stagger
- größere Heavy-Attack-Zone
- Bonus gegen gestaggerte Gegner

**Shield Specialist**

- besserer Blockwinkel
- Shield Bash
- Perfect Block reflektiert Schaden oder erzeugt Stagger

### Level 20: Ascension

Wähle eine Richtung:

**Blade Dancer**

- Mobilität, Crits, Dodge-Angriffe

**Rune Knight**

- Nahkampf + Runen-Synergie
- mehr Rune-Procs
- Echo-Schläge

**Warbreaker**

- Stagger, Bossdruck, Haltungsbruch

---

## 10. Weapon Familiarity Beispiel

Weapon Familiarity gibt kleine Boni, aber keine essenziellen Grundskills.

### Sword Familiarity

```text
Level 1: +2% Crit Chance
Level 3: Bonus nach Parry
Level 5: leicht höhere Rune-Proc-Chance
Level 10: Crescent Slash
```

### Axe Familiarity

```text
Level 1: +3% Schaden gegen Rüstung
Level 3: Chance auf Armor Break
Level 5: mehr Stagger bei Heavy Attacks
Level 10: Cleaving Spin
```

### Spear Familiarity

```text
Level 1: +5% Reichweite
Level 3: Bonus gegen anstürmende Gegner
Level 5: Pierce Chance
Level 10: Impaling Thrust
```

---

## 11. Skill Unlock Umsetzung

Aktive Skills sollten AbilitySets grantieren.

Beispiel:

```text
DA_SkillNode_HeavyAttack
  RequiredMastery = Combat Level 3
  GrantedAbilitySet = LAS_Skill_HeavyAttack
```

Passive Skills können GameplayEffects anwenden.

```text
DA_SkillNode_StaminaControl
  GrantedGameplayEffect = GE_Passive_AttackStaminaCostReduction
```

Upgrade-Skills können GameplayTags geben.

```text
Skill.Fireball.LeavesDamageArea
Skill.Melee.HeavyAttack.MoreStagger
Skill.Block.PerfectBlock.CounterWindow
```

Abilities lesen diese Tags und verändern ihr Verhalten.

---

## 12. Rune Knight Integration

Runen sollen Combat sichtbar verändern.

Beispiele:

```text
Flame Rune
- zusätzlicher Feuerschaden
- Chance auf Burn
- später Fire Puddle oder Explosion

Echo Rune
- jeder dritte Treffer erzeugt Echo-Schlag

Instability Rune
- mehr Schaden in Dungeonbreak-Zonen
- Risiko: mehr Korruption oder Instabilität
```

Rune-Effekte sollten über Equipment/Item Instance/AbilitySet/GameplayEffect/Tag-Aggregation in Combat einfließen.

---

## 13. MVP Reihenfolge

1. Basic Weapon Equip
2. Basic Attack Ability mit Montage
3. Damage über GameplayEffect
4. Enemy Health / Death
5. XP oder Mastery Event auf Kill
6. Block oder Dodge
7. Stagger / Hit Reaction
8. erster Skill Unlock
9. erste Rune auf Waffe
10. erstes Portalmonster

---

## 14. Drift vermeiden

Nicht tun:

- Combat direkt im Character hardcoden
- UI als Combat-Truth verwenden
- Loadout als runtime truth behandeln
- Waffenskills nur an statische Klassen binden
- Skilltree so bauen, dass Waffenwechsel Neustart bedeutet
- Spezialeffekte wie Fire/Rift in generische Core-Assets einbauen

Tun:

- Daten, Tags und AbilitySets nutzen
- Equipment als Quelle für Combat-Grants verwenden
- Mastery übergeordnet halten
- Spezialisierungen als Entscheidungen bauen
- Runen und Affixe als spätere Erweiterung einplanen
