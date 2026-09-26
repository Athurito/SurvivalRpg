# GASP-NET-03 – Traversal-Darstellung nach Paketpause

Stand: **26.09.2026, gemergt in [PR #149](https://github.com/Athurito/SurvivalRpg/pull/149)**:
`d933148f22e8051f0a50ab68719f2f507a6158b5`, bestätigt um 12:27:04 UTC.
Finaler PR-Head `8eafc16db581c898f7ebf3d8a54e486678d306c0`. Arbeitsbranch war
`codex/gasp-net-03-packet-gap-recovery`, Basis
`1490dd7d85306bffa2b2a4b1ab7d9202f1a9b0c4`, bestätigter Merge von PR #148.
Implementierung `49de7c876d72ce5414b75fc25a2708f8755c5525` (acht Tooldateien).
Die [Roadmap](gasp-integration-roadmap.md) führt NET-03 als gemergt und
[VAL-03](gasp-buffered-traversal-replay.md) als validierten Folgeauftrag mit
[Draft-PR #150](https://github.com/Athurito/SurvivalRpg/pull/150) (Testcommit `8bc887e1`; Runtime/Assets unverändert,
Review/Merge offen). Danach folgt der begrenzte GASP-03-Source-Audit.
Die Ergebnisse unten gehören zur NET-03-Abnahme; für diese Merge-Statuspflege
wurden keine Tests oder Prozessläufe erneut ausgeführt. Die Bewertung behebt
die dokumentierte Laufzeit-Rekonstruktionsgrenze nicht.

Die frische Baseline und der portable Pausenlauf reproduzieren Abweichungen zwischen der nach einer Pause
angezeigten Traversal und der autoritativen Bahn. Montage und angezeigte Bewegung
bleiben während fehlender Pakete gemeinsam stehen. Dieser Auftrag
verbessert den reproduzierbaren Probe und die getrennte Messung von
Animationsphase und Geometrie. Ein behobener Laufzeit-Geometriefehler oder eine
angehobene Toleranz wird damit nicht behauptet.

## Historischer Ausgangspunkt

Der [Präsentations-Bericht](gasp-mover-traversal-presentation.md) hält die
Messgrenze des Laufs vom 20.09.2026 fest. Die lokalen, ignorierten Belege liegen
unter `Saved/GaspMoverProxyPose20260920/probe_run_final_host_gap`.

Der Lauf hatte **zwei Prozesse**, nicht drei: Listen-Host und einen Client,
beide mit 60 FPS, Fixed 50 Hz und 100 ms Interpolationspuffer. Der Host führt
einen laufenden Vault aus. Die Dateirolle `owner` beobachtet dabei den
Host-Pawn als `ROLE_SIMULATED_PROXY`; sie bezeichnet nicht dessen autonomen
Besitzer. Die einzige Route verwendet reale Enhanced-Input-Aktionen und die
gespeicherte Mover-Testmap. Der Probe schreibt keine Gameplay-Transforms.

Auf dem Client aktiviert `probe_runtime.py` bei einer beobachteten aktiven
Vault-Montage zwischen 0,30 und 0,60 s `NetEmulation.PktIncomingLoss 100` und
setzt den Wert nach mindestens 350 ms auf null. Damit werden die eingehenden
Pakete dieser Client-Verbindung verworfen; es handelt sich nicht um eine
Verzögerung mit späterer Auslieferung derselben Pakete. Die tatsächlich
aufgezeichnete Pause dauert **363,386 ms**. `owner.log`, Zeilen 2635–2636,
bestätigt das Umschalten um 11:03:48.837 und 11:03:49.200 UTC.

`analysis-v2.json` wertet **16 Ebenen ohne ausgeschlossene Referenzklammer** aus.
Nur X = −430 cm scheitert, 30 cm vor der Hindernisfront bei X = −400 cm:

| Wert an X = −430 cm | Authority | Beobachtender Client |
| --- | --- | --- |
| Interpolierte Montagephase | 0,883503166 s | 0,812516342 s |
| Dauer der aufgezeichneten Messklammer | 17,542362 ms | 18,520117 ms |
| Phase vor / nach der Klammer | 0,883441806 / 0,900108516 s | 0,800001085 / 0,855557024 s |
| Movement Mode in beiden Endpunkten | Traversing | Traversing |

Beide Seiten verwenden dieselbe Vault-Montage. Die Differenz beträgt
**−70,986824 ms**, gegenüber dem bisherigen Vergleichswert
`20 ms + Authority-Klammer + Client-Klammer = 56,062479 ms`.
`gap-analysis.json` bestätigt dagegen **17 zusammenhängende eingefrorene
Framepaare** über insgesamt 282,939 ms mit exakt null Positions- und
Montagefortschritt. Beide Rollen überqueren das Hindernis, fallen dahinter und
landen. Diese beiden Ergebnisse widersprechen sich nicht: korrektes Anhalten
beweist keine identische Bahn beim Aufholen.

Die historische Laufumgebung war UE 5.8.2, CL 56702186. Nach den erhaltenen
Zeitstempeln entstand der Lauf nach dem Onset-Build und vor dem späteren
Binding-Build bzw. dem finalen Präsentationscommit `06d810fa`. Das Manifest
enthält keinen Source-/Binaryhash; der genaue Zwischenstand lässt sich deshalb
nicht rückwirkend mit dem finalen Commit gleichsetzen.

## Frische Baseline vom 26.09.2026

Neue Belege liegen unter
`Saved/GaspPacketGapRecovery20260926/probe_run_baseline_host_gap`:
Manifest und Startparameter, beide Logs und Sample-JSONL-Dateien,
`analysis-v2.json` sowie `gap-analysis.json`.

Die Baseline verwendet ebenfalls zwei Prozesse, Host-Vault bei 60 FPS und eine
angeforderte 350-ms-Pause. Der Probe protokolliert tatsächlich **365,692 ms**.
Beide Prozesse beenden die Route und schließen regulär mit Exitcode 0.

| Prüfung | Tatsächliches Ergebnis |
| --- | --- |
| Gemeinsamer Stillstand | Bestanden: 16 eingefrorene Framepaare, 267,195 ms, gleiche Montage und null Fortschritt. |
| Alte Ebenenmessung | Weiterhin rot: 16 Vergleiche, keine ausgeschlossene Referenzklammer, drei Überschreitungen. |
| X = −470 cm | +59,064710 ms, bisheriger Vergleichswert 52,539368 ms. |
| X = −450 cm | **+74,215583 ms**, bisheriger Vergleichswert 51,494617 ms; größte absolute Abweichung. |
| X = −430 cm | −57,679753 ms, bisheriger Vergleichswert 52,072306 ms. |
| Route / Lifecycle | Host und Client überqueren, fallen und landen; kein Ensure, Fatal oder Mover-Zeitrückgabefehler laut bestehender Auswertung. |

Die neue größte Abweichung hat ein anderes Vorzeichen und liegt an einer
anderen Ebene als der historische Einzelwert. Das ist keine feste allgemeine
Verzögerung von 71 oder 74 ms. Der aufgezeichnete Client-Schritt bei X = −470
und −450 umfasst rund 16,925 ms Echtzeit, aber 275,611 ms Montagefortschritt.
Seine zeitliche und räumliche Auflösung muss bei der Interpretation getrennt
betrachtet werden. Die bisherigen roten Resultate bleiben erhalten.

## Runtime- und Messgrenze

Der Runtimeaudit verfolgt die Erholung über die vorhandene NetworkPrediction-
Interpolation und den GAS/Mover-Präsentationspfad. Die fehlenden Bewegungsschritte
werden zwischen vorhandenen Endpunkten rekonstruiert. Position und Phase können
demselben interpolierten Verlauf folgen, während dieser gerade Verlauf von der
verlorenen gekrümmten Authority-Bahn abweicht. Zwei empfangene Endpunkte liefern
ohne zusätzliche Information keine eindeutige Rekonstruktion aller verlorenen
Zwischenpunkte. Die gemeinsame Uhr und die räumliche Bahn sind daher getrennte
Nachweise.

Die alte Offline-Auswertung vergleicht die Montagephase an der ersten
Vorwärtsquerung derselben Hindernisebene. Sie interpoliert innerhalb zweier
aufgezeichneter Renderproben und addiert deren Echtzeitklammern zu einem
Fixed-Schritt. Beim beschleunigten Aufholen kann innerhalb einer solchen
Klammer deutlich mehr Quellphase vergehen. Eine einzelne geschätzte
Phasendifferenz ist deshalb nicht unmittelbar eine Bone-Pose-Messung oder der
Nachweis einer eigenständig vorauslaufenden Animation.

Die bestehenden JSONL-Proben lesen Montageposition, primären Visual-Transform,
Movement Mode und Landung nach dem Slate-Tick. Sie enthalten keine Bone-Pose,
exakten nativen NP-Interpolationsendpunkte oder direkt gemessene NP-Uhr. Auch
die exakte Montageinstanz ist nicht rollenübergreifend korreliert. Diese Grenzen
bleiben im endgültigen Toolumfang bestehen. Der historische
Freeze-Analyzer prüft Anzahl und summierte Dauer geeigneter Paare; er erzwingt
selbst keine durchgängige Sequenz. Der historische Datensatz enthält die
17 Paare tatsächlich zusammenhängend. Diese Messgrenzen werden im neuen
reproduzierbaren Nachweis ausdrücklich behandelt, nicht durch eine höhere
Bestehensgrenze verdeckt.

## Begrenzter Arbeitsumfang und Ownership

- Getrackter Zwei-Prozess-Launcher und Probe unter `Build/Tools/GaspPacketGap`.
  Reguläre Eingabe, definierte Paketpause, Rollenidentität,
  Prozessabschluss und Rohbelege müssen auch in einem anderen Checkout
  nachvollziehbar sein. Bisherige Hilfsskripte unter `Saved/` sind dort nicht
  automatisch vorhanden.
- Offline-Auswertung trennt beobachtete Phase und angezeigte Geometrie.
  Tatsächliche Messklammern und verbleibende räumliche Fehler sichtbar halten;
  die alte rote Ebenenauswertung bleibt als Vergleichsbeleg erhalten.
- `probe_analyze.py` behält das bisherige Ebenenkriterium bei.
  `phase_analyze.py` ergänzt eine diagnostische Auswertung ohne Passwert:
  Visual-Root-Abweichung zur Authority bei gleicher beobachteter Montagephase,
  mit offengelegter Referenzklammer. Keine Extrapolation und keine Zuordnung
  mehrdeutiger wiederholter Durchläufe desselben Assets.
- Freeze-Analyzer Version 2 verlangt mindestens drei zusammenhängende
  eingefrorene Paare über mindestens 40 ms. Fehlende Zielproben unterbrechen
  die Sequenz; ein fehlgeschlagenes Manifest kann nicht bestehen. Neue
  Ausgabedateien werden exklusiv angelegt, frühere Ergebnisse nicht überschrieben.

Root besitzt Launcher, Probe-Runtime und die Prozesssitzungen. Der Testagent
besitzt den Analyzer. Es werden **keine C++-Dateien oder Assets geändert**;
ein neuer Unreal-Build ist für diese Werkzeuge nicht notwendig und wurde im
NET-03-Auftrag nicht ausgeführt. Gameplay-
Authority, Bewegung und Prediction bleiben bei den bestehenden GAS/Mover/NP-
Schnittstellen; konkrete Animationen und Geometrie bleiben in den vorhandenen
Assets. Dieser Auftrag begründet kein neues Laufzeit-Rekonstruktionssystem.

## Portabler Start und erhaltene Fehlversuche

Der erste portable Kontrollstart `probe_run_control_no_gap` scheiterte vor
dem Probe-Boot an verschachtelten Anführungszeichen in `-ExecCmds`. Das
`host-error.txt`-Signal löste den regulären Fehlerpfad des Launchers aus;
dessen eigener Host wurde beendet. Das Manifest bleibt mit `status=failed`,
`graceful_quit=false` und Exitcode 1 erhalten. Alle 17 geschützten Map-/Save-
Dateien blieben unverändert. Dieser Versuch ist kein erfolgreicher Kontrolllauf.

Der Launcher übergibt den Python-Pfad jetzt ohne innere Anführungszeichen an
UE 5.8 und den Ausgabepfad über eine nur für seine Child-Prozesse gesetzte
Umgebungsvariable. Damit kann auch der Belegpfad Leerzeichen enthalten. Weitere
Reviewkorrekturen stellen den Paketverlust vor Quit, Fehler und Abschluss
zurück, lehnen Paketpausen mit `--driver owner` ab und werten abnormale
Child-Exits als fehlgeschlagenen Lauf. Der korrigierte Kontrollversuch
`portable evidence/probe_run_control_no_gap_02` ist mit `status=measured`
abgeschlossen: beide Child-Prozesse regulär mit Exitcode 0 beendet, alle
17 geschützten Dateien unverändert. Auch der Pausenlauf
`portable evidence/probe_run_gap350_01` ist mit `status=measured`, beiden
Child-Prozessen regulär beendet und 17/17 unveränderten Dateien abgeschlossen.
Erfolgreicher Prozessabschluss allein erfüllt keinen Phasen-/Geometrievertrag.

## Ergebnisse des getrackten Werkzeugs

Die beiden finalen Laufordner liegen unter
`Saved/GaspPacketGapRecovery20260926/portable evidence`. Jeder enthält
`analysis-v2.json` mit unverändertem Ebenenkriterium und `phase-analysis.json`
mit der neuen diagnostischen Geometrieauswertung; der Pausenlauf zusätzlich
`gap-analysis.json` des gehärteten Freeze-Analyzers.

| Beobachtender Client | `probe_run_control_no_gap_02` | `probe_run_gap350_01` |
| --- | --- | --- |
| Alte Ebenenprüfung | **16/16 bestanden** | **13/16 bestanden**, Gesamtergebnis bleibt rot |
| Größte absolute Phasendifferenz an einer Ebene | 33,621050 ms | 86,214577 ms |
| Same-phase-Geometrieproben | 53 ohne Paketpause | 33 nach Ende der Paketpause |
| Größte 3D-Root-Abweichung | 28,542584 cm | 63,996791 cm |
| Größte absolute Z-Abweichung | 24,838125 cm | +63,272468 cm |
| Fehlende Authority-Klammern | Zwei ausdrücklich ausgeschlossen | Zwei ausdrücklich ausgeschlossen |

Die drei roten Pausenvergleiche liegen bei X = −470 cm (**+67,163079 ms**),
−450 cm (**+86,214577 ms**) und −430 cm (**−55,387749 ms**).
Die bisherige Formel und ihre Toleranz sind unverändert. Freeze Version 2
besteht mit **17 zusammenhängenden Paaren über 285,775661 ms** bei einer
tatsächlich protokollierten Pause von **368,282318 ms**.

Der Pausenlauf besitzt keine gültigen `normal`-Vergleiche vor der Pause.
Seine 54 gültigen Geometrieproben verteilen sich auf 21 während der Pause und
33 danach. Eine Vorlauf-Baseline wird daraus nicht erfunden; dafür dient der
eigene Kontrolllauf. `post_gap` bezeichnet ausschließlich die Zeit nach dem
Abschalten des Paketverlusts bis zum Ende des beobachteten Traversal-Segments,
keinen gemessenen internen NetworkPrediction-Recovery-Endpunkt.

Am räumlichen Peak hat der Proxy in Frame 1925 die Montagephase 0,779999793 s.
Die nächste rohe Authority-Probe, Frame 3284 bei 0,783334792 s, liegt nur
**3,335 ms** davon entfernt. Auch dieser direkte Vergleich ohne interpolierte
Authority-Position zeigt ΔXYZ = **[−9,846613; +0,012612; +62,382198] cm** und
**63,154529 cm** 3D-Abstand. Der räumliche Befund hängt somit nicht allein vom
geschätzten Referenzpunkt innerhalb der Authority-Klammer ab.

Schon die Kontrolle hat bis zu 28,54 cm Root-Residuum. Diese Messung kombiniert
Visual-Smoothing, Render-/Montagephase und die Quantisierung der aufgezeichneten
Authority-Referenz; sie isoliert deren Einzelbeiträge nicht. Deshalb sind weder
alle Residuen Paketverlustfehler noch die beiden Maxima einfach als kausaler
Nettofehler voneinander abzuziehen. Gemessen wird der Mesh-/Komponenten-Root,
keine Bone- oder Kontaktpose. Es gibt keinen neuen Passwert für diese Diagnose.

**19/19 Offline-Unittests** bestehen im finalen Lauf in 0,008 s
(`unit-tests-final.log`), einschließlich ungültiger oder mehrdeutiger
Phasenvergleiche und unvollständiger bzw. nicht zusammenhängender Pausenbelege.
Vier negative Launcher-Aufrufe lehnen Authority-Paketverlust, einen Ausgabepfad
außerhalb `Saved`, null FPS und ungültigen Warmup ohne Child-Prozesse ab
(`launcher-negative-checks.json`). Das validiert die Werkzeuge, nicht eine
Änderung der Laufzeitrekonstruktion. Alle Python-Dateien wurden zusätzlich
syntaktisch per AST geprüft. `final-source-verification.json` bestätigt, dass
Launcher und Probe-Runtime beider erfolgreichen portablen Läufe unverändert
dem finalen Quellstand entsprechen.

## Erhaltung, geladene Module und Logbefunde

Der Abschlussvergleich mit dem Snapshot zu Beginn des Auftrags bestätigt alle
**zehn Maps und sieben SaveGames unverändert**. Vier NetworkPrediction-Overrides
wurden erneut verifiziert; nach Abschluss bleiben keine Unreal-Prozesse übrig.
`portable evidence/probe_run_gap350_01/loaded-module-paths.json` hält für Host
46612 und Client 45200 die tatsächlich geladenen Projekt-DLLs `SurvivalRpg`,
`Mover` und `NetworkPrediction` fest. Es wurde kein neuer Unreal-Build ausgeführt.

`Saved/GaspPacketGapRecovery20260926/log-health.json` trennt Startbefunde vom
Zeitraum ab `trigger.start_utc` bis zum Shutdown. Kontrolle und Pausenlauf haben
jeweils **82 Startup-Warnungen** (38 Host, 44 Client) und **116
`LogPython: Error`-Zeilen** (58 pro Prozess) aus der Toolset-Initialisierung im
`-game`-Prozess. Die frühere Baseline hat separat 83 Startup-Warnungen und
ebenfalls 116 dieser Python-Fehlerzeilen. Diese Befunde bleiben erhalten und
werden nicht durch die erfolgreichen Routen als behoben ausgegeben.
Ab dem jeweiligen Trigger bis zum Shutdown enthalten die Logs keine
Warning-/Error-Meldungen.

## Nachstellen aus einem anderen Checkout

Die [Tool-README](../Build/Tools/GaspPacketGap/README.md) beschreibt den
gemeinsamen Ablauf, Voraussetzungen und Grenzen der einzelnen Auswertungen.

Voraussetzungen sind Windows, Python **3.11 oder neuer**, eine lizenzierte
UE-5.8.2-Installation und ein bereits gebauter SurvivalRpg-Checkout. Die
folgenden PowerShell-Befehle werden im Repository-Root ausgeführt. `--engine`
ist erforderlich und bezeichnet das Engine-Verzeichnis. `--output` ist optional,
standardmäßig `Saved/GaspPacketGap`, und muss innerhalb von `Saved` dieses
Checkouts bleiben. Der Launcher prüft vor dem Start die vorhandenen Overrides,
protokolliert Skript-/Binärhashes und schützt Maps und bestehende SaveGames.
Ein Binärhash ist kein neuer Buildnachweis.

Kontrolle ohne Paketpause:

```powershell
python Build/Tools/GaspPacketGap/probe_launch.py control_repro `
  --engine 'D:\Programme\UE_5.8\Engine' `
  --output 'Saved/GaspPacketGap/repro evidence' `
  --driver host --gait run --without-observer --host-fps 60 --owner-fps 60 `
  --port 17994 --observer-gap-ms 0
```

Danach derselbe Host-Vault mit angeforderter 350-ms-Pause:

```powershell
python Build/Tools/GaspPacketGap/probe_launch.py gap_repro `
  --engine 'D:\Programme\UE_5.8\Engine' `
  --output 'Saved/GaspPacketGap/repro evidence' `
  --driver host --gait run --without-observer --host-fps 60 --owner-fps 60 `
  --port 17994 --observer-gap-ms 350
```

Für jede Wiederholung ein neues Label verwenden; der Launcher überschreibt
keinen vorhandenen Laufordner. Die Offline-Auswertungen lesen die gespeicherten
Rohdaten und starten keinen Unreal-Prozess:

```powershell
python Build/Tools/GaspPacketGap/probe_analyze.py 'Saved/GaspPacketGap/repro evidence/probe_run_gap_repro'
python Build/Tools/GaspPacketGap/probe_gap_analyze.py 'Saved/GaspPacketGap/repro evidence/probe_run_gap_repro'
python Build/Tools/GaspPacketGap/phase_analyze.py 'Saved/GaspPacketGap/repro evidence/probe_run_gap_repro'
```

Für die Kontrolle entsprechend `probe_run_control_repro` verwenden und den
Pausen-Analyzer auslassen. `analysis-v2.json` enthält das bisherige
Bestehenskriterium, `gap-analysis.json` den Stillstandsnachweis und
`phase-analysis.json` ausschließlich Diagnosewerte. Ein erfolgreich beendeter
Analyzer-Prozess bedeutet nicht, dass ein Messvertrag bestanden wurde; die
jeweiligen JSON-Ergebnisse und ihre Ausschlüsse sind maßgeblich.

## Abschluss und verbleibende Arbeit

| Arbeit | Stand |
| --- | --- |
| Historischen Befund und frische Baseline prüfen | Vorhanden; Ergebnisse oben. |
| Getrackter Probe / neue Auswertung | Implementiert und mit realen Kontroll-/Pausenläufen validiert; keine C++-/Assetänderung. |
| Offline-Tests / Launcher-Negativprüfungen | 19/19 Unittests und vier erwartete CLI-Ablehnungen ohne Child-Prozesse bestanden. |
| Erster portabler Kontrollstart | Vor Probe-Boot fehlgeschlagen; Fehler und unveränderte 17 Dateien dokumentiert. |
| Normale Kontrolle und Pause mit neuen Metriken | Abgeschlossen; Kontrolle 16/16, Pause 13/16 nach unverändertem Ebenenkriterium, Freeze bestanden. Räumliche Grenze bleibt sichtbar. |
| Unreal-Build | Nicht erforderlich und nicht ausgeführt; vorhandene Binaries verwendet. |
| Erhaltungsprüfung / Overrides / Prozesse | Zehn Maps und sieben SaveGames unverändert, vier Overrides verifiziert, keine Unreal-Prozesse verblieben. |
| Review / Merge | PR #149 am 26.09.2026 um 12:27:04 UTC bestätigt gemergt; Merge `d933148f`. |

NET-03 bewertet die bekannte Rekonstruktionsgrenze und liefert einen wiederholbaren
Nachweis. Es behebt die verlorene gekrümmte Bahn nicht. Der aktive Folgeauftrag
ist **[GASP-VAL-03](gasp-buffered-traversal-replay.md)**: Traversal B trifft nach gewöhnlicher Montage-Ersetzung
ein, während A noch im Präsentationspuffer liegt. Die vorhandene Play-Token-
Korrelation soll genau in dieser Reihenfolge ausgeübt werden. `GASP-VAL-01`
bleibt eine dokumentierte Onset-Messgrenze; `GASP-VAL-02` bleibt die offene
Prüfung in Produktionsumgebungen.

Der [NET-02-Bericht](gasp-terminal-reconciliation.md) dokumentiert separat die
bereits gemergte Mantle-Terminalkorrektur. Aus den dort bestandenen Builds und
28 Tests folgt keine Paketverlust-Abnahme. Auch die vorliegenden lokalen
uncooked Läufe sind keine Packaged-/WAN-Freigabe oder neue handgespielte
Sichtabnahme.
