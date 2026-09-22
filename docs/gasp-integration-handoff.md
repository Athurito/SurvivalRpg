# GASP-Integration: aktuelle Übergabe

Zentraler Plan und Aufgabenverträge:
[gasp-integration-roadmap.md](gasp-integration-roadmap.md).
Diese Datei hält den **aktuellen Arbeitsstand**, die Roadmap die Reihenfolge und
Abnahme. Beide bei relevanten Fortschritten im selben PR aktualisieren.

## Aktueller Stand – 22.09.2026

| Feld | Wert |
| --- | --- |
| Aktive Implementierungsaufgabe | Keine; `GASP-01` ist abgeschlossen und gemergt |
| Nächste bereite Aufgabe | `GASP-02` – gezielte Stabilisierung; einen begrenzten Befund aus dem Roadmap-Register auswählen |
| Zuständiger Chat / beanspruchte Dateien | GASP-01 abgeschlossen, keine Dateien mehr beansprucht. Mover Query/Chooser und zehn Hurdle-Montagen übernommen; keine Mapänderung |
| Runtime-Ausgangspunkt | `aa4447d69d3187dec3592913a1f683b5c91c0a7b`, bestätigter Merge PR #144 auf `master` am 22.09.2026 |
| Checkout / abgeschlossener Branch | `master`, `D:/Repos/SurvivalRpg`; abgeschlossener Feature-Branch `codex/gasp-01-mover-hurdle` |
| Letzter Implementierungs-Commit | `b78edf5b69296369a684b57e5103e4e0920dd0c5`; Hurdle-Grundlage `c8961b53ba413ddcd62741d643883d6fda816ac6` |
| Abgeschlossener Arbeits-PR | [PR #144](https://github.com/Athurito/SurvivalRpg/pull/144); Merge bestätigt, finaler PR-Head `159521b3988591f9d176b80dc224ab4588e10533` |
| Nächster Handgriff | Für `GASP-02` Repository-/PR-Stand und offene Befunde prüfen, dann einen einzelnen Stabilisierungsschritt im eigenen `codex/`-Branch bearbeiten |
| Blocker / offene Abnahme | GASP-01-Sichttest am 22.09.2026 akzeptiert. Bestehende Lifecycle-/Netzwerk-Folgearbeiten unter `GASP-02` bleiben offen |

## Letzter abgeschlossener Runtime-Schritt – GASP-01

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
