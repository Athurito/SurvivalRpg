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
| Aktive Aufgabe, Status, zuständiger Chat | `GASP-11` Gemeinsames Bewegungsprofil, **In Arbeit** (Claude, Worktree `musing-ishizaka-7bac20`). Zuletzt gemergt: `GASP-10` Animationsset und Tempo-Parität nach Sichtprüfung des Nutzers. |
| Branch, Worktree, Basis / PR | `claude/gasp-11-movement-profile`, Basis `master` `628b1400`; noch kein PR. Zuletzt `claude/gasp-10-animation-set` / [#235](https://github.com/Athurito/SurvivalRpg/pull/235), gemergt als `daaab0e2`. |
| Offene Abnahmepunkte | `GASP-11`: Umsetzung, Prüfung und Sichtprüfung stehen aus; die Entscheidungen sind getroffen (Sprung-Z 450, sofortiges Drehen am Boden). Jeder Checkout muss vor dem nächsten Build seine 5.8.2-Overrides gemäß [README](../Build/Patches/NetworkPrediction/README.md#engine-updates) entfernen und neu stagen. |
| Akzeptierter Runtime-Stand | `master` `628b1400`; enthält alle GASP-Schritte bis GASP-10 (#235, Status #236), die GASP-11-Definition (#237), GASP-ENG-01 (#221), GASP-STAB-03 (#223, nur Testcode) und den [Bewegungsplan](gasp-movement-plan.md) (#229, nur Doku). Der CMC-Pawn nutzt Rotationsmodi (freie Bewegung und Kampfhaltung) und spielt das Relaxed-Set mit passenden Tempi (Laufen 375, Sprint 585, Ducken 200 cm/s); `Lvl_RpgGaspCMC` steht in der GASP-Präsentation. Die Produktion nutzt weiter UEFN; der Manny-Pilot liegt isoliert unter `/Game/SurvivalRpg/Characters/GASP/Pilot/`. |
| Dateibesitz / Sperren | `GASP-11` ab Start: `RpgCharacterMovementComponent.*`, das neue `RpgMovementProfile.*` und sein Asset, die Sprinttempo-Quelle in `RpgAbilityTask_Sprint.*`, die Bewegungswerte in `BP_RpgGasp_CMC`, die CMC-Bewegungs- und Integrationstests und nur bei endlichen Drehraten die Trajektorien-Einstellungen in `ABP_RpgGasp_CMC`; `GASP-12` und `GASP-13` warten auf den Merge. Die geteilten Relaxed- und Dense-Datenbanken nutzen CMC und Mover gemeinsam; CMC-spezifische Änderungen brauchen eigene Kopien. Das Lyra-`SK_Mannequin` trägt seit `GASP-08` die Blend-Profile `FastFeet+Root_Weight` und `FastHead_Weight`, die der Manny-AnimBP braucht; nicht entfernen. Eine separate Claude-Sitzung untersucht den zeitabhängigen Remote-Melee-Test aus den offenen Befunden. |
| Editor / PIE / Builds | Installiert ist UE 5.8.3 (CL 58210709), `master` pinnt 5.8.3. Branches ohne GASP-ENG-01 bauen erst nach Übernahme von `master`. PIE-Netzwerktests nur in einem Prozess gleichzeitig laufen lassen. Vor eigener Editor-/MCP-Nutzung andere Checkouts und Sitzungen prüfen. |
| Plugin-Overrides | Git-ignoriert und checkout-lokal. In jedem Worktree vor Build oder Branchwechsel gemäß [README](../Build/Patches/NetworkPrediction/README.md) prüfen; Engine-Wechsel nach dem Abschnitt „Engine updates“. |
| Lokale Belege | `Saved/...` existiert nur im erzeugenden Checkout; versioniert sind Zusammenfassungen in Berichten und Manifesten unter `docs/assets`. GASP-08-Rohdaten liegen in `Saved/Gasp08` und `Saved/Profiling/CSV`, GASP-09-Rohdaten in `Saved/Gasp09`, GASP-10-Rohdaten in `Saved/Gasp10`, beide mit `Saved/ToolRuns` des Worktrees `musing-ishizaka-7bac20`. |

## Nächster Handgriff

`GASP-11` Phase A gemäß [Bericht](gasp-movement-profile.md): Anfahren, Bremsen,
Sprint-Ende, Drehen und Sprung unter 5.8.3 als Ausgangswert messen, danach
Schema und Overrides umsetzen. Der vollständige
Manny-Bake `GASP-14` backt die Relaxed-Datenbanken.

## Offene Befunde

- `GASP-08` (gemergt, gilt für `GASP-14`/`GASP-15`): Der Bake zielt auf `SKM_Manny_Simple`
  ohne Korrekturknochen. Das gebackene Becken sitzt konstant 3,09 cm weiter vorn
  als beim Runtime-Follower. Im Fußkontakt liegt der p95-Wert in 9 von 144 Clips
  bis zu 4,2 cm/s über dem Follower, in 4 Clips darunter. Pilot-Normalisierung
  und Datenbankauswahl weichen von der Produktion ab; Multiplayer und Gameplay
  auf einem Manny-Mesh sind ungeprüft. [Bericht](gasp-manny-skeleton-pilot.md#limits)

- `GASP-09` (gemergt): Das Aussehen des Aim Offsets und Angriffe mit Kameradrehung hat der Nutzer nicht gezielt beurteilt. Ausrüstungswechsel sind nicht vorhergesagt; seitliches oder Rückwärts-Sprinten beim Ziehen kann eine Korrektur auslösen. Ein Hurdle-Lauf stürzte einmal in der Engine ab (`FSceneViewport::Destroy` auf dem Render-Thread), der Wiederholungslauf bestand. [Bericht](gasp-rotation-modes.md#limits)
- `GASP-10` (gemergt): Die Sichtprüfung des Nutzers war pauschal („sieht gut aus“). Die Leans bleiben Neutral, bis `GASP-13` den Relaxed-Lean-Layer des Movers übernimmt. Diagonalen nutzen die LL-/RL-Clips mit Orientation Warping. In freier Bewegung bleibt die Richtung vorwärts, richtungsabhängige Pivots gibt es nur in der Kampfhaltung. Das Relaxed-Set kostet etwa 0,23 ms mehr Animations-Worker-Zeit pro Figur und hat keine Datenbank-LOD. [Bericht](gasp-animation-set.md#limits)
- CMC-Review 11.10.2026 (UE 5.8.2; Pakete `GASP-09` bis `GASP-13` im [Bewegungsplan](gasp-movement-plan.md)): Der
  Rotationskonflikt ist mit `GASP-09` behoben, die Tempo-Parität mit `GASP-10`. Offen bis `GASP-11`: Anfahren und
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
