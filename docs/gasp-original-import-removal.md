# GASP-05 – Originalimport entfernen

Stand: 27.09.2026, **Entfernung validiert und [PR #158](https://github.com/Athurito/SurvivalRpg/pull/158) bestätigt gemergt**.
Branch `codex/gasp-05-remove-sample-actors`, Basis
`d6cb927f5654a681bf0c02db551af86276cf916d`. Der Nutzer hat die Entfernung
aller **2298 Original-Assets** zusätzlich zu den vier Sample-Kartenobjekten
ausdrücklich bestätigt. Assetcommit `0c37d9c4b9fb5cb555569c81ea5b10aedaefa4d6`.
[PR #158](https://github.com/Athurito/SurvivalRpg/pull/158), bestätigt gemergt als `051f03f06abf84b82191338a52401dd64ddd6c32` am 2026-09-27T01:16:35Z; finaler geprüfter Head `594b202084737c6e98ad3cc853d3cc613504dcc5`.
Für die anschließende reine Merge-Statuspflege keine Prüfungen erneut ausgeführt.

## Ergebnis und Umfang

Die ursprünglichen GASP-Assets sind entfernt. Die übernommenen Inhalte unter
`/Game/SurvivalRpg/Characters/GASP/` bleiben erhalten. Alle **4662 übrigen
getrackten Assets**, darunter alle **2156 zugeordneten Kopien/Adaptionen**, sind
byteidentisch zur Basis. Alle **sieben persönlichen SaveGames** sind unverändert.
Kein Runtime-C++, keine Konfiguration und keine Projektkopie wurde geändert.

Der [vorangehende Audit](#vorangehender-audit-pr-156) konnte noch keine
Entfernung freigeben. Seine vier direkten Blocker wurden jetzt über Unreal MCP
aus `Lvl_RpgBaseline` und `Lvl_ThirdPerson` entfernt: je ein originaler
`SandboxCharacter_Mover_Ragdoll` und ein `LevelBlock_Traversable`. Andere Actors
referenzierten diese Instanzen weder per Attachment noch über die gelesenen
Properties. Die Demopawns hatten `AutoPossessPlayer=Disabled`; die echten
RPG-Spieler benötigen keinen Ersatz für sie.

Beide Karten enthalten nach frischem Laden jeweils **42 statt 44 Actors**.
PlayerStarts, WorldSettings, Baseline-/Prototype-Experience, andere Geometrie
und Präsentation bleiben erhalten. Von den Kartenpaketen selbst musste kein
Byte geändert werden: Die vier entfernten External-Actor-Dateien tragen diese
Änderung. Im frischen Actorvergleich unterscheiden sich ausschließlich die
neuen GUIDs von sechs prozessgenerierten Hilfsaktoren; gespeicherte Actors und
ihre Eigenschaften/Transforms bleiben gleich.

## Exakte Herkunft und Abhängigkeiten

[Originalinventar](assets/gasp-original-import-inventory.json) und
[Entfernungs-/Prüfmanifest](assets/gasp-original-import-removal.json) halten den
vollständigen Umfang fest. Kandidaten wurden aus zwei tatsächlichen
Importcommits abgeleitet, nicht aus Ordnernamen: 2297 Originalpakete aus
`9d6316256e6986888db29c4dc3dc8445194d04a3`, dazu `AC_VisualOverrideManager` aus
`68601da84a969a07327cc6f8992ce02d9102c23e`. Zwei damals mitgespeicherte
projektseitige External Actors sind keine importierten Originalassets.

2156 Originale haben eine belegte Zielzuordnung. Die übrigen **142** umfassen
unbenutzte Sample-Interaktionen wie Shoves/Tackles/Takedowns und Demo-Blueprints;
ihre Ziele bleiben im Manifest ausdrücklich `null`. Alle aktuellen Payloads
stimmten vor Entfernung mit den HEAD-LFS-Identitäten überein. 2295 waren seit
dem Import unverändert; die drei historischen Migrationsfixes sind benannt.

Nach Entfernung der vier Actors: frischer Graph **13361 Pakete / 86527
Paketkanten**, **keine** eingehende Kante aus den verbleibenden Paketen in die
gesamte Originalmenge. Unabhängiger Graph-/Git-LFS-Review bestätigt den Befund.
Frischer Textaudit über **972** getrackte lesbare Dateien findet keinen
ausführbaren Original-Ladepfad; ein `DevComment` ist rein historisch.

Nach Entfernung aller Originale: frischer Editorgraph **11063 Pakete / 63658
Paketkanten**, keine vorhandenen Originalpakete und keine Kanten auf entfernte
Originale oder Actors. Die **70** schon vorher unbekannten Registryziele bleiben
unverändert. Package-Identifier-Managementkanten sind null; der
PrimaryAssetId-Managementgraph und beliebige dynamische Blueprint-Strings werden
dadurch nicht vollständig bewiesen. Deshalb sind die folgenden Laufzeit- und
Cook-Prüfungen eigenständige Abnahmepunkte.

## Ausgeführte Validierung ohne Originale

- Frischer UE-5.8.2-Editor nach der vollständigen Entfernung. Zwei echte
  Saved-Map-PIE-Smokes: Baseline-/Prototype-Experience laut Load-Complete-Log,
  passendes PawnData, besessener `BP_Rpg_Character` und richtige ASC-Owner-/
  Avatar-Zuordnung. Normale W-Eingabe bewegt beide Spieler über 220 cm; Space
  führt zu Falling und anschließend zurück zu unterstütztem Walking.
- Persönliche Saves sind durch einen prozesslokalen `-UserDir` unter
  `Saved/GaspMapCleanup/IsolatedUser` getrennt; der tatsächliche Unreal-Saved-Pfad
  wurde geprüft. Die unveränderten GameModes dürfen innerhalb dieses separaten
  Verzeichnisses speichern. Playsettings wurden restauriert.
- **9/9 bestehende Tests bestanden**, 50,30 s: drei AssetComposition-Verträge,
  drei tatsächliche optionale Manny-/Retarget-Läufe für CMC, Mover und Ragdoll
  samt ihren vorhandenen Traversal-/Equipment-/Getup-/Late-Join-Szenarien sowie
  drei PlayerStart-Tests. **243 Warnungen**, keine Testfehler; Einzelheiten im
  Manifest. Das ist keine neue vollständige Netzwerk-/Gait-Kreuzproduktabnahme.
  Die Warnungen betreffen überwiegend temporäre PIE-NetGUIDs, Voice und Manny-
  PoseAssets; auch eine `RollbackFrame EQUAL PendingFrame`-Warnung bleibt erfasst.
- **9/9 Blueprint-/AnimBP-Kompilierungen** mit Warnungen als Fehler bestanden.
  Kein Save eigener Inhaltsassets erforderlich; Editor sauber beendet.
- Repräsentativer Windows-Cook für `Lvl_RpgBaseline`, `Lvl_ThirdPerson`,
  `Lvl_RpgGaspMantle`, `Lvl_RpgGaspMover` und `Lvl_RpgGaspMoverRagdoll`: **bestanden**,
  Exit 0, **3132/3132 Pakete**, 98,20 s Prozesslaufzeit. Vollständiger Cook mit
  warmem DDC, kein inkrementeller Cook. Alle fünf `.umap`-Ausgaben vorhanden;
  kein entferntes Original-/Actorpaket unter `Saved/Cooked/Windows`.
  **0 Fehler, 3 eindeutige Warnungen**: GameplayCue-Suchpfad-Fallback, MCP-
  Lizenzhinweis und bekannter NativeTick-Konflikt des Respawn-Widgets.
  Der erste Lauf bleibt mit Exit 1 dokumentiert: alle Pakete verarbeitet,
  aber MCP-Port 8000 durch den Editor belegt. Wiederholung mit Port 8001 bestanden.
  Kein neuer C++-Build; vorhandene Runtime-Binaries sind unverändert.
  Vier lokale Plugin-Overrides abschließend verifiziert. Keine ausführbare
  gepackte Anwendung und keine vollständige Feature-/Netzwerkmatrix getestet.

Der alte Probe meldet geschützte Felder wie `CurrentExperience` ausdrücklich
als nicht lesbar; Experience wird über WorldSettings, Load-Complete-Log und
tatsächliches PawnData belegt. Viewportbilder verwenden die Editor-Kamera und
sind keine neue Nutzer-Sichtabnahme der Spielfigur.

## Entfernung und Rückweg

Der Standard-MCP-Audio-Löschaufruf entfernte zunächst nur 272 von 278 Dateien
und meldete `false`. Der Editor wurde sauber geschlossen und alle 278 Dateien
hashidentisch wiederhergestellt. Nach ausdrücklicher Freigabe der vollständigen
2298er-Liste wurden die exakt inventarisierten Git-Pfade bei geschlossenem
Editor entfernt; kein rekursives Ordnerlöschen. Git/LFS enthält die Originale
und ihren geprüften Vorzustand. Die vier Actorpakete wurden regulär über MCP
gelöscht und gespeichert.

Lokale Rohbelege: `Saved/GaspMapCleanup` und die beiden neuen Registryaufnahmen
unter `Saved/GaspImportAudit`. Das versionierte Prüfmanifest enthält ihre
Hashes. Andere Checkouts müssen neue Laufzeitbelege erzeugen.
Beim versionierten Inventar bezieht sich der Beleg-Hash auf den kanonischen
Git-Inhalt mit LF-Zeilenenden; Windows-Checkouts können ihn als CRLF ausgeben.
Die lokalen `Saved/`-Belege werden als unveränderte Rohbytes gehasht.

## Einfach nachstellen und Fortsetzung

`Lvl_RpgBaseline` oder `Lvl_ThirdPerson` öffnen und Play starten: Die zusätzliche
Sample-Figur und ihr Traversalblock fehlen; der normale RPG-Spieler bewegt sich
weiter mit W und springt mit Space. Die drei GASP-Testkarten verwenden ihre
eigenen übernommenen Varianten wie bisher.

GASP-05 ist im dokumentierten Umfang abgeschlossen und bestätigt gemergt. GASP-02-/04-Grenzen bleiben dokumentiert;
GASP-06 bleibt gemäß Roadmap zurückgestellt.

## Roadmap-Auftrag

Aus der Roadmap hierher verschoben; Auftrag für den
[Abhängigkeitsaudit](#vorangehender-audit-pr-156) und diese Entfernung.

Erst nach dem Variantenvergleich einen eigenen Bereinigungs-PR erstellen.
Aktive eigene Inhalte bleiben unter `/Game/SurvivalRpg`; vorhandene Engine-,
Plugin- und GameFeature-Abhängigkeiten werden wiederverwendet.

Vor jeder Entfernung eine konkrete Paketliste und Quell-Ziel-Zuordnung erzeugen.
Harte, weiche, Management- und dynamisch konfigurierte Referenzen, Chooser,
PoseSearch, Retargeting, Foley und Map-Auswahl prüfen. Frisches Laden/Kompilieren
und repräsentatives Cooking müssen ohne Importoriginale bzw. externes
`D:/Repos/GameAnimationSample` als Laufzeitquelle funktionieren. Baseline und
benötigte Varianten erhalten, Rückweg über Git/LFS sichern. Vorhandene
Foundation-/Sample-Dateien nicht allein aufgrund ihres Ordnernamens löschen.

## Vorangehender Audit (PR #156)

Fortsetzung: Die damals noch blockierte Entfernung wird in
[Originalimport entfernen](#ergebnis-und-umfang) mit dem aktuellen
Validierungsstand dokumentiert. Die folgenden Zahlen beschreiben ausdrücklich
den Audit vor der Entfernung.

Stand: 27.09.2026, **Audit abgeschlossen, [PR #156](https://github.com/Athurito/SurvivalRpg/pull/156) bestätigt gemergt**. Arbeitsbranch `codex/gasp-05-import-audit`,
Basis `577d760606be08e84491114db7c8a1eed0c2b43a`. Der begrenzte erste Schritt
erzeugt eine konkrete Paketliste mit Herkunft und aktuellen Referenzbelegen.
Es werden in diesem Audit keine Assets entfernt oder Runtime-Systeme geändert.
Werkzeugcommit: `9c8d7c77c3be6d261a268339096d00cdfe823b82`.
Bestätigter Merge am 27.09.2026 um 00:29:00 UTC:
`fdff8b91341c0c130db8487cea62f5c50f86bfd8`, finaler geprüfter Head
`6c89b9f18308abcc6cbf83a7973899cd351087d4`. Die unten genannten Prüfungen
gehören zum Audit; für diese reine Merge-Statuspflege wurden sie nicht wiederholt.

### Zuständigkeit und Methode

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

### Entscheidung: derzeit keine Entfernung aus der geprüften Liste

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

### Tatsächlich ausgeführte Prüfungen

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

### Dynamische Routen und noch offene Abnahme

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

### Nächster begrenzter Schritt innerhalb GASP-05

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

### Reproduktion

[Werkzeuganleitung](../Build/Tools/GaspImportAudit/README.md),
Registryexport `Build/Tools/Unreal/gasp_import_audit.py` und Offline-Planer
`Build/Tools/GaspImportAudit/plan.py`. Das versionierte Manifest enthält die
reproduzierbare Zuordnung als `mappings`. Lokale Rohdaten liegen unter
`Saved/GaspImportAudit`; ihre SHA-256-Hashes stehen im Manifest. Andere
Checkouts müssen die Rohaufnahme neu ausführen und dürfen lokale Belege nicht
als vorhanden voraussetzen.
