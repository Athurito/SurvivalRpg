# GASP-Integration: Übergabe

Aktueller Arbeitsstand für den nächsten Chat. Status, Reihenfolge und
Arbeitsregeln stehen in der [Roadmap](gasp-integration-roadmap.md), Belege und
Verlauf jeder Aufgabe in ihrem Bericht und PR. Beim Abschluss eines Schritts
diese Datei **ersetzen, nicht ergänzen**. Das frühere fortlaufende Protokoll bis
28.09.2026 liegt in der [Git-Historie](https://github.com/Athurito/SurvivalRpg/blob/a2ddbc833212c053df51fa7d3d3e2ec334aabd30/docs/gasp-integration-handoff.md).

## Aktueller Stand

| Feld | Wert |
| --- | --- |
| Stand | 16.10.2026 |
| Aktive Aufgabe, Status, zuständiger Chat | `GASP-13` Darstellung und Drehen im Stand, **PR offen** (Claude, Worktree `musing-ishizaka-7bac20`); nach zwei Sichtprüfungen (15. und 16.10.2026) behält eine stehende Kampfhaltung ihre Blickrichtung (Wahl des Nutzers), die dritte Sichtprüfung steht aus. Zuletzt gemergt: `GASP-STAB-04` Zittern von Client-Figuren in der Host-Ansicht nach Sichtprüfung des Nutzers. |
| Branch, Worktree, Basis / PR | `claude/gasp-13-presentation-polish`, Basis `master` `6df33197` / [#247](https://github.com/Athurito/SurvivalRpg/pull/247). Definition [#246](https://github.com/Athurito/SurvivalRpg/pull/246). Zuletzt `claude/gasp-stab-04-host-view-jitter` / [#244](https://github.com/Athurito/SurvivalRpg/pull/244), gemergt als `681eeafa` (Definition [#243](https://github.com/Athurito/SurvivalRpg/pull/243)). Davor `claude/gasp-12-walk-gait` / [#241](https://github.com/Athurito/SurvivalRpg/pull/241), gemergt als `1c6a076c`. |
| Offene Abnahmepunkte | `GASP-13`: dritte Sichtprüfung des Nutzers als Host und als Client ([Playtest](%s#repro-after-implementation)), dabei Entscheidung über die nicht übernommenen Tuning-Werte, die Proxy-Clipwahl und die Relaxed-Leans aus `GASP-10` ([Grenzen](gasp-presentation-polish.md#limits)). Jede Gameplay- und Bewegungsänderung auch im Mehrspieler prüfen (Nutzerwunsch 13.10.2026): PIE-Netzwerktests, 2- bis 3-Spieler-Messung der entfernten Figur und ein Mehrspieler-Schritt im Playtest. Jeder Checkout muss vor dem nächsten Build seine 5.8.2-Overrides gemäß [README](../Build/Patches/NetworkPrediction/README.md#engine-updates) entfernen und neu stagen. |
| Akzeptierter Runtime-Stand | `master` `6df33197`; enthält alle GASP-Schritte bis GASP-12 (#241, Status #242), GASP-STAB-04 (#244, Definition #243, Status #245), die GASP-13-Definition (#246), GASP-ENG-01 (#221), GASP-STAB-03 (#223, nur Testcode) und den [Bewegungsplan](gasp-movement-plan.md) (#229, nur Doku). Der CMC-Pawn nutzt Rotationsmodi (freie Bewegung und Kampfhaltung) und spielt das Relaxed-Set mit passenden Tempi (Laufen 375, Sprint 585, Ducken 200 cm/s) und bewegt sich nach `DA_RpgGasp_MovementProfile` (Sprung-Z 450, etwa 0,48 s bis Lauftempo); Gehen mit 165 cm/s über Feststelltaste, rechte Schultertaste oder einen Stick unter 0,7; `Lvl_RpgGaspCMC` steht in der GASP-Präsentation. Ein Listen-Server-Host sieht Client-Figuren beim Drehen so ruhig wie die anderen Clients. Die Produktion nutzt weiter UEFN; der Manny-Pilot liegt isoliert unter `/Game/SurvivalRpg/Characters/GASP/Pilot/`. |
| Dateibesitz / Sperren | `GASP-13` (PR offen): `ABP_RpgGasp_CMC`, die Chooser unter `CMC/RPG/Choosers` und nötige CMC-eigene Kopien unter `CMC/RPG`, das Kamerafeld von `DA_PawnData_GaspCMC` und `DA_PawnData_GaspMantle` mit einem neuen CMC-Kameramodus, `RpgAnimInstance.*` nur für einen fehlenden Snapshot-Eingang, ein neuer CMC-Turn-in-Place-PIE-Test und `RpgGaspCMCHostViewTests.cpp`; seit der zweiten Sichtprüfung auch die Drehregel der Kampfhaltung in `URpgCharacterMovementComponent`, der Facing-Commitment-Getter in `URpgAbilitySystemComponent` mit ihren nativen Tests sowie die Abbruchprüfungen der Mantle- und Vault-PIE-Tests. Nicht anfassen: geteilte Datenbanken, Mover-Assets, `CM_ThirdPerson`, `CM_RpgGasp_Traversal`, Bewegungswerte. Die geteilten Relaxed- und Dense-Datenbanken nutzen CMC und Mover gemeinsam; CMC-spezifische Änderungen brauchen eigene Kopien. Das Lyra-`SK_Mannequin` trägt seit `GASP-08` die Blend-Profile `FastFeet+Root_Weight` und `FastHead_Weight`, die der Manny-AnimBP braucht; nicht entfernen. Eine separate Claude-Sitzung untersucht den zeitabhängigen Remote-Melee-Test aus den offenen Befunden. |
| Editor / PIE / Builds | Installiert ist UE 5.8.3 (CL 58210709), `master` pinnt 5.8.3. Branches ohne GASP-ENG-01 bauen erst nach Übernahme von `master`. PIE-Netzwerktests nur in einem Prozess gleichzeitig laufen lassen. Vor eigener Editor-/MCP-Nutzung andere Checkouts und Sitzungen prüfen. |
| Plugin-Overrides | Git-ignoriert und checkout-lokal. In jedem Worktree vor Build oder Branchwechsel gemäß [README](../Build/Patches/NetworkPrediction/README.md) prüfen; Engine-Wechsel nach dem Abschnitt „Engine updates“. |
| Lokale Belege | `Saved/...` existiert nur im erzeugenden Checkout; versioniert sind Zusammenfassungen in Berichten und Manifesten unter `docs/assets`. GASP-08-Rohdaten liegen in `Saved/Gasp08` und `Saved/Profiling/CSV`, GASP-09-Rohdaten in `Saved/Gasp09`, GASP-10-Rohdaten in `Saved/Gasp10`, GASP-11-Rohdaten in `Saved/Gasp11`, GASP-12-Rohdaten in `Saved/Gasp12`, GASP-STAB-04-Rohdaten in `Saved/GaspStab04`, GASP-13-Rohdaten in `Saved/Gasp13`, alle mit `Saved/ToolRuns` des Worktrees `musing-ishizaka-7bac20`. |

## Nächster Handgriff

Dritte Sichtprüfung des Nutzers für `GASP-13` gemäß [Playtest](gasp-presentation-polish.md#repro-after-implementation),
als Host und als Client: Im Stand dreht nur die Kamera, Blocken, Angreifen und Losgehen drehen
die Figur zur Kamera. Dabei entscheiden, ob die nicht übernommenen Tuning-Werte
so bleiben, ob die Clipwahl auf Proxies ein eigenes Paket braucht und wohin die
Relaxed-Leans aus `GASP-10` gehen. Dann mergen. Jede Prüfung schließt
Mehrspieler ein. Der vollständige Manny-Bake `GASP-14` backt die
Relaxed-Datenbanken.

## Offene Befunde

- `GASP-08` (gemergt, gilt für `GASP-14`/`GASP-15`): Der Bake zielt auf `SKM_Manny_Simple`
  ohne Korrekturknochen. Das gebackene Becken sitzt konstant 3,09 cm weiter vorn
  als beim Runtime-Follower. Im Fußkontakt liegt der p95-Wert in 9 von 144 Clips
  bis zu 4,2 cm/s über dem Follower, in 4 Clips darunter. Pilot-Normalisierung
  und Datenbankauswahl weichen von der Produktion ab; Multiplayer und Gameplay
  auf einem Manny-Mesh sind ungeprüft. [Bericht](gasp-manny-skeleton-pilot.md#limits)

- `GASP-09` (gemergt): Das Aussehen des Aim Offsets und Angriffe mit Kameradrehung hat der Nutzer nicht gezielt beurteilt. Ausrüstungswechsel sind nicht vorhergesagt; seitliches oder Rückwärts-Sprinten beim Ziehen kann eine Korrektur auslösen. Ein Hurdle-Lauf stürzte einmal in der Engine ab (`FSceneViewport::Destroy` auf dem Render-Thread), der Wiederholungslauf bestand. [Bericht](gasp-rotation-modes.md#limits)
- `GASP-10` (gemergt): Die Sichtprüfung des Nutzers war pauschal („sieht gut aus“). Die Leans bleiben Neutral; der Port des Relaxed-Lean-Layers des Movers fehlt in der `GASP-13`-Definition und wartet auf die Entscheidung des Nutzers. Diagonalen nutzen die LL-/RL-Clips mit Orientation Warping. In freier Bewegung bleibt die Richtung vorwärts, richtungsabhängige Pivots gibt es nur in der Kampfhaltung. Das Relaxed-Set kostet etwa 0,23 ms mehr Animations-Worker-Zeit pro Figur und hat keine Datenbank-LOD. [Bericht](gasp-animation-set.md#limits)
- `GASP-11` (gemergt): Vom Nutzer pauschal abgenommen.
  Die Mehrspieler-Messung lief lokal ohne Latenz (`GASP-VAL-02`). Blocken aus dem Lauf bremst jetzt mit 500 cm/s² (etwa 0,44 s) statt fast
  sofort. Die Last ist nur am Tag-Seam geprüft, nicht mit echten schweren
  Gegenständen in PIE. Die Pose-Search-Trajektorie sagt beim Sprintanlauf mit der
  Laufbeschleunigung und Lauf-Reibung voraus. [Bericht](gasp-movement-profile.md#limits)
- `GASP-12` (gemergt): Vom Nutzer pauschal abgenommen. Stick-Eingaben nur simuliert über das echte Mapping, nicht
  mit einem Gamepad; Rauschen um 0,7 deckt nur das 0,05-Band ab. Abweichend von der
  Definition sendet der Besitzer seine Stick-Wahl als Flag. Geduckt meldet der Pawn
  Run an den AnimBP. In der gemeinsamen Regression stürzte Mantle einmal in der Engine
  ab (`FSceneViewport::Destroy`), und zwei Mover-Fälle (Ragdoll-Getup, Vault-Late-Join)
  scheiterten einmal; alle bestanden einzeln. [Bericht](gasp-walk-gait.md#limits)
- `GASP-STAB-04` (gemergt, vom Nutzer abgenommen): Das Zittern in der Host-Ansicht ist behoben (0 Richtungswechsel der
  Root bei 30, 60 und 120 FPS wie bei Besitzer und Proxy). Ursache: Der Server tickt die Pose
  einer Client-Bewegung in einem aufgeschobenen Kapsel-Update, die Animation merkte sich das
  Mesh von vor der Bewegung; dazu verschob die Listen-Server-Rotationsglättung das Mesh nach
  dieser Erfassung. Offen bleiben: Ein Host über 60 FPS zeigt Posen entfernter Figuren in
  60-Hz-Stufen (Senderate der Clients), am stärksten beim Drehen mit Block (Rauigkeit 1,1 bis
  4,5 °/Frame bei 120 FPS, keine Richtungswechsel). Dedizierte Server behalten den um eine
  Bewegung alten Transform (serverseitige Bone-/Socket-Abfragen). [Bericht](gasp-host-view-turn-jitter.md#limits)
- `GASP-13` (PR offen): Eine stehende Figur in der Kampfhaltung dreht per Turn-in-Place mit
  der Kamera mit, auf Besitzer, Host, Proxy und Late Joiner. Ursache war das GASP-Gate (Drehen
  im Stand nur beim Zielen) und die fehlende Kostenpräferenz der Relaxed-Turn-Datenbank, nicht
  `GASP-10`. Nach der ersten Sichtprüfung (zu ruckartig) läuft ein begonnener Schritt weiter,
  bis die Root auf ihrer Seite bis auf 10° heran ist. Nach der zweiten (16.10.2026, „fühlt sich
  weird an“) wählte der Nutzer den Souls-/Valheim-Stil: Eine stehende Kampfhaltung behält ihre
  Blickrichtung, bis sie läuft, blockt oder angreift; ein Angriff dreht zuerst mit 1800 °/s zur
  Kamera (`URpgCharacterMovementComponent`, Option `bCombatStanceFacesCameraWhileStanding` für
  das alte Verhalten). Das ändert die `GASP-09`-Drehregel. Andere Clients sehen weiter keinen
  Aim Offset entfernter Spieler. Offen: Proxies wählen beim Drehen fast jeden Frame einen anderen Turn-Clip, weil
  die Engine-Trajektorie die Blickrichtung aus der im Netzwerktakt springenden Actor-Rotation
  vorhersagt (Host-Ansicht unauffällig). Steering 0,5, die Offset-Root-Bone-Halbwertszeit und
  weniger Spins wurden gemessen und nicht übernommen. Die Relaxed-Leans aus `GASP-10` fehlen
  in der Definition. [Bericht](gasp-presentation-polish.md#limits)
- CMC-Review 11.10.2026 (UE 5.8.2; Pakete `GASP-09` bis `GASP-13` im [Bewegungsplan](gasp-movement-plan.md)): Der
  Rotationskonflikt ist mit `GASP-09` behoben, die Tempo-Parität mit `GASP-10`. Anfahren und
  Bremsen mit `GASP-11`: mit Profil 0,48 s bis Lauftempo und 0,29 s/51 cm
  Stopp bei 60 FPS, vorher 0,25 s und 0,12 s/16,5 cm. Bewegung bleibt
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
- `GaspMoverHurdlePIE.MaximumDepthHurdleLandsBeyondRearEdge` lief in der
  gemeinsamen `GASP-11`-Regression einmal in das 60-s-Zeitlimit und bestand
  einzeln; der Mover nutzt die geänderte CMC-Komponente nicht. Zeitabhängigkeit
  offen.
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
