# GASP-06 – Blockpose beim Drehen im Stand stabilisieren

Stand: 27.09.2026, **historischer Teilschritt; nachfolgende Sichtprobe abgelehnt**.
Die nächste Rückmeldung zeigte kaum nachsetzende/kreuzende Beine und eine späte
Rückkehr zur Schildstellung. Die [Folgekorrektur](gasp-block-foot-turn.md)
behandelt Auswahl echter Drehschritte und die Rückblendung. Die folgenden
Prüfwerte gelten ausschließlich für den hier beschriebenen früheren Stand.
Implementierung `4c0cc320026a317275545c51af1a7a2608a859c6` auf Basis `f3dd0473`.
[PR #160](https://github.com/Athurito/SurvivalRpg/pull/160) bleibt ein offener Draft;
Merge und erneute Nutzer-Sichtabnahme sind offen. Der Nutzer meldete nach der
[vorigen Nachbesserung](gasp-block-refinement.md) normale Laufbewegung, aber starke
Vibrationen beim Drehen auf der Stelle mit gehaltenem RMB. Deren frühere Build-,
Test- und Cook-Ergebnisse sind historische Prüfungen des vorherigen Assetstands.

## Ursache und begrenzte Korrektur

Die Animation entscheidet zwischen der vollständigen Block-Standpose und der
Beinbewegung aus GASP anhand von Beschleunigung, Geschwindigkeit, Drehgeschwindigkeit
und Root-/Actor-Winkelabstand. Mover finalisiert den Actor im festen Simulationstakt;
die sichtbare Mesh-Bewegung wird separat geglättet. Der vorhandene Animations-Snapshot
liest den Actor. Seine abgeleitete Drehgeschwindigkeit enthält deshalb Nullschritte,
obwohl Controller und sichtbares Mesh kontinuierlich drehen. Im normalen roten
Messlauf entsprechen 53 ausgeschaltete Bewegungsproben genau 53 Nullschritten des
Actor-Winkels. Das wiederholte Umschalten der Beinpose erklärt den beobachteten
Jitter; die Pelvis-Winkelsprünge korrelieren dort stark mit den Alpha-Änderungen.

Die Root-Winkelabweichung bleibt in diesem Lauf unter rund 3,1 Grad und überbrückt
die Nullschritte bei den bestehenden Schwellen nicht. Eine zusätzliche zirkuläre
Foot-IK-Rückkopplung ist damit nicht nachgewiesen. Der UpperBody-Slot aktualisiert
seine Quellpose bereits durchgehend (`bAlwaysUpdateSourcePose=True`).

In `ABP_RpgGasp_CMC` und `ABP_RpgGasp_Mover` erhält ausschließlich die kosmetische
Funktion `Update_BlockLocomotion` einen neuen Gleitkomma-Timer:

- Frisch gemessene Bewegung oder Drehung setzt ihn auf **150 ms**.
- Ohne frische Aktivität sinkt er um die Animations-Delta-Zeit, begrenzt auf null.
- Solange Restzeit besteht, bleibt die Beinbewegung aktiv. Der gehaltene Zustand
  selbst erneuert den Timer nicht; die Standpose kann wieder erreicht werden.
- Die Drehschwellen betragen **5 Grad/s zum Einschalten und 2 Grad/s zum Halten**.
  So wird auch langsames Drehen erfasst, bevor einzelne Actor-Schritte ausfallen.

Geschwindigkeitsschwellen 10/3 cm/s, Root-Winkelschwellen 20/10 Grad und die
Alpha-Interpolation mit Faktor 12 bleiben bestehen. Stand-/Bein-Posegraph, Masken,
Slot-Priorität, Root-Release, prozedurale Knoten und 15-%-Lean bleiben unverändert.
Gameplay, normales Bewegungstempo, GAS, Eingaben und Netzwerk-Simulation ändern
sich nicht. Für diese Korrektur wurde kein C++ geändert oder neu gebaut.

## Messbelege

Je Muster wurden fünf Sekunden mit gehaltenem Block und tatsächlicher
Enhanced-Input-Aktion `InputTag.Look.Mouse` aufgenommen, ohne WASD. Die Tabelle
wertet alle veröffentlichten Proben nach den ersten 0,7 Sekunden aus.
„Wechsel“ zählt Änderungen der Entscheidung für Beinbewegung; der Pelvis-Schritt
ist der Winkel zwischen zwei aufeinanderfolgenden Component-Space-Knochenrotationen.

| Mover-Muster | Wechsel vorher → final | Größter Pelvis-Schritt vorher → final |
|---|---:|---:|
| Gleichmäßiges Drehen | 106 → 0 | 10,19° → 1,14° |
| Langsames Drehen | 22 → 0 | 7,90° → 1,09° |
| Wechselnde Drehrichtung | 76 → 0 | 11,17° → 1,19° |

Der erste Kandidat mit 150 ms und den alten Drehschwellen 20/8 Grad/s bleibt als
Zwischenergebnis erhalten: Er beseitigte die Wechsel im normalen Muster, schaltete
beim langsamen Drehen spät ein und ließ beim Richtungswechsel zwei Wechsel übrig.
Die finalen 5/2 Grad/s beseitigen diese Wechsel in den drei erfassten Mustern.

Für CMC liegen frische Aufnahmen ausschließlich des finalen Stands vor: ebenfalls
null Wechsel in allen drei Mustern, maximale Pelvis-Schritte 0,10/0,10/0,09 Grad.
Ein passender CMC-Vorherlauf liegt nicht vor; daraus folgt kein CMC-A/B-Verbesserungswert.
Separate Ruheaufnahmen zeigen bei beiden Varianten die Rückkehr zu deaktivierter
Beinbewegung und Alpha null. Ihr Zeitnullpunkt ist der Beginn der separaten Aufnahme,
nicht die letzte Eingabe. Der einmalige Rückblendvorgang ist kein Nachweis vollkommen
ruckfreier Poseübergänge; dabei bleiben auch größere einzelne Pelvis-Schritte sichtbar.

## Prüfstand und Grenzen

| Prüfung | Aktueller Nachweis |
|---|---|
| Beide AnimBPs | Gespeichert, frisch geladen und kompiliert; Abhängigkeiten 45/53 |
| Unabhängiger Exportvergleich | Ein neuer Timer und begrenzte Funktionsänderung; übrige Graphverträge erhalten |
| Mover-Messung | Vorher, erster Kandidat und final; drei Drehmuster und Ruheproben |
| CMC-Messung | Drei finale Drehmuster und Ruheproben; ohne Vorhervergleich |
| 18 ausgewählte Automationstests | Sammellauf **17/18**, 220,31 s; unveränderte Einzelwiederholung **1/1**, 23,24 s. Alle 18 Fälle zuletzt erfolgreich, kein einzelner 18/18-Lauf |
| Neuer Windows-Cook für fünf Karten | Tatsächlicher Child-Exit **0**, 494,81 s; 3125 gekocht, sieben platformbedingt übersprungen, null inkrementell übersprungen; null Fehler, drei Warnungen |
| Bestand und Overrides | Genau zwei AnimBPs geändert; 4660 weitere Assets und sieben persönliche Saves byteidentisch; vier Plugin-Overrides verifiziert |
| Erneute Nutzer-Sichtabnahme | **Offen; erforderlich vor Merge** |

Die Messungen betreffen jeweils die Authority und veröffentlichte Animationsdaten,
keine NetworkPrediction-Frames oder erzwungene gemeinsame Auswertungsphase. Mover
liefert dabei ungefähr 56–63, CMC ungefähr 55–58 Proben/s während der Drehung; die
Instrumentierung beeinflusst den Ablauf. Gleiche Eingabestärke pro Callback bedeutet
bei verschiedenen Raten keine exakt gleiche Kamerageschwindigkeit. Animationsfelder
und Knochenposen können aus verschiedenen Auswertungsphasen stammen. Die Messungen
belegen deshalb weder perfekte Phasengleichheit noch Owner-/Proxy-/Late-Join-Parität
oder eine allgemeine visuelle Qualitätsgrenze. Die Aktion durchläuft den vorhandenen
Eingabepfad, prüft jedoch keine physische Maus samt Geräte-Mapping und Modifikatoren.

Der fehlgeschlagene Release-Rollback-Test beobachtet eine echte Korrektur um
−50 cm mit neun Replay-Schritten, aber `K=R=F403`: Der ausgewählte Frame ist bereits
der unblocked Authority-Restore und kein historischer blocked Replay-Output.
Das strenge Kriterium `F > R` weist ihn korrekt ab. Der Held-Teil bestand.
Der frühere grüne Lauf traf `R405 < F406`; das enge Fixture-Zeitfenster bleibt
eine dokumentierte Grenze. Testcode, Assertions und Runtime werden für die
Einzelwiederholung nicht geändert. Der Sammellauf enthält 803 Warnungen, die
erfolgreiche Wiederholung 35; das 17/18-Ergebnis wird dadurch nicht ersetzt.
Die Wiederholung belegt Held `K=R=F295` und Release `K396, R394 < F396`, jeweils
−50 cm und zehn Replay-Schritte. Historischer Block, freigegebener aktueller
Head/GAS und unveränderter Head derselben Korrektur sind tatsächlich gemessen.
Die Warnungen betreffen Voice, temporäre Level-NetGUIDs, NetworkPrediction,
ältere Manny-PoseAssets, einen asynchronen PoseSearch-Index und den Respawn-Widget-Tick.
Die drei Cook-Warnungen betreffen GameplayCue-Suchpfade, den MCP-Hinweis und
ebenfalls den Respawn-Widget-Tick. Die längere Cookdauer enthält 270 erfolgreiche
Index-Neubauten statt zuvor 92; kein endloser Wiederholungszyklus wurde beobachtet.

Rohdaten, Zwischenkandidaten, Exporte und die unabhängige Auswertung liegen unter
`Saved/GaspBlockRefinement20260927/`, insbesondere `turn-comparison.json` und
`IsolatedUser/Saved/Gasp06/ABP_{CMC,Mover}-turn-settle-02.t3d`. Dieser Ordner ist
ignoriert und in einem anderen Checkout nicht automatisch vorhanden. Die
[versionierten Ergebnisse](assets/gasp-block-turn-stability.json) erhalten die
Dateihashes, Messwerte, beide Testläufe, Cook- und Erhaltungsnachweise.

## Manuelle Wiederholung

`Lvl_RpgGaspMover` öffnen und Play starten. RMB halten, ohne WASD langsam und zügiger
mit der Maus drehen; anschließend mehrfach die Drehrichtung wechseln. Die Beine
sollen die Drehung begleiten, ohne zwischen Stand- und Bewegungsbeinpose zu zittern.
Die Maus anhalten und RMB weiter halten: die vollständige Block-Standpose soll
wiederkehren. Danach mit RMB und WASD gehen, anhalten und RMB loslassen.
Dasselbe auf `Lvl_RpgGaspMantle` für CMC prüfen. Diese einfache Sichtprobe bleibt die
ausstehende Nutzerabnahme; die Messwerte ersetzen sie nicht.
