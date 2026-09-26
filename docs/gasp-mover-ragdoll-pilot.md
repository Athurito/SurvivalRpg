# GASP-03 – Begrenzter Ragdoll-/Getup-Pilot

Stand: **26.09.2026, Validierung abgeschlossen; Commit/Draft-PR offen**. Branch
`codex/gasp-03-ragdoll-pilot`, Basis
`9baac5f36fd978b86c364df3e82aa74d76cfb7d2`, bestätigter Merge des
[Source-Audits in PR #151](https://github.com/Athurito/SurvivalRpg/pull/151)
am 26.09.2026 um 13:40:22 UTC. Noch kein Pilot-Implementierungscommit oder
Arbeits-PR. Finaler Editor-Build 12 und inkrementeller Game-Build bestehen.
Ragdoll, Getup, Kontrollrückgabe und Waffenangriff sind auf der gespeicherten,
neu geladenen Pilotkarte mit frischen Binaries manuell geprüft. Die finale
Regression besteht mit **20/20**, einschließlich aller sechs neuen Fälle.
Der ältere sechsteilige Lauf bleibt **5/6** mit inzwischen korrigierter
Host-Fixture. Das Abschlussaudit ist bestanden; Commit und Draft-PR zur
manuellen Sichtprüfung stehen noch aus. Kein automatischer Merge dieses Piloten.

Der vereinbarte erste Umfang ist **lebendes Ragdoll aus dem stationären,
nicht geduckten Stand und kontrolliertes Aufstehen auf freier, unterstützter,
ebener Fläche**. UEFN bleibt das Gameplay-Mesh für
Physics, GAS-Montagen, Notifies und Equipment. Der Pilot verbindet
originalabgeleitete Blueprint-/PhysicsControl-/Getup-Inhalte mit den
bestehenden RPG-Schnittstellen. Während der Ragdoll-Phase bleibt die
autoritative Capsule verankert; PhysicsControl bewegt das Mesh. Es gibt
keine kontrollierte Rollkraft und keine Bewegungsautorität aus clientseitigen
Bone-Transforms. Diese bewusste Begrenzung gegenüber der Quelle ist Teil
des Piloten, keine vollständige Portierung ihres Physics-Bewegungspfads.

## Ausgangspunkt und begrenzter Umfang

Der [Source-Audit](gasp-mover-ragdoll-source-audit.md) belegt Originalgraphen
aus `D:/Repos/GameAnimationSample` unter UE 5.8.2. Ein genauer Sample-Release-
Identifier ist nicht bekannt. Das
[Auditmanifest](assets/gasp-ragdoll-source-audit.json) enthält 44 Kandidaten,
keine Import-Whitelist. Vorhandene Foundation-Referenzen brauchen semantische
Prüfung; die tatsächlich benötigte minimale Assethülle ergibt sich aus dem
konkreten Piloten. Das [Pilotmanifest](assets/gasp-ragdoll-pilot.json) erfasst
17 Quellkopien und acht RPG-Kompositionsassets. Original- und Importkopie-
Hashes wurden getrennt frisch gelesen. Nach dem Asset-Freeze sind auch alle
25 gespeicherten Zielhashes erfasst und erneut geprüft.

Zu erhalten sind die designer-owned PhysicsControl-Profile und Kurven,
PoseSnapshot/PoseHistory und die PoseSearch-basierte Getup-Auswahl samt
Startzeit. Die bestehende RPG-Experience-/PawnData-Komposition bleibt der
Einstieg für die neue Variante. Andere Experiences und ihre Inhalte werden
dabei nicht als bereits migriert oder ersetzt behandelt.

Der Source-Audit zeigt zwei besonders relevante Grenzen: Der Sample-Ausstieg
startet bei jedem Verlassen von Ragdoll den Getup-Pfad, und physische
Bone-Transforms gehen über Ragdoll-Inputs in die Capsule-Bewegung ein.
Im RPG-Piloten gewinnt endgültiger Tod immer gegenüber lebendem Ragdoll
und Getup. Der native Mover-Modus `Ragdoll` hält die autoritative Capsule;
die lokale Physics-Pose liefert keine Client-Bewegungstransforms an den
Server. Der bestehende Health-/Death-/Respawn-Pfad bleibt terminal für den
alten Pawn.

## Vereinbarter Eingabe- und Abilityvertrag

Eine konkrete Blueprint-`GA` verwendet `ServerInitiated`, `InstancedPerActor`
und die Aktivierungsgruppe `Exclusive_Blocking`. **R** startet den Eintritt;
ein zweites **R** fordert über `WaitInputPress` das Aufstehen an. Die Ability
behält damit die Lebensdauer und den Abbruch des Vorgangs. Eine native
Auswahl-Task korreliert das Ergebnis mit Revision und konkreter Aktivierung,
sodass ein verspätetes Ergebnis keinen späteren Vorgang freigibt.

Die originale PoseHistory-/Chooser-Semantik mit vier Getup-Montagen bleibt
designer-owned. Die serverseitig gewählte Montage und ihre Startzeit werden
für genau den aktiven Vorgang veröffentlicht. Der Pilot setzt keine rohe
Sample-`PlayMontage`-Kette als Ersatz für GAS-Ownership ein. Diese Beschreibung
hält den vereinbarten Vertrag fest; die bisherigen Builds und Ausführungen
stehen unten, die vollständige Asset-/Gameplayabnahme bleibt offen.

Die nativen Defaults begrenzen den Eintritt auf höchstens **5 cm/s** und
verlangen mindestens **0,35 s** Ragdoll vor der Auswahl. Eintritt und Auswahl
prüfen Authority, lebenden Avatar und dieselbe aktive GAS-Aktivierung;
veröffentlichte Montagen müssen aus der konfigurierten Liste stammen und
eine gültige Startposition besitzen. Dies sind geprüfte API-/Codeverträge,
keine eigenständigen Netzwerk- oder Assetabnahmen.

## Offlinevergleich der Importkopien

Die frischen `import-*.json`-Snapshots unter
`Saved/GaspRagdollPilot20260926` wurden mit den entsprechenden
`source-*.json`-Originalsnapshots aus `Saved/GaspRagdollSourceAudit20260926`
verglichen. Diese Belege betreffen die vorhandenen Importkopien unter ihren
ursprünglichen `/Game`-Pfaden, nicht bereits eine vollständige Abnahme der
neuen RPG-Pilotassets. Die Snapshots sind weiterhin partiell; native T3D-
Exporte ergänzen die Python-unlesbaren Werte.

| Asset | Beobachteter Vergleich Original → Importkopie |
| --- | --- |
| `SandboxCharacter_Mover_Ragdoll` | Der separat erstellte `import-source-pawn-comparison.json` bestätigt 28 identische authored Graphen samt Parent/Komponenten. Bei 2072 erfassten Objekten unterscheiden sich nur drei reflektierte `bReplicateUsingRegisteredSubObjectList`-Werte von `true` zu `false`: Pawn-CDO sowie PhysicsControl- und PostABPTick-Komponententemplates. |
| `PCA_SandboxCharacter` | Gesamter exportierter Semantic-Teil identisch; auch der native T3D-Text ist exakt gleich. Keine erfasste Profil-/Limb-/PhysicsAsset-Abweichung. |
| `CHT_GetUpMontages` | Gesamter exportierter Semantic-Teil und nativer T3D-Text exakt gleich; keine erfasste Änderung an Auswahl, PoseSearch oder Startzeitausgabe. |
| `AC_PostABPTick` | Beide authored Graphen `EventGraph` und `Tick`, Parent, Pins/Verbindungen und Dependencies sind gleich. Der CDO-Wert `bReplicateUsingRegisteredSubObjectList` wechselt `true` → `false`; `bReplicates` bleibt `false`. |

Beim PostTick-Export enthält nur der Originalsnapshot die generierten Graphen
`ExecuteUbergraph_AC_PostABPTick` und `Tick_MERGED` mit insgesamt sieben
zusätzlichen Objekten sowie deren Referenzen. Der gemeinsame generierte
`ReceiveTick`-Graph besitzt andere Graph-/Pin-GUIDs; nach Zuordnung der GUIDs
zu den Pin-Namen und Sortierung stimmen Eigenschaften und Verbindungen
überein. Diese Export-/Compilerartefakte sind getrennt von der ausdrücklich
erhaltenen CDO-Abweichung dokumentiert; vollständige Binärgleichheit wird
nicht behauptet.

Die minimale Assethülle benötigt zusätzlich
`BP_AnimNotify_FoleyEvent_Roll` unter dem Pilotpfad `Blueprints/AnimNotifies`.
Dies ist eine tatsächliche Abhängigkeit der Originalmontagen und eine der
17 Pilotkopien; vorher fehlte eine entsprechende Foundation-Kopie. Die
frischen Notify-/Referenzbelege unten ersetzen weiterhin keinen Gameplay-
oder Cooked-Abhängigkeitsnachweis.

Für die projektlokalen Notify-Klassen ist eine eng begrenzte editorseitige
`RemapAnimationNotifyClasses`-Operation ergänzt. Sie prüft explizite Klassen-
und Typzuordnungen einschließlich Layout, Flags, Enumwerten und Boolmasken,
bereitet kompatible Instanzen außerhalb des Assets vor und ersetzt nur die
Notify-/NotifyState-Zeiger in bestehenden Eventrecords. Eventreihenfolge,
GUIDs und Timing-/Trackdaten sollen dadurch erhalten bleiben; es gibt kein
implizites Speichern. Der unabhängige Quellreview fand in der korrigierten
Fassung keinen offenen konkreten Befund. Explizite skalare Objektzuordnungen
setzen zusätzlich die projektlokale Foley-Bank unter ihrem passenden
Klassenconstraint; weder CDO-Fallback noch beliebige Containerkonvertierung
werden dafür zugelassen.

`notify-before-{0..7}.json`, `notify-after-{0..7}.json` und
`notify-reloaded-{0..7}.json` belegen nach MCP-Save/Reload vier Montagen mit
je zwei und vier Sequenzen mit je fünf Events: Alle **28 Eventrecords**
behalten GUIDs, Timing, Reihenfolge, Tracks und Filter. Notifywerte bleiben
bis auf die ausdrücklich zugeordneten Klassen-/Instanzpfade und die
Foley-Bank erhalten. Die alten unreferenzierten Originalklasseninstanzen
sind nach Reload entfernt. Native T3D zeigt bei den Sequenzen nur erneuerte
`Signature`-GUIDs von DataModel/MovieScene/Track/Section; Inhaltswerte bleiben
gleich. Die Foley-Operation ersetzt je fünf Instanzen, ein zweiter Aufruf
ersetzt null.

`pilot-closure-after-notifies.json` erfasst **2115 Projektpakete, 81 externe
Grenzen und keine fehlenden Pakete**. In den erfassten Package-/Management-
Kanten verbleibt kein `/Game`-Pfad außerhalb `/Game/SurvivalRpg`. Das ist die
beobachtete Registry-Hülle einschließlich Editor-Referenzen; dynamisch
erzeugte und Cooked-Pfade sowie Gameplay sind damit nicht vollständig geprüft.

## Zuständigkeiten

| Verantwortung | Bestehender beziehungsweise geplanter Owner |
| --- | --- |
| Komposition und konkrete Inhalte | Eigene Experience/PawnData sowie vom Original abgeleitete Blueprint-, PhysicsControl-, Chooser- und Animationsassets. Referenzen und Tuning bleiben designer-owned. |
| Authority und lebender Lifecycle | Konkrete Blueprint-GA mit ServerInitiated/InstancedPerActor/Exclusive_Blocking; native Aktivierungs-/Revisionskorrelation und Mover-Modus Ragdoll. Capsule während Ragdoll verankert, PhysicsControl nur am Mesh. |
| Endgültiger Tod und Respawn | Kanonische Health-/Death-/GameMode-Pfade; kein zweiter Health-, Revive-, Corpse- oder Respawn-Mechanismus. |
| Montagen und Equipment | Bestehende GAS- und Equipment-Owner; rohe Sample-Montagestopps und pauschales Collision-Cleanup sind kein fertiger Projektvertrag. |
| Gameplay-Mesh und optionales Retargeting | UEFN besitzt Gameplay-/Physics-Verbindung; ein Retarget-Follower bleibt kosmetisch ohne eigene Physics-Authority. |
| Sitzung und Dateien | Root besitzt Editor/MCP, Builds, binäre Assets und Mover-Integration; Runtimeagent Lifecycle-Konzeption, Testagent Validierungskonzept, Dokumentationsagent diesen Bericht/Roadmap/Übergabe/Audit-Mergestatus. |

Sample-Shove/Takedown, Mehrcharakter-Interaktionen, Schieß-/Spawn-Demos,
automatisches NPC-Getup und neue Downed-/Revive-Systeme gehören nicht zum
ersten Piloten. Eine ChaosMover-Backendumstellung oder Physiksimulation des
Retarget-Followers ist ebenfalls kein vorausgesetzter Bestandteil.

## Entscheidende Integrationskorrekturen

Die lokale Physics-Konfiguration sichert Collisionprofil, Responses,
CollisionEnabled, PhysicsTransformUpdateMode und Mover-Smoothing. Vor der
Simulation wird das Mesh einmal am vorhandenen Base-Visual-Transform
positioniert; während Ragdoll gelten `QueryAndPhysics`, Block für
`WorldStatic`, `ComponentTransformIsKinematic` und ausgeschaltetes
Mover-Smoothing. `QueryOnly` hatte `ShouldBlendPhysicsBones` unterdrückt,
`RpgPawnMesh` ignorierte den Boden, und Smoothing schrieb in simulierte
Bodies. Die Rückgabe wartet auf tatsächlich kinematische Bodies nach dem
PhysicsControl-Tick und restauriert auch die Component-Physicsflags. Beim
EndPlay ohne weiteren PhysicsControl-Tick werden verbliebene Bodies zuvor
angehalten. Capsule-Authority und andere Kollisionsantworten bleiben erhalten.

Eine gesperrte Bewegung liefert jetzt gültiges `DirectionalIntent` mit
Nullvektor statt `None`; damit erhält der Walking-Modus einen unterstützten
Inputtyp. Bei terminalem Tod stoppt der bestehende ASC auch eine bereits auf
dem Simulated Proxy laufende Getup-Montage. Die drei gezielt beobachteten
Warnklassen – nicht unterstützter MoveInputType, inkompatible Physics-
Simulate-Optionen und Verschieben vollständig simulierter Meshs – treten im
korrigierten Fokuslauf nicht mehr auf. Warnungen werden nicht unterdrückt.

Der Listen-Host-Eingabekonflikt war konkret: `IMC_Combat` belegt **R** für
Weapon03 mit Priorität 1, ebenso zunächst der Pilotkontext. Nur diese
Tastenüberschneidung wurde bestätigt. Die Pilot-Experience verwendet deshalb
Priorität **2** und reserviert dort R für Ragdoll/Getup. Nach dem Fix erreicht
der Host echte Physics und Getup. Im roten Hostfall war die gewählte
Startposition **0,100 s**, die initiale SimProxy-GAS-Montage stand bei
**0,000 s**; die Fixture verlangte mindestens 0,09 s. Sie berücksichtigt nun
die vorhandene GAS-Korrekturschwelle von 0,1 s; die Runtime-Schwelle wird nicht
erhöht. Die korrigierte Fixture ist in Build 11 enthalten und besteht im
finalen Lauf.

## Sichtbarer Pilot und Editorwerkzeuge

Die neue Karte ist `/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMoverRagdoll`.
Im stationären, nicht geduckten Stand startet **R** Ragdoll; nach mindestens
0,35 s fordert ein zweites **R** Getup an. Die neuen Input- und Experience-
Assets kapseln diese Belegung innerhalb des Piloten.

Die manuelle MCP-Prüfung zeigte einen authored Kartenfehler: Der übernommene
Boden war `Movable` mit `BlockAllDynamic`. Nur die neue Pilotkarte wurde
korrigiert: Boden-Root und -Mesh sind nun `Static`, Profil `BlockAll` mit
`WorldStatic`; die drei PlayerStarts wurden von Z=110 auf **88,15 cm** gesetzt.
Nach Save/Reload gelang R direkt nach Spawn mit realem Ragdoll. Der frühe
Beleg `manual-map-fixed-ragdoll.png` zeigt den Körper nur am unteren Bildrand;
die abschließende Prüfung verwendet deshalb zusätzlich Frontansichten.

Root prüfte im frisch gestarteten Editor 9 (PID 17044) mit finalen Binaries
die echte Eingabefolge über `manual-final-sequence.py` und `manual-front.py`:
**R → am Boden liegend → zweites R → Getup → stehende Rückkehr → W → LMB**.
Die `manual-final-*-front.png`-Bilder zeigen den ganzen Körper ohne die
HUD-Verdeckung der Rückansicht; weitere Bilder erfassen Getup, Bewegung und
Waffenangriff mit erhaltenem Attachment. Das ist eine ausgeführte Sichtprüfung,
keine Behauptung perfekter Übergangsglätte anhand exakter Frames. Audio wurde
mit `-NoSound` nicht geprüft.

Zum Nachstellen: Pilotkarte öffnen und **Play** starten; im Stand **R**,
etwa eine Sekunde warten, erneut **R**, anschließend **WASD** und **LMB**.
Mit der Maus etwas nach unten sehen hilft, da die Kamera an der gehaltenen
Capsule bleibt. Der Pilot wird als Draft-PR zur einfachen manuellen
Sichtprüfung bereitgestellt.

Ein editorseitiger `PlaytestTools`-Inputbridge leitet den Eingabedruck über
`PlayerController::InputKey` in PIE weiter. Ein Slate-Key-Down und -Up im
selben Frame hatte die Enhanced-Input-Flanke verloren. Zusätzlich erlaubt
MCP `set_pie_view_rotation` die Kamerablickrichtung für die Sichtprüfung;
es verändert keine Pawn-/Physics-Transforms. Die Gameplay-Eingabe und die
Sichtprüfung bleiben von Runtime-Autorität getrennt.

## Validierungsstand

Alle lokalen Belege liegen unter `Saved/GaspRagdollPilot20260926`. Ignorierte
`Saved/`-Dateien sind in anderen Checkouts nicht automatisch vorhanden.

| Prüfung | Tatsächlich belegt / offen |
| --- | --- |
| Editor | Finaler Build 12 **Succeeded, 11,21 s**; Builds 10/11 zuvor bestanden (12,38 / 20,20 s). |
| Game | Game-Build **Succeeded, 68,64 s**; finaler inkrementeller Build einschließlich WorldStatic-Guard **Succeeded, 22,82 s**. |
| Korrigierter Fokus | `pie-retry-attempt2-recovered.json`: Remote/Late Join **Success, 7,479343 s**, Getup-Tod **Success, 24,905853 s**; null Fehler in diesen Fällen und null der drei gezielten Warnklassen. Der Host-Teil dieses Versuchs bleibt rot. |
| Sechsteiliger Stand | `pie-full-current.json`: **5/6 Success**, Host-Fall nach tatsächlicher Physics/Getup wegen Startpositionsannahme rot. 360 Warnungen und eine Assertion; der Lauf wird nicht nachträglich grün umgedeutet. |
| Finale Regression | `final-tests.json`: **20/20 Success**, null Fehler, null übersprungen, **187,454575 s**. Alle sechs neuen Ragdollfälle einschließlich Listen Host bestanden; Auswahl in `final-selected-names.json`. |
| Assets / Referenzen | Acht Notify-Assets nach Save/Reload geprüft; 28 Eventrecords erhalten. Registry 2115 Pakete / 81 externe Grenzen, keine fehlenden Pakete oder rohen Sample-`/Game`-Referenzen. Dynamische/Cooked-Hülle unbewiesen. |
| Sichtprüfung | Direkter Ragdoll-Eintritt, Getup, stehende Rückkehr, Bewegung und Waffenangriff durch Root auf gespeicherter/neu geladener Karte geprüft; keine Audio-/exakte Frameglätteabnahme. |
| Erhaltung / Manifest | Finale Prüfung bestanden: 6938 ursprüngliche Assethashes, 17 Baseline-Map-/Savedateien und alle 25 Manifest-Zielhashes unverändert. |
| Abschlussaudit | Sechs Blueprints mit `warnings_as_errors=true` kompiliert; Experience-Hülle 2115/81, Karten-Hülle 2225/99, jeweils keine fehlenden Pakete oder rohen Sample-`/Game`-Kanten. Editor 17044 ohne dirty Content/Maps sauber beendet; anschließend keine UnrealEditor-/Cmd-Prozesse. Vier Overrides, Python-AST, Projekt-JSON und Diffprüfung bestanden. Unabhängiger Runtime-Review ohne konkreten Blocker. |
| PR / Merge | Validierung abgeschlossen, Commit und Pilot-Draft-PR noch ausstehend. Manuelle Nutzer-Sichtprüfung vorgesehen, kein automatischer Merge. |

Der finale Lauf enthält **1089 Warnungen**: 944 NetPackageMap, 110
VoiceInterface, 17 NetworkPrediction (`RollbackFrame == PendingFrame`),
14 Animation (veraltete PoseAssets), drei RpgCharacter (ungültiges
Retargetprofil wird abgelehnt, Gameplay-Mesh bleibt) und eine Blueprint-
Warnung zum nativen Tick des Respawn-Widgets. Die drei gezielten neuen
Warnklassen MoveInputType, inkompatible Simulate-Optionen und Verschieben
vollständig simulierter Meshs sind im gesamten Testergebnis **null**.
Die übrigen Warnungen werden weder als behoben noch als warnungsfreier Lauf
ausgegeben; diese Zahlen beziehen sich auf die Testaufzeichnungen.

Nach den finalen Builds wurden ausschließlich erläuternde API-Kommentare
in Task-/State-Headern ergänzt; keine Runtimefunktion oder Assetdatei änderte
sich danach.

Finale Auditbelege: `final-blueprint-compiles.json`,
`final-closure-experience.json`, `final-closure-map.json`,
`final-assets-before-preserved.json`, `final-preservation-before-preserved.json`,
`final-target-hashes.json`, `final-editor-after.json` und `final-editor-close.json`. Die Registry-Hüllen
bleiben einschließlich Editor-Referenzen erfasst; ein vollständiger Cook-
oder dynamischer Ladepfadnachweis wird daraus nicht abgeleitet.

Frühere Fehler sind weiter in den Build- und `pie-attempt*`-/
`pie-remaining-attempt1.json`-Belegen erhalten. Dazu gehören fehlende
Editor-Linkabhängigkeit/Notify-API-Korrektur, unpassender Fixture-Start ohne
Bodenkontakt, die Physics-/Smoothing-Probleme und der Proxy-Getup-Tod.
`pie-attempt5.json` war bereits ein grüner Remote-Lauf (27,146833 s), enthielt
aber noch 16842 Warnungen; das ist ein historischer Zwischenstand vor den
gezielten Warnkorrekturen.

Eine Modusfolge oder bestandene Fixture allein belegt keine vollständige
Bonepose-/Root-Kontinuität oder Parität aller Netzwerkbedingungen. Die
stehenden, ebenen Pilotbedingungen, fehlende kontrollierte Rollkraft und
unveränderte terminale Health-/Death-/Respawn-Ownership bleiben Grenzen.
Die 115 Hashprüfungen aus PR #151 sind historische Source-Audit-Ergebnisse.
VAL-01/02, die NET-03-Rekonstruktionsgrenze und die ältere Pending-Fixture-
Timingempfindlichkeit bleiben eigenständige offene Folgepunkte.
