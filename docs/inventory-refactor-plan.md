# Inventory Refactor Plan

Stand: 2026-07-19

Dieses Dokument hält die verbindliche Reihenfolge für die Bereinigung des
Tarkov-artigen Spatial Inventory fest. Es dient als Fortschrittsliste über
mehrere Codex-Aufgaben hinweg.

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

## Phase 0 – Safety-Net und bestätigte Fehler

Status: **In Arbeit**

- [x] Generischen Mutation-RPC auf sichere lokale Operationen begrenzen;
      physisches Drop/Pickup/Transfer nur über eigene Intents.
- [x] Sort-Commit darf einen fehlgeschlagenen `ApplySort` nicht als Erfolg
      melden.
- [x] Idempotency-/Request-Result-Cache bei Erfolg und Ablehnung begrenzen.
- [x] Gepoolten Storage-Screen bei Payload-Wechsel und Deaktivierung vollständig
      zurücksetzen.
- [x] Inventory-, Storage-, Crafting- und BaseTerminal-Screens auf
      `UI.Layer.GameMenu` verschieben.
- [x] Gemeinsame Equipment-Placement-Policy für Layout, Planner, Loadout und
      UI-Preview einführen.
- [x] `Split` gegen denselben Equipment-/Carry-Placement-Vertrag wie Move und
      Equip validieren.
- [x] Leere `AllowedSlots` eindeutig als nicht ausrüstbar behandeln.
- [x] Collect-Autoequip, Starter-Equipment und Provider-Unequip über physische
      Inventory-Moves führen; Loadout bleibt Spiegel/Aktivierung.
- [x] Provider-Unequip mit der erwarteten Item-ID gegen veraltete UI-Requests
      absichern und item-owned Container-Inhalte beim Move erhalten.
- [x] Verbotene Equipment-/Carry-Ziele im Planner eindeutig als
      `ItemNotAllowed` melden und im UI als ungültigen Slot darstellen.
- [x] `BothHands` unabhängig von veralteten Asset-Zulassungen in `OffHand`
      verbieten; Default-Slot nur aus tatsächlich gültigen Slots ableiten.
- [x] Partielle Cross-Inventory-Transfers erhalten Runtime-Instanz und
      `EntryId` des überlebenden Quellstacks sowie die Identität unbeteiligter
      Zielitems; aktive Hand-Zuweisungen bleiben dadurch gültig.
- [x] Regressionstests für die in diesem Schnitt behobenen Fehler ergänzen.

Verifizierter Zwischenstand vom 2026-07-19:

- `SurvivalRpgEditor Win64 Development` mit Unreal Engine 5.8 gebaut.
- `SurvivalRpg.Inventory`: 52 von 52 Automationtests erfolgreich.
- `SurvivalRpg.Inventory.UI`: 19 von 19 Automationtests erfolgreich
  (im vollständigen Inventory-Lauf enthalten).
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 10 von 10 Automationtests erfolgreich, einschließlich
  der aktiven Crafting-, BaseTerminal-, Loot-, Storage- und
  Player-Inventory-Registry-Verträge.
- Die aktiven Screen-Registry-Einträge wurden in frischen Editor-Prozessen
  gegen `UI.Layer.GameMenu` und ihre exakten authored Screen-Klassen
  validiert.
- Noch offen in Phase 0: Server/Client-, Late-Join- und interaktive
  Editor-/Gamepad-Prüfung.
- Definitionlose Slot-Provider besitzen vorläufig einen ausdrücklich
  dokumentierten Legacy-Fallback. Dieser wird erst nach Asset-Migration und
  Data Validation entfernt.

Bekannte Restpunkte, bereits einer späteren Phase zugeordnet:

- Abstrakte Equipment-Slot-Previews prüfen noch nicht in jedem Pfad dieselbe
  konkrete Belegung, Swap- und Handkonfliktlage wie der Transaction-Planner
  (Phase 1/2).
- Legacy-Snapshot-Import und einzelne Auto-Placement-Pfade konsumieren noch
  nicht den vollständigen Placement-Vertrag (Phase 2/4).
- Cross-Inventory-Transfers erhalten jetzt die Runtime- und Entry-Identität
  bestehender Items, importieren aber weiterhin beide Vollgraphen und senden
  dadurch noch unnötige Remove-/Add-Nachrichten sowie Subobject-
  Registrierungs-Churn (Phase 3).
- Collect-/Starter-Grant-Pfade sind jetzt physisch korrekt geroutet, benötigen
  aber noch eigene Ende-zu-Ende-Tests für Vollbelegung und Rollback (Phase 2).
- Item-owned Container dürfen nicht anhand ihrer lokalen `FName`-ID wie
  statische Gear-/Carry-Root-Slots normalisiert werden (Phase 4).
- Schema-v1-Saves können Platzierungen enthalten, die vor der gemeinsamen
  Equipment-Policy nur durch Kategorieprüfung zulässig waren. Diese dürfen
  nicht stillschweigend verworfen werden, sondern brauchen einen
  versionierten Import-/Migrationspfad (Phase 4).
- `BothHands + OffHand` wird zur Laufzeit bereits abgelehnt; die entsprechende
  Asset-Kombination braucht zusätzlich Data Validation (Phase 5).
- Die bisherigen Storage-BindWidget-Warnungen sind durch den
  screen-spezifischen Presentation-Hook beseitigt. Der fokussierte
  Storage-Lifecycle-Lauf ist ohne diese Warnungen erfolgreich.

Bestätigte Asset-/Migrationsbefunde:

- Kein aktuell gefundenes Equipment-Asset serialisiert absichtlich leere
  `AllowedSlots`; die neue Disabled-Semantik verursacht daher nach heutigem
  Asset-Stand keinen direkten Bruch.
- `EQ_TestShield` erbt derzeit `MainHand` und wirkt semantisch falsch bzw.
  verwaist. `ID_TestSword1` bleibt als `Misc` aus Weapon-Slots ausgeschlossen.
- `ED_BasicSword` erlaubt `OffHand`, während das aktuelle ShieldSlot-Layout
  dort nur `Shield` akzeptiert; diese Asset-Regel ist effektiv unerreichbar.
- `ID_BasicTwoHandedSword` besitzt für den Player-Pfad weder Weapon-Traits noch
  ein Spatial-Fragment. Der Enemy-Loadout-Pfad kann weiterhin gültig sein.
- Diese Assets werden nicht blind geändert. Phase 5 ergänzt Cross-Asset
  Data Validation; Phase 7 entscheidet anschließend referenzbasiert über
  Korrektur, Migration oder Löschung.

## Phase 1 – UI Composition und Editor-WYSIWYG

Status: **In Arbeit**

Verbindliche UI-Verantwortungslinie:

- Die Screen Registry wählt per `UI.Screen.*` ausschließlich die
  `UCommonActivatableWidget`-Klasse und den CommonUI-Layer.
- CommonUI besitzt Aktivierung, Deaktivierung, Pooling, Fokus und
  Back-Navigation.
- Das jeweilige `CUI_*`-Asset besitzt die statische, im Designer sichtbare
  Widget-Hierarchie.
