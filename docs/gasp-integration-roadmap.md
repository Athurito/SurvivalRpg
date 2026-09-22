# GASP-Integration: Roadmap und Einstieg für neue Chats

Stand: **22.09.2026**. Diese Datei ist der zentrale Arbeitsplan für die weitere
GASP-/Lyra-Integration. Sie ersetzt nicht die [Spielvision](game-vision.md) oder
die technischen Einzelberichte. Der Umfang ist Bewegung und ihre RPG-Anbindung,
nicht eine neue Gesamtplanung für Combat, Crafting oder Portale.

## Schnellstart

- **Nächster Schritt: `GASP-02` – gezielte Stabilisierung.** `GASP-01` ist nach Nutzer-Sichtabnahme am 22.09.2026 mit [PR #144](https://github.com/Athurito/SurvivalRpg/pull/144) gemergt. Keine Implementierungsaufgabe ist aktuell beansprucht.
- Aktive Aufgabe, Branch, letzte Ergebnisse und konkrete Fortsetzung stehen in
  [gasp-integration-handoff.md](gasp-integration-handoff.md).
- Letzter akzeptierter Runtime-Stand: [PR #144](https://github.com/Athurito/SurvivalRpg/pull/144),
  am 22.09.2026 nach manuellem Sichttest auf `master` gemergt.
  Merge: `aa4447d69d3187dec3592913a1f683b5c91c0a7b`, getesteter Runtime-Commit: `b78edf5b`.
  Der finale PR-Head `159521b3` ergänzt ausschließlich Dokumentation zu diesem Runtime-Stand.
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
| Mover-Ragdoll | Noch keine integrierte RPG-Experience/PawnData. Die Assetbasis enthält unter `Mover/Ragdoll` bisher nur die referenzierte Input-Struktur; die vollständige Pawn-Abhängigkeit ist neu zu prüfen | [Assetumfang](gasp-asset-foundation.md) |

Die Bezeichnung „drei Varianten“ meint **CMC, Mover und Mover-Ragdoll**. Bereits
vorhandene Baseline-/CMC-Test-Experiences bleiben bestehen; daraus folgt keine
Vorgabe, insgesamt genau drei Experience-Dateien zu besitzen.

## Reihenfolge und Status

| ID | Arbeitspaket | Status | Voraussetzung / Abschluss |
| --- | --- | --- | --- |
| `GASP-01` | Grounded Hurdle für Mover | **Gemergt** | [PR #144](https://github.com/Athurito/SurvivalRpg/pull/144), Merge `aa4447d6`; Runtime `b78edf5b`; Editor/Game, 47 Tests und zwei Prozessläufe bestanden; Nutzer-Sichtabnahme am 22.09.2026; [Bericht](gasp-mover-hurdle.md) |
| `GASP-02` | Gezielte Stabilisierung | Offen | Kleine getrennte Aufgaben aus dem Register unten; vor Erweiterung um Ragdoll bewerten |
| `GASP-03` | Mover-Ragdoll-Experience | Geplant | `GASP-01`; belastbarer Lifecycle-Stand aus `GASP-02`; Source-Audit zuerst |
| `GASP-04` | Vergleich der drei Varianten | Geplant | `GASP-03`; dokumentierte gemeinsame Abnahmematrix |
| `GASP-05` | Importbereinigung | Geplant | `GASP-04`; geprüfte Abhängigkeiten und konkrete Entfernungsliste |
| `GASP-06` | Blocken beim Laufen / RPG-Animationsfeinschliff | Zurückgestellt | Eigener späterer Auftrag; bestehende Zurückstellung respektieren |

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
einem PR zu bearbeiten. **Alle folgenden Punkte sind offen.** Die Priorität
innerhalb dieses Schritts beginnt bei den Lifecycle-Punkten.

| ID | Einordnung | Arbeit und Abschlussnachweis |
| --- | --- | --- |
| `GASP-STAB-01` | Beobachteter Cleanup-Fehler | Block-Cleanup greift beim Equipment-/Attributabbau auf bereits entferntes `BlockAngleDegrees` zu. Reihenfolgefesten, wiederholbar sicheren Cleanup herstellen; normale Block-Freigabe und wiederholtes Entfernen testen. |
| `GASP-STAB-02` | Ungeklärtes Laufzeit-Symptom | Gelegentlich Falling mit Geschwindigkeit null nach Respawn. Fehlenden Übergang mit Eingabe-, Movement- und Lifecycle-Daten erfassen; Ursache und gezielten Nachweis liefern. Später grüne Wiederholungen sind allein kein Fix. |
| `GASP-NET-01` | Korrektur-Nachweislücke | Beim aktiven Mantle kann normales Warping die injizierte Abweichung entfernen, bevor der Test die relevante Korrektur erfasst. Echten Rollback über den betroffenen aktiven Frame nachweisen; Warp-/Collider-Vertrag nicht abschwächen. |
| `GASP-NET-02` | Ungeklärte Zustandsabweichung | Ein Lauf zeigte vorhergesagtes `Finished` gegenüber autoritativem `Cancelled`. Ursache und zulässige terminale Semantik klären. Ein Wiederholungslauf mit gleichem Grund auf beiden Seiten löst diese Beobachtung nicht. |
| `GASP-NET-03` | Gemessene Paketverlust-Grenze | Nach 350 ms Paketpause stehen Animation und Position gemeinsam still, beim Aufholen bleibt an einer Ebene eine Abweichung von etwa 71 ms durch die rekonstruierte Bahn. Gezielt bewerten/verbessern; keine pauschale perfekte Pose-/Hindernisübereinstimmung bei Verlust behaupten. |
| `GASP-VAL-01` | Messgrenze | Im finalen separaten Prozesslauf ist ein Walking→Traversing-Messpaar nicht numerisch vergleichbar. Die erste aktive Traversing-Probe enthält bereits die Montage. Bei Bedarf Onset-Messung verfeinern; keinen belegten Animationsaussetzer daraus ableiten. |
| `GASP-VAL-02` | Noch nicht ausgeführte Umgebungsprüfung | Cooked/packaged und WAN bzw. gezielt emulierte Netzwerkbedingungen prüfen: verschiedene Render-FPS, Delay/Jitter/Loss, Late Join, Traversal, Korrekturen und Respawn. Lokale uncooked Ergebnisse ersetzen diese Prüfung nicht. |
| `GASP-VAL-03` | Noch nicht gezielt ausgeübte Reihenfolge | Traversal B trifft ein, während A noch im Präsentationspuffer liegt, nach einer gewöhnlichen Montage-Ersetzung. Play-Token-Korrelation ist implementiert; der bisherige Replay-Test wartet vor B auf Cleanup und beweist diesen engeren Fall nicht. |

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

Danach eine eigene Experience/PawnData-Variante aufbauen. Bestehende Experiences
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

## GASP-06 – Zurückgestellter Feinschliff

Stationäre Beine beim Blocken und gleichzeitigem Laufen sind vom Nutzer
vorerst akzeptiert, sowohl bei CMC als auch Mover. Später in einem eigenen
Combat-/AnimGraph-Schritt Körperaufteilung und Montage-/Locomotion-Überblendung
prüfen. Weitere Skeletons, visuelle Griff-/Socket-Anpassungen, Last-/Stamina-
Tuning und schwereres Traversal-Gefühl sind eigene spätere Entscheidungen.
Sie gehören nicht automatisch zu Hurdle oder zur Ragdoll-Basis.

## Verbindliche Arbeitsweise über mehrere Chats

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
öffne einen PR zur Prüfung. Merge nicht ohne meinen Auftrag.
```

Einen begonnenen Schritt fortsetzen:

```text
Setze die in docs/gasp-integration-handoff.md aktive Aufgabe fort.
Lies zuerst die Roadmap, den angegebenen Branch/PR und die letzte Übergabe.
Prüfe, was bereits implementiert und tatsächlich getestet wurde. Fahre beim
genannten nächsten Handgriff fort und aktualisiere am Ende die Übergabe.
```
