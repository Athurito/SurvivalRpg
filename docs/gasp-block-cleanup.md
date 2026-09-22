# GASP-STAB-01 – Block-Cleanup

Teil von `GASP-02`, begonnen am 22.09.2026 auf `b00ba74b`.
Branch: `codex/gasp-02-block-cleanup`. Implementierungs-Commit:
`43ac69b8b6f109b942ffdd1edc83d0ef333257aa`. Status: **Validiert, PR wird vorbereitet**.

## Befund und Ownership

Der historische CMC-PIE-Abbau in `Saved/GaspMoverNetFix20260918/handoff-final-tests.log`
erreichte `URpgGameplayAbility_Block::ClearBlockState`, nachdem der Defense-
Attributsatz entfernt worden war. Die Wiederherstellung von `BlockAngleDegrees`
löste dabei einen Ensure aus. `URpgGameFeatureAction_AddAbilities` entfernt
Attributsätze vor Ability-Grants; auch der lokale Lyra-Vergleich verwendet diese
Reihenfolge. Equipment- und Attribut-Grants haben zudem unabhängige Besitzer.
Eine globale Umordnung allein würde den Cleanup daher nicht absichern.

Die Änderung gehört in den bestehenden nativen Block-Lifecycle: Server/GAS
besitzen Blockzustand und Attribute, Equipment besitzt die Ability-Grants.
Gespeicherte Werte gelten nur für den ursprünglichen ASC und Defense-Attributsatz.
Die konkreten `GA_*`-Assets, Weapon-Definitionen, Montagen und GASP-AnimGraphs
bleiben designer-owned. Keine neue native Klasse, keine Asset-/Mapänderung und
keine neue Replikationsstruktur sind vorgesehen. Die GASP-Quellanimationen
benötigen für diesen Cleanup keine Adaption.

Stabile Prüfgrenze sind echte ASC-/Ability-Lifecycle-Aufrufe: normale Freigabe,
Attributabbau vor Ability-Ende, Ersatz desselben Attributtyps und wiederholter
Cleanup. Bestehende Netzwerkprüfungen decken Blockeingabe, replizierte Tags,
Montagen, Tod/Respawn und CMC-/Mover-Integration ab.

## Korrektur

- Die Ability speichert Basiswerte zusammen mit schwachen Referenzen auf den
  ursprünglichen ASC und DefenseSet. Ein anderer Satz derselben Klasse darf
  diese Werte nicht erben. Die Registrierung wird vor jedem Restore geprüft,
  weil ein synchroner Attribut-Callback den Satz währenddessen ersetzen kann.
- Cleanup verbraucht seinen Snapshot vor Tag-/Attribut-Callbacks. Timer und
  Block-Tags werden auch bei bereits entferntem DefenseSet freigegeben.
- `EndAbility` prüft GAS-Gültigkeit und Scope-Lock vor Seiteneffekten; eine eigene
  End-Guard schützt den Abschnitt vor dem GAS-Basisaufruf gegen rekursive Enden.
- Aktivierung benötigt den DefenseSet und prüft dessen Besitz auch unmittelbar
  beim Anwenden. Die Attribute werden über `GetNumericAttributeBase` gesichert,
  passend zu `SetNumericAttributeBase`; aktive Modifikatoren bleiben separat.

## Validierung

Lokale Rohdaten: `Saved/GaspBlockCleanup20260922` (ignoriert, nicht automatisch
in anderen Checkouts vorhanden).

- `prepare.py verify`: alle vier checkout-lokalen NetworkPrediction-/Mover-
  Overrides vor der Branchanlage bestätigt.
- UE 5.8.2 Win64 Development Editor vor dem Fix erfolgreich gebaut
  (`build-editor-red-01.log`), Runtime unverändert gegenüber `b00ba74b`.
- Sieben neue native Tests gegen diesen Stand: **2 erfolgreich, 5 fehlgeschlagen**
  (`native-red-01/index.json`). Nachgewiesen sind Modifier-Einbacken, Aktivierung
  ohne Attribute, alte Cancellation gegen neue Block-Tags, Ersatzset-Überschreiben
  und veränderter Endgrund durch rekursive Cancellation.
- Der Missing-Set-Test verbrauchte dabei Unreals einmalige Ensure-Ausgabe.
  Deshalb den eigentlichen Abbaufehler separat in einem frischen Prozess geprüft:
  `RemovedDefenseSetBeforeGrantRemoval` scheitert mit dem originalen
  `ClearBlockState → SetNumericAttributeBase(BlockAngleDegrees)`-Ensure
  (`removed-set-red-01/index.json`, Log ab Zeile 2568). Keine Expected-Error-
  Unterdrückung. Beide Prozesse lieferten trotzdem Exitcode 0; maßgeblich ist
  jeweils der fehlgeschlagene Automation-Bericht.