- Der native Screen-Presenter validiert Payloads und verbindet die
  authorierten Leaves; er erzeugt keine statischen Screen-Hälften zur Laufzeit.
- MVVM projiziert Gameplay-State read-only in stabile Leaf-ViewModels.
- Ein screen-scoped Coordinator besitzt Drag/Hold, Preview, Quick-Transfer und
  Panel-Navigation. Widgets und ViewModels mutieren kein Inventory direkt.

Aktuelle native Screen-Hierarchie:

```text
URpgInventoryControllerActionsWidget
└─ URpgInventoryInteractionScreenWidget
   ├─ URpgPlayerInventoryWidget
   ├─ URpgStorageInventoryWidget
   ├─ URpgBaseTerminalWidget
   └─ URpgCraftingStationWidget

Authorierte, passive Composition-Leaves:
CUI_BaseTerminalSpatial
├─ CUI_SpatialInventoryPane (URpgInventorySpatialPaneWidget)
└─ CUI_BaseResourceListSpatial (URpgBaseResourceListWidget)

CUI_CraftingStationSpatial
├─ CUI_SpatialInventoryPane (Output)
├─ CUI_CraftingRecipeEntrySpatial
├─ CUI_CraftingIngredientEntrySpatial
└─ CUI_CraftingJobEntrySpatial
```

`URpgInventoryInteractionScreenWidget` besitzt genau einen Drag-/Interaction-
Coordinator und genau einen Panel-Navigator mit dem Screen als UObject-Outer.
Der Player-Screen besitzt ausschließlich seinen Gear-/Carry-/Content-/
Actionbar-Vertrag. Der Storage-Screen besitzt ausschließlich Payload,
aggregierte Player-Gruppen, Secondary-Grid und bidirektionales Quick-Transfer.
Der BaseTerminal-Screen besitzt Payload, Navigation, Transfer-/Action-Policy
und seine BaseStorage-Projektion. Der Crafting-Screen besitzt Station- und
Output-Payload, Crafting-Actions, drei typisierte Listenprojektionen und eine
ausdrückliche Output→Player-Transferroute. Der Spatial-Pane besitzt dagegen
nur genau ein authoriertes Grid, einen stabilen pane-owned Panel-ViewModel und die
Bindung an einen expliziten Container-Handle. Er besitzt weder Screen-
Lifecycle oder Payload-Policy noch Gameplay-Mutationen.
Es gibt bewusst keine spekulative allgemeine Dual-Inventory-Basis.

- [x] Blueprint-basiertes Primary Game Layout mit sichtbaren und validierten
      `Game`, `GameMenu`, `Menu` und `Modal` Stacks erstellen.
- [x] `CUI_RpgPrimaryGameLayout` als statische Composition Authority in der
      UI-Policy konfigurieren und alle vier Layer per `BindWidget` erzwingen.
- [x] Frisches, graphfreies Spatial-Storage-Asset direkt mit
      `PlayerGroupsPanel` und `SecondaryInventoryGrid` authoren; das alte
      `CUI_StorageContainer` bleibt bis zur Verifikation als Rollback bestehen.
- [x] Runtime-Replacement, magische Widget-Namen, harte Content-Pfade und
      Storage-Sonderfall im Screen-Router entfernen.
- [x] Storage-Payload bei inaktivem Screen nur stagen und über einen einzigen
      Bindepunkt genau einmal pro Aktivierung beziehungsweise aktivem
      Kontextwechsel anwenden. Dabei das Player-Inventar gegen die kanonische
      PlayerState-Instanz prüfen und identische Primary-/Secondary-Inventare
      ablehnen.
- [x] Gear, Carry und Content über einen screen-scoped
      Drag-/Interaction-Coordinator und denselben Preview-Vertrag anbinden.
- [x] Für Storage und BaseTerminal jeweils einen expliziten
      Payload-/Activation-Vertrag verwenden.
- [x] Crafting auf denselben expliziten Payload-/Activation-Vertrag bringen.
- [x] Player- und Storage-Screen-Verträge trennen: nur die tatsächlich
      gemeinsame Interaction-Screen-Basis extrahieren und Player/Storage als
      Geschwister modellieren, statt Storage vom vollständigen Player-Screen
      abzuleiten.
- [x] `UI.Screen.Inventory` samt Screen Registry als einzige
      Composition Authority für das Player-Inventar verwenden und die direkte
      Einbettung von `CUI_PlayerInventory` in `CUI_GameMenu` entfernen.
- [x] Den direkten Legacy-`Y`-Key-Pfad und den unverbundenen
      `IA_UI_Inventory`-Blueprint-Event entfernen. Inventory wird jetzt über
      `InputTag.UI.Inventory` in `DA_InputConfig`, die Pawn-Inputbindung und
      den vorhandenen Controller-InputRouter zum Registry-Screen geroutet.
- [x] Sichtbare, direkt authorierte Player-Content-Hosts für Belt, Pouch und
      ResourceBag ergänzen und per Composition-Test erzwingen.
- [x] BaseTerminal als ersten kleineren Consumer auf einen authorierten,
      wiederverwendbaren Spatial-Inventory-Pane migrieren.
- [x] Crafting auf denselben Spatial-Inventory-Pane migrieren.
- [x] `UI.Screen.Loot` explizit in der Screen Registry konfigurieren und die
      beiden versteckten nativen Aliase auf `UI.Screen.Storage` entfernen.
- [x] Context-Menü, Split-Dialog und Feedback-Toast als authored
      Presentation-Klassen zentral am Interaction-Screen konfigurieren;
      unvollständige Assets dürfen ihre Root-Hierarchie nicht still ersetzen.
- [x] Ein authored Drop-Confirmation-Modal ergänzen, das nach erneuter stabiler
      Item-/Entry-ID-Prüfung genau einen bestätigten serverautoritativen
      Drop-Intent sendet.
- [x] Verfügbare Context-Actions und Klick-Revalidierung über einen gemeinsamen
      Query-/Policy-Vertrag statt mehrfacher Fragment-Inferenz bestimmen.
- [x] Controller-Context-Menüs an der fokussierten Item-/Slot-Geometrie statt
      an der linken oberen Viewport-Ecke verankern.
- [ ] Gear- und Spatial-Drag verwenden dieselbe authored Drag-Visual-Klasse.
- [ ] Quick-Access-Radial in die reguläre CommonUI-/Input-Komposition
      überführen, statt Controller-`BindKey`, `NativePaint` und direkten
      Viewport-Push zu mischen.
- [ ] Direkte Viewport-Widgets in sichtbare CommonUI-/UIExtension-Hosts
      überführen, sofern sie keine echten Drag-Decorators sind.

Verifizierter UI-Zwischenstand vom 2026-07-19:

- Das Root-Layout besitzt im UMG-Designer einen `RootOverlay` und vier
  fullscreen `CommonActivatableWidgetStack`-Kinder in eindeutiger Z-Reihenfolge.
- Die Config lädt
  `/Game/SurvivalRpg/UI/CUI_RpgPrimaryGameLayout.CUI_RpgPrimaryGameLayout_C`.
