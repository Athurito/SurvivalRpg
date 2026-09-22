# GASP-Integration: aktuelle Übergabe

Zentraler Plan und Aufgabenverträge:
[gasp-integration-roadmap.md](gasp-integration-roadmap.md).
Diese Datei hält den **aktuellen Arbeitsstand**, die Roadmap die Reihenfolge und
Abnahme. Beide bei relevanten Fortschritten im selben PR aktualisieren.

## Aktueller Stand – 22.09.2026

| Feld | Wert |
| --- | --- |
| Aktive Implementierungsaufgabe | Keine |
| Nächste bereite Aufgabe | `GASP-01` – Grounded Hurdle für Mover |
| Zuständiger Chat / beanspruchte Dateien | Noch nicht zugeteilt; keine Implementierungsdateien beansprucht |
| Runtime-Ausgangspunkt | `master`, Merge `8268b07325f0404617a9e8e70d938d05c581afc6` aus PR #142; bei Start neueren Stand prüfen |
| Dokumentations-Branch | `codex/gasp-integration-roadmap` |
| Dokumentations-PR | Noch keiner; Dokumentation lokal erstellt, Veröffentlichung ausstehend |
| Nächster Handgriff | Branch-/Editor-/Plugin-Stand prüfen, dann Mover-Hurdle-Quellchooser und die CMC-Hurdle-Anbindung gegen die vorhandenen Mover-Seams auditieren; Umfang vor Assetänderungen festhalten |
| Blocker | Keiner für den Quellen-Audit; konkrete Lifecycle-/Netzwerk-Folgearbeiten stehen im Register `GASP-02` |

## Letzter abgeschlossener Runtime-Schritt

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
