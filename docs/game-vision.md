# Game Vision - Dark Fantasy Survival RPG

## 1. Projektziel

Dieses Projekt ist ein **Open World Survival Action RPG** im **Medieval Dark Fantasy Setting**.

Die Vision ist es, ein Spiel zu erschaffen, das die stärksten Elemente aus Spielen wie **Valheim**, **Enshrouded**, **New World** und modernen **Rune-/Utility-Systemen** aufgreift, aber daraus eine **eigene Identität** formt.

Der Fokus liegt auf:

- belohnendem, skillbasiertem Combat
- einer motivierenden Survival- und Crafting-Loop
- langfristig relevanten Ressourcen ohne Wegwerf-Progression
- hoher Wiederspielbarkeit durch dynamische Welt- und Portalereignisse

Dieses Dokument ist **kein vollständiges GDD**, sondern eine **klare Vision und Systemorientierung für Entwicklung und technische Umsetzung**.

---

## 2. High Concept

Die Spielwelt ist eine düstere mittelalterliche Fantasy-Welt, die von **Portalen in fremde Reiche** bedroht wird.

Diese Portale erscheinen dynamisch in der Welt und führen in gefährliche andere Dimensionen mit eigenen Gegnern, Ressourcen, Regeln und Bossen. Wenn der Spieler den Boss eines Portals besiegt, kann das Portal geschlossen werden. Bleiben Portale zu lange offen, kann es zu einem **Dungeon Break** kommen: Feinde und Korruption breiten sich in die Oberwelt aus und verändern Regionen aktiv.

Der Spieler sammelt Ressourcen, überlebt in der Welt, baut und verbessert seine Basis, entwickelt Waffen- und Runenfähigkeiten weiter und entscheidet, wie mit dieser Bedrohung umzugehen ist.

---

## 3. Core Vision Statement

**Das Spiel soll sich anfühlen wie das Überleben und Meistern einer lebenden Welt, die durch Portale aktiv verändert wird.**

Der Spieler soll nicht nur stärker werden, sondern die Welt verstehen, kontrollieren, stabilisieren oder bewusst riskanter machen, um bessere Belohnungen zu erhalten.

---

## 4. Design-Ziele

### Primäre Ziele

- Combat soll direkt, präzise und belohnend sein.
- Gathering und Crafting sollen befriedigend wirken und nicht wie Arbeit.
- Survival soll Spannung erzeugen, aber nicht nerven.
- Alte Ressourcen und Ausrüstung sollen auch im späteren Spiel relevant bleiben.
- Die Welt soll dynamisch und reaktiv sein.
- Builds aus Waffen, Magie und Runen sollen echten Spielstil-Unterschied erzeugen.

### Sekundäre Ziele

- Base Building als funktionaler Progressions-Hub
- hohe Wiederspielbarkeit durch variable Welt- und Eventstruktur
- gute technische Skalierbarkeit für spätere Content-Erweiterungen

---

## 5. Was das Spiel nicht sein soll

- kein reines Soulslike
- kein reines Sandbox-Bauspiel
- kein MMO
- keine überkomplexe Survival-Simulation mit permanentem Micromanagement
- kein System, in dem Early-Game-Content schnell wertlos wird
- kein Crafting-System, das nur linear höhere Tier-Stufen ersetzt

---

## 6. Player Fantasy

Der Spieler soll mehrere Fantasien gleichzeitig erfüllen können:

- Überlebender in einer feindlichen dunklen Welt
- Jäger von Portalbestien
- Runenschmied / magischer Handwerker
- Build-Crafter mit eigener Kampfweise
- Entdecker gefährlicher Regionen und fremder Reiche
- Verteidiger oder Manipulator einer sich wandelnden Welt

---

## 7. Core Gameplay Loop

### Macro Loop

1. Welt erkunden
2. Ressourcen sammeln
3. Crafting-Materialien verarbeiten
4. Ausrüstung, Stationen, Runen und Build verbessern
5. Portale / Dungeons / Events angehen
6. Boss besiegen oder Event abschließen
7. Weltzustand verändert sich
8. neue Ziele, Gefahren und Chancen entstehen

### Micro Loop

1. Vorbereitung in der Basis
2. Auszug in eine Region
3. Ressourcen und Gegnerbegegnungen
4. Loot, Essenzen, Rezepte oder Runen sichern
5. Rückkehr zur Basis
6. Verarbeitung und nächste Zielsetzung

---

## 8. Design-Säulen

### 8.1 Combat First

Das Combat-System ist ein Hauptträger des Spiels.

Wichtige Merkmale:

- Nahkampf, Fernkampf und Magie
- responsive Steuerung
- klare Trefferreaktionen
- Stagger- und Poise-System
- Perfect Block / Parry als belohnender Skill-Moment
- Waffen mit eigenem Spielgefühl
- aktive Skills und Combos über Weapon Mastery