- Ein frischer Editor-Prozess kompiliert das Asset ohne Blueprint-Fehler; der
  Automationtest bestätigt, dass alle vier GameplayTags auf die authorierten
  Stacks registriert werden.
- Der native Root-Layout-Fallback bleibt vorläufig als Notfallpfad bestehen und
  wird erst nach PIE-/Cook-/Packaged-Verifikation separat entfernt.
- `CUI_StorageSpatial` ist graphfrei mit `RootOverlay`, `ContentRow`,
  `PlayerGroupsPanel`, `SecondaryInventoryGrid`, CommonUI-ActionBar und einem
  abschließenden, hit-test-invisible `DragVisualCanvas` authoriert.
- `UI.Screen.Storage` und `UI.Screen.Loot` besitzen in
  `DA_RpgUIScreenRegistry` je einen eigenen Eintrag. Beide wählen bewusst
  `CUI_StorageSpatial_C` auf `UI.Layer.GameMenu`, behalten aber getrennte
  semantische Screen-Identitäten und getrenntes Single-Instance-Tracking.
  Ein frischer Editor-Prozess hat alle fünf gespeicherten Zuordnungen erneut
  geladen und bestätigt.
- Der Router liefert Payloads nur noch über
  `IRpgUIScreenPayloadReceiver`. `RefreshPlayerSlotGroupPresentation()` trennt
  die aggregierte Dual-Inventory-Darstellung von den frei authorierten
  Player-Inventory-Hosts, während Coordinator, Input und Drag-Session bewusst
  gemeinsam bleiben.
- Die nachweislich unbenutzte Blueprint-Variable
  `BP_Rpg_PlayerController.StorageContainerWidget` wurde entfernt. Der
  anschließende Frischprozess-Scan findet keine direkte Referenz mehr auf
  `CUI_StorageContainer`.
- Das temporäre Editor-Authoring-Tool und seine zusätzlichen Modulabhängigkeiten
  wurden nach dem Asset-Cutover wieder vollständig entfernt.
- `PlayerGroupsPanel` und `SecondaryInventoryGrid` bleiben im nativen
  Rollback-kompatiblen Parent formal `BindWidgetOptional`. Der
  Composition-Test erzwingt für das kanonische Asset jedoch Namen, Typen,
  native Property-Bindings und die Drag-Canvas-Z-Reihenfolge.
- `ReceiveScreenPayload()` stagiert den Storage-Kontext im inaktiven Zustand.
  `BindStorageScreenContext()` ist der einzige echte Bindepunkt. Sowohl
  Payload-vor-Activation als auch Activation-vor-Payload binden genau einmal;
  dieselbe aktive Payload ist idempotent und ein A→B-Kontextwechsel bindet
  genau einmal neu.
- Der Storage-Presenter vergleicht das Player-Inventar der Payload mit der
  kanonischen PlayerState-Instanz, bevor Projektion und Quick-Transfer-Routen
  verbunden werden. Ein abweichendes Primary-Inventar sowie
  `PrimaryInventory == SecondaryInventory` werden fail-closed zurückgesetzt.
- `StorageContextLifecycle` prüft echten CommonUI
  Activate/Deactivate/Reactivate-Pool-Reuse, beide Initialreihenfolgen,
  exakte Bind-Generationen, A→B-Kontextwechsel, Same-Payload-Idempotenz,
  Hold-/Fokus-Cleanup, beide Quick-Transfer-Richtungen sowie null,
  ungültige und aliierte Payloads.
- Der gemeinsame Interaction-Screen besitzt Drag/Hold, Pointer-Routing,
  Preview-Cleanup, Feedback, freien Drag-Ghost und Panel-Navigation. Seine
  Coordinator- und Navigator-Instanzen sind screen-owned, bleiben beim
  CommonUI-Pooling stabil und werden von Player und Storage gemeinsam
  verwendet.
- Der Storage-Screen erbt nachweislich nicht mehr vom Player-Screen. Er besitzt
  eine eigene `URpgPlayerInventoryViewModel`-Projektion für die aggregierten
  Carry-/Content-Gruppen sowie eine eigene Secondary-Panel-VM; Gear- und
  Actionbar-Verträge sind nicht Teil seiner nativen Oberfläche.
- Native Hierarchie-, Ownership- und Pooling-Assertions sichern ab, dass
  Player und Storage Geschwister bleiben und pro Screen nicht versehentlich
  mehrere Coordinator-/Navigator-Instanzen entstehen.
- `CUI_StorageContainer` bleibt vorerst als unreferenziertes Rollback-Asset
  erhalten. Seine 48 Legacy-Nodes sind keine aktive Composition Authority und
  werden erst nach PIE-/Cook-/Packaged-Verifikation in Phase 7 gelöscht.
- Ein frischer UE-5.8-Commandlet-Prozess kompiliert sowohl das kanonische
  `CUI_StorageSpatial` als auch das unreferenzierte Legacy-Rollback-Asset
  `CUI_StorageContainer` nach der nativen Vererbungsänderung ohne
  Blueprint-Compilerfehler; dabei wurden keine Assets gespeichert.
- Der Player-Presenter besitzt jetzt genau eine screen-scoped
  `URpgPlayerInventoryViewModel` mit dem Screen als Outer. Er injiziert sie
  unter dem exakten Namen `RpgPlayerInventoryViewModel` in eine manuelle
  Blueprint-MVVM-Quelle; Source-Scan, Blueprint-`CreateInstance` und
  konkurrierender nativer Fallback sind entfernt.
- `URpgPlayerInventoryViewModel` erlaubt über
  `MVVMAllowedContextCreationType` nur noch `Manual`. Das kanonische
  `CUI_PlayerInventory` ist entsprechend gespeichert, optional und
  initialisiert seine native Ownership auch ohne bereits zugewiesenen
  Player-Kontext.
- UE 5.8 generiert für eine setzbare manuelle Source einen
  Expose-on-Spawn-Setter. Der Player-Presenter validiert deshalb die
  Pointer-Identität erneut an der Activation-Grenze und holt eine nach
  `NativeOnInitialized` überschriebene Source auf seine native Instanz zurück.
- Composition- und Pooling-Tests erzwingen exakt eine direkte Player-VM,
  Screen-Outer, stabile Pointer, eindeutige Delegate-Bindungen,
  Player-/Storage-getrennte VM-Instanzen und den Expose-on-Spawn-Reclaim. Der
  Player-Test durchläuft zusätzlich einen echten Slate-Release samt
  `NativeDestruct` und anschließendem `NativeConstruct`.
- Ein vollständiger Scan von 1.272 Assets und Maps fand keine serialisierten
  Aufrufer der entfernten Player-Blueprint-Lifecycle- und Refresh-Wrapper.
  Diese nachweislich unbenutzte Oberfläche wurde aus dem nativen Presenter
  entfernt.
- Das Root-Asset `CUI_PlayerInventory` besitzt bewusst keine deklarativen
  MVVM-Bindings. Das erste stabile Leaf `CUI_InventorySlotGroupEntry` verwendet
  jetzt dagegen genau eine optionale manuelle Source und genau ein
  `DisplayName -> Text_GroupName.Text`-Binding.
