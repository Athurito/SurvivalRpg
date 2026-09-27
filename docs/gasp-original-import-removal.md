# GASP-05 – Originalimport entfernen

Stand: 27.09.2026, **Entfernung ausgeführt und validiert, bereit zum Merge**.
Branch `codex/gasp-05-remove-sample-actors`, Basis
`d6cb927f5654a681bf0c02db551af86276cf916d`. Der Nutzer hat die Entfernung
aller **2298 Original-Assets** zusätzlich zu den vier Sample-Kartenobjekten
ausdrücklich bestätigt. Assetcommit `0c37d9c4b9fb5cb555569c81ea5b10aedaefa4d6`.
Noch kein Merge behauptet.

## Ergebnis und Umfang

Die ursprünglichen GASP-Assets sind entfernt. Die übernommenen Inhalte unter
`/Game/SurvivalRpg/Characters/GASP/` bleiben erhalten. Alle **4662 übrigen
getrackten Assets**, darunter alle **2156 zugeordneten Kopien/Adaptionen**, sind
byteidentisch zur Basis. Alle **sieben persönlichen SaveGames** sind unverändert.
Kein Runtime-C++, keine Konfiguration und keine Projektkopie wurde geändert.

Der [vorangehende Audit](gasp-import-cleanup-audit.md) konnte noch keine
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

## Einfach nachstellen und Fortsetzung

`Lvl_RpgBaseline` oder `Lvl_ThirdPerson` öffnen und Play starten: Die zusätzliche
Sample-Figur und ihr Traversalblock fehlen; der normale RPG-Spieler bewegt sich
weiter mit W und springt mit Space. Die drei GASP-Testkarten verwenden ihre
eigenen übernommenen Varianten wie bisher.

GASP-05 ist für den Merge abgenommen und wird erst nach bestätigtem Merge als
abgeschlossen geführt. GASP-02-/04-Grenzen bleiben dokumentiert;
GASP-06 bleibt gemäß Roadmap zurückgestellt.
