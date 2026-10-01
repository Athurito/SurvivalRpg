# GASP-04 – Gemeinsamer Variantenvergleich

Stand: **27.09.2026, [PR #154](https://github.com/Athurito/SurvivalRpg/pull/154) bestätigt gemergt**.
Merge am 26.09.2026 um 23:49:16 UTC als `bb690eefc111bd22f9267b78c07e01e7937fc499`,
finaler geprüfter PR-Head `d3240b4ba59e4ebb3a5b2748b8a9293f0957b3ed`. Die nachfolgende
Merge-Statuspflege führt keine neuen Builds oder Tests aus.
Testcode `b96083570d463547aa8f23b93445619010999269`, Branch `codex/gasp-04-variant-comparison`,
Basis `4c23134f320eb1b94d6bd2e7adaee2dfebbe0e33`. PR #152 brachte den
Ragdoll-Piloten einschließlich grüner Farbe und MetaSound-Korrektur; PR #153
führte den bestätigten Merge nach. Vorherige Testergebnisse gelten hier als
historische Belege, bis die zugehörige aktuelle Prüfung tatsächlich lief.

## Umfang und Zuständigkeit

CMC, Mover und Mover-Ragdoll werden mit UEFN sowie einem gezielt konfigurierten
kompatiblen Retarget-Profil verglichen. Unterschiede sind zulässig und werden
erklärt. Bestehende GASP-02-Befunde bleiben sichtbar; Importbereinigung ist GASP-05.

Gameplay-Wahrheit bleibt in den vorhandenen CMC-/Mover-, GAS-, Equipment- und
Health/Lifecycle-Komponenten. Experience/PawnData komponieren die Varianten.
Blueprints, Chooser und Profile besitzen konkrete Animation, Darstellung und
Tuning. UEFN bleibt Gameplay-/Physics-/Montage-/Notify-Quelle; ein Retarget-Mesh
ist Darstellung. Editor-/MCP-Werkzeuge prüfen und konfigurieren Assets;
native Automation prüft wiederverwendbare Netzwerk-/Lifecycle-Verträge.

## Unterschiede und Nachstellen

Alle drei Testkarten nutzen die vorhandene projektlokale GASP-Darstellung.
`Lvl_RpgGaspMantle` ist hier die vollständige CMC-Traversal-Komposition;
`Lvl_RpgGaspCMC` bleibt die einfache Locomotion-Karte. Die Ragdoll-Karte ist
eine freie Ebene. Ihre geerbte Traversal wird auf `Lvl_RpgGaspMover` mit
expliziter Ragdoll-Experience geprüft; die native Fixture restauriert den
vorherigen Experience-Override nach jedem Test.

| Variante / Karte | Eingabe und bewusster Umfang |
| --- | --- |
| CMC: `Lvl_RpgGaspMantle` | WASD, Maus, Strg Hocke umschalten, Space Sprung/Traversal, LMB Angriff. Bestehendes Run-Preset; kein separater Tastatur-Walk-/Sprint-Input. |
| Mover: `Lvl_RpgGaspMover` | WASD, Maus, Strg Walk, Shift Sprint, C ducken, Space Sprung/Traversal, LMB Angriff, RMB Block. |
| Mover-Ragdoll: `Lvl_RpgGaspMoverRagdoll` | Mover-Belegung; zusätzlich auf freier Fläche stillstehen, R, etwa 1 s warten, R, anschließend WASD/LMB. Eintritt nur lebend, ungeduckt und nahezu stationär. |

CMC und Mover müssen in diesem Vergleich keine identischen Gaits erhalten.
Eine spätere Vereinheitlichung der Eingabe und belastungsabhängige Profile
benötigen einen eigenen kleinen Auftrag. Das lebende Ragdoll hält seine
Capsule am autoritativen Anker und simuliert UEFN lokal; es ist kein frei
rollender oder netzwerkidentischer Physikkörper.

Die vorhandenen CMC-/Mover-Retarget-Profile lassen TargetMesh und Retargeter
standardmäßig leer; die Ragdoll-PawnData besitzt gar kein festes Profil.
Die Prüfungen komponieren ausschließlich für ihre Sitzung das projektlokale
`SKM_Manny`, `RTG_UEFN_to_UE5_Mannequin` und
`ABP_RpgGasp_RuntimeRetarget`. Danach gilt exakt die vorherige Konfiguration.
Manny ist damit keine Auswahl des finalen Characters.

## Abnahmematrix

| Bereich | CMC | Mover | Mover-Ragdoll |
| --- | --- | --- | --- |
| Bewegung, Hocke, Sprung | Basis-PIE bestanden; echte Eingabe in Traversal-Karte zusätzlich gesichtet | Host-/Remote-Eingabe und Late Join bestanden, zusätzlich gesichtet | Bewegung nach Getup automatisiert; Hocke/Sprung zusätzlich gesichtet |
| Gaits | Bestehendes Run-Preset, kein separater Walk-/Sprint-Key | Walk-/Sprint-Eingabe und Darstellung gesichtet; keine vollständige Gait-Wechselmatrix | Geerbte Mover-Eingabe gesichtet; gleiche Grenze |
| Mantle/Vault/Hurdle | Drei Run-Fälle auf gespeicherter Karte bestanden | Drei Remote-Run-Fälle bestanden | Drei neue Remote-Run-Fälle mit tatsächlicher Ragdoll-Komposition bestanden |
| Equipment, Combat-Montagen | Netzwerk- und Follower-Vertrag bestanden | Angriff, Block, Abbruch und Follower bestanden | Angriff/Equipment nach Getup einschließlich Follower bestanden |
| Tod, Respawn, Remote/Late Join | Basis-CMC-Lifecycle bestanden | Vier Lifecycle-Fälle einschließlich Follower-Respawn bestanden | Tod während Ragdoll/Getup, Reentry und Late Join mit UEFN bestanden |
| Lebendes Ragdoll/Getup | Nicht Bestandteil dieser Variante | Nicht Bestandteil dieser Variante | Host, Remote, Late Join und optionaler Follower bestanden |
| UEFN und kompatibles Retarget-Profil | Follower: Pose, Late Join, Mantle, Equipment und Fallback bestanden | Follower: Gameplay, Equipment, Angriff und Lifecycle bestanden | Neuer Follower: physische Quelle, Ragdoll, Getup, Late Join, anschließende Bewegung/Angriff bestanden |
| Asset-/Abhängigkeitsprüfung | 3 relevante Blueprints/AnimBPs; Registry-Hüllen ohne fehlende/rohe Sample-Pakete | 3 relevante Blueprints/AnimBPs; gleiche Registry-Prüfung | 2 relevante Blueprints; gleicher Registry-Vertrag; gemeinsamer Retarget-AnimBP zusätzlich geprüft |
| Aktuelle Sichtprüfung / Nutzerabnahme | Root-Sichtprobe bestanden; keine neue Nutzerabnahme für GASP-04 | Root-Sichtprobe bestanden; frühere GASP-01-Nutzerabnahme bleibt historisch | Root-Sichtprobe bestanden; „passt“ aus GASP-03 bleibt historische Nutzerabnahme |

Die CMC-Ergebnisse verteilen sich bewusst auf Basis-CMC-Integration und
Mantle-/Traversal-/Retarget-Komposition. Retarget ist eine gezielte Auswahl,
kein vollständiges Kreuzprodukt: CMC-Follower prüft Mantle, die drei neuen
Ragdoll-Traversal-Fälle verwenden UEFN. Kein neuer Ragdoll-Follower-Test für
Tod/Respawn, Reentry oder Traversal und kein unmittelbarer Übergang zwischen
aktivem Ragdoll/Getup und Traversal. Die vier verglichenen lokalen
Knochenrotationen belegen Poseaktivität, keine quantitative physische
Source-/Target-Poseparität oder synchronisierte Netzwerk-Boneposes.

## Prüfbelege und Grenzen

Versionierte Zusammenfassung einschließlich aller 31 Testnamen und Ergebnisse:
[Prüfmanifest](assets/gasp-variant-comparison.json). Lokale Rohbelege liegen unter
`Saved/GaspVariantComparison20260927`; die ignorierten Dateien stehen anderen
Checkouts nicht automatisch zur Verfügung.

- UE 5.8.2: Editor-Build **77,46 s**, Game-Build **66,77 s**, beide tatsächlich
  erfolgreich. Vier generierte Plugin-Overrides vor und nach der Arbeit verifiziert.
- Ein gemeinsamer frischer Lauf: **31/31 Success, 0 Fehler, 291,05 s**;
  27 bestehende und vier neue Fälle. Keine volle GASP-Gesamtsuite behauptet.
  Die exakten Manifest-Testnamen lassen sich über den Automation-Test-Dialog
  oder `AutomationTestToolset.RunTests` erneut ausführen.
- **1.361 Testwarnungen**: NetPackageMap 1160, Voice 160, NetworkPrediction 18,
  Animation 14, RpgCharacter 7, Blueprint 1, PoseSearch 1. Beispiele stehen im
  Manifest: temporäre PIE-Level-NetGUIDs, fehlende Voice-Schnittstelle,
  Rollback/PendingFrame, veraltete Manny-PoseAssets, absichtlich ungültige
  Retarget-Fallbackprofile, Respawn-Widget-Tick und noch laufender PoseSearch-
  Indexaufbau. Diese Warnungen sind nicht pauschal behoben. Zwei bereits aus
  früheren Sitzungen bekannte Startup-`Condition failed` stehen außerhalb des
  Testlaufs im Log; keine MetaSound-Warnung, -Fehler oder doppelte Registrierung.
- Neun relevante Blueprints/AnimBPs mit `warnings_as_errors` kompiliert.
  Alle sechs Registry-Hüllen enthalten null fehlende Pakete und null
  `/Game`-Pakete außerhalb `/Game/SurvivalRpg`: CMC Map/Experience **1536/1479**,
  Mover **2209/2098**, Ragdoll **2226/2116** Pakete. Editor-only/harte/weiche und
  Management-Kanten sind enthalten; dynamische/Cooked-Verweise sind nicht bewiesen.
- Root sichtete aktuelle gerenderte Standalone-PIE-Bilder nach echter
  Controller-Eingabe: Stand/Bewegung, Hocke, Sprung und Angriff aller drei
  UEFN-Varianten, zusätzlich Ragdoll/Getup/stehende Rückkehr. Braun/blau/grün
  und Equipment entsprechen dem bisherigen Stand. Keine neue menschliche
  GASP-04-Abnahme, framegenaue Glätte-, Retarget-Grip- oder Klangqualitätsabnahme.
  Der erste CMC-Lauf verwendete irrtümlich Mover-Gait-Bezeichnungen; Beleg bleibt
  erhalten, Strg wurde als Hocke korrigiert und separat erneut gesichtet.
- Finale **6964/6964** getrackte Assets/Maps und **7/7** bestehende SaveGames
  SHA-256-identisch; keine hinzugekommenen oder entfernten Dateien dieser Arten.
  PIE beendet, Playsettings restauriert, keine dirty Pakete; Editor sauber beendet.
  Runtime und gespeicherte Content-Assets wurden in GASP-04 nicht verändert.
- Gegenseitiger Code-Review der neuen Follower- und Traversal-Fälle sowie
  Abgleich der Dokumentationsgrenzen ohne konkreten Blocker.

Offen bleiben GASP-VAL-01/02, die dokumentierte NET-03-Rekonstruktionsgrenze
und die frühere Timingempfindlichkeit der Pending-Traversal-Fixture. Keine
Cooked-/WAN-/Klangqualitäts- oder netzwerkidentische Bonepose-Abnahme wird aus
lokalen PIE-Ergebnissen abgeleitet.

Der technische Variantenvergleich ist damit abgeschlossen. Die zusätzliche
GASP-04-Nutzer-Sichtabnahme ist nicht erfolgt und wird nicht aus früheren
Freigaben abgeleitet. Der Merge betrifft Tests und Dokumentation gemäß der
stehenden Freigabe für technische Schritte. Als nächster begrenzter Auftrag
kann GASP-05 eine konkrete Import-Entfernungsliste aus den Abhängigkeiten
ableiten; diese Matrix allein autorisiert keine pauschale Asset-Löschung.

## Roadmap-Auftrag

Aus der Roadmap hierher verschoben; Auftrag, an dem dieser Vergleich gemessen
wurde.

Eine gemeinsame Matrix für CMC, Mover und Mover-Ragdoll führen: Bewegung,
vorhandene Gaits, kontextabhängiger Sprung, Mantle/Vault/Hurdle soweit pro Variante
unterstützt, Equipment/Combat-Montagen, Tod/Respawn und optionales Retargeting.
Unterschiede explizit erklären statt automatisch Gleichheit aller Features zu
erzwingen. Erst hier verbleibende CMC-/Mover-Gait- oder Komfortunterschiede für
einen eigenen kleinen Auftrag bewerten.

UEFN-Standard und mindestens ein gezielt konfiguriertes kompatibles Retarget-
Profil prüfen; daraus folgt keine Entscheidung für Manny als finalen Character.
Gameplay-Mesh, Notifies, Root Motion und Equipment-Sockets bleiben eindeutig
zugeordnet. Build-/Asset-/Multiplayer-Ergebnisse und Nutzerabnahme pro Variante
festhalten; offene Punkte aus `GASP-02` mitführen.
