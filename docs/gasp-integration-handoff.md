# GASP-Integration: Übergabe

Aktueller Arbeitsstand für den nächsten Chat. Status, Reihenfolge und
Arbeitsregeln stehen in der [Roadmap](gasp-integration-roadmap.md), Belege und
Verlauf jeder Aufgabe in ihrem Bericht und PR. Beim Abschluss eines Schritts
diese Datei **ersetzen, nicht ergänzen**. Das frühere fortlaufende Protokoll bis
28.09.2026 liegt in der [Git-Historie](https://github.com/Athurito/SurvivalRpg/blob/a2ddbc833212c053df51fa7d3d3e2ec334aabd30/docs/gasp-integration-handoff.md).

## Aktueller Stand

| Feld | Wert |
| --- | --- |
| Stand | 19.10.2026 |
| Aktive Aufgabe, Status, zuständiger Chat | Keine. Kein Schritt ist **Bereit**; der Nutzer markiert das nächste Paket. `GASP-VAL-02` ist am 19.10.2026 **definiert** ([Definition](gasp-packaged-network-validation.md)) und wartet auf **Bereit**. Nutzerentscheidungen vom 19.10.2026: kein echtes WAN jetzt (Phase E zurückgestellt), der Nutzer spielt die Routen im gepackten Build selbst, die Emulationsprofile bleiben wie vorgeschlagen. Zuletzt gemergt: `GASP-17` (Paladin in den produktiven CMC-Experiences, [Bericht](gasp-paladin-production.md)) auf Wunsch des Nutzers; ein Ergebnis der Nutzerprüfung wurde nicht gemeldet. Nutzerentscheidungen: Paladin bleibt, entfernbar über die Experience; Charakter-Editor, männliche MetaHuman-Spieler-Varianten (eigenes Paket) und Sound später. |
| Branch, Worktree, Basis / PR | Definition auf `claude/gasp-val-02-definition` im Worktree `musing-ishizaka-7bac20`, Basis `master` `8e497136`, PR [#270](https://github.com/Athurito/SurvivalRpg/pull/270) (nicht gemergt). Zuletzt gemergt am 19.10.2026: [#269](https://github.com/Athurito/SurvivalRpg/pull/269) (Status `GASP-17`) als `8e497136`; davor [#268](https://github.com/Athurito/SurvivalRpg/pull/268) (`GASP-17`) als `7a559d37`; davor [#267](https://github.com/Athurito/SurvivalRpg/pull/267) (Definition `GASP-17`) als `323015a9` und [#266](https://github.com/Athurito/SurvivalRpg/pull/266) (`GASP-16`) als `9c181c48`. Geparkt: `claude/gasp-stab-05-block-release` / [#249](https://github.com/Athurito/SurvivalRpg/pull/249) (Entwurf, nicht gemergt). |
| Offene Abnahmepunkte | Keine offene Aufgabe. Aus `GASP-17`: ein seltener Timing-Fall in `CancelledHurdleReleasesMovementCollisionAndRotation` (7 von 8 Läufen mit Paladin grün) und Engine-Abstürze beim PIE-Abbau; Testläufe brauchen `bAutoStartServer=False` für den MCP-Server, wenn ein externer MCP-Client Port 8000 abfragt; `Lvl_HarvestPickaxe`, `Lvl_ResourceParity` und `Lvl_VfxGallery` zeigen den Paladin. Aus `GASP-16`: der Körper hängt dem Ragdoll um einen Physik-Frame nach; etwa 0,17 ms Animation und 41 Draw Calls pro Paladin. Jede Gameplay- und Bewegungsänderung auch im Mehrspieler prüfen (Nutzerwunsch 13.10.2026): PIE-Netzwerktests, 2- bis 3-Spieler-Messung der entfernten Figur und ein Mehrspieler-Schritt im Playtest. Jeder Checkout muss vor dem nächsten Build seine 5.8.2-Overrides gemäß [README](../Build/Patches/NetworkPrediction/README.md#engine-updates) entfernen und neu stagen. |
| Akzeptierter Runtime-Stand | `master` `7a559d37`; enthält `GASP-17` (#268, Definition #267), `GASP-16` (#266, Definition #262, Inventar-Fix #264), GASP-STAB-06 (#257, Definition #255, Status #259), alle GASP-Schritte bis GASP-15 (#254, Definition #253; GASP-14 #251, Definition #250, Status #252), Status #256, GASP-STAB-04 (#244, Definition #243, Status #245), GASP-ENG-01 (#221), GASP-STAB-03 (#223, nur Testcode) und den [Bewegungsplan](gasp-movement-plan.md) (#229, nur Doku). Die CMC-Pawns (`BP_RpgGasp_CMC`, `BP_RpgGasp_MantleCharacter`) spielen das Lyra-Manny-Skelett nativ: `SKM_Manny_Simple_GaspLook` mit `ABP_RpgGasp_CMC_Manny`, `PA_Mannequin`, Manny-Traversal und ohne Runtime-Follower; Gameplay-Montagen laufen ohne Umrechnung. Der Block wählt seine Darstellung je Skelett (`FRpgWeaponBlockDefinition::SkeletonPresentations`): Manny-Layer und -Montagen auf dem CMC, UEFN auf dem Mover. Der Mover bleibt auf UEFN mit optionalem Follower-Profil. Der `GASP-08`-Pilot und die `GASP-14`-Entwicklungskette sind entfernt. Der CMC-Pawn nutzt Rotationsmodi (freie Bewegung und Kampfhaltung) und spielt das Relaxed-Set mit passenden Tempi (Laufen 375, Sprint 585, Ducken 200 cm/s) und bewegt sich nach `DA_RpgGasp_MovementProfile` (Sprung-Z 450, etwa 0,48 s bis Lauftempo); Gehen mit 165 cm/s über Feststelltaste, rechte Schultertaste oder einen Stick unter 0,7; `Lvl_RpgGaspCMC` steht in der GASP-Präsentation. Eine stehende Kampfhaltung behält ihre Blickrichtung, bis sie läuft, blockt oder angreift (Angriffe drehen mit 1800 °/s ein); dann drehen die Füße per Turn-in-Place mit, und die CMC-Kamera läuft nach (`CM_RpgGasp_Traversal`). Ein Listen-Server-Host sieht Client-Figuren beim Drehen so ruhig wie die anderen Clients. Ein Blockdruck nach einer Kameradrehung richtet Körper und Schild auf allen Rollen in höchstens 0,16 s aus, auch auf dem Mover ohne Turn-Clip; die Rechts-Turns unter Block holen nicht mehr gegen die Drehrichtung aus. |
| Dateibesitz / Sperren | Keine aktive Sperre. Der Paladin der CMC-Experiences hängt allein an `System/Experiences/ActionSets/LAS_RpgCharacter_Paladin`; Tests prüfen das Gameplay-Skelett, nicht das Mesh-Asset, weil der Körper das Gameplay-Mesh unsichtbar macht. Aus `GASP-16`: `GASP-16` nutzte den Editor des Asset-Projekts `Assets_3DAssets` (MCP-Port 8001); dort hat Phase D2 das Plugin `MetaHumanCharacter` aktiviert und `/Game/MetaHumans_G16` (zusammengebauter `MHC_paladin`, Gesicht ohne Post-Process und DNA) angelegt, sonst ändert das Paket dort nichts. `GASP-16` besitzt `Content/SurvivalRpg/Characters/Polyphoria`, `Content/SurvivalRpg/Characters/MetaHumans` (`Common` ist für spätere Varianten gedacht), `Characters/Parts/ABP_RpgPart_CopyPose_MetaHuman`, `GF_Dev_Sandbox/Content/Characters/Knight_01`, `Source/SurvivalRpg/Cosmetics` und die Character-Parts-Anbindung in `RpgEquipmentDefinition.*`, `RpgEquipmentInstance.*` und `RpgEquipmentManagerComponent.cpp`; Phase D legt Teile-Blueprints, Rüstungs-Items (`GF_Combat_Core`) und eine Pilot-Experience mit Karte (`GF_Dev_Sandbox`) an. Körper, Hände und Knight_01 hängen am GASP-UE5-Skelett (`Characters/GASP/Shared/Characters/UE5_Mannequins/Meshes/SK_Mannequin`); dieses Skelett und sein `PA_Mannequin` nicht verschieben oder ändern. Ordner mit Redirectors nie per `rename_directory` verschieben (Bericht, Phase B). Die Block-Layer-Basis existiert zweimal (`Shared/RPG/Block/ABP_RpgBlockLocomotion` für den Mover, `Manny/Block/ABP_RpgBlockLocomotion_Manny` für den CMC); Änderungen an der Block-Logik gehören in beide, und nur eine der fünf Kopien der Update-Logik im EventGraph ist erreichbar. Die Rechts-Turn-Clips beider Block-Sets tragen eine Beckenkorrektur (`GASP-STAB-06`), die ein Neubake wiederholen muss. Die geteilten Relaxed- und Dense-Datenbanken nutzt der Mover; die CMC-Pawns nutzen die Manny-Kopien. Das Lyra-`SK_Mannequin` trägt die Blend-Profile `FastFeet+Root_Weight` und `FastHead_Weight` (`GASP-08`) und `FastFeet_InstantRoot` (`GASP-14`), die die Manny-AnimBPs und -Montagen brauchen; nicht entfernen. **Keine Knochen oder Hierarchieänderungen am `SK_Mannequin`** (neue Skelett-GUID, etwa 1.800 Assets würden bei jedem Laden neu angepasst). In den Manny-Sprung- und -Traversal-Clips trägt der Lyra-Knochen `interaction` den Kantenpunkt (in GASP `attach`). **Keine Legacy-Asset-Typen** wie `BlendSpace1D` oder `AimOffsetBlendSpace1D` anlegen (Nutzerwunsch 17.10.2026). Eine separate Claude-Sitzung untersucht den zeitabhängigen Remote-Melee-Test aus den offenen Befunden. |
| Editor / PIE / Builds | Installiert ist UE 5.8.3 (CL 58210709), `master` pinnt 5.8.3. Branches ohne GASP-ENG-01 bauen erst nach Übernahme von `master`. PIE-Netzwerktests nur in einem Prozess gleichzeitig laufen lassen. Vor eigener Editor-/MCP-Nutzung andere Checkouts und Sitzungen prüfen. |
| Plugin-Overrides | Git-ignoriert und checkout-lokal. In jedem Worktree vor Build oder Branchwechsel gemäß [README](../Build/Patches/NetworkPrediction/README.md) prüfen; Engine-Wechsel nach dem Abschnitt „Engine updates“. |
| Lokale Belege | `Saved/...` existiert nur im erzeugenden Checkout; versioniert sind Zusammenfassungen in Berichten und Manifesten unter `docs/assets`. GASP-08-Rohdaten liegen in `Saved/Gasp08` und `Saved/Profiling/CSV`, GASP-09-Rohdaten in `Saved/Gasp09`, GASP-10-Rohdaten in `Saved/Gasp10`, GASP-11-Rohdaten in `Saved/Gasp11`, GASP-12-Rohdaten in `Saved/Gasp12`, GASP-STAB-04-Rohdaten in `Saved/GaspStab04`, GASP-13-Rohdaten in `Saved/Gasp13`, GASP-STAB-05-Rohdaten in `Saved/GaspStab05`, GASP-14-Rohdaten in `Saved/Gasp14`, GASP-15-Rohdaten in `Saved/Gasp15`, GASP-STAB-06-Rohdaten in `Saved/GaspStab06` und `Saved/Gasp15/UserCheck`, GASP-16-Rohdaten in `Saved/Gasp16` (Phasen A, D, D2), alle mit `Saved/ToolRuns` des Worktrees `musing-ishizaka-7bac20`. |

## Nächster Handgriff

Der Nutzer markiert `GASP-VAL-02` als **Bereit** ([Definition](gasp-packaged-network-validation.md)) oder wählt ein anderes Paket.
 Danach beginnt `GASP-VAL-02` mit Phase A (`ue.py package`). Vorgemerkt: männliche MetaHuman-Spieler-Varianten unter der
Paladin-Rüstung als eigenes Paket. Weiter offen
und nicht eingeplant: das Überschießen von Teil-Turns unter Block, das zurückgestellte `GASP-STAB-05`, das Entfernen der ungenutzten
UEFN-CMC-Kette, die Turn-Clip-Wahl auf Proxies, die in `GASP-13` nicht übernommenen Tuning-Werte und der Port der Relaxed-Leans aus
`GASP-10`.

## Offene Befunde

- `GASP-STAB-06` (gemergt, [#257](https://github.com/Athurito/SurvivalRpg/pull/257), vom Nutzer positiv geprüft): Blockdruck nach Kameradrehung richtet
  Körper und Schild auf allen Rollen in 0,07–0,16 s aus (entfernte Kopien vorher etwa 1 s mit Turn-Clip); die Rechts-Turns drehen am
  Anfang nicht mehr falsch herum. Offen: Ist die Schuld kleiner als der Clip-Winkel, schießt der Körper am Turn-Ende über und dreht
  12–24° zurück (Links-Turns; Rechts-Turns bis 12° an den Schultern), unverändert seit `GASP-06`; ein exakter 180°-Flick wird nicht
  auf allen Rollen zur gleichen Seite gezwungen; dreht die Kamera beim Blockdruck schneller als 30 °/s, folgt der Körper bis 0,8 s
  unkompensiert. Beide Block-Basen enthalten vier unerreichbare Kopien der Update-Logik. Der Mover dreht nach dem Loslassen von Block
  in seine alte Blickrichtung zurück (außerhalb des Pakets). [Bericht](gasp-block-turn.md#limits)
- `GASP-15` (gemergt, [#254](https://github.com/Athurito/SurvivalRpg/pull/254)): Entfernte Rollen beenden ein Drehen im Stand
  auf Manny bis 41–44° hinter der Kapsel (UEFN bis 33°), unter der Turn-in-Place-Schwelle von 50°; der Aim Offset deckt es ab,
  aber die Füße können auf anderen Bildschirmen sichtbar nachhängen. Der Host-View-Test prüft deshalb jetzt das dokumentierte
  Kriterium (Root innerhalb 60° der Kapsel) statt der Drehung seit dem Flick (Nutzerentscheidung). Die Block-Layer-Basis musste
  für Manny kopiert werden, weil `bIsTemplate` mit den Projektwerkzeugen nicht setzbar ist; die abstrakten Basen lösen beim
  Speichern ein Engine-Ensure im Thumbnail-Render aus. Die Manny-Block-Clips gingen durch zwei Retargets. Der CMC-Test mit
  Follower in der Traversal und die Missing-Source-Probe sind entfernt. Kosten: im selben Editorlauf Manny 0,82–0,83 ms gegen
  0,94 ms UEFN Animations-Worker, also kein verlässlicher Unterschied (`GASP-14` maß das Gegenteil); nur eine Figur in Editor-PIE.
  Die ungenutzte UEFN-CMC-Kette (`ABP_RpgGasp_CMC`, Chooser, `AC_RpgGasp_TraversalQuery`, `GA_RpgGasp_Mantle`, Ability-Set,
  `DA_RpgGasp_RuntimeRetarget`) bleibt bis zu einem späteren Paket. [Bericht](gasp-manny-gameplay-mesh.md#limits)

- `GASP-08` (gemergt, gilt für `GASP-14`/`GASP-15`): Der Bake zielt auf `SKM_Manny_Simple`
  ohne Korrekturknochen. Das gebackene Becken sitzt konstant 3,09 cm weiter vorn
  als beim Runtime-Follower. Im Fußkontakt liegt der p95-Wert in 9 von 144 Clips
  bis zu 4,2 cm/s über dem Follower, in 4 Clips darunter. Pilot-Normalisierung
  und Datenbankauswahl wichen von der Produktion ab; die Pilot-Assets sind mit `GASP-15` entfernt, Gameplay und
  Mehrspieler auf Manny prüft `GASP-15`. [Bericht](gasp-manny-skeleton-pilot.md#limits)

- `GASP-14` (gemergt, vom Nutzer abgenommen): Der Manny-Pawn kostet im selben Editorlauf etwa
  0,08 ms (10 %) mehr Animations-Worker-Zeit als der UEFN-Pawn (0,92 gegen 0,84 ms); die Ursache
  ist nicht gefunden, und zwischen Editorläufen schwankten die Werte bis 0,17 ms. Gemessen nur in
  Editor-PIE mit einer Figur. Der Lyra-Knochen `interaction` trägt in 59 Manny-Clips den
  Traversal-Kantenpunkt. Nach Änderungen an den Traversal-Daten kann die erste Traversal einmal in
  einen Sprung fallen, solange der Index neu aufgebaut wird (auch bei UEFN). Manny-Assets verweisen
  weiter auf experimentelle UEFN-Datenbanken und -Clips. In der Regression scheiterten drei
  Mover-Fälle, die keine Manny-Assets laden. Davon ist
  `GaspVariantComparisonPIE.RagdollVariantRunningVaultPreservesMoverTraversal` neu zeitabhängig:
  2 von 6 Läufen auf dem Branch, vorher 0 von 9; der beobachtende Proxy verpasst die kurze Fallphase
  nach dem Vault. [Bericht](gasp-manny-full-bake.md#limits)
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
- `GASP-13` (gemergt, vom Nutzer abgenommen): Eine stehende Figur in der Kampfhaltung dreht per Turn-in-Place mit
  der Kamera mit, auf Besitzer, Host, Proxy und Late Joiner. Ursache war das GASP-Gate (Drehen
  im Stand nur beim Zielen) und die fehlende Kostenpräferenz der Relaxed-Turn-Datenbank, nicht
  `GASP-10`. Nach der ersten Sichtprüfung (zu ruckartig) läuft ein begonnener Schritt weiter,
  bis die Root auf ihrer Seite bis auf 10° heran ist. Nach der zweiten (16.10.2026, „fühlt sich
  weird an“) wählte der Nutzer den Souls-/Valheim-Stil: Eine stehende Kampfhaltung behält ihre
  Blickrichtung, bis sie läuft, blockt oder angreift; ein Angriff dreht zuerst mit 1800 °/s zur
  Kamera (`URpgCharacterMovementComponent`, Option `bCombatStanceFacesCameraWhileStanding` für
  das alte Verhalten). Das ändert die `GASP-09`-Drehregel. Nach der dritten Sichtprüfung
  („Blocken triggert doppelt“) startet unter dem Block-Layer und unter `DefaultSlot`-Montagen kein
  Basis-Turn-in-Place mehr; die eingefrorene Basis-Pose machte beim Loslassen einen zweiten
  Schritt. Das danach gemeldete Problem beim Loslassen von Block ist `GASP-STAB-05` (zurückgestellt). Andere Clients sehen weiter keinen Aim Offset entfernter Spieler. Offen: Proxies wählen beim Drehen fast jeden Frame einen anderen Turn-Clip, weil
  die Engine-Trajektorie die Blickrichtung aus der im Netzwerktakt springenden Actor-Rotation
  vorhersagt (Host-Ansicht unauffällig). Steering 0,5, die Offset-Root-Bone-Halbwertszeit und
  weniger Spins wurden gemessen und nicht übernommen. Die Relaxed-Leans aus `GASP-10` fehlen
  in der Definition. [Bericht](gasp-presentation-polish.md#limits)
- `GASP-STAB-05` (zurückgestellt am 17.10.2026, Entwurf [#249](https://github.com/Athurito/SurvivalRpg/pull/249), nicht gemergt):
  Beim Loslassen im Stand spielt die End-Montage des Schilds den ganzen Körper und endet in der
  breiten Kampfstellung des Pakets; danach rutschen die Füße etwa 40 cm (pro Fuß 58 bis 72 cm in
  Bodennähe) in die GASP-Idle-Stellung, auf allen Rollen. Der Entwurf ersetzt das durch zwei
  Schritte aus `Idle_Combat_to_Idle_Seq` (10 bis 22 cm), machte aber das Drücken kurz nach dem
  Loslassen langsamer; deshalb zurückgestellt. Zusätzlich gemessen: Auf dem Mover stehen manche
  Block-Starts etwa 20 cm zu hoch (auch auf `master`), nicht analysiert.
  [Bericht auf dem Branch](https://github.com/Athurito/SurvivalRpg/blob/claude/gasp-stab-05-block-release/docs/gasp-block-release.md)
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