### 8.2 Survival Without Friction

Survival soll relevant sein, aber den Spielfluss nicht ständig unterbrechen.

Wichtige Merkmale:

- Nahrung als Vorbereitung und Buff-System statt als Dauerpflicht
- Temperatur, Korruption oder Umweltgefahren nur dort relevant, wo sie Spannung erzeugen
- Rested-/Camp-/Basis-Boni als Motivation für Planung und Rückkehr

### 8.3 Lasting Economy

Ressourcen sollen langfristig Bedeutung behalten.

Wichtige Merkmale:

- Recycling / Scrapping alter Ausrüstung
- Materialveredelung statt kompletter Entwertung alter Rohstoffe
- Low- und Mid-Tier-Materialien bleiben in High-Tier-Rezepten nutzbar
- alte Biome und Regionen behalten Wert

### 8.4 Dynamic World Threat

Die Welt ist kein statischer Hintergrund.

Wichtige Merkmale:

- Portale erscheinen dynamisch
- offene Portale beeinflussen Regionen
- Dungeon Breaks können Feinde in die Oberwelt bringen
- Weltzustand verändert Ressourcen, Gegner, Risiko und Belohnungen

### 8.5 Replayability Through Variation

Wiederspielbarkeit entsteht durch Variation, nicht nur durch Grind.

Wichtige Merkmale:

- variable Weltstruktur oder hybride prozedurale Elemente
- wechselnde Portaltypen und Eventkombinationen
- unterschiedliche Build-Pfade
- alternative Entscheidungen im Umgang mit Bedrohungen

---

## 9. Kernsysteme

## 9.1 Combat System

### Ziele

- direkte Kontrolle
- hohes Trefferfeedback
- defensive Skill-Momente mit offensiver Belohnung
- Hybrid-Builds unterstützen

### Kernmechaniken

- Light Attack
- Heavy Attack
- Charged Attack
- Dodge / Evade
- Block
- Perfect Block / Parry
- Stagger
- Weakpoint Damage
- Status Effects
- Weapon Skills
- Rune Skills

### Combat-Prinzip

Ein guter Spieler soll durch Timing, Positionierung und Build-Synergien deutlich effektiver sein als durch reine Itemstärke.

### Beispiel

Perfect Block gegen geeigneten Angriff erzeugt ein kurzes Stagger-Fenster. Innerhalb dieses Fensters verursachen bestimmte Waffen-Skills oder Combos erhöhten Schaden oder spezielle Reaktionen.

---

## 9.2 Weapon Mastery

Jede Waffenklasse besitzt einen eigenen Fortschritt und Skillbaum.

### Ziele

- Waffen sollen sich stark voneinander unterscheiden
- Skilltrees sollen Spielstil ändern, nicht nur Zahlen erhöhen
- Freischaltungen sollen neue Skills, Combos, Passives und Rollen bieten

### Beispielhafte Waffenklassen

- Schwert
- Schwert + Schild
- Zweihandwaffe
- Axt
- Speer
- Bogen
- Stab / Fokuswaffe

### Erwartete Inhalte pro Waffenklasse

- Basiskombo
- Spezialskill 1
- Spezialskill 2
- passiver Pfad
- Mastery-Fähigkeiten
- Upgrade-/Variantensynergien

---

## 9.3 Magie

Magie ist nicht nur ein Kampfwerkzeug, sondern ein System für Kampf, Utility und Weltinteraktion.

### Rollen der Magie

1. Combat Magic
2. Utility Magic
3. World Magic

### Beispiele

- Projektilzauber
- Crowd Control
- Waffenverzauberung
- Erzschmelzen beschleunigen
- Ressourcen aufspüren
- Portale analysieren oder stabilisieren
- Korruption reinigen oder nutzen

---

## 9.4 Runensystem

Das Runensystem ist ein zentrales Alleinstellungsmerkmal.

### Ziele

- Runen sollen Gathering, Crafting, Combat und Weltinteraktion verbessern
- Runen sollen modular, upgradebar und kombinierbar sein
- Runen sollen neue Problemlösungen ermöglichen statt nur Schadenszauber zu sein

### Kategorien

#### Tool Runes

- Baumfällen effizienter
- Erzabbau beschleunigen
- Ressourcen einsammeln
- Schmelzprozesse verstärken

#### Combat Runes

- Burst-Fähigkeiten
- Movement-Skills
- Crowd Control
- Elementeffekte

#### World Runes

- Schutzkreise
- Portalkontrolle
- Wetter- oder Korruptionsinteraktion
- Basisverstärkung

### Beispielideen