- Der Activation-Reclaim schließt den normalen Create-Widget/
  Expose-on-Spawn-Pfad. Der von UE 5.8 generierte öffentliche Manual-Source-
  Setter könnte bei einem späteren aktiven Blueprint-Aufruf jedoch erneut
  eine fremde VM setzen. Aktuell existiert kein gefundener Asset-Aufrufer;
  langfristig soll ein nativer Getter-/PropertyPath-Vertrag den Setter ganz
  vermeiden.
- Die Headless-Ownership-Tests verwenden bewusst ownerlose Widgets. Ein
  Integrationstest mit echtem `ARpgPlayerController`, `ARpgPlayerState` und
  kanonischem Player-Inventar bleibt für Listener, Projektion,
  Primary-Mismatch und Cleanup offen.
- `DA_RpgUIScreenRegistry` ist jetzt die einzige Composition Authority für
  `UI.Screen.Inventory`. Das unreferenzierte Legacy-Asset `CUI_GameMenu`
  enthält weder `CUI_PlayerInventory` noch eine Package-Abhängigkeit darauf.
- Bei der Bereinigung wurde ein zusätzlicher Legacyfehler behoben:
  `CUI_GameMenu` hatte fünf Switcher-Seiten, aber sechs Tab-Namen. Dadurch
  wurden Inventory, Character und Skills unter falschen Namen registriert.
  Switcher und `TabButtonNames` sind jetzt beide exakt
  `Map, Journal, Character, Skills`.
- Der dauerhafte Test
  `SurvivalRpg.UI.CompositionAuthority.PlayerInventoryRegistryOnly` erzwingt
  die vier verbleibenden Seiten und Namen, das Fehlen der generierten
  `CUI_PlayerInventory`-Property und -Dependency sowie die fortbestehende
  Registry-Abhängigkeit.
- Das temporäre UMG-Migrationstool und seine Editor-Modulabhängigkeiten wurden
  nach Compile und Save wieder vollständig entfernt.
- Das kanonische `CUI_PlayerInventory` authoriert jetzt alle fünf direkten
  Content-Hosts als `CUI_InventorySlotGroupEntry`:
  `Content_Pockets`, `Content_Backpack`, `Content_Belt`, `Content_Pouch` und
  `Content_ResourceBag`. Die drei Provider-Hosts liegen in einer zweiten,
  klar gerasterten Designer-Spalte; Backpack, Belt, Pouch und ResourceBag
  bleiben in eindeutiger, zusammenhängender Canvas-Reihenfolge.
- `Size To Content` war zuvor nur eine Laufzeitkorrektur des Presenters.
  Es ist jetzt auf allen fünf Canvas-Slots im UMG-Asset authoriert, sodass
  Designer-Vorschau und Runtime denselben Größenvertrag verwenden.
- Der dauerhafte Test
  `SurvivalRpg.Inventory.UI.PlayerAuthoredContentHosts` erzwingt Namen,
  exakten Typ, direkte Parent-Identität, authored Positionen und AutoSize,
  eindeutige Provider-Reihenfolge, native BindWidget-Pointer sowie die
  abschließende Z-Reihenfolge des `DragVisualCanvas`.
- Das einmalige Content-Host-Migrationstool und seine zusätzlichen
  Editor-Modulabhängigkeiten wurden nach Compile, Save und Frischprozess-
  Validierung wieder vollständig entfernt.
- Nach diesem Player-/Leaf-Schnitt war der UE-5.8-Editor-Build erfolgreich.
  Die damaligen vollständigen Läufe `SurvivalRpg.UI` (8/8) und
  `SurvivalRpg.Inventory` (41/41, darin `SurvivalRpg.Inventory.UI` 11/11)
  waren grün. Die früheren Warnungen für fehlende Belt-, Pouch- und
  ResourceBag-Hosts traten nicht mehr auf.
- Der Player-Inventory-Input folgt jetzt ohne Blueprint-Sonderweg der
  bestehenden Lyra-rooted Linie:
  `IMC_UI_PlayerHUD → IA_UI_Inventory → DA_InputConfig.NativeInputActions →`
  `RpgPawnGameplay → RpgPlayerGameplayInputRouter → UI.Screen.Inventory`.
  Der neue native Tag `InputTag.UI.Inventory` wird ausschließlich auf
  `Started` gebunden; der Router öffnet nur für den lokalen Controller und
  verwendet bewusst `OpenScreen` statt Toggle.
- `BP_Rpg_PlayerController` enthält weder einen direkten `Y`-Event noch einen
  `IA_UI_Inventory`-Event oder einen Inventory-Toggle-Aufruf. Seine
  Package-Abhängigkeit auf `IA_UI_Inventory` ist entfernt; `DA_InputConfig`
  besitzt diese Abhängigkeit jetzt genau einmal und Inventory liegt nicht in
  den GAS-Ability-Inputs.
- Die Default-Mappings sind als Contract abgesichert:
  Inventory exakt `I` und `Gamepad Special Right`, Menu exakt `Y` und
  `Gamepad Special Left`. CommonUI Back schließt den geöffneten Screen.
  Ein zweiter Inventory-Keypress zum Schließen wird nicht versprochen, solange
  CommonUI im aktiven Menu-Inputmodus normalen Gameplay-Input blockiert.
- `RpgPrototypeExperience` ist jetzt der einzige Composition-Owner von
  `IMC_UI_PlayerHUD`. Der doppelte Character-DefaultMapping-Eintrag und die
  Package-Abhängigkeit von `BP_Rpg_Character` wurden entfernt. Der dauerhafte
  Test `SurvivalRpg.UI.Input.PlayerHudMappingCompositionAuthority` erzwingt
  genau einen Experience-Eintrag und das Fehlen des Character-Doppelwegs.
- `CUI_InventorySlotEntry` und `CUI_ActionBarSlotEntry` verwenden jetzt
  ausschließlich ihre exakten optionalen Manual-Sources
  `RpgInventoryEntryViewModel` beziehungsweise `RpgActionBarSlotViewModel`.
  By-Class-Injection ist aus beiden Presentern entfernt; die zugehörigen
  ViewModel-Klassen erlauben nur noch manuelle MVVM-Komposition.
- Beide datengetriebenen Entry-Assets initialisieren ihre MVVM-Extensions nun
  ohne PlayerContext. Das Inventory-Leaf behält exakt zwei authored Bindings
  (`Icon`, `StackCount`), das Actionbar-Leaf exakt drei (`Icon`, `StackCount`,
  `HotkeyActionRowName`); beide EventGraphs sind leer.
- Die Release-Grenze leert VM, benannte MVVM-Source, native VM-Delegates,
  Coordinator-Delegate und Coordinator-Referenz. Selection, Panelzustand,
  Actionbar-Preview und Drag-State werden neutralisiert, laufende Animationen
  gestoppt und der Blueprint-Release-Hook beobachtet abschließend bereits
  `Normal`.
- Die permanenten Tests `InventorySlotEntryPooling` und
  `ActionBarSlotEntryPooling` konstruieren die echten authored Slate-/MVVM-
  Leaves und prüfen auf derselben Widget-Instanz den Vertrag
  VM A → Release → VM B einschließlich Source-, Delegate- und Preview-Cleanup.
