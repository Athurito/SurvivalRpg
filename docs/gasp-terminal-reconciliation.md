# GASP-NET-02 – Terminalgrund nach Mantle-Reconciliation

Stand: **26.09.2026, implementiert und validiert: Fokusprüfung 3/3,
gemeinsame Regression 28/28 und Editor-/Game-Builds bestanden; Review/Merge offen**.
Branch `codex/gasp-net-02-terminal-reconciliation`, Basis `44a5e512`
(bestätigter Merge PR #147), Implementierung `8de5d927`. Die
[Roadmap](gasp-integration-roadmap.md) führt NET-02 als begrenzten Folgeauftrag
in abgeschlossener technischer Validierung. Ein Arbeits-PR ist noch nicht angelegt.

## Historischer Befund vom 20.09.2026

`FixedCorrectionAfterHandoffCannotRestoreOldTraversal` scheiterte im Lauf
`Saved/GaspMoverProxyPose20260920/regression-onset.log` an der Gleichheit des
Terminalgrundes. Der Request war auf Owner und Authority dieselbe Aktivierung:
Ability 295, Prediction-Key `[45/0]`, Montage-Sequence 1; die lokalen
Montageinstanzen hatten die IDs 66 und 67.

| Logzeile | UTC-Zeit | Beobachtung |
| --- | --- | --- |
| 8877 | 11:06:16.891 | Owner beendet GAS normal: `cancelled=0`, `replicateEnd=1`. |
| 8878 | 11:06:16.904 | Authority verarbeitet das normale Ende: `cancelled=0`, `replicateEnd=0`. |
| 8879 | 11:06:16.908 | Authority-Simulation hat bereits `Cancelled` (Phase 3), Walking und angewandten Endzustand. |
| 8883 | 11:06:16.911 | Owner-Simulation hat `Finished` (Phase 2), ebenfalls Walking und angewandten Endzustand. |
| 8886 | 11:06:16.911 | Erst jetzt injiziert die Fixture den seitlichen 50-cm-Positionsfehler. |
| 8889–8891 | 11:06:16.927–.928 | Reconciliation bei unverändertem lokalen Head 294, Offset 24 und 20-ms-Schritt: Position korrigiert, Identität/Context/Montage/Warp-History erhalten, `lifecycle=0`. |

Die Terminalabweichung bestand somit vor der Testinjektion. Alle Rollen
landeten; die GAS-Endbeobachter zählten normale Enden. Das beweist keine
übereinstimmende Mover-Terminalphase: `EndTraversal` leitet `Finished` oder
`Cancelled` aus den separaten Cleanup-Erfolgsbedingungen ab.

Der damalige Mover-Cleanup protokollierte diese einzelnen Bedingungen nicht.
Eine spätere Wiederholung bestand mit `Cancelled` auf beiden Seiten
(`focused-binding.log`, ab Zeile 6094). Sie klärte die Ursache nicht. Diese
Grenze wurde im [Präsentations-Bericht](gasp-mover-traversal-presentation.md)
und in dessen `final-validation.json` ausdrücklich erhalten.

Der relevante Ability-Code ist im Git-Verlauf nachvollziehbar:
`RpgGameplayAbility_Mantle.cpp` hat in `2d9f5242`, `06d810fa^` und `06d810fa`
denselben Blob `44595e158b02e27bfca1faa31c6fbbd1688fbb63`. Der Lauf entstand
während der Entwicklung von `06d810fa`; nicht jeder Zwischenstand der übrigen
Dateien ist als eigener Commit gespeichert. Die frische Diagnose unten ist
deshalb ein eigenständiger Ursachenbeleg, keine rückwirkend erfundene Messung
der historischen Cleanup-Bools.

## Frischer Ursachenbeleg vom 26.09.2026

Neue lokale Belege liegen unter `Saved/GaspTerminalReconciliation20260926`.
Die vollständigen Aufrufe stehen in `*-command.json`, Ergebnisse in den
jeweiligen `index.json`-Reports. Diese ignorierten Dateien sind in anderen
Checkouts nicht automatisch vorhanden.

Die erste Baseline ohne zusätzliches FPS-Limit bestand **1/1**, weil der
Beobachter bereits `Cancelled → Cancelled` erfasste. Mit `t.MaxFPS 30`
reproduzierte `diagnosis-30fps-01` den ursprünglichen Fehler: Authority Phase 3,
Owner Phase 2, anschließend `expectedPhase=2 beforePhase=2 afterPhase=3`
bei vollständig freigegebener Traversal. Das steht in Zeilen 3598–3612;
der fehlgeschlagene Test und seine unveränderte Assertion bei 3743 und 3878.

`diagnosis-30fps-02` ergänzt passive Verbose-Ausgaben im bestehenden Cleanup.
Die beiden maßgeblichen Zeilen 3551 und 3553 zeigen:

| Cleanup-Bedingung | Owner, 10:30:39.401 UTC | Authority, 10:30:39.430 UTC |
| --- | --- | --- |
| `cancelled` / `gameplayCancel` | 0 / 0 | 0 / 0 |
| `alive` / `capsuleClear` | 1 / 1 | 1 / 1 |
| `remoteEnded` | 0 | 1 |
| `stopped` | 1 | 1 |
| Montageposition | 1,999950 s | 1,966675 s |
| Source-/zulässiger Handoff | 2,000000 s | 2,000000 s |
| Ende des letzten Warp-Fensters | 0,799546 s | 0,799546 s |
| `reachedHandoff` / `supported` | 1 / 1 | 0 / 0 |
| `preserveMomentum` | 1, damit `Finished` | 0, damit `Cancelled` |

`supported=0` ist auf der Authority kein Beleg eines fehlenden Bodens: Der
Support-Trace wird erst bei erreichtem Handoff ausgeführt. Hier verhindert
bereits die Montagezeitprüfung diesen Zweig. Beide Pawns stehen beim Cleanup
am gleichen Ort, und die anschließende normale Floor-Akquisition setzt sie auf
Walking.

Dieser zweite Diagnoselauf besteht trotzdem **1/1**: Die Authority-Korrektur
erreicht den Owner noch vor dessen Beobachtung/Injektion, sodass Zeile 3566
wieder `Cancelled → Cancelled` prüft. Ein grünes Gesamtergebnis bedeutet hier
nicht, dass der falsche Abbruchgrund verschwunden ist.

## Ursache und erforderlicher Runtimevertrag

UE 5.8 definiert `FAnimMontageInstance::IsStopped()` als
`Blend.GetDesiredValue() == 0.f` (`Engine/Source/Runtime/Engine/Classes/Animation/AnimMontage.h`,
Zeile 532). Das gilt bereits im natürlichen Auto-Blend-Out. Der End-Callback
folgt erst nach Abschluss des Blends; die Engine muss dabei nicht genau auf dem
letzten authored Frame landen.

Die bisherige Mantle-Wartebedingung für ein normales Remote-Ende verlangte
`IsPlaying() && !IsStopped()` und eine Position vor dem Handoff. Die Authority
war im natürlichen Blend-Out bereits `stopped=1`; deshalb durfte das normale
GAS-Ende den Cleanup bei 1,966675 s auslösen. Dessen Zeitprüfung gegen
`2,0 - 0,001` lieferte falsch, und der Mover erhielt `Cancelled`, obwohl keine
Gameplay-Cancellation vorlag. Die zuvor vorhandene Behandlung natürlicher
Hurdle-Enden deckte diesen Mantle-Pfad nicht ab.

Der implementierte und zu prüfende Vertrag lautet:

- Bei einem Mover-Mantle, dessen Source-Handoff das natürliche Montageende ist,
  zählt der nicht unterbrochene Engine-End-Callback der exakten Montageinstanz.
  Ein normales Remote-Ende darf diesen Callback auch während Auto-Blend-Out
  nicht vorwegnehmen. Der vorhandene GAS-Task-Enddelegate bleibt erhalten.
- Finale Warp-Grenze, Capsule-Clearance, gültige Unterstützung, aktive
  Ownership und Lebendzustand bleiben erforderlich. Bedingte oder vorzeitige
  authored Handoffs behalten ihre Zeit-/Inputbedingungen.
- Echte Gameplay-Cancellation, Montage-Unterbrechung, Tod, Geometrieverlust
  und Timeout dürfen nicht als natürlicher Erfolg eingeordnet oder aufgeschoben
  werden. Eine autoritative Cancellation bleibt `Cancelled`.
- Reconciliation übernimmt den autoritativen Terminalgrund desselben Requests.
  Eine legitime Korrektur von vorhergesagtem Erfolg zu autoritativem Abbruch
  darf weder alte Traversal reaktivieren noch Lease, Warpziele, Root Motion
  oder Montage wiederherstellen. Identitäts- und Cleanup-Prüfungen bleiben
  dabei aussagekräftig; beliebige Terminalzustände allein genügen nicht.

## Implementierung und identischer Rot-/Grünvergleich

Commit `8de5d927` erweitert in `RpgGameplayAbility_Mantle.h/.cpp` den bereits
vorhandenen natürlichen Hurdle-Endpfad auf Mover-Mantle mit einem Source-Handoff
am vollständigen Clipende. Der Callback der exakten Montageinstanz erfasst den
natürlichen Endgrund vor dem bestehenden GAS-Taskdelegate. Ein normales
Remote-Ende wartet auch im Auto-Blend-Out darauf. Cleanup akzeptiert diesen
Endgrund zusätzlich zur bisherigen Zeitbedingung, weiterhin mit finalem Warp,
Clearance, Support und den bisherigen Cancellation-/Lebendprüfungen. Die
Capture-Guards verhindern, dass ein späterer Engine-Callback eine schon
beendete oder abgebrochene Ability nachträglich zum Erfolg macht.

Die Runtime-Verantwortung bleibt im vorhandenen
[`URpgGameplayAbility_Mantle`](../Source/SurvivalRpg/Traversal/RpgGameplayAbility_Mantle.cpp)
und Projekt-Mover. Der Nachweis gehört in die gemeinsame native Editor-Fixture
und die Mantle-Tests: drei Testdateien, zwei Runtime-Dateien. CMC, konkrete
Blueprint-/Montage-/Chooser-Inhalte und Engine-Overrides bleiben unverändert.

Die zwei gezielten Fälle üben dieselbe reale Blendphase aus und unterscheiden
natürliches Ende von echter Authority-Cancellation. Die Fixture hält nur die
Positionsfortschreibung der exakten Authority-Montageinstanz nach dem letzten
Warp und vor dem Clipende per `SetPlayRate(0)` an; die bereits laufende
natürliche Blendzeit läuft weiter. Der echte Engine-Enddelegate wird beobachtet
und weitergereicht. Der Cancellation-Fall ruft den regulären
`CancelAbilityHandle` auf. Die Ankunft des Owner-End-RPC innerhalb dieser
Blendphase ist ein zusätzlicher Messwert, keine durch diesen Positionshalt
garantierte Reihenfolge.

`red-01` verwendete den erfolgreichen `build-editor-03`, noch ohne Runtimefix
und nur mit passiver Cleanup-Diagnose. Der natürliche Fall endet im echten,
nicht unterbrochenen Engine-Callback bei **1,766684 s**, während das authored
Clipende bei 2,0 s liegt (`red-01.log`, Zeilen 4943–4949). Die Authority hatte
GAS schon vor diesem Callback beendet; alle drei Rollen erhalten `Cancelled`.
Der natürliche Test scheitert deshalb an sieben Assertions. Auch der bisherige
terminale Korrekturtest scheitert mit `Finished → Cancelled` und vier
Assertions. Die echte Authority-Cancellation besteht bereits vor dem Fix.

`build-editor-04` baut danach die Runtimekorrektur. Die kompilierte Testfixture
wurde zwischen Rot und Grün nicht geändert: `fixture-red-compiled.cpp` und der
aktuelle Quellstand haben SHA-256
`C2F695800965684BECA37C284782A55107CA8C60BBE739D30366657A247F1A03`.
`runtime-before-fix.cpp` hält den roten Runtime-Quellstand fest. Beide Läufe
nutzen dieselben drei Tests, `t.MaxFPS 30`, `np.ForceReconcile 0` und
`np.SkipReconcile 0`.

`green-01` besteht **3/3**. Der bestehende terminale Test beobachtet das
aufgeschobene Remote-Ende bei 1,966708 s und anschließend `naturalEnd=1`,
`supported=1` und `Finished` (`green-01.log`, Zeilen 4054–4057). Im gehaltenen
natürlichen Fall wird das Remote-Ende um 10:40:34.061 UTC bei **1,766956 s**
aufgeschoben; der echte Engine-Callback folgt um .062 und derselbe Cleanup
meldet Erfolg. `ownerEndDuringBlend=1`, `authorityEarlyEnd=0` und der bestandene
Test belegen hier tatsächlich diese Reihenfolge und `Finished` auf allen Rollen
(Zeilen 4543–4570).

Die anschließende echte terminale Korrektur entfernt die injizierten 50 cm im
selben beobachteten Frame 313 bei unverändertem lokalem Head 293. Vorher und
nachher gilt Phase 2 (`Finished`), der Endzustand ist angewandt und Lease,
Collider, Root-Move, Ability und alle drei eigenen Warpziele bleiben freigegeben
(`green-01.log`, Zeilen 4068–4071). Identität, Context, Lifecycle, Montage und
Warp-History bestehen die bisherigen Prüfungen; alte Traversal wird nicht
wiederhergestellt.

Die Cancellation greift dagegen synchron mit `gameplayCancel=1` und
`immediateCancel=1`; alle Rollen bleiben `Cancelled`. Ihr späterer Engine-
Endcallback hat trotzdem `interrupted=0`, weil das natürliche Auto-Blend-Out
bereits lief (Zeilen 3553–3582). Das ist kein natürlicher Gameplay-Erfolg:
Der explizite Abbruch und die bereits beendete Ability haben Vorrang.

## Ausführung und verbleibende Grenzen

| Prüfung | Tatsächliches Ergebnis |
| --- | --- |
| `baseline-01` | 1/1 bestanden, sechs Warnungen, 18,49 s; beide beobachteten Terminalgründe `Cancelled`. |
| `build-editor-01.log` | UE 5.8.2 Win64 Development Editor **Succeeded**, 23,27 s; Diagnose-Build, keine Fixabnahme. |
| `diagnosis-30fps-01` | 0/1 bestanden, eine fehlgeschlagene Assertion, 18 Warnungen, 18,83 s; echte Phase-2→3-Abweichung. |
| `diagnosis-30fps-02` | 1/1 bestanden, 18 Warnungen, 18,45 s; Cleanup-Gates belegen weiterhin den falschen Authority-Abbruch vor natürlichem Ende. |
| `build-editor-02.log` | Fehlgeschlagen: drei C4458-Fehler durch lokale `State`-Namen, die ein Fixture-Member verdecken. |
| `build-editor-03.log` | Nach reinem Variablennamensfix **Succeeded**, 11,28 s; Grundlage des roten Laufs. |
| `red-01` | 1/3 bestanden, 30 Warnungen, 33,07 s. Echte Cancellation besteht; natürlicher Abschluss und bestehende terminale Korrektur scheitern, zusammen elf Assertionfehler. |
| `build-editor-04.log` | Finaler Runtime-Editor-Build **Succeeded**, 23,27 s. |
| `green-01` | Identischer Fokusvergleich bei 30 FPS: **3/3 bestanden**, 30 Warnungen (18 Voice-, zwölf Konsolenwarnungen), keine Testfehler, 33,32 s. |
| `build-game-01.log` | Win64 Development Game **Succeeded**, 123,09 s, 37 Aktionen; bekannte C4996-Warnung zu `Chaos::Filter::FInstanceData::GetComponentId` in ChaosMover. |
| Gemeinsame Regression `regression-01` | **28/28 bestanden**, 227,00 s, 287 Warnungen; keine Errors, Ensures oder Fatals im Testintervall. |
| Map-/SaveGame-Erhaltung | Abschlussvergleich mit `preservation-before.json`: alle zehn Maps und sieben SaveGames unverändert. |
| Overrides / Prozesse / Quellstand | Vier NetworkPrediction-Overrides verifiziert; keine Editor-/Test-/Buildprozesse verblieben; getestete Quelldateien identisch mit `8de5d927`. |

Die gemeinsame Regression umfasst 17 Mover-Mantle-Fälle, drei Hurdle-Fälle
(natürliches Ende, Cancellation, Supportverlust), zwei Vault-Fälle (laufender
Handoff, terminale Korrektur), einen gemeinsamen CMC-Traversal-Fall, vier native
Vertragsprüfungen und einen Gameplay-Replay-Fall. Damit sind auch die bestehende
terminale und aktive Mantle-Korrektur, Late Join, Tod und Colliderverlust
einbezogen. Dieser Lauf verwendet kein zusätzliches Render-FPS-Limit; der
separate Rot-/Grünvergleich oben läuft mit 30 FPS bei Fixed 50 Hz.

`audit.json` hält Reports, Fehler, Warnungskategorien und getestete Quellhashes
fest. Der finale Report `regression-01/index.json` hat SHA-256
`d048db53abb2af38774d45ca47990d3874256d4388b3de0a9b179929ff640b15`.
Die 287 Warnungen bestehen aus 140 Voice-, 143 NetPackageMap-, einer Blueprint-,
einer PoseSearch- und zwei NetworkPrediction-Warnungen. Konkret bleiben die
Native-Tick-/Never-Tick-Meldung von `CUI_RespawnScreen`, während
`AsyncBuildIndex` übersprungene PoseSearch-Suchen und
`RollbackFrame == PendingFrame` bei Frame 4/Offset 67 sowie Frame 219/Offset 19
offene Diagnosebefunde; NET-02 behebt sie nicht. Zwei bekannte
`Condition failed`-Startmeldungen vor dem Testintervall sind ebenfalls erhalten.

Dieser Bericht beansprucht keine neue handgespielte Sichtabnahme,
Independent-, Packet-Loss-, Packaged- oder WAN-Validierung und schließt keine
weiteren Roadmap-Punkte. Frühere rote Läufe bleiben als Beleg erhalten.

Nach Review/Merge folgt `GASP-NET-03`: Ausgangspunkt ist die erhaltene Messung
unter `Saved/GaspMoverProxyPose20260920/probe_run_final_host_gap`. Nach 350 ms
Paketpause blieben Bewegung und Montage in 17 eingefrorenen Framepaaren
zusammen stehen; beim Wiederaufholen wich eine Hindernisebene um −70,99 ms ab.
Die rekonstruierte Bahn und ihre Messklammer zuerst auf aktuellem Stand erneut
prüfen. Die bestandene NET-02-Reconciliation ist kein Nachweis für diese
Paketverlust-Erholung.

## Nachstellen ohne manuelles Timing

Nach einem aktuellen Editor-Build und der Override-Prüfung gemäß
`Build/Patches/NetworkPrediction/README.md` führt dieser PowerShell-Aufruf die
drei Fokusfälle aus. Engine-/Projektpfade an den eigenen Checkout anpassen;
der neue Reportpfad überschreibt keinen der oben ausgewerteten Läufe.
Die Fixture erzeugt den Auto-Blend-Out-Fall selbst. Ein lokales Hilfsskript aus
`Saved/` wird dafür nicht benötigt.

```powershell
& 'D:\Programme\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
  'D:\Repos\SurvivalRpg\SurvivalRpg.uproject' '/Engine/Maps/Entry' `
  -unattended -nop4 -nosteam -nosplash -nosound -NoSaveConfig -RenderOffscreen `
  -stdout -FullStdOutLogOutput `
  '-ExecCmds=np.ForceReconcile 0,np.SkipReconcile 0,t.MaxFPS 30,Automation RunTests SurvivalRpg.GASP.Mover.Mantle.GaspMoverMantlePIE.NaturalAutoBlendCompletionRetainsFinishedOnEveryRole+SurvivalRpg.GASP.Mover.Mantle.GaspMoverMantlePIE.AuthorityCancellationDuringAutoBlendRemainsCancelled+SurvivalRpg.GASP.Mover.Mantle.GaspMoverMantlePIE.FixedCorrectionAfterHandoffCannotRestoreOldTraversal' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=D:\Repos\SurvivalRpg\Saved\GaspTerminalReconciliationRepro' `
  '-abslog=D:\Repos\SurvivalRpg\Saved\GaspTerminalReconciliationRepro.log' `
  '-LogCmds=LogRpgAbilitySystem Verbose'
```

Im bereits geöffneten Editor lassen sich vor dem einzelnen Fall
`np.ForceReconcile 0`, `np.SkipReconcile 0`, `t.MaxFPS 30` und
`Log LogRpgAbilitySystem Verbose` über die Konsole setzen. Danach:

```text
Automation RunTests SurvivalRpg.GASP.Mover.Mantle.GaspMoverMantlePIE.NaturalAutoBlendCompletionRetainsFinishedOnEveryRole
```

Die Konsolenwerte nach dem Versuch auf die zuvor verwendeten Werte zurücksetzen.
