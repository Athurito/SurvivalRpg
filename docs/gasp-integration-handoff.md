# GASP-Integration: aktuelle Übergabe

Zentraler Plan und Aufgabenverträge:
[gasp-integration-roadmap.md](gasp-integration-roadmap.md).
Diese Datei hält den **aktuellen Arbeitsstand**, die Roadmap die Reihenfolge und
Abnahme. Beide bei relevanten Fortschritten im selben PR aktualisieren.

## Aktueller Stand – 26.09.2026

| Feld | Wert |
| --- | --- |
| Aktive Implementierungsaufgabe | `GASP-03` – begrenzter spielbarer Ragdoll-/Getup-Pilot, **PR offen (Draft)**; Prüfungen abgeschlossen, Nutzersichtprüfung offen |
| Nächste bereite Aufgabe | Nutzersichtprüfung des validierten Piloten, danach PR #152 bei Abnahme mergen; weitere Roadmap-Aufgaben danach neu bewerten |
| Zuständiger Chat / beanspruchte Dateien | Root: sämtliche Editor-/MCP-/Buildsitzungen, binäre Assets und Mover-Integration. Runtimeagent: native Lifecycle-Konzeption. Testagent: Validierungskonzept. Dokumentationsagent: Roadmap, Übergabe, Audit-Mergestatus und `gasp-mover-ragdoll-pilot.md`. Weitere Dateizuteilungen nur koordiniert |
| Runtime-Ausgangspunkt | `9baac5f36fd978b86c364df3e82aa74d76cfb7d2`, bestätigter Merge PR #151 am 26.09.2026 um 13:40:22 UTC; Audit enthält keine Runtimeänderung |
| Checkout / aktiver Branch | `codex/gasp-03-ragdoll-pilot`, `D:/Repos/SurvivalRpg`; Basis `9baac5f3` |
| Letzter Implementierungs-Commit | `7c2a769a26ec1396f9cbc3804eaedefee33cd2db`, einschließlich 25 LFS-Assets gepusht |
| Aktueller Arbeits-PR | [PR #152](https://github.com/Athurito/SurvivalRpg/pull/152), **OPEN / Draft**, Basis `master`, Implementierungshead `7c2a769a`; noch nicht gemergt |
| Übernommene Statuspflege | Bestätigten Audit-Merge in Roadmap, Übergabe und `gasp-mover-ragdoll-source-audit.md` im Pilotbranch nachgeführt |
| Nächster Handgriff | Nutzer prüft Pilotkarte: Play, R, etwa 1 s warten, R, WASD/LMB; danach PR #152 bei Abnahme mergen |
| Blocker / offene Abnahme | Finaler Editor 12 und Game bestanden; Root-Sichtprüfung von Ragdoll/Getup/Rückkehr/Bewegung/Angriff ausgeführt, 25 Zielhashes erfasst. Finale Regression 20/20 bestanden, 1089 Warnungen; Abschlussaudit bestanden, älterer Lauf bleibt 5/6. Draft-PR #152 offen; manuelle Nutzer-Sichtprüfung und Merge noch ausstehend. VAL-01/02, NET-03-Grenze und ältere Pending-Fixture-Timingempfindlichkeit bleiben Folgepunkte |

## Aktueller Auftrag – GASP-03-Pilot

- Erstes lebendes Ragdoll aus stationärem, nicht geducktem Stand und Getup auf
  freier unterstützter Ebene; UEFN bleibt Physics-/GAS-/Equipment-Gameplay-Mesh.
- Bestehende Experience/PawnData-, GAS-, Mover-, Health-/Death-/Respawn- und
  Equipment-Schnittstellen nutzen. Konkrete Inhalte und Tuning bleiben in
  originalabgeleiteten Blueprint-/PhysicsControl-/Chooser-/Animationsassets.
- Vereinbarter Vertrag: Capsule während Ragdoll autoritativ verankert,
  PhysicsControl nur am Mesh, keine kontrollierte Rollkraft oder Client-Bone-
  Authority. Konkrete ServerInitiated-/InstancedPerActor-/Exclusive_Blocking-
  Blueprint-GA: R zum Eintritt, zweites R via WaitInputPress für Getup;
  native Auswahl-Task korreliert Revision/Aktivierung, terminaler Tod gewinnt.
- Keine neue Health-/Respawn-Architektur oder Physics-Authority eines
  Retarget-Followers. Keine automatische Erweiterung um Sample-Demo-
  Interaktionen, Takedowns oder NPC-Autogetup.
- Finaler Editor-Build 12 bestanden (11,21 s), Game-Build bestanden
  (68,64 s), finaler inkrementeller Game-Build bestanden (22,82 s).
  Scoped Physics/Smoothing, gültiger Null-DirectionalIntent, restaurierte
  Component-Physicsflags und ASC-Stop beim Proxy-Getup-Tod implementiert.
- Korrigierter Fokus: Remote/Late Join und Getup-Tod bestanden; keine der drei
  gezielten Warnklassen (MoveInputType, incompatible Simulate, fully simulated
  Mesh movement). Frühere 16842 Remote-Warnungen bleiben historisch erhalten.
- `pie-full-current.json` bleibt **5/6**, 360 Warnungen und eine Assertion.
  Host erreicht echte Physics/Getup; gewählter Start war 0,100 s, initiale
  SimProxy-GAS-Montage 0,000 s, Fixture verlangte mindestens 0,09 s. Die
  korrigierte Fixture berücksichtigt die bestehende 0,1-s-GAS-Schwelle und
  besteht final; keine Runtime-Toleranzerhöhung.
- `final-tests.json`: **20/20 Success**, 0 Fehler/übersprungen, 187,454575 s,
  alle sechs neuen Ragdollfälle grün. 1089 Warnungen: NetPackageMap944,
  Voice110, NP17, Animation14, RpgCharacter3, Blueprint1; null der drei
  gezielten neuen Warnklassen. Übrige Warnungen bleiben dokumentiert.
- Tatsächlicher Host-Inputkonflikt: Combat und Pilot belegten R mit gleicher
  Priorität 1; Pilot-Experience reserviert R nun mit Priorität 2. Nur diese
  Tastenüberschneidung bestätigt. Neue Pilotkarte: Boden Static/BlockAll mit
  WorldStatic, drei Starts Z=88,15. Nach Save/Reload direkter Ragdoll-Eintritt
  beobachtet. Mit finalen Binaries prüfte Root zusätzlich Getup, stehende
  Rückkehr, Bewegung und Waffenangriff; Frontbilder vermeiden HUD-Verdeckung.
  Keine exakte Frameglätte-/Audioabnahme (-NoSound). Editor-Inputbridge nutzt
  PC.InputKey, die zusätzliche MCP-Blicksteuerung verändert nur die Kamera.
- Acht Notify-Assets gespeichert und neu geladen: 28 Eventrecords erhalten,
  alte Originalklasseninstanzen entfernt. Registry-Hülle **2115 Pakete / 81
  externe Grenzen**, keine fehlenden Pakete oder Sample-`/Game`-Referenzen
  außerhalb `/Game/SurvivalRpg`; dynamische/Cooked-Hülle bleibt unbewiesen.
  [Pilotmanifest](assets/gasp-ragdoll-pilot.json): 17 Quellkopien + acht
  Kompositionsassets; alle 25 gespeicherten Zielhashes erfasst und erneut
  geprüft. Final 6938 ursprüngliche Assets und 17 Baseline-Map-/Savedateien
  unverändert. Sechs Blueprints mit warnings_as_errors kompiliert;
  Experience-Hülle 2115/81 und Karten-Hülle 2225/99 ohne fehlende Pakete oder
  rohe Sample-Kanten. Editor 17044 sauber beendet, keine UnrealEditor-/Cmd-
  Prozesse übrig. Vier Overrides, Python-AST, Projekt-JSON und Diffprüfung
  bestanden; unabhängiger Review ohne Blocker. Nach finalen Builds nur
  erklärende API-Kommentare ergänzt, keine Funktions-/Assetänderung.
  [Pilotbericht](gasp-mover-ragdoll-pilot.md).
- [Draft-PR #152](https://github.com/Athurito/SurvivalRpg/pull/152) ist offen,
  Implementierung `7c2a769a` gepusht. Nutzersichtprüfung, danach Merge bei Abnahme. Nachstellen: neue Pilotkarte Play,
  R, etwa 1 s warten, R, dann WASD/LMB; bei Bedarf mit der Maus nach unten sehen.
  Die Dauerfreigabe für nicht sinnvoll manuell prüfbare Schritte bleibt davon
  unberührt; historische Prüfungen nicht als neue Pilotabnahme ausgeben.

## Letzter abgeschlossener Schritt – GASP-03-Source-Audit

- [PR #151](https://github.com/Athurito/SurvivalRpg/pull/151) bestätigt gemergt
  am 26.09.2026 um 13:40:22 UTC: `9baac5f36fd978b86c364df3e82aa74d76cfb7d2`,
  finaler Head `5a0e743c4796d7ef2aacf7ddb82c5fa565edcf17`. Die folgenden
  Ergebnisse sind historische Auditbelege, keine erneut ausgeführte
  Pilotvalidierung.

- Tatsächlich geöffnetes Originalprojekt `D:/Repos/GameAnimationSample`, UE
  5.8.2; kein Sample-Release-Identifier belegt. Vier Kernassets unterscheiden
  sich binär von Importkopien. Frische Original-Snapshots und DSL unter
  `Saved/GaspRagdollSourceAudit20260926` belegen Tick-/PhysicsControl-Aufbau,
  Ragdoll-Input/Mover-Capsule-Führung und PoseHistory-/Chooser-Getup.
- Original-Mover-CDO: NetworkPrediction-Liaison, `bSyncInputsForSimProxy=True`,
  erfasste Modi `bSupportsAsync=False`. Das beweist noch keinen RPG-
  Autoritäts-/Replay-/Late-Join-Vertrag; diese bleiben Pilotarbeit.
- [Versioniertes Manifest](assets/gasp-ragdoll-source-audit.json): 44
  Quellkandidaten, davon 27 vorhandene Referenzen und 17 vorgeschlagene Ziele;
  keine Import-Whitelist. Korrigierte Registry: 3017 Projektpakete, 25907
  Package-Kanten, null Management-Kanten, 164 externe Grenzen, keine fehlenden
  Registry-Pakete. Editor-only enthalten; dynamische/Cooked-Hülle nicht bewiesen.
- Alle sieben semantischen Snapshots partiell, native T3D-Ergänzung; sieben
  gescheiterte DSL-Exporte und doppelte Blattnamen ausdrücklich dokumentiert.
  Keine vollständige Graphabdeckung behauptet.
- Finale 115 Hashprüfungen bestanden, vier ursprüngliche Kernassets und alle
  17 Map-/Savedateien unverändert, vier Overrides verifiziert, keine Unreal-
  Prozesse. Erste dirty-Quellsitzung ohne Speichern verworfen; korrigierte
  Sitzung sauber geschlossen. Unabhängiger Manifestreview ohne P1/P2-Befund.
- Quell-Ziel-Abhängigkeiten und Zuständigkeiten sind für den begrenzten Audit
  dokumentiert; konkrete minimale Importliste erst im Piloten festlegen.
- Gameplay-Authority, GAS-Abbruch, Tod/Respawn, Equipment, Kontrollrückgabe,
  Observer und Late Join an vorhandene RPG-Schnittstellen anbinden; keine
  vorweggenommene zweite Health-/Respawn-Architektur.
- Noch keine Runtimeänderung, kein Assetimport und keine neue Build-/Testabnahme
  behauptet. [Abgeschlossener Auditbericht](gasp-mover-ragdoll-source-audit.md).
- **Dauerhafte Nutzerfreigabe vom 26.09.2026:** Nicht sinnvoll manuell prüfbare
  Schritte nach geeigneter automatischer Validierung und Review direkt pushen,
  mergen und mit dem nächsten begrenzten Roadmap-Schritt fortfahren. Dafür
  nicht auf zusätzliche Sichtabnahme warten; tatsächliche Ergebnisse, offene
  Befunde und bestätigte Merges weiterhin dokumentieren.

## Vorheriger abgeschlossener Schritt – GASP-VAL-03

- [PR #150](https://github.com/Athurito/SurvivalRpg/pull/150) bestätigt gemergt
  am 26.09.2026 um 13:09:25 UTC: `4ed174d2cdc1a2d52fc5a9272ad68437fed98025`,
  finaler Head `453b265bdfa8439565017b397af66a3b193ed1a3`. Die folgenden
  Ergebnisse gehören zur abgeschlossenen VAL-03-Abnahme und wurden für diese
  Merge-Statuspflege nicht erneut ausgeführt.

- Geforderte Reihenfolge: Traversal A, normale Waffenmontage als Ersetzung,
  anschließend Empfang der GAS-Montage für Traversal B, während A auf dem
  beobachtenden Client noch im NP-Präsentationspuffer liegt.
- Die vorhandene Play-Token-Korrelation soll A weiterhin sperren und B erst mit
  der passenden präsentierten Traversalidentität zulassen. Ein neuer Empfang
  derselben Montage allein darf A nicht freigeben.
- `TryReplayTraversal` wartet bisher auf `Clean`, `IsOnGround` und Ende der
  Ersetzungsmontage auf Owner, Authority und Observer. Die bisherigen Tests
  belegen Ersetzung und späteren Replay, nicht diese engere A/B-Überlappung.
- Testcommit `8bc887e1` setzt den testlokalen NP-Puffer vor PIE auf 1000 ms
  und stellt ihn nach PIE wieder her. A erreicht mindestens 150 ms beobachteten
  Montagefortschritt, wird
  autoritativ abgebrochen; echte Waffenmontage O wird empfangen, schreitet
  fort und endet durch Gameplay-Cancel. Erst danach B per W/Space. Empfangs-
  Wire-Kante und alter A-Sync werden im selben Dispatch beobachtet, volle
  B-Identität gegebenenfalls im nächsten Fixed-Schritt gebunden.
- Korrigierter Fokus **1/1 bestanden**, vier Warnungen, null Testfehler.
  Receipt Frame 2215: O2→B3 bei A1; Bindung 2216, B-Instanz 4 ab 2289,
  A nie sichtbar, genau eine Traversalinstanz. Erster roter Messversuch und
  vorheriger Null-Test-Filterversuch bleiben dokumentiert.
- Nur den Runtime-Tokenvergleich temporär entfernt: gleicher Test erwartungsgemäß
  rot mit drei Assertions, tatsächlicher A-Instanz 4 nach gültigem B-Empfang
  und späterer B-Instanz 5. Testhashes unverändert. Originalruntime exakt
  wiederhergestellt, kein Runtime-Diff; anschließender Editor-Build **Succeeded,
  17,87 s**. Kein Runtimebug oder Assetfix behauptet.
- Gemeinsame Regression **15/16 bestanden**, genau eine Assertion im alten
  Pending-Replay-Test, 129 Warnungen, 185,93 s. Neuer VAL-03-Fall bestanden.
  Der Altfall verfehlt seine kurze Overlap-Beobachtungsvoraussetzung; keine
  Traversal-Wiederbelebung. Isolierter Repeat mit unverändertem Code **1/1
  bestanden**, vier Warnungen, keine Testfehler, 38,916065 s; echte Überlappung
  diesmal belegt. Der rote Batch bleibt 15/16; alte Timingempfindlichkeit offen.
- Zusätzlicher Lauf bei 30 Render-FPS und 50 Hz Fixed **1/1 bestanden**,
  16 Warnungen, keine Testfehler, 35,490707 s. Receipt/Bindung Frame 760 bei
  unsichtbarer A, B-Start 789; sechs A-Proben, drei nach Empfang, kein A- oder
  Frühstart. Genau eine B-Instanz, 22 B-Proben, Phasenfehler 0, ein Notify-Beginn
  ohne Duplikat.
- Abschlussaudit: alle zehn Maps und sieben SaveGames unverändert, vier
  Overrides verifiziert, geladene Projekt-DLLs im 30-FPS-Log belegt,
  Runtime-Git-Diff leer und keine Unreal-Prozesse. Kein neuer Game-Build für
  die ausschließlich im Editor-Modul liegende Teständerung ausgeführt.
  [Architektur, Belege und portables Nachstellen](gasp-buffered-traversal-replay.md).

## Vorheriger abgeschlossener Schritt – GASP-NET-03

- [PR #149](https://github.com/Athurito/SurvivalRpg/pull/149) am 26.09.2026 um
  12:27:04 UTC bestätigt gemergt: `d933148f22e8051f0a50ab68719f2f507a6158b5`,
  finaler Head `8eafc16db581c898f7ebf3d8a54e486678d306c0`.
  Die folgenden Ergebnisse gehören zur abgeschlossenen NET-03-Abnahme und
  wurden für diese Merge-Statuspflege nicht erneut ausgeführt.

- Historischer Ausgangspunkt ist
  `Saved/GaspMoverProxyPose20260920/probe_run_final_host_gap`: zwei Prozesse,
  Host-Vault, beobachtender SimulatedProxy, 16 Ebenen ohne Ausschluss.
  Die alte Ebenenmessung ergibt einmal −70,99 ms; 17 eingefrorene Framepaare
  behalten Position und Montagephase exakt bei.
- Frische Baseline auf NET-02-Basis:
  `Saved/GaspPacketGapRecovery20260926/probe_run_baseline_host_gap`.
  Beide Rollen überqueren, fallen und landen. Freeze besteht; die alte
  Ebenenmessung bleibt mit drei Abweichungen bei 16 Vergleichen rot, maximal
  74,22 ms. Das ist ein neuer Ursachen-/Grenzbeleg, kein Fixerfolg.
- Der Runtimeaudit trennt gemeinsame Interpolation von Position und Phase von
  der verlorenen gekrümmten Authority-Bahn. Zwei empfangene Endpunkte enthalten
  diese Zwischengeometrie nicht vollständig. Zudem deckt eine kurze Render-
  Messklammer beim beschleunigten Aufholen deutlich mehr Montagezeit ab.
- Der begrenzte Auftrag verbessert deshalb den reproduzierbaren, getrackten
  Zwei-Prozess-Probe und die getrennte Offline-Phasen-/Geometrieauswertung.
  Das alte Kriterium bleibt bestehen; der neue Phasen-Analyzer ist rein
  diagnostisch und hat keinen Passwert. Keine C++-/Assetänderung und kein neuer
  Unreal-Build. Exakte NP-Endpunkte, NP-Uhr und Montageinstanz bleiben ungemessen.
- Erster portabler Kontrollstart scheiterte vor Probe-Boot an verschachtelten
  `ExecCmds`-Quotes; eigener Host über Launcher beendet, alle 17 Dateien
  erhalten. Fehler dokumentiert und Launcher korrigiert. Final **19/19 Tests**
  in 0,008 s, vier erwartete CLI-Ablehnungen ohne Child-Prozesse, Python-AST-
  Prüfung bestanden. Finale Launcher-/Runtimehashes entsprechen beiden Läufen.
- Beide portablen Läufe unter `portable evidence` abgeschlossen: Kontrolle
  **16/16** Ebenen, maximal 33,62 ms; Pause **13/16**, maximal 86,21 ms. Das alte
  Kriterium bleibt rot und unverändert. Freeze besteht mit 17 zusammenhängenden
  Paaren über 285,78 ms bei tatsächlich 368,28 ms Pause.
- Neue diagnostische Geometriemessung: Kontrolle 53 Proben, bis 28,54 cm
  3D-Root-Abweichung; nach Pause 33 Proben, bis 64,00 cm bzw. +63,27 cm Z.
  Nahe rohe Authority-Probe bei nur 3,335 ms Phasendifferenz bestätigt 63,15 cm
  Abstand. Je zwei fehlende Klammern ausgeschlossen; keine gültige Vorlauf-
  Baseline im Pausenlauf. Keine Bone-/Kontaktpose oder interne NP-Uhr gemessen.
- Je Lauf zwei Child-Prozesse, insgesamt vier, regulär mit Exitcode 0 beendet.
  Zehn Maps und sieben SaveGames unverändert, vier
  Overrides verifiziert, keine Unreal-Prozesse übrig; geladene Projekt-DLLs
  im Pausenlauf nachgewiesen. Pro portablem Lauf 82 Startup-Warnungen und 116
  Python-Error-Zeilen erhalten; ab Trigger bis Shutdown keine Warning/Error.
- [Befunde, Ownership, Nachstellen und Grenzen](gasp-packet-gap-recovery.md).
  Der anschließend gestartete Auftrag ist VAL-03; VAL-01 bleibt
  dokumentierte Messgrenze, VAL-02 offene Produktionsumgebungsprüfung.

## Vorheriger abgeschlossener Schritt – GASP-NET-02

- [PR #148](https://github.com/Athurito/SurvivalRpg/pull/148) am 26.09.2026 um
  11:56:35 UTC bestätigt gemergt: `1490dd7d85306bffa2b2a4b1ab7d9202f1a9b0c4`,
  finaler Head `065bacc917aedf80051281aee04655492ce3d657`.
  Die folgenden Ergebnisse gehören zum abgeschlossenen NET-02-Auftrag; sie
  wurden für diese Merge-Statuspflege nicht erneut ausgeführt.

- Historischer Beleg: `Saved/GaspMoverProxyPose20260920/regression-onset.log`,
  Zeilen 8877–8891. Owner und Authority beenden dieselbe GAS-Aktivierung normal;
  trotzdem ist der Owner-Terminalgrund `Finished`, der Authority-Grund `Cancelled`.
  Die Abweichung besteht vor der Testinjektion. Reconciliation erhält Identität,
  Montage und Warp-Cleanup, verletzt aber die bisherige Gleichheitsprüfung des
  Terminalgrundes. Einzelne Cleanup-Bedingungen wurden damals nicht protokolliert.
- Der spätere historische grüne Lauf hatte `Cancelled` auf beiden Seiten und
  bewies weder Ursache noch Fix. Frische 30-FPS-Diagnose belegt jetzt den
  vorzeitigen Authority-Cleanup durch normales Remote-Ende: 1,966675 s statt
  2,0 s, `stopped=1`, Handoff noch nicht erreicht. Der Support-Trace wurde deshalb
  noch gar nicht ausgeführt.
- Gameplay-Grund und Authority bleiben in GAS/Projekt-Mover. Der Test muss eine
  echte terminale Reconciliation und fehlende Wiederbelebung alter Traversal
  nachweisen; keine bloße Abschwächung auf beliebige Terminalzustände.
- Commit `8de5d927` erweitert den vorhandenen exakten natürlichen Hurdle-
  Endcallback auf Mover-Mantle am vollständigen Clipende und verschiebt normales
  Remote-Ende bis dahin. Echte Cancellation, finale Warp-/Supportbedingungen
  und bedingte Handoffs bleiben erhalten; CMC und Assets unverändert.
- Identische Fixture bei 30 FPS: `red-01` **1/3**, `green-01` **3/3** bestanden.
  Der rote natürliche Engine-Abschluss bei 1,766684 s beweist die Lücke ohne
  künstlichen Completion-Callback. Grün beobachtet tatsächlich Remote-Ende vor
  lokalem Engine-Abschluss, korrektes Warten und `Finished` auf allen Rollen;
  echte Authority-Cancellation bleibt sofort `Cancelled`.
- Finaler Editor-Build **Succeeded**, 23,27 s; Game-Build **Succeeded**, 123,09 s.
  Vorheriger C4458-Buildfehler durch verdeckende Fixture-Variablennamen und rote
  Läufe bleiben dokumentiert. Fokuslauf: 30 Warnungen, keine Testfehler.
  Gemeinsame Regression **28/28 bestanden**, 227,00 s: alle 17 Mover-Mantle-
  Fälle, drei Hurdle-, zwei Vault-, ein CMC-, vier native Vertrags- und ein
  Gameplay-Replay-Fall. Keine Errors/Ensures/Fatals im Testintervall.
- Finale 287 Warnungen: 140 Voice, 143 NetPackageMap, eine Blueprint-Tick-,
  eine PoseSearch-AsyncBuildIndex- und zwei NP-`RollbackFrame == PendingFrame`-
  Meldungen. Diese Befunde sowie zwei bekannte Start-`Condition failed`-Meldungen
  bleiben offen und erhalten. Zehn Maps und sieben SaveGames unverändert,
  vier Overrides verifiziert, Quellstand unverändert `8de5d927`, alle Prozesse beendet.
- [Ursache, Runtimevertrag, Nachweise und Grenzen](gasp-terminal-reconciliation.md),
  lokale ignorierte Belege `Saved/GaspTerminalReconciliation20260926`.
- NET-03 ist inzwischen der aktive begrenzte Folgeauftrag. Ausgangspunkt:
  `Saved/GaspMoverProxyPose20260920/probe_run_final_host_gap` und
  [Präsentations-Bericht](gasp-mover-traversal-presentation.md). Nach 350 ms
  Paketpause halten 17 Framepaare die Montage mit der Bewegung; eine Ebene
  bleibt beim Aufholen um −70,99 ms abweichend. Aktuell nachstellen und die
  rekonstruierte Bahn samt Messklammer prüfen; durch NET-02 nicht abgenommen.

## Vorheriger abgeschlossener Schritt – GASP-NET-01

- PR #147 am 26.09.2026 um 10:24:29 UTC bestätigt gemergt:
  `44a5e5127bab0d064608b682fcd002adf91c4159`, Implementierung `b6709eb4`.
  Die folgenden Ergebnisse gehören zum abgeschlossenen NET-01-Auftrag; sie
  wurden für diese Merge-Statuspflege nicht erneut ausgeführt.

- Ownership bleibt bei GAS/Projekt-Mover für Traversal und NetworkPrediction
  für History/Replay. Konkrete Blueprint-/Montage-/Chooser-Inhalte unverändert;
  Nachweisinstrumentierung gehört in das bestehende native Editor-Testharness.
- Begrenzte echte Forward-History mit dem restaurierten/replayten Zustand
  desselben Frames verglichen. Aktiver Request, konkretes Warp-Fenster, Collider,
  Montage und unveränderter lokaler Head bleiben Teil der Abnahme. Veraltete
  History wird bei Restore verworfen, alle Kopien vor PIE-GC freigegeben.
- Finaler Editor-Build erfolgreich; gemeinsame Regression **10/10 bestanden**.
  Mantle: Injektion K=218, korrigierter Frame F=R=219, H=220, ein Replay-Schritt,
  −43,72 cm Gegenkorrektur. Zusätzlich 30-FPS-Limit bei Fixed 50 Hz bestanden:
  K=R=F=206, H=209, drei Replay-Schritte, −50 cm. Damit sind ursprünglicher Frame
  und betroffener Folgeframe tatsächlich ausgeübt.
- Echte Negativkontrolle mit `np.SkipReconcile 1` und `np.ForceReconcile 0`:
  alle Rollen beenden/landen, aber kein Restore-/Replaybeleg; erwarteter Timeout.
  Erste 9/10-Regression und fehlgeschlagener Include-Build bleiben dokumentiert.
- Finaler Regressionslauf: 110 Warnungen, keine Testfehler/Ensures/Fatals;
  zwei bekannte ungeklärte Startmeldungen vor den Tests bleiben erhalten.
  Alle zehn Maps und sieben SaveGames unverändert, vier Overrides verifiziert.
  Editor-/Test-/Buildprozesse beendet. Keine neue handgespielte Sichtabnahme.
- [Nachweis, Grenzen und Nachstellen](gasp-active-mantle-rollback.md), lokale
  ignorierte Belege `Saved/GaspActiveMantleRollback20260926`. NET-02 und weitere
  Registerpunkte bleiben offen; bestehende historische Ergebnisse unten sind
  keine neu ausgeführten Prüfungen dieses Auftrags.

## Vorheriger abgeschlossener Schritt – GASP-STAB-02

- PR #146 am 22.09.2026 um 21:09:22 UTC auf ausdrücklichen Nutzerauftrag
  gemergt: `b9a653608b6ada64dfd1c17b2e24d5dfc691202c`, finaler PR-Head
  `e98e37418d9f3c326bc1883404a54e9e1b60c07e`. Lokaler `master` synchronisiert;
  vier NetworkPrediction-Overrides vor dem Wechsel erfolgreich verifiziert.
  Nachfolgende Statuspflege ändert nur Dokumentation; Builds und Tests dafür
  nicht erneut ausgeführt. Der anschließende Auftrag `GASP-NET-01` ist oben dokumentiert.

- Auf Nutzerauftrag zusätzlich selbst im sichtbaren Editor validiert: Crowd-
  sowie Block-/Follower-Lifecycle **2/2 bestanden**. Unbeleuchtete Fixture für
  eine weitere eigene Sichtprüfung vorübergehend ohne Lighting gerendert,
  Crowd erneut **1/1 bestanden**; Owner-Körper, Laufpose und Waffenangriff in
  echten PIE-Aufnahmen geprüft. Normalen Renderzustand wiederhergestellt,
  Editor in GASP-Testmap offen gelassen. Keine Runtimeänderung; Maps/SaveGames
  und Quell-/Assethashes unverändert. Belege `Saved/GaspRespawnEditor20260922`.
- Echte Zwei-Pawn-Belegung am gespeicherten Checkpoint reproduziert denselben
  Falling-/Nullgeschwindigkeitszustand auf Authority, Owner und Late Observer.
  Normale Eingabe liegt an; `LogMover` belegt scheiternde Penetrationsauflösung.
  Der rote Lauf bleibt mit seinem 35-Sekunden-Bewegungstimeout erhalten.
- Einzige Runtimeänderung: `BP_RpgGasp_Mover` nutzt
  `AdjustIfPossibleButAlwaysSpawn`. Engine-Spawnanpassung erfolgt vor BeginPlay,
  der vorhandene NP-Liaison übernimmt die Position. Keine neue native
  Spawnpolicy, keine Mover-/NetworkPrediction-Änderung, keine Laufzeitteleports.
- Identischer Testquellcode nach ausschließlich dieser Assetänderung grün.
  Der zuvor ungenaue Einzelblocker-Test pinnt seinen Checkpoint jetzt vor Tod;
  begrenzte passive Aufzeichnung hält erste Respawnzustände fest.
- UE 5.8.2 Win64 Development Editor und Game gebaut; final **11/11** in einem
  Lauf bestanden: vier Lifecycle-, zwei Mover-Input/Kamera-, drei Startauswahl-,
  ein AssetComposition- und ein CMC-Fall. Keine Testfehler/Ensures/Fatals,
  526 Warnungen und zwei bekannte Startmeldungen vor den Tests dokumentiert.
- MCP: reflektierte Eigenschaft gesetzt, Blueprint kompiliert/gespeichert/frisch
  geladen. Aktive Graphen und Verbindungen unverändert. Zehn Maps und sieben
  SaveGames unverändert; getestete Quell-/Assethashes nachgeprüft.
- [Bericht und ausführbarer Repro-Befehl](gasp-respawn-falling.md), lokale
  ignorierte Belege `Saved/GaspRespawnFalling20260922`. Der ursprüngliche
  historische Lauf hatte keine Blockerpositionsdaten; der neue Ursachenbeleg
  wird davon getrennt. Vollständig verbaute Checkpoints behalten den bisherigen
  AlwaysSpawn-Fallback; die übrigen GASP-02-Registerpunkte bleiben offen.

## Vorheriger abgeschlossener Schritt – GASP-STAB-01

- PR #145 am 22.09.2026 um 20:05:43 UTC auf Nutzerauftrag gemergt:
  `4039be2560b1733859005ec052865cff0bb03d3b`, finaler PR-Head `82542580`.
  Keine neue manuelle Sichtabnahme; gezielte Reproduktion erfolgte automatisiert.

- Originalen `ClearBlockState`-Ensure nach entferntem DefenseSet in einem frischen
  Editorprozess nachgewiesen; sieben initiale Regressionstests ergaben zuvor
  zwei erfolgreiche und fünf fehlgeschlagene Fälle. Fehlversuche erhalten.
- Fix im bestehenden Block-Ability-Lifecycle: Basiswerte gehören zur exakten
  ASC-/DefenseSet-Instanz; Cleanup verbraucht seinen Snapshot vor Callbacks,
  prüft jede weitere Wiederherstellung und schützt vor mehrfachen/rekursiven Enden.
  Keine globale Grant-Umordnung, keine neuen nativen Klassen oder Assetänderungen.
- UE 5.8.2 Win64 Development Editor und Game tatsächlich erfolgreich gebaut.
  **11/11 Tests in einem Lauf bestanden**: acht native Lifecycle-Tests plus drei
  gerenderte CMC-/Mover-PIE-Fälle für Blockfreigabe, Equipment, Tod/Respawn,
  optionales Retargeting und Late Join mit Owner/Authority/Observer.
- Keine Fehler/Ensures im Testintervall. 238 PIE-Warnungen und zwei ungeklärte
  `Condition failed`-Startmeldungen vor den Tests erhalten; keine Packaged-/WAN-
  oder neue manuelle Sichtabnahme behauptet. Zehn Maps und sieben SaveGames
  unverändert. Test-/Editorprozesse beendet.
- Quelle und Einschränkungen: [Block-Cleanup-Bericht](gasp-block-cleanup.md).
  Lokale, ignorierte Belege unter `Saved/GaspBlockCleanup20260922`.
- Dieser Schritt schloss weder das intermittierende Falling nach Respawn noch
  andere `GASP-02`-Netzwerk-/Messbefunde. Der darauf folgende Auftrag
  `GASP-STAB-02` ist oben mit eigenem Ursachenbeleg und Merge dokumentiert.

## Vorheriger abgeschlossener Runtime-Schritt – GASP-01

- [PR #144](https://github.com/Athurito/SurvivalRpg/pull/144) ist nach ausdrücklicher
  Nutzerfreigabe seit 22.09.2026 gemergt: `aa4447d69d3187dec3592913a1f683b5c91c0a7b`.
  Der lokale `master` wurde per Fast-forward synchronisiert. Diese Statuspflege
  nach dem Merge ändert ausschließlich Dokumentation.
- Bestehende Experience/Space-Ability erweitert; zehn originale grounded Relaxed
  Mover-Hurdle-Montagen, BackFloor und schwacher Landing-Support in Fixed-Historie,
  serverseitige Geometrieprüfung und vorhandener Collision-/Warp-Cleanup.
- Natürliches Montage-Ende berücksichtigt den echten Engine-Endgrund. Eine
  reproduzierte Late-Join-Lücke zwischen ASC-Binding und PawnExtension wurde
  geschlossen. Kein pauschaler Abschluss von `GASP-NET-02`/`GASP-NET-03`.
- UE 5.8.2 Win64 Development **Editor und Game tatsächlich erfolgreich gebaut**
  auf dem Quellstand von `b78edf5b` (`build-editor-03.log`, `build-game-03.log`).
- **47/47 verschiedene Tests bestanden**, drei Läufe mit 2 + 17 + 28 Tests auf
  diesem Stand. Darunter alle 15 Mover-Hurdle-Fälle, echte Fixed-Rollbacks während
  BackFloor-Warping und nach Handoff, Late Join, CMC-Traversal, Mover-Mantle/Vault,
  Equipment und optionales Retargeting. Kein einzelner 47-Test-Lauf.
- Asset-MCP: frisch geladen/kompiliert; zehn Montageverträge und 30 Chooser-
  Referenzänderungen geprüft, authored Query-Graph erhalten, 2.292 Packages in
  der Abhängigkeitshülle ohne `/Game`-Referenz außerhalb `/Game/SurvivalRpg`.
- Zwei getrennte Loopback-Prozessläufe bestanden: Owner-Stand (20 cm) mit 8/8
  Ebenen je Client, Host-Run (40 cm) mit 15/15 je Client; eine Onset-Ebene beim
  Host-Run ausdrücklich ausgenommen. Messtoleranzen und tatsächliche FPS im Bericht.
- Nach allen Läufen 7.637 Bestandsdateien erneut geprüft: nur Query/Chooser
  geändert, zehn neue Montagen; alle zehn Maps und sieben Spielstände unverändert.
  Editor, PIE, Build und sämtliche sechs Probe-Spielprozesse sind beendet.
- Vorherige Fehlversuche, Warnungen, genaue Quell-/Asset-Verträge und aktueller
  Prozessvergleich stehen im [Mover-Hurdle-Bericht](gasp-mover-hurdle.md).
  Lokale Belege: `Saved/GaspMoverHurdle20260922`; ignoriert, nicht automatisch
  in anderen Checkouts verfügbar. Versionierte Automationstests bleiben ausführbar.
- Nutzer-Sichtabnahme am 22.09.2026 nach Editor-Validierung in `Lvl_RpgGaspMover`:
  „schaut gut aus kann gemerged werden“. Die genaue manuelle Rollen-/Gangarten-/
  Hindernisabdeckung wurde nicht einzeln protokolliert; die automatisierten
  Nachweise oben bleiben davon getrennt. Seit `b78edf5b` nur Dokumentation geändert;
  Builds und Tests wurden für diese Abnahmeaktualisierung nicht erneut ausgeführt.
- Kein Packaged-/WAN-Test. Vor einem erneuten Build
  den NetworkPrediction-Override gemäß `Build/Patches/NetworkPrediction/README.md`
  vorbereiten/prüfen.

## Vorheriger akzeptierter Runtime-Schritt – Mover-Vault

- [PR #142](https://github.com/Athurito/SurvivalRpg/pull/142) ist seit 20.09.2026
  gemergt. Feature-Commit `06d810fa`; Nutzer bestätigte die sichtbare Vault-Korrektur
  und beauftragte ausdrücklich den Merge.
- Inhalt: Mover-Vault, Spawn-Auswahl, versionierte Fixed-Interpolationserholung
  und synchronisierte Montage-/Bewegungsdarstellung auf beobachtenden Clients.
- Damals erfolgreich ausgeführt: Win64 Development Editor und Game Builds;
  nach den letzten Korrekturen 15/15 gezielte Tests. Neueste Einzelresultate über
  42 verschiedene Tests sind erfolgreich, **kein einzelner sauberer 42-Test-Lauf**.
- Separater Prozessvergleich: nach Onset-Korrektur beide Observer 16/16; finaler
  Binding-Build ein Observer 16/16 und einer 15 messbare Vergleiche plus ein
  nicht vergleichbarer Startübergang. Der strenge Analyzer-Bericht bleibt fehlgeschlagen.
- Bei 350 ms Paketpause hält die Montage mit der Bewegung; beim Aufholen blieb
  eine geometrische Phasenabweichung von rund 71 ms. Weitere offene Befunde
  werden durch erfolgreiche Sichttests nicht geschlossen.
- Details: [Darstellung](gasp-mover-traversal-presentation.md),
  [Recovery](gasp-mover-network-recovery.md),
  lokale Belege ursprünglich unter `Saved/GaspMoverProxyPose20260920` und
  `Saved/GaspMoverNetFix20260918`. Verfügbarkeit im neuen Checkout prüfen.

## Übergabevorlage für den nächsten Abschluss

Den aktuellen Stand oben ersetzen, dann die letzte konkrete Übergabe darunter
eintragen. Ältere Detailverläufe bleiben in Git/PRs; keine endlose Chatabschrift.

```text
Datum / Aufgaben-ID / Status:
Zuständiger Chat oder Verantwortlicher:
Branch / absoluter Worktree-Pfad / Basis-Commit:
Letzter Implementierungs-Commit / PR-Link / bestätigter Merge, falls erfolgt:

Ziel und tatsächlicher Umfang:
Geänderte Dateien/Assets und Zuständigkeit:
Wichtige Entscheidungen und bewusste Source-Abweichungen:

Tatsächlich ausgeführte Validierung:
- Engine-Version, gebautes Target und Commit/Quellstand:
- Testfilter, Ergebnis und zugehörige Belege:
- Asset-Laden/Compile/Audit und Referenzprüfung:
- Netzwerkrollen/-bedingungen; manueller Sichttest:
- Nicht ausgeführte Prüfungen und erhaltene Fehlversuche:

Offene Punkte mit Roadmap-/Issue-ID:
Editor/PIE/Build-Prozesse noch aktiv? Welcher Checkout und welche Sitzung?
Uncommitted Änderungen, Locks oder andere parallel bearbeitete Dateien:
Nächster konkreter Handgriff:
Abnahmepunkte, die bis zum PR/Merge noch fehlen:
```