- Das Leeren einer optionalen MVVM-Source entfernt die FieldNotify-Bindings,
  schreibt aber absichtlich keine Nullwerte in die unsichtbare gepoolte
  Darstellung. Beim Rebind überschreibt VM B alle authored Ziele unmittelbar.
- Bestätigter UX-Restpunkt: `CUI_ActionBarSlotEntry` implementiert den nativen
  `BP_OnActionBarSlotDragDropStateChanged`-Hook derzeit nicht. Focused,
  ValidTarget und InvalidTarget werden berechnet und pooling-sicher
  zurückgesetzt, aber im kanonischen Asset noch nicht als eigener visueller
  Zustand dargestellt.
- Das MVVM-gestützte Address-Leaf `CUI_AddressSlotEntry` und
  `CUI_GearSlot` verwenden jetzt ausschließlich die exakten optionalen
  Manual-Sources `RpgInventoryAddressSlotViewModel` beziehungsweise
  `RpgEquipmentSlotViewModel`. Beide Source-Typen werden vor der Injection
  geprüft; By-Class-Injection und automatische VM-Erzeugung sind entfernt.
- Beide Assets sind ohne PlayerContext initialisierbar. Das Address-Leaf
  besitzt exakt zwei deklarative Bindings (`Icon`, `StackCount`), das
  Gear-Leaf exakt eines (`Icon`). Release und Destruct lösen VM- und
  Coordinator-Delegates, leeren die benannte Source, schließen transienten
  UI-Zustand und setzen die Drag-Darstellung auf `Normal` zurück.
- Ein Presenter darf beim Release nur den gemeinsamen Interaction-Preview
  löschen, den er zuvor selbst über seinen lokalen External-Preview-Zustand
  authoriert hat. Die Lifecycle-Tests decken deshalb zusätzlich zwei
  gleichzeitige Präsentationsflächen desselben Address-/Gear-Ziels ab.
- `CUI_SpatialInventoryItem` ist bewusst kein MVVM-Leaf: Das kanonische Asset
  besitzt keine MVVM-Extension, weshalb die frühere By-Class-Injection
  garantiert wirkungslos war. Sie ist entfernt. Address- und Entry-Modus
  schließen sich jetzt gegenseitig aus; Grid-Removal und Destruct lösen beide
  VM-Delegates sowie den Coordinator und neutralisieren Visual-, Pointer-,
  Panel- und Drag-Zustand.
- Dynamische Spatial-Items, Zellen, Preview-Ghosts und Drag-Visuals verwenden
  nun ihr Parent-Widget als Owner statt `GetWorld()`. Damit bleibt der exakte
  OwningPlayer-Kontext auch für Split-Screen erhalten.
- Die permanenten Tests `InventoryAddressSlotEntryPooling`,
  `EquipmentSlotLifecycle` und `SpatialItemPresentationLifecycle` prüfen die
  echten authored Assets, Source-Namen und -Typen, A→B-Rebind, Release,
  Delegate-Eindeutigkeit, Wiederverwendung sowie die fehlende Spatial-MVVM-
  Extension.
- Bewusste Restgrenze: Der tatsächlich aktive `CUI_CarrySlot` ist ein
  spezialisierter imperativer Address-Presenter ohne MVVM-Extension;
  `CUI_AddressSlotEntry` besitzt derzeit keinen Asset-Registry-Referencer und
  bleibt bis zur Referenz-/Cook-Prüfung als Contract-Fixture erhalten.
  Carry besitzt noch doppelte VM-Beobachtung sowie fehlende Active-,
  Holstered- und Drag-State-Visuals. Das wird in Phase 6/7 separat bereinigt,
  statt den Spezialfall still als scheinbares MVVM-Leaf zu behandeln.
- Weitere bekannte Presentation-Reste: Im kanonischen Spatial-Item schreiben
  zwei Blueprint-Hooks nur noch ungenutzte VM-Variablen, und der Drag-State-
  Switch besitzt keine wirksamen Zweige. Focused, HeldSource, ValidTarget und
  InvalidTarget werden nativ berechnet, aber noch nicht sichtbar dargestellt.
  Die Graphwriter gehören in die Legacy-Bereinigung; die sichtbaren Zustände
  in den gemeinsamen UX-Schnitt.
- `UI.Screen.Loot` ist jetzt ein expliziter fünfter Eintrag in
  `DA_RpgUIScreenRegistry`: eigener ScreenTag, `UI.Layer.GameMenu`,
  `CUI_StorageSpatial_C`, Input-Suspension während Async-Load und
  Single-Instance-Verhalten sind vollständig authoriert. Der bestehende
  serverautorisierte Loot-Öffnungspfad behält dabei seine eigene
  `UI.Screen.Loot`-Payload-Identität.
- `URpgUIScreenSubsystem::ResolveScreenEntry` löst Registry-Asset und
  `DefaultScreenMappings` nur noch nach exakter Tag-Gleichheit auf. Die beiden
  nativen Loot→Storage-Kompatibilitätszweige sind entfernt; ein fehlender
  Loot-Eintrag scheitert damit sichtbar statt still eine andere
  Composition-Identität zu übernehmen.
- Die permanenten Tests `LootSpatialMapping` und `ExactResolution` erzwingen
  den authored Loot-Eintrag und beweisen getrennt, dass weder ein
  Storage-only Registry-Asset noch ein Storage-only Config-Fallback Loot
  auflösen kann. Der aktuelle vollständige UI-Lauf enthält fünf grüne
  ScreenRegistry-Verträge einschließlich BaseTerminal.
- `CUI_SpatialInventoryPane`, `CUI_BaseResourceListSpatial` und
  `CUI_BaseTerminalSpatial` sind graphfrei und besitzen keine
  Blueprint-MVVM-Extension. Der Pane authoriert exakt ein Spatial-Grid; das
  Terminal authoriert Resource List, Pockets-Pane, Deposit-/Upgrade-Buttons,
  eine `CommonBoundActionBar` und als oberstes Kind ein hit-test-invisibles
  `DragVisualCanvas`.
- `UI.Screen.BaseTerminal` zeigt in `DA_RpgUIScreenRegistry` exakt auf
  `CUI_BaseTerminalSpatial_C` in `UI.Layer.GameMenu`. Das neue Terminal-Asset
  besitzt keine Abhängigkeit auf die alten Assets `CUI_BaseTerminal`,
  `CUI_BaseResourceList` oder `CUI_Inventory`.
- Der BaseTerminal-Presenter besitzt eine stabile screen-owned
  BaseStorage-VM; der passive Pane eine stabile pane-owned Panel-VM. Ungültige
  oder aliierte Payloads sowie Deaktivierung und Pool-Reuse werden
  fail-closed vollständig gelöst, ohne die ViewModel-Instanzen auszutauschen.
- Der erste BaseTerminal-Slice rendert bewusst nur den kanonischen
  `Pockets`-Container. `Armory` wird als Teil des Payload-Vertrags validiert,
  aber noch nicht als zweite Fläche dargestellt. Diese Produktentscheidung
  bleibt sichtbar, statt durch eine allgemeine Dual-Inventory-Basis verdeckt
  zu werden.
