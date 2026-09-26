# GASP-VAL-03 – Neue Traversal bei noch gepufferter Vorgängerin

Stand: **26.09.2026, Validierung abgeschlossen, Draft-PR #150 offen; Review/Merge
offen**. Fokus und 30-FPS-Prüfung bestanden, Negativkontrolle erwartungsgemäß
rot. Die gemeinsame Regression bleibt **15/16**; der ältere timingempfindliche
Fall besteht unverändert bei isolierter Wiederholung. Branch
`codex/gasp-val-03-buffered-traversal-replay`, Basis
`d933148f22e8051f0a50ab68719f2f507a6158b5`, bestätigter Merge von PR #149
am 26.09.2026 um 12:27:04 UTC. Implementierung
`8bc887e16d9db543f94e6f00f4328aec43ceb19d` (drei Testdateien).
Arbeitsstand: [Draft-PR #150](https://github.com/Athurito/SurvivalRpg/pull/150). Die bestehende Runtime ist unverändert und geschützt;
ein Runtimefehler ist nicht nachgewiesen.

Der begrenzte Auftrag prüft eine konkrete Multiplayer-Reihenfolge:
Traversal A wird autoritativ abgebrochen, eine normale Waffenmontage O über
Equipment-Input gestartet und nach ihrer beobachteten Wiedergabe ebenfalls
regulär abgebrochen. Danach empfängt der beobachtende Client bereits die
GAS-Montage für Traversal B, während A noch in seiner verzögerten
NetworkPrediction-Präsentation liegt. **O ist vor B bereits beendet.** Der
Test behauptet deshalb nicht, A verdränge an diesem Zeitpunkt eine noch
spielende Waffenmontage. Er prüft, dass der neue B-Empfang die alte A nicht
freigibt und B erst mit ihrem passenden Bewegungszustand präsentiert wird.

Die Umsetzung ändert ausschließlich drei Editor-Fixture-/Vault-Testdateien.
Die regulär geprüfte Runtime und Assets sind unverändert; es ist kein
Runtimebug nachgewiesen. Die temporäre Mutation für die Negativkontrolle
ist unten gesondert dokumentiert.

## Vorhandene Abdeckung und Nachweislücke

Der [Präsentations-Bericht](gasp-mover-traversal-presentation.md) dokumentiert
den Schutz vor einer Wiederbelebung alter Traversal nach gewöhnlicher
Montage-Ersetzung. Die vorhandenen Vault-Fälle
`EquipmentMontageReplacesPresentedVaultThenSameAssetCanReplay` und
`EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection` prüfen die
Ersetzung einer bereits sichtbaren bzw. noch ausstehenden Traversal und einen
späteren Replay desselben Montage-Assets.

`TryReplayTraversal` in
`Source/SurvivalRpgEditor/Private/Network/RpgGaspMoverTraversalTestFixture.cpp`
wartet jedoch für **Owner, Authority und Observer** auf alle drei Bedingungen:

- Traversal-Ressourcen sind über `Clean` freigegeben.
- Mover ist wieder `IsOnGround`.
- Die Ersetzungsmontage wird nicht mehr abgespielt.

Erst dann startet dieser bisherige Fixture-Pfad die nächste Traversal. Damit
blieb die engere Reihenfolge offen, in der B bereits über GAS ankommt, während
A auf dem Observer noch gepuffert ist. Die früheren grünen Replay-Ergebnisse
belegten diesen Zeitpunkt nicht. Der neue Fokuslauf und seine Negativkontrolle
adressieren diese Lücke; daraus folgt kein belegter Bug der geschützten Runtime.

## Frische Baseline vom 26.09.2026

Die lokalen, ignorierten Belege liegen unter
`Saved/GaspBufferedTraversalReplay20260926`. `baseline-02/index.json` bestätigt
**1/1 bestanden**, sieben Warnungen, keine Testfehler und 29,761135 s für:

```text
SurvivalRpg.GASP.Mover.Vault.GaspMoverVaultPIE.EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection
```

Dieser Lauf verwendet die bestehende Runtime und vorhandene Binaries, ohne
neue Implementierungsänderung. Er bestätigt die bisherige Abdeckung, nicht
die engere A/B-Empfangsreihenfolge. `preservation-before.json` hält den
Ausgangsbestand von 17 Map-/SaveGame-Dateien fest; der Abschlussvergleich
bestätigt alle 17 Dateien unverändert.

Der erhaltene erste Versuch `baseline-01` ließ im Filter den Abschnitt
`GaspMoverVaultPIE` aus. Es wurden **null Tests ausgeführt**;
`baseline-01.log`, Zeile 2550, meldet `No automation tests matched`.
Trotz Prozess-Exitcode 0 ist das weder ein bestandener Lauf noch ein Nachweis.
Beide vollständigen Aufrufe bleiben in den jeweiligen `*-command.json` erhalten.

Die gezielte Negativkontrolle entfernte vorübergehend ausschließlich den
Tokenvergleich. Ihr erwartetes rotes Ergebnis, die Wiederherstellung und der
anschließend bestandene Editor-Build sowie die Regression und zusätzliche
30-FPS-Prüfung sind unten dokumentiert.

## Erster neuer Testlauf

`build-editor-01.log` bestätigt einen erfolgreichen Editor-Build
(`Succeeded`, **32,58 s**). Anschließend führte `buffered-green-01` den neuen
Fall aus:

```text
SurvivalRpg.GASP.Mover.Vault.GaspMoverVaultPIE.SameAssetReplayReceiptCannotReleaseUnseenReplacedVault
```

Der Lauf **scheitert mit einer Assertion**, vier Warnungen und 41,409275 s.
Der Beobachter erfasste den B-Empfang nicht (`bReceiptWithA=0`, `bWire=-1`)
und ordnete den späteren echten B-Montagestart deshalb fälschlich A zu. Das
ist kein belegter Runtimefehler. Der unverändert erhaltene Report und die
fehlgeschlagene Assertion stehen in `buffered-green-01/index.json` bzw.
`buffered-green-01.log`, Zeile 3775.

Die Runtime-Verbose-Ausgaben zeigen zuvor tatsächlich die Sperre von A mit
`presentedId=1` gegenüber dem bereits empfangenen B-Token `receivedId=3`
(unter anderem Zeilen 3510–3514). Erst rund eine Sekunde später startet die
Montage mit B-Token `wireId=3` (Zeile 3523, 12:41:18.903 UTC). Die unten
beschriebene Empfangsbeobachtung ist inzwischen korrigiert; dieser rote Lauf
bleibt unverändert erhalten. `build-editor-02.log` bestätigt **Succeeded in
12,28 s**. Der korrigierte Lauf ist nachfolgend dokumentiert.

## Korrigierter Fokuslauf und gezielte Negativkontrolle

`buffered-green-02/index.json` bestätigt **1/1 bestanden**, vier Warnungen,
keine Testfehler und **29,624754 s**. Die tatsächlichen Empfangs- und
Präsentationsbelege aus `buffered-green-02.log` sind:

| Ereignis | Beobachteter Zustand |
| --- | --- |
| B-Empfang in Frame 2215 | Echte Wire-Kante O=2 → B=3; präsentierter A-Token=1, A-Phase 0,050728 s, `witnessed=1`, A zuvor nie sichtbar. |
| B-Identitätsbindung in Frame 2216 | Vollständige autoritative Identität Ability 6 / Prediction-Key 4 / Server-Key 1 / Sequence 1 an den festgehaltenen B-Empfang gebunden. |
| B-Start in Frame 2289 | Reale Instanz 4, passender B-Sync, sichtbare und präsentierte Phase jeweils 0,039003 s. |
| Abschluss | 14 aktive A-Proben, davon zwölf nach B-Empfang; `aVisible=0`, `earlyB=0`, `oldReturned=0`. Genau eine Traversalinstanz und 53 B-Proben. |
| Waffenmontage O | Instanz 2, beobachteter Fortschritt 0,015–0,089 s; tatsächlicher Empfang und Gameplay-Cancellation vor B bestätigt. |

Dieser Fokuslauf belegt die geforderte Reihenfolge mit der bestehenden
geschützten Runtime. `negative-control.json` hält deren Originalhash,
den Hash der temporären Mutation und die Hashes aller drei unveränderten
Testdateien fest. Für die Gegenprobe wurde nur der Vergleich des autoritativen
Play-Tokens entfernt. `build-editor-negative-01.log` bestätigt **Succeeded in
17,49 s**.

`buffered-negative-01` scheitert **wie erwartet mit drei Assertions**, vier
Warnungen und **28,855349 s**. Die gültige B-Empfangsbeobachtung liegt vor dem
tatsächlichen Fehler:

| Ereignis | Beobachteter Zustand ohne Tokenvergleich |
| --- | --- |
| Empfang und Bindung in Frame 2320 | O=2 → B=3, aktiver A-Token=1 bei Phase 0,068411 s; `witnessed=1`, A zuvor nie sichtbar. B-Identität ebenfalls in Frame 2320 gebunden. |
| Unzulässiger A-Start danach | Reale Traversalinstanz 4 bei A-Sync und schon empfangenem B, Phase 0,086824 s. |
| Tatsächlicher B-Start in Frame 2390 | Zusätzliche Instanz 5 mit B-Sync, Phase 0,035425 s. |
| Abschluss | 13 aktive A-Proben, zehn nach B-Empfang; `aVisible=1`, `earlyB=1`, zwei Traversalinstanzen, 44 B-Proben. O-Fortschritt 0,014–0,073 s. |

Damit unterscheidet der unveränderte Test den tatsächlich falschen A-Start
von einem späteren gültigen B-Start. Alle drei Testdateihashes sind gegenüber
dem grünen Lauf unverändert. Die temporäre Mutation ist bereits entfernt:
`RpgAbilitySystemComponent.cpp` hat wieder exakt SHA-256
`b142d1382360c7e26e614c6b1bb20d1e82156bc8589287392a8e32b7a16117ec`
und keinen Runtime-Git-Diff. B erreicht in grünem und negativem Fokuslauf auf
allen Rollen `Finished`, ohne gemessenen Phasenfehler; die drei negativen
Assertions betreffen genau den unerlaubten A-/Frühstart und die zusätzliche
Montageinstanz. Der unabhängige Belegabgleich bestätigt beide Läufe.

Der wiederhergestellte Editor-Build besteht in **17,87 s**. Die
Abschlussregression `regression-01` endet mit **15/16 bestanden**, genau einer
Assertion, **129 Warnungen** und **185,932022 s**; kein Fall bleibt unausgeführt.
Der neue VAL-03-Fall besteht, ebenso Late Join, Korrekturen, Mantle, Hurdle und
die vier nativen Vertragsprüfungen. Der rote Fall ist der bestehende
`EquipmentMontageReplacesPendingVaultWithoutDelayedResurrection`: Um
12:50:48 UTC scheitert die Assertion, dass die Equipment-Ersetzung die alte
aktive NP-Traversal des Observers überlappte.

Der neue VAL-03-Fall war bei diesem Fehler noch nicht gestartet. Der Audit
des Altfalls bestätigt eine verfehlte, etwa einen Fixed-Schritt breite
Beobachtungsvoraussetzung: `overlap=0`, keine wiederbelebte Traversal,
Waffenmontageinstanz 26 stabil von 0,017 bis 0,907 s. Das ist eine
Timingempfindlichkeit der vorhandenen Fixture; eine Runtimekorrektur folgt
daraus nicht. Der isolierte Lauf `pending-repeat-01` besteht mit unverändertem
Quellstand: **1/1**, vier Warnungen, keine Testfehler, **38,916065 s**.
Diesmal ist die echte Überlappung belegt (`overlap=1`); O-Instanz 2 schreitet
von 0,023 bis 0,915 s fort, ohne Übernahme oder Neustart der alten Traversal.
Der bestandene Wiederholungslauf macht den roten Batch **nicht** rückwirkend
zu 16/16. Die Timingempfindlichkeit des alten Fixtures bleibt ein Folgepunkt.

## 30-FPS-Nachweis und Abschlussprüfung

`buffered-30fps-01/index.json` bestätigt **1/1 bestanden**, **16 Warnungen**,
keine Testfehler und **35,490707 s** bei 30 Render-FPS und weiterhin 50 Hz
Fixed-Simulation. Empfang und volle Bindung erfolgen in Frame 760:
O2→B3 bei A1, A-Phase 0,113684 s, A zuvor nie sichtbar. B startet in Frame
789 als Instanz 4; sichtbare und präsentierte Phase sind jeweils 0,020354 s.
Sechs aktive A-Proben, davon drei nach B-Empfang, enthalten keinen A-Start,
keinen Frühstart und keine Wiederkehr der alten Traversal. Genau eine
Traversalinstanz, 22 B-Proben, gemessener maximaler Phasenfehler 0 und ein
Notify-Beginn ohne Duplikat bestätigen den Vertrag auch bei diesem Verhältnis
von Render- und Fixed-Takt.

Der finale Umgebungsaudit bestätigt alle zehn Maps und sieben SaveGames
unverändert, alle vier Plugin-Overrides verifiziert und keine verbliebenen
Unreal-Prozesse. Das 30-FPS-Log belegt die vier geladenen Projekt-DLLs; der
Runtime-Git-Diff ist leer. Die Warnungen, unter anderem VoiceInterface- und
CVar-Lookup-Meldungen, bleiben erhalten; die Läufe werden nicht als warnfrei
ausgegeben. Für diese ausschließlich im Editor-Modul liegende Teständerung
wurde kein neuer Game-Build benötigt oder ausgeführt.

## Bestehender Runtimevertrag

Der Projekt-ASC erfasst in `OnRep_ReplicatedAnimMontage` die empfangene
Traversal-Montage und ihren autoritativen `PlayInstanceId`. Für einen
simulierten Mover-Pawn startet dieser Empfang die kosmetische Montage nicht
sofort: Der präsentierte Mover-Zustand bestimmt den Beginn und die Phase.

Eine gewöhnliche Montage-Ersetzung verwendet weiterhin den GAS-Pfad. Der
ASC stoppt seine bisherige Traversal-Präsentation und wartet auf eine neue
korrelierte Traversal. Im Präsentationspfad muss das empfangene Montage-Asset
zum Asset des Mover-Commands passen und der empfangene Play-Token dessen
`PresentationPlayId` entsprechen. Die vollständige Traversalidentität
unterscheidet die konkreten Requests zusätzlich. Ein neuer Empfang desselben
Montage-Assets allein darf deshalb eine alte gepufferte Traversal nicht
freigeben.

Einstieg für den Runtimeaudit ist
`Source/SurvivalRpg/AbilitySystem/RpgAbilitySystemComponent.cpp`, insbesondere
Montage-OnRep und Mover-Traversal-Präsentation. Den bestehenden Vertrag übt
der neue Fokuslauf in der geforderten A/B-Reihenfolge aus; die gezielte
Negativkontrolle belegt die Empfindlichkeit des Tests gegenüber dem fehlenden
Tokenvergleich.

## Implementierte Testarchitektur

Das neue Szenario `ReplayWhileReplacedTraversalBuffered` gehört zur bestehenden
gemeinsamen Fixture und wird durch
`SameAssetReplayReceiptCannotReleaseUnseenReplacedVault` ausgeführt. Die drei
geänderten Dateien sind `RpgGaspMoverTraversalTestFixture.h/.cpp` und
`RpgGaspMoverVaultTests.cpp` im Editor-Modul. Der unabhängige Review meldet
keine Findings; die Ausführungsabnahme bleibt davon getrennt.

Vor PIE setzt ausschließlich dieses Szenario den regulären
`FixedTickInterpolationBufferedMS`-Wert am Settings-CDO auf **1000 ms**.
Der bisherige Wert wird nach PIE wiederhergestellt. Fixed bleibt bei **50 Hz**
mit 64 Frames History; der Test verändert weder Simulationsuhr noch Synczustände
oder Transforms direkt. Damit entsteht die zeitliche Überlappung über den
normalen Interpolationspuffer.

Der Ablauf nutzt echte Gameplay- und Empfangspfade:

1. A muss auf der Authority mindestens **150 ms beobachteten Montagefortschritt**
   erreichen (`LastTime >= FirstTime + 0.15`). Dann erfolgen der
   Authority-Abbruch und realer PrimaryAttack-Input für O.
2. Der Observer muss O tatsächlich empfangen und mehr als 40 ms
   Montagefortschritt beobachten. Danach beendet Gameplay-Cancel O.
3. Nach lokalem Cleanup startet B per W-/Space-Eingabe, während der Observer
   noch einen aktiven A-Zustand präsentiert. A und B verwenden dasselbe
   Montage-Asset, besitzen aber unterschiedliche echte Play-Tokens.
4. Zwischen `OnWorldTickStart` und `OnWorldPreActorTick` beobachtet die Fixture
   die tatsächliche Wire-Kante von O zu B und den gleichzeitigen A-Synczustand.
   B-Token und Asset werden dabei festgehalten. Die vollständige autoritative
   B-Identität darf erst im nächsten Fixed-Schritt verfügbar werden; sie wird
   später an genau diesen festgehaltenen Empfang gebunden. Die ursprüngliche
   A/B-Überlappung wird nicht nachträglich aus dem späteren Snapshot konstruiert.
5. Eine über A und B erhaltene Liste erfasst alle realen Montageinstanzen,
   einschließlich bereits ausblendender Instanzen. Dadurch kann ein kurzlebiger
   falscher A-Start nicht hinter dem jeweils aktiven Montagezeiger verschwinden.
   B durchläuft anschließend die bestehenden Phasen-, Notify-, Finished- und
   Cleanup-Prüfungen.

Die Assertions beziehen sich auf diese konkreten Zustände:

| Abschnitt | Zu belegender Zustand |
| --- | --- |
| Traversal A | Request, Montage und Play-Token der ursprünglichen Traversal identifiziert. |
| Normale Ersetzung | Reale Waffenmontage O erreicht den beobachtenden Client, schreitet fort und endet vor B durch Gameplay-Cancel. |
| Empfang von B | GAS-Play-Token von B tatsächlich empfangen, während der relevante gepufferte Mover-Zustand noch A beschreibt. |
| Alte Präsentation | A bleibt nach O und bei B-Empfang unsichtbar; keine kurzlebige A-Instanz oder vorzeitige B-Präsentation. |
| Präsentation von B | B startet mit dem passenden Mover-Command und der passenden Identität; Verhalten und abschließendes Cleanup bleiben korrekt. |

Die bestehende Tokenlogik bleibt im endgültigen Runtime-Quellstand unverändert.
Die gezielte Negativkontrolle hat den unveränderten Test nach temporärer
Entfernung des Tokenvergleichs scheitern lassen; die Originalruntime ist
wiederhergestellt. Das ist ein Sensitivitätsnachweis für den Test, kein
Fehlernachweis gegen die bereits geschützte Runtime. Build- und
Regressionsergebnisse sind oben getrennt erfasst.

## Ownership und aktueller Stand

Gameplay-Authority, GAS-Montagereplikation, Mover-Prediction und kosmetische
Präsentation bleiben in den vorhandenen Projekt-Schnittstellen. Konkrete
Montagen, Equipment-Assets und Experience-Komposition bleiben designer-owned.

- Root besitzt Runtimeänderungen sowie sämtliche Build-/Editorsitzungen.
- Der Testagent besitzt die drei koordinierten Fixture-/Vault-Testdateien für
  den neuen Nachweis.
- Der Runtimeagent auditiert zunächst rein lesend.
- Der Dokumentationsagent besitzt diesen Bericht, Roadmap, Übergabe und den
  Mergestatus des NET-03-Berichts.

| Arbeit | Tatsächlicher Abschlussstand |
| --- | --- |
| Ausgangsstand | PR #149 bestätigt gemergt; sauberer Folgebranch auf `d933148f`. |
| Vorhandener Replay-Pfad | Wartet vor B auf Cleanup, Grounded und Attack-End aller Rollen. |
| Bestehende Baseline | `baseline-02`: 1/1 bestanden, sieben Warnungen, null Testfehler; `baseline-01` führte wegen falschem Filter null Tests aus. |
| Editor-Builds | `build-editor-01`: Succeeded, 32,58 s; `build-editor-02`: Succeeded, 12,28 s. |
| Erster neuer Testlauf | `buffered-green-01`: 0/1 bestanden, eine Assertion, vier Warnungen, 41,409275 s; B-Empfangsbeobachtung fehlt. |
| Runtimeaudit / Testkorrektur | Testarchitektur und Empfangsbeobachtung implementiert, Review ohne Findings; Runtime/Assets unverändert. |
| Korrigierter Testlauf | `buffered-green-02`: 1/1 bestanden, vier Warnungen, null Testfehler, 29,624754 s. |
| Echte A/B-Empfangsreihenfolge | Im Fokuslauf belegt: O→B-Empfang bei A-Sync, spätere echte B-Präsentation, keine A-Instanz. |
| Negativkontrolle | Negativ-Build Succeeded, 17,49 s; unveränderter Test erwartungsgemäß rot mit drei Assertions, vier Warnungen, 28,855349 s. |
| Implementierungscommit | `8bc887e16d9db543f94e6f00f4328aec43ceb19d`, ausschließlich drei Testdateien. |
| Wiederherstellung / Editor-Build | Originalruntime exakt wiederhergestellt, kein Runtime-Diff; Editor-Build Succeeded, 17,87 s. |
| Gemeinsame Regression | `regression-01`: 15/16 bestanden, eine Assertion im alten Pending-Replay-Fall, 129 Warnungen, 185,932022 s; neuer VAL-03-Fall bestanden. |
| Isolierte Wiederholung | `pending-repeat-01`: unverändert 1/1 bestanden, vier Warnungen, null Testfehler, 38,916065 s; echte Überlappung diesmal belegt. Batch bleibt 15/16. |
| 30 FPS | `buffered-30fps-01`: 1/1 bestanden, 16 Warnungen, null Testfehler, 35,490707 s; gültiger B-Empfang bei unsichtbarer A, genau eine B-Instanz, kein Phasenfehler. |
| Abschlussaudit | Zehn Maps und sieben SaveGames unverändert, vier Overrides verifiziert, geladene Projekt-DLLs belegt, Runtime-Diff leer, keine Unreal-Prozesse. |
| Review / Merge | [Draft-PR #150](https://github.com/Athurito/SurvivalRpg/pull/150) offen; nicht gemergt. |

Die Ergebnisse des [NET-03-Berichts](gasp-packet-gap-recovery.md) gehören zum
bereits abgeschlossenen Paketpausen-Auftrag und wurden für diese Statuspflege
nicht wiederholt. Dessen verbleibende Rekonstruktionsgrenze, die dokumentierte
Onset-Messgrenze aus VAL-01 und die offene Produktionsumgebungsprüfung aus
VAL-02 bleiben getrennte Punkte. Dieser Bericht beansprucht keine
neue handgespielte Sichtabnahme, Packaged- oder WAN-Validierung.

Nach VAL-03-Review und Merge ist **GASP-03** der nächste begrenzte
Auftrag: Source-Audit des originalen Mover-Ragdoll-Pawns und seiner
Abhängigkeiten, noch keine vorausgesetzte vollständige Migration. VAL-01,
VAL-02 und die NET-03-Rekonstruktionsgrenze bleiben dabei ausdrücklich offen.

## In einem anderen Checkout nachstellen

Zuerst bei geschlossenem Editor die projektlokalen Overrides gemäß
[Prepare/Verify-Anleitung](../Build/Patches/NetworkPrediction/README.md)
vorbereiten beziehungsweise verifizieren und anschließend `SurvivalRpgEditor`
bauen. Engine-, Projekt- und Ausgabepfade an den eigenen Checkout anpassen.
Der ignorierte lokale Runner `Saved/GaspBufferedTraversalReplay20260926/validate.py` ist keine Voraussetzung.

```powershell
& 'D:/Programme/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
  'D:/Repos/SurvivalRpg/SurvivalRpg.uproject' '/Engine/Maps/Entry' `
  -unattended -nop4 -nosteam -nosound -NoSaveConfig -RenderOffscreen `
  '-ExecCmds=np.ForceReconcile 0,np.SkipReconcile 0,Automation RunTests SurvivalRpg.GASP.Mover.Vault.GaspMoverVaultPIE.SameAssetReplayReceiptCannotReleaseUnseenReplacedVault' `
  '-TestExit=Automation Test Queue Empty' `
  '-ReportExportPath=D:/Repos/SurvivalRpg/Saved/Val03Repro' `
  '-abslog=D:/Repos/SurvivalRpg/Saved/Val03Repro.log' `
  '-LogCmds=LogRpgAbilitySystem Verbose'
```

Für die zusätzliche 30-FPS-Prüfung `t.MaxFPS 30,` unmittelbar vor
`Automation RunTests` in `ExecCmds` ergänzen und einen anderen Reportpfad
verwenden. Anschließend `index.json` auf genau einen ausgeführten Test und
dessen Ergebnis prüfen: Exitcode 0 allein reicht nicht, wie der erhaltene
Null-Test-Versuch zeigt. Die temporäre Runtime-Mutation gehört ausschließlich
zur dokumentierten Negativkontrolle und ist zum regulären Nachstellen unnötig.
