# GASP-Integration: aktuelle Übergabe

Zentraler Plan und Aufgabenverträge:
[gasp-integration-roadmap.md](gasp-integration-roadmap.md).
Diese Datei hält den **aktuellen Arbeitsstand**, die Roadmap die Reihenfolge und
Abnahme. Beide bei relevanten Fortschritten im selben PR aktualisieren.

## Aktueller Stand – 26.09.2026

| Feld | Wert |
| --- | --- |
| Aktive Implementierungsaufgabe | `GASP-02` / `GASP-NET-01` – echter Rollback während aktivem Mantle, **In Arbeit** |
| Nächste bereite Aufgabe | Nach NET-01 `GASP-NET-02`: vorhergesagtes Finished gegenüber autoritativem Cancelled klären |
| Zuständiger Chat / beanspruchte Dateien | Dieser Chat: `RpgGaspMoverTraversalTestFixture.cpp`, `RpgMoverPredictionTestTypes.*`, passende native Belegtests und GASP-Dokumentation. Root besitzt Editor/Build; parallele Audits koordinieren ihre Dateigrenzen |
| Runtime-Ausgangspunkt | `b9a653608b6ada64dfd1c17b2e24d5dfc691202c`, bestätigter Merge PR #146 auf `master` am 22.09.2026 um 21:09:22 UTC |
| Checkout / aktiver Branch | `codex/gasp-net-01-active-mantle-rollback`, `D:/Repos/SurvivalRpg`; Basis `b9a65360`, origin/master am 26.09.2026 verifiziert |
| Letzter Implementierungs-Commit | `57fa8de9` – Spawnanpassung im konkreten RPG-Mover-Pawn und gezielter Crowd-/Checkpoint-Nachweis |
| Aktueller Arbeits-PR | Keiner; [PR #146](https://github.com/Athurito/SurvivalRpg/pull/146) und [PR #145](https://github.com/Athurito/SurvivalRpg/pull/145) sind bestätigt gemergt |
| Übernommene Statuspflege | Bestätigten STAB-02-Merge in Roadmap, Übergabe und `gasp-respawn-falling.md` im NET-01-Arbeitsbranch nachgeführt |
| Nächster Handgriff | Bestehenden Mantle-Korrekturfall frisch ausführen; Injektion, restaurierten aktiven Frame und tatsächliche Resimulation eindeutig korrelieren |
| Blocker / offene Abnahme | Kein Implementierungsblocker; NET-01-Build und Nachweis noch offen. Am 26.09.2026 vor Arbeitsbeginn kein Editor/Build aktiv; neue Prüfsitzung ausschließlich durch Root. Vier Plugin-Overrides verifiziert |

## Aktueller Auftrag – GASP-NET-01

- Ownership bleibt bei GAS/Projekt-Mover für Traversal und NetworkPrediction
  für History/Replay. Konkrete Blueprint-/Montage-/Chooser-Inhalte unverändert;
  Nachweisinstrumentierung gehört in das bestehende native Editor-Testharness.
- Die historische Nachweislücke bleibt bis zum frischen framebezogenen Beleg
  offen. Ein Rollbackzähler oder eine durch normales Warping korrigierte Position
  allein genügt nicht. Warp-/Collider-/Montagevertrag nicht abschwächen.
- Lokale neue Belege: `Saved/GaspActiveMantleRollback20260926`; Ausgangshashes
  aller zehn Maps und sieben SaveGames gesichert. Historische Ergebnisse unten
  sind keine neu ausgeführten Prüfungen dieses Auftrags.

## Letzter abgeschlossener Schritt – GASP-STAB-02

- PR #146 am 22.09.2026 um 21:09:22 UTC auf ausdrücklichen Nutzerauftrag
  gemergt: `b9a653608b6ada64dfd1c17b2e24d5dfc691202c`, finaler PR-Head
  `e98e37418d9f3c326bc1883404a54e9e1b60c07e`. Lokaler `master` synchronisiert;
  vier NetworkPrediction-Overrides vor dem Wechsel erfolgreich verifiziert.
  Nachfolgende Statuspflege ändert nur Dokumentation; Builds und Tests dafür
  nicht erneut ausgeführt. Nächster begrenzter Auftrag ist `GASP-NET-01`.

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
