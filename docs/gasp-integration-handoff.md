# GASP-Integration: Übergabe

Aktueller Arbeitsstand für den nächsten Chat. Status, Reihenfolge und
Arbeitsregeln stehen in der [Roadmap](gasp-integration-roadmap.md), Belege und
Verlauf jeder Aufgabe in ihrem Bericht und PR. Beim Abschluss eines Schritts
diese Datei **ersetzen, nicht ergänzen**. Das frühere fortlaufende Protokoll bis
28.09.2026 liegt in der [Git-Historie](https://github.com/Athurito/SurvivalRpg/blob/a2ddbc833212c053df51fa7d3d3e2ec334aabd30/docs/gasp-integration-handoff.md).

## Aktueller Stand

| Feld | Wert |
| --- | --- |
| Stand | 12.10.2026 |
| Aktive Aufgabe, Status, zuständiger Chat | `GASP-10` Animationsset und Tempo-Parität, **PR offen** (Draft, Claude, Worktree `musing-ishizaka-7bac20`); umgesetzt und automatisch geprüft, Sichtprüfung des Nutzers ausstehend. Zuletzt gemergt: `GASP-09` Rotationsmodi und Kampfhaltung nach Sichtprüfung des Nutzers. |
| Branch, Worktree, Basis / PR | `claude/gasp-10-animation-set` im Worktree `.claude/worktrees/musing-ishizaka-7bac20`, Basis `master` `3320408c`; [#235](https://github.com/Athurito/SurvivalRpg/pull/235). Zuletzt `claude/gasp-09-rotation-modes` / [#232](https://github.com/Athurito/SurvivalRpg/pull/232), gemergt als `e5e84cd4`. |
| Offene Abnahmepunkte | `GASP-10`: Der Nutzer hat am 12.10.2026 Relaxed gewählt und die Traversal-Montagen belassen ([Bericht](gasp-animation-set.md#decisions)); Der Nutzer hat die Sichtprüfung am 12.10.2026 abgenommen („sieht gut aus“) und den Merge angefordert. Jeder Checkout muss vor dem nächsten Build seine 5.8.2-Overrides gemäß [README](../Build/Patches/NetworkPrediction/README.md#engine-updates) entfernen und neu stagen. |
| Akzeptierter Runtime-Stand | `master` `e5e84cd4`; enthält alle GASP-Schritte bis GASP-09 (#232), GASP-ENG-01 (#221), GASP-STAB-03 (#223, nur Testcode) und den [Bewegungsplan](gasp-movement-plan.md) (#229, nur Doku). Der CMC-Pawn nutzt Rotationsmodi (freie Bewegung und Kampfhaltung); `Lvl_RpgGaspCMC` steht in der GASP-Präsentation. Die Produktion nutzt weiter UEFN; der Manny-Pilot liegt isoliert unter `/Game/SurvivalRpg/Characters/GASP/Pilot/`. |
| Dateibesitz / Sperren | `GASP-10` bis zum Merge: `ABP_RpgGasp_CMC`, die Chooser unter `CMC/RPG/Choosers`, die Tempowerte in `BP_RpgGasp_CMC`, `RpgGaspCMCIntegrationTests.cpp` und `RpgGaspCMCSpeedParityTests.cpp`; `GASP-11` bis `GASP-13` warten auf den Merge. Die geteilten Relaxed- und Dense-Datenbanken bleiben unverändert, weil der Mover sie nutzt. Das Lyra-`SK_Mannequin` trägt seit `GASP-08` die Blend-Profile `FastFeet+Root_Weight` und `FastHead_Weight`, die der Manny-AnimBP braucht; nicht entfernen. Eine separate Claude-Sitzung untersucht den zeitabhängigen Remote-Melee-Test aus den offenen Befunden. |
| Editor / PIE / Builds | Installiert ist UE 5.8.3 (CL 58210709), `master` pinnt 5.8.3. Branches ohne GASP-ENG-01 bauen erst nach Übernahme von `master`. PIE-Netzwerktests nur in einem Prozess gleichzeitig laufen lassen. Vor eigener Editor-/MCP-Nutzung andere Checkouts und Sitzungen prüfen. |
| Plugin-Overrides | Git-ignoriert und checkout-lokal. In jedem Worktree vor Build oder Branchwechsel gemäß [README](../Build/Patches/NetworkPrediction/README.md) prüfen; Engine-Wechsel nach dem Abschnitt „Engine updates“. |
| Lokale Belege | `Saved/...` existiert nur im erzeugenden Checkout; versioniert sind Zusammenfassungen in Berichten und Manifesten unter `docs/assets`. GASP-08-Rohdaten liegen in `Saved/Gasp08` und `Saved/Profiling/CSV`, GASP-09-Rohdaten in `Saved/Gasp09`, GASP-10-Rohdaten in `Saved/Gasp10`, beide mit `Saved/ToolRuns` des Worktrees `musing-ishizaka-7bac20`. |

## Nächster Handgriff

Der Nutzer prüft `GASP-10` gemäß [Repro](gasp-animation-set.md#repro-after-implementation).
Bei Abnahme: [#235](https://github.com/Athurito/SurvivalRpg/pull/235) mergen und als **Gemergt** führen, danach `GASP-11`
definieren. Bei Befunden: im selben PR nachbessern. Der vollständige Manny-Bake
`GASP-14` backt die Relaxed-Datenbanken.

## Offene Befunde

- `GASP-08` (gemergt, gilt für `GASP-14`/`GASP-15`): Der Bake zielt auf `SKM_Manny_Simple`
  ohne Korrekturknochen. Das gebackene Becken sitzt konstant 3,09 cm weiter vorn
  als beim Runtime-Follower. Im Fußkontakt liegt der p95-Wert in 9 von 144 Clips
  bis zu 4,2 cm/s über dem Follower, in 4 Clips darunter. Pilot-Normalisierung
  und Datenbankauswahl weichen von der Produktion ab; Multiplayer und Gameplay
  auf einem Manny-Mesh sind ungeprüft. [Bericht](gasp-manny-skeleton-pilot.md#limits)

- `GASP-09` (gemergt): Das Aussehen des Aim Offsets und Angriffe mit Kameradrehung hat der Nutzer nicht gezielt beurteilt. Ausrüstungswechsel sind nicht vorhergesagt; seitliches oder Rückwärts-Sprinten beim Ziehen kann eine Korrektur auslösen. Ein Hurdle-Lauf stürzte einmal in der Engine ab (`FSceneViewport::Destroy` auf dem Render-Thread), der Wiederholungslauf bestand. [Bericht](gasp-rotation-modes.md#limits)
- `GASP-10` (PR #235, nicht gemergt): Die Leans bleiben Neutral, bis `GASP-13` den Relaxed-Lean-Layer des Movers übernimmt. Diagonalen nutzen die LL-/RL-Clips mit Orientation Warping. In freier Bewegung bleibt die Richtung vorwärts, richtungsabhängige Pivots gibt es nur in der Kampfhaltung. Das Relaxed-Set kostet etwa 0,23 ms mehr Animations-Worker-Zeit pro Figur und hat keine Datenbank-LOD. [Bericht](gasp-animation-set.md#limits)
- CMC-Review 11.10.2026 (UE 5.8.2; Pakete `GASP-09` bis `GASP-13` im [Bewegungsplan](gasp-movement-plan.md)): Der
  Rotationskonflikt ist mit `GASP-09` behoben, die Tempo-Parität mit `GASP-10` ([#235](https://github.com/Athurito/SurvivalRpg/pull/235), nicht gemergt). Offen: Anfahren und
  Bremsen sind deutlich direkter als im Original (gemessen 0,24 s bzw.
  0,125 s/18,5 cm gegenüber ca. 0,6 s bzw. 0,25 s/62 cm). Bewegung bleibt
  lastunabhängig ([Spielvision](game-vision.md)).
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