- Der achte Test ergänzt den synchronen Set-Austausch während des ersten Restore.
- Mit Fix: UE 5.8.2 Win64 Development **Editor und Game erfolgreich gebaut**
  (`build-editor-green-01.log`, `build-game-green-01.log`). Die nachfolgende
  Automation läuft auf genau diesen Quellen; Hashes stehen lokal in
  `green-source-hashes.json`.
- **11/11 verschiedene Tests in einem Lauf erfolgreich**, davon acht ohne und
  drei mit Warnungen; keine fehlgeschlagenen oder ausgelassenen Tests
  (`validation-green-01/index.json`, SHA-256
  `2a02be9181ba7b9064833663f479a300e04e9838e2ce79e7444f30d244e7637a`).
  Die acht nativen Fälle haben keine Warnungen. Die drei gerenderten PIE-Fälle
  prüfen jeweils Listen-Server, Owner und später beigetretenen Observer.
  Mover-Blockfreigabe sowie Tod während Block und Respawn mit optionalem
  Retarget-Follower bestanden; der historische CMC-Abbaufall bestand ebenfalls.
  Beim Blocktest tritt der Observer vor der Blockaktivierung bei, beim
  Death-/Follower-Fall vor dem Tod; CMC prüft Beitritt während Crouch. Kein
  Beitritt während bereits aktivem Block/Tod und kein eigener lokal gesteuerter
  Listenhost-Block sind damit nachgewiesen.
- 238 Warnungen in den drei PIE-Tests erhalten: Voice-Interface, NetGUID für
  temporäre Testwelten, veraltete Manny-PoseAssets sowie je eine Respawn-Widget-
  Tick-, NetworkPrediction-`RollbackFrame EQUAL PendingFrame`- und PoseSearch-
  AsyncIndex-Warnung. Kein Fehler, Ensure oder Fatal im
  Testintervall. Die zwei bereits vor Testbeginn auftretenden
  `LogAutomationTest: Error: Condition failed`-Startmeldungen sind weiterhin
  ungeklärt; der gesamte Startlog wird deshalb nicht als fehlerfrei bezeichnet.
- Tatsächlich geladenes NetworkPrediction-Modul:
  `D:/Repos/SurvivalRpg/Plugins/NetworkPrediction/Binaries/Win64/UnrealEditor-NetworkPrediction.dll`.
  Alle zehn Projekt-/GameFeature-Maps und sieben vorhandenen SaveGames nach
  den Tests per SHA-256 unverändert. Alle gestarteten Editor-/Testprozesse beendet.
- Quellen seit den erfolgreichen Builds/Tests unverändert; Commit `43ac69b8`
  enthält genau diesen Runtime-/Teststand. Keine Assets geändert; kein neuer
  Asset-Compile/Audit erforderlich. Unabhängiges read-only Review und
  `git diff --check` ohne blockierenden Befund.

Reproduzierbare Automation-Filter:

```text
SurvivalRpg.Combat.Block.Lifecycle
SurvivalRpg.GASP.CMC.GaspCMCExperiencePIE.RemoteMovementLateJoinAndEquipmentMontage
SurvivalRpg.GASP.Mover.Gameplay.GaspMoverGameplayPIE.RightMouseHoldReplicatesBlockAndReleasesCleanly
SurvivalRpg.GASP.Mover.Lifecycle.GaspMoverLifecyclePIE.DeathCancelsHeldBlockAndRespawnRecomposesOptionalFollower
```

Die Filter mit `+` verbinden und dem gebauten `UnrealEditor-Cmd.exe` zusammen
mit `SurvivalRpg.uproject`, `/Engine/Maps/Entry`,
`-ExecCmds="Automation RunTests <Filter>"`,
`-TestExit="Automation Test Queue Empty"`, `-ReportExportPath=<neuer Ordner>`,
`-abslog=<neues Log>`, `-unattended -nop4 -nosteam -nosplash -nosound
-NoSaveConfig -RenderOffscreen` übergeben. Die Netzwerkprüfungen benötigen
gerenderten PIE; kein NullRHI. Den JSON-Bericht prüfen, nicht nur den Exitcode.

Kein neuer manueller Sichttest, Packaged-/WAN-Test oder separater Nachweis eines
absichtlich erzwungenen GAS-Scope-Locks. Die unveränderte Timer-Expiry hat keinen
eigenen neuen Test. Der Schritt ändert keine Montagen, AnimGraphs oder Assets.

Die übrigen `GASP-02`-Befunde bleiben offen, insbesondere Falling nach Respawn
(`GASP-STAB-02`) und die dokumentierten Netzwerk-/Messgrenzen.
