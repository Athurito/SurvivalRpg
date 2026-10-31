# Inventory Refactor Plan

Stand: 2026-07-29, gekürzt am 2026-10-31.

Status: **Abgeschlossen.** Alle acht Phasen (97 von 97 Punkten) sind seit
Juli/August 2026 umgesetzt. Diese Fassung behält die verbindlichen
Leitplanken, das Addendum zum Player-Inventory-Pane und die Abschlussauswertung
mit Editor-Checkliste. Das vollständige Fortschrittsprotokoll der Phasen 0
bis 8 liegt in der [Git-Historie](https://github.com/Athurito/SurvivalRpg/blob/dcc0d5cb12556da7dee58fbc4ddf04fd976af160/docs/inventory-refactor-plan.md). Spätere Layout-Änderungen stehen
in der [UI-Roadmap](ui-roadmap.md) (UI-02, UI-08).

## Architektur-Leitplanken

- `URpgInventoryManagerComponent` bleibt die einzige autoritative
  Inventory-Wahrheit. Es entsteht kein paralleler Inventory-Manager.
- Item-Definitionen und Fragmente enthalten statische Daten; konkrete
  Item-Instanzen enthalten veränderlichen, replizierten und gespeicherten
  Zustand.
- Physische Equipment-Platzierung ist eine Inventory-Transaktion.
  Equipment/Loadout spiegelt oder aktiviert diesen Zustand, besitzt ihn aber
  nicht doppelt.
- Gameplay-Mutationen sind serverautoritativ und verwenden Item-IDs,
  Container-Handles und intentionale Requests statt roher UObject-Pointer.
- CommonUI besitzt Screen-Lifecycle, Layer, Fokus und Back-Navigation.
- MVVM projiziert Gameplay-Zustand read-only. Drag-Geometrie, Fokus und
  Rendering dürfen bewusst imperativ im Presenter/Widget bleiben.
- UMG-Assets zeigen im Designer die statische Hierarchie, die auch zur
  Laufzeit verwendet wird. Statische Screen-Hälften werden nicht zur Laufzeit
  ersetzt.

## Addendum 2026-07-29 – Wiederverwendbares Player-Inventory-Pane

Dieses Addendum ersetzt die älteren Aussagen der Phasen (Git-Historie), nach denen der
Player-Root selbst die Aggregate-VM und alle Player-Leaves besitzt oder
Storage/Crafting nur reduzierte Player-Gruppen darstellen.

- `CUI_PlayerInventoryPane` ist ein passives `URpgPlayerInventoryPaneWidget`
  und enthält vollständig Gear, Carry, Pockets, Backpack, Belt, Pouch,
  ResourceBag und Quickbar. Es besitzt genau eine stabile, native
  `URpgPlayerInventoryViewModel`-Projektion und keine Gameplay-Autorität.
- `CUI_PlayerInventory`, `CUI_StorageSpatial` und
  `CUI_CraftingStationSpatial` sind weiterhin eigenständige Activatable
  Root-Screens. Jeder Root besitzt genau einen Drag-/Interaction-Coordinator,
  einen Panel-Navigator, CommonUI-Actions, Back, Modals, Feedback und den
  screen-weiten `DragVisualCanvas`.
- Alle drei Roots betten dasselbe vollständige Pane ein. Player zeigt es
  zentriert; Storage und Crafting verwenden links das Player-Pane und rechts
  ihren unveränderten Storage- beziehungsweise Crafting-Kontext. Die alten
  `PlayerGroupsPanel`-Teilansichten und doppelten Player-VMs entfallen.
- Das Pane registriert ausschließlich stabile `Player.*`-Panel-IDs, meldet
  räumliche Grids und Non-Spatial-Drop-Ziele an den Root und verwendet nur
  lokale Layout-Koordinaten. Payload-Routing, Quick-Transfer-Policy und jede
  Mutation bleiben im Root und in `URpgInventoryUiActionComponent`.
- Storage behält seine bidirektionalen Access-Policy-geprüften
  Quick-Transfer-Routen. Crafting behält ausschließlich Output→Player. Das
  BaseTerminal und der Registry-Pfad von `CUI_PlayerInventory` bleiben
  unverändert.
- Das Pane ist auf maximal 920 Slate Units Breite begrenzt und wird über
  einen Down-Only-ScaleBox-Vertrag sowohl standalone als auch in den beiden
  gleich breiten Dual-Screen-Spalten verwendet.

## Verifikation bei Änderungen

- Relevanten `SurvivalRpgEditor`-Build ausführen.
- Inventory- und Equipment-Automationtests ausführen.
- Für replizierte Änderungen mindestens Server/Client, OwnerOnly versus
  actor-relevant Inventory und Late Join prüfen.
- Für UI-Änderungen wiederholtes Öffnen/Schließen, CommonUI-Pooling,
  Payload-Wechsel, Fokus, Gamepad und Mouse-Drag prüfen.
- Erst nach tatsächlich ausgeführter Verifikation Build-, Netzwerk- oder
  Editor-Korrektheit als bestätigt markieren.

## Abschlussauswertung (Phase 8)

### Abschlussbild und eindeutiger Verantwortungsfluss

Die verbindliche Linie des Systems lautet:

`Input/UI -> screen-scoped Presenter und Coordinator -> schmale UiAction-Commands -> synchrone Domain-Handler -> autoritativer Inventory-/Equipment-/Station-State -> Replikation und Gameplay-Messages -> read-only MVVM -> passive Widgets`

Damit gibt es keinen zweiten UI- oder MVVM-State-Owner. Im Einzelnen gilt:

| Bereich | Endgültige Verantwortung |
| --- | --- |
| `URpgInventoryManagerComponent` | Einzige serverautoritative und replizierte Inventory-Graph-Wahrheit. Besitzt FastArray, Item-Subobjects, Placements, Transaktionen, Revision, Mutation-Epoch und Persistenz. Die getrennten Storage-, Rules/Planner-, Transactions- und Persistence-TUs sind nur Implementierungsgrenzen. |
| `URpgInventoryUiActionComponent` | Controller-owned Netzwerkfassade für schmale Commands. Prüft Zugriff, Request-Snapshots, Replay und Feedback, besitzt aber keinen Gameplay-Zustand. Physische Use-, Split-, Move-, Transfer-, Equip- und Drop-Aktionen korrelieren über stabile IDs. |
| Domain-Handler | Führen synchrone Policy und Delegation für Transaction, Equipment, QuickAccess, Drop, BaseStorage und weitere Domänen aus. Sie replizieren nichts selbst und halten keine asynchronen Item-Pointer fest. |
| Layout, Inventory, Loadout und Equipment | Layout authoriert Container und Rollen. Inventory besitzt die physische Gear-/Carry-Platzierung. Loadout/Actionbar besitzen Auswahl und Bindings. Der Equipment-Manager besitzt remote-relevante Runtime-Instanzen und den GAS-Grant-Lifecycle, aber keine zweite physische Item-Platzierung. |
| ViewModels | Stabile, read-only Projektionen des autoritativen Zustands. Child-VMs bleiben pro Item-ID beziehungsweise Container-Handle stabil; FieldNotify und Invalidierung werden nur für tatsächlich geänderte Felder/Commits ausgelöst. |
| Presenter und Coordinator | Besitzen Screen-Lifecycle, Payload-Wechsel, Pooling-Bindings, Reconciliation, Selektion, Drag-Preview, Kontextmenü, Split-/Drop-Dialoge, Fokus und Request-Konstruktion. Sie sind keine Gameplay-Autorität. |
| CUI-/UMG-Assets | Besitzen statische Designer-Hierarchie, Slots, Styling, Animation und konfigurierte Widget-Klassen. Leaf-Widgets zeigen Daten an und leiten Nutzerintentionen weiter; sie schreiben keinen Inventory-State. |

### Woher die Widgets jetzt kommen

- `DA_RpgUIScreenRegistry` ist der eindeutige Einstieg für die vier
  refaktorierten Screen-Familien: PlayerInventory ->
  `CUI_PlayerInventory`, Storage -> `CUI_StorageSpatial`, BaseTerminal ->
  `CUI_BaseTerminalSpatial` und CraftingStation ->
  `CUI_CraftingStationSpatial`. Sie werden auf `UI.Layer.GameMenu` geöffnet;
  es gibt keinen stillen Legacy-Fallback.
- Persistente HUD-Elemente kommen weiterhin aus der Lyra-rooted
  Experience-/GameFeature-Komposition und werden über die vorgesehenen
  UIExtension-Slots registriert. Screen-Registry und UIExtension lösen damit
  unterschiedliche Lebenszyklen und sind keine konkurrierenden Widget-
  Fabriken.
- Das jeweilige CUI-Asset authoriert die statische Screen-Hierarchie. Native
  Presenter erzeugen nur die wirklich dynamischen Entries innerhalb dieser
  authored Parents. Statische Screen-Hälften werden nicht zur Laufzeit durch
  `CreateWidget` ersetzt.
- MVVM erzeugt keine Widgets. Es liefert eine read-only Datenprojektion an
  bereits authorierte oder vom Presenter erzeugte Widgets. Root-Screens
  stellen ihre screen-owned Source bereit; deklarative Leaves besitzen genau
  die vorgesehene PropertyPath- oder Manual-Source und keine parallelen
  Blueprint-Setter.
- Kontextmenü, Split-/Drop-Bestätigung, Drag-Visual, Preview und Fokus sind
  screen-owned Präsentation. Sie dürfen imperativ sein, solange die daraus
  entstehenden Gameplay-Änderungen ausschließlich über die schmalen Commands
  zur autoritativen Domäne laufen.

### Vorher-/Nachher-Auswertung

| Thema | Vorher | Jetzt |
| --- | --- | --- |
| Gameplay-Autorität | Single-Slot-Ursprung, Spatial-Erweiterungen, UI-Adapter und Legacy-Fassaden ließen mehrere scheinbare Wahrheiten entstehen. | Ein Inventory-Manager besitzt den Graph; Equipment, Loadout, Presenter und MVVM besitzen klar abgegrenzte Spiegel-, Aktivierungs- oder Präsentationsaufgaben. |
| Netzwerk und Sicherheit | Pointer-basierte oder generische Requests konnten veraltete Client-Annahmen und zu breite Mutationsflächen transportieren. | Physische Kernaktionen verwenden Stable-ID-Requests, vollständige erwartete Snapshots, serverseitige Access-/Authority-Prüfung und korreliertes Feedback. Replay-Caches revalidieren den aktuellen Zugriff; `NoAccess` transportiert nur stabile IDs und niemals einen Live-Item-Pointer. |
| Transaktionen | Move, Split, Transfer, Equip, Drop, Storage und Save/Load waren eng in großen Implementierungen und Compatibility-Adaptern vermischt. | Planner, atomare Commit-Pfade, Cross-Inventory-Transfer, Storage und Persistenz besitzen getrennte Implementierungsgrenzen bei unveränderter Autorität. |
| Equipment | Physische Platzierung, Loadout-Auswahl und Equipment-Runtime konnten wie konkurrierende Zustände wirken. | Inventory besitzt Gear/Carry, Loadout/Actionbar besitzen Auswahl/Bindings, Equipment besitzt Runtime/GAS; gemeinsame Placement-Policy verhindert widersprüchliche Regeln. |
| UI-Komposition | Native `CreateWidget`-Pfade, Blueprint-Hierarchie, Registry, UIExtension und MVVM wirkten wie gemischte Herkunftsquellen. | Registry öffnet Screens, UIExtension registriert persistentes HUD, CUI authoriert die statische Hierarchie, Presenter erzeugt dynamische Entries, MVVM liefert ausschließlich Daten. |
| MVVM und Lifecycle | Root-/Leaf-Sources, Blueprint-Setter und imperative Graphwriter konnten dieselben Daten schreiben; Pooling hinterließ leicht stale Bindings. | Eine stabile screen-owned Root-VM, exakte Leaf-Sources, selektives FieldNotify und vollständiges Unbind/Reset bei Deaktivierung oder Pool-Release. Carry-Rebind und Gruppenverlust löschen nur das eigene Preview-Target und lassen fremde Session-Previews unberührt. |
| Editor-Verständlichkeit | Große Aggregate, Umbrella-Includes, tote Assets und unsichtbar erzeugte Widgets erschwerten Navigation und WYSIWYG. | Klassen sind dateiweise auffindbar, die relevante statische Hierarchie ist im Designer sichtbar und aktive/retired Assets sind durch Registry- und Manifesttests eindeutig. |
| Wartbarkeit | Legacy-Snapshot, globale Sort-/Index-Pfade, Wrapper und breite RPC-Fassade vervielfachten Änderungsorte. | Alte Oberflächen sind entfernt, Domain-Handler und Manager-TUs sind schmaler, und kanonische Commands besitzen gezielte Verträge. |
| Testabdeckung | Viele lokale Unit-Verträge, aber wenig Schutz für Asset-Herkunft, Pooling, Request-Replay und Packaging. | Fokussierte Use-/Split-/UiAction-/Carry-Tests, 154 Inventory- und 209 projektweite Tests, Blueprint-Compile, Asset-/Manifest-Gates, Cook/Package und packaged Smokes sind grün. |

### Entfernte Legacy-Pfade

- Alte Snapshot-Rekonstruktion samt Converter und zugehörigen Tests sowie die
  dauerhaften Global-Sort-/Index-Reorder-Oberflächen.
- Der deprecated Slot-Container-Provider, rohe BlueprintCallable-
  Manager-Mutatoren, alte M1/M2/M3-/UQ-/DroppedActor-Fassaden und 18
  Compatibility-RPCs der UiAction-Fassade.
- Sieben Compatibility-Umbrella-Header und sechs große UI-Aggregate. Die
  aktuellen Klassen leben in ihren eigenen Dateien; die alten Include-Pfade
  sind kein unterstützter Vertrag mehr.
- Legacy-`CUI_Inventory`/TileView/SlotEntry, alte Storage-, BaseTerminal- und
  Crafting-Screens/Leaves, `CUI_AddressSlotEntry`, `CUI_Hotbar_Old`,
  `CUI_ActionBar_Old`, `BP_DragOperation` und `EQ_TestShield`.
- Die Orphan-/Test-Assets `BP_TestDrop`, `ID_Cheese` und `ID_TestSword1`.
- Der `Testing`-Graph in `BP_Rpg_PlayerController` sowie ungenutzte
  Spatial-Item-VM-Member, leere Setter-Graphwriter und wirkungslose alte
  Drag-State-Events.

### Bewusst beibehaltene Migrationen und Save-Grenze

> Nutzerentscheidung vom 26.10.2026 (`AGENTS.md`): Bestehende Saves müssen nicht
> kompatibel bleiben; neue Migrationen werden nicht gebaut. Die Felder unten
> sind bestehende Migrationen, keine Vorgabe für neue.

- `FRpgInventorySlotAddress::ContainerId_DEPRECATED` bleibt ausschließlich
  für die Schema-1-Adressmigration erhalten. Es ist keine neue Runtime-
  Autorität und darf in aktuellem Content nicht authoriert werden.
- `CarryRole_DEPRECATED` bleibt für die bestehende Carry-/QuickAccess-
  Schema-Migration erhalten.
- Der Legacy-Core-StatTags-Payload in Fragment-Schema v1 einschließlich
  `LegacyCoreStatTagsPayloadVersion` bleibt als gezielte Datenmigration
  erhalten. Diese drei Felder sind absichtliche Migrationen, keine
  versehentlich übersehene Compatibility-Schicht.
- Die C++-Dateischnitte benötigen keine CoreRedirects, weil reflektierte
  Namen und Packages gleich geblieben sind. Gelöschte Legacy-Assets werden
  ebenfalls nicht über Redirectors künstlich am Leben gehalten.
- Sehr alte Root-only-Saves, die nie in das aktuelle Spatial-/Schemaformat
  migriert wurden, sind im finalen Build nicht direkt kompatibel. Falls
  solche produktiven Saves existieren, müssen sie einmal mit einer älteren,
  migrationsfähigen Projektversion geladen und neu gespeichert oder durch
  ein separates Offline-Migrationswerkzeug konvertiert werden. Vor Release
  ist bewusst zu entscheiden, ob diese Alt-Saves unterstützt oder verworfen
  werden.

### Bekannte Grenzen und verbleibende technische Schuld

- Die Request-Replay-Caches halten jeweils höchstens 64 kürzliche Einträge.
  Use, Split und die weiteren geschützten Commands sind innerhalb dieses
  Fensters gegen identische Retries und RequestId-Payload-Kollisionen
  abgesichert; das ist keine dauerhafte Exactly-once-Garantie nach Eviction,
  Prozessneustart oder Save/Restore.
- Use, Split, Equipment, ExactTransfer, QuickTransfer und ManualDrop
  revalidieren vor einem Replay den aktuell benötigten Inventory-Zugriff.
  Wird er entzogen, überschreibt ein redigiertes `NoAccess` den alten
  Cache-Eintrag dauerhaft; auch nach erneutem Zugriff kann derselbe Request
  keinen zuvor autorisierten Live-Item-Pointer wiederbeleben.
- Nicht jeder Feature-Command besitzt bereits einen eigenen Request-Struct.
  Station-, Building- und Crafting-Objektreferenzen werden serverseitig
  validiert. `RequestDepositMaterialStackToBase` ist die verbliebene direkte
  Item-Pointer-Ausnahme und sollte bei der nächsten Änderung dieser Domäne
  auf denselben Stable-ID-/Snapshot-Vertrag migriert werden.
- `CanBootstrapItemInstance` ist für den Inventory-Zustand read-only, kann bei
  fremden detached Items aber ein transientes UObject/GUID erzeugen und
  Fragment-Hooks ausführen. `BlueprintPure` bedeutet hier nicht globale
  Hook-Purity.
- Fragment-Hooks besitzen noch Revalidation-/Lifetime-Schuld. Insbesondere
  können externe Hook-Seiteneffekte zwischen Planung und Commit auftreten;
  außerdem können synchrone Listener den neuen Graph kurz vor der neuen
  Revision sehen. Diese Semantik wurde nicht verdeckt im Refactor geändert.
- Die Subobject-Replikationsbedingung wird beim Aufbau festgelegt.
  `SetReplicationPolicy` nach begonnenem Replication-Lifecycle ist kein
  unterstützter dynamischer Umschaltpfad und kann sonst Condition-Drift
  erzeugen.
- Die automatisierten Tests beweisen Autoritäts-, Snapshot-, Delta-,
  Lifecycle- und Reflection-Verträge in Standalone-Prozessen. Sie sind kein
  echter Zwei-Client-ActorChannel-/FastArray-Nachweis für OwnerOnly versus
  ActorRelevant und kein Late-Join-Nachweis.
- Interaktives Mouse-/Gamepad-PIE, CommonUI-Fokus/Pooling unter realer Eingabe,
  Drag-and-drop und die Dialogbedienung wurden im finalen Gate nicht manuell
  ausgeführt. Diese Punkte stehen deshalb unten als empfohlener manueller
  Playtest und werden nicht als verifiziert behauptet.

### Editor-/Blueprint-Checkliste

#### Pflicht

- Vor der ersten Editor-Sitzung auf diesem Stand Unreal Editor und Live
  Coding vollständig schließen, den C++-Stand bauen und anschließend den
  Editor neu öffnen. Keine alten Hot-Reload-Klassen im Prozess behalten.
- `BP_Rpg_PlayerController` kompilieren und prüfen: kein `Testing`-Graph,
  keine Nodes der entfernten UiAction-RPCs. `CUI_SpatialInventoryItem`
  kompilieren und prüfen: keine eigenen VM-Variablen, keine entfernten
  Setter-/Drag-State-Events. Beide Assets liegen bereits resaved vor; neue
  Editorarbeit darf diese Legacy-Pfade nicht wieder erzeugen.
- In `DA_RpgUIScreenRegistry` die vier exakten Zuordnungen
  `CUI_PlayerInventory`, `CUI_StorageSpatial`,
  `CUI_BaseTerminalSpatial` und `CUI_CraftingStationSpatial` auf
  `UI.Layer.GameMenu` beibehalten. Keine Legacy-Fallback-Klasse eintragen.
- In `CUI_PlayerInventory` genau die native screen-owned
  Getter-/PropertyPath-Source verwenden. Keine zusätzliche Root-Manual-Source
  und keine Leaf-Datenbindings am Root authorieren.
- Bei ActionBar-, Address-, Gear-, Carry- und BaseResource-Leaves die jeweils
  vorgesehene exakte Manual-/PropertyPath-Source und die deklarativen
  Blueprint-MVVM-Bindings beibehalten. Keine parallelen `SetText`, SetItem
  oder Inventory-Mutationsgraphen ergänzen.
- In den Screen-Assets die authorierten Widget-Klassen für Kontextmenü,
  Split-, Drop- und Drag-Visual prüfen; `DragVisualCanvas` muss im statischen
  WidgetTree vorhanden bleiben. Dynamische Entries gehören ausschließlich
  unter ihre dafür authorierten Parent-Container.
- CommonUI-Back-Actions, Input-Konfiguration, initialen Fokus und den
  vorgesehenen `GameMenu`-Layer je Screen prüfen, wenn ein Screen-Asset
  verändert wird. Gameplay-Input gehört nicht in die Leaf-Widgets.
- Gelöschte Legacy-Assets und Redirectors nicht einchecken. Vor einem Release
  in der Asset-Registry beziehungsweise im Reference Viewer prüfen, dass die
  retired Packages weiterhin ohne Consumer sind.
- Vor Release eine ausdrückliche Produktentscheidung für sehr alte Root-only-
  Saves treffen und gegebenenfalls die oben beschriebene Einmalmigration
  durchführen.
- Nach jeder weiteren Inventory-/UI-Assetänderung dieselbe Gate-Reihenfolge
  verwenden: Editor-Build -> ForceUnity-Build -> Development-Game-Build ->
  `CompileAllBlueprints` -> fokussierte Tests -> `SurvivalRpg.Inventory` ->
  `SurvivalRpg` -> sauberer Cook/Package -> Manifest-Gate -> packaged Smokes.

#### Empfohlen

- PlayerInventory, Storage, BaseTerminal und Crafting mehrfach öffnen und
  schließen, Payload/Target wechseln und CommonUI-Pooling, Back-Navigation,
  Fokuswiederherstellung und stale Auswahl/Preview beobachten.
- Mouse und Gamepad interaktiv prüfen: Move, Rotate, Split, Use, Equip,
  QuickTransfer, Kontextmenü, Drop-Bestätigung, Drag-Abbruch und Drag über
  ungültige beziehungsweise belegte Zellen.
- Zwei-Client-PIE mit Serverautorität durchführen: OwnerOnly-Inventory gegen
  actor-relevantes Equipment vergleichen, FastArray-/Subobject-Transport
  beobachten und einen Late Join nach mehreren Move-/Split-/Equip-
  Transaktionen testen.
- Die gelöschten Assets im Source-Control-Review noch einmal als bewusste
  Deletions kontrollieren.

#### Keine manuelle Änderung nötig

- Für die Aufteilung der C++-Aggregate sind keine CoreRedirects, Blueprint-
  Reparents oder Class-Resaves nötig; die reflektierten Typnamen blieben
  stabil.
- Keinen zweiten Inventory-Manager, Widget-State-Store oder MVVM-Mutator
  anlegen. Gameplay-Wahrheit bleibt im bestehenden Lyra-rooted
  `URpgInventoryManagerComponent`-/Equipment-Pfad.
- `CUI_SpatialInventoryItem` nicht wieder mit einer eigenen Item-VM versehen.
  Es erhält sein Modell und seine Interaktionskonfiguration vom Presenter.
- Statische Screen-Hälften nicht per `CreateWidget` ersetzen. Ihre Hierarchie
  bleibt im CUI-Asset; nur dynamische Entries werden nativ reconciled.
- Entfernte Legacy-Screens, Umbrella-Includes, Snapshot-/Sort-Fassaden,
  Controller-Debuggraphen und Testitems nicht als vermeintliche
  Compatibility-Lösung wiederherstellen.

### Exakt ausgeführte Abschlussverifikation

Der große Code-/Asset-Schnitt ist im verifizierten Zwischencommit `62e1d621`
gesichert. Die folgenden Gates wurden mit Unreal Engine 5.8 auf dem darauf
aufbauenden Abschlussstand tatsächlich ausgeführt:

| Gate | Ergebnis |
| --- | --- |
| Adaptiver `SurvivalRpgEditor Win64 Development`-Build | Erfolgreich. |
| `SurvivalRpgEditor Win64 Development -ForceUnity -DisableAdaptiveUnity` | Erfolgreich. |
| Sauberer `SurvivalRpg Win64 Development`-Game-Build | Erfolgreich. |
| `CompileAllBlueprints -ProjectOnly` | 0 Fehler, 0 fehlgeschlagene Blueprints; 2 Compilerwarnungen und 7 Warnungen insgesamt. |
| `SurvivalRpg.Inventory.UiActions` | 2 von 2 Tests erfolgreich. |
| `SurvivalRpg.Inventory.UseRequest.ExactlyOnceEndToEnd` | 1 von 1 Test erfolgreich; die Garantie bleibt bewusst auf das 64er-Replay-Fenster begrenzt. |
| `SurvivalRpg.Inventory.UseRequest.AuthorizationReplayRedaction` | 1 von 1 Test erfolgreich; Access-Entzug und erneuter Zugriff bleiben `NoAccess` und pointerfrei. |
| `SurvivalRpg.Inventory.Transfer.AuthorizationFeedbackRedactionAndReplay` | 1 von 1 Test erfolgreich; Move, ExactTransfer, Split, Equipment, ManualDrop sowie QuickTransfer-Source/-Target sind abgedeckt. |
| `SurvivalRpg.Inventory.UI.CarrySlot` | 2 von 2 Tests erfolgreich; Pooling, Rebind, Gruppenverlust und Preview-Ownership sind abgedeckt. |
| `SurvivalRpg.Inventory.Transaction.ExactHalfSplitAndIdempotency` | 1 von 1 Test erfolgreich. |
| `SurvivalRpg.Inventory` | 154 von 154 Tests erfolgreich. |
| `SurvivalRpg` | 209 von 209 Tests erfolgreich. |
| `SurvivalRpg.UI.Indicator.CompositionAssets` nach dem ActionSet-Fix | 1 von 1 Test erfolgreich; Scanregel, nativer Typ, `AlwaysCook` und Primary-Asset-ID-Auflösung sind abgedeckt. |
| Sauberer `BuildCookRun` für die finalen Maps | 948 Packages, `BUILD SUCCESSFUL`. |
| Cook-/Package-Manifest-Gate | 11 erforderliche Packages in `Manifest_UFSFiles_Win64.txt` und `ReferencedSet.txt` vorhanden; 19 retired Packages in beiden Dateien abwesend. |
| Packaged Smoke `BootMenu`/`MainMenu` | Sauberer Exit, keine harten Fehler. |
| Erneuter Clean-`BuildCookRun` nach dem ActionSet-Fix | Editor- und Game-Target frisch gebaut, 948 Packages, Stage und Archive erfolgreich. |
| Packaged Smoke `Lvl_ThirdPerson` | Sauberer Exit, keine harten Fehler; Experience, Game Features und HUD wurden vollständig aktiviert. Die frühere `RpgExperienceActionSet:LAS_Rpg_StandardUI`-Warnung tritt nicht mehr auf. |

`LAS_Rpg_StandardUI` wird jetzt über den dedizierten Verzeichnis-Scan für
`RpgExperienceActionSet` registriert. Damit kann der Experience Manager das
ActionSet im gepackten Build über seine Primary-Asset-ID auflösen und dessen
Client-/Server-Bundles zuverlässig laden. Dafür sind keine manuellen
Blueprint- oder Editor-Anpassungen nötig.

Der Manifestvertrag bestätigt insbesondere die aktiven Assets
`CUI_SpatialInventoryItem`, `CUI_SpatialInventoryGrid`, `CUI_CarrySlot`,
`CUI_PlayerInventory`, `CUI_StorageSpatial`, `CUI_BaseTerminalSpatial`,
`CUI_BaseResourceListSpatial`, `CUI_CraftingStationSpatial`,
`DA_RpgUIScreenRegistry`, `DA_RpgGameData` und `BP_Rpg_PlayerController`.
Abwesend sind die 19 retired Packages `CUI_AddressSlotEntry`,
`EQ_TestShield`, `BP_DragOperation`, `CUI_Hotbar_Old`, `CUI_ActionBar_Old`,
`CUI_Storage`, `CUI_StorageContainer`, `CUI_BaseTerminal`,
`CUI_BaseResourceList`, `CUI_CraftingStation`,
`CUI_CraftingIngredientEntry`, `CUI_CraftingJobEntry`,
`CUI_CraftingRecipeEntry`, `CUI_CraftButtonBase`, `CUI_Inventory`,
`CUI_InventorySlotEntry`, `BP_TestDrop`, `ID_Cheese` und
`ID_TestSword1`.

Nicht Bestandteil dieser automatisierten Abschlussverifikation waren
interaktives Mouse-/Gamepad-PIE, ein echter Zwei-Client-FastArray-
ActorChannel-Lauf, OwnerOnly-versus-ActorRelevant-Verifikation und Late Join.
Diese Grenzen sind ausdrücklich offengelegt und nicht als grün behauptet.
