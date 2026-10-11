# GASP-Integration: Übergabe

Aktueller Arbeitsstand für den nächsten Chat. Status, Reihenfolge und
Arbeitsregeln stehen in der [Roadmap](gasp-integration-roadmap.md), Belege und
Verlauf jeder Aufgabe in ihrem Bericht und PR. Beim Abschluss eines Schritts
diese Datei **ersetzen, nicht ergänzen**. Das frühere fortlaufende Protokoll bis
28.09.2026 liegt in der [Git-Historie](https://github.com/Athurito/SurvivalRpg/blob/a2ddbc833212c053df51fa7d3d3e2ec334aabd30/docs/gasp-integration-handoff.md).

## Aktueller Stand

| Feld | Wert |
| --- | --- |
| Stand | 11.10.2026 |
| Aktive Aufgabe, Status, zuständiger Chat | Keine. Zuletzt `GASP-STAB-03` Testboden vor den Pawns in CQTest-Netzwerktests, **gemergt**. |
| Branch, Worktree, Basis / PR | Kein aktiver Branch. Zuletzt `claude/gasp-stab-03-test-floor-race` / [#223](https://github.com/Athurito/SurvivalRpg/pull/223), gemergt als `426c3fa3`; davor `GASP-ENG-01` / [#221](https://github.com/Athurito/SurvivalRpg/pull/221) als `e13ab82b`. |
| Offene Abnahmepunkte | Keine. Jeder Checkout muss vor dem nächsten Build seine 5.8.2-Overrides gemäß [README](../Build/Patches/NetworkPrediction/README.md#engine-updates) entfernen und neu stagen. |
| Akzeptierter Runtime-Stand | `master` `426c3fa3`; enthält alle GASP-Schritte bis GASP-07 (#161), GASP-ENG-01 (#221) und GASP-STAB-03 (#223, nur Testcode). Spätere Merges betreffen andere Systeme. |
| Dateibesitz / Sperren | Keine. Eine separate Claude-Sitzung untersucht den zeitabhängigen Remote-Melee-Test aus den offenen Befunden. |
| Editor / PIE / Builds | Installiert ist UE 5.8.3 (CL 58210709), `master` pinnt 5.8.3. Branches ohne GASP-ENG-01 bauen erst nach Übernahme von `master`. PIE-Netzwerktests nur in einem Prozess gleichzeitig laufen lassen. Vor eigener Editor-/MCP-Nutzung andere Checkouts und Sitzungen prüfen. |
| Plugin-Overrides | Git-ignoriert und checkout-lokal. In jedem Worktree vor Build oder Branchwechsel gemäß [README](../Build/Patches/NetworkPrediction/README.md) prüfen; Engine-Wechsel nach dem Abschnitt „Engine updates“. |
| Lokale Belege | `Saved/...` existiert nur im erzeugenden Checkout; versioniert sind Zusammenfassungen in Berichten und Manifesten unter `docs/assets`. |

## Nächster Handgriff

Kein Paket ist bereit: mit dem Nutzer das nächste Paket festlegen, etwa
`GASP-VAL-02` oder einen Punkt aus den offenen Befunden.

## Offene Befunde

- `GASP-ENG-01`: UE 5.8.3 ändert den Mover-Rollback für Bewegungsmodifikatoren;
  nach einem Rollback laufen `OnStart`/`OnEnd` jetzt, beim Mover-Ducken also die
  Kapselanpassung des `FStanceModifier`. Kein Projekttest prüft Ducken während
  eines erzwungenen Rollbacks. [Bericht](gasp-engine-5-8-3.md)
- `CombatRemoteMeleePIE.RemoteClientAttackWindowDamageAndCancellation`
  verfehlt zeitabhängig bei einem zufälligen der 21 Angriffe das Ziel
  (Basis 2 von 4, mit `GASP-STAB-03` 2 von 5 bestanden). Der Boden ist nicht
  die Ursache; vermutlich der feste, inhaltskalibrierte Kontakt-Offset des
  Tests. Außerhalb von GASP; eine separate Sitzung bearbeitet ihn.
  [Bericht](gasp-test-floor-race.md)
- Der VAL-03-Vault-Fall verfehlte unter 5.8.3 seine Überlappungsbedingung in
  3 von 6 isolierten Läufen (siehe Pending-Replay-Fall unten).
- `BlockLocomotionPIE.MoverFixedRollbackPreservesHeldBlockAndReplaysBlockedHistoryAcrossRelease`
  bestand unter 5.8.3 nur 1 von 3 isolierten Läufen. Bei Frameversatz 22
  landet die Wiederherstellung genau auf dem verfolgten Frame, an dem der Block
  schon gelöst ist; bei Versatz 24 einen Frame davor, und der Test besteht.
  Testaufbau zeitabhängig; kein 5.8.2-Vergleich mehr möglich. [Bericht](gasp-engine-5-8-3.md)

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
