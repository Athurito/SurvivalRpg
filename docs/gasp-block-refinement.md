# GASP-06 – Nachbesserung von Blockpose und Bewegung

Stand: 27.09.2026, **technisch validiert; neue Nutzer-Sichtprobe offen**, im offenen
[Draft-PR #160](https://github.com/Athurito/SurvivalRpg/pull/160).
Die vorige Nutzer-Sichtabnahme ist fehlgeschlagen. Die historischen Ergebnisse
in [gasp-moving-block.md](gasp-moving-block.md) nehmen diese Korrektur nicht ab.
Eine neue Nutzer-Sichtabnahme ist vor dem Merge erforderlich.
Implementierung: `8ee05aa9690a817ee110438869196ea7232e5b0f` auf
`codex/gasp-06-moving-block`. [Versionierte Ergebnisse](assets/gasp-block-refinement.json).

## Ursache und Korrektur der Armposition

Die vier Blockclips wurden zuerst ohne Oberkörpermaske und ohne nachgelagerte
IK auf dem ursprünglichen Manny und dem tatsächlichen UEFN-Gameplay-Mesh
ausgewertet. Schon diese Auswertung reproduziert auf UEFN lokale
Translationsabweichungen: bis 1,34 cm am Oberarm, 6,31 cm am Unterarm und
5,98 cm an der Hand. Knochenlängen und Skalierung bleiben dabei nahezu gleich;
ein reiner Längentest würde den Fehler übersehen. Auf Manny stimmen die
Armtranslationen mit dessen Referenzpose überein.

Die Ursache liegt damit bereits in der Übertragung der Translationen zwischen
den unterschiedlichen Referenzskeletten. Die Übernahme der Retarget-Modi des
Quellskeletts wurde ebenfalls geprüft und verworfen: Sie korrigiert den Fehler
nicht. Die ausgewählte Korrektur setzt beim projektlokalen UEFN-Skeleton genau
acht benannte Gelenke auf den Standardmodus `Skeleton`: `clavicle`, `upperarm`,
`lowerarm` und `hand`, jeweils links/rechts. Rotationen bleiben animiert.
Root, Pelvis, Finger, Twist- und IK-Knochen werden nicht verändert; zusätzliche
Knochenverschiebungen werden nicht eingeführt.

Der direkte Vorher-/Nachher-Vergleich verwendet 17 Zeitpunkte pro Clip. Die
vier Blockphasen haben danach keine lokale Translationsabweichung in den
ausgewerteten acht Gelenken. Acht vorhandene GASP-Clips aus Stand, Vorwärts-
und Seitwärtslauf, Gehen, Drehen, Mantle, Vault und Getup behalten exakt ihre
vorherigen Armtranslationen. Das ist ein begrenzter Stichprobenvertrag;
die endgültige Spielpose wird zusätzlich im AnimGraph und PIE geprüft.

## Gameplay und Vorhersage

GAS besitzt eine aktivierungsgebundene Block-Bewegungsfreigabe. Sie beginnt
erst nach erfolgreicher Blockaktivierung und endet vor End-/Cancel-Callbacks;
Avatarwechsel und Komponentenende räumen sie ebenfalls auf. Equipment und
die bestehende Block-Ability bleiben für die eigentliche Blockregel zuständig.

CMC verwendet während eines gültigen Blocks die Controller-Ausrichtung in
`PhysicsRotation`. Die widersprechende Orientierung zur Bewegung wird nur
für diesen Aufruf ausgeschaltet; die bisherigen Flags werden anschließend
wiederhergestellt. Normaltempo bleibt unverändert. Traversal, Root Motion,
Tod und gestoppte Bewegung behalten Vorrang.

Mover behält ursprüngliche Gangart und Ausrichtungswünsche im Eingabeverlauf
und auf dem Netzwerk. Nur die Kopie für den jeweiligen Simulationsschritt
erhält die wirksame Blockregel. Der Server leitet Block aus seiner eigenen
GAS-Aktivierung ab; die Owner-Wiederholung verwendet den zugehörigen lokalen
historischen Zustand. Der lokale Blockwert wird nicht vom Client zum Server
übertragen. Ein eigener Mover-Sync-Wert trägt das simulierte Ergebnis durch
Replikation und Korrekturen.

Die native Bewegung richtet den Körper am horizontalen Kamera-Yaw aus. Der
Pawn-Blueprint passt seine GASP-Eingaben an: `Strafe`, `Sprint` → `Run`,
Richtung aus dem Welt-Bewegungswunsch relativ zum Kamera-Yaw und zusätzlicher
Rotationsoffset null. `Walk`/`Run`, Bewegungseingabe, Crouch und die übrigen
Collection-Einträge bleiben erhalten. Nach dem Loslassen wirkt eine weiterhin
gehaltene Sprinttaste wieder. Die vorhandenen normalen Geschwindigkeiten
werden nicht vereinheitlicht oder reduziert.

Der bestehende CMC-Adapter besitzt noch keinen separaten Sprint: Er liefert
konstant `Run` und `WantsToSprint=false`, mit 600 cm/s Normaltempo. Das ist im
frischen Assetexport bestätigt. Mover besitzt den geprüften Wechsel von
375 cm/s Run auf 585 cm/s Sprint. Dieser vorhandene Sprint wird beim Block
gesperrt; eine neue CMC-Sprintmechanik wird hier nicht eingeführt.

## AnimBlueprint-Vertrag

Beide AnimBlueprints speichern die Pose nach dem `UpperBody`-Slot einmal ab.
Bei ruhigem Stand wird die vollständige Blockpose einschließlich Hüfte und
Beinen verwendet. Beim Laufen oder Drehen kommen die Beine und die Hüfte aus
GASP; die Blockhaltung wird ab `spine_01`, Tiefe 1, mit Rotation im Mesh-Raum
gemischt. Der vorhandene `DefaultSlot` bleibt danach für Ganzkörperaktionen
erhalten; Mover-Ragdoll und der `PreRagdoll`-Cache bleiben bestehen.

Der Wechsel verwendet Hysterese: Bewegungseintritt bei 10 cm/s, Rückkehr
unter 3 cm/s; Drehung 20/8 Grad/s und verbleibender Root-Winkel 20/10 Grad.
Beschleunigung öffnet die Beinbewegung frühzeitig. Die Überblendung verwendet
`FInterpTo`, Rate 12. Während der Blockmontage löst Offset Root Bone seinen
angesammelten Rotationsversatz. CMC wählt dafür ebenfalls Strafe. Zusätzliche
Rumpfneigung wird weich auf 15 Prozent ihres normalen Einflusses reduziert.
Alle entsprechenden Worker-Funktionen verwenden AnimInstance-Snapshots und
Animationsabfragen; sie fragen keine Gameplay-UObjects vom Worker ab.

`Walk_Block`-Clips ersetzen die GASP-Laufbewegung nicht. Ein Ausbau der
Combat-Pose-Search-Datenbanken ist nicht Teil dieses PRs.

## Tatsächlich ausgeführte Abnahme

| Prüfung | Ergebnis |
| --- | --- |
| Editor/Game | Erfolgreich; letzter Editor-Testbuild 13,34 s, Game 71,70 s |
| Clip-Negativkontrolle | 0/1, genau vier Fehler an den vier Blockclips mit den ursprünglichen acht Retarget-Modi |
| Fokuslauf | `focused-final`: 7/8 in 178,85 s; letzter Fehler war das unten erläuterte Cancel-Messkriterium |
| Gezielte Wiederholung | `cancel-final`: 1/1 in 23,98 s nach dessen Korrektur; die sieben übrigen Fälle blieben unverändert |
| Regression | `regression-final`: 25/25 in 146,33 s; Block-Lifecycle, Angriff/Root Motion, Tod/Respawn, Ragdoll/Late Join, Composition, Mantle sowie alle sieben bisherigen Fixed-Correction-Fälle für Gameplay/Mantle/Vault/Hurdle |
| Assets | Frisches Reload, drei Blueprint-/AnimBP-Compiles mit Warnungen als Fehler, unabhängiger Review der endgültigen T3D-Exporte; keine fehlenden direkten Projektabhängigkeiten |
| Sichtprüfung | Tatsächliche RMB/WASD/Shift-Eingabe und Kameradrehung in beiden Varianten; `cmc-04-*`/`mover-04-*`, zusätzliche Schulter-/Schild-Nahaufnahmen aus drei Richtungen |
| Windows-Cook | Fünf Karten, Commandlet meldet **Success, null Fehler, drei Warnungen**; 3125 gekocht, sieben platformbedingt übersprungen, null inkrementell übersprungen; alle fünf Map-Ausgaben frisch vorhanden |
| Erhaltung | Nach Cook erneut bestätigt: vier Änderungen dieser Nachbesserung, 4658 übrige Assets und sieben persönliche Saves byteidentisch; vier lokale Plugin-Overrides verifiziert |

Damit sind **33 unterschiedliche Prüffälle aktuell bestanden**, verteilt auf
die genannten Läufe, kein einzelner 33/33-Lauf. Sie enthalten insgesamt
896 Warnungen: 120 Voice-Interface, 745 NetPackageMap, 16 NetworkPrediction,
14 bestehende Manny-PoseAsset-Meldungen und eine bestehende Respawn-Widget-
Tick-Warnung. Der Cook enthält GameplayCue-Pfadsuche, MCP-Hinweis und dieselbe
Widget-Warnung. Die Cook-Dauer beträgt 183,49 s im Commandlet beziehungsweise
206,32 s einschließlich Start/Shutdown. Der PowerShell-Launcher hat den
Child-Exitcode als `null` erfasst; es wird deshalb **kein Prozess-Exitcode 0
behauptet**. Commandlet-Erfolg, vollständige Paketzählung, frische Map-Ausgaben
und sauberes Engine-Shutdown sind im Log belegt.

Über den gesamten PR sind acht Assets verändert: die vier ursprünglichen
Blockmontagen, beide AnimBlueprints, der Mover-Pawn und das UEFN-Skeleton.
4654 übrige Assets sind gegenüber der PR-Basis unverändert. Die Erhaltung der
vier ursprünglichen Montageänderungen wird nicht als neue Änderung ausgegeben.

### Netzwerk- und Gelenknachweise

Die PIE-Messung prüft laufend alle acht Arm-Gelenktranslationen relativ zum
tatsächlichen Mesh sowie Skalierungen auf Authority, Owner und Observer;
Late Join und die Blockphasen werden eingeschlossen. Kamerablick, echte
WASD-Bewegungsrichtung und Geschwindigkeit werden in stabilen Messfenstern
getrennt erfasst. Mover prüft beide Reihenfolgen von Shift/RMB und das
Wiederaufnehmen von gehaltenem Shift nach RMB-Release.

Bei gehaltenem Block korrigiert Fixed Prediction den injizierten seitlichen
50-cm-Fehler mit zehn realen Replay-Schritten, ohne Ability-/Montagewechsel.
Über Release wird Frame 406 nach Restore von 405 tatsächlich erneut simuliert,
während heutiger Head und GAS bereits unblocked sind. Sein historischer Block
bleibt erhalten, der seitliche Fehler wird um 50 cm korrigiert.

Beim serverseitigen Cancel verwendet Authority bereits den unveränderten
rohen Sprintwunsch, während der verzögerte Owner noch Block vorhersagt:
fünf Messungen, zuletzt 386,13 cm/s gegenüber 375 cm/s Run. Eine echte
Korrektur desselben Frames 294 ersetzt anschließend `Blocking=true` durch
`false`, erhöht dessen Geschwindigkeit von 378,686 auf 381,900 cm/s und
führt 22 zusammenhängende Replay-Schritte aus. Der aktuelle Head und die
beendete GAS-Aktivierung bleiben erhalten. Es wird dafür kein künstlicher
Positionsfehler eingespeist.

### Erhaltene Fehlversuche und Reviewkorrekturen

Die ersten Fokusläufe bleiben mit 4/8 und 1/4 erhalten. Der Late-Join-Test
hatte einen noch nicht aufgelösten Equipment-Actor als Baseline gespeichert;
er wartet jetzt auf tatsächlich replizierte Actors und Attachments, bevor
die unveränderten strikten Identitätsprüfungen beginnen. Der Cancel-Observer
verlor nach einem Restore seine Vergleichsgeneration; nur dieser Fall erhält
nun nachgewiesene reale Replay-Ausgaben als Historie der nächsten Korrektur.

Der Release-Test verlangt ausdrücklich einen Replay-Ausgabeframe **nach**
dem Restore-Frame. Der letzte Cancel-Fehler verlangte dagegen mindestens
1 cm Positionsdifferenz innerhalb jeder bereits fortlaufend korrigierten
Generation. Stattdessen verlangt dieser Fall jetzt eine positive
Geschwindigkeitskorrektur von mindestens 1 cm/s am exakt gleichen Restore-
Frame, zusätzlich zu echtem `true → false`, Replay, gleichem Head und dem
separaten Authority-/Owner-Nachweis. Die beiden 50-cm-Positionsfälle bleiben
unverändert. Dies verändert den Messvertrag, keine Runtime-Toleranz.

Der Assetreview fand außerdem eine zunächst ungespeicherte Slot-Abfrage
für die Lean-Reduktion. UE 5.8s Pin-Setter markiert das Package dabei nicht
automatisch dirty. Die Korrektur wurde gespeichert, frisch geladen und
erneut geprüft: beide Graphen verwenden `UpperBody`, im tatsächlichen PIE
beträgt der Lean-Einfluss 0,1501 beziehungsweise 0,15. Frühere Bilder und
Exporte werden nicht als Nachweis dieser letzten Korrektur ausgegeben.

## Einfache neue Sichtprobe

`Lvl_RpgGaspMover` öffnen und Play starten. RMB halten: im ruhigen Stand die
vollständige Schildhaltung ansehen, Kamera drehen und mit WASD vorwärts,
seitwärts und rückwärts laufen. Besonders Schulter, Ellenbogen und das
Anhalten beobachten. W und Shift bei gehaltenem RMB dürfen keinen Sprint
starten. Dann nur RMB loslassen: gehaltenes W/Shift soll wieder sprinten.
Dasselbe Bewegungs-/Poseprogramm auf `Lvl_RpgGaspMantle` für CMC wiederholen;
dessen bestehendes Run-only-Profil hat keinen separaten Shift-Sprint.

Neue Nutzer-Sichtabnahme und Merge bleiben offen. Die vorhandenen
`GASP-VAL-01/02`- und `GASP-NET-03`-Grenzen gelten weiter; diese Prüfung ist
keine vollständige Waffen-, Gait- oder Retarget-Matrix. Eine neu ausgelöste
Block-Hit-Reaktion ist durch den Cliptest, nicht durch einen neuen visuellen
Trefferablauf geprüft. Combat Motion Matching ist weiterhin außerhalb dieses PRs.

Rohbelege: `Saved/GaspBlockRefinement20260927`. Sie sind ignoriert und in
anderen Checkouts neu zu erzeugen. Frühere Compile-/Toolversuche und
fehlgeschlagene Sicht-/Testläufe werden nicht als Abnahme ausgegeben.
