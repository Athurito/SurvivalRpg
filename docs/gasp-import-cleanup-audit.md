# GASP-05 – Import-Abhängigkeiten und Entfernungsliste

Stand: 27.09.2026, **Audit abgeschlossen, [PR #156](https://github.com/Athurito/SurvivalRpg/pull/156) offen**. Branch `codex/gasp-05-import-audit`,
Basis `577d760606be08e84491114db7c8a1eed0c2b43a`. Der begrenzte erste Schritt
erzeugt eine konkrete Paketliste mit Herkunft und aktuellen Referenzbelegen.
Es werden in diesem Audit keine Assets entfernt oder Runtime-Systeme geändert.
Werkzeugcommit: `9c8d7c77c3be6d261a268339096d00cdfe823b82`.

## Zuständigkeit und Methode

Runtime-Wahrheit bleibt in den bestehenden CMC-/Mover-, GAS-, Equipment- und
Lifecycle-Systemen. Experiences/PawnData/GameFeatures komponieren die Varianten;
Blueprints, Chooser, PoseSearch und DataAssets besitzen Inhalte und Darstellung.
Das Editorwerkzeug liest den Asset-Registry-Graph, der Offline-Planer bewertet
Paketmengen. Seine Entscheidung ist keine allgemeine Löschfreigabe.

Die 2138 Foundation-Zuordnungen und 18 zusätzlichen Ragdoll-Zuordnungen bilden
den ersten exakten Kandidatenraum. Alle sonstigen registrierten Pakete bleiben
konservative Schutzwurzeln; Test-/Konfigurationspfade und bestehende Maps sind
zusätzlich zu prüfen. Projektlokale Foundation-Kopien und Engine-/Plugin-
Abhängigkeiten werden nicht allein wegen Sample-Namen zu Löschkandidaten.

## Entscheidung: derzeit keine Entfernung aus der geprüften Liste

**Alle 2156 zugeordneten Originalpakete sind weiterhin referenziert.** Jede der
beiden vorhandenen Karten `Lvl_RpgBaseline` und `Lvl_ThirdPerson` erreicht allein
bereits die gesamte Menge. Das Ergebnis entsteht damit nicht nur durch die
konservative Behandlung anderer Importdateien als Schutzwurzeln.

Die konkrete Entfernungsliste ist aktuell **leer**. Das
[Prüfmanifest](assets/gasp-import-cleanup-audit.json) enthält trotzdem jede der
2156 Quellen, ihr konkretes Ziel, aktuelle SHA-256-Hashes, Herkunft und eine
tatsächliche Baseline-Map-Referenzkette. Jeder Schritt dieser Ketten wurde gegen
den aktuellen Graph geprüft. Quellen und Ziele fehlen in keinem dieser Fälle.

Vier gespeicherte External-Actor-Pakete halten die Originalhüllen im Projekt:

| Karte | External-Actor-Endung | Gespeicherte Originalklasse |
| --- | --- | --- |
| `Lvl_RpgBaseline` | `0/WO/BN4ICY9WZ2JTXADBUILFC8` | `/Game/Blueprints/SandboxCharacter_Mover_Ragdoll` |
| `Lvl_RpgBaseline` | `5/11/0Y71IRXDVEO8I9XLYB8SPV` | `/Game/Levels/LevelPrototyping/LevelBlock_Traversable` |
| `Lvl_ThirdPerson` | `5/J3/4R4XZG0BTYVATG2CYD48YU` | `/Game/Blueprints/SandboxCharacter_Mover_Ragdoll` |
| `Lvl_ThirdPerson` | `E/TT/JYZHXIW3UTAWZLS3AGG853` | `/Game/Levels/LevelPrototyping/LevelBlock_Traversable` |

Die vollständigen Paketnamen und direkten Referenzen stehen unter
`direct_project_blockers` im Manifest. Beispiel:
`Lvl_RpgBaseline` → External Actor `BN4ICY9WZ2JTXADBUILFC8` → originaler
`SandboxCharacter_Mover_Ragdoll` → weitere Original-Blueprints, Animationen,
Chooser, Rigs und Foley. Die damaligen Physics-Control-A/B-Versuche hatten
diese Figuren nur ungespeichert entfernt; der frische Registrybefund bestätigt
ihren weiterhin gespeicherten Bestand.

## Tatsächlich ausgeführte Prüfungen

- Frische UE-5.8.2-MCP-Aufnahme: **13365** registrierte Pakete außerhalb
  `/Engine` und `/Script`, **86576** Paketkanten, inklusive harter/weicher und
  Game-/Editor-only-Referenzen. Alle **6964 getrackten Projektassets** sind
  enthalten, ebenso **78 External-Actor-/Object-Pakete**. Keine fehlende
  External-Actor-/Object-Kante in dieser Aufnahme.
- **0 Package-Identifier-Managementkanten**. Die Python-Paketabfrage erfasst
  ausdrücklich **nicht** den `PrimaryAssetId`-Managementgraph des AssetManagers.
  Null ist daher kein vollständiger Management-Abwesenheitsnachweis. Der
  vorhandene harte/weiche Referenzpfad verhindert die Entfernung unabhängig
  von dieser zusätzlichen Grenze.
- Offline-Planer: **2156 retained_reference**, null fehlende Quellen/Ziele,
  null unreferenzierte Kandidaten. **11209** sonstige Registrypakete bleiben
  Schutzwurzeln. **17/17** sinnvolle Graph-/CLI-Tests bestanden: transitive und
  zyklische Verweise, Managementkanten, fehlende Daten, mehrere Ziele,
  deterministische Pfade und Schutz bestehender Ausgabedateien.
- **2156/2156 Importoriginale** stimmen mit ihren historischen Herkunftshashes
  überein. Genau **18 Zielhashes** unterscheiden sich von der alten Foundation;
  alle entsprechen exakt dem separat dokumentierten MetaSound-ID-Fix aus
  [PR #152](https://github.com/Athurito/SurvivalRpg/pull/152).
- Textaudit: **970 getrackte lesbare Dateien** aus Source, Plugins, Config,
  Build und `.uproject`; **29 explizite GASP-Paketliterale**, ausschließlich
  projektlokal in Editor-Tests/Probe. Keine ausführbaren rohen GASP-Sample-
  Paketadressen im untersuchten Runtime-C++/Plugintext. Das beweist keine
  Abwesenheit beliebiger dynamischer Strings in binären Blueprints.
- Unabhängiger Review von Exporter und Planer: Management-Coverage präzisiert,
  kein weiterer konkreter Blocker. Die erste Aufnahme hatte irrtümlich
  Paketkanten zusätzlich als Managementkanten gezählt. Sie bleibt als
  `registry-20260927.json` erhalten und ist von den Entscheidungen ausgeschlossen;
  die korrigierte Aufnahme stammt aus einem frischen Editorprozess.
- Finale Hashprüfung: **6964/6964 Assets/Maps und 7/7 SaveGames unverändert**.
  Vier Plugin-Overrides verifiziert. Keine Asset-Löschung, kein Save, kein PIE,
  kein neuer C++-Build, Blueprint-Compile oder Cook in diesem Audit. Editor
  sauber beendet; GASP-04-Build-/Netzwerk-/Sichtbelege werden nicht als neue
  Prüfungen ausgegeben.

## Dynamische Routen und noch offene Abnahme

Die Prüfung schützt zusätzlich zu den drei verglichenen GASP-Experiences die
Basis-CMC-, Baseline- und Prototype-Komposition, Menü-/Bootkarten, GameFeatures
und vorhandene Testinhalte. Die optionalen Manny-/Retargeter-Assets werden in
`RpgRuntimeRetargetTests.cpp`, `RpgGaspMoverGameplayTests.cpp`,
`RpgGaspMoverLifecycleTests.cpp` und `RpgGaspMoverRagdollTests.cpp` ausdrücklich
geladen; leere gespeicherte Retarget-Profile beweisen deshalb keine Unbenutztheit.
Der PacketGap-Probe lädt außerdem projektlokale `IA_Move`/`IA_Jump` und die
Mover-Karte. Foundation-Kopien unter `/Game/SurvivalRpg` sind hier keine Kandidaten.

`RpgGameModeBase` wählt Experiences auch über URL, PIE-Override, Kommandozeile
und WorldSettings. Der ExperienceManager lädt PrimaryAssetIds und AssetBundles
und aktiviert GameFeatures per Namen; deren Policy scannt konfigurierte
GameplayCue-Verzeichnisse. `DefaultGame.ini` scannt Experiences/ActionSets als
AlwaysCook und PrimaryAssetLabels unter `/Game`. In dieser Registryaufnahme
ist kein PrimaryAssetLabel-Asset vorhanden. Es gibt keine versionierte
vollständige GASP-Testkarten-Cookliste. `bCookAll=False` und eine selektive
Testauswahl sind kein Cook-Nachweis.

Der breite Graph enthält **70 unbekannte bzw. nicht inventarisierte
Dependency-Ziele**; die vollständige Liste mitsamt Referencern steht im
Manifest. Darunter sind Engine-Plugin-Beispiele/Previewreferenzen und bereits
fehlende Combat-/UI-Inhalte. Sie werden weder als behoben noch pauschal als
neue GASP-Fehler bewertet. Das Audit behauptet keine globale fehlerfreie
Lade-/Cook-Hülle. Die vollständige historische Importmenge ist außerdem größer
als die 2156 zugeordneten Quellen; übrige Dateien brauchen vor einer Entfernung
ihre eigene Herkunfts-/Nutzungsbewertung.

## Nächster begrenzter Schritt innerhalb GASP-05

Die zwei alten Testkarten von ihren vier Original-Demoobjekten entkoppeln,
einschließlich External-Actor-/Object-Paketen. Vor dem konkreten Austausch die
gespeicherten Instanzwerte und das benötigte Demo-Verhalten aufnehmen; einen
platzierten Sample-Pawn nicht blind durch einen besitzbaren RPG-Spieler-Pawn
ersetzen. Aktive Baseline-Experience, Spawns, Persistenzisolation und die
genehmigte Kartenpräsentation erhalten. Blueprint-/Map-Arbeit über Unreal MCP.

Danach den Graph neu erfassen, auch die übrigen historischen Importdateien
klassifizieren und erst eine geschlossene Entfernungsliste bilden. Vor ihrer
Abnahme frisches Laden/Kompilieren und repräsentatives Cooking **ohne** die
entfernten Importoriginale prüfen; Git/LFS sichert den Rückweg. GASP-05 ist mit
diesem Audit noch nicht als gesamte Importbereinigung abgeschlossen. Die
bekannten GASP-02-/GASP-04-Grenzen bleiben erhalten; GASP-06 bleibt zurückgestellt.

## Reproduktion

[Werkzeuganleitung](../Build/Tools/GaspImportAudit/README.md),
Registryexport `Build/Tools/Unreal/gasp_import_audit.py` und Offline-Planer
`Build/Tools/GaspImportAudit/plan.py`. Das versionierte Manifest enthält die
reproduzierbare Zuordnung als `mappings`. Lokale Rohdaten liegen unter
`Saved/GaspImportAudit`; ihre SHA-256-Hashes stehen im Manifest. Andere
Checkouts müssen die Rohaufnahme neu ausführen und dürfen lokale Belege nicht
als vorhanden voraussetzen.
