# GASP-06 – Blocken beim Laufen

**Historischer Erststand vom 27.09.2026.** Die Nutzer-Sichtabnahme dieses Stands
ist fehlgeschlagen. Der aktuelle Vertrag und seine neuen Prüfungen stehen in
[gasp-block-refinement.md](gasp-block-refinement.md).
[Draft-PR #160](https://github.com/Athurito/SurvivalRpg/pull/160) bleibt offen;
die folgenden Ergebnisse sind keine Abnahme der Nachbesserung.
Branch `codex/gasp-06-moving-block`, Basis `908b7c7544a42848afe6c8b673988735ebd4ba1b`.
Implementierung `71602034e5eee0d9d0a172b483fe4a5cdd7a2ec6`: sechs Assets, zwei kleine native
Slot-Gates-Erweiterungen und zwei neue Netzwerktests. Editor/Game-Builds,
20/20 gezielte Tests, Root-Sichtprüfung und Windows-Cook bestanden.
[Versionierte Ergebnisse und Assethashes](assets/gasp-moving-block.json).

## Verhalten und Zuständigkeit

Gehaltenes RMB stellt die Blockpose im Oberkörper dar, während WASD weiter
die Beinanimation antreibt. Start, Loop, Trefferreaktion und Ende verwenden
die vorhandenen Equipment-Blockmontagen. GAS, Equipment und CMC/Mover behalten
Aktivierung, Replikation, Bewegung und Cleanup; die Körperaufteilung bleibt
in den designerseitigen AnimGraphs. `State.Blocking` steuert keine nachgelagerte
Posemaske. Es gibt keine neue Ability, kein neues Skeleton und keine Änderung
an Stamina, Bewegungstempo oder Blockregeln.

Die drei bestehenden Montageausschlüsse in `RpgPawnGameplayComponent` und
`RpgGameplayAbility_Mantle` berücksichtigen jetzt `UpperBody` zusätzlich zu
`DefaultSlot`. Damit umgeht der neue Slot nicht die bisherige
Sprung-/Traversal-Sperre während einer Combat-Montage.

## Assetvertrag

Geändert sind sechs Assets:

- `/Game/SurvivalRpg/Characters/GASP/CMC/RPG/ABP_RpgGasp_CMC`
- `/Game/SurvivalRpg/Characters/GASP/Mover/RPG/ABP_RpgGasp_Mover`
- `Block_Start_Seq_Montage`, `Block_Loop_Seq_Montage`,
  `Block_End_Seq_Montage` und `Block_Hit_Seq_Montage` unter
  `/GF_Combat_Core/Animations/Sword_and_Shield/Animations/Sequence2/08_Hit/12_Block/`.

Beide AnimGraphs ergänzen denselben Aufbau unmittelbar vor dem vorhandenen
`DefaultSlot`: Die bisherige Quellpose wird als `RpgLocomotionBeforeMontages`
gespeichert; zwei Cache-Leser speisen die Basis und den neuen `UpperBody`-Slot.
Ein `Layered Blend per Bone` mischt dessen Pose ab `spine_02`, BlendDepth **3**,
mit Gewicht **1**. `UpperBody.bAlwaysUpdateSourcePose=true` hält die zugrunde
liegende Locomotion aktiv. `CurveBlendOption=UseBasePose` erhält die Kurven
der Locomotion. Der nachfolgende `DefaultSlot` kann weiterhin den ganzen
Körper übernehmen; bestehende prozedurale Folgeschritte bleiben erhalten.

Der unabhängige Vergleich der gespeicherten und neu geladenen T3D-Exporte
bestätigt je **fünf neue Nodes**: CMC 31→36, Mover 28→33. Vorhandene AnimNode-
Konfigurationen bleiben gleich; operative Pinänderungen bestehender AnimNodes
beschränken sich auf Quellpose→neuen Cache und Blend-Ergebnis→`DefaultSlot`.
Der vorhandene Mover-Cache `PreRagdoll` und sein `NoRagdoll`-Verweis bleiben
unverändert. CMC erhält zusätzlich die benötigte CachedPose-Compilerextension.
Der Vergleich unterscheidet diese Änderungen von generierten Exportobjekten
und neu aufgebauten Editor-Metadaten; die AnimBlueprints sind nicht byteidentisch.

Bei jeder der vier Montagen ändert sich im vollständigen T3D genau eine
Zeile: `SlotAnimTracks(0)` erhält `SlotName="UpperBody"`. Clips, Segmentzeiten,
Sections, Notifies, Blendwerte und Skeleton bleiben gleich. Die Clips haben
weiterhin `bEnableRootMotion=false`. Eine Oberkörpermaske wäre für sich allein
keine Sperre gegen GAS-/Montage-Root-Motion. Die geteilten Montagen werden
auch von der Baseline verwendet, deren bestehender AnimGraph bereits einen
`UpperBody`-Slot mit `spine_02`/Depth 3 enthält.

### Bewusste Grenze der Slotgruppen

Die vier Montagen verwenden Standard-Blending und ihr `SK_Mannequin` ordnet
`DefaultSlot` sowie `UpperBody` der `DefaultGroup` zu. UE 5.8 bestimmt die
Montagegruppe zum gegenseitigen Stoppen aus dem **Montage-Skeleton**; das
AnimGraph-Mischen erfolgt nach Slotnamen. Das UEFN-Skeleton behält seinen
ursprünglichen Vertrag `DefaultGroup/DefaultSlot` und `Partials/UpperBody`
unverändert. Eine künftige Umstellung auf Montage-Inertialization benötigt
eine erneute Prüfung: Deren Request-Gruppe und die Gruppenauflösung am
Gameplay-Skeleton müssen zusammenpassen. Dieser Schritt verändert deshalb
weder Skeleton-Gruppen noch den Blendmodus vorsorglich.

## Belegte Prüfungen

Lokale Rohbelege liegen unter `Saved/GaspBlockLocomotion20260927/`; die T3D-
Exporte unter `IsolatedUser/Saved/Gasp06/`. Diese ignorierten Dateien sind
in anderen Checkouts neu zu erzeugen.

| Prüfung | Tatsächliches Ergebnis |
| --- | --- |
| Editor/Game-Build | Erfolgreich, 67,58 s / 62,29 s; Editor nach Fixture-Korrektur erneut erfolgreich in 11,57 s |
| Ausgangszustand vor Assetänderung | `negative-results.json`: 0/2, genau acht Beinanimations-Assertions; je Variante vier Fehler in den beiden Rollen und Messfenstern unter gehaltenem Block |
| Finaler Fokuslauf | `positive-final-results.json`: **2/2**, 38,98 s, null Fehler; CMC 19,36 s, Mover 19,62 s |
| Unabhängiger Assetreview | Sechs T3D-Verträge ohne konkreten Blocker, begrenzt auf den oben beschriebenen Graph-/Montagevertrag |
| Gemeinsame Regression | `regression-results.json`: **14/14**, 41,04 s; Block-Lifecycle, CMC/Mover-Angriff und Root Motion, Tod/Respawn/Follower, CMC-/Mover-Mantle |
| Composition / Ragdoll | `contracts-results.json`: **4/4**, 7,57 s; drei Assetverträge und optionaler Follower mit Physik/Getup/Late Join |
| Sichtprüfung | Tatsächliche PIE-Eingabe in CMC/Mover; `cmc-final-*` und `mover-*` zeigen Block, zwei unterschiedliche Gehposen, Stillstand, Release und erneuten Block, jeweils mit passendem `State.Blocking`-Nachweis |
| Windows-Cook | Fünf Karten, Exit **0**, 151,64 s; 3132 erfasste Pakete, 3125 gekocht und sieben platformbedingt übersprungen, keine inkrementellen Überspringungen; fünf Map-Ausgaben vorhanden |
| Erhaltung | Genau sechs geplante Assetänderungen, **4656 Assets/Maps und sieben persönliche Saves byteidentisch**; vier lokale Plugin-Overrides verifiziert |

Der Fokuslauf enthält **136 Warnungen**: 12 Voice-Interface-, 122
NetPackageMap-, eine PoseSearch- und eine NetworkPrediction-Warnung.
Er wird nicht als warnungsfrei beschrieben. Die Negativmessung enthält
zusätzlich Zen-/PoseSearch-/NP-Befunde und bleibt vollständig erhalten.
Über alle drei finalen Testgruppen: **20/20**, null Testfehler, **515 Warnungen**
(52 Voice, 443 NetPackageMap, eine PoseSearch, vier NetworkPrediction,
eine Respawn-Widget-Tick-Warnung und 14 veraltete Manny-PoseAsset-Meldungen).
Die betroffenen PoseAssets und das Respawn-Widget wurden nicht geändert.
Im frischen Editorstart stehen zudem zwei bereits dokumentierte
`Condition failed`-Meldungen außerhalb der Tests; keine MetaSound-Warnung/-Fehler.
Der Cook endet mit null Fehlern und fünf Warnungen: Zen-Start/Verbindung,
GameplayCue-Pfadsuche, MCP-Hinweis und Respawn-Widget-Tick. Er wird ebenfalls
nicht als warnungsfrei dargestellt.

Die neuen Fälle heißen
`SurvivalRpg.GASP.MovingBlock.GaspMovingBlockPIE.CMCMovingBlockKeepsLegsAnimatedThroughLateJoinAndRelease`
und entsprechend `MoverMovingBlockKeepsLegsAnimatedThroughLateJoinAndRelease`.
Sie verwenden echte Bewegung und RMB-Eingabe, erfassen abgeschlossene lokale
Knochenposen und verlangen für **beide Beine in zwei getrennten Fenstern**
mehr als 0,15 rad Bewegung. Montage-Einblendung allein reicht nicht: Die
Messung beginnt nach 0,35 s stabiler Phase, jedes Fenster umfasst mindestens
0,65 s und sechs Samples. Authority, Owner und später beitretender Observer
müssen denselben gehaltenen Block über Bewegung, Stillstand, erneute Bewegung
und normales Loslassen zeigen. Geprüft werden außerdem wirksames Slotgewicht,
Oberkörperpose, erhaltene Gameplay-Mesh-/ASC-/Equipment-Zuordnung, ausbleibende
Root-Motion-Übernahme und das reale Ende der Ability/Montage.

Die Beinmessung der Negativkontrolle ist unverändert; erst danach wurde die
Fokus-Isolierung der Testfixture ergänzt. Frühere Positivversuche bleiben rot dokumentiert. Der PIE-Fokuswechsel beim
Late Join hatte gehaltene Eingaben freigegeben; die Fixture erhält dafür
testlokal die Eingabe und restauriert anschließend die ursprüngliche Policy.
Ein vorheriger Mover-Stillstand bereits ohne Block trat im frischen Editor
mit denselben Assets nicht auf. Weder ein gespeicherter Graphfehler noch
Reload als alleinige Ursache ist damit bewiesen. Diese Diagnose wird nicht
als zusätzlicher Runtimefix ausgegeben.

## Manuell nachstellen und verbleibende Grenzen

`/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMantle` für CMC und
`/Game/SurvivalRpg/Maps/Test/Lvl_RpgGaspMover` für Mover öffnen und Play starten.
Mit der vorhandenen blockfähigen Ausrüstung RMB halten, mit WASD vorwärts,
seitwärts und rückwärts gehen, bei gehaltenem RMB stehenbleiben und wieder
losgehen. Danach RMB loslassen und normal weiterlaufen; LMB prüft anschließend
die Rückkehr zum Angriff. Sichtbar erwartet werden die Oberkörper-Blockpose,
laufende Beine und der reguläre Übergang aus dem Block.

Die Posefenster beweisen keine exakte Bildkontinuität, keinen vollständigen
Gait-/Retarget-/Ragdoll-Vergleich und keine allgemeine Netzwerktoleranz.
Die offenen `GASP-VAL-01/02`- und `GASP-NET-03`-Grenzen bleiben bestehen.
Eine neue Nutzer-Sichtabnahme ist noch nicht erfolgt. Die Root-Sichtprüfung
deckt den gehaltenen Schildblock und seine Übergänge ab, keine neu ausgelöste
Block-Hit-Reaktion oder vollständige Gait-/Retarget-Matrix. Der Editor wurde
nach restaurierten Playsettings und beendetem PIE sauber geschlossen.
