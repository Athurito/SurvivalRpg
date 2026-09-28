# GASP-07 – CMC-Sprint mit GAS-Ausdauer

Status: technisch und durch eigene Sichtaufnahmen geprüft; einfache Nutzersichtprobe offen.
Branch `codex/gasp-07-cmc-gas-sprint`, auf `c357ddb1` aus dem offenen
[Block-PR #160](https://github.com/Athurito/SurvivalRpg/pull/160) aufgebaut.
Die Sprint-Erweiterung erhält einen eigenen PR; der Block-PR ist nicht gemergt.

## Auftrag und Ownership-Entscheidung

Original-GASP vergleichen, CMC-Sprint als GAS-Ability ergänzen und Ausdauer
während tatsächlichen Sprints verbrauchen. Der Nutzer hat einfache automatische
Regeneration nach einer Pause ausdrücklich bestätigt. Mover-Sprint wird dabei
nicht auf ein neues Ressourcensystem umgestellt.

- Autoritative Wahrheit: GAS-Aktivierung und bestehendes `URpgStaminaSet`.
  Server prüft die Sprintfreigabe und verändert Ausdauer mit GameplayEffects.
- Bestehende native Seams: ASC erhält eine aktivierungsgebundene Sprintfreigabe;
  CMC erhält Wunschübertragung, SavedMove-Historie, effektive Tempowahl und
  replizierte Darstellung. Ein Clientwunsch allein berechtigt nicht zum Sprint.
- Genau zwei neue native Typen: AbilityTask für Sprint (Bewegungszeit,
  Erschöpfung, identitätssicheres Cleanup) und AbilityTask für einfache
  Ausdauerregeneration (Authority, Attribut-/Avatar-Lifetime, Verzögerung).
  Diese Mechanismen benötigen Engine-/Prediction-/Lifecycle-Zugriff; keine
  konkrete native Ability und kein zusätzlicher Movement- oder Resource-Manager.
- Designerassets: konkrete `GA_*` direkt von `URpgGameplayAbility`, sofortiger
  Ausdauer-GE, aktivierungsgebundener Regen-Tuning-GE, AbilitySet und Eingabezuordnung.
  Tempo, Verbrauch, Mindestreserve und Regenpause bleiben Asset-Tuning.
- Darstellung: CMC liefert den effektiven Sprintzustand; der bestehende
  GASP-Adapter reicht ihn an seine Animation weiter. Keine GAS-Abfragen im
  Animation-Worker und keine Animation als Gameplay-Autorität.
- Editor: vorhandener Unreal-MCP-Workflow, Änderungen nur an Projektassets.
  Originalprojekt dient ausschließlich als Vergleichsquelle.
- Stabile Tests: Bewegung/Replay/Serverablehnung, Kosten und Regeneration,
  gehaltene Eingabe bei Block/Stillstand, Erschöpfung, Avatar-/Feature-Cleanup,
  Owner/Server/Observer/Late Join und Assetverträge.

## Originalvergleich und bewusste Projektabstimmung

Frischer Original-MCP-Export vom 28.09.2026, Projekt `D:/Repos/GameAnimationSample`,
Asset `/Game/Blueprints/SandboxCharacter_CMC`: `CalculateMaxSpeed` verwendet
Walk 200/180/150, Run 500/350/300 und Sprint 700/700/700 cm/s
(vorwärts/seitwärts/rückwärts). Seine Demo-Felder 165/375/600 werden in den
Graphen nicht verwendet. Bei `bUseControllerDesiredRotation=false` gilt der
Vorwärtswert. Sprint setzt `FullMovementInput` (fester Eingabegang oder
Analogwert über 0,7) und entweder OrientToMovement oder einen Winkel unter
50 Grad zwischen Figur und Eingaberichtung voraus.

Original-Mover: Run 375, Sprint 585 cm/s, belegt durch unveränderte Quellhashes
zum Originalexport vom 26.09.2026. Beide Originalpfade haben also bewusst
unterschiedliche Abstimmungen. Original-Shift/Gamepad-LeftShoulder verwenden
eine IA mit Pressed-/Released-Triggern; dieser Sample-Eingabevertrag wird
nicht blind auf den bestehenden GAS-Hold-Input übertragen.

Das bisherige RPG-CMC lief mit dem allgemeinen Engine-Standard 600 cm/s.
Projektentscheidung: CMC normal 375, Sprint 585 cm/s, passend zum vorhandenen
Mover. Dies ist eine gemeinsame RPG-Abstimmung, keine unveränderte Übernahme
des Original-CMC. Der Sprint bleibt in diesem Schritt CMC-spezifisch.
Starttuning: 15 Ausdauer/s, mindestens 5 beim Start; Regeneration 12/s nach
zwei Sekunden ohne Verbrauch. Stillstand und Block kosten nichts. Gehaltenes
Shift darf nach Block wieder sprinten; Erschöpfung verlangt Loslassen.

Lokale Rohbelege: `Saved/GaspCmcSprint20260928/source-{cmc,input,sprint-action}.json`
und native T3D-Ergänzung. Reflection deckt nicht alle geschützten Properties ab;
die relevanten Graphen/Defaults und Input-Trigger wurden gezielt ausgewertet.
Keine Quellassets gespeichert; Originaleditor ohne schmutzige Pakete geschlossen.

## Umsetzung und Abnahme vom 28.09.2026

Zwölf Projektassets: zwei konkrete GA-Blueprints, zwei Effects, AbilitySet,
CMC-InputConfig, echte Hold-InputAction und ein ergänzender MappingContext;
beide CMC-PawnDatas und Charakter-Blueprints erhalten die Zuweisung. Die
geerbte Gameplay-Komponente wird im jeweiligen Kind überschrieben. Die
gemeinsame Baseline und Mover bleiben erhalten. Der generische MCP-Helfer
`editable_blueprint_component` löst dafür ein tatsächlich dem Kind gehörendes
Komponententemplate auf; Werte, Graphen, Compile und Save bleiben MCP-Aufrufe.

GAS besitzt eine aktivierungsgebundene Sprintfreigabe mit eingefrorenem Tempo.
CMC überträgt nur den Wunsch, prüft auf Authority die eigene Freigabe und
bewahrt deren historische Werte in SavedMoves. Simulated Proxies erhalten
den effektiven Sprintzustand für die Animation. Crouch, Block, Traversal,
Root Motion, Tod und schwacher Analoginput verhindern effektiven Sprint.
Kosten entstehen ausschließlich auf Authority bei echter horizontaler
Sprintbewegung. Regeneration liest das vorhandene Attribut; jede negative
Ausdaueränderung startet die Pause neu. Ein Avatar-/Attributwechsel beendet
die alte Task, ohne Daten eines Nachfolgers zu übernehmen.

Die Netzprüfung fand einen echten Grant-Reihenfolgefehler: Ein aus PawnData
vergebener Regen-Effect wurde vor dem vom GameFeature angelegten StaminaSet
angewendet und blieb wirkungslos. Der Regen-Tuning-GE gehört nun der Task und
wird erst auf dem bereitstehenden Set angewendet. Eigene Handles werden bei
Ende entfernt, gleichklassige fremde Effects bleiben erhalten. Dies wird mit
verspätetem Set, Owner-Cancel, Set-Entfernung und Set-Ersatz geprüft.

| Prüfung | Tatsächliches Ergebnis |
| --- | --- |
| Engine-Overrides | Vier NetworkPrediction/Mover-Overrides vor Builds verifiziert |
| Builds | Editor Win64 Development und Game Win64 Development erfolgreich; letzter Folge-Build betrifft nur den Korrekturtest |
| Sprint | **13/13**: sieben native Fälle, zwei Assetverträge, vier echte Mehrspieler-PIEs; 268 Warnungen |
| Regressionen | **9/9**: CMC/Mantle-Komposition, Remote/LateJoin/Equipment/Respawn, Traversal-RootMotion/Lifecycle, Block-Cap/Replay/Hold, alter Avatar und Mover-Sprint/Block; 277 Warnungen |
| Reale Korrektur | 200 ms Empfangsverzögerung, einmalig 50 cm Owner-Positionsfehler; acht gepaarte SavedMove-Endpunkte im isolierten Lauf, neun im finalen Fokus, jeweils tatsächlich gemessene 50 cm Gegenkorrektur und korrektes Sprint-Replay |
| Sichtprobe | Fünf kurze Eingabefenster auf der Mantlekarte, zehn Aufnahmen und 686 fortschreitende Pose-Samples; Run 375, Sprint 585, Block 157, Wiederaufnahme 585, Stop 0 cm/s; Gait Run/Sprint passend |
| Cook | Windows, fünf Testkarten, Exit 0 in 104,65 s; 3171 gekochte Pakete, sieben plattformbedingt übersprungen; null Fehler, drei Warnungen |
| Review | Unabhängiger Runtime-/Lifecycle-/Prediction-Review, Quellen-Hashabgleich und Engine-Review des Korrektur-Nachweises ohne offene Must-fix-Befunde |

Die 545 Testwarnungen umfassen 494 temporäre NetGUID-/Levelmeldungen,
46 fehlende Voice-Interfaces, drei NetworkPrediction-Rollbackmeldungen,
eine noch laufende PoseSearch-Indexierung und den vorhandenen Respawn-Widget-
Tickhinweis. Die drei Cookwarnungen betreffen GameplayCue-Suchpfade,
den allgemeinen MCP-Lizenzhinweis und dasselbe Widget. Zwei bekannte
`Condition failed`-Meldungen während der Testregistrierung beim Editorstart
liegen außerhalb der erfolgreichen ausgewählten Testergebnisse.

Frühere rote Läufe bleiben im
[Validierungsmanifest](assets/gasp-cmc-sprint-validation.json) erhalten:
6/12, 8/12 und 12/13. Neben dem echten Regenfehler wurden Crouch-Flags und
World-Cleanup im Fixture korrigiert. Der erste Korrekturtest konnte seine
Injektion durch PendingMove-Combining selbst verlieren; der finale wartet
auf den natürlichen Sendestand und vergleicht Endpunkte derselben
Move-Generation vor/nach echtem Replay. Keine Runtime-Testhaken und kein
künstlicher Replay-Zähler. Der isolierte Korrekturlauf und der anschließende
vollständige Fokuslauf sind grün.

Rohbelege liegen ignoriert unter `Saved/GaspCmcSprint20260928`; Hashes und
Zusammenfassung stehen im Manifest. Die Sichtaufnahmen stammen von der
Editor-Kamera und sind keine durchgehende Video- oder Audioabnahme.
Die finale kurze Strecke korrigiert eine verdeckte Endaufnahme am Kartenrand
aus dem vorherigen Versuch. Kein neuer Dedicated-Server-/Mehrrechner-Test.
Ein alter Kommentar im CMC-Graph nennt Sprint noch als Folgearbeit; der
aktuelle Graph und die dokumentierten nativen APIs sind maßgeblich.
Frühere GASP-06-Ergebnisse werden nicht als neue Sprintprüfung ausgegeben.

## Einfache Nutzersichtprobe

`Lvl_RpgGaspMantle` öffnen und Play starten. Mit WASD normal laufen, dann
Shift halten: sichtbar schnellerer Sprint. Währenddessen RMB halten: Block
begrenzt auf sein normales Blocktempo. RMB loslassen bei weiter gehaltenem
Shift: Sprint setzt wieder ein. Shift loslassen: normales Tempo. Länger
sprinten bis zur Erschöpfung: normales Tempo bleibt bis zum Loslassen und
erneuten Drücken von Shift; Ausdauer erholt sich nach zwei Sekunden.

Sprint und Regen sind Asset-Tuning. Die Skalierung von Schild/Schwert und
eine eigene Combat-Sprintpose wurden in diesem Schritt nicht verändert.
Der separate Sprint-PR basiert auf dem weiterhin offenen PR #160.
Bis zur Sichtfreigabe kein Merge und kein Shutdown.
