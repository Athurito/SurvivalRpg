# GASP-03 – Source-Audit der Mover-Ragdoll-Variante

Stand: **26.09.2026, begrenzter Source-Audit abgeschlossen, in Review;
Commit/PR noch offen**. Branch
`codex/gasp-03-ragdoll-source-audit`, Basis
`4ed174d2cdc1a2d52fc5a9272ad68437fed98025`, bestätigter Merge von
[PR #150](https://github.com/Athurito/SurvivalRpg/pull/150) am 26.09.2026 um
13:09:25 UTC. Noch kein GASP-03-Implementierungscommit oder Arbeits-PR.

Dieser erste Teilauftrag prüft den originalen GASP-Mover-Ragdoll-Pawn, seine
Physics-Control-/Mover-Abhängigkeiten und die passenden Schnittstellen in
SurvivalRpg. Ergebnis sind eine nachvollziehbare Quell-Ziel-Zuordnung, klare
Zuständigkeiten und ein begrenzter Implementierungsumfang. Der Audit behauptet
noch keine integrierte Experience/PawnData, importierte Assets, Runtimeänderung
oder neu bestandene Unreal-Builds beziehungsweise Gameplaytests.

## Herkunft und Belege

Der Audit untersucht das tatsächlich geöffnete Originalprojekt
`D:/Repos/GameAnimationSample` mit **UE 5.8.2**. Ein bestimmter GASP-Release-
Identifier ist nicht belegt. Die bestehende SurvivalRpg-Importkopie ist kein
ausreichender Ersatz für diese Originalquelle: `initial-source-parity.json`
unter `Saved/GaspRagdollSourceAudit20260926` zeigt abweichende Binärhashes für
alle vier verglichenen Kernassets:

- `Blueprints/SandboxCharacter_Mover_Ragdoll.uasset`
- `Blueprints/SandboxCharacter_Mover.uasset`
- `Blueprints/SandboxCharacter_Mover_ABP.uasset`
- `Blueprints/MovementModes/BP_MovementMode_Ragdoll.uasset`

Die Hashdifferenzen belegen unterschiedliche Dateien, für sich allein aber
keine konkrete semantische Änderung. Die folgenden Aussagen verwenden deshalb
frische Snapshots und Graphexporte aus dem Originaleditor. `source-ragdoll-pawn.json`,
`source-mover-parent.json` und `source-ragdoll-mode.json` enthalten reflektierte
Objekte/CDOs sowie Verweise auf die ergänzenden nativen T3D-Exporte in
`native_exports`. Die `source-*.dsl`-Dateien machen den Graphablauf lesbar;
sie ersetzen bei Pinwerten oder Verbindungen nicht den vollständigen Export.

Diese Rohbelege liegen im ignorierten `Saved/` und sind in einem anderen
Checkout nicht automatisch vorhanden. Das versionierte
[Auditmanifest](assets/gasp-ragdoll-source-audit.json) hält Quellen,
SHA-256-Hashes, Snapshot-Abdeckung, Graphmerkmale und ausgewählte Zuordnungen
fest. Es enthält **44 Quellkandidaten**: 27 vorhandene Foundation-Referenzen
und 17 vorgeschlagene Zielkandidaten. Diese Liste ist ausdrücklich **keine
Import-Whitelist** und keine implementierte Variante; vorhandene Ziele sind
vor einer Wiederverwendung semantisch zu prüfen.

Das korrigierte Registry-Inventar
`source-dependency-inventory-corrected.json` setzt alle sieben UE-5.8-
Abfrageoptionen ausdrücklich: harte/weiche Package-Referenzen, Game- und
Editor-only-Packages, Searchable Names sowie harte/weiche Management-
Referenzen. Package- und Management-Abfragen sind getrennt. Die rekursive
Package-Hülle enthält **3017 Projektpakete**, **25907 Package-Kanten**,
**0 Management-Kanten**, **164 externe Grenzen** und keine in der Registry
fehlenden Projektpakete. Sie umfasst Editor-only-Referenzen; daraus folgen
weder eine minimale Importliste noch vollständige dynamische, Cooked- oder
Runtime-Abhängigkeiten. Die erste Abfrage hatte einen ungültigen Management-
Label, weil zusätzliche UE-5.8-Filterfelder nicht ausdrücklich getrennt waren;
für diese Aussagen gilt ausschließlich der korrigierte Capture.

Alle **sieben semantischen Snapshots sind partiell** und weisen nicht lesbare
Felder aus. Native T3D-Exporte ergänzen sie; der Bericht behauptet keine
vollständige semantische Exportabdeckung. Sieben DSL-Exporte scheiterten
(sechs Toolfehler, ein Slash im Dateinamen). Zusätzlich kollidieren einzelne
Graph-Blattnamen als Ausgabedateinamen. Die DSL-Dateien sind daher kein
vollständiger Graphkorpus; das Manifest hält die Lücken und die Abdeckung
der Snapshots mit vollständiger Objektidentität fest.

Während des Ladens/Exports der ersten vier Blueprints wurden dirty-Quellpakete
beobachtet; eine einzelne verursachende API ist nicht belegt.
Die Vorher-/Nachher-Dateihashes blieben identisch; die ausschließlich lesende
Quellsitzung wurde ohne Speichern verworfen. Ein dirty-Flag beim Laden ist
hier deshalb kein gespeicherter Quellumbau. Die spätere korrigierte
Quellsitzung schloss mit leeren Dirty-Content-/Map-Listen regulär; die
abschließenden Erhaltungsprüfungen stehen unten.

## Belegtes Sample-Verhalten

Die [Roadmap](gasp-integration-roadmap.md) führt die vorhandene Mover-Variante
und akzeptierte Traversal getrennt von der noch nicht integrierten Ragdoll-
Variante. Die bisherige Assetbasis enthält unter `Mover/Ragdoll` die referenzierte
Input-Struktur; daraus folgt keine vollständige migrierte Pawn-Abhängigkeit.

Der Original-Mover-CDO verwendet `MoverNetworkPredictionLiaisonComponent` und
`bSyncInputsForSimProxy=True`. Die erfassten Bewegungsmodi haben
`bSupportsAsync=False`. Das belegt den konfigurierten Backend-/Inputpfad,
aber noch keine serverautoritativ sichere Ragdoll-Rekonstruktion, Korrektur
oder Late-Join-Abnahme im RPG-Projekt.

| Quellpfad | Belegter Ablauf | Bedeutung für die Adaption |
| --- | --- | --- |
| `source-ragdoll-pawn-EventGraph.dsl`, `BeginPlay` / `PostABPTick` | Mesh-Tick vor `AC_PostABPTick`, dieser vor PhysicsControl; Controls und BodyModifiers werden aus dem PhysicsControlAsset für das Mesh erzeugt. | Tickreihenfolge und Gameplay-Mesh müssen in der RPG-Komposition erhalten beziehungsweise ausdrücklich ersetzt werden. |
| `TriggerRagdoll` | Optional Montage-Stop, InjuryState setzen, Mesh-Physik aktivieren, Profil `Ragdoll`, Mover-Modus `Ragdoll` einreihen; vorübergehende Rotationssperre und Prüfung im Folgeframe. | Das ist Sample-Ablauf, noch kein GAS-Abbruch-/Ownershipvertrag. |
| `source-ragdoll-pawn-ProduceInput.dsl` | Bei nächstem Modus `Ragdoll` ergänzt der Pawn benutzerdefinierte Ragdoll-Inputs und die Zielorientierung in der Mover-Inputcollection. | Die erfasste Physics-Pose wird Bewegungsinput; Autoritätsprüfung und Replay-Verhalten müssen im RPG-Piloten festgelegt werden. |
| `source-ragdoll-pawn-Get_RagdollTransform.dsl` | Zieltransform wird aus `pelvis` und `spine_05` des SkeletalMesh abgeleitet. | Eine lokale Bone-Pose darf nicht ungeprüft einen parallelen autoritativen Bewegungszustand bilden. |
| `source-ragdoll-mode-SimulationTick.dsl` | Ragdoll-Input lesen, Capsule-Ziel über Boden-SphereTrace anpassen, `TrySafeMoveAndSlide` und Mover-Sync-Ausgabe für Position, Orientierung und Geschwindigkeit. | Capsule-Führung und Physics-Pose sind gekoppelt; feste Sample-Geometrie und laufzeitabhängige Queries brauchen einen bewussten Projektvertrag. |
| `source-ragdoll-pawn-SetPhysicsProfile.dsl` | Profilwechsel steuert PhysicsControl, gegenseitige Bein-Kollision und Constraint-Profile. | Designer-Tuning erhalten; Wiederherstellung bei Unterbrechung und Tod ist zusätzlich abzusichern. |

Die Kurven `Ragdoll_Strength_Legs`, `Ragdoll_Strength_Arms`,
`Ragdoll_Strength_Torso` und `Ragdoll_Strength_Head` steuern im Profil `Ragdoll`
die jeweiligen `ParentSpace_*`-ControlMultiplier. Der Ablauf ist in
`source-ragdoll-pawn-Ragdoll_UpdatePhysicsStrengthsFromCurves.dsl` belegt.
Dies ist Animations-/PhysicsControl-Komposition, kein Grund, das konkrete
Kurven- oder Profiltuning in eine native Gameplayklasse zu verlagern.

`source-physics-control.json` und sein nativer Export belegen
`PCA_SandboxCharacter` ohne ParentAsset oder AdditionalProfileAssets und mit
`PA_UEFN_Mannequin`. Die sechs Limbstarts liegen an `neck_01`, `clavicle_l/r`,
`thigh_l/r` und `pelvis`. Die Profile heißen `Kinematic`, `PhysicalAnimation`
und `Ragdoll`: Kinematic deaktiviert die Controls und verwendet kinematische
Modifier; Ragdoll deaktiviert WorldSpace und setzt unter anderem ParentSpace
AngularStrength 10, Damping 3 und LinearDamping 0. PhysicalAnimation besitzt
eigene WorldSpace-/ParentSpace-/Feet-Werte. Diese sparsamen Profildefinitionen
sind keine vollständigen Zustandssnapshots; bei Wechseln dürfen implizite
Defaults und vorherige Einstellungen nicht gleichgesetzt werden.

Beim Modusausstieg setzt `On_RagdollMode_Exit` das Capsuleprofil zurück,
speichert den PoseSnapshot `Ragdoll`, überschreibt `PoseHistory` aus dem Mesh
und liest die History über das AnimBP-Interface. `CHT_GetUpMontages` erhält
PoseHistory, aktuellen Bewegungsmodus und die Rolling-Getup-Auswahl;
`PlayMontage(MoverActor)` verwendet Montage und Startzeit des Choosers.
Danach setzt der Graph die lineare und angulare `ParentSpace`-Stärke auf 0
und stellt im Folgeframe das Standardprofil wieder her, sofern der nächste
Mover-Modus nicht erneut `Ragdoll` ist. Die Rolling-Auswahl verwendet im
erfassten Graphen eine horizontale COM-Geschwindigkeit über 150 cm/s.

Der frische Chooser-Export `source-getup-chooser.json` enthält genau vier
Zeilen: `AM_M_ragdoll_getup_stand_F/B` für `Rolling=false` und
`AM_M_ragdoll_getup_roll_R/L` für `Rolling=true`, jeweils im Quellenum
`ON_GROUND` (`NewEnumerator4`), auf das die MovementModeMap `Walking` abbildet.
Eine PoseSearch-Spalte verwendet PoseHistory und liefert `MontageStartTime`;
`MaxNumberOfResults=1`. Die eingebettete `PSD_GetUp` verwendet `PSS_Ragdoll`
und BruteForce. Diese Auswahl ist folglich mehr als ein einfacher
Front-/Back-Boolean und sollte mit ihrer PoseHistory-/Startzeit-Semantik
übernommen werden.

`source-post-abp-tick.json` belegt einen nicht replizierenden
`AC_PostABPTick`, dessen ReceiveTick den Tick-Dispatcher aufruft; die CDO-
Tickgruppe ist DuringPhysics. Zusammen mit den Pawn-Prerequisites erklärt
das die beabsichtigte Reihenfolge. Eine tatsächlich ausgeführte Live-
Tickparität wurde in diesem Source-Audit nicht gemessen.

Die Getup-Callbacks `OnCompleted`, `OnInterrupted` und `OnBlendOut` des
Montagestarts sind im nativen Export unverbunden. Aus diesem Samplepfad folgt
deshalb kein fertiger GAS-Finish-/Cancellationvertrag für das Projekt.

Der frische AnimBP-Audit (`source-mover-abp.json` und
`source-mover-abp-Update_PropertiesFromCharacter.dsl`) zeigt einen vorhandenen
Snapshot-Ansatz: `BlueprintUpdateAnimation` liest die Pawn-Interfaces
`Get_PropertiesForAnimation` und `Get_PropertiesForRagdoll` in lokale
`CharacterProperties`/`RagdollProperties`. Der Thread-Safe-Update führt unter
seinen Owner-/Mover-/Konfigurationsgates `Update_Logic` aus; Ragdoll-Property-
Access im AnimGraph liest die lokalen Daten. Die hier geprüften Pawn-
PhysicsControl-/Body-Updates liegen im Pawn-Tick beziehungsweise `PostABPTick`.
Daneben liest `ProduceInput` die Bone-Pose; der Ragdoll-`SimulationTick` führt
seinen Boden-Trace und die Capsule-Bewegung aus. Das ist ein begrenzter
Graphbefund, kein umfassender Thread-Safety- oder Netzwerkparitätsnachweis.
Die erfassten Ragdoll-Assets umfassen die BlendSpaces `BS_Ragdoll_Flail`,
`BS_Ragdoll_Reach`, `BS_Ragdoll_OnGround_Poses`, `BS_Ragdoll_Rolling_Poses`,
`M_ragdoll_poses_injured`, `Ragdoll_Fall_to_Ground_BlendCurve` sowie die
InjuryState-/Properties-Strukturen. Der PoseSnapshot `Ragdoll` wird in einem
eigenen Blend-Out-Pose-State verwendet; gebundene Pins sind bei einer Adaption
maßgeblicher als unbenutzte Node-Structdefaults.

## Konkrete Integrationsrisiken

1. **Endgültiger Tod darf kein Getup auslösen.**
   `source-ragdoll-pawn-On_MovementModeChanged_PostFinalize.dsl` ruft nach dem
   Parent bei jedem `PreviousMode=Ragdoll` den beschriebenen Ausstieg auf,
   ohne Gate auf neuen Modus oder Health. Unverändert übernommen würde dieser
   Pfad auch bei einem Übergang zu `RpgDead` Capsuleprofil und Getup anstoßen.
   Das ist eine Integrationsfolgerung aus dem Graphen, kein ausgeführter
   Fehlertest. Der Pilot muss den kanonischen Death-Cleanup erhalten und
   lebendes Aufstehen ausdrücklich davon trennen.
2. **Ragdoll-Eintritt und Getup brauchen Gameplay-Ownership.** Die geprüften
   Eingabeketten führen ohne Authority-/GAS-Prüfung zu `TriggerRagdoll`
   beziehungsweise `ExitRagdoll`; Getup spielt direkt über den Mover-Node,
   mit `bShouldStopAllMontages=true`. `QueueNextMovementMode` ist ein lokaler
   Simulationsaufruf. Der konfigurierte NP-Backendpfad allein belegt keine
   replizierte Eintrittsentscheidung, Cancellation oder Late-Join-Rekonstruktion.
   Der RPG-Pilot muss diese Verträge an GAS/Mover anbinden.
3. **Physics-Input ist hier bewegungswirksam.** Der Ragdoll-Modus übernimmt die
   physisch gewonnene Zielposition, verwendet einen SphereTrace mit Radius
   30 cm, 88 cm Tiefe und 86 cm Bodenoffset und bewegt die Capsule. Der native
   Export bestätigt `bSweep=false` am Move-Aufruf. Diese Quelle ist daher kein
   bloß kosmetischer Follower. Gültigkeit, Capsule-Abmessungen, Kollision und
   Wiederholung nach Korrektur benötigen einen überprüfbaren Projektvertrag.
4. **Demo-Interaktionen besitzen fremde Ressourcen.**
   `source-ragdoll-pawn-Try_MultiCharacterInteraction.dsl` setzt gegenseitige
   Move-Ignores und Mesh-Tick-Prerequisites; der Interaktions-Montagepfad ruft
   beim Blend-Out `ClearMoveIgnoreComponents` auf. Eine pauschale Übernahme
   könnte projektseitige Traversal-Leases löschen. Shove, Takedown, Aim-/Shoot-
   Demo, Spawn-Demo und automatisches NPC-Getup gehören deshalb nicht zum
   ersten kontrollierten Ragdoll-/Getup-Piloten.

## Vorgesehene RPG-Zuordnung und nächster begrenzter Pilot

Die spätere Umsetzung soll die bestehende Experience-/PawnData-Komposition
nutzen. Stabile Authority-, Replikations-, Prediction- und Lifecycle-Mechanismen
gehören in vorhandene oder begründet erweiterte native Schnittstellen; konkrete
Pawn-/Animationskomposition, Referenzen und Tuning bleiben in Blueprint oder
DataAssets. Der genaue Zuschnitt ist ein Ergebnis des Audits, keine bereits
beschlossene neue Klasse oder vollständige Migration.

Der nächste kleine GASP-03-Pilot soll einen vom Original abgeleiteten,
designer-owned Pawn-/Animationsaufbau mit PhysicsControl und Getup auf die
vorhandenen RPG-Schnittstellen setzen, zunächst mit kontrolliertem Eintritt
und Aufstehen auf freier unterstützter Fläche. Der bestehende synchrone
NetworkPrediction-Mover bleibt die Bewegungsbasis. Sample-Demo-Eingaben, Kamera, Schießen
oder Mehrcharakter-Interaktionen sind keine automatische Importanforderung.
Die 44 Manifestkandidaten sind Ausgangspunkte für den Piloten; eine tatsächlich
benötigte minimale Importliste ist erst aus dessen konkreten Änderungen und
erneuter Referenzprüfung abzuleiten.

| Verantwortung | Vorgesehener Owner / noch nachzuweisender Vertrag |
| --- | --- |
| Komposition und Inhalte | Eigene Experience/PawnData sowie projektlokale Blueprint-/PhysicsControl-/Chooser-/Animationsassets, vorhandene Varianten erhalten. |
| Lebendes Ragdoll | Projektseitiger autoritativer Lifecycle mit Eintritt, aktivem Zustand, Getup und Unterbrechung; noch umzusetzen und über Netzwerk zu beweisen. |
| Endgültiger Tod / Respawn | Bestehender RPG-Health-/Death-/Respawn-Pfad; lebendes Ragdoll darf keinen zweiten Todspfad einführen oder nach Tod wieder aufstehen. |
| GAS / Equipment / Eingabe | Vorhandene Owner für Gameplay- und Montageabbrüche, Equipmentbindung und Kontrollrückgabe; keine bloße Übernahme des Sample-Montage-Stops. |
| Bewegung / Physics-Pose | Verbindliche Trennung zwischen autoritativem Mover-Zustand und kosmetischer physischer Pose; Owner, Authority, Observer, Korrektur und Late Join getrennt prüfen. |
| Mesh / Retargeting | UEFN bleibt Gameplay-Mesh; ein optionaler Retarget-Follower besitzt keine zusätzliche Bewegungs- oder Physics-Authority. |

Das Manifest schlägt `BP_RpgGasp_MoverRagdoll` unter
`/Game/SurvivalRpg/Characters/GASP/Mover/Ragdoll/RPG` auf Basis des vorhandenen
`BP_RpgGasp_Mover` vor, mit `ABP_RpgGasp_Mover` als Animationsausgangspunkt,
eigener `DA_PawnData_GaspMoverRagdoll` und
`/Game/SurvivalRpg/System/Experiences/RpgGaspMoverRagdollExperience`.
Diese Zielnamen sind vorgeschlagen, nicht bereits angelegte Assets.

Die vorhandenen RPG-Einstiegspunkte sind konkret:

| Projektdatei / Schnittstelle | Bestehender Vertrag für den Piloten |
| --- | --- |
| `Core/Character/RpgMoverPawn.cpp`, `OnDeathStarted` / `OnDeathFinished` | Terminale Bewegungssperre, Capsule-NoCollision und Weitergabe an den bestehenden GameMode-Todspfad. |
| `Core/Character/RpgCharacterMoverComponent.cpp`, `DisableMovementForDeath` / `PrepareTraversalSimulation` | Tod sperrt Bewegung terminal für diesen Pawn; aktive Traversal und Root Motion werden aufgeräumt, fremder Moduswechsel beendet Traversal. |
| `Core/Character/RpgHealthComponent.cpp`, `OnRep_DeathState`; `AbilitySystem/Abilities/RpgGameplayAbility_Death.cpp` | Monotone DeathState-Rekonstruktion und GAS-Abbruch nicht überlebender Abilities bleiben kanonisch. |
| `Core/Game/RpgGameModeBase.cpp`, `NotifyPlayerDeath` / `ExecuteRespawn` | Bestehender Inventory-Drop-, UnPossess-, Destroy- und Respawnpfad. |
| `Core/Character/RpgPawnExtensionComponent.cpp`, `UninitializeAbilitySystem` / `FindGameplayMesh`; `Equipment/RpgEquipmentInstance.cpp` | Alte GAS-Ressourcen ohne Beschädigung neuer Avatarbindung freigeben; Equipment am kanonischen Gameplay-Mesh binden. |
| `AbilitySystem/RpgAbilitySystemComponent.cpp`, Montage-OnRep / Mover-Präsentation / `ClearActorInfo` | Bestehende Montagekorrelation, Darstellung und Cleanup erhalten; Getup muss deren Ownership berücksichtigen. |
| `Animation/RpgRuntimeRetargetComponent.cpp`, `ApplyProfile` / Cleanup | Optionaler Follower bleibt kosmetisch, ohne Kollision oder eigene Replikation; keine zweite Physics-Authority einführen. |

Die Pfade sind relativ zu `Source/SurvivalRpg`. Ihre Existenz und Zuständigkeit
sind quellengeprüft; sie sind noch kein ausgeführter Ragdoll-Netzwerktest.

Die Pilotabnahme muss mindestens echten Ein-/Ausstieg, Unterbrechung während
Getup, Tod während Ragdoll/Getup, Respawn, Observer und Late Join erfassen.
Collision-, Montage-, PhysicsControl- und Inputzustände müssen dabei eindeutig
freigegeben oder rekonstruiert werden. Das ist die spätere Abnahmeforderung,
kein Ergebnis dieses reinen Source-Audits.
Dabei sind tatsächlich erzeugte Controls/BodyModifiers, gültiger Mesh-Posecache,
die vier Kurven und Controlsets sowie eine gewählte Getup-Montage mit gültiger
Chooser-Startzeit und PoseHistory/Snapshot zu prüfen. Eine bloße Modusfolge
belegt keine Bonepose- oder Root-Kontinuität. Normaler Abbruch und Tod dürfen
jeweils kein verspätetes Getup wieder freigeben.

`Config/DefaultEngine.ini` besitzt bereits `CharacterCapsule`, gemeinsame
`Ragdoll`-Profilanpassungen, den dokumentierten `Obstacle`-Kanal und
`DDCVar.CharacterPhysics`-Definitionen. Gleiche Quellnamen sind damit vorhanden;
eine pauschale Übernahme der Sample-Konfiguration ist nicht erforderlich.
Die beabsichtigte Nutzung der vorhandenen Profile ist im Piloten dennoch zu
prüfen, besonders beim Übergang zu Tod und beim Wiederherstellen der Kollision.

## Ownership und Abnahmegrenzen

- Root koordiniert Quellprüfung, sämtliche Editor-/MCP-/Buildsitzungen und
  die Repository-Anweisung in `AGENTS.md`.
- Der Dokumentationsagent besitzt diesen Bericht, Roadmap, Übergabe und den
  bestätigten VAL-03-Mergestatus. Weitere Auditbeiträge bleiben rein lesend,
  soweit keine eigene Datei ausdrücklich zugeteilt ist.
- Dieser Audit führt keine Runtime- oder Assetmigration vorweg. Die belegten
  Quellabläufe, das korrigierte Inventar und die vorgeschlagenen Zuständigkeiten
  bilden den abgeschlossenen begrenzten Audit. Commit/PR und dessen Review
  stehen noch aus; die gesamte GASP-03-Variante ist damit nicht implementiert.

## Tatsächliche Abschlussprüfung

`Saved/GaspRagdollSourceAudit20260926/final-validation.json` hält die finalen
lokalen Prüfungen fest:

- **115 SHA-Prüfungen** stimmen: 44 Originaldateien, 44 Importkopien und
  27 vorhandene Zielreferenzen. Die vier anfangs verglichenen Original-
  Kernassets sind ebenfalls unverändert; keine Quellassetänderung gespeichert.
- Alle **zehn Maps und sieben SaveGames** sind unverändert; alle vier
  generierten Plugin-Overrides bestehen `verify`.
- Die korrigierte Quellsitzung mit PID 46052 hatte beim regulären Schließen
  keine dirty Content-Pakete oder Maps. Es bleiben keine Unreal-Prozesse.
- Der unabhängige Manifestabgleich bestätigt Zuordnungen, lokale Hashes und
  die korrigierten Registry-Zahlen. Der bisherige Review meldet keine P1-/P2-
  Findings; `git diff --check` besteht.

Es wurden für diesen Dokumentations-/Metadaten-Audit **kein neuer Unreal-Build,
keine Automation und kein PIE-Lauf** ausgeführt. Die Source-/Hash-/Registry-
Prüfungen ersetzen weder eine Implementierungsabnahme noch den späteren
Ragdoll-Netzwerk- und Lifecycle-Nachweis. Nächster begrenzter Auftrag ist der
**GASP-03-Pilot** mit den oben beschriebenen Grenzen.

Die Nutzerfreigabe vom 26.09.2026 erlaubt für nicht sinnvoll manuell prüfbare
Schritte nach geeigneter automatischer Validierung und Review direktes Pushen,
Mergen und Fortsetzen des nächsten begrenzten Roadmap-Schritts. Dafür ist keine
zusätzliche Sichtabnahme abzuwarten. Neue manuelle oder automatische Ergebnisse
werden dadurch nicht vorausgesetzt; tatsächlichen Merge gesondert bestätigen.

Die Ergebnisse von [VAL-03](gasp-buffered-traversal-replay.md) sind historisch
und wurden für diesen Auditstart nicht wiederholt. Dessen ältere Pending-
Fixture-Timingempfindlichkeit, VAL-01/02 und die NET-03-Rekonstruktionsgrenze
bleiben offen. Historische Physics-Control-Befunde aus dem
[Follow-up](gasp-physics-control-followup.md) müssen gegen aktuelle Quellen
geprüft werden; der Auditstart bestätigt weder deren Fortbestand noch Behebung.
