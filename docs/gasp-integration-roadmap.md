# GASP-Integration: Roadmap

Stand: **13.10.2026**. Zentraler Arbeitsplan für die GASP-/Lyra-Integration:
Bewegung und ihre RPG-Anbindung, keine Gesamtplanung für Combat, Crafting oder
Portale (dafür gilt die [Spielvision](game-vision.md)).

Diese Datei enthält nur Status, Arbeitsregeln und den nächsten bereiten Schritt.
Den aktuellen Arbeitsstand mit aktiver Aufgabe, Branch, Dateibesitz und offenen
Befunden führt die [Übergabe](gasp-integration-handoff.md). Ziel, Umfang,
Abnahme, Belege und Grenzen jeder Aufgabe stehen in ihrem Bericht und PR. Die
frühere Fassung mit Schnellstart-Verlauf und Aufgabenverträgen liegt in der
[Git-Historie](https://github.com/Athurito/SurvivalRpg/blob/a2ddbc833212c053df51fa7d3d3e2ec334aabd30/docs/gasp-integration-roadmap.md).

## Nächster bereiter Schritt

**`GASP-15` Manny als CMC-Gameplay-Mesh** ist **Bereit** ([Definition](gasp-manny-gameplay-mesh.md)):
Der Nutzer hat am 17.10.2026 den Start gewünscht und entschieden: Nur die CMC-Pawns wechseln auf Manny; der
Mover bleibt auf UEFN mit Runtime-Follower, und die Block-Darstellung wird je Skelett wählbar. Der
`GASP-08`-Pilot wird entfernt, die `GASP-14`-Entwicklungskette geht in der Produktion auf.
`GASP-STAB-05` (Loslassen von Block) bleibt zurückgestellt; der PR [#249](https://github.com/Athurito/SurvivalRpg/pull/249) bleibt als Entwurf.
`GASP-01`, `GASP-03` bis `GASP-14`, `GASP-STAB-04` und das Engine-Update `GASP-ENG-01` sind
gemergt; offen bleiben `GASP-VAL-01`, `GASP-VAL-02` und die
[offenen Befunde](gasp-integration-handoff.md#offene-befunde).

## Bereits vorhanden

| Bereich | Akzeptierter Stand | Berichte |
| --- | --- | --- |
| Baseline und Assetbasis | Baseline erhalten; übernommene Inhalte unter `/Game/SurvivalRpg/Characters/GASP`; Quell-Ziel-Zuordnung vorhanden; Originalimport entfernt | [Assetbasis](gasp-asset-foundation.md), [Manifest](assets/gasp-asset-map.json), [Entfernung](gasp-original-import-removal.md) |
| CMC | GASP-Locomotion, Mantle, Vault und Hurdle in bestehenden Experiences; GAS-Sprint mit Ausdauer | [CMC](gasp-cmc-integration.md), [Mantle](gasp-mantle-integration.md), [Vault](gasp-vault-integration.md), [Hurdle](gasp-hurdle-integration.md), [Sprint](gasp-cmc-sprint.md) |
| Block-Locomotion | Gemeinsamer Linked Layer mit Datenkindern pro Animationssatz auf CMC und Mover | [Block-Locomotion](gasp-block-locomotion.md) |
| Runtime-Retargeting | Optionales Profil; UEFN bleibt Gameplay-Mesh und sichtbarer Standard; noch kein fest ausgewählter neuer Hauptcharacter | [Retargeting](gasp-runtime-retargeting.md), [Mover-Anbindung](gasp-mover-gameplay.md) |
| Manny-Skelett-Pilot | Bestanden: 144 CMC-Dense-Clips offline mit dem offiziellen Retargeter auf das Lyra-`SK_Mannequin` gebacken, isolierte Pilot-Pawns und -Experiences; die Produktion nutzt weiter UEFN, bis `GASP-14`/`GASP-15` umstellen | [Manny-Pilot](gasp-manny-skeleton-pilot.md), [Manifest](assets/gasp-08-manny-pilot.json) |
| Mover-Grundlage | Eigene `RpgGaspMoverExperience`, Bewegung inklusive Sprint-Eingabe, Equipment/GAS, Tod/Respawn und optionales Retargeting | [Foundation](gasp-mover-foundation.md), [Gameplay](gasp-mover-gameplay.md), [Lifecycle](gasp-mover-lifecycle.md) |
| Mover-Traversal | Mantle, Vault und Grounded Hurdle | [Mover-Mantle](gasp-mover-mantle.md), [Mover-Vault](gasp-mover-vault.md), [Mover-Hurdle](gasp-mover-hurdle.md) |
| Mover-Netzwerk | Fixed 50 Hz; versionierte UE-5.8.3-Interpolationserholung; entfernte Traversal-Animation folgt angezeigter Bewegung | [Fixed](gasp-mover-fixed-tick.md), [Recovery](gasp-mover-network-recovery.md), [Darstellung](gasp-mover-traversal-presentation.md) |
| Mover-Ragdoll | Begrenzter Pilot mit eigener Experience/PawnData, stationärem lebendem Ragdoll/Getup und verankerter Capsule | [Source-Audit](gasp-mover-ragdoll-source-audit.md), [Pilot](gasp-mover-ragdoll-pilot.md) |

„Drei Varianten“ meint **CMC, Mover und Mover-Ragdoll**. Bestehende Baseline-
und CMC-Test-Experiences bleiben erhalten; daraus folgt keine Vorgabe, genau
drei Experience-Dateien zu besitzen.

## Status

| ID | Arbeitspaket | Status | PR | Bericht |
| --- | --- | --- | --- | --- |
| `GASP-01` | Grounded Hurdle für Mover | **Gemergt** | [#144](https://github.com/Athurito/SurvivalRpg/pull/144) | [Mover-Hurdle](gasp-mover-hurdle.md) |
| `GASP-02` | Gezielte Stabilisierung, Register unten | **Teilweise abgeschlossen** | #145–#150 | Registerzeilen |
| `GASP-STAB-01` | Block-Cleanup nach entferntem DefenseSet | **Gemergt** | [#145](https://github.com/Athurito/SurvivalRpg/pull/145) | [Block-Cleanup](gasp-block-cleanup.md) |
| `GASP-STAB-02` | Falling bei belegtem Respawn | **Gemergt** | [#146](https://github.com/Athurito/SurvivalRpg/pull/146) | [Respawn-Falling](gasp-respawn-falling.md) |
| `GASP-STAB-03` | Testboden vor den Pawns in CQTest-Netzwerktests | **Gemergt** | [#223](https://github.com/Athurito/SurvivalRpg/pull/223) | [Testboden](gasp-test-floor-race.md) |
| `GASP-STAB-04` | Listen-Server: Client-Figur zittert in der Host-Ansicht beim Drehen im Stand (Offset Root Bone) | **Gemergt**; Sichtprüfung durch den Nutzer | [#244](https://github.com/Athurito/SurvivalRpg/pull/244) | [Host-Ansicht](gasp-host-view-turn-jitter.md) |
| `GASP-STAB-05` | Loslassen von Block im Stand: Schritte statt Rutschen | **Zurückgestellt** (Nutzer, 17.10.2026): Drücken wirkte nicht mehr snappy; PR als Entwurf, nicht gemergt | [#249](https://github.com/Athurito/SurvivalRpg/pull/249) | [Loslassen (Branch)](https://github.com/Athurito/SurvivalRpg/blob/claude/gasp-stab-05-block-release/docs/gasp-block-release.md) |
| `GASP-NET-01` | Rollback während aktivem Mantle | **Gemergt** | [#147](https://github.com/Athurito/SurvivalRpg/pull/147) | [Mantle-Rollback](gasp-active-mantle-rollback.md) |
| `GASP-NET-02` | Terminaler Mantle-Grund bei Reconciliation | **Gemergt** | [#148](https://github.com/Athurito/SurvivalRpg/pull/148) | [Reconciliation](gasp-terminal-reconciliation.md) |
| `GASP-NET-03` | Rekonstruktion nach Paketpause | **Gemergt**; Grenze bewertet, Werkzeug validiert, Runtime-Grenze bleibt | [#149](https://github.com/Athurito/SurvivalRpg/pull/149) | [Paketpause](gasp-packet-gap-recovery.md) |
| `GASP-VAL-01` | Onset-Messung Walking→Traversing | **Zurückgestellt**: Messgrenze, nur bei Bedarf verfeinern | – | [Offene Befunde](gasp-integration-handoff.md#offene-befunde) |
| `GASP-VAL-02` | Cooked/packaged und WAN bzw. emulierte Netzbedingungen | **Geplant**: noch nicht ausgeführte Umgebungsprüfung | – | [Offene Befunde](gasp-integration-handoff.md#offene-befunde) |
| `GASP-VAL-03` | Traversal B bei noch gepufferter Traversal A | **Gemergt** | [#150](https://github.com/Athurito/SurvivalRpg/pull/150) | [Gepufferter Replay](gasp-buffered-traversal-replay.md) |
| `GASP-03` | Mover-Ragdoll-Experience: Source-Audit und Pilot | **Gemergt** | [#151](https://github.com/Athurito/SurvivalRpg/pull/151), [#152](https://github.com/Athurito/SurvivalRpg/pull/152) | [Source-Audit](gasp-mover-ragdoll-source-audit.md), [Pilot](gasp-mover-ragdoll-pilot.md) |
| `GASP-04` | Vergleich der drei Varianten | **Gemergt** | [#154](https://github.com/Athurito/SurvivalRpg/pull/154) | [Variantenvergleich](gasp-variant-comparison.md) |
| `GASP-05` | Importbereinigung: Audit und Entfernung | **Gemergt** | [#156](https://github.com/Athurito/SurvivalRpg/pull/156), [#158](https://github.com/Athurito/SurvivalRpg/pull/158) | [Audit](gasp-import-cleanup-audit.md), [Entfernung](gasp-original-import-removal.md) |
| `GASP-06` | Erweiterbare Block-Locomotion | **Gemergt** | [#160](https://github.com/Athurito/SurvivalRpg/pull/160) | [Block-Locomotion](gasp-block-locomotion.md) |
| `GASP-07` | CMC-Sprint mit GAS-Ausdauer | **Gemergt** | [#161](https://github.com/Athurito/SurvivalRpg/pull/161) | [CMC-Sprint](gasp-cmc-sprint.md) |
| `GASP-ENG-01` | Engine-Update auf UE 5.8.3: NetworkPrediction-/Mover-Overrides und Build-Pin | **Gemergt** | [#221](https://github.com/Athurito/SurvivalRpg/pull/221) | [UE 5.8.3](gasp-engine-5-8-3.md) |
| `GASP-08` | Pilot: GASP-Locomotion offline auf UE5-Manny-Skelett | **Gemergt**; Exit-Entscheidung **Pass** (Nutzer) | [#228](https://github.com/Athurito/SurvivalRpg/pull/228) | [Manny-Pilot](gasp-manny-skeleton-pilot.md) |
| `GASP-09` | Rotationsmodi: freie Bewegung und Kampfhaltung (Strafe), Sprint-Richtungs-Gate | **Gemergt**; Sichtprüfung durch den Nutzer | [#232](https://github.com/Athurito/SurvivalRpg/pull/232) | [Rotationsmodi](gasp-rotation-modes.md) |
| `GASP-10` | Animationsset und Tempo-Parität (Relaxed), Ducktempo | **Gemergt**; Sichtprüfung durch den Nutzer | [#235](https://github.com/Athurito/SurvivalRpg/pull/235) | [Animationsset](gasp-animation-set.md) |
| `GASP-11` | Gemeinsames Bewegungsprofil (Trägheit, Drehung, Luft), lastunabhängig | **Gemergt**; Sichtprüfung durch den Nutzer | [#238](https://github.com/Athurito/SurvivalRpg/pull/238) | [Bewegungsprofil](gasp-movement-profile.md) |
| `GASP-12` | Walk-Gait und Analog-Schwelle | **Gemergt**; Sichtprüfung durch den Nutzer | [#241](https://github.com/Athurito/SurvivalRpg/pull/241) | [Walk-Gait](gasp-walk-gait.md) |
| `GASP-13` | Darstellung: Drehen im Stand (Kampfhaltung), Offset Root Bone, Steering, Spins, Kamera | **Gemergt**; Sichtprüfung durch den Nutzer, Problem beim Loslassen von Block auf später verschoben | [#247](https://github.com/Athurito/SurvivalRpg/pull/247) | [Darstellung](gasp-presentation-polish.md) |
| `GASP-14` | Vollständiger CMC-Bake auf Manny (`SKM_Manny_Simple`): Relaxed-Locomotion, Traversal, Motion-Matching-Kopien, Manny-AnimBP, Entwicklungs-Pawn | **Gemergt**; Sichtprüfung durch den Nutzer | [#251](https://github.com/Athurito/SurvivalRpg/pull/251), Definition [#250](https://github.com/Athurito/SurvivalRpg/pull/250) | [Manny-Bake](gasp-manny-full-bake.md) |
| `GASP-15` | Manny als CMC-Gameplay-Mesh: Mesh, Traversal, Block je Skelett, Waffen, Tod/Ragdoll, Pilot entfernen; Mover bleibt UEFN | **PR offen** | [#254](https://github.com/Athurito/SurvivalRpg/pull/254), Definition [#253](https://github.com/Athurito/SurvivalRpg/pull/253) | [Manny-Gameplay-Mesh](gasp-manny-gameplay-mesh.md) |

Statuswerte: **Geplant**, **Bereit**, **In Arbeit**, **Validierung**,
**PR offen**, **Gemergt**, **Blockiert**, **Zurückgestellt**.

- Eine Aufgabe wird erst nach tatsächlich bestätigtem Merge als **Gemergt**
  geführt. „Gemergt“ bedeutet nicht, dass alle Netzwerk-/Lifecycle-Randfälle
  gelöst oder alle Umgebungen getestet sind; die Grenzen stehen im Bericht.
- Bei Teilfortschritt offene Abnahmepunkte einzeln in der Übergabe nennen. Ein
  neuer Chat darf eine dokumentierte aktive Aufgabe nicht als erledigt oder frei
  behandeln.
- Jede `GASP-02`-Registerzeile ist ein begrenzter Folgeauftrag, keine
  Aufforderung, alle Probleme in einem PR zu bearbeiten.
- Merge-Hash, finaler Head, Prüfzahlen und Fehlversuche stehen im Bericht und
  im PR, nicht in dieser Tabelle.

## Geplante Pakete

Geplante, noch nicht bereite Pakete aus dem [Bewegungsplan](gasp-movement-plan.md).
Befunde, Abhängigkeiten und Reihenfolge stehen dort. Ein Paket wechselt erst
mit **Bereit** in die Statustabelle oben.

- `GASP-16` setzt auf dem Manny-Skelett aus `GASP-14`/`GASP-15` auf.

| ID | Arbeitspaket | Status | Plan |
| --- | --- | --- | --- |
| `GASP-16` | Charakter- und Rüstungspilot (Asset-Auswahl durch den Nutzer), Rüstungs-Foley aus `GASP-13` | **Geplant** | [Plan](gasp-movement-plan.md#gasp-16-character-and-armor-pilot) |

## Arbeitsregeln über mehrere Chats

**Dauerhafte Nutzerfreigabe vom 26.09.2026:** Schritte, die sich nicht sinnvoll
manuell prüfen lassen, nach geeigneter automatischer Validierung und Review
direkt pushen, mergen und mit dem nächsten begrenzten Roadmap-Schritt fortfahren.
Dafür nicht auf eine zusätzliche Sichtabnahme warten. Tatsächliche Ergebnisse,
offene Befunde und den bestätigten Merge weiterhin dokumentieren. Sinnvolle
manuelle Sichtprüfungen werden dadurch nicht als automatisch erledigt behauptet.

1. `AGENTS.md`, die [Übergabe](gasp-integration-handoff.md) und diese Datei
   lesen, danach nur den Bericht der betroffenen Aufgabe. Auftrag anhand seiner
   ID wählen: einen begonnenen fortsetzen oder den als **Bereit** eingetragenen
   bearbeiten. Keine ganze Roadmap in einem Chat starten.
2. Git-Status, aktuellen `master`, offene PRs und den tatsächlichen Asset-/Code-
   Stand prüfen. Eigener Branch pro begrenztem Auftrag.
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
   Ergebnisse nicht als frisch ausgeführt darstellen, Fehlversuche erhalten.
   Ein späterer grüner Lauf schließt keinen offenen Befund ohne eigenen Nachweis.
6. Am Ende im selben Arbeits-PR aktualisieren:
   - **Bericht** `docs/gasp-<thema>.md`: Auftrag, Ownership, tatsächliche
     Validierung, Fehlversuche, Grenzen, Nachstellen, Commits und PR; bei
     Assetumfang zusätzlich ein Manifest unter `docs/assets`.
   - **Roadmap**: nur die Statuszeile und gegebenenfalls den nächsten bereiten
     Schritt.
   - **Übergabe**: den aktuellen Stand ersetzen, nicht anhängen.
   - **PR-Beschreibung**: Validierung und Grenzen.

   Ein offener PR ist kein bestätigter Merge; Merge erst gemäß Nutzerauftrag und
   tatsächlichem Repository-Status eintragen. Eine reine Merge-Statuspflege
   ändert Statuszeile, Übergabe und den Statuskopf des Berichts.

### Unreal-Umgebung und bestehende Grenzen

- Verwendeter Stand: **UE 5.8.3** (seit `GASP-ENG-01`; ältere Berichte nennen
  ihre damalige Version 5.8.2), Fixed 50 Hz / 20 ms. Render-FPS und
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
Lies AGENTS.md, docs/gasp-integration-handoff.md und
docs/gasp-integration-roadmap.md, danach nur den Bericht der betroffenen
Aufgabe. Prüfe zuerst den aktuellen Repository- und PR-Stand. Bearbeite nur die
als Bereit eingetragene Aufgabe in einem eigenen Branch; ist keine bereit,
frag mich, statt eine auszuwählen. Respektiere Umfang und Abnahmekriterien.
Halte tatsächliche Validierung, Fehlversuche und Grenzen im Task-Bericht und im
PR fest, aktualisiere die Statuszeile der Roadmap und ersetze den aktuellen
Stand der Übergabe. Push den geprüften Stand und öffne einen PR zur Prüfung.
Für nicht sinnvoll manuell prüfbare Schritte gilt meine dokumentierte
Dauerfreigabe: nach geeigneter automatischer Validierung und Review direkt
mergen und mit dem nächsten begrenzten Schritt fortfahren; nicht auf eine
zusätzliche Sichtabnahme warten.
```

Einen begonnenen Schritt fortsetzen:

```text
Setze die in docs/gasp-integration-handoff.md aktive Aufgabe fort. Lies zuerst
die Übergabe, die Roadmap, den angegebenen Branch/PR und den Task-Bericht.
Prüfe, was bereits implementiert und tatsächlich getestet wurde. Fahre beim
genannten nächsten Handgriff fort. Ergänze am Ende den Task-Bericht und ersetze
den aktuellen Stand der Übergabe.
```
