# GASP-STAB-02 – Falling nach Respawn

Status: **Implementiert und validiert, PR-Vorbereitung**, Branch `codex/gasp-02-respawn-falling`, Basis
`4039be2560b1733859005ec052865cff0bb03d3b` (gemergter PR #145).
Implementierungs-Commit: `57fa8de9`.

## Historischer Befund und Nachweislücke

`Saved/GaspMoverVault20260915/editor-lifecycle-diagnostic.log`, ab Zeile 5386:
Am 15.09.2026 um 19:53:38.980 stehen Authority, Owner und Observer nach Respawn
bei `(0,500,120)`. Actor-, Sync- und Spawnposition sind identisch, der Modus ist
`Falling`, Geschwindigkeit null und MoveInput `(1,0,0)`. Owner-Input ist gebunden,
`rawLeftY=1`, `ignoreMove=0`; der neue Pawn ist gesund und vollständig komponiert.
Der Snapshot liegt rund 2,5 Sekunden nach Respawn, der Bewegungstest scheitert
später am 35-Sekunden-Timeout. Ein vorheriger Block-Test ist keine notwendige
Bedingung: `editor-regression-rerun.log` zeigt den Fehler bereits im ersten Test
eines frischen Editors. Diese ignorierten Belege sind lokal vorhanden.

Die damalige Beobachtung enthält keine Folge der ersten Simulationsschritte,
keinen initialen Kollisions-/Penetrationsbefund und keinen Nachweis, ob der
Simulationsframe während des Stillstands weiterläuft. Spätere erfolgreiche
Wiederholungen erklären den Ausfall nicht.

Der bisherige `OccupiedRespawnStartAllowsMovementAndCombat`-Test schloss einen
tatsächlich belegten Spawn nicht aus: Er berechnete nach dem Tod erneut
`GetPlayerCheckpointTransform`, während die Runtime schon beim Tod einen
`PendingRespawnTransform` gespeichert hatte. Ohne gesetzten Checkpoint können
das unterschiedliche zufällig ausgewählte PlayerStarts sein. Ein damaliger
grüner Lauf belegte `(0,0,120)`, respawnte aber bei `(0,500,120)`.

## Untersuchungsgrenze

GameMode/PlayerState besitzen weiterhin Checkpoint, Respawn und ASC-Lifecycle;
Mover/NetworkPrediction besitzen Bewegung und Simulationshistorie. Blueprint
besitzt den konkreten Pawn, seine Kapsel, Bewegungskonfiguration und Darstellung.
Zuerst den vorhandenen Netzwerk-Test über die öffentliche Checkpoint-API
deterministisch machen und die frühesten verfügbaren Respawnzustände erfassen.
Keine Teleports, Mode-Resets oder Input-Nudges als Behelf. Eine Runtimekorrektur
wird erst aus einem konkreten Ursachenbeleg abgeleitet.

Die Engine korrigiert in `UMoverNetworkPredictionLiaisonComponent::BeginPlay`
bereits den initialen Sync-Transform nach einer Spawn-Anpassung. Eine pauschal
veraltete Startposition ist daher keine belegte Ursache. Die bisherige
PlayerStart-Belegungsprüfung schützt die Auswahl, aber nicht einen zwischen
Tod und Respawn nachträglich belegten gespeicherten Punkt.

## Bisherige Untersuchung

Der korrigierte Einzelblocker-Test besteht ohne Runtimeänderung
(`occupied-diagnostic-01`, 22.09.2026, 20:16 UTC). Checkpoint vor Tod,
belegter Punkt vor RPC und gewünschter Respawn sind identisch `(0,0,120)`.
Der neue Authority-Pawn steht beim ersten TickEnd bereits bei
`(0,-60.12,118.82)`, Actor und Sync stimmen überein, Fallgeschwindigkeit
`-39.2 cm/s`. Nach 0,2662 Sekunden wechselt er zu Walking; Owner und Observer
folgen ebenfalls und bestehen Bewegung sowie neuen Waffenangriff.
Die Engine kann diese einzelne Überlappung also selbst auflösen.

Die historische seitliche Verschiebung um rund 60,12 cm passt zu zwei
Kapselradien von 30 cm plus Mover-Penetrationsabstand. Die damaligen Logs
halten allerdings keine Positionen der anderen Spieler fest. Nächste gezielte
Hypothese: Host und späterer Joiner stehen nebeneinander am selben Checkpoint;
der dritte Spawn überlappt den Host und kann nicht in Richtung des Joiners
ausweichen. Der ursprüngliche Fehlerlauf allein beweist diese Anordnung nicht.

## Gezielter roter Nachweis und Korrektur

`CrowdedCheckpointAllowsMovementAndCombat` pinnt vor dem Tod über
`SetPlayerCheckpoint` einen tatsächlich vom Host belegten authored Start.
Der echte Late Joiner erhält diesen als einzigen verbleibenden Start und
weicht selbst seitlich aus. Der Test prüft beide realen Kapseln unmittelbar
vor dem bestehenden Owner-Respawn-RPC. Keine Pawn-Teleports, Moduswechsel,
Geschwindigkeitsänderungen oder künstlichen Bewegungskorrekturen.

`crowded-diagnostic-01` scheitert auf der unveränderten Runtime am
35,012-Sekunden-Bewegungstimeout. Host `(0,500,120)`, Joiner
`(0,439.875,88.15)`, Abstand 60,125 cm. Der dritte Pawn bleibt im Host bei
`(0,500,120)` stehen, auf allen drei Rollen Actor = Sync, `Falling`, Velocity 0,
MoveInput `(1,0,0)`. Die ersten echten `LogMover VeryVerbose`-Bewegungsversuche
und alle weiteren Versuche zeigen `DidMove=0`: einfache Anpassung nach -Y,
kombinierte Anpassung -Y/+Z und Anpassung plus Eingabe/Schwerkraft. Die
Ausweichziele kollidieren wiederum mit dem Joiner. Der native Falling-Pfad
berechnet aus der ausgebliebenen Bewegung erneut Geschwindigkeit null.

Einzige Runtimeänderung ist die vorhandene Eigenschaft des konkreten Assets
`/Game/SurvivalRpg/Characters/GASP/Mover/RPG/BP_RpgGasp_Mover`:
`SpawnCollisionHandlingMethod = AdjustIfPossibleButAlwaysSpawn`, zuvor
`AlwaysSpawn`. Dies ist eine bewusste RPG-Abweichung vom GASP-Quelldefault.
Die Engine prüft mit der tatsächlich konstruierten Kapsel vor BeginPlay weitere
Ausweichrichtungen. Der vorhandene NP-Liaison übernimmt die angepasste Position
in den initialen Sync-State. Replizierte Client-Pawns übernehmen den
Server-Spawn; sie suchen keinen eigenen Ausweichpunkt.

`crowded-green-01` besteht mit **identischem Testquellcode** und ausschließlich
dieser Assetänderung. Diesmal liegt der zufällig gewählte Host-Start bei
`(0,-500,120)`, der Joiner bei `(0,-560,120)`. Der Respawn erscheint auf der
freien Gegenseite `(0,-440,120)`; die ersten beobachteten Actor-/Sync-Werte
stimmen auf allen Rollen überein. Danach bestehen unverändert mindestens
150 cm Weg, 100 cm/s Geschwindigkeit und der neue Waffenangriff auf allen
Rollen. Die Translation des Startpunkts ändert die getestete Anordnung nicht.

Der Fix behandelt die nachgewiesene **ausweichbare** Mehrspielerbelegung.
Die vorhandene AlwaysSpawn-Rückfallpolitik bleibt erhalten, falls die Engine
keinen freien Punkt findet. Eine allgemeine Garantie bei vollständig verbautem
Checkpoint würde eine eigene Ersatzpunkt-/Fehler-/Retry-Regel benötigen und
ist nicht Bestandteil dieses Schritts. Die historischen Logs sind mit der
reproduzierten Ursache konsistent; ihre damals nicht erfassten Blockerpositionen
werden nicht rückwirkend als bewiesen dargestellt.

## Assetprüfung

Über den konfigurierten lokalen Unreal-MCP-Endpunkt frisch inspiziert, die
einzelne reflektierte Eigenschaft gesetzt, mit `warnings_as_errors` kompiliert,
gespeichert und frisch geladen. Readback bestätigt den neuen Wert; Editor
danach ohne schmutzige Packages geschlossen. Parent bleibt `ARpgMoverPawn`.
Der strukturelle Exportvergleich hält aktive Graphen, Verbindungen, Defaultwerte
und Blueprint-Identität konstant. Kompilierung/Neuladen entfernt unreferenzierte
Knoten und Compiler-Zwischengraphen und erneuert einige Pin-/Text-Metadaten.
Die Exporte enthalten keine CDO-/SCS-Template-Körper; dafür wird kein separater
vollständiger Exportvergleich behauptet. Die konkrete Änderung ist über den
MCP-Aufruf und frischen Eigenschafts-Readback belegt.

## Validierung

UE 5.8.2 Win64 Development Editor erfolgreich gebaut
(`build-editor-diagnostic-01.log`, finaler Teststand `build-editor-crowded-01.log`),
Game ebenfalls erfolgreich (`build-game-01.log`).

| Lauf | Ergebnis | Zweck |
| --- | --- | --- |
| `occupied-diagnostic-01` | 1/1 bestanden, 75 Warnungen | Echter Einzelblocker ohne Runtimefix |
| `crowded-diagnostic-01` | 1/1 fehlgeschlagen, 75 Warnungen | Ursachennachweis; Bewegungs-Timeout |
| `crowded-green-01` | 1/1 bestanden, 74 Warnungen | Identischer Teststand nach Assetfix |
| `regression-green-01` | **11/11 bestanden**, 526 Warnungen, keine Fehler/Skips | Vier Lifecycle-, zwei Mover-Input/Kamera-, drei Startauswahl-, ein AssetComposition- und ein CMC-Fall |

Der abschließende Lauf enthält sieben PIE- und vier native/Asset-Tests.
Warnungen: 465 NetPackageMap, 42 Voice/AdvancedFriendsInterface, 14 Manny-
PoseAssets, ein Respawn-Widget-Tick, ein asynchroner PoseSearch-Index und drei
`RollbackFrame EQUAL PendingFrame`. Keine Fehler, Ensures oder Fatals im
Testintervall. Zwei bereits bekannte ungeklärte `Condition failed`-Meldungen
beim Editorstart vor den Tests sind erhalten, nicht als behoben ausgewiesen.
Report-SHA256:
`6648deb2b42366209531ffa61f9ddae236784bd74e35a9ede7452c27113ae628`.

Die Tests sind gerenderte PIE-/Loopback-Läufe mit Fixed NetworkPrediction.
Keine neue manuelle, Independent-, Packaged- oder WAN-Abnahme behauptet.
NetworkPrediction-Override vor Branchwechsel und nach Abschluss erfolgreich
verifiziert. Alle zehn Maps und sieben SaveGames nach Abschluss unverändert;
Hashes des getesteten Quellcodes und Assets stimmen weiterhin überein. Editor-,
PIE- und Buildprozesse beendet. Lokale Belege unter
`Saved/GaspRespawnFalling20260922` sind ignoriert und nicht automatisch in einem
anderen Checkout verfügbar; der versionierte Automation-Test bleibt ausführbar.

## Nachstellen

Im Development Editor über Session Frontend → Automation oder die Konsole:

```text
Automation RunTests SurvivalRpg.GASP.Mover.Lifecycle.GaspMoverLifecyclePIE.CrowdedCheckpointAllowsMovementAndCombat
```

Der Test erstellt isolierte temporäre PIE-Welten, deaktiviert deren
Disk-Persistenz und führt Tod, echten Late Join und Respawn selbst aus.
Mit dem aktuellen Fix muss er bestehen. Der gesicherte rote Lauf enthält den
Vergleich mit `AlwaysSpawn`; zum normalen Validieren muss die Einstellung nicht
zurückgesetzt werden. Manuell ist die gleiche Konstellation mit zwei Spielern
unmittelbar am gespeicherten Checkpoint und einem dritten respawnenden Spieler
möglich, die Automation stellt sie gezielt her.