- Deposit und Upgrade bleiben serverautorisierte Requests über
  `URpgInventoryUiActionComponent`. Tastatur und Gamepad verwenden semantische
  CommonUI-Actions aus `DT_RpgUIActions_BaseTerminal`; ihre Sichtbarkeit folgt
  derselben Verfügbarkeitsprüfung wie die authorierten Buttons.
- `CUI_CraftingStationSpatial` ist ein graphfreier authored Screen mit
  Player-Gruppen, genau einem wiederverwendbaren
  `CUI_SpatialInventoryPane` für den Output, typisierten Recipe-, Ingredient-
  und Job-Leaves, einer CommonUI-Actionbar und einem obersten
  hit-test-invisiblen `DragVisualCanvas`.
- Der native Crafting-Presenter besitzt stabile screen-owned ViewModels,
  validiert Station, Player-Inventory und Output-Inventory fail-closed und
  räumt Payload, Routen, Delegates, Fokus und Interaction-State bei
  Deaktivierung sowie Pool-Reuse vollständig auf.
- `UI.Screen.Crafting` zeigt in `DA_RpgUIScreenRegistry` exakt auf
  `CUI_CraftingStationSpatial_C`. Der aktive Screen hängt cook-sichtbar am
  Spatial Pane, seinen typisierten Leaves und
  `DT_RpgUIActions_Crafting`, aber weder an `CUI_Inventory` noch am alten
  `CUI_CraftingStation`.
- Craft und Pause verwenden explizite CommonUI-Actions. Craft liegt auf
  `C`/`Gamepad Face Left`, Pause auf `P`/`Gamepad Face Top`; damit kollidiert
  Craft nicht mit CommonUI-Accept.
- Fünf graphfreie, MVVM-freie Presentation-Assets unter
  `SurvivalRpg/Inventory/UI/Presentation` bilden einen sichtbaren Vertrag für
  Context-Action-Row, Quick-Access-Row, Context-Menü, Split-Dialog und
  Feedback-Toast.
- Player, Storage/Loot, BaseTerminal und Crafting konfigurieren exakt dieselben
  Context-/Split-Klassen. Jeder Screen authoriert den Toast direkt vor seinem
  abschließenden `DragVisualCanvas`.
- Nur `URpgInventoryInteractionScreenWidget` pusht Context- und Split-Modals
  auf `UI.Layer.Modal`, hält ihren Lifecycle und schließt sie bei
  Deaktivierung oder Destruct. Grid-, Address-, Gear- und Pane-Leaves leiten
  Presentation-Anfragen ausschließlich an diesen Screen-Host weiter.
- Der Screen verfolgt zusätzlich den konkreten Modal-Ursprung. Rebind, Release
  und Pool-Reuse schließen nur dessen aktive Präsentation; Deaktivierungs-
  Delegates werden beim normalen Close wieder entfernt und sammeln sich nicht
  auf wiederverwendeten CommonUI-Widgets an.
- Fehlende Pflicht-BindWidgets oder Entry-Klassen scheitern geschlossen.
  Native Root-Replacement- und `StaticClass()`-Presentation-Fallbacks sowie
  der direkte Toast-`AddToPlayerScreen()`-Pfad wurden entfernt.
- Inventory-Feedback wird auf dem empfangenden Client dem owning
  `APlayerController` zugeordnet. Screen und Interaction-Session verwenden
  denselben Recipient-Vertrag; ein ungültiger Component-Owner sendet nicht
  versehentlich als globales Legacy-Feedback.
- `SurvivalRpg.Inventory.UI.AuthoredActionPresentation` erzwingt Parent-Klassen,
  graph- und MVVM-freie Widget-Blueprints, exakte BindWidget-Typen, die
  Screen-Komposition sowie cook-sichtbare Package-Abhängigkeiten der Screens
  und Context-Rows.
- Die fokussierten Frischprozess-Tests
  `SurvivalRpg.Inventory.UI.AuthoredActionPresentation` und
  `SurvivalRpg.Inventory.Feedback.LocalPlayerRecipientRouting` sind jeweils
  1/1 erfolgreich.
- Das temporäre Presentation-Authoring-Tool und seine zusätzlichen
  Modulabhängigkeiten wurden nach Compile, Save und Frischprozess-Verifikation
  vollständig entfernt.
- Der aktuelle UE-5.8-Editor-Build ist erfolgreich. Der fokussierte
  Exact-Placement-Test ist 1/1 grün. Die vollständigen Läufe
  `SurvivalRpg.Crafting` (6/6),
  `SurvivalRpg.Inventory` (55/55, davon `SurvivalRpg.Inventory.UI` 19/19),
  `SurvivalRpg.UI` (10/10) und `SurvivalRpg.Equipment` (5/5) sind ebenfalls
  grün.
- Entry-, Address- und Equipment-Präsentationen beziehen ihre geordneten
  Context-Actions jetzt aus einem screen-scoped Coordinator-Vertrag und
  revalidieren denselben Vertrag unmittelbar vor dem Klick-Dispatch.
  Mutierende Actions sperren während eines pending Interaction-Requests;
  stabile Entry-, Item-, Placement-, Address- und Equipment-Identitäten
  verwerfen veraltete Menüs und Held-Payloads fail-closed.
- Split und Equipment→Content verwenden dieselben read-only Placement-
  Preflights wie ihre autoritativen Gateway-Pfade. Eine Context-Rotation ist
  eine echte In-place-`Rotate`-Transaktion und kann nicht mehr unbemerkt einen
  Nachbarn swappen. Die finale Servervalidierung bleibt unverändert
  autoritativ.
- `SurvivalRpg.Inventory.ContextActions.SourceSemanticsAndStaleState` ist 1/1
  warnungsfrei erfolgreich. Der endgültige UE-5.8-Build sowie
  `SurvivalRpg.Inventory` (55/55), `SurvivalRpg.UI` (10/10),
  `SurvivalRpg.Equipment` (5/5) und `SurvivalRpg.Crafting` (6/6) sind
  warnungsfrei grün.
- Controller-Context-Menüs werden jetzt aus der fokussierten Spatial-Auswahl
  beziehungsweise der gecachten Carry-/Equipment-Slot-Geometrie verankert.
  Nur wenn keine gültige Auswahlgeometrie existiert, wird die Mitte der
  Player-Screen-Geometrie verwendet. Die Position wird im tatsächlichen
  Context-Menu-Canvas konvertiert und an dessen Grenzen geklemmt.
- `Gamepad_LeftTrigger` wird zentral am Controller-Actions-Host behandelt;
  Spatial-, Carry- und Equipment-Leaves lassen den Input dorthin hochreichen
  und synchronisieren ihren Fokus vor einer Pointer-basierten Modalöffnung.
- Der abschließende UE-5.8-Editor-Build ist erfolgreich.
  `SurvivalRpg.Inventory.UI.ContextAnchor.GeometryAndClamping` ist 1/1 grün;
  die vollständigen Läufe `SurvivalRpg.Inventory` (56/56),
  `SurvivalRpg.UI` (10/10), `SurvivalRpg.Equipment` (5/5) und
  `SurvivalRpg.Crafting` (6/6) sind ohne Warnungen, Fehler oder ausgelassene
  Tests erfolgreich.
