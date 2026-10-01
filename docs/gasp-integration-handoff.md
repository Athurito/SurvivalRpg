# GASP-Integration: Übergabe

Aktueller Arbeitsstand für den nächsten Chat. Status, Reihenfolge und
Arbeitsregeln stehen in der [Roadmap](gasp-integration-roadmap.md), Belege und
Verlauf jeder Aufgabe in ihrem Bericht und PR. Beim Abschluss eines Schritts
diese Datei **ersetzen, nicht ergänzen**. Das frühere fortlaufende Protokoll bis
28.09.2026 liegt in der [Git-Historie](https://github.com/Athurito/SurvivalRpg/blob/a2ddbc833212c053df51fa7d3d3e2ec334aabd30/docs/gasp-integration-handoff.md).

## Aktueller Stand

| Feld | Wert |
| --- | --- |
| Stand | 01.10.2026 |
| Aktive Aufgabe, Status, zuständiger Chat | Keine. Kein GASP-Gameplay-Schritt begonnen. |
| Branch, Worktree, Basis / PR | – |
| Offene Abnahmepunkte bis Merge | – |
| Akzeptierter Runtime-Stand | `master` mit [PR #161](https://github.com/Athurito/SurvivalRpg/pull/161), Merge `849dc7aa`, geprüfter Head `78074034`; enthält GASP-06 (#160) und alle früheren GASP-Schritte. Seitdem nur Dokumentations- und Agent-Tooling-Änderungen. |
| Dateibesitz / Sperren | Keine eingetragen. |
| Editor / PIE / Builds | Beim letzten GASP-Abschluss am 28.09.2026 lief kein UnrealEditor-Prozess. Vor eigener Editor-/MCP-Nutzung andere Checkouts und Sitzungen prüfen. |
| Plugin-Overrides | Git-ignoriert und checkout-lokal. In jedem Worktree vor Build oder Branchwechsel gemäß [README](../Build/Patches/NetworkPrediction/README.md) prüfen. |
| Lokale Belege | `Saved/...` existiert nur im erzeugenden Checkout; versioniert sind Zusammenfassungen in Berichten und Manifesten unter `docs/assets`. |

## Nächster Handgriff

Kein Paket ist bereit. Mit dem Nutzer das nächste Paket festlegen, etwa
`GASP-VAL-02` oder einen Punkt aus den offenen Befunden, und es in der Roadmap
als **Bereit** eintragen, bevor die Umsetzung beginnt.

## Offene Befunde

- `GASP-VAL-02`: Cooked/packaged und WAN bzw. gezielt emulierte
  Netzbedingungen nie geprüft (verschiedene Render-FPS, Delay/Jitter/Loss,
  Late Join, Traversal, Korrekturen, Respawn). Lokale uncooked Ergebnisse
  ersetzen das nicht; Voraussetzung für Produktionsreife.
- `GASP-VAL-01`: Im separaten Prozesslauf ist ein Walking→Traversing-Messpaar
  nicht numerisch vergleichbar, weil die erste Traversing-Probe die Montage schon
  enthält. Bei Bedarf Onset-Messung verfeinern; daraus keinen Animationsaussetzer
  ableiten. [Darstellung](gasp-mover-traversal-presentation.md)
- `GASP-NET-03`: Nach Paketpause bleibt die alte Ebenenprüfung 13/16 und die
  Root-Abweichung bei gleicher Phase bis 64 cm (Kontrolle schon 28,54 cm).
  Freeze korrekt; Runtime-Rekonstruktionsgrenze offen, keine Toleranzerhöhung.
  [Bericht](gasp-packet-gap-recovery.md)
- Älterer Pending-Replay-Fall: in der gemeinsamen Regression 15/16, weil die
  Overlap-Beobachtung verfehlt wird; isoliert 1/1. Timingempfindlichkeit offen.
  [VAL-03](gasp-buffered-traversal-replay.md)
- `GASP-STAB-02`: Vollständig verbaute Checkpoints behalten den
  AlwaysSpawn-Fallback; keine allgemeine Garantie. [Bericht](gasp-respawn-falling.md)
- Ragdoll-Pilot: nur stationärer, nicht geduckter Einstieg auf freier ebener
  Fläche, keine kontrollierte Rollkraft; dynamische und gekochte
  Abhängigkeitshülle unbewiesen. [Pilot](gasp-mover-ragdoll-pilot.md)
- Variantenvergleich: keine Nutzer-Sichtabnahme für GASP-04 und keine
  gemeinsame vollständige Gait-Abnahme; CMC-/Mover-Gait-Unterschiede bei Bedarf
  als eigener kleiner Auftrag. [Vergleich](gasp-variant-comparison.md)
- Block und Sprint: Die Nutzerfreigabe ist keine universelle Audio-, Footlock-
  oder Mehrrechnerabnahme; kein Dedicated-Server-Test. Mover-Sprint hat kein
  Ausdauersystem. [Block](gasp-block-locomotion.md), [Sprint](gasp-cmc-sprint.md)
- Wiederkehrende Testwarnungen (NetGUID/NetPackageMap, Voice, NetworkPrediction
  `RollbackFrame == PendingFrame`, PoseSearch-Indexaufbau, Respawn-Widget-Tick)
  und zwei `Condition failed`-Meldungen beim Editorstart sind nicht behoben.
