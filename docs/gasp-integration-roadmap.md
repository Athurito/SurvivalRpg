# GASP-Integration: Roadmap und Einstieg für neue Chats

Stand: **28.09.2026**. Diese Datei ist der zentrale Arbeitsplan für die weitere
GASP-/Lyra-Integration. Sie ersetzt nicht die [Spielvision](game-vision.md) oder
die technischen Einzelberichte. Der Umfang ist Bewegung und ihre RPG-Anbindung,
nicht eine neue Gesamtplanung für Combat, Crafting oder Portale.

## Schnellstart

- **GASP-06: Neuaufbau geprüft, Nutzer-Sichtprobe offen.** Rückbau auf `908b7c75` im eigenen Commit `de852e98`; gemeinsamer Linked Anim Layer mit zwei Datenkindern und Chooser Tables, abgeleiteter Sword-and-Shield-Satz mit 157 cm/s sowie CMC-/Mover-Vorhersage integriert. Stopps übernehmen die Kontaktphase aus der gemeinsamen Sync-Gruppe. Editor/Game, 19/19 Fokus- und 25/25 Regressionstests sowie eigene Turn-/Richtungs-/Stopaufnahmen bestanden; Fünf-Karten-Cook und eigene Reaktionssichtprobe ebenfalls erfolgreich. Frühere GASP-06-Ergebnisse gelten ausdrücklich nicht als Abnahme. [Draft-PR #160](https://github.com/Athurito/SurvivalRpg/pull/160) bleibt bis zur neuen Nutzer-Sichtprobe offen. [Neuaufbau und Prüfstand](gasp-block-locomotion.md), [Übergabe](gasp-integration-handoff.md).

- **Gemergt: GASP-05 – Originalimport entfernt.** [PR #158](https://github.com/Athurito/SurvivalRpg/pull/158), bestätigt gemergt als `051f03f06abf84b82191338a52401dd64ddd6c32` am 2026-09-27T01:16:35Z; finaler geprüfter Head `594b202084737c6e98ad3cc853d3cc613504dcc5`. Branch `codex/gasp-05-remove-sample-actors`, Assetcommit `0c37d9c4b9fb5cb555569c81ea5b10aedaefa4d6`, Basis `d6cb927f`. Vier alte Sample-Akteure und exakt 2298 historische Originalassets entfernt; 4662 übrige Assets einschließlich aller 2156 übernommenen Ziele byteidentisch, sieben persönliche SaveGames erhalten. Frischer Graph, zwei Karten-Smokes, 9/9 Tests, neun BP-/AnimBP-Compiles und voller Windows-Cook (3132 Pakete, fünf Karten, Exit 0) bestanden. [Entfernung und Grenzen](gasp-original-import-removal.md), [Manifest](assets/gasp-original-import-removal.json). Bestätigter Merge: `051f03f06abf84b82191338a52401dd64ddd6c32`.
- **Gemergter Teilschritt: GASP-05 – Import-Abhängigkeits-/Entfernungslisten-Audit.** [PR #156](https://github.com/Athurito/SurvivalRpg/pull/156) am 27.09.2026 um 00:29:00 UTC bestätigt gemergt als `fdff8b91341c0c130db8487cea62f5c50f86bfd8`, geprüfter Head `6c89b9f18308abcc6cbf83a7973899cd351087d4`, Werkzeugcommit `9c8d7c77`. Arbeitsbranch `codex/gasp-05-import-audit`, Basis `577d7606`. Damals waren alle 2156 zugeordneten Originalpakete über jede der zwei alten Testkarten erreichbar; vier gespeicherte Sample-Akteure waren die direkten Projektblocker. Entfernungsliste leer, 17/17 Planertests bestanden, 6964 Assets/Maps und sieben SaveGames unverändert. Für diese Merge-Statuspflege keine Neuläufe. Der damalige Folgeauftrag zur Entkopplung und Entfernung ist inzwischen ausgeführt; aktueller Stand im Eintrag darüber. [Auditbericht](gasp-import-cleanup-audit.md), [Manifest](assets/gasp-import-cleanup-audit.json).
- **Gemergt: GASP-04 – Vergleich von CMC, Mover und Mover-Ragdoll.** [PR #154](https://github.com/Athurito/SurvivalRpg/pull/154) am 26.09.2026 um 23:49:16 UTC bestätigt gemergt als `bb690eefc111bd22f9267b78c07e01e7937fc499`, geprüfter Head `d3240b4ba59e4ebb3a5b2748b8a9293f0957b3ed`, Testcode `b9608357`. Editor/Game, **31/31 Tests**, neun Blueprint-/AnimBP-Compiles, sechs Registry-Hüllen und Root-Sichtprüfung bestanden; 1361 Warnungen und Grenzen dokumentiert. Keine neue GASP-04-Nutzerabnahme; Runtime/Assets unverändert. Der aktuelle GASP-05-Audit steht im Eintrag darüber. [Vergleich](gasp-variant-comparison.md), [Manifest](assets/gasp-variant-comparison.json). Für diese Merge-Statuspflege keine Neuläufe.
- Folgefix in PR #152: Nach gemeldeter doppelter RunStrafe-MetaSound-GUID sind 18 Shared/Foley-Klassen neu identifiziert und 19 Graphen angepasst; 37 eindeutige Klassen-IDs, 19/19 semantische Reviews und Audio-PIE-Smoke bestätigt, keine doppelten Klassen oder MetaSound-Warnungen/-Fehler im frischen Log. Frühere vollständige Erhaltungszahlen gelten vor diesem gezielten Assetfix; kein neuer Build/20-Test-Lauf oder Klangqualitätsnachweis. Details einschließlich Editor-Regleränderungen und zweier bestehender Startup-Fehler im [Manifest](assets/gasp-metasound-identity-fix.json).
- **Gemergt: `GASP-03` – begrenzter spielbarer Ragdoll-/Getup-Pilot.** [PR #152](https://github.com/Athurito/SurvivalRpg/pull/152) am 26.09.2026 um 20:51:05 UTC bestätigt gemergt als `9de031c9e255a13d117662713739e44984a98edc`, finaler geprüfter Head `7ab180a85290c78474a903ce7683901e3b1c2dcd`. Historisch: Editor/Game, 20/20 Regressionen, Root-Sichtprüfung und Abschlussaudit bestanden; 1089 Warnungen dokumentiert, älterer Lauf bleibt 5/6. Nutzer bestätigt „passt“; grüne Farbe und MetaSound-Folgefix sind enthalten. Builds/20 Tests liefen vor den Assetnachbesserungen, MetaSound-Review 19/19 danach; für diese Merge-Statuspflege keine Neuläufe. Der aktuelle GASP-04-Vergleich steht im Eintrag darüber. [Pilotbericht](gasp-mover-ragdoll-pilot.md), [Manifest](assets/gasp-ragdoll-pilot.json).
- **Source-Audit gemergt: `GASP-03`.** Auditcommit `1d89072c`, [PR #151](https://github.com/Athurito/SurvivalRpg/pull/151), bestätigt gemergt am 26.09.2026 um 13:40:22 UTC als `9baac5f36fd978b86c364df3e82aa74d76cfb7d2`. [Manifest](assets/gasp-ragdoll-source-audit.json): 44 Kandidaten, keine Import-Whitelist; korrigierte Registry 3017 Pakete, Exportlücken erfasst. Historische 115 Hashprüfungen, 17 erhaltene Map-/Savedateien und vier Overrides; kein Build/PIE oder Runtime-/Assetimport im Audit. Diese Ergebnisse werden nicht als neue Pilotabnahme ausgegeben; [Auditbericht](gasp-mover-ragdoll-source-audit.md).
- **Gemergt: `GASP-02` / `GASP-VAL-03` – Traversal B bei noch gepufferter Traversal A.** Testcommit `8bc887e1`, [PR #150](https://github.com/Athurito/SurvivalRpg/pull/150), bestätigt gemergt am 26.09.2026 um 13:09:25 UTC als `4ed174d2cdc1a2d52fc5a9272ad68437fed98025`. Fokus und 30 FPS bestanden, Negativkontrolle erwartungsgemäß rot. Regression bleibt **15/16** mit 129 Warnungen und einer Assertion; alter Pending-Replay-Fall verfehlt die Overlap-Beobachtung, besteht unverändert isoliert 1/1. Runtime/Assets unverändert. Diese historischen Ergebnisse wurden für die Merge-Statuspflege nicht erneut ausgeführt; [Bericht](gasp-buffered-traversal-replay.md).
- **Gemergt: `GASP-NET-03` – Rekonstruktionsgrenze bewertet und Werkzeug validiert.** Commit `49de7c87`, [PR #149](https://github.com/Athurito/SurvivalRpg/pull/149), bestätigter Merge `d933148f22e8051f0a50ab68719f2f507a6158b5` am 26.09.2026 um 12:27:04 UTC. Getrackter Probe, 19/19 Offline-Tests und vier CLI-Negativprüfungen. Kontrolle 16/16 Ebenen, Pause weiterhin 13/16; bis 64,00 cm Root-Abweichung bei gleicher beobachteter Phase, Freeze korrekt. Runtime-Rekonstruktionsgrenze bleibt bestehen; [Bericht](gasp-packet-gap-recovery.md).
- **Gemergt: `GASP-NET-02` – terminaler Mantle-Grund bei Reconciliation.** Implementierung `8de5d927`, [PR #148](https://github.com/Athurito/SurvivalRpg/pull/148), bestätigter Merge `1490dd7d85306bffa2b2a4b1ab7d9202f1a9b0c4` am 26.09.2026 um 11:56:35 UTC. Identischer Fokusvergleich rot 1/3, grün 3/3; Editor/Game und Regression **28/28** bestanden. Maps/SaveGames unverändert, Overrides verifiziert; 287 Warnungen und offene Befunde dokumentiert. [Ursache und Belege](gasp-terminal-reconciliation.md).
- **Gemergt: `GASP-NET-01` – Rollback während aktivem Mantle.** Commit `b6709eb4`, [PR #147](https://github.com/Athurito/SurvivalRpg/pull/147), bestätigter Merge `44a5e5127bab0d064608b682fcd002adf91c4159` am 26.09.2026 um 10:24:29 UTC. Editor-Build, 10/10 Regressionen, zusätzlicher 30-FPS-Lauf und echte Negativkontrolle; [Nachweis](gasp-active-mantle-rollback.md). Ausschließlich Editor-Testcode und Dokumentation, Runtime unverändert.

- **Gemergt: `GASP-02` / `GASP-STAB-02` – Falling bei belegtem Respawn.** Commit `57fa8de9`, [PR #146](https://github.com/Athurito/SurvivalRpg/pull/146), bestätigter Merge `b9a65360`. `GASP-STAB-01` ist mit [PR #145](https://github.com/Athurito/SurvivalRpg/pull/145) gemergt (`4039be25`). Der anschließende NET-01-Nachweis steht oben; weitere Registerpunkte bleiben offen.
- Aktive Aufgabe, Branch, letzte Ergebnisse und konkrete Fortsetzung stehen in
  [gasp-integration-handoff.md](gasp-integration-handoff.md).
- Letzter akzeptierter Runtime-Ausgangspunkt: [PR #152](https://github.com/Athurito/SurvivalRpg/pull/152),
  Merge `9de031c9`, finaler geprüfter Head `7ab180a8`. GASP-01 bleibt mit PR #144
  und damaliger Sichtabnahme übernommen. Die anschließende Originalasset-
  Bereinigung ist in PR #158 gemergt; alle übernommenen Projektassets sind erhalten.
- Dieser Stand enthält offene Folgearbeiten. „Gemergt“ bedeutet nicht, dass alle
  Netzwerk-/Lifecycle-Randfälle gelöst oder alle Umgebungen getestet sind.
- Vor Arbeit prüfen: aktueller Git-Stand, diese Roadmap, Übergabe und die zum
  Arbeitspaket verlinkten Berichte. Aktueller Code und nachvollziehbare Ergebnisse
  haben Vorrang vor historischen Beschreibungen eines früheren Piloten.

## Bereits vorhanden

| Bereich | Akzeptierter Stand | Technischer Einstieg |
| --- | --- | --- |
| Baseline und Assetbasis | Baseline erhalten; übernommene Inhalte unter `/Game/SurvivalRpg/Characters/GASP`; Quell-Ziel-Zuordnung vorhanden | [Assetbasis](gasp-asset-foundation.md), [Manifest](assets/gasp-asset-map.json) |
| CMC | GASP-Locomotion, Mantle, Vault und Hurdle in bestehenden Experiences | [CMC](gasp-cmc-integration.md), [Mantle](gasp-mantle-integration.md), [Vault](gasp-vault-integration.md), [Hurdle](gasp-hurdle-integration.md) |
| Runtime-Retargeting | Optionales Profil; UEFN bleibt Gameplay-Mesh und sichtbarer Standard; noch kein fest ausgewählter neuer Hauptcharacter | [Retargeting](gasp-runtime-retargeting.md), [Mover-Anbindung](gasp-mover-gameplay.md) |
| Mover-Grundlage | Eigene `RpgGaspMoverExperience`, Bewegung inklusive vorhandener Sprint-Eingabe, Equipment/GAS, Tod/Respawn und optionales Retargeting | [Foundation](gasp-mover-foundation.md), [Gameplay](gasp-mover-gameplay.md), [Lifecycle](gasp-mover-lifecycle.md) |
| Mover-Traversal | Mantle, Vault und Grounded Hurdle akzeptiert und gemergt | [Mover-Mantle](gasp-mover-mantle.md), [Mover-Vault](gasp-mover-vault.md), [Mover-Hurdle](gasp-mover-hurdle.md) |
| Mover-Netzwerk | Fixed 50 Hz; versionierte UE-5.8.2-Interpolationserholung; entfernte Traversal-Animation folgt angezeigter Bewegung | [Fixed](gasp-mover-fixed-tick.md), [Recovery](gasp-mover-network-recovery.md), [Darstellung](gasp-mover-traversal-presentation.md) |
| Mover-Ragdoll | Begrenzter Pilot mit eigener Experience/PawnData, stationärem lebendem Ragdoll/Getup und verankerter Capsule validiert, vom Nutzer bestätigt und mit Farb-/MetaSound-Folgefix in PR #152 gemergt | [Assetumfang](gasp-asset-foundation.md), [Pilot / PR #152](gasp-mover-ragdoll-pilot.md) |

Die Bezeichnung „drei Varianten“ meint **CMC, Mover und Mover-Ragdoll**. Bereits
vorhandene Baseline-/CMC-Test-Experiences bleiben bestehen; daraus folgt keine
Vorgabe, insgesamt genau drei Experience-Dateien zu besitzen.

## Reihenfolge und Status

| ID | Arbeitspaket | Status | Voraussetzung / Abschluss |
| --- | --- | --- | --- |
| `GASP-01` | Grounded Hurdle für Mover | **Gemergt** | [PR #144](https://github.com/Athurito/SurvivalRpg/pull/144), Merge `aa4447d6`; Runtime `b78edf5b`; Editor/Game, 47 Tests und zwei Prozessläufe bestanden; Nutzer-Sichtabnahme am 22.09.2026; [Bericht](gasp-mover-hurdle.md) |
| `GASP-02` | Gezielte Stabilisierung | **Teilweise abgeschlossen, offene Folgepunkte** | `GASP-STAB-01/02`, `GASP-NET-01/02/03` und `GASP-VAL-03` gemergt in PR #145–150; VAL-01/02, NET-03-Rekonstruktionsgrenze und ältere Pending-Fixture-Timingempfindlichkeit bleiben offen |
| `GASP-03` | Mover-Ragdoll-Experience | **Gemergt** | [PR #152](https://github.com/Athurito/SurvivalRpg/pull/152), Merge `9de031c9`, finaler Head `7ab180a8`. Historische Editor/Game-, 20/20-Test-, Sicht- und Assetprüfungen; Farb-/MetaSound-Folgefix enthalten. [Pilotbericht](gasp-mover-ragdoll-pilot.md) |
| `GASP-04` | Vergleich der drei Varianten | **Gemergt** | [PR #154](https://github.com/Athurito/SurvivalRpg/pull/154), Merge `bb690eef`, geprüfter Head `d3240b4b`, Testcode `b9608357`; Editor/Game, 31/31 Tests und Root-Sichtprüfung. [Matrix und Grenzen](gasp-variant-comparison.md); zusätzliche Nutzer-Sichtabnahme nicht erfolgt |
| `GASP-05` | Importbereinigung | **Gemergt: [PR #158](https://github.com/Athurito/SurvivalRpg/pull/158)** | Vier Sample-Akteure und exakt 2298 Originalassets entfernt; Projektkopien erhalten. Zwei Map-Smokes, 9/9 Tests, neun BP-Compiles, frischer Graph und Windows-Cook (3132 Pakete, fünf Karten, Exit 0) bestanden; [PR #158](https://github.com/Athurito/SurvivalRpg/pull/158) bestätigt gemergt. [Entfernung und Grenzen](gasp-original-import-removal.md) |
| `GASP-06` | Erweiterbare Block-Locomotion | **PR offen – Nutzer-Sichtprobe** | Rückbau `de852e98` auf Basis `908b7c75`; gemeinsame Linked-Layer-Logik, zwei Chooser-Datenkinder und native Blockvorhersage. Editor/Game, 19/19 Fokus, 25/25 Regression, Cook und eigene Sichtprobe bestanden; [Bericht](gasp-block-locomotion.md). PR #160 bleibt bis neuer Nutzer-Sichtabnahme offen |

Statuswerte: **Geplant**, **Bereit**, **In Arbeit**, **Validierung**,
**PR offen**, **Gemergt**, **Blockiert**, **Zurückgestellt**.
Eine Aufgabe wird erst nach tatsächlich bestätigtem Merge als **Gemergt** geführt.
Bei Teilfortschritt offene Abnahmepunkte einzeln nennen. Ein neuer Chat darf
eine dokumentierte aktive Aufgabe nicht einfach als erledigt oder frei behandeln.

## GASP-01 – Hurdle für Mover

**Ziel:** Niedrige, dünne Hindernisse mit geprüftem Boden dahinter im originalen
GASP-Ablauf überwinden und in normales Walking zurückkehren. Vault bleibt die
Variante ohne den entsprechenden BackFloor; Mantle bleibt erhalten.

**Umfang:**

- Originalen Mover-Chooser, benötigte Hurdle-Montagen und ihre Abhängigkeiten
  gegen die vorhandene CMC-Adaption prüfen. Nur die tatsächlich benötigten
  Inhalte als projektlokale Mover-Kopien übernehmen.
- Bestehende `RpgGaspMoverExperience`, Query, kontextabhängige Space-Eingabe und
  `GA_RpgGasp_MoverMantle` erweitern. Die Namen stammen aus dem Mantle-Piloten;
  daraus entsteht keine zusätzliche Ability-Familie oder Experience für Hurdle.
- Die bestehenden nativen Traversal-Seams für FrontLedge, BackLedge, BackFloor,
  Landefläche, Collision-Lease und vorhergesagte Bewegung gezielt erweitern.
  Die aktuelle Hurdle-Anbindung setzt an mehreren Stellen `ACharacter` voraus;
  Mover darf diese CMC-Prüfungen nicht einfach überspringen.
- Quellkurven, Notifies, Montage-Tempo und bedingte Handoffs erhalten. Testszenen
  verwenden die freigegebenen GASP-Blöcke, Grid-Materialien und LevelVisuals.

**Ownership:** Server/GAS und Mover besitzen Aktivierung, gültige Geometrie,
Prediction, Warping-Historie und Cleanup. Blueprints, Chooser und Montage-Assets
besitzen konkrete Auswahl und Darstellung. Vor neuen nativen Typen begründen,
welche bestehende Schnittstelle nicht ausreicht; keine vorab verordnete neue Klasse.

**Abnahme:**

- Stehend, gehend, laufend und schräg an geeigneten Hindernissen testen.
- Blockierte Landung, verlorener Support, Abbruch, Tod und Korrektur räumen die
  eigenen Warp-/Collision-/Montage-Ressourcen korrekt auf.
- Host, besitzender Client und beobachtender Client sehen passende Bewegung
  und Animationsphase; Late Join und Handoff ins Weiterlaufen funktionieren.
- Reale Fixed-Korrektur prüfen; Wertetests allein sind kein Multiplayer-Nachweis.
- Mover-Mantle/Vault, CMC-Traversal, Equipment-Montagen und optionales Retargeting
  entsprechend den berührten Schnittstellen auf Regression prüfen.
- Editor-/Game-Builds soweit betroffen tatsächlich ausführen; geänderte Assets
  frisch laden/kompilieren und Quell-Ziel-/Referenzvergleich dokumentieren.
- Manuellen Sichttest sowie verbleibende Grenzen im PR/Übergabestand festhalten.

Umsetzung und aktuelle Belege: [Mover-Hurdle](gasp-mover-hurdle.md).
Einstieg: [CMC-Hurdle](gasp-hurdle-integration.md),
[Mover-Vault](gasp-mover-vault.md),
[Traversal-Darstellung](gasp-mover-traversal-presentation.md),
`Source/SurvivalRpg/Traversal/RpgGameplayAbility_Mantle.cpp` und
`Source/SurvivalRpg/Core/Character/RpgMoverTraversalTypes.h`.

## GASP-02 – Register der offenen Folgearbeiten

Jede Zeile ist ein begrenzter Folgeauftrag, keine Aufforderung, alle Probleme in
einem PR zu bearbeiten. **`GASP-STAB-01/02`, `GASP-NET-01/02/03` und
`GASP-VAL-03` sind gemergt. Auch der GASP-03-Pilot ist gemergt;
GASP-04 ist mit PR #154 gemergt; GASP-05-Audit ist mit PR #156 gemergt;
die Originalentfernung ist mit PR #158 bestätigt gemergt.
VAL-01/02 bleiben dokumentierte
Mess- bzw. Umgebungsgrenzen.**

| ID | Einordnung | Arbeit und Abschlussnachweis |
| --- | --- | --- |
| `GASP-STAB-01` | **Gemergt** | Ursprünglichen Ensure frisch reproduziert; Cleanup an ursprünglichen ASC/DefenseSet gebunden, Basiswerte und rekursive Enden abgesichert. Runtime `43ac69b8`; Editor/Game und 11/11 Tests bestanden. [PR #145](https://github.com/Athurito/SurvivalRpg/pull/145), Merge `4039be25`; [Bericht](gasp-block-cleanup.md). |
| `GASP-STAB-02` | **Gemergt** | Zwei reale Pawns am Checkpoint reproduzieren Falling/Velocity 0 durch gescheiterte Penetrationsauflösung. Konkreter RPG-Pawn nutzt Engine-Spawnanpassung; identischer Crowd-Test rot/grün, Editor/Game und 11/11 Regressionen bestanden. Zusätzlich 2/2 im sichtbaren Editor und 1/1 Crowd-Diagnose mit eigener Sichtprüfung. Commit `57fa8de9`, [PR #146](https://github.com/Athurito/SurvivalRpg/pull/146), Merge `b9a65360`; [Ursache, Grenzen und Nachstellen](gasp-respawn-falling.md). Kein allgemeiner Anspruch bei vollständig verbautem Checkpoint. |
| `GASP-NET-01` | **Gemergt** | Echten Restore/Replay samt Gegenkorrektur desselben aufgezeichneten aktiven Frames nachgewiesen, einschließlich vom Server übersprungenem Injektionsframe. Warp-/Collider-/Montagevertrag erhalten; Editor-Build, 10/10 Regressionen, 30-FPS-Fokus und erwartete negative Gegenprobe. Commit `b6709eb4`, [PR #147](https://github.com/Athurito/SurvivalRpg/pull/147), Merge `44a5e512`; [Nachweis](gasp-active-mantle-rollback.md). |
| `GASP-NET-02` | **Gemergt** | Frische Cleanup-Gates belegen vorzeitigen Abbruch durch normales Remote-Ende während Auto-Blend-Out. Exakten natürlichen Endcallback auf Mover-Mantle erweitert; echte Cancellation bleibt sofort wirksam. Commit `8de5d927`, [PR #148](https://github.com/Athurito/SurvivalRpg/pull/148), Merge `1490dd7d`; 30-FPS-Fokus rot 1/3 und grün 3/3, Editor/Game und Regression **28/28** bestanden; [Bericht](gasp-terminal-reconciliation.md). |
| `GASP-NET-03` | **Gemergt**, Grenze bewertet / Werkzeug validiert | Commit `49de7c87`, [PR #149](https://github.com/Athurito/SurvivalRpg/pull/149), Merge `d933148f`. Getrackter Probe und getrennte Offline-Diagnose, 19/19 Tests. Kontrolle 16/16 Ebenen, Pause 13/16, maximal 86,21 ms im alten Kriterium; Root-Abweichung bis 64,00 cm nach Pause, schon 28,54 cm in Kontrolle. Freeze besteht; Runtime-Grenze bleibt, keine Toleranzerhöhung; [Bericht](gasp-packet-gap-recovery.md). |
| `GASP-VAL-01` | Messgrenze | Im finalen separaten Prozesslauf ist ein Walking→Traversing-Messpaar nicht numerisch vergleichbar. Die erste aktive Traversing-Probe enthält bereits die Montage. Bei Bedarf Onset-Messung verfeinern; keinen belegten Animationsaussetzer daraus ableiten. |
| `GASP-VAL-02` | Noch nicht ausgeführte Umgebungsprüfung | Cooked/packaged und WAN bzw. gezielt emulierte Netzwerkbedingungen prüfen: verschiedene Render-FPS, Delay/Jitter/Loss, Late Join, Traversal, Korrekturen und Respawn. Lokale uncooked Ergebnisse ersetzen diese Prüfung nicht. |
| `GASP-VAL-03` | **Gemergt** | Testcommit `8bc887e1`, [PR #150](https://github.com/Athurito/SurvivalRpg/pull/150), Merge `4ed174d2`: B-GAS-Empfang nach beendeter normaler Waffenmontage bei noch aktiver A im NP-Puffer belegt. Fokus und 30 FPS grün, Negativkontrolle erwartungsgemäß rot, Runtime exakt wiederhergestellt und Editor-Build bestanden. Regression bleibt 15/16; alter Overlap-Test rot, unverändert isoliert 1/1 grün. Runtime/Assets unverändert, Abschlussaudit bestanden. [Bericht](gasp-buffered-traversal-replay.md). |

Quellen: [Recovery-Ergebnisse](gasp-mover-network-recovery.md),
[Präsentations-Ergebnisse und Grenzen](gasp-mover-traversal-presentation.md).
Die damaligen „PR bleibt Draft“-Sätze sind historische Befunde. PR #142 wurde
anschließend nach Nutzer-Sichttest ausdrücklich zum Merge freigegeben.

**Vor GASP-03:** Lifecycle-Fehler und Zustandsabweichungen zuerst gezielt bearbeiten
und ihre Auswirkung auf Ragdoll/Tod/Respawn dokumentieren. Nicht reproduzierte
Fälle bleiben offen; weitere Prototyp-Arbeit darf diese nicht als behoben ausgeben.
Ein konkreter blockierender Fehler wird zuerst korrigiert. Die vollständige
Umgebungsfreigabe aus `GASP-VAL-02` ist für Produktionsreife erforderlich, kein
pauschales Verbot eines begrenzten Ragdoll-Piloten.

## GASP-03 – Eigene Mover-Ragdoll-Experience

**Erster Teilauftrag:** Originalen GASP-Ragdoll-Pawn, Physics-Control-/Mover-
Abhängigkeiten und Anknüpfung an vorhandene RPG-Komponenten auditieren. Noch
keine vollständige migrierte Ragdoll-Pawn-Basis voraussetzen. Ergebnis ist eine
Quell-Ziel-/Ownership-Zuordnung und ein begrenzter Implementierungsumfang.
Dieser Audit ist mit PR #151 bestätigt gemergt (`9baac5f3`); Quellbefunde und Lücken stehen im
[Auditbericht](gasp-mover-ragdoll-source-audit.md) und
[Manifest](assets/gasp-ragdoll-source-audit.json). Er behauptet keine
Runtimeänderung, importierten Assets oder neu bestandenen Unreal-Builds.
Der begrenzte **GASP-03-Pilot** ist mit PR #152 gemergt: originalabgeleitete
Blueprint-/PhysicsControl-/Getup-Komposition auf vorhandenen RPG-Schnittstellen,
lebendes stationäres Ragdoll/Getup auf freier ebener Fläche mit UEFN und
autoritativ verankerter Capsule. Umfang, Quellabweichungen und begrenzte
Netzwerknachweise stehen im [Pilotbericht](gasp-mover-ragdoll-pilot.md).

Eine eigene Experience/PawnData-Variante aufbauen. Bestehende Experiences
bleiben erhalten. Ragdoll-Einstieg und Aufstehen, Kontrollrückgabe, Equipment,
GAS-Abbruch, Tod/Respawn und Rekonstruktion bei Late Join ausdrücklich behandeln.
Lebendes Ragdoll und endgültiger Tod dürfen keinen zweiten Health-/Respawn-Pfad
einführen. UEFN bleibt zunächst Gameplay-Mesh; die physische Autorität und Rolle
eines optionalen Retarget-Followers vor Umsetzung festlegen.

**Abnahme:** Ein-/Ausstieg, Unterbrechung, Tod während Ragdoll, Respawn, Observer
und Late Join ohne übrig gebliebene Collision-, Montage- oder Input-Ownership.
Aktive Abhängigkeiten projektlokal; Asset-Lade-/Compile- und passende Build-/Netz-
Tests sowie Sichttest nachweisen. Historische Physics-Control-Warnungen aus
[diesem Bericht](gasp-physics-control-followup.md) einbeziehen, ihre Ursache aber
nicht ohne neue Evidenz als gelöst oder erneut vorhanden behaupten.

## GASP-04 – Gemeinsame Abnahme

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

## GASP-05 – Importbereinigung

**Aktuell:** Nach dem Audit wurden die vier alten Demoobjekte entfernt; der
Nutzer hat zusätzlich alle exakt inventarisierten 2298 Originalassets zur
Entfernung freigegeben. Diese ist ausgeführt; frischer Graph, Karten-Smokes,
gezielte Tests, Compiles und repräsentativer Windows-Cook bestehen. [PR #158](https://github.com/Athurito/SurvivalRpg/pull/158) ist bestätigt gemergt;
[aktueller Bericht](gasp-original-import-removal.md). Der folgende Auditbefund
beschreibt den Ausgangspunkt vor diesen Änderungen.

Der erste [Abhängigkeitsaudit](gasp-import-cleanup-audit.md) ist in
[PR #156](https://github.com/Athurito/SurvivalRpg/pull/156) bestätigt gemergt (`fdff8b91`). Damals war die konkrete Entfernungsliste leer: Jede der Karten
`Lvl_RpgBaseline` und `Lvl_ThirdPerson` erreichte alle 2156 zugeordneten Originale.
Der damalige Folgeauftrag ist inzwischen ausgeführt: Die vier Sample-Akteure
wurden nach Instanzprüfung entfernt, der vollständige historische Importumfang
inventarisiert und die Entfernung ohne Originale validiert. Baseline, Spawns,
Persistenzisolation und genehmigte Präsentation sind erhalten. Die damalige
Auditprüfung allein enthielt noch keine Lade-/Compile-/Cook-Abnahme; die neue
Abnahme und ihre Grenzen stehen im aktuellen Entfernungsbericht.

Erst nach dem Variantenvergleich einen eigenen Bereinigungs-PR erstellen.
Aktive eigene Inhalte bleiben unter `/Game/SurvivalRpg`; vorhandene Engine-,
Plugin- und GameFeature-Abhängigkeiten werden wiederverwendet.

Vor jeder Entfernung eine konkrete Paketliste und Quell-Ziel-Zuordnung erzeugen.
Harte, weiche, Management- und dynamisch konfigurierte Referenzen, Chooser,
PoseSearch, Retargeting, Foley und Map-Auswahl prüfen. Frisches Laden/Kompilieren
und repräsentatives Cooking müssen ohne Importoriginale bzw. externes
`D:/Repos/GameAnimationSample` als Laufzeitquelle funktionieren. Baseline und
benötigte Varianten erhalten, Rückweg über Git/LFS sichern. Vorhandene
Foundation-/Sample-Dateien nicht allein aufgrund ihres Ordnernamens löschen.

## GASP-06 – Erweiterbare Block-Locomotion

Der Nutzer hat den bisherigen Ansatz visuell abgelehnt und ausdrücklich den
vollständigen Rückbau auf PR-Basis `908b7c75` gewählt. Der Rückbau umfasst auch
Schulterkorrektur, Blocklease, Kameraausrichtung, Sprint-Sperre, Werkzeuge und
Tests. Historische Berichte sind als zurückgenommen markiert und bleiben erhalten.

Neuaufbau: Ein gemeinsamer Blueprint-Linked-Layer besitzt Idle, MoveStart,
MoveLoop, MoveStop und Turn. Datenkinder konfigurieren Chooser, BlendSpaces und
Tuning pro Animationssatz. Equipment referenziert den Layer; GAS und CMC/Mover
besitzen Zustand, Kameraausrichtung, Sprint-Sperre und vorhergesagte Tempogrenze.
Sword-and-Shield ist der erste Satz, ein zweiter Testsatz belegt Austauschbarkeit.
Retargeting, Fußkontakte und Root-Yaw werden je Satz geprüft; Quellclips bleiben
erhalten. Natürliche Haltung hat Vorrang vor bisherigem Normaltempo.

Die Arbeit bleibt in PR #160. Neue Builds, Asset-/Netzwerkprüfungen, Cook und
eigene Sichtabnahme sind erforderlich. Danach folgt die Nutzer-Sichtprobe vor
Merge. Die alten Validierungsergebnisse ersetzen diese neue Abnahme nicht.

## Verbindliche Arbeitsweise über mehrere Chats

**Dauerhafte Nutzerfreigabe vom 26.09.2026:** Schritte, die sich nicht sinnvoll
manuell prüfen lassen, nach geeigneter automatischer Validierung und Review
direkt pushen, mergen und mit dem nächsten begrenzten Roadmap-Schritt fortfahren.
Dafür nicht auf eine zusätzliche Sichtabnahme warten. Tatsächliche Ergebnisse,
offene Befunde und den bestätigten Merge weiterhin dokumentieren. Sinnvolle
manuelle Sichtprüfungen werden dadurch nicht als automatisch erledigt behauptet.

1. `AGENTS.md`, diese Datei und die [Übergabe](gasp-integration-handoff.md) lesen.
   Auftrag anhand seiner ID auswählen; einen begonnenen Auftrag fortsetzen oder
   den nächsten bereiten bearbeiten. Keine ganze Roadmap in einem Chat starten.
2. Git-Status, aktuellen `master`, offene PRs und den tatsächlichen Asset-/Code-
   Stand prüfen. Eigener `codex/...`-Branch pro begrenztem Auftrag. Ein neuer
   Checkout benötigt den Commit, der diese Roadmap enthält; bei fehlender Datei
   zuerst den Dokumentations-Branch/PR aus der Übergabe einbeziehen.
3. ID, Status **In Arbeit**, Branch/Worktree und bearbeitete Dateien in der
   Übergabe festhalten. Andere Chats dürfen nicht gleichzeitig dieselben Assets
   oder gemeinsam genutzte Traversal-/Lifecycle-Seams ändern. Auch getrennte
   Worktrees brauchen koordinierte Editor-/MCP-Sitzungen.
4. Passende Skills nutzen: GASP + Lyra; bei Combat/Equipment zusätzlich Combat
   Foundation. Vor Umsetzung authoritative Runtime-Owner, native Schnittstelle,
   konkrete Designer-Assets, Darstellung, Tooling und stabile Tests benennen.
   Designer-Assets über Unreal MCP bearbeiten; kein nativer Ersatz aus Bequemlichkeit.
5. Nur die Abnahmepunkte des gewählten Schritts und berührte Regressionen prüfen.
   Builds, Automation, Asset-Audit und Sichttest getrennt protokollieren. Alte
   Ergebnisse nicht als frisch ausgeführt darstellen. Fehlversuche erhalten.
6. Am Ende Roadmap-Status und Übergabe im selben Arbeits-PR aktualisieren:
   Commit/PR, tatsächliche Ergebnisse, offene Fehler und der nächste konkrete
   Handgriff. Ein offener PR ist kein bestätigter Merge; Merge erst gemäß
   Nutzerauftrag und tatsächlichem Repository-Status eintragen.

### Unreal-Umgebung und bestehende Grenzen

- Verwendeter Stand: **UE 5.8.2**, Fixed 50 Hz / 20 ms. Render-FPS und
  Simulationsrate sind verschieden. Keine globale FPS-Kappung, langsamere
  Montage oder Rückkehr zu Independent als Ersatz für eine Ursachenanalyse.
- Der versionierte [NetworkPrediction-Patch](../Build/Patches/NetworkPrediction/README.md)
  erzeugt **Git-ignorierte, checkout-lokale** Plugin-Overrides. Ein neuer Worktree
  besitzt sie nicht automatisch. Bei vorhandener Installation `prepare.py verify`
  nutzen; sonst bei geschlossenem Editor gemäß README `check`, `stage`, `verify`
  mit der dort geprüften UE-Version durchführen und normal bauen.
- NetworkPrediction ist gepatcht; Mover, ChaosMover und MoverExamples sind
  unveränderte Consumer-Kopien. Die installierte Engine bleibt unangetastet.
  Laden des Projekt-Plugins tatsächlich prüfen; Hash-Verifikation ist kein Build.
- Vor Wechsel auf einen Branch ohne Patch bei geschlossenem Editor die
  verifizierte `prepare.py remove`-Prozedur verwenden. Git entfernt ignorierte
  Overrides nicht. Keine unkontrollierte Plugin-Kopie aus einem anderen Checkout.
- Testmaps verwenden die freigegebenen GASP-Materialien und LevelVisuals.
  Persistenz in isolierten Testwelten deaktivieren; bestehende Saves erhalten.
- Der Montage-Bridge-Vertrag deckt die geprüften Standard-Blends ab. Generische
  Inertialization-Replikation ist keine bereits implementierte Fähigkeit.
- `Saved/...` enthält lokale, nicht versionierte Belege. Ein neuer Chat/Worktree
  darf deren Vorhandensein nicht voraussetzen. Aussagekräftige Zusammenfassungen,
  Testfilter, Version/Commit und Einschränkungen gehören in versionierte Docs/PRs;
  fehlende Rohdaten benennen, erforderliche Prüfungen gezielt neu ausführen.

## Kopiervorlagen für neue Chats

Nächsten bereiten Schritt beginnen:

```text
Lies AGENTS.md, docs/gasp-integration-roadmap.md und
docs/gasp-integration-handoff.md. Prüfe zuerst den aktuellen Repository- und
PR-Stand. Bearbeite nur die dort als Nächstes bereite Aufgabe in einem eigenen
codex/-Branch. Respektiere Umfang und Abnahmekriterien, dokumentiere tatsächliche
Validierung und aktualisiere Roadmap und Übergabe. Push den geprüften Stand und
öffne einen PR zur Prüfung. Für nicht sinnvoll manuell prüfbare Schritte gilt
meine dokumentierte Dauerfreigabe: nach geeigneter automatischer Validierung
und Review direkt mergen und mit dem nächsten begrenzten Schritt fortfahren;
nicht auf eine zusätzliche Sichtabnahme warten.
```

Einen begonnenen Schritt fortsetzen:

```text
Setze die in docs/gasp-integration-handoff.md aktive Aufgabe fort.
Lies zuerst die Roadmap, den angegebenen Branch/PR und die letzte Übergabe.
Prüfe, was bereits implementiert und tatsächlich getestet wurde. Fahre beim
genannten nächsten Handgriff fort und aktualisiere am Ende die Übergabe.
```
