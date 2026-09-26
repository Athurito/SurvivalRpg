# GASP-Integration: aktuelle Übergabe

Zentraler Plan und Aufgabenverträge:
[gasp-integration-roadmap.md](gasp-integration-roadmap.md).
Diese Datei hält den **aktuellen Arbeitsstand**, die Roadmap die Reihenfolge und
Abnahme. Beide bei relevanten Fortschritten im selben PR aktualisieren.

## Aktueller Stand – 26.09.2026

| Feld | Wert |
| --- | --- |
| Aktive Implementierungsaufgabe | `GASP-02` / `GASP-NET-02` – vorhergesagtes Finished gegenüber autoritativem Cancelled, **implementiert und validiert; Review/Merge offen** |
| Nächste bereite Aufgabe | Nach NET-02 `GASP-NET-03`: historische Abweichung von −70,99 ms nach 350-ms-Paketpause frisch reproduzieren und rekonstruierte Bahn/Messklammer prüfen |
| Zuständiger Chat / beanspruchte Dateien | Root: Runtime `RpgGameplayAbility_Mantle.h/.cpp`, gemeinsame Traversal-Testfixture, Mantle-Tests und sämtliche Editor-/Buildsitzungen. Dokumentationsagent: Roadmap, Übergabe und NET-02-Bericht. Keine Assetänderung |
| Runtime-Ausgangspunkt | `44a5e5127bab0d064608b682fcd002adf91c4159`, bestätigter Merge PR #147 am 26.09.2026 um 10:24:29 UTC; NET-01 änderte nur Editor-Testcode und Dokumentation |
| Checkout / aktiver Branch | `codex/gasp-net-02-terminal-reconciliation`, `D:/Repos/SurvivalRpg`; Basis `44a5e512` |
| Letzter Implementierungs-Commit | `8de5d927` – natürlicher Mover-Mantle-Abschluss trotz Remote-Ende im Auto-Blend-Out, zwei Runtime- und drei Testdateien |
| Aktueller Arbeits-PR | [PR #148](https://github.com/Athurito/SurvivalRpg/pull/148), **Draft, offen**; [PR #147](https://github.com/Athurito/SurvivalRpg/pull/147) und [PR #146](https://github.com/Athurito/SurvivalRpg/pull/146) sind bestätigt gemergt |
| Übernommene Statuspflege | Bestätigten NET-01-Merge in Roadmap, Übergabe und `gasp-active-mantle-rollback.md` im NET-02-Arbeitsbranch nachgeführt |
| Nächster Handgriff | Validierten NET-02-Stand in PR #148 prüfen; nach bestätigtem Merge mit NET-03 fortsetzen |
| Blocker / offene Abnahme | Kein bekannter Implementierungsblocker. Editor/Game, Fokus 3/3 und Regression 28/28 bestanden; Erhaltung und Overrides geprüft, alle Prozesse beendet. Review/Merge offen; dokumentierte Warnungen und Umgebungsgrenzen bleiben bestehen |

## Aktueller Auftrag – GASP-NET-02

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
- Nach Review/Merge ist NET-03 der begrenzte Folgeauftrag. Ausgangspunkt:
  `Saved/GaspMoverProxyPose20260920/probe_run_final_host_gap` und
  [Präsentations-Bericht](gasp-mover-traversal-presentation.md). Nach 350 ms
  Paketpause halten 17 Framepaare die Montage mit der Bewegung; eine Ebene
  bleibt beim Aufholen um −70,99 ms abweichend. Aktuell nachstellen und die
  rekonstruierte Bahn samt Messklammer prüfen; durch NET-02 nicht abgenommen.

## Letzter abgeschlossener Schritt – GASP-NET-01

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