- Astralaxt: geworfenes Werkzeug für mehrere Bäume
- Seismischer Schlag: bricht Erzadern oder Steinflächen
- Glutzeichen: beschleunigt Ofen oder Schmelze
- Aetherzug: zieht Drops und Ressourcen in Radius an

---

## 9.5 Gathering

Gathering muss befriedigend und ruhig lesbar sein.

### Ziele

- gute audiovisuelle Rückmeldung
- effiziente Tools und Rune-Synergien
- kein ständiges Unterbrechen durch übermäßige Aggro
- Skill- und Fortschrittsgefühl beim Sammeln

### Design-Prinzipien

- Mining / Logging / Harvesting sollen eigene Fortschrittswerte haben
- Schwachstellen oder Timing-Elemente können Yield verbessern
- Gefahr entsteht eher durch regionale Bedrohung und Lärmaufbau statt durch permanentes Spawnen von Feinden

---

## 9.6 Crafting & Verarbeitung

Crafting soll eine zentrale Langzeitmotivation sein.

### Stationen

- Werkbank
- Schmiede
- Schmelze
- Gerberei
- Küche
- Alchemietisch
- Runenschrein
- Verzauberungsaltar

### Ziele

- sichtbarer Fortschritt durch Stations-Upgrades
- Materialveredelung statt harter Tier-Ersetzung
- Ressourcenrecycling
- optionale magische Beschleunigung oder Veredelung

### Beispielideen

- Magie kann Schmelzprozesse beschleunigen
- Runen können Materialverlust senken
- Upgrades schalten neue Konvertierungen frei
- Endgame-Rezepte benötigen weiterhin frühere Materialien in veredelter Form

---

## 9.7 Ressourcensystem / Anti-Obsolescence

Eines der wichtigsten Designziele ist die Vermeidung wertloser Inhalte.

### Regeln

- Alte Ausrüstung kann zerlegt werden
- Zerlegte Ausrüstung gibt Basismaterialien teilweise zurück
- Niedrigere Materialien können veredelt oder in Rezepte eingebunden werden
- High-Tier-Rezepte nutzen gemischte Materialstufen
- Regionen behalten durch Spezialmaterialien langfristigen Wert

### Ziele

- keine toten Ressourcen
- keine toten Biome
- kontinuierlicher Nutzen alter Inhalte

---

## 9.8 Survival

Survival soll Vorbereitung vertiefen, nicht bestrafen.

### Gewünschte Philosophie

- Survival erzeugt Relevanz
- Survival unterbricht nicht permanent
- Buff-orientierter statt Straf-orientierter Ansatz

### Beispiele

- Essen erhöht Werte und Regeneration, leerer Magen macht ineffizient, aber nicht sofort hilflos
- Wetter oder Kälte sind in manchen Regionen relevant
- Korruption wird in bestimmten Zonen oder Portalen zum Vorbereitungsthema
- Basis und Lagerfeuer geben starke Rückkehr- und Ruhe-Boni

---

## 9.9 Portale und Dungeon Breaks

Portale sind das strukturelle Herzstück des Spiels.

### Portal-Funktion

- erscheinen in der Oberwelt
- führen in fremde Subwelten / Dungeons
- besitzen eigene Gegner, Materialien und Bosse
- können geschlossen oder eventuell kontrolliert werden

### Dungeon Break

Wenn ein Portal zu lange offen bleibt oder bestimmte Bedingungen erfüllt, bricht dessen Bedrohung in die Oberwelt über.

### Folgen eines Dungeon Breaks

- stärkere Feinde in betroffener Region
- veränderte Umweltbedingungen
- korrumpierte Ressourcen oder neue seltene Drops
- höheres Risiko, aber höhere Belohnungen

### Ziel

Portale sollen nicht nur Content-Container sein, sondern aktiv auf die Welt zurückwirken.

---

## 9.10 Basis / Hub

Base Building ist optionaler Ausdruck, aber funktional zentral.

### Rolle der Basis

- Crafting-Hub
- Verarbeitungszentrum
- sichere Vorbereitung
- Lagerung
- Buff-Quelle
- Verteidigungsraum gegen Weltbedrohung

### Design-Prinzipien

- Stationen und Upgrades klar sichtbar
- Basis bietet spielrelevante Vorteile
- dekoratives Bauen möglich, aber nicht alleiniger Fokus

---

## 10. Weltstruktur

Die Welt soll möglichst stark nach Open World wirken, aber technisch kontrollierbar bleiben.

### Empfohlene Richtung

**Hybrid aus handgebauter Welt und variablen/prozeduralen Elementen**

### Handgebaut

- Biome
- Hauptorte
- Landmarken
- Lore-relevante Zonen
- wichtige Bossräume

### Variabel