- Ein separater frischer UE-5.8-Commandlet-Prozess kompiliert alle drei neuen
  Widget-Blueprints ohne Fehler und lädt Registry sowie dedizierte
  BaseTerminal-Action-Tabelle erneut. Dabei wurden keine Assets gespeichert.
- Das alte `CUI_BaseTerminal` bleibt bis zu PIE-, Cook- und
  Packaged-Verifikation als Rollback-Asset erhalten. Interaktive Mouse-,
  Gamepad-, Fokus-, Actionbar- und Drag-Prüfungen sowie der kanonische
  Primary-Mismatch mit echtem OwningPlayer bleiben offen.
- Verbindliche nächste Umsetzungsreihenfolge:
  1. [x] Gepoolte Manual-Sources in Inventory- und Actionbar-Entries beim
     Release explizit leeren und den vollständigen transienten Entry-Zustand
     auf `Normal` zurücksetzen.
  2. [x] MVVM-gestützte Address- und Gear-Widgets auf benannte, typgeprüfte
     Source-Injection vereinheitlichen; wirkungslose MVVM-Injection im
     Spatial-Item entfernen und dessen eigenen Release-Vertrag schließen.
  3. [x] Expliziten `UI.Screen.Loot`-Registry-Eintrag anlegen und beide
     C++-Aliase entfernen.
  4. [x] BaseTerminal als kleineren ersten Consumer auf einen authored,
     wiederverwendbaren Spatial-Pane migrieren.
  5. [x] Crafting auf denselben Pane migrieren und den aktiven Crafting-Pfad
     von Legacy-`CUI_Inventory` sowie dem alten Crafting-Screen trennen.
     Die physischen Rollback-Assets werden erst in Schritt 11 entfernt.
  6. [x] Authored Context-Menü, Split-Dialog und Feedback-Toast zentralisieren
     und native Root-Replacement-Fallbacks fail-closed machen.
  7. [x] **P0:** Authored Drop-Confirmation-Modal am Interaction-Screen
     ergänzen. Confirm revalidiert stabile Item-/Entry-ID, Quelle und Menge und
     sendet genau einen Retry mit `bConfirmed=true`; Cancel, Deactivation und
     Pool-Reuse verwerfen den Request vollständig.
  8. [x] **P1:** Einen gemeinsamen `CanExecuteContextAction`-/AvailableActions-
     Vertrag im bestehenden Coordinator-/Policy-Pfad verwenden; die finale
     Servervalidierung bleibt autoritativ.
  9. [x] **P2:** Controller-Context-Menüs an der fokussierten Auswahl
     verankern und die Viewport-Mitte nur als Fallback verwenden.
  10. [ ] Quick-Access-Radial und Gear-Drag-Visual in die gemeinsame UI-/Input-
     Komposition überführen.
  11. [ ] Orphan-Assets und Notfall-Fallbacks erst nach Referenz-, Cook- und
     Packaged-Prüfung entfernen.
- Noch offen: den kanonischen Primary-Mismatch mit echtem OwningPlayer in PIE
  beziehungsweise einem passenden Test-Harness abdecken sowie interaktive
  Mouse-/Gamepad-, PIE-, Cook- und Packaged-Prüfungen ausführen.

Interaktiver Smoke-Test für den aktuellen Storage-Schnitt:

- Zielkarte ist
  `/Game/SurvivalRpg/Maps/Test/Lvl_ThirdPerson`; dort ist
  `BP_InventoryTestAcotr` als echter
  `ARpgInventoryContainerActor`-Öffnungspfad platziert.
- Interaktion ist im aktuellen `IMC_Movement` nur über `M` gebunden. Eine
  Gamepad-Interact-Zuordnung fehlt.
- Der Test-Container besitzt derzeit keine deterministisch serialisierte
  Startbefüllung. Vor dem Smoke-Test müssen ein rotierbares Mehrzellen-Item
  und ein Stack reproduzierbar in Player und Container bereitgestellt werden.
- Spatial-Grid-Tasten werden teilweise direkt im nativen Widget behandelt,
  während andere Inventory-Aktionen aus `DT_RpgUIActions_Inventory` stammen.
  Diese doppelte Input-Autorität wird zusammen mit Fokus- und
  Gamepad-Verträgen vereinheitlicht.

## Phase 2 – Einheitlicher Mutation-Kernel

Status: **Offen**

- [ ] Öffentliche Low-Level-Add/Remove/Move/Sort-Blueprintfläche deprecaten.
- [ ] Schmale Intents für Grant/Bootstrap, Consume, Move, Transfer, Drop und
      Restore anbieten.
- [ ] Remove/Consume/Drop grundsätzlich subtree-sicher machen.
- [ ] Raw-Add gegen fremden Outer, doppelte Item-ID und bereits enthaltene
      Instanzen absichern.
- [ ] Physisches Equippen ausschließlich über Inventory-Transaktionen führen.
- [ ] Eine öffentliche Placement-Auswertung als gemeinsamen Vertrag für Move,
      Equip, Split, Add, Transfer, Auto-Placement und Restore verwenden.
- [ ] UI-Preview aus einem echten Mutation-Plan ableiten; konkrete Belegung,
      Swap und dynamische Handkonflikte müssen mit dem Server-Commit
      übereinstimmen.
- [ ] `URpgEquipmentLoadoutComponent` auf Aktivierung der Hände und
      Reconciliation des physischen Gear-Zustands reduzieren.

## Phase 3 – Runtime-Transfer vom Save/Load trennen

Status: **Offen**

- [ ] Source und Target vollständig vorvalidieren.
- [ ] Transfer als In-place-Delta atomar committen.
- [ ] Nur den übertragenen Subtree für den neuen Actor-Outer rekonstruieren.
- [x] UObject- und EntryId-Identität aller überlebenden und unbeteiligten
      Items beim aktuellen Graph-Commit erhalten.
- [ ] Notifications und FastArray-Deltas einmal pro Commit bündeln.
- [ ] Rollback ohne extern sichtbaren Zwischenzustand sicherstellen.
- [ ] Batch-Pickup und Collect gegen Scratch-Occupancy planen.

## Phase 4 – Datenmodell und Persistenz

Status: **Offen**

- [ ] Aktuelle Footprints und Rotationsregeln beim Import aus der
      Item-Definition rekonstruieren.
- [ ] Einen kanonischen Stack-Key für Runtime-State-Kompatibilität einführen.
- [ ] Legacy Snapshot nur noch über einen versionierten Konverter zulassen und
      anschließend entfernen.
- [ ] `ContainerHandle` kanonisch machen; Legacy-`ContainerId` nur noch als
      echte Deprecated-/Migrationsproperty führen.
- [ ] Normalisierung und Root-Slot-Erkennung immer mit dem vollständigen
      `FRpgInventoryContainerHandle` statt nur mit lokaler `FName`-ID
      durchführen.
