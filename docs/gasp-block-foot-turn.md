# GASP-06: Schritte bei gehaltener Block-Standdrehung

Stand: 27.09.2026. **Drehschritte korrigiert; Nutzer-Sichtabnahme offen.**
Implementierung `1aaee13dbf9ef7194b9ddbde56b865365b7861f9`, Ausgangspunkt `94ac50d3`.
[PR #160](https://github.com/Athurito/SurvivalRpg/pull/160) bleibt offen; dieser
Bericht bestätigt weder einen Merge noch eine Nutzer-Sichtabnahme. Aktuelle Prüfungen und Grenzen stehen unten. Frühere Build-, Cook- und
Testergebnisse gelten nicht automatisch als Prüfung dieses Kandidaten.

## Befund und Auswahlpfade

Die vorige Korrektur verhinderte schnelle Wechsel des Bein-Blend-Gates. Das
beweist weder sichtbare Schritte noch korrekte Fußstellung. Die erneute
Nutzerrückmeldung beschreibt kaum nachsetzende beziehungsweise kreuzende Beine
beim Drehen mit gehaltener rechter Maustaste ohne WASD.

Die Laufzeitabfrage zeigt zwei absichtlich unterschiedliche Locomotion-Pfade:

- **Mover: `LocomotionSetup = 1`, experimentelle State Machine.**
  `CurrentSelectedAnim/Database = None` und leere Motion-Matching-Datenbanken
  sind hier kein Nachweis einer fehlenden Animation. Entscheidend ist das
  bestehende `BlendStackInputs.anim`-Readmodel. Vorher zeigen die zehn
  Bild-begleitenden Abfragen von `mover-feet-before-02` ausschließlich Idle-Clips.
  Im Kandidaten `mover-feet-candidate-01` zeigen drei Abfragen tatsächlich
  `M_Relaxed_Stand_Turn_090_R`. Der gespeicherte State-Name allein bleibt dabei
  `Idle Loop`; er reicht zur Diagnose nicht aus.
- **CMC: `LocomotionSetup = 0`, Motion Matching.** Die 5-Sekunden-Zeitreihen
  veröffentlichen im normalen Muster `M_Neutral_Stand_Turn_090_R`, im langsamen
  Muster `...Turn_045_R`, beim Richtungswechsel `...Turn_090_R` und `...Turn_090_L`.
  Diese Felder enthalten den letzten SearchResult aus dem bestehenden
  PostSelection-Callback, nicht zwingend den dominant gewichteten Clip der
  fertigen Blend-Stack-Pose. Inaktive State-Machine-Felder werden nicht als
  CMC-Auswahl interpretiert.

Die bisherige Block-Ausnahme führte die Root-Rotation während der Drehung eng
an der Character-Rotation nach. Im aktuellen Mover-Vorherlauf blieb der
gemessene Root-Winkelabstand unter 2,95 Grad, `FutureFacingDelta` unter
17,49 Grad. Im Kandidaten erreicht letzterer 55,98 Grad und echte Turn-Auswahl
wird separat beobachtet. Zusammen mit dem geprüften Auswahlgraphen stützt dies
die Diagnose eines unterdrückten Drehbedarfs; ein offenes Bein-Gate allein
garantierte keinen geeigneten ausgewählten Schrittclip.

## Begrenzter Kandidat

Die zwei projektlokalen AnimBPs behalten ihre ursprünglichen Locomotion-Pfade,
den weiter ausgewerteten Offset-Root-Bone-Pfad und nachgelagerte Fußverarbeitung.
Es gibt keinen neuen nativen Selektor und keine Änderung der autoritativen
Bewegungs- oder GAS-Regeln.

- Die zusätzliche UpperBody-Block-Ausnahme in `Get_OffsetRootRotationMode`
  greift nur noch bei ruhendem Beinbedarf. Während Bewegung/Drehung darf die
  ursprüngliche Root- und Turn-Logik arbeiten. Bestehende andere Ausnahmen
  bleiben erhalten.
- Hinter Offset Root Bone wird die gecachte Block-Oberkörperpose ab `spine_01`
  über einen Layered Blend mit Mesh-Space-Rotationsblend erneut gewichtet.
  Kurven stammen aus der Basispose (`UseBasePose`). Der Block-Gewichtsverlauf
  steuert diese Korrektur; ein aktiver DefaultSlot setzt ihr Gewicht auf null.
  Dies ist ein Pose-Blend, keine Behauptung einer ausschließlich rotatorischen
  Operation oder exakter Schildwinkel.
- CMC erlaubt die bestehende Turn-in-place-Auswahl zusätzlich bei aktivem
  UpperBody-Slot. Mover bleibt auf seinem vorhandenen State-Machine-Pfad.
- Frische Dreh-/Translationsbewegung hält den Beinbedarf mit einer Brücke von
  80 ms aufrecht. Der Root-Winkelabstand verlängert diese Frist nicht. Der
  konstante Blend erreicht sein Ziel spätestens nach 150 ms.

Autorierungsbelege: `author-foot-turn-candidate.py`,
`resume-cmc-foot-candidate.py` und die beiden `*-foot-candidate-nodes.json`
unter `Saved/GaspBlockRefinement20260927`. Der erste Autorierungslauf benötigte
für den anderen CMC-Nachfolgeknoten einen getrennten Fortsetzungsschritt.
Die Marker allein sind kein vollständiger Compile-/Cook-Nachweis.

## Gemessene Zeitreihen

Rohbelege und reproduzierbare Definitionen stehen im ignorierten
`Saved/GaspBlockRefinement20260927/foot-turn-analysis.json` einschließlich
SHA-256-Hashes der Eingabedateien. Diese lokalen Dateien werden nicht durch
Git in andere Checkouts übertragen.

Alle sieben Hauptproben liefen fünf Sekunden mit tatsächlicher
Enhanced-Input-Look-Mausaktion auf lokaler Authority. Normal und Reverse
verwenden Amplitude 0,5, langsam 0,1. Das ist keine garantierte Winkelgeschwindigkeit.
Die Tabelle nennt den tatsächlich aufsummierten absoluten Capsule-Drehweg über
die gesamte Probe. Posebereiche und Kreuzungswerte beziehen sich auf die
Zeit **ab 0,5 Sekunden**, um unterschiedliche Anfangs-Blends auszuklammern.

| Probe | Samples | Drehweg gesamt | Root-Abstand min/max | Negative Fußordnung: Samples / Dauer | Fuß-Höhenspanne links/rechts |
| --- | ---: | ---: | ---: | ---: | ---: |
| Mover vorher, normal | 319 | 381,06° | 1,16 / 2,94° | 167 / 2,605 s | 1,32 / 2,32 cm |
| Mover Kandidat, normal | 320 | 382,91° | −2,08 / 41,74° | 0 / 0 s | 7,58 / 3,41 cm |
| Mover Kandidat, langsam | 319 | 76,18° | −1,55 / 47,62° | 0 / 0 s | 8,20 / 5,55 cm |
| Mover Kandidat, Wechsel | 314 | 242,63° | −41,84 / 41,40° | 0 / 0 s | 7,65 / 7,70 cm |
| CMC Kandidat, normal | 316 | 393,75° | −20,89 / 51,02° | 0 / 0 s | 8,26 / 4,55 cm |
| CMC Kandidat, langsam | 324 | 80,75° | −4,57 / 50,04° | 0 / 0 s | 2,36 / 2,85 cm |
| CMC Kandidat, Wechsel | 322 | 255,56° | −50,37 / 50,46° | 0 / 0 s | 8,26 / 7,87 cm |

In allen sieben ausgewerteten Hauptfenstern sind `Speed2D = 0`, null Wechsel
von `RpgBlockNeedsLegMotion` und null Alpha-Übertritte durch 0,5 gemessen.
Gerade die Vorherprobe zeigt, warum diese Gatewerte keine Fußqualitätsabnahme sind.

Negative Fußordnung bedeutet den negativen projizierten Abstand von rechtem
zu linkem Fuß entlang der aus den beiden Hüftgelenken bestimmten Rechtsachse.
Sie beweist eine anatomische Seitenumkehr, aber nicht allein eine physische
Durchdringung. Die Dauer summiert tatsächliche benachbarte World-Time-Intervalle;
am letzten Sample wird nichts extrapoliert. Die Höhenspanne ist ebenfalls kein
Kontakt- oder Bodendurchdringungstest. Die absoluten Minimalabstände und weitere
Werte bleiben im JSON erhalten; es gibt keinen neuen Qualitäts-Passgrenzwert.

Die separat gestarteten 1,2-Sekunden-Nachläufe enden alle bei Bein-Alpha null.
Der erste Nullwert liegt relativ zu diesen Nachlaufproben bei Mover vorher
1,062 s, Kandidat normal/langsam/Wechsel 0,438 / 0,250 / 0,172 s; bei CMC war
Alpha bereits im ersten Nachlaufsample null. Das ist wegen der Zwischenzeit
für MCP-Ergebnisabfragen keine gemessene Latenz ab dem Ende der Dreheingabe.
Der finale absolute Root-Abstand beträgt höchstens 0,0121 Grad. Beide normalen
Mover-Läufe kehren zu etwa 103,19 cm Fußabstand der Block-Standpose zurück;
die schnellere Rückblendung im Kandidaten bleibt gesondert visuell zu bewerten.

## Bildbelege und offene Abnahme

`mover-feet-before-02` und `mover-feet-candidate-01` bleiben **fehlgeschlagene
Aufnahmeläufe mit jeweils zehn Bildern**. Beim ersten überstieg Screenshot-/
Property-Latenz das 5-Sekunden-Fenster. Die erste 8-Sekunden-Kopie behielt noch
eine feste 5-Sekunden-Abbruchgrenze. Ihre Teilbelege werden nicht gelöscht oder
nachträglich als vollständige Aufnahme bezeichnet.

Der korrigierte Originalrunner verwendet durchgängig `--duration` mit erlaubten
5–8 Sekunden und Standard acht Sekunden. `cmc-feet-candidate-02` und
`mover-feet-candidate-02` sind vollständig: jeweils zwölf sequentielle,
zeitgestempelte Bilder, benachbarte Property-Abfragen, Probe und Nachlauf,
anschließend dokumentiertes RMB-/Probe-Aufräumen. Die Bilder sind kein Video
mit garantierten vier Bildern pro Sekunde. MCP-Aufrufe beeinflussen die
Maus-/Sample-Kadenz; die beiden 8-Sekunden-Reverse-Proben haben nur 175/169
Samples und asymmetrische Nettodrehungen von −48,09°/−38,05°. Sie sind keine
zeitlich identische Wiederholung der separaten 5-Sekunden-Proben.

Die erfassten Bilder wurden auf Fußstellung, Schulterzusammenhang und Schildhaltung
geprüft. In den betrachteten Drehbildern setzen die Füße nach; die Nahaufnahmen
zeigen zusammenhängende Schultern/Arme. Die ruhige Blockpose bleibt breit.
Diese Stichproben ersetzen keine subjektive Bewertung sämtlicher Übergänge
bei voller Bildrate und keine erneute Nutzer-Sichtabnahme.

## Aktuelle technische Prüfung

- Beide gespeicherten AnimBPs frisch geladen und mit `warnings_as_errors`
  kompiliert; Editor vor/nach Prüfung sauber. Direkte Abhängigkeiten unverändert
  (CMC 45, Mover 53), projektlokale Assets. Exportreview gegen den vorherigen
  Stand ohne konkreten Blocker: ursprüngliche Root-/Feet-/Mover-DeadBlend-Pfade,
  DefaultSlot-Vorrang, Mover-Modi, Split-Pins und ThreadSafe-Metadaten erhalten.
- Neuer Lauf `foot-validation-01`: **17/18**, 170,87 s. Fehlgeschlagen ist
  `MoverBlockFacesCameraAndPreservesDirectionalMovementSpeed`: im Abschnitt
  `BlockedDirection2` verletzen Authority und Owner jeweils Tempo und Richtung
  (Minimum ca. 124 cm/s, Richtungsdot ca. 0,424; erwartet etwa 375 cm/s).
  GAS-Zustand, Kameraausrichtung und Bodenkontakt stimmen. Der Observer zeigt
  zu diesem Zeitpunkt noch volle Geschwindigkeit. Der Release-Rollback-Test
  besteht diesmal; seine historisch enge Zeitfenstergrenze bleibt bestehen.
- **Genau eine unveränderte Einzelprüfung** `foot-direction-isolation-01`
  besteht: 1/1, 26,99 s. Sie ersetzt den Fehllauf nicht. Der danach gezielt
  instrumentierte Lauf `foot-direction-contact-01` scheitert erneut: 0/1,
  31,80 s, fünf Assertions. Die passive Aufzeichnung bestätigt im nachgestellten
  Fall Kapselkontakt mit dem ruhenden Listen-Host: bei unverändertem S-Input
  sinkt Owner-Tempo von 373,97 auf 190,52 und 68,27 cm/s, sobald der Abstand
  60,75 beziehungsweise 60,10 cm erreicht. Beide Kapseln haben 30 cm Radius;
  anschließendes tangentiales Gleiten und derselbe Authority-Verlauf liegen
  innerhalb des eigentlichen Messfensters nach der Anlaufsekunde. Der Late-Join-
  Pawn ist nicht der Kontaktpartner. Es wurde kein FHitResult aufgezeichnet;
  die Schlussfolgerung beruht auf Geometrie, Zeitbezug, Eingabe und Bewegung.
  Der frühere uninstrumentierte Fehler bleibt separat erhalten, sein konkreter
  Kontaktpartner wird nicht nachträglich als bewiesen bezeichnet.
- Die Test-Fixture setzt ihre drei regulären Starts deshalb jetzt 5000 statt
  500 cm auseinander. Boden, echte Eingaben, Kollisionen und alle Assertions
  bleiben erhalten. Maximal 10000 cm Startabstand hält die Pawns innerhalb der
  normalen Netzrelevanz; die Richtungsrouten schneiden keine ruhenden Startkapseln
  mehr. Nur `RpgGaspMovingBlockTests.cpp` wurde zusätzlich geändert. Frischer
  `SurvivalRpgEditor Win64 Development`-Build: Exit 0, 16,01 s. Der neue volle
  Regressionslauf `foot-validation-02` besteht **18/18**, 184,75 s, 804 Warnungen.
  Testfix-Commit: `fc2e2b6c7954e95b8dedd8daf49eab9cb8842faa`. Die früheren
  17/18, 1/1 und instrumentierten 0/1 bleiben als getrennte Läufe erhalten.
- Frischer Hashaudit: nur die zwei AnimBPs geändert, 4660 andere Assets/Maps und
  sieben persönliche Saves unverändert gegenüber dem dokumentierten historischen
  Manifest `f3dd0473`. Die zwei unmittelbar vorherigen `94ac50d3`-Assetkopien
  und aktuelle Bytes sind zusätzlich im neuen Manifest gehasht. Vier lokale
  Plugin-Overrides verifiziert. Keine Runtime-C++-Änderung; der zusätzliche
  Editor-Testfix ist oben getrennt vom Assetstand und dessen Cook dokumentiert.
- Neuer Windows-Cook für fünf Maps: tatsächlicher Child-Exit **0**, 103,51 s,
  3125 Pakete gekocht, null inkrementell und sieben platformbedingt übersprungen.
  Null Fehler/drei Warnungen: GameplayCue-Suchpfad, MCP-Lizenzhinweis und
  `CUI_RespawnScreen`-Tick. Kein Laufzeittest einer gepackten Fassung.

Versionierter [Mess- und Prüfmanifest](assets/gasp-block-foot-turn.json),
lokale Rohbelege unter `Saved/GaspBlockRefinement20260927`. Die lokalen Drehproben
prüfen keine kontinuierliche Fußqualität auf Owner/SimProxy/Late Join und keine
vollständige Waffen-, Gait- oder Retarget-Matrix. Die Fehlerläufe und die
Kontaktaufzeichnung bleiben erhalten. Es liegt keine Freigabe zum Merge vor.

## Einfache erneute Sichtprobe

Der frische Editor PID **4984** läuft im isolierten UserDir mit Standalone-PIE
in `Lvl_RpgGaspMover`; Viewport maximiert, kein Diagnose-Observer und keine
synthetisch gehaltenen Tasten.

In `Lvl_RpgGaspMover` RMB halten, ohne WASD langsam und zügig drehen, die Richtung
wechseln, dann die Maus ganz anhalten. Die Beine sollen nachsetzen und anschließend
zügig in die Schildstellung zurückkehren. Danach mit gehaltenem RMB WASD nutzen,
Shift versuchen und RMB bei weiterhin gehaltenem Shift loslassen.
CMC lässt sich in `Lvl_RpgGaspMantle` vergleichen; CMC bleibt Run-only.
PR #160 bleibt offen. Neue Nutzer-Rückmeldung zu Schritten und Rückblendung
abwarten; ursprüngliche Fehlerläufe nicht durch spätere Erfolge ersetzen.