- Portalspawns
- Eventspawns
- Ressourcencluster
- Elite-Begegnungen
- regionale Modifikatoren
- Wetter- oder Korruptionsereignisse

### Warum

Vollständige prozedurale Generierung erhöht Wiederspielwert, aber auch Entwicklungsrisiko und Komplexität stark. Eine hybride Struktur ist realistischer und kontrollierbarer.

---

## 11. Wiederspielwert

Wiederspielbarkeit entsteht durch Kombination mehrerer Faktoren:

- unterschiedliche Builds
- wechselnde Portale und Weltzustände
- variable Ressourcenverteilung
- veränderliche Bedrohungszonen
- alternative Prioritäten zwischen Risiko, Kontrolle und Expansion

### Ziel

Jeder Spieldurchlauf soll ein anderes strategisches Gefühl erzeugen, ohne die Kernidentität zu verlieren.

---

## 12. Technische Leitidee für Umsetzung

### Engine

Unreal Engine

### Bestehende Grundlage

Projekt im Lyra-Stil mit GAS als Kern für Fähigkeiten und Statussysteme.

### Architektur-Ziele

- modular erweiterbare Ability- und Combat-Struktur
- datengetriebene Waffen, Runen, Gegner und Rezepte
- Welt-Events als systemische Trigger statt harte Scripte, wo sinnvoll
- gute Grundlage für spätere Content-Skalierung

### Geeignete Systemaufteilung

- Combat über GAS-Abilities, Effects und Tags
- Weapon Mastery datengetrieben über Skillnodes / Unlocks
- Portale als World Events / Encounter Definitions
- Crafting-Rezepte datengetrieben
- Ressourcen- und Stationssystem modular
- Runen als Equipment/Ability-Hybrid

---

## 13. Prioritäten für frühe Entwicklung

### Fokus für Vertical Slice

1. Combat-Grundgefühl
2. Gathering + Crafting Loop
3. eine funktionierende Basis / Hub-Struktur
4. ein Portal mit Boss und Abschlusszustand
5. erste Progression über Weapon Mastery oder Rune-System

### Was zuerst bewiesen werden muss

- macht der Kampf Spaß?
- fühlt sich Gathering gut an?
- ist der Portal-Loop spannend?
- ergänzt Survival die Loop statt zu nerven?
- funktioniert der Ressourcenkreislauf ohne Obsolescence?

---

## 14. MVP-Empfehlung

### MVP-Inhalt

- 1 kleine Open-World-Region
- 1 Basis-Hub
- 3-4 Ressourcenarten
- 2-3 Craftingstationen
- 2 Waffenklassen
- 1 Magietyp
- 2-3 Runen
- 3 normale Gegner
- 1 Elite-Gegner
- 1 Portaltyp
- 1 Boss
- rudimentäres Recycling-System

### Zweck des MVP

Nicht Content-Menge beweisen, sondern die Kernidentität des Spiels.

---

## 15. USP / Alleinstellungsmerkmale

### 1. Portale verändern die Oberwelt aktiv

Portale sind nicht nur Dungeons, sondern Bedrohungsquellen mit Weltwirkung.

### 2. Runen als echtes Utility-System

Runen dienen nicht nur dem Kampf, sondern auch Gathering, Crafting, Reisen und Weltkontrolle.

### 3. Keine toten Ressourcen

Die Ökonomie des Spiels ist auf Wiederverwertung, Veredelung und Langzeitrelevanz ausgelegt.

### 4. Survival ohne ständigen Frust

Survival ist spürbar, aber nicht als permanenter Strafmechanismus gestaltet.

---

## 16. Kurzfassung für Coding Agent

### Ziel

Baue ein modulares Survival Action RPG mit Fokus auf Combat, Crafting, Portale und nachhaltige Progression.

### First Playable Anforderungen

- Third-Person Character Controller
- Melee Combat mit Block / Perfect Block / Stagger
- einfache Resource Nodes zum Abbauen
- Inventar und einfache Item-Datenstruktur
- Craftingstation mit 2-3 Rezepten
- Portal-Encounter mit Teleport in Sublevel
- Boss-Encounter mit Portal-Close-State
- rudimentäre Character- und Weapon-Progression

### Entwicklungsprinzipien

- Systeme modular halten
- datengetrieben arbeiten
- zuerst Fun und Lesbarkeit beweisen, dann skalieren
- keine unnötige Komplexität früh einbauen
- Features so bauen, dass spätere Runen, Waffen, Portale und Rezepte leicht ergänzt werden können

---

## 17. Ein-Satz-Vision

**Ein Dark-Fantasy Survival RPG, in dem Spieler in einer durch Portale bedrohten Welt überleben, kämpfen, craften und eine dynamische Bedrohung kontrollieren, während nahezu jedes System langfristig relevant bleibt.**