- [ ] MaxEntries, Tiefe, Cycles, Duplicate IDs und Subtree-Grenzen in allen
      Import-/Transferpfaden einheitlich validieren.

## Phase 5 – Datengetriebenes Layout und Editor-Validierung

Status: **Offen**

- [ ] `URpgPlayerInventoryLayoutDefinition` als DataAsset über
      PawnData/Experience zuweisen.
- [ ] Semantische Rollen statt Logik aus hartcodierten `FName`-IDs verwenden.
- [ ] Gear-/Carry-Gruppen erhalten eine explizite, typisierte
      `ERpgEquipmentSlot`-Rolle statt indirekter Namens- oder
      Kategorieableitung.
- [ ] Explizites Spatial-Fragment für jedes gridfähige Item verlangen.
- [ ] `IsDataValid` für ItemDefinitions, Container-IDs, Footprints,
      Equipment-Slots, LayoutDefinition und ScreenRegistry ergänzen.
- [ ] `BothHands + OffHand`, leere `AllowedSlots` bei ausrüstbaren Items und
      definitionlose Provider als konkrete Validierungsfehler bzw.
      Migrationswarnungen abbilden.
- [ ] Widget-Compiler-/Editor-Validierung für erforderliche BindWidgets,
      Layer und Input-Actions ergänzen.

## Phase 6 – MVVM, Refresh und Komponentenschnitt

Status: **In Arbeit**

- [x] Der Player-Screen besitzt genau eine native screen-scoped VM, injiziert
      sie in eine exakte manuelle MVVM-Quelle und behält VM, Source und
      Delegate-Bindungen stabil über CommonUI-Pooling.
- [x] `CUI_InventorySlotGroupEntry` als erstes deklaratives Leaf abschließen:
      genau eine optionale manuelle Source, genau ein
      `DisplayName -> Text_GroupName.Text`-Binding und kein konkurrierender
      Blueprint-`GroupId -> SetText`-Writer.
- [x] Das datengetriebene Leaf ohne PlayerContext initialisierbar machen,
      dynamische Gruppen unter dem WidgetTree ihres authored Parent erzeugen
      und Compile-, Construct-, FieldNotify- sowie Unbind-Vertrag dauerhaft
      testen.
- [x] Gepoolte Inventory- und Actionbar-Entries auf exakte optionale
      Manual-Sources umstellen und bei Release VM-, MVVM-, Delegate-,
      Coordinator-, Selection-, Preview-, Animations- und Drag-State
      vollständig neutralisieren.
- [x] MVVM-gestützte Address- und Gear-Leaves auf exakte optionale
      Manual-Sources umstellen, Preview-Eigentum presenter-lokal halten und
      Pooling/Rebind mit den echten Assets testen.
- [x] Wirkungslose Spatial-Item-MVVM-Injection entfernen, Address-/Entry-Modus
      exklusiv machen und die Reconciliation-/Destruct-Grenze einschließlich
      OwningPlayer-Kontext dauerhaft testen.
- [ ] Stabile Child-VMs pro Item-ID und Container-Handle verwenden.
- [ ] Nur tatsächlich geänderte FieldNotify-Felder senden.
- [ ] Inventory-Invalidierungen pro Commit/Tick bündeln.
- [x] Player- und Storage-Projektionen bei Deaktivierung unbinden, ohne ihre
      screen-owned VM-Instanzen beim Pooling zu ersetzen.
- [x] BaseTerminal auf denselben stabilen Unbind-/Pooling-Vertrag bringen.
- [x] Crafting auf denselben stabilen Unbind-/Pooling-Vertrag bringen.
- [ ] Weitere gepoolte Screens auf denselben Unbind-/Pooling-Vertrag bringen.
- [ ] Blueprint-MVVM als einzigen Leaf-Datenbindungsweg verwenden; BP-Events
      nur für Animation und imperative Präsentation.
- [ ] Den öffentlich generierten Manual-Source-Setter durch einen
      unverletzbaren nativen Getter-/PropertyPath-Vertrag ersetzen.
- [ ] BlueprintCallable Lifecycle-Mutatoren der Aggregate-VM nach
      Asset-Referenzprüfung auf eine native Presenter-Oberfläche reduzieren.
- [ ] Player-/Storage-Lifecycle mit echtem PlayerController, PlayerState,
      kanonischem Inventory und Listener-Cleanup als Integrationstest
      abdecken.
- [ ] Gear-, Carry- und Content-Widgets auf denselben Coordinator-/MVVM-Pfad
      bringen; Kontextmenüs fragen Fähigkeiten statt Fragmente zu erraten.
- [ ] `CUI_CarrySlot` auf genau eine VM-Beobachtung reduzieren und eine
      ausdrückliche Presenter-Policy festlegen: deklarative Itemdaten über die
      exakte Address-Source, imperative Hooks nur für Active/Holstered,
      Interaktion und Animation.
- [ ] `RpgInventoryUiActionComponent` in schmale Domain-Handler aufteilen;
      Controller bleibt RPC-Eigentümer und kann vorübergehend als Fassade
      bestehen.
- [ ] Manager intern in Storage, Rules/Planner, Transactions und Persistence
      schneiden, ohne die öffentliche Autorität zu duplizieren.
- [ ] Große UI-Sammeldateien in eine Klasse pro Datei aufteilen.

## Phase 7 – Legacy endgültig entfernen

Status: **Offen**

- [ ] Asset-Registry-Referenzbericht für deprecated Klassen, APIs und Assets
      erzeugen.
- [ ] Blueprints resaven und notwendige Save-/CoreRedirect-Migrationen
      festhalten.
- [ ] Snapshot, Index-/Global-Sort, Compatibility-Includes und tote
      BlueprintCallable-Mutatoren entfernen.
- [ ] Legacy-`CUI_Inventory` samt TileView-/SlotEntry-Pfad erst nach der noch
      offenen Crafting-Migration sowie Referenz-, Cook- und
      Packaged-Verifikation löschen.
- [ ] Das referenzlose `CUI_AddressSlotEntry` nach Carry-Migration,
      Asset-Registry-, Cook- und Packaged-Prüfung entweder einem echten
      Consumer zuordnen oder löschen.
- [ ] Ungenutzte Spatial-Item-VM-Variablen, leere Setter-Graphwriter und den
      wirkungslosen Drag-State-Switch nach Asset-Resave entfernen.
- [ ] Verwaiste `_Old`-, Test- und Self-only-Assets nach Referenzprüfung
      entfernen.
- [ ] Übergangsbranches, `#if 0`-Blöcke und veraltete Kommentare löschen.

## Verifikation pro Phase

- Relevanten `SurvivalRpgEditor`-Build ausführen.
- Inventory- und Equipment-Automationtests ausführen.
- Für replizierte Änderungen mindestens Server/Client, OwnerOnly versus
  actor-relevant Inventory und Late Join prüfen.
- Für UI-Änderungen wiederholtes Öffnen/Schließen, CommonUI-Pooling,
  Payload-Wechsel, Fokus, Gamepad und Mouse-Drag prüfen.
- Erst nach tatsächlich ausgeführter Verifikation Build-, Netzwerk- oder
  Editor-Korrektheit als bestätigt markieren.
