# GASP-NET-01 – Rollback während aktivem Mantle

Stand: **26.09.2026, implementiert und validiert; Review/Merge offen**. Branch
`codex/gasp-net-01-active-mantle-rollback`, Basis `b9a65360` (Merge PR #146).
Implementierung: `b6709eb4`; vorausgehende Merge-/Auftragsstatuspflege `91f49fcc`.
Der Auftrag schließt die in der [Roadmap](gasp-integration-roadmap.md)
erfasste Nachweislücke. Runtime, Engine-Overrides, Blueprints, Montagen und
übrige Assets werden dafür nicht geändert.

## Historischer Befund

Der finale Network-Recovery-Lauf vom 18.09.2026 ließ
`SurvivalRpg.GASP.Mover.Mantle.GaspMoverMantlePIE.FixedCorrectionPreservesActiveWarpAndCollider`
nach 60,033 Sekunden am Korrekturbeobachter scheitern. Der Owner erhielt in
Frame 243 bei 4840 ms einmalig 50 cm seitlichen Positionsfehler. Die folgenden
Dispatch-Proben zeigten jeweils identische Vorher-/Nachherpositionen: 50 cm
in Frame 243, 18,39 cm in Frame 244 und 0 cm in Frame 245. Normales Motion
Warping hatte den Fehler zwischen den Proben entfernt. Alle drei Rollen
beendeten den Mantle und landeten; zehn Rollback-Callbacks allein belegten
nicht, dass der betroffene aktive Frame restauriert oder neu simuliert wurde.

Die konkreten Daten stehen in
`Saved/GaspMoverNetFix20260918/handoff-final-tests.log`: Injektion und
Dispatch-Proben ab Zeile 5955, Rollenabschluss ab 5971, fehlender Nachweis und
letzter Callback bei 6005–6006, Timeout bei 6216. Die frühere gezielte
Wiederholung bestand dagegen mit einem gemessenen Dispatch-Sprung von
−23,74 cm (`targeted-tests.log`, ab 4176). Spätere grüne Wiederholungen des
[Präsentations-Fixes](gasp-mover-traversal-presentation.md) lösten die
Beobachtungslücke nicht grundsätzlich. Der
[Network-Recovery-Bericht](gasp-mover-network-recovery.md) hält diese Grenze
ausdrücklich fest.

## Framebezogener Nachweis

Die Änderung gehört ausschließlich zum bestehenden nativen Editor-Testharness:
[Traversal-Fixture](../Source/SurvivalRpgEditor/Private/Network/RpgGaspMoverTraversalTestFixture.cpp)
und optional aktivierter
[Rollback-Observer](../Source/SurvivalRpgEditor/Private/Network/RpgMoverPredictionTestTypes.cpp).
GAS/Projekt-Mover bleiben für Traversal zuständig; NetworkPrediction besitzt
History und Replay. Es entsteht kein weiterer Gameplay- oder Animationspfad.

Die Fixture injiziert weiterhin einmalig denselben 50-cm-Fehler in Pending-Sync
und UpdatedComponent und invalidiert die bisherigen Floor-/Base-Caches.
Der Zeitpunkt richtet sich nun nach dem tatsächlich aktiven `FrontLedge`-Fenster
in der Simulationshistory, mit Abstand zu beiden Fenstergrenzen. Die sichtbare,
gegebenenfalls geglättete Montagezeit wählt den Zeitpunkt nicht mehr aus.

Der zusätzliche Nachweis verbindet innerhalb eines Netzwerk-Dispatchs:

1. **Injizierter Frame K und folgende Prediction:** Die Fixture sichert den
   tatsächlich geschriebenen Zustand K. Der passive Observer zeichnet außerdem
   echte Forward-Ausgaben bis höchstens K+63 auf. Jeder Snapshot gehört zu
   seinem eigenen lokalen Frame F. Vor dem Dispatch hält die Fixture Head H fest.
2. **Echter Restore R:** Der vorhandene Mover-Rollback-Callback muss einen
   lokalen Frame `R < H` restaurieren. Der Observer kopiert den
   bereits angewandten Sync-Zustand. Mover meldet hier den nächsten Serverframe
   `R + Offset + 1`; dieser Wert darf nicht unmittelbar mit K verglichen werden.
3. **Ersatz desselben aufgezeichneten Frames F:** Bei `R >= K` wird ausschließlich
   ein vorhandener Prediction-Snapshot für F=R mit dem restaurierten Zustand R
   verglichen. Für `R < K` ist F=K; sein Ersatz kommt aus der tatsächlichen
   `OnPostMovement`-Ausgabe des Replay-Schritts mit Input K−1. Ohne Original für
   exakt F gibt es keinen Nachweis. Der Server muss nicht jeden gesendeten
   Simulationsframe einzeln als Autoritäts-Snapshot zurückliefern.
4. **Zusammenhängendes Replay:** Auf den Restore müssen innerhalb desselben
   Dispatchs lückenlose Simulationsschritte unter H folgen. Mindestens ein
   solcher Schritt ist erforderlich. Ein neuer Restore beginnt einen neuen
   Nachweis; Zustände unterschiedlicher Restores werden nicht kombiniert.
   Nach Sicherung des Originalkandidaten werden gespeicherte Zustände ab R
   verworfen. Replay wird nicht als neue Forward-Historie aufgezeichnet.
5. **Unveränderter Head und aktive Verträge:** Vor und nach dem Dispatch müssen
   lokaler Head und Fixed-Schritt übereinstimmen. Restore- und Ersatzsnapshot
   sowie der ursprüngliche Prediction-Snapshot müssen dieselbe aktive
   Traversal-Identität, Montage, Collider, FrontLedge-
   Zieltransformation und das ausgewählte aktive Warp-Fenster enthalten. Die
   Positionen von Prediction[F] und Ersatz[F] müssen eine Gegenkorrektur von
   mindestens 1 cm zeigen. Spätere Frames werden nicht mit der Position von K verglichen.

Damit kann ein bereits durch Forward-Warping ausgeglichener aktueller Head den
historischen Korrekturbeleg weder vortäuschen noch verdecken. Der NP-Liaison setzt
`bIsResimulating` hier nicht; der Nachweis verwendet deshalb den realen Restore
und die zusammenhängenden lokalen Replay-Schritte, nicht dieses ungesetzte Flag.
Der Observer verändert weder Sync/Aux noch Bewegungsablauf. Er löst seine
Delegates und gibt kopierte Zustände vor dem PIE-Teardown frei.

Die bisherigen Prüfungen auf aktive Collider-Lease, unveränderte Montageinstanz,
Warp-Fenster und Bone-Cache-Identität, Ownership-Cleanup und Rollenabschluss
bleiben erhalten. Der neue Beleg zeigt, dass der verfälschte Frame einen echten
Restore-/Replay-Pfad durchläuft. Er behauptet **nicht**, dass ausschließlich
diese Positionsinjektion den weltweiten NetworkPrediction-Rollback ausgelöst hat.

## Tatsächlich ausgeführte Prüfungen

Neue lokale, ignorierte Belege liegen unter
`Saved/GaspActiveMantleRollback20260926`; `*-command.json` enthält die jeweiligen
vollständigen Editor-Aufrufe. Sie stehen in einem anderen Checkout nicht
automatisch zur Verfügung.

| Prüfung | Ergebnis / Beleg |
| --- | --- |
| Unveränderte Baseline | **1/1 bestanden, mit Warnungen**: `baseline-01/index.json` und `baseline-01.log`. Dispatch-Korrektur −11,76 cm bei unverändertem Head, Callback 0→1. Das ist der alte Beobachtungsvertrag, noch keine Abnahme des neuen Frame-Nachweises. |
| Erster Editor-Build | Fehlgeschlagen: vollständige Definition von `FMoverSyncState` im Observer-Header fehlte; `build-editor-01.log`. Fehlversuch bleibt erhalten. |
| Editor-Build nach Include-Korrektur | **Succeeded**, UE 5.8.2 Win64 Development Editor, 28,30 Sekunden; `build-editor-02.log`. |
| Finaler Editor-Build mit Folgeframe-Erfassung | **Succeeded**, UE 5.8.2 Win64 Development Editor, 19,29 Sekunden; `build-editor-03.log`. |
| Erster Nachweis, noch auf K begrenzt | **1/1 bestanden**, `witness-01`, 19,05 Sekunden. K=209, R=209, H=211 vor/nach Dispatch, zwei Replay-Schritte; historischer Positionsunterschied −50 cm seitlich, aktive Window 0 und alle Verträge erhalten. 54 Warnungen: sechs Voice-/AdvancedFriendsInterface-Meldungen und 48 absichtlich eingeschaltete NP-Reconcile-Diagnosen. |
| Erste Negativkontrolle | **Erwartet fehlgeschlagen**, `negative-no-reconcile-01`: `np.SkipReconcile 1`, `np.ForceReconcile 0`. Injektion bei K=211; alle drei Rollen beenden/landen normal, aber kein Restore, kein Replay und kein Ersatzsnapshot. Der Test läuft nach 60,002 Sekunden in seinen unveränderten Szenario-Timeout. Sechs Voice-Warnungen; dieser erhaltene Fehler ist die Gegenprobe, kein grüner Regressionstest. |
| Erste Regression | `regression-01`: **9/10 bestanden**; der neue Mantle-Fall blieb rot, obwohl ein späterer betroffener Head korrigiert wurde. Der erste Observer verlangte zu eng den ursprünglichen Frame K. Das führte zur oben beschriebenen Erfassung tatsächlicher Forward-Snapshots und zum Vergleich desselben betroffenen Frames F. 112 Warnungen; Fehlversuch erhalten. |
| Finaler Mantle-Nachweis mit 30-FPS-Limit | **1/1 bestanden**, `witness-30fps-02`, 19,48 Sekunden. K=R=F=206, H=209, drei echte Replay-Schritte; historisches Delta −50 cm, sämtliche Verträge erhalten. Fixed-Schritt weiterhin 20 ms. 48 Warnungen: sechs Voice, 30 absichtlich eingeschaltete NP-Reconcile-Diagnosen und zwölf Console-Find-Performancehinweise. |
| Finale Negativkontrolle | **Erwartet fehlgeschlagen**, `negative-no-reconcile-02`, mit denselben isolierten CVar-Einstellungen und finalem Code. K=213; alle Rollen beenden/landen, Rollbackzähler 0, kein Replay-/Ersatzbeleg, unveränderter Szenario-Timeout. Sechs Voice-Warnungen. |
| Finale gemeinsame Regression | **10/10 bestanden**, `regression-02`, 116,55 Sekunden, acht PIE- und zwei native Fälle. Mantle injiziert bei K=218; tatsächlicher Restore und Witness F=R=219, H=220 vor/nach Dispatch, ein Replay-Schritt und −43,72 cm Gegenkorrektur zwischen den beiden Zuständen von Frame 219. Alle aktiven Verträge erhalten. 110 Warnungen: 48 Voice, 61 NetPackageMap und eine NP-Warnung. Keine Fehler/Ensures/Fatals im Testintervall. |
| Abschließende Datei-/Map-/SaveGame-Erhaltung | Alle zehn Maps und sieben SaveGames gegenüber `preservation-before.json` unverändert. Vier Plugin-Overrides erneut verifiziert; getestete Quellen entsprechen `b6709eb4`. Editor-/Test-/Buildprozesse beendet. |

Die zehn Fälle umfassen die beiden Mantle-, Vault- und Hurdle-Korrekturen,
Mantle-Late-Join, unmittelbaren Montage-Replay nach Korrektur sowie
`FixedPredictionHeadWitness` und `SnapshotSerializationAndLifetime`.
SHA-256 des finalen Reports:
`01028160fc714a0d67cfb8c4b733bfc202df8013efb1eb9d37d1d0bf87070cf0`.
`audit.json` hält Report- und Quellhashes sowie die unveränderten Fehlversuche fest.
Der echte Prozess lud NetworkPrediction aus dem verifizierten Projekt-Override.
Zwei bekannte `Condition failed`-Startmeldungen vor den Tests bleiben ungeklärt.
Ein neuer Game-Build oder Asset-Compile war für diese reinen Editor-Teständerungen
nicht erforderlich und wurde nicht als zusätzliche Prüfung ausgeführt.

Die ausgeführte Negativkontrolle nutzt den vorhandenen Fixed-Reconcile-Schalter der Engine.
`ForceReconcile` muss 0 bleiben, da dessen eigener Zweig den Rollbackframe vor
dem Skip-Gate setzen kann. Ein aussagekräftiges negatives Ergebnis erfordert
tatsächlich erfolgte Injektion und weiterlaufendes normales Warping, aber keinen
historischen Restore-/Replaynachweis. Ein Fehler vor Erreichen des Szenarios
würde diese Kontrolle nicht erfüllen. Normale Focusläufe bei verschiedenen
Render-FPS können zusätzlich Robustheit zeigen.

## Nachstellen und verbleibende Grenzen

Im Development Editor über Session Frontend → Automation oder die Konsole:

```text
Automation RunTests SurvivalRpg.GASP.Mover.Mantle.GaspMoverMantlePIE.FixedCorrectionPreservesActiveWarpAndCollider
```

Der versionierte Test verwendet die vorhandene GASP-Mover-Testmap und echte
Owner-/Authority-/Observer-Rollen in PIE mit isolierter Persistenz. Er startet
die Bewegung selbst und muss bei normalen NP-Einstellungen bestehen. Für die
Negativkontrolle einen getrennten Editorprozess verwenden; dessen vollständiger
Aufruf steht im jeweiligen `*-command.json`. Der fehlende Rollback ist dann
absichtlich ein Testfehler. Die Gegenprobe ist keine erfolgreiche Regression.

NET-01 ist damit im getesteten Fixed-/PIE-Umfang nachgewiesen. Review und Merge
sind noch offen. Eine neue handgespielte Sichtabnahme ist für diesen nativen
Frame-Nachweis nicht erfolgt; der numerische Verlauf stammt aus gerendertem PIE.

Dieser Auftrag behebt nicht den gesonderten Terminal-Grund-Konflikt
`Finished`/`Cancelled` (`GASP-NET-02`) und erweitert weder Packet-Loss-,
Independent-, Packaged- noch WAN-Abnahme. Es wird kein neuer visueller
Animations- oder allgemeiner Netzwerkfehler als behoben behauptet.
