# Inventory Refactor Plan

Stand: 2026-07-25

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

Status: **Abgeschlossen**

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
- Definitionlose Slot-Provider besaßen vorläufig einen ausdrücklich
  dokumentierten Legacy-Fallback. Phase 5F hat ihn nach Asset-Audit und
  Data Validation entfernt; definitionlose ItemContainer bleiben normale
  portable beziehungsweise verschachtelbare Container, aber keine
  impliziten Gear-Provider.

In Phase 0 bekannte Restpunkte, späteren Phasen zugeordnet:

- Der Cross-Inventory-Vollgraph-Import war Phase 3 zugeordnet. Seit Phase 3A
  arbeiten Transfers als vorvalidierte In-place-Deltas; unnötige Remove-/Add-
  Nachrichten und pauschaler Subobject-Registrierungs-Churn sind entfernt.
  Detached/static Pickup-Payloads und der kanonische Multi-Root-Collect aus
  einem `ARpgDroppedInventoryActor` planen seit Phase 3B1/3B2 jeweils gegen
  einen gemeinsamen Scratch-Zustand und committen ohne sichtbare Zwischenlage.
- Collect-/Starter-Grant-Pfade sind jetzt physisch korrekt geroutet, benötigen
  aber noch eigene Ende-zu-Ende-Tests für Vollbelegung und Rollback (Phase 2).
- Der gemeinsame Placement-Evaluator unterscheidet Root- und item-owned
  Container inzwischen über den vollständigen Handle. Verbleibende
  Legacy-Normalisierung außerhalb dieses Vertrags wird in Phase 4 entfernt.
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
- `ID_BasicTwoHandedSword` besitzt für den Player-Pfad weiterhin keine
  Weapon-Traits. Sein zuvor fehlendes Spatial-Fragment wurde in Phase 5D
  verhaltensneutral als 1 x 1 und nicht rotierbar ergänzt; der
  Enemy-Loadout-Pfad kann weiterhin gültig sein.
- Semantische Asset-Korrekturen bleiben von der technischen Spatial-Migration
  getrennt. Phase 5 ergänzt Cross-Asset-Data-Validation; Phase 7 entscheidet
  anschließend referenzbasiert über Korrektur, weitere Migration oder Löschung.

## Phase 1 – UI Composition und Editor-WYSIWYG

Status: **Abgeschlossen**

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
- [x] Gear- und Spatial-Drag verwenden dieselbe authored Drag-Visual-Klasse.
- [x] Quick-Access-Radial in die reguläre CommonUI-/Input-Komposition
      überführen, statt Controller-`BindKey`, `NativePaint` und direkten
      Viewport-Push zu mischen.
- [x] Direkte Viewport-Widgets in sichtbare CommonUI-/UIExtension-Hosts
      überführen, sofern sie keine echten Drag-Decorators sind.

  Der Inventory-/Gameplay-HUD-, Indicator-, Respawn- und Frontend-Pfad ist
  abgeschlossen. `CUI_RpgHudLayout` authoriert die sichtbare Indicator-Layer,
  `UI.Screen.Respawn` läuft auf `UI.Layer.Modal`, und Boot sowie Main Menu
  werden ausschließlich als registrierte `UI.Screen.*`-Roots auf
  `UI.Layer.Menu` geöffnet. Die Frontend-GameModes konfigurieren nur noch
  ihren Screen-Tag; CommonGame besitzt Primary Layout und Viewport, während
  `ARpgFrontendHUD` den Map-Lifecycle koordiniert. `CUI_BootMenu` besitzt nur
  noch seine sichtbaren Splash-Seiten, `CUI_MainMenuStack` seine fünf
  authorierten Navigationsstacks. Die direkten Boot-/MainMenu-HUD- und
  Controller-Assets sind entfernt. Framework-eigene Root-/Loading-Screen-
  Pfade und echte Drag-Decorators sind ausdrücklich kein Migrationsziel.

Verifizierter UI-Zwischenstand vom 2026-07-19:

- Das Root-Layout besitzt im UMG-Designer einen `RootOverlay` und vier
  fullscreen `CommonActivatableWidgetStack`-Kinder in eindeutiger Z-Reihenfolge.
- Die Config lädt
  `/Game/SurvivalRpg/UI/CUI_RpgPrimaryGameLayout.CUI_RpgPrimaryGameLayout_C`.
- Ein frischer Editor-Prozess kompiliert das Asset ohne Blueprint-Fehler; der
  Automationtest bestätigt, dass alle vier GameplayTags auf die authorierten
  Stacks registriert werden.
- `CUI_RpgPrimaryGameLayout` ist nach Clean-Cook, Package und packaged Smoke
  die einzige Root-Composition-Authority. Sowohl der native Policy-
  Klassenfallback als auch die zur Laufzeit erzeugten Layer-Stacks sind
  entfernt; fehlende Config beziehungsweise BindWidgets scheitern geschlossen.
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
- Der Player-Presenter besitzt genau eine screen-scoped
  `URpgPlayerInventoryViewModel` mit dem Screen als Outer und erzeugt sie vor
  `Super::NativeOnInitialized()`. Die authored Source
  `RpgPlayerInventoryViewModel` liest diese Instanz über den exakten nativen
  PropertyPath `GetPlayerInventoryViewModel`; Source-Scan,
  Blueprint-`CreateInstance`, manuelle Injection und konkurrierender nativer
  Fallback sind entfernt.
- `URpgPlayerInventoryViewModel` erlaubt über
  `MVVMAllowedContextCreationType` nur noch `PropertyPath`. Das kanonische
  `CUI_PlayerInventory` ist nicht optional gespeichert, kann ohne bereits
  zugewiesenen Player-Kontext initialisieren und besitzt weder einen
  öffentlichen generierten Setter noch eine `ExposeOnSpawn`-Source.
- Composition- und Pooling-Tests erzwingen exakt eine direkte Player-VM,
  Screen-Outer, stabile Pointer, eindeutige Delegate-Bindungen,
  Player-/Storage-getrennte VM-Instanzen, abgelehnte Fremdzuweisung und
  Activation vor dem ersten Slate-Construct. Der Player-Test durchläuft
  zusätzlich einen echten Slate-Release samt `NativeDestruct` und
  anschließendem `NativeConstruct`.
- Ein vollständiger Scan von 1.272 Assets und Maps fand keine serialisierten
  Aufrufer der entfernten Player-Blueprint-Lifecycle- und Refresh-Wrapper.
  Diese nachweislich unbenutzte Oberfläche wurde aus dem nativen Presenter
  entfernt.
- Das Root-Asset `CUI_PlayerInventory` besitzt bewusst keine deklarativen
  MVVM-Bindings. Das erste stabile Leaf `CUI_InventorySlotGroupEntry` verwendet
  jetzt dagegen genau eine optionale manuelle Source und genau ein
  `DisplayName -> Text_GroupName.Text`-Binding.
- Der frühere Activation-Reclaim des Player-Roots ist nicht mehr nötig:
  `CreateWidget` bietet keinen VM-Eingabepin mehr an, und der kompilierte
  PropertyPath-Vertrag lehnt eine spätere fremde `SetViewModel`-Zuweisung ab.
  Die dynamischen Leaf-Sources bleiben davon getrennt, weil ihre Presenter
  Pointerwechsel von A nach B und anschließend auf `null` abbilden müssen.
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
  blieb auch in Clean-Cook, Stage und Package referenzlos. Das Asset bleibt bis
  zur Carry-Migration als ausdrücklich inaktives Contract-Fixture erhalten;
  seine fehlende Drag-Visual-Konfiguration scheitert ohne aktiven Consumer
  geschlossen.
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
- Gear-, Spatial- und Address/Carry-Dragquellen akzeptieren nur noch
  `URpgInventoryDragVisualWidget` als presentation-only Decorator. Die drei
  Legacy-Zweige, die vollständige Slot-/Item-Presenter als Drag-Visual
  wiederverwendeten und dafür ViewModels sowie Coordinator injizierten, sind
  entfernt.
- `CUI_InventoryDragVisual_C` ist die explizite authored Klasse für Gear,
  Spatial-Item, Spatial-Controller-Preview, Carry und die freien Ghosts aller
  vier aktiven Interaction-Screens. Dadurch stammt die Darstellung nicht mehr
  implizit aus der Operation-Klasse oder einem anderen Widget-Fallback.
- `SurvivalRpg.Inventory.UI.CanonicalDragVisualContract` ist 1/1 grün und
  erzwingt exakte Klassenidentität, typisierte native Properties und die
  authored BindWidgets. Der UE-5.8-Editor-Build sowie
  `SurvivalRpg.Inventory` (57/57), `SurvivalRpg.UI` (10/10),
  `SurvivalRpg.Equipment` (5/5) und `SurvivalRpg.Crafting` (6/6) sind ohne
  Warnungen, Fehler oder ausgelassene Tests erfolgreich.
- Das Quick-Access-Radial ist jetzt ein passiver, authorierter
  CommonUI-/UIExtension-Presenter im HUD-Slot
  `UI.HUD.Slot.QuickAccessRadial`. Die Experience besitzt sowohl den
  Widget-Beitrag als auch genau einen separaten
  `IMC_UI_QuickAccessRadial` mit Priorität 10; `IMC_UI_PlayerHUD` enthält
  keine konkurrierenden Radial-Mappings mehr.
- Hold, Stick-Auswahl, Commit und Cancel laufen über native Enhanced-Input-
  Tags und den lokalen Gameplay-Input-Router. Der Controller besitzt weder
  direkte `BindKey`-Radialpfade noch Tick-basiertes Stick-Polling,
  `CreateWidget`/Viewport-Komposition oder einen nativen
  `NativePaint`-Fallback mehr. Gameplay-Aktivierung bleibt ausschließlich
  beim bestehenden serverautoritativen Actionbar-Pfad.
- `CUI_QuickAccessRadial` authoriert acht feste Slot-Leaves.
  `CUI_QuickAccessRadialSlotEntry` besitzt genau eine optionale manuelle
  `RpgActionBarSlotViewModel`-Source und drei einseitige Bindings für Icon,
  Kurzname und kompakten Stack-Text. UIExtension-Pooling, CommonUI-Back,
  Delegate-Cleanup und Look-Input-Suppression besitzen explizite,
  idempotente Lifecycle-Grenzen.
- Die permanenten Tests
  `SurvivalRpg.UI.QuickAccessRadial.CompositionAssets`,
  `SurvivalRpg.UI.QuickAccessRadial.InputAssets` und
  `SurvivalRpg.UI.QuickAccessRadial.SelectionMath` sind 3/3 grün. Ein
  separater frischer UE-5.8-Commandlet-Prozess kompilierte und speicherte die
  Presenter-Assets ohne Ensure oder Fehler; das einmalige Autorierungstool
  und alle Inspektionsskripte wurden anschließend vollständig entfernt.
- Der finale UE-5.8-Editor-Build sowie die vollständigen Läufe
  `SurvivalRpg.Inventory` (57/57), `SurvivalRpg.UI` (13/13),
  `SurvivalRpg.Equipment` (5/5) und `SurvivalRpg.Crafting` (6/6) sind ohne
  Fehler oder ausgelassene Tests erfolgreich.
- Ein separater frischer UE-5.8-Commandlet-Prozess kompiliert alle drei neuen
  Widget-Blueprints ohne Fehler und lädt Registry sowie dedizierte
  BaseTerminal-Action-Tabelle erneut. Dabei wurden keine Assets gespeichert.
- Das alte `CUI_BaseTerminal` und die zugehörige Legacy-Resource-List sind nach
  Referenz-, Clean-Cook-, Stage- und packaged Verifikation entfernt.
  Interaktive Mouse-, Gamepad-, Fokus-, Actionbar- und Drag-Prüfungen sowie der
  kanonische Primary-Mismatch mit echtem OwningPlayer bleiben offen.
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
  10. [x] Quick-Access-Radial und Gear-Drag-Visual in die gemeinsame UI-/Input-
     Komposition überführen.
  11. [x] Orphan-Assets und Notfall-Fallbacks erst nach Referenz-, Cook- und
     Packaged-Prüfung entfernen.

Verifizierter Legacy-Retirement-Schnitt vom 2026-07-19:

- Ein frischer Asset-Registry-Bericht bestätigte für 14 Legacy-Packages keine
  externen Referencer. Entfernt wurden `BP_DragOperation`, `CUI_Hotbar_Old`,
  `CUI_ActionBar_Old`, die alten Storage-/BaseTerminal-/Crafting-Screens samt
  Legacy-Leaves sowie `CUI_Inventory` und `CUI_InventorySlotEntry`.
- Der geschlossene C++-Pfad aus `RpgInventoryTileView` und
  `RpgInventorySlotEntryWidget` ist gelöscht. Coordinator, Controller-Event und
  `URpgInventoryEntryViewModel` enthalten keine TileView-Branches oder den
  linearisierten Legacy-`SlotIndex` mehr.
- `URpgGameUIPolicy` und `URpgPrimaryGameLayout` besitzen keine nativen
  Root-Layout-/Layer-Fallbacks mehr. Address-, Gear- und Spatial-Drags starten
  ohne exakte authored Drag-Visual-Klasse keine Interaction-Session; Spatial-
  und Screen-Ghosts verwenden ausschließlich ihre erforderlichen authored
  Canvas-Hosts.
- Der permanente Test `SurvivalRpg.Inventory.UI.LegacyAssetRetirement`
  erzwingt Abwesenheit, fehlende Referencer, aktive Registry-Abhängigkeiten und
  den korrigierten Always-Cook-Pfad von `DA_RpgGameData`.
- Ein expliziter UE-5.8-Non-Unity-Editor-Build ist erfolgreich.
  `SurvivalRpg.Inventory` (57/57), `SurvivalRpg.UI` (14/14),
  `SurvivalRpg.Equipment` (5/5) und `SurvivalRpg.Crafting` (6/6) sind ohne
  Testwarnungen, Fehler oder ausgelassene Tests erfolgreich.
- Ein Clean `BuildCookRun` baute Editor- und Game-Target, kochte die vier Maps
  `BootMenu`, `MainMenu`, `Lvl_ThirdPerson` und
  `Lvl_PortalRealm_RiftGruntTrial`, stagte 967 Packages und erzeugte
  Pak/IoStore sowie das archivierte Development-Package erfolgreich.
- `ReferencedSet.txt` und `Manifest_UFSFiles_Win64.txt` enthalten alle
  erwarteten Spatial-Screens und keinen der 14 entfernten Package-Namen.
  Packaged-Headless-Smokes für BootMenu→MainMenu und `Lvl_ThirdPerson`
  beendeten mit Exitcode 0; die Gameplay-Map lud
  `RpgPrototypeExperience` und aktivierte ihre GameFeatures ohne Missing-
  Package-, Linker-, Streaming-, Blueprint- oder GameFeature-Fehler.

Verifizierter Indicator-/HUD-Composition-Schnitt vom 2026-07-19:

- `CUI_RpgHudLayout` authoriert genau eine direkt unter dem Root liegende,
  back-most, fullscreen und hit-test-invisible `RPG Indicator Layer`.
  `RpgPrototypeExperience` pusht dieses HUD genau einmal auf `UI.Layer.Game`;
  `LAS_Rpg_StandardUI` ergänzt den zuständigen Indicator-Manager genau einmal
  am RPG-PlayerController.
- `URpgIndicatorManagerComponent` ist jetzt ausschließlich Registry und
  Delegate-Quelle. Der native `RpgIndicatorHostWidget` sowie dessen
  `CreateWidget`-/`AddToPlayerScreen`-/`RemoveFromParent`-Lifecycle sind
  entfernt. Im Projekt-C++ verbleiben keine direkten
  `AddToPlayerScreen`-/`AddToViewport`-Aufrufe.
- Ein frischer UE-5.8-Editor-Build ist erfolgreich.
  `SurvivalRpg.UI.Indicator` (2/2), `SurvivalRpg.UI` (16/16) und
  `SurvivalRpg.Inventory` (57/57) liefen in frischen Commandlet-Prozessen ohne
  fehlgeschlagene oder ausgelassene Tests und jeweils mit Exitcode 0.
- Ein vollständiger Clean-`BuildCookRun` baute Game- und Editor-Target, kochte
  `BootMenu`, `MainMenu`, `Lvl_ThirdPerson` und
  `Lvl_PortalRealm_RiftGruntTrial`, stagte 967 Packages und erzeugte Pak,
  IoStore, Package und Archiv erfolgreich.

Verifizierter Respawn-/Modal-Composition-Schnitt vom 2026-07-19:

- `UI.Screen.Respawn` wählt über `DA_RpgUIScreenRegistry` ausschließlich
  `CUI_RespawnScreen` auf `UI.Layer.Modal`; Streaming suspendiert Eingabe und
  der Screen bleibt pro LocalPlayer eine Single Instance.
- `CUI_RespawnScreen` ist ein graphfreies, explizit `Never` tickendes
  Designer-Asset mit sichtbarem `RootOverlay`, Respawn-Panel und
  `RespawnButton`. Der native `URpgRespawnScreenWidget` konsumiert Back,
  besitzt Menu-Input/Fokus und verwendet einen begrenzten Timer statt
  Tick-Polling.
- `ARpgPlayerState` bleibt die replizierte Respawn-Wahrheit.
  `ARpgPlayerController` öffnet beziehungsweise schließt nur den Registry-
  Screen; der Button sendet weiterhin lediglich den bestehenden
  serverautoritativen `RequestRespawn`.
- Der Blueprint-Pfad aus `CreateWidget`, `AddToViewport`,
  `RemoveFromParent`, manuellen InputModes, Cursor-Steuerung und
  `DeathWidget` ist aus `BP_Rpg_PlayerController` entfernt.
  `DeathTest` hatte in einem frischen Asset-Referenzscan keine Referencer und
  wurde anschließend gelöscht.
- Ein frischer UE-5.8-Editor-Build ist erfolgreich.
  `SurvivalRpg.UI` (18/18), `SurvivalRpg.Inventory` (57/57),
  `SurvivalRpg.Crafting` (6/6) und `SurvivalRpg.Equipment` (5/5) liefen in
  frischen Commandlet-Prozessen ohne fehlgeschlagene oder ausgelassene Tests.
- Ein zusätzlicher Clean-`BuildCookRun` baute Editor- und Game-Target, kochte
  `BootMenu`, `MainMenu`, `Lvl_ThirdPerson` und
  `Lvl_PortalRealm_RiftGruntTrial`, stagte 967 Packages und erzeugte Pak,
  IoStore, Package sowie das archivierte Development-Build erfolgreich.
- `ReferencedSet.txt` und das archivierte `Manifest_UFSFiles_Win64.txt`
  enthalten `DA_RpgUIScreenRegistry` und `CUI_RespawnScreen`; `DeathTest`
  kommt in beiden mit null Treffern nicht mehr in der Liefermenge vor.
- Ein interaktiver packaged Forced-Death-/Respawn-Smoke bleibt für diesen
  Teilschnitt offen.

Verifizierter Frontend-/CommonUI-Abschlussschnitt vom 2026-07-19:

- `BP_BootMenu_Gamemode` und `BP_MainMenuGameMode` erben vom schlanken
  `ARpgFrontendGameModeBase` und konfigurieren ausschließlich
  `UI.Screen.Boot` beziehungsweise `UI.Screen.MainMenu`.
  `ACommonPlayerController` liefert den CommonGame-LocalPlayer-Lifecycle;
  `ARpgFrontendHUD` öffnet und schließt nur den konfigurierten Registry-Tag.
- `CUI_BootMenu` ist ein graphfreier `URpgBootScreenWidget`. Seine drei
  sichtbaren Splash-Seiten bleiben authoriert; der native Presenter besitzt
  die expliziten Timings 1/2/2 Sekunden und reist anschließend nach
  `MainMenu`. `CUI_MainMenuStack` bleibt die sichtbare Authority für seine
  fünf Stacks und pusht `CUI_MainMenu` als erste Seite.
- Die fünf bestehenden MainMenu-Navigationsfunktionen bleiben als native,
  LocalPlayer-sichere Kompatibilitäts-API erhalten. Unbekannte World-Kontexte
  schlagen geschlossen fehl, statt implizit Player 0 zu verwenden.
  `BP_BootMenuHud`, `BP_MainMenuHud` und `BP_MainMenuController` hatten keine
  Referencer und wurden entfernt.
- Der Screen-Router hält jeden `FStreamableHandle` bis `AfterPush`, behält
  abgebrochene Tags bis zum terminalen Callback und entfernt ein trotz
  Cancellation fertig initialisiertes Widget erst nach der Registrierung.
  `SurvivalRpg.UI.ScreenRouter.AsyncCloseLifecycle` deckt Close-vor-Initialize
  und Close-während-Initialize dauerhaft ab.
- Ein Package-Redirect für das verschobene `SG_Settings` erhält alte
  Audio-Settings-Saves. Der MainMenu-Runtime-Smoke lädt den alten lokalen Save
  ohne `Setting Ref`-/`Accessed None`-Warnungen.
- Ein frischer UE-5.8-Editor-Build ist erfolgreich.
  `SurvivalRpg.UI` (20/20), `SurvivalRpg.Inventory` (57/57),
  `SurvivalRpg.Crafting` (6/6) und `SurvivalRpg.Equipment` (5/5) liefen in
  frischen Commandlet-Prozessen ohne fehlgeschlagene oder ausgelassene Tests.
  Als bekannte, unabhängige Warnungen bleiben vier fehlende Settings-
  Stringtable-Einträge und der bestehende Never-Tick-Hinweis des
  Respawn-Widgets.
- Ein Clean-`BuildCookRun` baute Editor- und Game-Target, kochte die vier Maps
  `BootMenu`, `MainMenu`, `Lvl_ThirdPerson` und
  `Lvl_PortalRealm_RiftGruntTrial`; 963 von 970 ermittelten Packages wurden
  gekocht, sieben plattformspezifisch übersprungen. Pak/IoStore/Package und
  das Development-Archiv unter `Saved/Phase1FrontendGateFinal` wurden
  erfolgreich erzeugt.
- `ReferencedSet.txt` und `Manifest_UFSFiles_Win64.txt` enthalten Boot-Screen,
  MainMenu-Stack, Screen Registry und beide Frontend-GameModes; alle drei
  entfernten Legacy-Packages kommen mit null Treffern nicht mehr vor.
  Editor- und packaged Headless-Smokes durchliefen
  `BootMenu -> MainMenu`; ein zusätzlicher packaged Gameplay-Smoke lud
  `Lvl_ThirdPerson`, `RpgPrototypeExperience`, das Primary Layout und die
  zugehörigen GameFeatures ohne Blueprint-, Linker-, Streaming- oder
  Missing-Package-Fehler.

- Weiterführende manuelle QA außerhalb des Phase-1-Abschlusskriteriums:
  den kanonischen Primary-Mismatch mit echtem OwningPlayer in PIE
  beziehungsweise einem passenden Test-Harness abdecken sowie interaktive
  Mouse-/Gamepad-, Fokus-, Drag- und gerenderte packaged Prüfungen ausführen.
- Bekannter Controller-Legacy-Restpunkt: `OnPossess` und
  `RestoreGameplayInputFocus` rufen weiterhin blind
  `SetIgnoreLookInput(false)` auf und können dadurch einen gezählten
  Look-Input-Lock eines fremden Systems abbauen. Der neue Radial-Router besitzt
  und löst ausschließlich sein eigenes balanciertes Lock; die beiden globalen
  Fokus-Resets werden in einem separaten Input-Lifecycle-Schnitt bereinigt.

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

Status: **Abgeschlossen**

- [x] Öffentliche Low-Level-Add/Remove/Move/Sort-Blueprintfläche deprecaten.
- [x] Schmale Intents für Grant/Bootstrap, Consume, Move, Transfer, Drop und
      Restore anbieten.
- [x] Remove/Consume/Drop grundsätzlich subtree-sicher machen.
- [x] Raw-Add gegen fremden Outer, doppelte Item-ID und bereits enthaltene
      Instanzen absichern.
- [x] Physisches Equippen ausschließlich über Inventory-Transaktionen führen.
- [x] Eine öffentliche Placement-Auswertung als gemeinsamen Vertrag für Move,
      Equip, Split, Add, Transfer, Auto-Placement und Restore verwenden.
- [x] UI-Preview aus einem echten Mutation-Plan ableiten; konkrete Belegung,
      Swap und dynamische Handkonflikte müssen mit dem Server-Commit
      übereinstimmen.
- [x] `URpgEquipmentLoadoutComponent` auf Aktivierung der Hände und
      Reconciliation des physischen Gear-Zustands reduzieren.

Verifizierter Phase-2A-Zwischenstand vom 2026-07-20:

- `GrantItemDefinition`, `BootstrapItemInstance` und
  `CanBootstrapItemInstance` bilden den ersten schmalen Intent-Schnitt.
  Die gemeinsame Intent-Checkbox bleibt offen, bis auch Consume, Move,
  Transfer, Drop und Restore auf den Zielvertrag reduziert sind.
- Öffentliche Low-Level-Add-, Remove-, Move- und Sort-Funktionen bleiben für
  Blueprint-Migration reflektiert, sind aber mit konkreten
  `DeprecatedFunction`-Hinweisen versehen. Aktive Production-Grants verwenden
  den Grant-/Bootstrap-Pfad; die in diesem Schnitt migrierten
  inventory-eigenen Instanzen wechseln Inventories über den atomaren
  Cross-Inventory-Transfer.
- Raw-Add akzeptiert nur Instanzen mit exakt passendem Actor-Outer, gültiger
  Definition und Item-ID. Bereits enthaltene Pointer, doppelte persistente IDs,
  fremde Outer und eine zweite Referenz aus einem Sibling-Inventory desselben
  Actors werden abgelehnt. Fremde, nicht verwaltete Setup-Instanzen werden beim
  expliziten Bootstrap unter dem Zielactor mit frischer ID rekonstruiert.
- Räumliche Transfer-Preflights sind vom Raw-Add-Vertrag getrennt, damit
  Preview und Cross-Inventory-Commit eine noch im Quellinventar verwaltete
  Instanz prüfen können, ohne die Ownership-Guards zu umgehen.
- Vollständige Player-Quick-Transfers validieren die Lösbarkeit von
  Equipment-Zuweisungen vor dem physischen Commit und leeren den Mirror erst
  nach einem erfolgreichen Transfer. Fehlgeschlagene Transfers verändern
  dadurch weder Hände noch Remembered-Offhand-Zustand.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut.
- `SurvivalRpg.Inventory`: 60 von 60 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 2: 2 von 8 Punkten abgeschlossen (25,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 50 von 94 Punkten
  abgeschlossen (53,2 %), 44 Punkte offen.
- Nächster Safety-Schnitt: vollständige Container-Subtrees bei
  Remove/Consume/Drop atomar behandeln; partielle Container-Entfernung
  fail-closed ablehnen. Death-Drop und Legacy-Snapshot-Import bleiben als
  bestätigte anschließende Integritätsrisiken offen.

Verifizierter Phase-2B-Zwischenstand vom 2026-07-20:

- Exaktes Consume per persistenter Item-ID sowie Definition-Consume planen alle
  betroffenen Einträge vor der ersten Mutation. Container-Provider werden nur
  als vollständiger Subtree entfernt; partielle Entfernung wird auch bei
  fehlerhaft konfigurierten Legacy-Definitionen fail-closed abgelehnt.
- Physische Drops übertragen konkrete Runtime-Instanzen mitsamt persistenter
  Item-ID und Hierarchie. Unzureichende Zielkapazität bleibt atomar, während
  bereits überfüllte Quellen durch gültige Transfers schrittweise schrumpfen
  dürfen. Der Death-Drop-Container erweitert sein Root-Grid bei Bedarf, statt
  Restitems still im Spielerinventar zu belassen.
- Manuelle Drops prüfen die Drop-Policy des vollständigen Subtrees und ändern
  Hand-/Remembered-Zuweisungen erst nach erfolgreichem Transfer. Death Drop
  schützt ausgerüstete Gear-Roots samt Descendants und entfernt
  Blueprint-`StaticInventory` aus frisch erzeugten Runtime-Loot-Proxys.
- Item-Use validiert positive Use-Zahlen und Multiplikationsüberläufe,
  konsumiert über den exakten Intent und bereinigt Equipment-Zuweisungen auch
  bei verzögerten Ability-Abschlüssen genau einmal.
- Import- und Removal-Callbacks sehen erst einen vollständig stabilen
  Inventarzustand. Cross-Inventory-Transfer importiert weiterhin vollständige
  Graphen; atomare Benachrichtigungsbündel, Request-Fingerprints und
  inkrementelle Deltas bleiben deshalb ausdrücklich Phase 3.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut.
- `SurvivalRpg.Inventory`: 65 von 65 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 2: 3 von 8 Punkten abgeschlossen (37,5 %).
- Gesamtfortschritt der verbindlichen Checkliste: 51 von 94 Punkten
  abgeschlossen (54,3 %), 43 Punkte offen.
- Nächster Safety-Schnitt: die noch gemischte öffentliche Oberfläche auf die
  schmalen Move-/Transfer-/Drop-Intents reduzieren. Anschließend folgen
  physisches Equippen über Inventory-Transaktionen und der gemeinsame
  Placement-Vertrag. Legacy-Snapshot-Restore, Collect-Ende-zu-Ende-Rollback,
  leere Drop-Actor-Lebensdauer und Ability-Effekt-vor-Consume bleiben gezielte
  Integrationsrisiken.

Verifizierter Phase-2C-Zwischenstand vom 2026-07-20:

- Grant/Bootstrap, Consume, Move, Transfer, Pickup, Drop und Restore besitzen
  schmale Gameplay-Intents. Move und Transfer binden Item-ID, Entry-ID,
  Quellplatzierung und erwartete Menge an einen Request; die generische
  Blueprint-Mutationsfläche ist deprecatet und für aktive UI-Pfade auf Split
  und Sort beschränkt.
- Request-IDs sind an den vollständigen Payload einschließlich Ziel und
  Partial-Stack-Policy gebunden. Begrenzte Replay-Caches schützen Manager,
  Quick-/Exact-Transfer sowie manuelle und physische Drops vor doppelten
  Mutationen und wiederholten Loadout-Seiteneffekten. Ein echter Disk-Restore
  eröffnet bewusst eine neue Request-Epoche.
- Der Profil-Restore wird pro PlayerController verfolgt. Persistierte
  Platzierungen mit abweichendem aktuellem Footprint oder Rotationsvertrag
  werden fail-closed abgelehnt; der interne Runtime-Recovery-Import bleibt vom
  Disk-Restore getrennt.
- Drop-Actor-Ersetzung, Crafting-Auto-Deposit und Base-Storage-Transfers
  kompensieren fehlgeschlagene Folgeschritte. Collect erhält den
  `StaticInventory`-Fallback, wenn ein Drop-Actor nicht kanonisch aufgebaut
  werden konnte.
- Drag/Drop, Collect, Crafting und Equipment verwenden die typisierten
  Produktionspfade. `BP_Rpg_PlayerController` serialisiert noch Legacy-Knoten
  für `RequestTransferItemStack` und `AddItemDefinition`; deren Wrapper bleiben
  migrationssicher, Asset-Resave und endgültige Entfernung gehören zu Phase 7.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut.
- `SurvivalRpg.Inventory`: 74 von 74 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Die gezielten Wiederholungsläufe `SurvivalRpg.Inventory.Intent` (8 von 8),
  `SurvivalRpg.Inventory.Drop` (3 von 3) und
  `SurvivalRpg.Inventory.Transfer` (2 von 2) waren ebenfalls erfolgreich und
  sind in der Inventory-Gesamtsuite enthalten.
- Fortschritt Phase 2: 4 von 8 Punkten abgeschlossen (50,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 52 von 94 Punkten
  abgeschlossen (55,3 %), 42 Punkte offen.
- Nächster Safety-Schnitt: physisches Equippen ausschließlich über
  Inventory-Transaktionen führen und danach den gemeinsamen Placement-Vertrag
  für Commit und Preview etablieren. In-place-Transfer-Deltas und ein
  callback-atomarer gemeinsamer Commit bleiben Phase 3.

Verifizierter Phase-2D-Zwischenstand vom 2026-07-21:

- Gear und Carry werden für Spieler ausschließlich durch Inventory-Transaktionen
  verändert. Das Loadout spiegelt den physischen Gear-Zustand und verwaltet nur
  die aktiven Hände; der direkte Runtime-Equipment-Pfad für NPC-Combat-Loadouts
  bleibt als bewusst getrennte Ausnahme bestehen.
- Typisierte, pointer-freie Equipment-Intents binden Item-ID, Entry-ID,
  Quellplatzierung und Menge an den Request. Veraltete generische
  Equipment-Aktionen werden serverseitig abgelehnt; Replay und
  Request-ID-Kollisionen bleiben ohne zweite Mutation.
- Der vertrauenswürdige `Operation::Equip`-Pfad erhält die konkrete
  Item-Identität und verwendet bei belegten Equipment-Zellen einen atomaren
  Swap statt Stack-Merge. Same-Player-Preview und Commit teilen diesen Vertrag;
  Equipment-Drag-Payloads ohne exakten oder mit veraltetem Snapshot werden
  abgelehnt.
- Die Loadout-Reconciliation entfernt zunächst abweichende Runtime-Instanzen
  und erzeugt danach fehlende Zielinstanzen. Dadurch bleiben unveränderte
  Instanzen bestehen, Slotwechsel verdoppeln keine GAS-Grants, Zwei-Hand-Wechsel
  räumen Main- und Offhand genau einmal auf und slot-spezifisches Leeren lässt
  die jeweils unabhängige andere Hand bestehen.
- Generische Split-/Sort-Intents synchronisieren abgeleitete Loadout-Zustände
  nur nach einer tatsächlichen Gear-/Carry-Mutation; Replays lösen keine
  erneuten Seiteneffekte aus.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut.
- `SurvivalRpg.Inventory`: 79 von 79 Automationtests erfolgreich.
- Der fokussierte Lauf `SurvivalRpg.Inventory.Intent.Equip`: 5 von 5
  Automationtests erfolgreich und in der Inventory-Gesamtsuite enthalten.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 2: 5 von 8 Punkten abgeschlossen (62,5 %).
- Gesamtfortschritt der verbindlichen Checkliste: 53 von 94 Punkten
  abgeschlossen (56,4 %), 41 Punkte offen.
- Nächster Safety-Schnitt: die öffentliche Placement-Auswertung als
  gemeinsamen Vertrag verbreitern und UI-Previews vollständig aus dem echten
  Mutation-Plan ableiten. Danach wird die öffentliche Legacy-Fläche des
  Loadouts auf Hand-Aktivierung und Reconciliation reduziert.

Verifizierter Phase-2E-Zwischenstand vom 2026-07-21:

- `FRpgInventoryPlacementQuery`, die benannten Subject-Factories und
  `FRpgInventoryPlacementPlan` bilden einen öffentlichen, side-effect-freien
  C++-Vertrag. Move, Equip, Split, Add, Transfer, FirstFit-Auto-Placement und
  Restore verwenden dieselbe Auswertung; Commit-Pfade konsumieren die
  abgeleiteten Place-, Merge-, Swap- und NoOp-Schritte.
- Der Vertrag trennt die Operationen bewusst: Move darf kompatibel mergen und
  genau ein Ziel swappen, Equip darf swappen aber nie mergen, Split platziert
  nur eine neue konkrete Instanz, Transfer darf mergen oder platzieren aber
  nicht cross-inventory swappen, und Restore akzeptiert ausschließlich exakt
  gestagte Place-Schritte.
- Add unterscheidet Definition-/Generated-Grants von detached konkreten
  Instanzen. Nur generierte Grants dürfen deterministisch in kompatible
  Stacks mergen oder über mehrere neue Stacks auffächern; detached Identitäten
  bleiben erhalten. Runtime-State-Kompatibilität entscheidet über Merge statt
  nur die statische Item-Definition.
- Source-gebundene Operationen validieren Item-ID, Entry-ID, die rohe
  vollständige Quellplatzierung und `ExpectedSourceQuantity`. Transfer-,
  QuickTransfer- und ManualDrop-Requests sowie ihre Replay-Fingerprints binden
  die vollständige Quellmenge getrennt von der gewünschten Teilmenge.
- Deterministisches FirstFit berücksichtigt Container-Reihenfolge, Zelle und
  Rotation. Entry-Capacity zählt vollständige transferierte Subtrees;
  Descendant-Tiefe und Container-Regeln werden vor dem Commit validiert.
- Restore wertet jede Zeile gegen batch-lokale Grid-/Occupancy-Scratch-Daten
  aus. Dadurch kann kein gespeicherter Overlap durch Live-Graph-Ausnahmen
  rutschen. Root-SingleCell-Normalisierung greift nur bei echten Root-Handles;
  ein item-owned Container mit derselben lokalen `FName`-ID behält seinen
  eigenen Spatial-Vertrag.
- Der identitätserhaltende physische Drop akzeptiert für seine konkrete
  Zielzelle ausschließlich einen `Place`-Schritt. Der deprecated
  Live-Snapshot-Adapter erneuert seinen Snapshot nach Source-/Target-Restore-
  Epochen; wirklich fehlende Items liefern `ItemNotFound`, vorhandene aber
  veraltete Snapshots `SourceMismatch`.
- Manual-Drop-Presenter vergleichen Entry-ID, Placement und vollständige
  Quellmenge mit der präsentierten Entry-/Address-View. Eine veraltete UI kann
  dadurch keinen neuen Request gegen einen inzwischen geänderten Stack
  erzeugen.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut.
- `SurvivalRpg.Inventory`: 83 von 83 Automationtests erfolgreich.
- Die fokussierten Läufe `SurvivalRpg.Inventory.PlacementEvaluator` (4 von 4),
  `SurvivalRpg.Inventory.Intent.Transfer` (1 von 1),
  `SurvivalRpg.Inventory.Drop` (3 von 3),
  `SurvivalRpg.Inventory.QuickTransfer` (1 von 1) und der ergänzte
  Stale-Quantity-Presenter-Test waren erfolgreich und sind in der
  Inventory-Gesamtsuite enthalten.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 2: 6 von 8 Punkten abgeschlossen (75,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 54 von 94 Punkten
  abgeschlossen (57,4 %), 40 Punkte offen.
- Nächster Safety-Schnitt: UI-Previews vollständig aus dem echten
  Mutation-Plan ableiten. Der callback-atomare gemeinsame In-place-Commit für
  zwei Inventare bleibt bewusst Phase 3; anschließend wird die öffentliche
  Legacy-Fläche des Loadouts auf Hand-Aktivierung und Reconciliation reduziert.

Verifizierter Phase-2F-Zwischenstand vom 2026-07-21:

- `FRpgInventoryInteractionPreviewPlan` ist der native, screen-lokale
  Projektionsvertrag zwischen Domain-Plan und Darstellung. Er behält den
  vollständigen `FRpgInventoryPlacementPlan` in C++, während Blueprint/MVVM
  weiterhin nur Semantik und normalisierte Präsentationsplatzierung sehen.
  Der Client sendet keinen gecachten Plan; jeder Server-Gateway wertet den
  pointer-freien Intent erneut gegen den aktuellen Graphen aus.
- Same-Inventory-Move, Equipment-Move, exakter Cross-Inventory-Transfer und
  FirstFit-QuickTransfer erzeugen ihre Vorschau aus `EvaluatePlacement`.
  Die frühere Widget-Inferenz über Overlap, gleiche Item-Definition und freie
  Stack-Kapazität ist entfernt. Runtime-inkompatible Stacks können dadurch
  nicht mehr fälschlich als Merge erscheinen.
- Abstrakte Gear- und Handziele werden vor der Darstellung auf eine konkrete
  Gear-/Carry-Platzierung aufgelöst. Belegte Equipment-Ziele zeigen den
  tatsächlichen Swap samt verdrängter Item-/Entry-ID und Zielplatzierung;
  ein aktives Zwei-Hand-Item blockiert OffHand bereits im selben Plan, den der
  Commit erneut konsumiert.
- `PlanExactTransferPlacement`, `PlanQuickTransferDestination` und
  `PlanEquipmentIntentPlacement` bündeln Access-, Direction-, Snapshot-,
  Loadout- und Placement-Prüfung. Vollständige Drags akzeptieren nur
  `IsCompleteSuccess`; partielle Fits bleiben sichtbar im Domain-Plan, dürfen
  aber keinen vollständigen Commit vortäuschen.
- Source-Snapshots vergleichen neben Item-/Entry-ID, Menge, Footprint und
  Rotation auch den rohen Legacy-`ContainerId`. Preview, Replay-Fingerprint
  und autoritativer Manager lehnen dadurch denselben stale Snapshot ab.
- Spatial-Grid-Widgets evaluieren pro Kandidat einmal im Coordinator und
  publizieren daraus Ghost, Zellfarben und Session-Semantik. Das Request-Ziel
  bleibt unverändert servervalidierbar; nur die Darstellung verwendet die
  normalisierte Plan-Platzierung. Entry-/Address-Änderungen planen einen
  aktiven Hover auch ohne neue Pointerbewegung erneut. Wiederverwendete
  Item-Widgets bündeln ihre Reconciliation-Setter und führen danach genau eine
  semantische Target-Auswertung aus, ohne einen Domain-Plan zu speichern.
- Eine Interaction-Session erlaubt global nur einen Pending-Request. Fehlendes
  oder fremdes Action-Feedback kann diesen Request unabhängig vom Target nicht
  mehr lösen; ausschließlich die exakt korrelierte `RequestId` beendet den
  Feedback-Pfad. Separat geprüfte replizierte Zustands-Acknowledgements bleiben
  davon unberührt.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (8/8 Build-Actions nach den finalen Audit-Korrekturen).
- `SurvivalRpg.Inventory`: 89 von 89 Automationtests erfolgreich. Die
  erweiterten Tests prüfen konkrete Place-/Merge-/Swap-Schritte,
  `DisplacedPlacement`, partiellen Fit, Runtime-State-Inkompatibilität,
  stale Legacy-Snapshots, Gear-Zielauflösung, dynamische Handkonflikte,
  blockiertes Gear-Clear bei vollem Content, globales Pending mit echter
  Feedback-Korrelation sowie exakte Preview-/Commit-Zielparität für
  Cross-Inventory-, FirstFit- und Hand-Equip-Pfade.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 2: 7 von 8 Punkten abgeschlossen (87,5 %).
- Gesamtfortschritt der verbindlichen Checkliste: 55 von 94 Punkten
  abgeschlossen (58,5 %), 39 Punkte offen.
- Nächster Safety-Schnitt: `URpgEquipmentLoadoutComponent` auf öffentliche
  Hand-Aktivierung und Reconciliation des physischen Inventory-Zustands
  reduzieren. Netzwerk-/Late-Join- und interaktive Mouse-/Gamepad-Smokes
  bleiben ergänzende Integrations-QA, nicht Ersatz für die autoritative
  Re-Evaluation.

Verifizierter Phase-2G-Abschlussstand vom 2026-07-21:

- Der Inventory-Graph ist die alleinige physische Wahrheit für Gear und Carry.
  `URpgEquipmentLoadoutComponent` besitzt keine öffentliche Blueprint-Mutation
  und keinen Server-RPC mehr; reflektiert bleiben nur read-only Slot-, Last-,
  Tier- und Dodge-Projektionen. Handaktivierung, Pawn-Lifecycle,
  Reconciliation und Selection-Persistenz sind schmale native C++-Seams.
- Physische Equipment-Aktionen laufen ausschließlich über den
  `URpgInventoryUiActionComponent`. Starter-Equipment baut denselben
  pointer-freien Intent aus dem aktuellen Entry-Snapshot und fragt keinen
  parallelen Loadout-Mirror mehr ab. Transfer, Consume und Drop bereinigen
  Hände erst nach erfolgreicher physischer Mutation durch Reconciliation.
- Nicht-Hand-Gear wird als vollständiger Snapshot aus den kanonischen
  Gear-Platzierungen importiert. Wiederholte Reconciliation erhält dieselben
  Runtime-Instanzen und GAS-Grants; aktive Hände bleiben reine Auswahl über
  physisch passende Carry-Rollen. Die eigenständige Definition-zu-Manager-Seam
  für NPC-/Runtime-Equipment bleibt ohne Player-Loadout nutzbar.
- Controller-private Slots, Remembered-Offhands sowie Last/Tier replizieren
  `COND_OwnerOnly`. Die pawn-relevante `EquipmentList` des
  `URpgEquipmentManagerComponent` bleibt `COND_None`, damit Remote-Clients und
  Late Joiner die sichtbaren Runtime-Actors aus Pawn-Zustand rekonstruieren
  können.
- Der Abschlusslauf deckte zusätzlich einen Phase-2F-Fall-through auf:
  QuickTransfer gab bei einem vollen bevorzugten Container den letzten
  verworfenen Swap-Plan als Erfolg zurück und übersprang dadurch ein späteres
  freies Ziel. Der Planner behält nun nur abgelehnte Kandidaten als Fallback;
  die Regression wählt wieder deterministisch das freie Belt-Grid.
- Der Asset-Audit fand keinen Blueprint-Knoten mit
  `RpgEquipmentLoadoutComponent` als Funktions-Owner. Der gezielte Compile von
  `BP_Rpg_PlayerController` endete mit 0 Fehlern und 14 bekannten
  Migrationswarnungen aus den verbleibenden Inventory-/UiAction-Adaptern, nicht
  aus der entfernten Loadout-Fläche.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (4/4 Actions im finalen inkrementellen Build).
- `SurvivalRpg.Inventory`: 90 von 90 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 2: 8 von 8 Punkten abgeschlossen (100,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 56 von 94 Punkten
  abgeschlossen (59,6 %), 38 Punkte offen.
- Ein echter Zwei-Client-PIE-Late-Join sowie interaktive Mouse-/Gamepad-Smokes
  bleiben manuelle Integrations-QA und werden durch die automatisierten
  Replikationsbedingungen nicht als ausgeführt behauptet.
- Nächster Schnitt nach Wiederaufnahme: Phase 3, Runtime-Transfer vom
  Save-/Load-Rekonstruktionspfad trennen.

## Phase 3 – Runtime-Transfer vom Save/Load trennen

Status: **Abgeschlossen**

- [x] Source und Target vollständig vorvalidieren.
- [x] Transfer als In-place-Delta atomar committen.
- [x] Nur den übertragenen Subtree für den neuen Actor-Outer rekonstruieren.
- [x] UObject- und EntryId-Identität aller überlebenden und unbeteiligten
      Items beim aktuellen Graph-Commit erhalten.
- [x] Notifications und FastArray-Deltas einmal pro Commit bündeln.
- [x] Rollback ohne extern sichtbaren Zwischenzustand sicherstellen.
- [x] Batch-Pickup und Collect gegen Scratch-Occupancy planen.

Verifizierter Phase-3A-Zwischenstand vom 2026-07-22:

- Der Runtime-Transfer verwendet keinen Export-/Import- oder Save-Graph-Pfad
  mehr. Die interne Import-Oberfläche enthält keine Transfer-Ausnahmen für
  Source-Inventare oder temporäre Überkapazität mehr.
- Source- und Target-Graph, Revisionen, Platzierungsplan, Merge-Ziele,
  Subtree-Beziehungen, Kapazität sowie persistente Item- und Entry-Identitäten
  werden vor dem Commit geprüft und unmittelbar davor gegen den Live-Zustand
  revalidiert. Legacy-Partial ist für generische Transfer-/Drop-Intents
  fail-closed; nur der explizite Pickup-Intent darf partiell anwenden.
- Vollständige Same-Actor-Transfers ohne Merge verwenden die bestehende Item-
  Instanz mit frischer Ziel-`EntryId` weiter. Cross-Actor-Transfers
  rekonstruieren ausschließlich den bewegten Subtree unter dem neuen Actor-
  Outer; unbeteiligte und überlebende Items behalten UObject- und Entry-
  Identität. Vorbereitete und gerade entfernte Instanzen bleiben bis zum Ende
  synchroner Callbacks GC-sicher referenziert.
- Nur geänderte beziehungsweise neue FastArray-Zeilen werden dirty markiert;
  strukturelles `MarkArrayDirty` und Inventory-Revision erfolgen pro
  betroffenem Inventar genau einmal. Die Subobject-Registrierung ändert sich
  nur je tatsächlich entfernter oder hinzugefügter Instanz. Der Replay-Cache
  wird vor Benachrichtigungen veröffentlicht, und alle Listener sehen bereits
  beide finalen Graphen. Der Persistenz-`MutationEpoch` bleibt von Runtime-
  Transfers unberührt.
- Vier neue Transfer-Delta-Tests decken Merge-plus-Place mit reentrantem
  Request-Replay, fail-closed Legacy-Partial, Same-Actor-UObject-Reuse und die
  Cross-Actor-Rekonstruktion eines Container-Subtrees ab. Sie prüfen außerdem
  exakte Benachrichtigungs-, Revisions-, Identitäts- und Seiteneffektverträge.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (4/4 Actions im finalen inkrementellen Build).
- `SurvivalRpg.Inventory`: 94 von 94 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 3: 6 von 7 Punkten abgeschlossen (85,7 %).
- Gesamtfortschritt der verbindlichen Checkliste: 61 von 94 Punkten
  abgeschlossen (64,9 %), 33 Punkte offen.
- Die Standalone-Automation prüft Server- und Callback-Atomizität sowie den
  finalen Zustand für spätere Leser, aber keine echten ActorChannel-Pakete.
  Zwei-Client-PIE, FastArray-/Subobject-Replikation und Late Join bleiben
  deshalb gezielte Netzwerk-QA und werden nicht als ausgeführt behauptet.
- Nächster Schnitt: Batch-Pickup und Collect als gemeinsamen, vollständig
  vorvalidierten Scratch-Occupancy-Plan ausführen, ohne zwischen Items auf
  Save-Graph-Rollback zurückzufallen.

Verifizierter Phase-3B1-Zwischenstand vom 2026-07-22:

- Detached und statische `FInventoryPickup`-Payloads werden über einen
  Manager-eigenen Batch-Plan gegen einen gemeinsamen Stack-, Entry-, Kapazitäts-
  und Spatial-Occupancy-Scratch-Zustand geprüft. Der vollständige Plan wird vor
  dem Commit gegen den Live-Zustand revalidiert und als eine FastArray-/
  Revisionsmutation veröffentlicht; ein Fehlschlag hinterlässt keine Teilmenge.
- Definition-Templates dürfen deterministisch in vorhandene Stacks mergen und
  über mehrere neue Stacks auffächern. Explizite Instanzen behalten den
  bisherigen Bootstrap-Vertrag und werden nicht gemergt: lokale, noch nicht
  verwaltete Instanzen behalten ihre Identität; fremde Instanzen werden mit
  neuem actorweit eindeutigem `ItemId` und ihrem exportierten Runtime-Zustand
  rekonstruiert. Bereits verwaltete oder kollidierende Identitäten schlagen
  fail-closed fehl.
- `IPickupable` und der statische Collect-Pfad delegieren auf denselben Batch-
  Vertrag. Doppelte Kapazitäts-/Stack-Vorprüfungen sowie Save-Graph-Rollback in
  den Adaptern sind entfernt. Der generische Helper lehnt einen kanonischen
  `ARpgDroppedInventoryActor` bewusst ab, damit dessen Source-Inventar nicht
  dupliziert werden kann; dessen source-owned Multi-Root-Transfer folgt in 3B2.
- Fünf neue Tests decken gemeinsamen Scratch-Reject ohne Mutation,
  deterministische Commit-Reihenfolge, Merge plus detached Instanz mit
  Kapazität und Reentrancy, actorweite Identität beziehungsweise Managed-
  Instance-Reject sowie den Duplizierungsschutz des kanonischen Drop-Helpers ab.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (6/6 Actions im finalen inkrementellen Build).
- `SurvivalRpg.Inventory.PickupBatch`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Inventory`: 99 von 99 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 3 bleibt 6 von 7 Punkten (85,7 %), weil der letzte
  Checklistenpunkt beide Pickup-Quellen umfasst.
- Gesamtfortschritt der verbindlichen Checkliste bleibt 61 von 94 Punkten
  (64,9 %), 33 Punkte offen.
- Nächster Schnitt: die kanonischen Drop-Roots als einen source-owned
  Collect-as-much-as-fits-Batch über gemeinsamen Target-Scratch planen und mit
  genau einer Source- und Target-Revision committen. Partial-Stacks bleiben
  erlaubt; Provider-/Container-Subtrees werden weiterhin nur vollständig
  übertragen.

Verifizierter Phase-3B2-Abschluss vom 2026-07-22:

- Der kanonische Drop-Collect plant alle physischen Source-Roots in
  deterministischer Reihenfolge gegen einen gemeinsamen Target-Scratch. Die
  authored Reihenfolge der Content-Container bleibt erhalten; ein unpassender
  Root blockiert kleinere, später passende Roots nicht.
- Gewöhnliche Stacks dürfen vorhandene kompatible Stacks auffüllen und
  teilweise in der Source verbleiben. Container-Provider und ihre vollständigen
  Descendant-Graphen werden weiterhin ausschließlich als unteilbarer Subtree
  übertragen.
- Source- und Target-Graph, Revisionen, Entry-Kapazität, Spatial-Occupancy,
  actorweite Item-/Entry-Identitäten und exportierter Fragment-Runtime-State
  werden vor dem Commit erfasst und unmittelbar davor revalidiert. Cross-Actor-
  Items werden unter dem dauerhaften Target-Actor rekonstruiert; Same-Actor-
  Vollmoves können ihre Instanzidentität behalten.
- Der gemeinsame Commit markiert nur tatsächlich geänderte FastArray-Zeilen,
  ändert jede Inventory-Revision genau einmal und veröffentlicht synchrone
  Notifications erst, nachdem beide finalen Graphen sichtbar sind. Exakte
  Request-Replays liefern das gecachte Ergebnis und dieselben Autoequip-IDs
  ohne eine zweite Mutation.
- Die Collect-Ability delegiert den kanonischen Pfad auf genau einen
  source-owned Batch-Aufruf. Nur tatsächlich betroffene Target-Root-IDs gehen
  an Autoequip; ein vollständig geleerter Drop kann zerstört werden, während
  ein Restbestand das Loot-Inventar öffnet.
- Fünf neue Collect-Batch-Tests decken Mixed Roots mit Shared Scratch,
  Skip-and-Continue, Provider-Subtree und Partial-Merge, vollständiges Leeren
  plus exakten Replay, Callback-Reentrancy, Root- und item-owned Target-
  Container mit Depth-Rebase, Merge-then-Place sowie leere und vollständig
  blockierte Quellen ohne Seiteneffekte ab.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (16/16 Actions im vollständigen Build, 6/6 Actions im finalen
  inkrementellen Build).
- `SurvivalRpg.Inventory.CollectBatch`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Inventory`: 104 von 104 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 3: 7 von 7 Punkten abgeschlossen (100,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 62 von 94 Punkten
  abgeschlossen (66,0 %), 32 Punkte offen.
- Die Automation prüft den autoritativen Manager-Commit und kanonische Drop-
  Quellen, aber keinen echten Zwei-Client-PIE-/Late-Join-Datenstrom und nicht
  den vollständigen GAS-/Client-RPC-Lifecycle der Collect-Ability. Diese Punkte
  bleiben gezielte Integrations-QA und werden nicht als ausgeführt behauptet.
- Nächster Schnitt nach Wiederaufnahme: Phase 4 – Datenmodell und Persistenz.

## Phase 4 – Datenmodell und Persistenz

Status: **Abgeschlossen**

- [x] Aktuelle Footprints und Rotationsregeln beim Import aus der
      Item-Definition rekonstruieren.
- [x] Einen kanonischen Stack-Key für Runtime-State-Kompatibilität einführen.
- [x] Legacy Snapshot nur noch über einen versionierten Konverter zulassen und
      anschließend entfernen.
- [x] `ContainerHandle` kanonisch machen; Legacy-`ContainerId` nur noch als
      echte Deprecated-/Migrationsproperty führen.
- [x] Normalisierung und Root-Slot-Erkennung immer mit dem vollständigen
      `FRpgInventoryContainerHandle` statt nur mit lokaler `FName`-ID
      durchführen.
- [x] MaxEntries, Tiefe, Cycles, Duplicate IDs und Subtree-Grenzen in allen
      Import-/Transferpfaden einheitlich validieren.

Verifizierter Phase-4A-Zwischenstand vom 2026-07-22:

- Graph-Restore behandelt Container, Position und gewünschte Orientierung als
  persistierte Eingaben. Gespeicherte Breite und Höhe bleiben nur
  kompatibilitäts- beziehungsweise diagnosehalber im DTO und werden vor jeder
  Graphvalidierung aus der aktuellen Item-Definition rekonstruiert.
- Eine aktuell erlaubte Rotation bleibt erhalten; eine von der heutigen
  Item-Definition verbotene Rotation wird atomar abgelehnt. Echte Gear-/Carry-
  Root-Container normalisieren auf ihren semantischen `1x1`-/unrotierten
  Vertrag. Item-owned Container werden weiterhin über ihren vollständigen
  Handle ausgewertet.
- Der vollständig normalisierte Batch durchläuft weiterhin denselben
  Restore-Evaluator mit Bounds-, Regel- und Scratch-Occupancy-Prüfung. Ein nach
  Definition-Änderung größer gewordenes Item kann deshalb keinen Overlap
  einschleusen; der komplette Graph bleibt bei einem Konflikt unverändert.
- Das Graphschema bleibt bei Version 1, weil weder DTO-Shape noch persistierte
  Bedeutung von Position und Orientierung geändert wurden. `Width` und
  `Height` sind nun ausdrücklich abgeleitete Kompatibilitätsfelder.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (24/24 Actions).
- `SurvivalRpg.Inventory.IntentBoundary.RestoreReconstructsDefinitionPlacement`:
  1 von 1 Automationtests erfolgreich.
- `SurvivalRpg.Inventory`: 104 von 104 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 6 von 6 Automationtests erfolgreich.
- Fortschritt Phase 4: 1 von 6 Punkten abgeschlossen (16,7 %).
- Gesamtfortschritt der verbindlichen Checkliste: 63 von 94 Punkten
  abgeschlossen (67,0 %), 31 Punkte offen.
- Nächster Schnitt: ein kanonischer, nicht persistierter Stack-Key aus
  Item-Definition und deterministischem Runtime-State. Dabei muss der
  definitionsbasierte Base-Storage-Pfad runtime-verschiedene Materialien
  explizit erhalten oder fail-closed ablehnen.

Verifizierter Phase-4B-Zwischenstand vom 2026-07-22:

- `FRpgInventoryStackKey` bildet Definition und den vollständig exportierten,
  lexikalisch sortierten Fragment-Runtime-State bytegenau ab. Item-/Entry-ID,
  Anzahl, Placement und Outer bleiben bewusst außerhalb des nicht
  persistierten Schlüssels. Ein Exportfehler lehnt den Vergleich fail-closed
  ab; ein Cache wird ohne gemeinsamen Fragment-Invalidierungsvertrag nicht
  geführt.
- `IsStackCompatibleWith` vergleicht ausschließlich diesen kanonischen Key.
  Der parallele Fragment-Compatibility-Hook und vier unbenutzte,
  definitionsbasierte List-Helfer wurden entfernt. Nach einem Split prüft
  `CopyRuntimeStateFrom` zusätzlich, dass Quell- und Ziel-Key identisch sind.
- Der Fragment-Exportvertrag verlangt deterministische, nebenwirkungsfreie und
  vollständige stackrelevante Payloads mit exakt der aktuell deklarierten
  Fragmentversion. Die Tests belegen unter anderem kanonische StatTag-
  Reihenfolge, abweichende Fragment-Bytes, Definitionen, Split-Identität und
  fehlende Definitionen.
- Die definitionsbasierte Base bleibt ein bewusst zustandsloser
  Definition-/Count-Pool. Konkrete Materialinstanzen dürfen nur projiziert
  werden, wenn sie weder Container-Provider noch semantische StatTags oder
  fragment-eigenen Runtime-State besitzen. UI-Deposit und Crafting-
  Autodeposit validieren diese Grenze vor dem ersten Consume; der Caller
  bleibt für Consume und Rollback verantwortlich. Der definitionsbasierte
  Synthetic-/Refund-Credit ist klar benannt und nur noch nativ erreichbar.
  Varianten verbleiben bei Ablehnung unverändert als konkrete Inventory-
  Instanzen.
- Crafting-Autodeposit verarbeitet ausschließlich physische Output-Roots.
  Item-owned Descendants werden nie separat verschoben; ein übertragbarer
  Provider bewegt seinen vollständigen Subtree, während ein nicht
  kollabierbarer Material-Provider samt Child-Identitäten, Entry-IDs,
  Placements und Revision unverändert im Output verbleibt.
- `BP_Rpg_PlayerController` wurde rekompiliert und gespeichert. Der permanente
  UI-Test akzeptiert den fehlerfreien Status `UpToDateWithWarnings`, lehnt aber
  weiterhin Dirty- und Error-Zustände ab. Die vorhandenen Deprecation-Warnungen
  markieren bekannte Legacy-Aufrufe und werden nicht als Compilefehler
  kaschiert. Eine exakte Warnungs-Allowlist bleibt ein Editor-QA-Restpunkt,
  damit später neu hinzukommende Warnungen separat auffallen.
- Bekannter angrenzender Restpunkt: Crafting-Refund-Datensätze speichern heute
  nur Definition und Count. Runtime-spezifische Verbrauchsmaterialien könnten
  bei einem späteren Refund deshalb Zustand verlieren; dieser Refund-Vertrag
  ist mit der Stack-Key-/BaseStorage-Grenze noch nicht gelöst.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (4/4 Actions im finalen inkrementellen Build; der anschließende exakte Gate
  meldete `Target is up to date` und UBT `Result: Succeeded`).
- `SurvivalRpg.Inventory`: 105 von 105 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- Fortschritt Phase 4: 2 von 6 Punkten abgeschlossen (33,3 %).
- Gesamtfortschritt der verbindlichen Checkliste: 64 von 94 Punkten
  abgeschlossen (68,1 %), 30 Punkte offen.
- Nächster Schnitt: Legacy-Snapshots ausschließlich über einen explizit
  versionierten Konverter importieren und die parallelen Restore-Wege
  anschließend entfernen.

Verifizierter Phase-4C-Zwischenstand vom 2026-07-22:

- Die historischen DTOs liegen getrennt in `RpgInventoryLegacySnapshot` und
  sind nur noch Eingabe für einen nativen, nebenwirkungsfreien Konverter.
  `SingleSlotV0` liest den wieder explizit als deprecated geführten
  `SortIndex` und packt nach aktueller Definition, aktuellem Layout sowie
  gemeinsamem First-Fit-Scratch. `SpatialV1` übernimmt ausschließlich
  widerspruchsfreie Root-Platzierungen; item-owned oder partiell ungültige
  Handles werden geschlossen abgelehnt.
- Gültige persistente Item-IDs bleiben erhalten. Fehlt die erst später
  eingeführte Item-ID, wird eine gültige historische Entry-ID deterministisch
  genau einmal als Item-ID übernommen. Fehlende oder doppelte resultierende
  Identitäten verwerfen den vollständigen Konvertierungsoutput. Historische
  Entry-IDs werden nicht als neue FastArray-Entry-IDs restauriert.
- Da die alten Snapshot-Zeilen keinen Fragmentzustand besaßen, initialisiert
  der explizite Legacy-Pfad eine transiente Default-Instanz und exportiert
  daraus den heutigen versionierten Runtime-State. Der Konverter mutiert kein
  Live-Inventar und liefert immer nur ein DTO im aktuellen Graphschema;
  autoritativ kann es ausschließlich nach der vollständigen Prüfung durch
  `RestoreInventoryGraph` werden.
- Der unversionierte direkte `FRpgInventoryList::ImportSnapshot`-/
  `ExportSnapshot`-Pfad sowie die reflektierten
  `ImportInventorySnapshot`-/`ExportInventorySnapshot`-Funktionen sind
  entfernt. Auch der mehrdeutige Blueprint-Pfad `ImportInventoryGraph` ist
  entfernt. Interne atomare Rollbacks verwenden nun die native, klar
  begrenzte Funktion `RestoreRuntimeCheckpoint`, während Disk-/Profil-Restore
  weiterhin eine neue Command-Epoch über `RestoreInventoryGraph` etabliert.
- Ein alter Test, der über den direkten Snapshot-Bypass absichtlich einen
  unmöglichen Provider-Stack erzeugte, wurde durch die stärkere Grenzprüfung
  ersetzt: Der Legacy-Konverter darf das historische DTO syntaktisch lesen,
  aber der kanonische Restore lehnt den Provider-Stack atomar ab und lässt
  Graph, UObject-/Entry-Identität, Revision und Mutation-Epoch unverändert.
- Es wurde kein belastbarer historischer Disk-Byte-Fixture gefunden; die alten
  Snapshot-Felder waren bis auf die später ergänzte Item-ID nicht als
  `SaveGame` markiert. Der neue Pfad ist deshalb eine explizite
  DTO-/Asset-Migration und keine unbelegte Zusage, beliebige alte Binärsaves
  automatisch lesen zu können.
- Der Reflection-Test bestätigt, dass die drei entfernten Blueprint-Funktionen
  nicht mehr existieren. Zusätzlich kompilierte `CompileAllBlueprints` alle
  erreichbaren Blueprints mit 0 Fehlern und 0 nicht ladbaren Blueprints; die
  16 Compilerwarnungen gehören zu bereits dokumentierten Deprecated-/UI- und
  Lokalisierungsrestpunkten. In Content, Config und Plugins wurde zudem keine
  serialisierte Referenz auf die entfernten Funktionsnamen gefunden.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (24/24 Actions).
- Die drei fokussierten `SurvivalRpg.Inventory.LegacySnapshot`-Tests sind
  erfolgreich: V0-First-Fit, deterministische V1-Konvertierung samt
  Identity-Retention sowie Fail-closed-/Atomizitätsmatrix.
- `SurvivalRpg.Inventory`: 107 von 107 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- Fortschritt Phase 4: 3 von 6 Punkten abgeschlossen (50,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 65 von 94 Punkten
  abgeschlossen (69,1 %), 29 Punkte offen.
- Nächster Schnitt: `FRpgInventoryContainerHandle` zur einzigen kanonischen
  Containeradresse machen und `FRpgInventoryGridPlacement::ContainerId` nur
  noch als echte Deprecated-/Migrationsproperty führen.

Verifizierter Phase-4D-Zwischenstand vom 2026-07-22:

- `FRpgInventoryGridPlacement::ContainerHandle` ist die einzige kanonische
  Laufzeitadresse. `GetContainerHandle`, Placement-Vergleiche, Overlap,
  Restore, Sortierung, Debug-Ausgabe, UI-Helfer und sämtliche Add-, Transfer-
  und Move-Eingänge synthetisieren keinen Root-Handle mehr aus einer lokalen
  `FName`-ID; ein Placement ohne gültigen vollständigen Handle wird
  fail-closed und ohne Revision-/Epoch-/Ownership-Änderung abgelehnt.
- Das historische Feld wird in C++ nur noch als
  `ContainerId_DEPRECATED` geführt. UHT reflektiert es migrationskompatibel
  weiterhin unter `ContainerId` mit `SaveGame`- und `Deprecated`-Flag, aber
  weder editor- noch Blueprint-sichtbar. Ein Tagged-Serialization-Test lädt
  den historischen `ContainerId`-Property-Tag tatsächlich in dieses Feld,
  ohne daraus still einen kanonischen Handle zu erzeugen.
- Produktiv liest ausschließlich der explizite, versionierte
  Legacy-Snapshot-Konverter die Deprecated-Property. Er übernimmt nur eine
  widerspruchsfreie historische Root-Adresse; ein Konflikt mit einem bereits
  vorhandenen vollständigen Handle verwirft die komplette Konvertierung.
  Kanonische Runtime- und erneut exportierte Graphdaten löschen den
  Deprecated-Shadow.
- Source-Snapshot- und Placement-Vergleiche berücksichtigen ausschließlich
  den kanonischen Handle. Veraltete Deprecated-Daten können deshalb keinen
  falschen Stale-Konflikt erzeugen, während eine abweichende vollständige
  Adresse weiterhin korrekt abgelehnt wird.
- Ein ASCII-/UTF-16-Scan von Content und Plugins fand keine serialisierte
  Referenz auf `FRpgInventoryGridPlacement`, `ContainerHandle` oder den
  historischen Placement-Pfad. Die zwei Asset-Treffer auf `ContainerId`
  gehören zu weiterhin gültigen Layout-/Item-Container-Definitionen und
  erfordern keine Property-Weiterleitung oder Asset-Migration.
- Ein frischer `CompileAllBlueprints`-Lauf lud alle erreichbaren Blueprints
  und schloss mit 0 Fehlern sowie 0 nicht ladbaren Blueprints ab. Die 16
  Warnungen entsprechen den bereits dokumentierten Deprecated-, UI- und
  Lokalisierungsrestpunkten.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut;
  der exakte abschließende Gate meldete UBT `Result: Succeeded`.
- Die fokussierten Placement-, Intent-Boundary- und Legacy-Snapshot-Tests
  prüfen historische Tagged-Daten, alle entfernten Runtime-Fallbacks,
  atomaren Restore sowie widersprüchliche Shadow-Werte erfolgreich.
- `SurvivalRpg.Inventory`: 108 von 108 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- Fortschritt Phase 4: 4 von 6 Punkten abgeschlossen (66,7 %).
- Gesamtfortschritt der verbindlichen Checkliste: 66 von 94 Punkten
  abgeschlossen (70,2 %), 28 Punkte offen.
- Nächster Schnitt: Normalisierung und Root-Slot-Erkennung vollständig auf
  `FRpgInventoryContainerHandle` umstellen. Insbesondere der eigene
  `FRpgInventorySlotAddress::ContainerId`-Fallback und root-only `FName`-APIs
  bleiben bewusst diesem nächsten Checklistenpunkt zugeordnet.

Verifizierter Phase-4E-Zwischenstand vom 2026-07-22:

- `FRpgInventorySlotAddress::ContainerHandle` ist die einzige kanonische
  Laufzeitidentität einer Zelle. Gleichheit, Hashing, Lookup und Reverse-
  Lookup berücksichtigen Owner, Item und lokale Container-ID vollständig;
  gleich benannte item-owned Container verschiedener Provider können daher
  weder miteinander noch mit einem Root-Container kollidieren.
- Das historische SlotAddress-Feld ist nur noch als privater
  `ContainerId_DEPRECATED`-SaveGame-Shadow vorhanden. Es wird ausschließlich
  beim autoritativen Quick-Access-Restore zu einem eindeutigen Root-Handle
  migriert. Passende, von der Vorgängerversion doppelt geschriebene Root- und
  item-owned Handles behalten ihre vollständige Identität und löschen nur den
  Shadow. Widersprüchliche Werte setzen das Binding fail-closed zurück;
  normale Runtime-Zugriffe synthetisieren daraus keinen Handle.
- Normalisierung, Grid-Größe, First-Fit, Single-Cell-, Carry- und Gear-
  Erkennung arbeiten produktiv nur noch mit vollständigen Handles. Die
  öffentlichen root-only-`FName`-APIs, `FName`-Placement-Helfer und der
  mehrdeutige Panel-Binder wurden entfernt. Lokale Namen wie `Main` oder
  `Gear.Head` erben in item-owned Containern keine Root-Semantik.
- UI, MVVM, Drag-and-drop, Quick-Transfer und Interaction-Sessions reichen
  exakte Handles weiter. Fehlende Handles führen zu einer ungebundenen oder
  nicht ausführbaren Ansicht statt zu einem stillen Root-Fallback. Die UI
  bleibt Projektion; autoritative Mutationen validieren den eingebauten
  Root-Handle erneut.
- Der Carry-RPC leitet die Rolle nach der exakten Layoutvalidierung aus dem
  tatsächlich adressierten Carry-Root ab. Eine vom Client mitgeschickte
  abweichende Rolle kann damit keine Mutation mehr gegen einen anderen
  Carry-Vertrag auslösen; designerdefinierte Carry-Roots bleiben unterstützt.
- Automation deckt kanonische Adressgleichheit und Hashing, SaveGame-
  Reflection und Memory-Roundtrip, Legacy-Quick-Access-Migration,
  providerübergreifende `Main`-Kollisionen sowie Root- gegen item-owned
  `Gear.Head` ab. Zusätzlich bleibt ein designerdefinierter Carry-Root über
  den vollständigen Handle bis in den autoritativen ActionBar-Pfad bindbar.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (31/31 Actions, UBT `Result: Succeeded`).
- `SurvivalRpg.Inventory`: 110 von 110 Automationtests erfolgreich.
- `SurvivalRpg.Save.WorldSave.MemoryRoundTrip`: 1 von 1 Automationtests
  erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 4: 5 von 6 Punkten abgeschlossen (83,3 %).
- Gesamtfortschritt der verbindlichen Checkliste: 67 von 94 Punkten
  abgeschlossen (71,3 %), 27 Punkte offen.
- Nächster Schnitt: `MaxEntries`, maximale Tiefe, Cycles, Duplicate IDs und
  Subtree-Grenzen in allen Import- und Transferpfaden über gemeinsame
  Graphinvarianten vereinheitlichen.

Verifizierter Phase-4F-Abschlussstand vom 2026-07-22:

- `ValidateInventoryGraph` ist der gemeinsame fail-closed Vertrag für
  Restore, Cross-Inventory-Transfer, Collect und Pickup. Er erzeugt kanonische
  Item-, Entry-, Parent- und Root-Indizes sowie die relative Subtree-Tiefe und
  prüft daraus denselben vollständigen Graphen vor jedem Commit.
- Die Prüfung unterscheidet stabile Ursachen für Kapazität, maximale Tiefe,
  Cycles, doppelte Item- und Entry-IDs, ungültige Owner/Outer, fehlende
  Provider oder lokale Container, falsche Parent-Child-Tiefe, Stack-Limits,
  Definition/Placement-Regeln, kanonische Footprints, Bounds und Occupancy.
  `DuplicateEntryId` wurde ordinalschonend am Ende des Result-Code-Vertrags
  ergänzt. Aktuelle Schemadaten ohne gültigen kanonischen Container-Handle
  liefern einheitlich `InvalidContainer` statt des früheren Sammelcodes.
- Kapazität ist eine Admission-Regel für Restore-, Pickup-, Collect- und
  Zielgraphen. Ein bereits übervoller Quellgraph darf weiterhin durch einen
  atomaren Egress verkleinert werden; kein Import oder Zielcommit kann
  `MaxEntries` überschreiten.
- Subtrees werden vor dem Commit gegen die exakte Zielrebasierung geprüft.
  Der vollständige Descendant-Pfad muss nach dem Move innerhalb der maximalen
  Tiefe bleiben; Parent-Handles, lokale Container und relative Tiefe dürfen
  weder verwaisen noch einen Cycle erzeugen.
- Restore und die Batch-/Cross-Inventory-Pfade besitzen gemeinsame
  Reentrancy-Sperren, halten gestagte beziehungsweise entfernte Item-UObjects
  während synchroner Callbacks stark referenziert und validieren den
  projizierten Source-/Target-Endzustand vor der ersten sichtbaren Mutation.
  Ablehnungen bewahren Graph, UObject- und Entry-Identität, Runtime-State,
  Revision, Mutation-Epoch und Nachrichtenanzahl vollständig.
- Der relevante UE-5.8-Build kompilierte alle geänderten Inventory-Einheiten
  und linkte beide Module erfolgreich (23/23 Actions); der abschließende
  inkrementelle Gate war ebenfalls grün (6/6 Actions, UBT
  `Result: Succeeded`).
- `SurvivalRpg.Inventory`: 112 von 112 Automationtests erfolgreich.
- `SurvivalRpg.Save.WorldSave.MemoryRoundTrip`: 1 von 1 Automationtests
  erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 4: 6 von 6 Punkten abgeschlossen (100,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 68 von 94 Punkten
  abgeschlossen (72,3 %), 26 Punkte offen.
- Nächster Schnitt: Phase 5 beginnt mit der datengetriebenen Zuweisung von
  `URpgPlayerInventoryLayoutDefinition` über PawnData/Experience.

## Phase 5 – Datengetriebenes Layout und Editor-Validierung

Status: **Abgeschlossen**

- [x] `URpgPlayerInventoryLayoutDefinition` als DataAsset über
      PawnData/Experience zuweisen.
- [x] Semantische Rollen statt Logik aus hartcodierten `FName`-IDs verwenden.
- [x] Gear-/Carry-Gruppen erhalten eine explizite, typisierte
      `ERpgEquipmentSlot`-Rolle statt indirekter Namens- oder
      Kategorieableitung.
- [x] Explizites Spatial-Fragment für jedes gridfähige Item verlangen.
- [x] `IsDataValid` für ItemDefinitions, Container-IDs, Footprints,
      Equipment-Slots, LayoutDefinition und ScreenRegistry ergänzen.
- [x] `BothHands + OffHand`, leere `AllowedSlots` bei ausrüstbaren Items und
      definitionlose Provider als konkrete Validierungsfehler bzw.
      Migrationswarnungen abbilden.
- [x] Widget-Compiler-/Editor-Validierung für erforderliche BindWidgets,
      Layer und Input-Actions ergänzen.

Verifizierter Phase-5A-Zwischenstand vom 2026-07-22:

- `URpgPlayerInventoryLayoutDefinition` ist die immutable,
  designer-authored Quelle für statische Gear-, Carry- und Content-Roots.
  `DA_PlayerInventoryLayout_Default` enthält 13 Gruppen und 48 Zellen;
  `Pockets` behält die im PlayerController-Blueprint konfigurierte Größe
  6 x 6. Dynamische, von Items bereitgestellte Container bleiben
  Runtime-Projektionen.
- `DA_PawnData` referenziert das Layout hart; `RpgPrototypeExperience`
  referenziert weiterhin `DA_PawnData`. Der Asset-Composition-Test prüft die
  konkrete Experience-/PawnData-/Layout-Abhängigkeitskette sowie Reihenfolge,
  Gruppenarten, Kategorien, Regeln und Carry-Rollen.
- `URpgPlayerInventoryLayoutComponent` besitzt kein reflektiertes
  `StaticSlotGroups` mehr und löst die statische Definition ausschließlich
  über das replizierte PlayerState-PawnData auf. `BP_Rpg_PlayerController`
  wurde neu gespeichert; sein serialisierter Legacy-Override ist entfernt.
- Ein gemeinsames PawnData-Changed-Signal für Authority und `OnRep_PawnData`
  sowie der sofortige Controller-Rebind schließen beide
  Initialisierungsreihenfolgen. Fehlende Definitionen liefern fail-closed
  keine statischen Gruppen; ein fehlendes Player-Layout nach Experience-Load
  blockiert außerdem persistente Schreibvorgänge.
- Profil-Restore wartet auf PlayerState, Inventory, PawnData und exakt dessen
  Layoutdefinition. Der idempotente Retry läuft nach der PawnData-Zuweisung
  und vor dem ersten Pawn-Restart; der Per-Controller-Test verwendet dafür
  genau einen echten `PostLogin` pro Verbindung und keinen künstlichen
  Framework-Reentry.
- Automation-Fixtures besitzen eigene transiente PawnData- und
  Layoutdefinitionen. Sie initialisieren diese auch in Standalone-Testwelten
  sicher über `PostActorCreated`; ihr bewusst kompaktes 4-x-2-Pockets-Layout
  bleibt vom produktiven 6-x-6-Assetvertrag getrennt.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (`Result: Succeeded`).
- `SurvivalRpg.Inventory`: 113 von 113 Automationtests erfolgreich.
- `SurvivalRpg.Save.WorldSave.MemoryRoundTrip`: 1 von 1 Automationtests
  erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0; seine Compile-Zusammenfassung
  meldete 0 Fehler, 16 Warnungen und 0 nicht ladbare Blueprints. Die
  abschließende Prozesszusammenfassung zählte 49 bekannte Warnemissionen:
  44 `LogBlueprint`-Compilerzeilen (42 Deprecated- und 2 UI-Tick-Warnungen),
  4 fehlende StringTable-Einträge und 1
  GameplayCue-Pfadkonfigurationswarnung.
- Fortschritt Phase 5: 1 von 7 Punkten abgeschlossen (14,3 %).
- Gesamtfortschritt der verbindlichen Checkliste: 69 von 94 Punkten
  abgeschlossen (73,4 %), 25 Punkte offen.
- Nächster Schnitt: semantische Rollen statt Logik aus hartcodierten
  `FName`-Container-IDs verwenden.

Verifizierter Phase-5B-Zwischenstand vom 2026-07-23:

- Statische Slot-Gruppen besitzen mit `SemanticRole` eine exakte
  `FGameplayTag`-Rolle. Die Rollen `Content.Primary`, `Carry.Primary`,
  `Carry.Secondary`, `Carry.OffHand` und `Carry.Utility` beschreiben ihre
  Gameplay-Bedeutung unabhängig von der physischen, weiterhin gespeicherten
  `ContainerId`.
- Semantische Rollen sind für statische Gruppen layoutweit eindeutig.
  Doppelte Rollen, ungültige Rollenanfragen und Carry-Rollen auf nicht
  exakt 1 x 1 großen Gruppen werden fail-closed abgelehnt. Item-owned
  Container erhalten bewusst keine globale semantische Rolle.
- ActionBar-Bindings speichern und replizieren seit Schema 2 die semantische
  Carry-Rolle. Der explizite Tagged-Schema-1-Import akzeptiert nur einen
  eindeutig auflösbaren historischen Root und besitzt für `WeaponSlot1`,
  `WeaponSlot2` und `ShieldSlot` zusätzlich feste Aliase über physische
  Umbenennungen hinweg. Aktuelle Schemadaten ohne gültige Rolle werden
  zurückgesetzt statt aus Namen rekonstruiert.
- Die serverautoritative ActionBar-Aktivierung validiert Slotindex und
  erwartete Rolle gegen das aktuelle Binding und löst erst danach die
  physische Inventory-Adresse aus dem aktuellen Layout auf. Drag/drop und
  Interaction-Confirmation führen dieselbe erwartete Rolle bis zur
  autoritativen Revalidierung mit.
- Equipment-provided Gruppen tragen ihre Herkunft nun typisiert als
  `ERpgEquipmentSlot`. Player-, BaseTerminal-, MVVM- und ActionBar-
  Projektionen lösen primären Content und Carry-Slots über Rollen auf;
  physische IDs bleiben nur Adresse, Save-Identität, Debugbezeichnung oder
  ausdrücklich versionierter Legacy-Input.
- `DA_PlayerInventoryLayout_Default` sowie die Automation-Fixtures besitzen
  die exakten Rollen. Asset-, Eindeutigkeits-, Migrations-, Authority-,
  Interaction-, UI- und Pooling-Regressionen sichern den Vertrag ab.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut;
  der abschließende inkrementelle Gate meldete UBT `Result: Succeeded`.
- `SurvivalRpg.Inventory`: 113 von 113 Automationtests erfolgreich.
- `SurvivalRpg.Save`: 2 von 2 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 5: 2 von 7 Punkten abgeschlossen (28,6 %).
- Gesamtfortschritt der verbindlichen Checkliste: 70 von 94 Punkten
  abgeschlossen (74,5 %), 24 Punkte offen.
- Nächster Schnitt: Gear-/Carry-Definitionen und ihre Views erhalten eine
  explizite `ERpgEquipmentSlot`-Rolle; die verbleibende Ableitung aus
  Gear-Container-Namen wird anschließend entfernt.

Verifizierter Phase-5C-Zwischenstand vom 2026-07-23:

- Statische Layoutdefinitionen und ihre Runtime-Views besitzen mit
  `EquipmentSlotRole` eine explizite `ERpgEquipmentSlot`-Rolle. Gear verwendet
  genau eine nicht-handbezogene Rolle, Carry verwendet `MainHand` oder
  `OffHand`, und Content sowie item-owned Gruppen verwenden `None`.
- Die redundanten Felder `bCarrySlot` und `CarryActivationRole` sowie der
  GameplayTag-zu-EquipmentSlot-Konverter sind entfernt. Gear-Adressen werden
  nicht mehr aus `Gear.*`-Namen konstruiert oder zurückübersetzt, sondern über
  die aktive PawnData-Layoutdefinition aufgelöst.
- Gear- und Carry-Gruppen müssen exakt 1 x 1 groß sein. Doppelte Gear-Rollen,
  fehlende beziehungsweise zur Gruppenart unpassende Rollen und mehrzellige
  Equipment-Gruppen werden zur Laufzeit fail-closed abgelehnt. Mehrere
  Carry-Gruppen mit `MainHand` bleiben dagegen ausdrücklich erlaubt, damit
  Primär- und Sekundärwaffe dieselbe Aktivierungsrolle besitzen können.
- Doppelte statische `ContainerId`-Werte verwerfen alle kollidierenden
  Definitionen statt reihenfolgeabhängig die erste zu behalten. Der rohe
  statische Equipment-Vertrag wird vor Death-Drop vollständig geprüft; ein
  fehlendes Layout sowie mehrdeutige oder fehlerhafte Gear-Klassifikation
  brechen die destruktive Operation ohne Teilmutation ab.
- `ContainerId` bleibt unverändert die physische Inventory-/Save-Identität;
  `SemanticRole` bleibt die eindeutige UI-/ActionBar-Identität.
  `SourceEquipmentSlot` bleibt davon getrennt die Herkunft item-owned
  Container. Dadurch kann `Gear.Backpack` nicht mit dem Inhalt eines
  ausgerüsteten Backpacks verwechselt werden.
- Equipment-Loadout, serverautoritative Equipment-Intents, ActionBar,
  Drag/drop, Death-Drop-Filter, MVVM und Carry-Widgets konsumieren den
  typisierten Vertrag. Export und Restore aktiver Hände validieren ihre
  physische Carry-Rolle ebenfalls gegen das aktuelle Layout.
- Physische Equipment-Reconciliation löst alle benötigten Gear-Adressen vor
  der ersten Mirror-/Runtime-Mutation auf. Ein fehlender typisierter Slot
  liefert `false`, ohne einen bestehenden Mirror oder GAS-Runtimezustand
  teilweise zu leeren. Equipment-Dragquellen müssen außerdem exakt zur
  angefragten Handrolle passen.
- Carry-Gruppen werden nur als ActionBar-bindbar angekündigt, wenn ihre
  `SemanticRole` gültig und layoutweit eindeutig auf genau diesen statischen
  Root zeigt. Vorschau, detaillierte Bindungsprüfung und Autorität verwenden
  damit denselben fail-closed Vertrag.
- `DA_PlayerInventoryLayout_Default` wurde mit den neun Gear-Rollen,
  `MainHand` für `WeaponSlot1` und `WeaponSlot2`, `OffHand` für `ShieldSlot`
  sowie `None` für `Pockets` neu gespeichert. Das Asset enthält weder
  `bCarrySlot` noch `CarryActivationRole`.
- Regressionen sichern umbenannte Head-Container, item-owned
  `Gear.Head`-Namenskollisionen, doppelte Gear-Rollen und Container-IDs,
  Death-Drop-Preflight bei fehlerhaftem Layout, fehlende Gear-Adressen ohne
  Mirror-Verlust, Cross-Role-Dragquellen, eindeutige Carry-ActionBar-Rollen,
  zusätzliche MainHand-Carry-Gruppen, Provider-Provenienz, Asset-Reflection
  und die
  typisierte MVVM-Projektion ab.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut;
  der abschließende inkrementelle Gate meldete UBT `Result: Succeeded`.
- `SurvivalRpg.Inventory`: 113 von 113 Automationtests erfolgreich.
- `SurvivalRpg.Save`: 2 von 2 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- `CompileAllBlueprints` beendete das Commandlet mit Ergebnis 0,
  0 Compilerfehlern, 16 bekannten Compilerwarnungen und 0 nicht ladbaren
  Blueprints.
- Fortschritt Phase 5: 3 von 7 Punkten abgeschlossen (42,9 %).
- Gesamtfortschritt der verbindlichen Checkliste: 71 von 94 Punkten
  abgeschlossen (75,5 %), 23 Punkte offen.
- Nächster Schnitt: gridfähige Items benötigen ein explizites
  Spatial-Fragment; implizite Footprint-Fallbacks werden anschließend
  inventarisiert, migriert und fail-closed entfernt.

Verifizierter Phase-5D-Zwischenstand vom 2026-07-23:

- `URpgInventoryItemDefinition` besitzt einen gemeinsamen Spatial-Vertrag:
  Genau ein `URpgInventoryFragment_SpatialItem` mit positivem Footprint ist
  gültig. Fehlende, doppelte oder ungültige Fragmente liefern keinen
  synthetischen 1-x-1-Footprint.
- Placement-Evaluator, First-Fit, Sort, Graph-Restore, Legacy-Import,
  Quick-Transfer und Drag/drop konsumieren denselben Vertrag. Gear und Carry
  normalisieren erst nach erfolgreicher Definitionvalidierung auf ihre
  ausdrückliche 1-x-1-Containersemantik.
- Leere Drag-Payloads starten mit einem ungültigen 0-x-0-Footprint.
  Interaktionen akzeptieren ausschließlich die zur ItemDefinition passende
  kanonische Größe; ein vom Client erfundener 1-x-1-Wert kann ein fehlendes
  Spatial-Fragment nicht ersetzen. Reine Presenter-Geometrie arbeitet dagegen
  weiterhin deterministisch mit bereits explizit übergebenen Größen und bleibt
  von der Gameplay-Validierung getrennt.
- Alle zwölf aktuell gefundenen Blueprint-ItemDefinitions besitzen exakt ein
  gültiges Spatial-Fragment. Acht Legacy-Assets wurden verhaltensneutral auf
  explizit 1 x 1 und nicht rotierbar migriert; die vier bereits räumlich
  authorierten Definitionen behielten ihre Maße und Rotationsregeln.
- Ein permanenter AssetRegistry-Test prüft konkrete ItemDefinitions in
  `/Game` sowie in allen gemounteten Projekt-Plugins. Runtime-Regressionen
  sichern Exact, FirstFit, Content-/Gear-/Carry-Restore, atomaren Rollback und
  Drag-Payload-Spoofing bei fehlendem Spatial-Vertrag ab.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (26/26 Actions, UBT `Result: Succeeded`).
- `SurvivalRpg.Inventory`: 115 von 115 Automationtests erfolgreich.
- `SurvivalRpg.Save`: 2 von 2 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 5 von 5 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 20 von 20 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 5: 4 von 7 Punkten abgeschlossen (57,1 %).
- Gesamtfortschritt der verbindlichen Checkliste: 72 von 94 Punkten
  abgeschlossen (76,6 %), 22 Punkte offen.
- Nächster Schnitt: gemeinsames `IsDataValid` für ItemDefinitions,
  Container-IDs, Footprints, Equipment-Slots, LayoutDefinition und
  ScreenRegistry ergänzen; dabei die bereits fail-closed behandelten
  Duplicate- und ungültigen Spatial-Fragmente als konkrete Editordiagnosen
  und Runtime-Regressionen festschreiben.

Verifizierter Phase-5E-Zwischenstand vom 2026-07-23:

- `URpgInventoryItemDefinition::IsDataValid` diagnostiziert fehlende,
  doppelte und nicht-positive Spatial-Fragmente mit Assetpfad, Fragmentindex
  und konkreten Maßen. Der Editorvertrag endet weiterhin im selben
  fail-closed `FindValidSpatialItemFragment`, den die Runtime verwendet.
- Das erste runtime-wirksame ItemContainer-Fragment validiert über
  `GetAuthoredContainerDefinitions` seine ungefilterte Authoring-Sicht.
  Fehlende oder fragmentlokal doppelte `ContainerId`s und nicht-positive
  Grid-Größen werden für native Container und sämtliche konvertierten
  Legacy-Compatibility-Zeilen mit Fragment- und Containerindex gemeldet.
  `GetProvidedContainers` behält seine bisherige gefilterte Runtime-Sicht;
  spätere Fragmentduplikate behalten bewusst die bestehende
  First-Match-Semantik.
- Ein effektives Equippable-Fragment ohne `EquipmentDefinition` ist ungültig.
  EquipmentDefinitions lehnen `None`, unbekannte oder doppelte
  `AllowedSlots` sowie unvollständige oder unerreichbare
  `SlotAbilitySetsToGrant` ab. Leere `AllowedSlots` und
  `BothHands + OffHand` bleiben ausdrücklich dem separaten Phase-5F-
  Migrationsschnitt vorbehalten.
- `URpgPlayerInventoryLayoutDefinition` besitzt einen gemeinsamen
  Editor-/Runtime-Regelpass für eindeutige Root-`ContainerId`s, positive
  Größen, gültige GroupKinds, die Content-/Carry-/Gear-Zuordnung typisierter
  Equipment-Rollen, 1-x-1-Equipmentgruppen, eindeutige Gear-Rollen sowie
  konkrete und eindeutige `Rpg.Inventory.Layout.Role.*`-Tags.
  Actionbar-bindbares Carry ohne SemanticRole wird ebenfalls konkret
  diagnostiziert.
- Runtime-View-Filter und der physische Equipment-Preflight konsumieren
  denselben Layout-Regelpass. Reine SemanticRole-/Actionbar-Fehler bleiben
  Editorfehler und blockieren weder Death-Drop noch Equipment-Reconciliation;
  ihre exakten Runtime-Resolver scheitern bei Verwendung weiterhin
  fail-closed.
- `URpgUIScreenRegistry::IsDataValid` verlangt eindeutige konkrete
  `UI.Screen.*`-Tags, konkrete `UI.Layer.*`-Tags und gesetzte, ladbare,
  nicht-abstrakte `UCommonActivatableWidget`-Klassen.
  `bSingleInstance=false` erzeugt eine Migrationswarnung. `FindScreen` behält
  seinen exakten First-Match-Vertrag; der Legacy-Config-Fallback wird nicht
  zu einer zweiten validierten Composition Authority ausgebaut.
- Permanente AssetRegistry-Gates validieren sämtliche konkreten
  Item-/Equipment-Blueprint-CDOs sowie alle LayoutDefinition- und
  ScreenRegistry-Assets unter `/Game` und in gemounteten Projekt-Plugins.
  Die aktuell authorierten Assets erfüllen die neuen Verträge ohne
  Validierungsfehler.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (finaler inkrementeller Build 4/4 Actions, UBT `Result: Succeeded`).
- `SurvivalRpg.Inventory`: 122 von 122 Automationtests erfolgreich.
- `SurvivalRpg.Save`: 2 von 2 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 6 von 6 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 22 von 22 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 5: 5 von 7 Punkten abgeschlossen (71,4 %).
- Gesamtfortschritt der verbindlichen Checkliste: 73 von 94 Punkten
  abgeschlossen (77,7 %), 21 Punkte offen.
- Nächster Schnitt: `BothHands + OffHand`, leere `AllowedSlots` bei
  ausrüstbaren Items und definitionlose Provider als konkrete
  Validierungsfehler beziehungsweise Migrationswarnungen abbilden.

Verifizierter Phase-5F-Zwischenstand vom 2026-07-23:

- `URpgEquipmentDefinition` trennt strukturelle Slotreferenzen von einem
  expliziten Handbelegungsvertrag. `BothHands` zusammen mit einem
  `OffHand`-Eintrag sowie unbekannte `HandOccupancy`-Werte sind konkrete,
  pfad- und wertbezogene Data-Validation-Fehler. Solche Definitionen können
  zur Runtime nicht neu ausgerüstet werden und liefern keinen Default-Slot;
  unbekannte Handregeln beanspruchen außerdem keinen Runtime-Slot. Eine
  bereits vorhandene bekannte `BothHands`-Belegung bleibt für defensives
  Legacy-Reconciliation konservativ als Belegung beider Hände sichtbar.
- Leere `AllowedSlots` bleiben auf einer eigenständigen
  EquipmentDefinition ein gültiger, bewusst deaktivierter Zustand. Erst ein
  tatsächlich ausrüstbares Item, dessen effektives `EquippableItem`-Fragment
  auf diese Definition zeigt, ist ungültig. Die Diagnose nennt Itempfad,
  Fragmentindex und referenzierte Equipment-Klasse.
- Gear-Provider benötigen jetzt die explizite Kette
  `ItemContainer -> EquippableItem -> EquipmentDefinition -> AllowedSlot`.
  Der gemeinsame Placement-Vertrag, serverseitiger Planner/Commit und die
  generischen beziehungsweise slotbezogenen UI-Equip-Pfade besitzen keinen
  definitionlosen Backpack-/Belt-/Pouch-/ResourceBag-Fallback mehr.
  Kontextmenü und Doppelklick klassifizieren reine ItemContainer ebenfalls
  nicht mehr als ausrüstbar; ihre Container-Präsentation bleibt verfügbar.
  `UnequipToContent` bleibt für vor der Migration vorhandenen Altbestand
  erreichbar.
- Ein definitionloses `ItemContainer` bleibt als portabler oder
  verschachtelbarer Container vollständig gültig und erhält genau eine
  Migrationswarnung, dass es nicht mehr als Gear-Provider verwendet werden
  kann. Ein explizit konfigurierter Provider erzeugt keine Warnung.
- Der read-only Asset-Audit prüfte zwölf konkrete ItemDefinition- und sieben
  konkrete EquipmentDefinition-Blueprints. `ID_TestBackpack` ist der einzige
  aktuell authorisierte Provider und verweist vollständig auf
  `EQ_TestBackpack` mit `AllowedSlots = Backpack`. Es existieren keine
  definitionlosen oder Legacy-Provider-Assets, keine ausrüstbaren Items mit
  leeren Slots und keine `BothHands + OffHand`-Definition; daher war keine
  Asset-Migration erforderlich. Die bekannte, derzeit unreferenzierte
  `EQ_TestShield`-Semantik bleibt bewusst Phase 7 zugeordnet.
- Permanente Regressionen prüfen den negativen definitionlosen Provider und
  den positiven Backpack-Vertrag durch gemeinsame Policy, Layout,
  serverseitigen Planner/Commit, explizite Slot-Action und generisches
  Default-Equip. Editor-Tests sichern Fehlertexte, Warnungsanzahl,
  Cross-Asset-Referenzen und den vollständigen authorisierten Assetbestand.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (voller Lauf 32/32 Actions; finaler inkrementeller Gate 23/23 Actions;
  jeweils UBT `Result: Succeeded`).
- `SurvivalRpg.Inventory`: 122 von 122 Automationtests erfolgreich.
- `SurvivalRpg.Save`: 2 von 2 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 6 von 6 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 22 von 22 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 5: 6 von 7 Punkten abgeschlossen (85,7 %).
- Gesamtfortschritt der verbindlichen Checkliste: 74 von 94 Punkten
  abgeschlossen (78,7 %), 20 Punkte offen.
- Nächster Schnitt: Widget-Compiler-/Editor-Validierung für erforderliche
  BindWidgets, Layer und Input-Actions ergänzen.

Verifizierter Phase-5G-Abschlussstand vom 2026-07-23:

- Die kanonischen Player-, Storage-, Base-Terminal- und Crafting-Screens
  deklarieren ihre strukturell erforderlichen WidgetTree-Knoten jetzt als
  echte `BindWidget`-Verträge. Carry-Slots besitzen zusätzlich den exakten
  nativen Typ `URpgInventoryCarrySlotWidget`; optionale Pointer-Aktionen am
  Base-Terminal bleiben bewusst optional.
- Die gemeinsamen Inventory- sowie die spezialisierten Base-Terminal- und
  Crafting-Input-Actions sind vollständig in den Widget-Blueprint-Defaults
  authorisiert. Der Widget-Compiler lehnt fehlende Tabellen, RowNames,
  falsche RowStructs und nicht vorhandene Rows ab. Der bisherige
  hardcodierte Runtime-Load-/Fallback-Pfad ist entfernt; Back bleibt
  ausschließlich vollständig leer als CommonUI-delegierte Option zulässig.
- Die vier Inventory-Präsentationsklassen für Drag-Visual, Kontextmenü,
  Split-Dialog und Drop-Bestätigung sind explizite, konkrete
  Widget-Blueprint-Verträge. Automation prüft außerdem die sechs gemeinsamen
  Action-Rows, die spezialisierten Rows und die konkreten authored Defaults
  aller vier Screens.
- `URpgPrimaryGameLayout`, `URpgUIScreenRegistry` und
  `URpgGameFeatureAction_AddWidgets` verwenden einen gemeinsamen Vertrag für
  exakt `UI.Layer.Game`, `UI.Layer.GameMenu`, `UI.Layer.Menu` und
  `UI.Layer.Modal`. Unbekannte Layer, fehlende oder nicht auflösbare Klassen
  sowie geladene falsche beziehungsweise abstrakte Klassen werden
  diagnostiziert. Ungeladene Blueprint-Klassen werden über ihren exakten
  `GeneratedClassPath` im AssetRegistry geprüft; transiente `SKEL_`-,
  `REINST_`- und veraltete Klassen werden nicht als stabile Klassen akzeptiert.
  Subobject-Suffixe und nicht exakt zum authored Pfad passende geladene
  Klassen werden ebenfalls abgelehnt.
  Soft-Class-Validierung lädt während eines Blueprint-Compiles keine
  abhängigen Widget-Blueprints rekursiv.
- `URpgInputConfig::IsDataValid` lehnt Null-Actions, ungültige oder falsch
  namespacete Input-Tags und doppelte Tags innerhalb sowie zwischen Native-
  und Ability-Mappings ab. Eine InputAction darf weiterhin bewusst mehreren
  unterschiedlichen semantischen Tags zugeordnet werden. Ein AssetRegistry-
  Test validiert alle gemounteten Projekt-InputConfigs.
- Die sieben betroffenen Widget-Blueprints wurden nach der Action-Row- und
  BindWidget-Migration neu kompiliert und gespeichert. Der gezielte
  `RpgPrototypeExperience`-Compile endete `BS_UP_TO_DATE`.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut
  (vollständiger relevanter Build 26/26 Actions; finaler inkrementeller
  Exaktstand 7/7 Actions, jeweils UBT `Result: Succeeded`).
- `SurvivalRpg.Inventory`: 122 von 122 Automationtests erfolgreich.
- `SurvivalRpg.Save`: 2 von 2 Automationtests erfolgreich.
- `SurvivalRpg.Equipment`: 6 von 6 Automationtests erfolgreich.
- `SurvivalRpg.Crafting`: 8 von 8 Automationtests erfolgreich.
- `SurvivalRpg.UI`: 25 von 25 Automationtests erfolgreich.
- `SurvivalRpg.Input`: 2 von 2 Automationtests erfolgreich.
- `SurvivalRpg.GameFeatures`: 3 von 3 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 5: 7 von 7 Punkten abgeschlossen (100,0 %).
- Gesamtfortschritt der verbindlichen Checkliste: 75 von 94 Punkten
  abgeschlossen (79,8 %), 19 Punkte offen.
- Nächster Schnitt: Phase 6 – stabile Child-VMs pro Item-ID und
  Container-Handle verwenden.

## Phase 6 – MVVM, Refresh und Komponentenschnitt

Status: **In Arbeit**

- [x] Der Player-Screen besitzt genau eine native screen-scoped VM, stellt sie
      über eine exakte nicht optionale Getter-/PropertyPath-Source read-only
      bereit und behält VM, Source und Delegate-Bindungen stabil über
      CommonUI-Pooling.
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
- [x] Stabile Child-VMs pro Item-ID und Container-Handle verwenden.
- [x] Nur tatsächlich geänderte FieldNotify-Felder senden.
- [x] Inventory-Invalidierungen pro Commit/Tick bündeln.
- [x] Player- und Storage-Projektionen bei Deaktivierung unbinden, ohne ihre
      screen-owned VM-Instanzen beim Pooling zu ersetzen.
- [x] BaseTerminal auf denselben stabilen Unbind-/Pooling-Vertrag bringen.
- [x] Crafting auf denselben stabilen Unbind-/Pooling-Vertrag bringen.
- [x] Weitere gepoolte Screens auf denselben Unbind-/Pooling-Vertrag bringen.
- [x] Bei den deklarativen MVVM-Leaves ActionBar, Address, Gear und
      BaseResource Blueprint-MVVM als einzigen Datenbindungsweg verwenden;
      BP-Events nur für Animation und imperative Präsentation. Der
      Carry-Spezialfall und die Spatial-Item-Legacy-Graphwriter bleiben den
      ausdrücklich getrennten Folgepunkten zugeordnet.
- [x] Beim stabilen Root `CUI_PlayerInventory` den öffentlich generierten
      Manual-Source-Setter durch einen unverletzbaren nativen
      Getter-/PropertyPath-Vertrag ersetzen.
- [x] BlueprintCallable Lifecycle-Mutatoren der Aggregate-VM nach
      Asset-Referenzprüfung auf eine native Presenter-Oberfläche reduzieren.
- [x] Player-/Storage-Lifecycle mit echtem PlayerController, PlayerState,
      kanonischem Inventory und Listener-Cleanup als Integrationstest
      abdecken.
- [x] Gear-, Carry- und Content-Widgets auf denselben Coordinator-/MVVM-Pfad
      bringen; Kontextmenüs fragen Fähigkeiten statt Fragmente zu erraten.
- [x] `CUI_CarrySlot` auf genau eine VM-Beobachtung reduzieren und eine
      ausdrückliche Presenter-Policy festlegen: deklarative Itemdaten über die
      exakte Address-Source, imperative Hooks nur für Active/Holstered,
      Interaktion und Animation.
- [x] `RpgInventoryUiActionComponent` in schmale Domain-Handler aufteilen;
      Controller bleibt RPC-Eigentümer und kann vorübergehend als Fassade
      bestehen.
- [x] Manager intern in Storage, Rules/Planner, Transactions und Persistence
      schneiden, ohne die öffentliche Autorität zu duplizieren.
- [ ] Große UI-Sammeldateien in eine Klasse pro Datei aufteilen.

Verifizierter Phase-6A-Zwischenstand vom 2026-07-23:

- `URpgInventoryEntryViewModel` projiziert die persistente
  `FRpgInventoryItemId` nun ausdrücklich als read-only FieldNotify-Feld.
  `URpgInventoryPanelViewModel` reconciled seine Child-VMs über diese
  Item-ID statt über die inventarlokale `EntryId`. Eine rekonstruierte
  Inventory-Zeile oder Item-Instanz übernimmt dadurch weiterhin dieselbe
  VM; die neue `EntryId`, Instanz, Platzierung und Stackdaten werden in
  dieser stabilen VM aktualisiert.
- Der Item-Child-Lifecycle endet weiterhin bewusst, sobald das Item die
  gebundene Projektion verlässt oder der Screen unbindet. Es gibt keinen
  unbegrenzten Cache veralteter Gameplay-Referenzen. Ungültige Item-IDs
  werden nicht gecacht; ein gecachter Pointer kann pro Refresh höchstens
  einmal wiederverwendet werden.
- Slot-Gruppen bleiben über den vollständigen
  `FRpgInventoryContainerHandle` stabil. Zwei gleichzeitig ausgerüstete
  Provider mit demselben definition-lokalen Container-Namen `Main` bleiben
  wegen ihrer verschiedenen Owner-Item-IDs getrennte Child-VMs.
  Address-Slots bleiben davon unabhängig über ihre vollständige
  `(ContainerHandle, X, Y)`-Adresse stabil; Gear- und Actionbar-VMs behalten
  ihre feste Slotsemantik.
- Gear-Gruppen werden vor der Content-/Carry-Child-Erzeugung ausgespart.
  Damit entstehen bei `RefreshSlotGroups()` nicht länger neun unreferenzierte
  Gear-Group- und Address-VMs, die anschließend sofort verworfen wurden.
- Zwei neue Regressionen verwenden den echten PlayerController-/
  PlayerState-/Equipment-Pfad sowie einen typisierten
  Cross-Inventory-Transfer-Roundtrip. Sie sichern neue inventarlokale
  `EntryId` bei gleicher Item-ID, stabile beteiligte und unbeteiligte
  Entry-VM-Pointer, vollständige Container-Handle-Trennung, wiederholtes
  Refresh-Reuse und konstante direkte Child-Objektzahlen ab.
- Die optionalen Fragment-Presenter werden weiterhin bei Entry-Refresh
  klassenbasiert neu aufgebaut. Da aktuell kein aktiver Consumer existiert,
  bleibt dieser Grandchild-Vertrag ein dokumentierter separater
  MVVM-Legacy-Rest und wurde nicht mit dem Item-/Container-Identitätsschnitt
  vermischt.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut;
  der abschließende Build meldete 4 von 4 Actions und
  `Result: Succeeded`.
- Die vollständige `SurvivalRpg`-Automation meldete 172 von 172 Tests
  erfolgreich: Combat 1, Crafting 8, Equipment 6, GameFeatures 3,
  Harvesting 1, Input 2, Inventory 124, Save 2 und UI 25.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 6: 10 von 22 Punkten abgeschlossen (45,5 %).
- Gesamtfortschritt der verbindlichen Checkliste: 76 von 94 Punkten
  abgeschlossen (80,9 %), 18 Punkte offen.
- Nächster Schnitt: nur tatsächlich geänderte FieldNotify-Felder senden.

Verifizierter Phase-6B-Zwischenstand vom 2026-07-23:

- Der vollständige aktive Inventory-MVVM-Schnitt wurde geprüft und
  vereinheitlicht: Inventory-Fragmente, Entry und Panel, Player-Address-,
  Group- und Aggregate-VMs, Loadout, Actionbar und Weapon-Cooldown, Nested
  Container, BaseStorage sowie Crafting-Ingredient, -Output, -Recipe, -Job
  und -Station. Die 234 zuvor vorhandenen FieldNotify-Emit-Pfade wurden dabei
  auf ihren tatsächlichen Änderungsvertrag und ihre Callback-Reihenfolge
  geprüft.
- Zusammengehörige Refreshes berechnen jetzt zuerst den vollständigen neuen
  Snapshot und alle Änderungsflags, weisen anschließend sämtliche finalen
  Felder zu und senden erst danach FieldNotify. Ein Callback kann dadurch
  nicht mehr die Hälfte des alten und die Hälfte des neuen Zustands sehen.
  Jedes Feld wird höchstens dann gemeldet, wenn sein eigener Wert tatsächlich
  geändert wurde.
- Parent-Arrays melden nur noch echte strukturelle Änderungen an Anzahl,
  Reihenfolge oder Child-VM-Pointer. In-place aktualisierte stabile Child-VMs
  erzeugen keine künstliche Parent-Array-Änderung. Die bewusst weiterhin pro
  Entry neu aufgebauten optionalen Fragment-Presenter bleiben davon
  ausgenommen; ihr Pointer-Array ändert sich real und bleibt ein separat
  dokumentierter MVVM-Legacy-Rest.
- Neu erzeugte `FText::Format`-, `AsNumber`-, `FromString`- und
  `FromName`-Werte werden tief beziehungsweise invariant-lexikalisch
  verglichen. Dadurch entfallen identische Refresh-Notifications, ohne die
  Localization-History oder verschiedene Text-Keys mit momentan gleicher
  Übersetzung zusammenzulegen.
- Panel- und Crafting-Rebinds wechseln Listener und finale Quellen jetzt
  direkt. Sie veröffentlichen weder ein kurzzeitig leeres Entry-Array noch
  `null` für Station oder RequestingActor. Der erste FieldNotify-Callback
  sieht bereits die vollständige neue Bindung. Explizites Unbind behält
  dagegen seinen neutralen Endzustand und seine bisherigen imperativen
  Delegates.
- Nested Container behält `OnPresentationChanged` als imperatives
  Präsentationsende bei erfolgreichem Open, Panel-Refresh und explizitem
  Close, während seine deklarativen Felder nur bei echten Änderungen
  melden. Crafting finalisiert Jobs, Pause-/Resume-Text und davon abhängige
  Action-Verfügbarkeit vor dem ersten Jobs-Callback. Cooldown-Felder werden
  einzeln und exakt publiziert.
- Dieser Schnitt ändert weder Gameplay-Autorität noch Replikation,
  Inventartransaktionen oder Save-Daten. Die UI bleibt eine read-only
  Projektion der serverautoritativen Lyra-rooted RPG-Systeme. Für diese reine
  C++-/Testphase sind keine manuellen Widget-, MVVM- oder Asset-Anpassungen
  im Editor erforderlich.
- Acht neue Automationtests prüfen identische Refreshes, isolierte
  Feldänderungen, stabile Parent-Arrays, atomare erste Callback-Snapshots,
  Panel-A-zu-B-Rebind, Nested-Presentation, BaseStorage-Ressourcen und
  Crafting-Station-Rebind. Die gezielten Suiten meldeten Inventory 5 von 5,
  BaseStorage 1 von 1 und Crafting 2 von 2 erfolgreich.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut.
  Der vollständige Source-Build meldete 29 von 29 Actions, der
  Fixture-Korrektur-Build 4 von 4 Actions; beide endeten mit
  `Result: Succeeded`.
- Die vollständige `SurvivalRpg`-Automation meldete 180 von 180 Tests
  erfolgreich: BaseStorage 1, Combat 1, Crafting 10, Equipment 6,
  GameFeatures 3, Harvesting 1, Input 2, Inventory 129, Save 2 und UI 25.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 6: 11 von 22 Punkten abgeschlossen (50,0 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 77 von 97
  Punkten abgeschlossen (79,4 %), 20 Punkte offen. Die Gesamtzahl enthält
  jetzt die drei auf Nutzerwunsch vorgemerkten Abschlussaufgaben aus
  Phase 8; deshalb ist der Prozentsatz trotz eines weiteren abgeschlossenen
  Implementierungspunkts gegenüber dem vorherigen 94-Punkte-Stand niedriger.
- Nächster Schnitt: Inventory-Invalidierungen pro Commit/Tick bündeln.

Verifizierter Phase-6C-Zwischenstand vom 2026-07-24:

- Message-getriebene Invalidierungen der aktiven Inventory-Panel-, Player-,
  Actionbar-, BaseStorage- und Crafting-Projektionen werden jetzt pro
  ViewModel und betroffener Refresh-Domain bis zum nächsten World-Tick
  gebündelt. Mehrere Nachrichten desselben autoritativen Commits erzeugen
  dadurch höchstens einen entsprechenden UI-Rebuild je Consumer, ohne
  getrennte Inventare oder ViewModel-Instanzen zusammenzulegen.
- Die gemeinsame presentation-only `FRpgViewModelInvalidationQueue` verwendet
  einen Weak-Target-Timer, den ursprünglichen World-TimerManager und eine
  Generation zum Verwerfen veralteter Callbacks. Der Callback konsumiert
  seinen Pending-Zustand vor dem Rebuild, sodass eine reentrante Nachricht
  sauber einen weiteren Tick einplanen kann.
- Player Inventory und Crafting vereinigen gleichzeitig ankommende
  Teilinvalidierungen als Domain-Maske und führen Gear, Slot-Gruppen und
  Actionbar beziehungsweise Station, Rezepte/Details und Jobs in
  deterministischer Reihenfolge aus. Ein expliziter Teilrefresh kann eine
  bereits vorgemerkte Crafting-Domain erfüllen, ohne andere Pending-Domains
  zu verlieren.
- Explizite Refresh-, Bind-, Unbind- und Destruct-Pfade bleiben synchron und
  löschen ihre Pending-Queue, bevor sie den finalen Zustand publizieren.
  Gameplay-Messages selbst bleiben synchron; Gameplay-Autorität,
  Replikation, RPCs, Inventartransaktionen und Save-Daten wurden nicht
  verändert. Die UI bleibt eine read-only Projektion der
  serverautoritativen Lyra-rooted RPG-Systeme.
- Die echte Unity-Kompilierung deckte einen zuvor verdeckten Namenskonflikt
  gleichnamiger dateilokaler FieldNotify-Textflags auf. Die betroffenen
  Konstanten besitzen nun pro Implementierungsdatei eindeutige Namen; ihr
  Verhalten ist unverändert.
- Sieben neue Regressionen sichern Commit-Bündelung, Trennung verschiedener
  Inventare, Cancellation und stale Callbacks, Reentrancy über zwei Ticks,
  selektive Player-Domain-Vereinigung sowie BaseStorage- und
  Crafting-Nachrichtenbursts. Die gezielten Suiten meldeten Inventory
  5 von 5, BaseStorage 1 von 1 und Crafting 1 von 1 erfolgreich.
- Bewusst offen bleiben die separaten Loadout- und Weapon-Cooldown-
  Projektionen sowie widget-lokale doppelte Refreshes. Auch der
  BaseStorage-Producer sendet für seine erste Ressourcenzeile weiterhin zwei
  Rohmeldungen ohne gemeinsame Commit-/Revision-ID; die UI bündelt diese
  bereits, die Producer-Bereinigung ist jedoch ein eigener späterer Schnitt.
  Ältere Split-, Move-, Rebase- und Sort-Producer können weiterhin partielle
  Zustände melden und werden nicht durch diese Präsentationsoptimierung als
  atomar eingestuft. Ein echter Seamless-Travel-/World-Teardown-Lauf bleibt
  außerdem manuelle Integrations-QA.
- Für diese reine C++-/Testphase sind keine manuellen Widget-, MVVM-,
  Blueprint- oder Asset-Anpassungen im Editor erforderlich.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut.
  Der Build nach der Unity-Korrektur meldete 12 von 12 Actions, der
  abschließende Test-Fixture-Build 4 von 4 Actions; beide endeten mit
  `Result: Succeeded`.
- Die vollständige `SurvivalRpg`-Automation meldete 187 von 187 Tests
  erfolgreich: BaseStorage 2, Combat 1, Crafting 11, Equipment 6,
  GameFeatures 3, Harvesting 1, Input 2, Inventory 134, Save 2 und UI 25.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Blueprint-Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 6: 12 von 22 Punkten abgeschlossen (54,5 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 78 von 97
  Punkten abgeschlossen (80,4 %), 19 Punkte offen.
- Nächster Schnitt: weitere gepoolte Screens auf denselben
  Unbind-/Pooling-Vertrag bringen.

Verifizierter Phase-6D-Zwischenstand vom 2026-07-24:

- Der zentrale CommonUI-Screen-Router verwaltet Deactivation-Bindungen jetzt
  pro konkretem Pool-Checkout über eine eindeutige Checkout-ID und das exakte
  Delegate-Handle. Abgebrochene Async-Pushes, synchrones Reuse derselben
  Widget-Instanz und LocalPlayer-Teardown können dadurch keinen Callback eines
  alten Checkouts auf einen neueren Screen anwenden.
- Context-Menü, Split-Dialog und Drop-Confirmation werden auf allen sechs
  Öffnungspfaden im Initialisierungs-Callback des CommonUI-Layer-Pushes
  vollständig vorbereitet und vom Host verfolgt, bevor die gepoolte Instanz
  aktiviert wird. Ein `OnActivated`-Pfad sieht dadurch bereits ausschließlich
  den Zustand des aktuellen Checkouts.
- Context- und Split-Quellen werden nur noch schwach gehalten. Deactivation und
  `NativeDestruct` neutralisieren Quellen, IDs, Requests, Actions,
  Auswahlwerte und dynamische Controls. Der Interaction-Screen und sein
  Controller-Action-Host verwenden dieselbe idempotente Release-Grenze auch
  bei einem Slate-Reconstruct ohne vorherige Aktivierung.
- Ein AssetRegistry-Vertrag schließt die project-owned
  Inventory-Interaction-Screen-Familie auf `CUI_PlayerInventory`,
  `CUI_StorageSpatial`, `CUI_BaseTerminalSpatial` und
  `CUI_CraftingStationSpatial`. Die fünf semantischen Routen Inventory,
  Storage, Loot, BaseTerminal und Crafting bleiben auf `UI.Layer.GameMenu`,
  Input-Suspension und Single-Instance festgelegt. Editor-transiente
  `SKEL_`-/Reinstancing-Klassen werden dabei nicht als authored Screens
  fehlgezählt.
- Eine neue Lifecycle-Regression prüft für Context, Split und Drop die
  Sequenz Checkout A ohne Aktivierung, Slate-Destruct, Reconstruct, Checkout B,
  Aktivierung und Deaktivierung. Sie belegt den ownerless
  `NativeDestruct`-Fallback, den bereits initialisierten Zustand beim
  Aktivieren sowie die vollständige Neutralisierung zwischen zwei
  Pool-Nutzungen; die sechs realen Host-Push-Pfade werden zusätzlich durch
  ihren gemeinsamen CommonUI-Initialisierungs-Callback abgesichert.
- Gameplay-Autorität, Replikation, Inventory-Transaktionen und Save-Daten
  wurden nicht verändert. Die UI bleibt eine read-only Projektion der
  serverautoritativen Lyra-rooted RPG-Systeme. Für diesen C++-/Test-Schnitt
  sind keine manuellen Widget-, MVVM-, Blueprint- oder Asset-Anpassungen im
  Editor erforderlich. Als interaktive QA bleiben wiederholtes Öffnen,
  Payload A nach B, Back/Fokus sowie Context, Split und Drop mit Mouse und
  Gamepad empfohlen.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut;
  der abschließende Build meldete 6 von 6 Actions und
  `Result: Succeeded`.
- Die gezielten Regressionen
  `SurvivalRpg.Inventory.UI.ActionModalPoolingLifecycle` und
  `SurvivalRpg.UI.ScreenRegistry.InventoryScreenFamilyClosure` meldeten
  jeweils 1 von 1 Test erfolgreich.
- `SurvivalRpg.Inventory` meldete 135 von 135 Automationtests erfolgreich.
- `SurvivalRpg.UI` meldete 26 von 26 Automationtests erfolgreich; darin sind
  auch der Screen-Family-Vertrag und der zuvor gehärtete
  `ScreenRouter.AsyncCloseLifecycle` enthalten.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Blueprint-Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 6: 13 von 22 Punkten abgeschlossen (59,1 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 79 von 97
  Punkten abgeschlossen (81,4 %), 18 Punkte offen.
- Nächster Schnitt: Blueprint-MVVM als einzigen Leaf-Datenbindungsweg
  verwenden; Blueprint-Events bleiben Animation und imperativer Präsentation
  vorbehalten.

Verifizierter Phase-6E-Zwischenstand vom 2026-07-24:

- Die vier deklarativen MVVM-Leaf-Verträge `CUI_ActionBarSlotEntry`,
  `CUI_AddressSlotEntry`, `CUI_GearSlot` und `CUI_BaseResourceEntry` besitzen
  jetzt jeweils genau einen typisierten nativen Presenter sowie genau eine
  optionale, manuell gesetzte MVVM-Source. Stabile Itemdaten gelangen nur noch
  über diese Source in die authored OneWay-to-Destination-Bindings.
- Die parallelen Datenereignisse `BP_OnActionBarSlotViewModelSet`,
  `BP_OnAddressSlotViewModelSet` und `BP_OnEquipmentSlotUpdated` samt nativen
  Aufrufen wurden entfernt. Der noch tatsächlich authorisierte Gear-Graph
  wurde aus `CUI_GearSlot` entfernt und das Asset kompiliert und gespeichert.
  Blueprint-Hooks bleiben nur für Release, Auswahl/Fokus, Drag/Drop,
  Inspektion, Animation und andere imperative Präsentation bestehen.
- `CUI_BaseResourceEntry` erbt nun von
  `URpgBaseResourceEntryWidget`. Der native `IUserObjectListEntry`-Presenter
  übernimmt List-Item-Zuweisung, exakte Source-Injektion, Release und
  `NativeDestruct`; der frühere Blueprint-Pfad zur Source-Zuweisung ist
  entfernt. Das Widget ist wie die übrigen datengetriebenen Leaves auch ohne
  Player-Kontext initialisierbar und räumt seine optionale Source beim Pooling
  zuverlässig.
- Der ActionBar-Presenter verwendet für ListView-Release und
  `NativeDestruct` dieselbe idempotente Release-Grenze. Er entfernt Delegates,
  VM, Coordinator, MVVM-Source und lokalen Preview-State und löscht aus der
  gemeinsamen Interaction-Session nur den exakt von ihm veröffentlichten
  ActionBar-Slot; ein inzwischen aktiver Peer-Slot bleibt erhalten.
- Der permanente Editor-Vertrag prüft alle authored Ubergraph-, Function-,
  Macro- und Delegate-Graphs auf parallele Datenwriter. Zusätzlich sichert er
  pro Leaf die exakten Source-Felder, Destination-Widgets beziehungsweise
  Destination-Funktionen, Conversion-Funktionen, Binding-Richtung sowie die
  einzige kanonische Manual-Source ab. Die Runtime-Regressionen prüfen
  BaseResource-VM-A/B/Release/Rebind und den ActionBar-Pfad mit realem
  Preview-Target, Peer-Wechsel, Pool-Reuse und Destruct eines manuell
  platzierten Slots.
- Gameplay-Autorität, Inventory-Transaktionen, Replikation und Save-Daten
  wurden nicht verändert. Die UI bleibt eine read-only Projektion der
  serverautoritativen Lyra-rooted RPG-Systeme.
- Die beiden betroffenen Widget-Blueprints wurden durch die Migration
  kompiliert, gespeichert und erfolgreich validiert. Für diesen Schnitt sind
  deshalb keine manuellen Reparent-, MVVM-Source- oder Event-Graph-Arbeiten im
  Editor nötig. Als interaktive Sichtprüfung bleiben ActionBar/Gear mit
  wechselnden Items sowie BaseResource-Listen über wiederholtes
  Öffnen/Schließen sinnvoll.
- Bewusste Grenze: `CUI_CarrySlot` bleibt bis zu seinem eigenen Folgepunkt der
  aktive imperative Address-Spezialfall mit gemischter Beobachtung. Die
  ungenutzten Spatial-Item-VM-Writer und der wirkungslose Drag-State-Switch
  bleiben der referenzbasierten Phase-7-Bereinigung zugeordnet; sie werden hier
  nicht fälschlich als abgeschlossene MVVM-Leaves gewertet.
- `SurvivalRpgEditor Win64 Development` wurde mit Unreal Engine 5.8 gebaut;
  der abschließende Build meldete 10 von 10 Actions und
  `Result: Succeeded`.
- Die gezielten Regressionen
  `SurvivalRpg.Inventory.UI.ActionBarSlotEntryPooling`,
  `SurvivalRpg.Inventory.UI.BaseResourceEntryPooling` und
  `SurvivalRpg.Inventory.UI.LeafMvvmAssetContracts` meldeten jeweils 1 von 1
  Test erfolgreich.
- `SurvivalRpg.Inventory` meldete 137 von 137 Automationtests erfolgreich.
- `SurvivalRpg.UI` meldete 26 von 26 Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Blueprint-Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Fortschritt Phase 6: 14 von 22 Punkten abgeschlossen (63,6 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 80 von 97
  Punkten abgeschlossen (82,5 %), 17 Punkte offen.
- Nächster Schnitt: Den öffentlich generierten Manual-Source-Setter durch
  einen unverletzbaren nativen Getter-/PropertyPath-Vertrag ersetzen.

Verifizierter Phase-6F-Zwischenstand vom 2026-07-24:

- `CUI_PlayerInventory` besitzt weiterhin genau eine screen-scoped
  `URpgPlayerInventoryViewModel`. Der native Presenter erzeugt diese stabile
  Instanz vor `Super::NativeOnInitialized()`, sodass sie bereits vorhanden ist,
  wenn die MVVM-View-Extension an das Widget gebunden wird.
- Die authored Source `RpgPlayerInventoryViewModel` verwendet exakt den
  PropertyPath `GetPlayerInventoryViewModel`. Sie ist nicht optional und
  kompiliert nicht setzbar. Es werden weder ein öffentlicher Source-Setter noch
  ein zusätzlicher Getter generiert; die interne Source-Property ist
  `BlueprintReadOnly`, nicht `ExposeOnSpawn`, `BlueprintPrivate` und besitzt
  keinen `BlueprintSetter`.
- `URpgPlayerInventoryViewModel` erlaubt über
  `MVVMAllowedContextCreationType` nur noch `PropertyPath`. Die frühere
  manuelle Source-Injektion und das erneute Zurücksetzen beziehungsweise
  Zurückerobern der Source bei Aktivierung sind entfernt.
- Initialisierung, Freigabe und erneute Auswertung der Source folgen dem
  regulären MVVM-Construct-/Destruct-/Reconstruct-Lifecycle. Die native
  VM-Instanz bleibt über CommonUI-Pooling stabil; ein fremder
  `SetViewModel`-Versuch wird vom kompilierten Vertrag abgelehnt und kann die
  native Source nicht ersetzen. Eine gültige Activation vor dem ersten
  Slate-Construct prüft bereits den kompilierten Vertrag, erwartet den
  Runtime-Source-Pointer aber erst nach MVVM-Source-Initialisierung.
- Bewusste Grenze: Die dynamischen Leaves ActionBar, Address, Gear und
  BaseResource bleiben optionale Manual-Sources. Ihre Presenter wechseln den
  Source-Pointer während ihrer Lebensdauer von A nach B und anschließend auf
  `null`; ein authored PropertyPath wird bei diesen Wechseln nicht automatisch
  neu ausgewertet. Ihre spätere Migration benötigt deshalb eine explizite
  Source-Reinitialisierung oder stabile Adapter. Phase 6F behauptet ausdrücklich
  nicht, dass global alle Manual-Source-Setter entfernt wurden.
- Die permanenten Verträge
  `SurvivalRpg.Inventory.UI.PlayerMvvmSourceAssetContract`,
  `SurvivalRpg.Inventory.UI.PlayerViewModelComposition`,
  `SurvivalRpg.Inventory.UI.PlayerViewModelPooling` und
  `SurvivalRpg.Inventory.UI.PlayerAuthoredContentHosts` liefen im gezielten
  Player-Filter 4 von 4 erfolgreich. Der UE-5.8-Build
  `SurvivalRpgEditor Win64 Development` endete mit `Result: Succeeded`.
- Die vollständige Suite `SurvivalRpg.Inventory` meldete 138 von 138
  Automationtests erfolgreich; `SurvivalRpg.UI` meldete 26 von 26
  Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Blueprint-Compilerwarnungen und 0 nicht ladbaren Blueprints.
- Das migrierte `CUI_PlayerInventory`-Asset ist kompiliert und gespeichert;
  im Editor ist daher keine verpflichtende MVVM-Source- oder Blueprint-Arbeit
  nötig. Als interaktive QA bleiben wiederholtes Öffnen, Schließen und erneutes
  Öffnen des Inventory-Screens sowie die Kontrolle sinnvoll, dass
  `CreateWidget` keinen Eingabepin für die Player-Inventory-VM anbietet.
- Fortschritt Phase 6: 15 von 22 Punkten abgeschlossen (68,2 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 81 von 97
  Punkten abgeschlossen (83,5 %), 16 Punkte offen.
- Nächster Schnitt: BlueprintCallable Lifecycle-Mutatoren der Aggregate-VM
  nach Asset-Referenzprüfung auf eine native Presenter-Oberfläche reduzieren.

Verifizierter Phase-6G-Zwischenstand vom 2026-07-24:

- Der Vorabscan über 2.318 Projektpakete aus `Content` und `Plugins`
  (2.314 `.uasset`, 4 `.umap`) fand keinen exakten Blueprint-Aufruf von
  `BindPlayerController`, `UnbindPlayerInventory` oder `RefreshAll` auf
  `URpgPlayerInventoryViewModel`. Die beiden reinen Namenstreffer für
  `BindPlayerController` gehören nachweislich zu den getrennten
  `URpgActionBarViewModel`- und
  `URpgWeaponAbilityLoadoutViewModel`-Verträgen und bleiben unangetastet.
- `BindPlayerController` und `UnbindPlayerInventory` sind jetzt öffentliche,
  nicht reflektierte C++-Lifecycle-Einstiege für die nativen Player-, Storage-
  und Crafting-Presenter. `RefreshAll` ist private Implementierung der
  Aggregate-VM. Damit können Blueprints die Beobachtungs- und Refresh-Grenze
  nicht mehr umgehen; die Blueprint-Oberfläche bleibt auf read-only Getter
  und Präsentationssignale beschränkt.
- Der ungenutzte Weak Pointer `OwningPlayerController` wurde entfernt. Die
  bestehenden Identitäts- und FieldNotify-Verträge lösen eine erneute
  Projektion jetzt über den echten idempotenten Presenter-Rebind aus, statt
  den privaten Komplettrefresh direkt aufzurufen.
- Der permanente Vertrag
  `SurvivalRpg.Inventory.UI.PlayerAggregateVmLifecycleAssetReferences`
  erzwingt, dass alle drei Lifecycle-Methoden nicht reflektiert sind. Er lädt
  und untersucht außerdem alle Projekt-Blueprints sowie Level-Script-
  Blueprints aus Projektmaps und prüft Funktionsname plus exakten Owner, damit
  gleichnamige APIs anderer ViewModels keine falschen Treffer erzeugen.
  Der verifizierte Lauf scannte 228 Blueprint-/Level-Script-Objekte,
  4 Maps, 2 Level Scripts und 974 CallFunction-Nodes ohne Aggregate-
  Lifecycle-Aufruf.
- Der UE-5.8-Build `SurvivalRpgEditor Win64 Development` endete mit
  `Result: Succeeded`. Die gezielte Suite
  `SurvivalRpg.Inventory.ViewModel` meldete 12 von 12, die vollständige Suite
  `SurvivalRpg.Inventory` 139 von 139 und `SurvivalRpg.UI` 26 von 26
  Automationtests erfolgreich.
- `CompileAllBlueprints` endete mit Prozesscode 0, 0 Compilerfehlern,
  16 bekannten Blueprint-Compilerwarnungen und 0 nicht ladbaren Blueprints.
  Damit ist nach dem Entfernen der Reflection-Metadaten kein verborgener
  serialisierter K2-Aufruf übrig.
- Es wurden keine Editor-Assets geändert; Resave, CoreRedirect oder manuelle
  Blueprint-Migration sind für diesen Schnitt nicht nötig. Als optionale
  interaktive QA bleiben Aktivieren, Deaktivieren und erneutes Aktivieren der
  Player-, Storage- und Crafting-Screens sinnvoll.
- Fortschritt Phase 6: 16 von 22 Punkten abgeschlossen (72,7 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 82 von 97
  Punkten abgeschlossen (84,5 %), 15 Punkte offen.
- Nächster Schnitt: Player-/Storage-Lifecycle mit echtem PlayerController,
  PlayerState, kanonischem Inventory und Listener-Cleanup als Integrationstest
  abdecken.

Verifizierter Phase-6H-Zwischenstand vom 2026-07-24:

- Der dauerhafte Integrationstest
  `SurvivalRpg.Inventory.UI.PlayerStorageLifecycleIntegration` schließt die
  Lücke der bisherigen Pooling-Tests: Er verwendet einen echten
  projektabgeleiteten PlayerController und PlayerState, das tatsächlich am
  PlayerState liegende kanonische Inventory sowie die realen Layout-,
  Equipment- und Actionbar-Komponenten des Controllers.
- Die authored Screens `CUI_PlayerInventory` und `CUI_StorageSpatial` werden
  mit demselben OwningPlayer erzeugt. Storage erhält das kanonische
  PlayerState-Inventory als Primary und ein getrenntes Secondary-Inventory;
  dadurch wird auch der kanonische Primary-Guard des Storage-Presenters
  tatsächlich durchlaufen.
- Beide Screens behalten getrennte, screen-owned Aggregate-VMs, beobachten
  aber exakt dieselben vier Gameplay-Quellen. Der Test prüft für Inventory,
  Layout, Equipment und Actionbar jeweils einen eigenen gültigen
  GameplayMessage-Handle sowie nach dem Unbind vier freigegebene Quellen,
  vier ungültige Handles und keine ausstehende Refresh-Domain.
- Alle vier Nachrichtenwege werden einzeln mit ihrer exakten Domain-Maske und
  ihren erwarteten Gear-, SlotGroup- und Actionbar-Callbacks geprüft. Der
  Inventory-Pfad läuft zusätzlich über einen echten
  `GrantItemDefinition`-Producer statt nur über einen synthetischen
  Test-Broadcast.
- Vor der Deaktivierung vorgemerkte Refreshes werden verworfen. Nach dem
  Player-Unbind reagiert nur noch der aktive Storage-Screen; nach dem
  Storage-Unbind reagiert keine VM mehr. Eine erneute Aktivierung verwendet
  dieselben gepoolten VM-Instanzen, registriert wieder exakt vier Listener
  pro Screen und erzeugt für den gemischten Nachrichtenburst genau einen
  präsentationswirksamen Refresh je Domain. Doppelte rohe Router-Aufrufe
  würden durch die Invalidation-Queue koalesziert und sind bewusst nicht
  Gegenstand dieses Tests.
- Der Test deckt bewusst den nativen Presenter- und
  Activation-/Deactivation-Lifecycle der authored Screens in einer
  isolierten Standalone-Welt ab. Er ist kein vollständiger CommonUI-
  Layer-/ActionRouter-E2E-Test; die bekannten Controller-Action-Warnungen
  des Commandlet-Fixtures ohne Root-Layer berühren die geprüften
  Lifecycle-Invarianten nicht.
- Für diesen reinen C++-/Test-Schnitt sind keine manuellen Blueprint-,
  MVVM- oder Asset-Anpassungen im Editor nötig. Als optionale interaktive QA
  bleiben Player-Inventory und Storage wiederholt öffnen, zwischen beiden
  wechseln, schließen und erneut öffnen sowie Live-Updates in beiden aktiven
  Screens prüfen.
- Der abschließende UE-5.8-Build
  `SurvivalRpgEditor Win64 Development` endete mit
  `Result: Succeeded`. Der gezielte Integrationstest meldete 1 von 1,
  `SurvivalRpg.Inventory` 140 von 140 und `SurvivalRpg.UI` 26 von 26
  Automationtests erfolgreich.
- Fortschritt Phase 6: 17 von 22 Punkten abgeschlossen (77,3 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 83 von 97
  Punkten abgeschlossen (85,6 %), 14 Punkte offen.
- Nächster Schnitt: Gear-, Carry- und Content-Widgets auf denselben
  Coordinator-/MVVM-Pfad bringen und Kontextmenüs Fähigkeiten abfragen
  lassen, statt Fragmente zu erraten.

Verifizierter Phase-6I-Zwischenstand vom 2026-07-24:

- `FRpgInventoryScreenPresentationContext` bildet nun den einen transienten
  Composition-Vertrag aus screen-owned Drag-/Action-Coordinator, Panel-
  Navigator und Modal-Presenter. Gear-, Carry- und Content-Leaves erhalten
  diesen Kontext zusammen mit ihrer exakten VM über einen gemeinsamen
  `BindInventoryPresentation`-/`ReleaseInventoryPresentation`-Lifecycle.
  Unvollständige Kontexte werden fail-closed freigegeben statt teilweise
  gebunden.
- `CUI_PlayerInventory` bleibt der Composition Root: Es bindet zuerst die
  stabile Aggregate-VM und komponiert daraus die authored Gear-, Carry- und
  Content-Leaves. Beim Deaktivieren wird zuerst die Aggregate-Projektion
  ungebunden, danach werden alle Leaf-VMs, Manual-MVVM-Sources, Delegates,
  Coordinatoren, Presenter-Hosts, Selection- und Preview-Zustände gelöst.
  Der Navigator wird als letzter Schritt geleert, damit ein finaler
  Aggregate-Callback keinen inaktiven Pooling-Screen erneut registriert.
- Ein Base-Lifecycle-Guard paart CommonUI-Aktivierung und -Deaktivierung
  exakt. Das spätere `NativeDestruct` eines bereits deaktivierten gepoolten
  Screens löst damit keinen zweiten Aggregate-Unbind und keine kurzzeitige
  Re-Komposition seiner Leaves mehr aus. Der Carry-Destruct-Pfad dispatcht
  seinen virtuellen Release ebenfalls nur noch einmal.
- `IRpgInventoryContextActionSource` und
  `FRpgInventoryContextActionSnapshot` vereinheitlichen Content-Entry,
  Address/Carry und Equipment als exakte, read-only Kontextquelle. Der
  Screen besitzt nur noch einen Open-Pfad und das gepoolte Kontextmenü nur
  noch einen Initialisierungs-, Revalidierungs- und Dispatch-Pfad. Vor jeder
  Aktion werden Source-Art, Item-/Entry-ID, Platzierung, Address oder
  Equipment-Slot, Menge und aktuell verfügbare Action erneut verglichen.
  Ein veralteter Quick-Access-Picker schließt fail-closed.
- `FRpgInventoryItemCapabilities` ist die gemeinsame stateless Domain-Grenze
  für gültige Item-Container-Grids, Use-Vertrag und Verbrauchsmenge,
  Manual-Drop-Policy, räumliche Größe/Rotation und die primäre
  Use-versus-Equip-Hybridentscheidung. Coordinator und autoritative
  `RpgInventoryUiActionComponent` verwenden dieselben abgeleiteten Regeln;
  UI oder ViewModels speichern keine Capability-Bools und mutieren weiterhin
  keinen Gameplay-State.
- Die Server-Autorität bleibt unverändert bei Inventory, Layout, Equipment
  und `RpgInventoryUiActionComponent`: Die Capability-Projektion ist nur
  lokale Darstellung. Server-RPCs prüfen Zugriff, aktuelle Identität,
  Placement, Menge, Equipment-Konflikte, Container-Subtrees und Gameplay-
  Zustand weiterhin erneut.
- Der echte
  `SurvivalRpg.Inventory.UI.PlayerStorageLifecycleIntegration`-Test prüft
  zusätzlich die authored `Gear_Head`-, `Carry_Weapon1`- und
  `Content_Pockets`-Leaves im tatsächlichen `CUI_PlayerInventory`-
  WidgetTree: gemeinsame screen-owned Coordinatoren, exakte Gear-/Content-
  MVVM-Sources, eindeutige Delegates, vollständiges Deactivate-Release,
  leeren Navigator und sauberen Rebind derselben gepoolten Screen-Objekte.
- Bewusste Restgrenze: `CUI_CarrySlot` besitzt weiterhin seinen getrennt
  geplanten zweiten Address-Observer und noch keine eigene deklarative
  Itemdaten-Source. Phase 6I behauptet ausdrücklich weder dessen
  Ein-Observer-Migration noch die endgültige Active-/Holstered-Presenter-
  Policy; beides bleibt der direkt folgende Checklistenpunkt.
- Es wurden keine Editor-Assets geändert. Für diesen Schnitt sind daher
  kein Resave, kein CoreRedirect und keine manuelle MVVM-/Blueprint-
  Anpassung nötig. Optionale interaktive QA: Player-Inventory wiederholt
  öffnen/schließen, zwischen Gear, Carry und Content per Maus/Gamepad
  wechseln und Kontextmenü, Quick Access, Split, Drop und Pooling-Reopen
  prüfen.
- Der abschließende UE-5.8-Build
  `SurvivalRpgEditor Win64 Development` endete mit
  `Result: Succeeded`. Die gezielten Capability-, Context-Action-, Modal-
  Pooling- und Player-/Storage-Lifecycle-Regressionen waren erfolgreich;
  `SurvivalRpg.Inventory` meldete 141 von 141 und `SurvivalRpg.UI` 26 von 26
  Automationtests erfolgreich.
- `CompileAllBlueprints -ProjectOnly` endete mit Prozesscode 0,
  0 Compilerfehlern, 16 bekannten Compilerwarnungen und 0 nicht ladbaren
  Blueprints.
- Fortschritt Phase 6: 18 von 22 Punkten abgeschlossen (81,8 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 84 von 97
  Punkten abgeschlossen (86,6 %), 13 Punkte offen.
- Nächster Schnitt: `CUI_CarrySlot` auf genau eine VM-Beobachtung reduzieren
  und seine deklarative Address-Source von imperativer Active-/Holstered-,
  Interaktions- und Animationspräsentation eindeutig trennen.

Verifizierter Phase-6J-Zwischenstand vom 2026-07-24:

- `URpgInventoryAddressSlotWidget` besitzt weiterhin den einzigen nativen
  `OnSlotChanged`-Delegate. Sein neuer virtueller
  `RefreshAddressSlotPresentation`-Seam erlaubt spezialisierten
  Address-Presentern, ihren Zustand aus derselben Änderung abzuleiten, ohne
  dieselbe VM ein zweites Mal zu beobachten. Der getrennte
  `ObservedCarryAddress`-/Handler-Pfad wurde aus dem Carry-Slot entfernt.
- `CUI_CarrySlot` besitzt nun genau eine optionale manuelle Source mit dem
  kanonischen Namen `RpgInventoryAddressSlotViewModel` und dem exakten Typ
  `URpgInventoryAddressSlotViewModel`. Genau zwei OneWay-Bindings projizieren
  stabile Itemdaten: `Icon -> Self.SetCarryItemIcon` und
  `bRenderItemVisual -> Self.SetCarryItemVisualVisible`.
- Die Presenter-Policy ist damit ausdrücklich getrennt: stabile Itemdaten
  kommen ausschließlich deklarativ aus der exakten Address-Source.
  `Empty`, `Holstered` und `Active`, der semantische Interaction-Preview-State
  sowie Animation/Styling bleiben imperative Präsentation. Dafür existiert
  nur noch der schmale `BP_OnCarrySlotStateChanged`-Hook ohne Item-, Address-
  oder Occupied-Daten. Der alte itemdatenhaltige
  `BP_OnCarrySlotPresentationChanged`-Pfad und seine Legacy-Getter/-Writer
  wurden entfernt.
- Die Lifecycle-Mutatoren für Group-Binding, Panel-Navigation und manuellen
  Refresh sind nicht mehr `BlueprintCallable`. `CUI_PlayerInventory` bleibt
  der native Composition Root; im Blueprint sind nur read-only Zustand, die
  beiden MVVM-Destinations und der schmale Animations-/Styling-Hook sichtbar.
- `ActiveIndicator` und `HolsteredIndicator` werden jetzt nativ aus einem
  gegenseitig ausschließenden Präsentationszustand gesetzt; zuvor waren sie
  nach `Construct` nur eingeklappt und wurden nicht mehr aktualisiert.
  Equipment-/Handwechsel aktualisieren Carry erst nach der zugehörigen
  SlotGroup-Reconciliation, sodass kein kurzlebiger falscher Zustand eine
  zweite Animation auslöst. Screen-lokale Pending-/Rejected-Wechsel
  refreshen alle drei Carry-Slots ausdrücklich. Die UI bleibt read-only;
  Inventory-, Layout- und Equipment-Autorität wurden nicht verschoben.
- Ein Carry-Binding akzeptiert fail-closed nur noch eine semantische
  MainHand-/OffHand-Carry-Gruppe mit `1x1`-Grid, genau einer passenden
  Carry-Address, identischem Container-Handle und Koordinate `[0,0]`.
  Malformed Gruppen werden weder sichtbar noch beim Screen-Kontext und
  Navigator registriert. Rebind `A -> B`, Release und Pool-Reuse lösen Source
  und Delegate atomisch; weil das Leeren einer optionalen MVVM-Source keine
  Defaultwerte in Ziele schreibt, neutralisiert Release Icon und Sichtbarkeit
  zusätzlich ausdrücklich.
- Der Assetvertrag prüft am echten `CUI_CarrySlot` exakten Parent, genau eine
  Source, genau zwei Bindings und die Abwesenheit der Legacy-Graphwriter.
  Der neue Carry-Lifecycle-Test prüft `A -> B -> Release -> Reuse` mit
  Delegate-Anzahlen `1 -> 1/0 -> 0 -> 1`. Der echte
  Player-/Storage-Lifecycle prüft die MVVM-Source und den Address-Delegate
  über Aktivierung, Deaktivierung und Reaktivierung als
  `aktuell/1 -> null/0 -> aktuell/1`. Er grantet zusätzlich ein echtes
  Weapon-Item, verschiebt es über den autoritativen Equipment-Intent nach
  Carry und beweist den Präsentationswechsel
  `Holstered -> Active -> Holstered` über den realen Loadout-Pfad.
- Das authored Asset wurde automatisch migriert, kompiliert und gespeichert.
  Im Editor sind keine manuelle Source-, Binding-, Graph- oder
  CoreRedirect-Anpassung nötig. Empfohlene interaktive QA: alle drei
  Carry-Slots leer, holstered und active prüfen; Handwechsel, Pending/
  Rejected, Mouse-/Gamepad-Interaktion, Drag und wiederholtes
  Schließen/Öffnen des gepoolten Inventory-Screens durchspielen.
- Der abschließende UE-5.8-Build
  `SurvivalRpgEditor Win64 Development` endete mit
  `Result: Succeeded`. Die gezielten Carry-Lifecycle-, Asset-, Pooling-,
  Context-Action- und Player-/Storage-Regressionen waren erfolgreich.
  `SurvivalRpg.Inventory` meldete 142 von 142 und `SurvivalRpg.UI` 26 von 26
  Automationtests erfolgreich.
- `CompileAllBlueprints -ProjectOnly` endete mit Ergebnis 0,
  0 Compilerfehlern, 16 bekannten Compilerwarnungen und 0 nicht ladbaren
  Blueprints. `CUI_CarrySlot` kompilierte dabei erfolgreich; `git lfs fsck`
  meldete einen intakten LFS-Zustand.
- Fortschritt Phase 6: 19 von 22 Punkten abgeschlossen (86,4 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 85 von 97
  Punkten abgeschlossen (87,6 %), 12 Punkte offen.
- Nächster Schnitt: `RpgInventoryUiActionComponent` in schmale
  Domain-Handler aufteilen; der Controller bleibt RPC-Eigentümer und kann
  während der Migration als Fassade bestehen.

Verifizierter Phase-6K-Zwischenstand vom 2026-07-24:

- `URpgInventoryUiActionComponent` ist jetzt eine schmale, Controller-owned
  Netzwerkfassade. Alle 46 bisherigen `Server, Reliable, BlueprintCallable`
  Request-RPCs sowie der eine `Client, Reliable` Feedback-Endpunkt bleiben
  dort mit unveränderten Namen und Reflection-Flags bestehen. Ein
  Reflection-Vertragstest prüft die exakte Menge und weist fehlende,
  zusätzliche oder falsch markierte RPCs ab.
- Die synchrone Aktionslogik ist nach acht klaren Domänen getrennt:
  Transaction, Equipment, QuickAccess, ItemUse, ManualDrop, BaseStorage,
  BaseBuilding und Crafting. Die Handler sind kleine stateless C++-Objekte,
  werden nur für einen Aufruf erzeugt und besitzen weder UObject-Lifecycle
  noch replizierten Zustand.
- Transaction und Equipment trennen zusätzlich reine Leseplanung in
  `FRpgInventoryTransactionQueryHandler` und
  `FRpgInventoryEquipmentQueryHandler`. Compile-time-Verträge sichern, dass
  Command-Handler ausschließlich einen mutablen Component-Kontext erhalten,
  während die Query-Handler mit einem const Kontext auskommen.
- Replay-Caches, Request-Fingerprints, Mutation-Epochen und
  Owning-Client-Feedback bleiben als querschnittlicher Netzwerkzustand auf
  der Component. Die Gameplay-Autorität liegt weiterhin bei Inventory
  Manager, Layout, Loadout und Stationen; UI, ViewModels und Handler erzeugen
  keine parallele autoritative Zuständigkeit.
- Inventory-Zugriff und Transfer-Richtung verwenden nun dieselbe zentrale
  Ermittlung von Crafting-Output-Inventories. Null-, PlayerInventory-,
  Crafting-, BaseArmory- und WorldContainer-Zugriff behalten ihre bisherige
  fail-closed Policy. Doppelte anonyme Hilfsfunktionen wurden eindeutig
  benannt oder zusammengeführt, damit adaptive Einzelkompilierung keine
  späteren Unity-Build-Kollisionen verdeckt.
- Die Fassade schrumpfte von ungefähr 4.129 Zeilen nach Phase 6K2 auf
  1.359 Zeilen. Server-Einstieg, Replay-/Feedback-Infrastruktur und
  Controller-Kontext sind dadurch direkt auffindbar; Domänenänderungen
  erfordern nicht mehr die Bearbeitung einer einzigen mehrere tausend Zeilen
  langen Sammeldatei.
- Es wurden keine UFUNCTION-Signaturen, RPC-Metadaten, Blueprint-Verträge
  oder Editor-Assets migriert. Im Editor sind deshalb kein Asset-Resave,
  kein CoreRedirect und keine manuelle Widget-/MVVM-Neuzuordnung nötig.
  Falls der Editor während der C++-Änderung offen war, sollte er vor der
  interaktiven QA vollständig neu gestartet werden. Empfohlen bleibt ein
  Maus-/Gamepad-Playtest für Move, Transfer, Split, Quick Access, Equip,
  Unequip, Drop, BaseStorage und Crafting.
- Der abschließende UE-5.8-Build
  `SurvivalRpgEditor Win64 Development` wurde mit `-ForceUnity` und
  `-DisableAdaptiveUnity` ausgeführt. Die echten Unity-Module kompilierten
  gemeinsam in 6 von 6 Actions; UBT meldete `Result: Succeeded`. Der
  vorangegangene adaptive Exaktstand war ebenfalls mit 13 von 13 Actions
  erfolgreich.
- `SurvivalRpg.Inventory` meldete 143 von 143,
  `SurvivalRpg.Equipment` 6 von 6 und `SurvivalRpg.UI` 26 von 26
  Automationtests erfolgreich. `CompileAllBlueprints -ProjectOnly` endete
  mit Prozesscode 0, 0 Compilerfehlern, 16 bekannten Compilerwarnungen und
  0 nicht ladbaren Blueprints.
- Bewusste Verifikationsgrenze: Dieser Schnitt ergänzt keinen echten
  Remote-Client/Server-Transport- oder Late-Join-Automationstest. Abgesichert
  sind Reflection-/Facade-Vertrag, standalone Server-Autorität und die
  bestehenden Black-Box-Regressionen; echte Netzwerk-PIE-QA bleibt für die
  phasenübergreifende Replikationsprüfung offen.
- Fortschritt Phase 6: 20 von 22 Punkten abgeschlossen (90,9 %).
- Gesamtfortschritt der erweiterten verbindlichen Checkliste: 86 von 97
  Punkten abgeschlossen (88,7 %), 11 Punkte offen.
- Nächster Schnitt: Manager intern in Storage, Rules/Planner, Transactions
  und Persistence aufteilen, ohne seine öffentliche Autorität zu
  duplizieren.

Verifizierter Phase-6L1-Zwischenstand vom 2026-07-24:

- Der erste Manager-Unterschnitt ist als verhaltensneutraler
  Translation-Unit-Split umgesetzt. `ExportInventoryGraph`,
  `RestoreInventoryGraph`, `RestoreRuntimeCheckpoint` und der vollständige
  atomare Restore-Commit liegen jetzt in
  `RpgInventoryManagerPersistence.cpp`; der versionierte Legacy-Konverter
  bleibt als Persistence-Sibling in `RpgInventoryLegacySnapshot.cpp`.
- Alle 604 verschobenen Methodenzeilen wurden gegen den vorherigen Stand
  abgeglichen. Authority- und Mutation-Lock-Gates, Runtime-State-Rollback,
  starke UObject-Roots, Replay-Cache-Invalidierung, Mutation-Epoch,
  FastArray-Dirtiness, Item-Subobject-Registrierung und die Reihenfolge der
  synchronen Removed-/Added-Nachrichten sind unverändert.
- `URpgInventoryManagerComponent` bleibt die einzige reflektierte und
  replizierte Inventory-Autorität. `InventoryList`, `InventoryRevision`,
  ReplicationPolicy, Replay-Caches, Mutation-Epoch, Reentrancy-Flags,
  Lifecycle und Subobject-Replikation wurden nicht in einen zweiten Manager,
  Handler oder ein UObject verschoben. Save-Daten bleiben validierte
  Rekonstruktionsdaten und keine Live-Gameplay-Wahrheit.
- Die Hauptimplementierung schrumpfte von 7.510 auf 6.906 Zeilen; der
  eigenständig kompilierbare Persistence-TU umfasst 662 Zeilen mit direkten
  Includes und eindeutig benannten privaten Helpers. Der bestehende
  `LogRpgInventoryManager`-Kanal bleibt über beide Dateien erhalten.
- Zwei neue Contract-Tests frieren die Grenze für die folgenden Unterschnitte
  ein: Der Manager bleibt `BlueprintType`, replizierte
  `UActorComponent` ohne eigene Net-RPCs; repräsentative
  AuthorityOnly-/Pure-/Deprecated-Funktionen und native-only Intents bleiben
  unverändert. Zusätzlich werden `COND_None`/`COND_Dynamic`, RepNotify-Namen,
  der FastArray-NetDelta-Trait sowie ItemInstance-Replikation und
  `ItemId`-SaveGame-Metadaten geprüft.
- Es wurden keine UFUNCTION-Signaturen, UPROPERTY-Namen, Save-DTOs,
  Enum-Ordnungen, Blueprint-Verträge oder Editor-Assets geändert. Im Editor
  sind kein Resave, kein CoreRedirect und keine manuelle Widget-/MVVM-
  Anpassung nötig. Nach offenem Hot Reload sollte der Editor vor weiterer QA
  vollständig neu gestartet werden.
- `SurvivalRpgEditor Win64 Development` wurde sowohl vollständig ohne Unity
  als auch mit echtem erzwungenem Unity gebaut: Non-Unity 408 von 408
  Actions, finaler adaptiver Exaktstand 55 von 55 Actions und finaler
  `-ForceUnity -DisableAdaptiveUnity`-Stand 6 von 6 Actions; jeweils
  `Result: Succeeded`.
- `SurvivalRpg.Inventory` meldete einschließlich der neuen Contracts
  145 von 145 und `SurvivalRpg.Save` 2 von 2 Automationtests erfolgreich.
- Bewusste Grenze: Standalone-Reflection- und Black-Box-Tests beweisen noch
  keinen echten FastArray-Transport, OwnerOnly versus ActorRelevant oder
  Late Join. Diese Netzwerk-PIE-Prüfung bleibt ebenso offen wie die
  Absicherung, dass `SetReplicationPolicy` nach begonnener
  Subobject-Replikation nicht mehr geändert werden darf.
- Der verbindliche Manager-Checklistenpunkt bleibt bewusst offen, bis auch
  Rules/Planner, Transactions und Storage physisch getrennt sowie ihre
  Abhängigkeitsrichtung geprüft sind. Fortschritt Phase 6 bleibt damit
  20 von 22 Punkten (90,9 %); Gesamtfortschritt bleibt 86 von 97 Punkten
  (88,7 %), 11 Punkte offen.
- Nächster Schnitt: Placement-, Capacity- und Graphregeln in einen
  eigenständigen read-only `RpgInventoryManagerRulesPlanner.cpp` verschieben,
  ohne Fragment-Staging fälschlich als nebenwirkungsfreie Query auszugeben.

Verifizierter Phase-6L2A-Zwischenstand vom 2026-07-24:

- Der Rules/Planner-TU besitzt jetzt den ersten vollständigen kanonischen
  Regelblock: Player-Layout-Auflösung, validierter Graphaufbau,
  Subtree-Indizes, Live-Graph-Validierung, Root-/ItemContainer-
  Größenauflösung, Containerdefinitionen, Placement-Graphregeln,
  Cycle-Erkennung und Single-Cell-Policy für Gear/Carry.
- 579 Member-Implementierungszeilen wurden verhaltensneutral aus der
  Component nach `RpgInventoryManagerRulesPlanner.cpp` verschoben. Die
  dateilokalen Container-, Depth-, Placement- und Scratch-Helfer besitzen
  eindeutige Rules-Präfixe; der TU ist sowohl eigenständig kompilierbar als
  auch Unity-sicher.
- Der querschnittliche `IsInventoryMutationLocked`-Vertrag bleibt bewusst in
  `RpgInventoryManagerComponent.cpp`. Ebenso verbleiben FastArray-Storage,
  Replikation, Replay/Epoch, Commit-Gates und sämtliche öffentliche API auf
  der Component. Der Rules-TU besitzt keinen eigenen Zustand und keine
  Mutationsautorität.
- Die Manager-Hauptimplementierung umfasst nun 6.327 statt ursprünglich
  7.510 Zeilen; der Rules/Planner-TU umfasst aktuell 680 Zeilen und wird im
  nächsten Unterschnitt um den Placement-/Capacity-Planer ergänzt.
- Der eigenständige adaptive Build kompilierte beide betroffenen TUs und
  endete mit 6 von 6 Actions erfolgreich. Der echte
  `-ForceUnity -DisableAdaptiveUnity`-Build endete mit 5 von 5 Actions und
  `Result: Succeeded`.
- `SurvivalRpg.Inventory` meldete erneut 145 von 145 Automationtests
  erfolgreich. Damit sind insbesondere Restore, Nested Container,
  Placement, Transfer, Pickup/Collect, Replay und die Manager-Contracts auf
  dem verschobenen Graphregelpfad grün.
- Es wurden weiterhin keine Reflection-, Blueprint-, Save-DTO- oder
  Editor-Asset-Verträge geändert; im Editor ist keine manuelle Anpassung
  nötig.
- Der Manager-Checklistenpunkt bleibt offen. Phase 6 bleibt bei 20 von
  22 Punkten (90,9 %), der Gesamtplan bei 86 von 97 Punkten (88,7 %).
- Nächster Schnitt: `EvaluatePlacement`, Definition-/Rotation-/Capacity-
  Regeln und die dazugehörigen CanAdd-/CanReceive-Preflights in denselben
  read-only Rules/Planner-TU verschieben.

Verifizierter Phase-6L2B-Zwischenstand vom 2026-07-24:

- Der Placement-/Capacity-Kern liegt jetzt im kanonischen
  `RpgInventoryManagerRulesPlanner.cpp`: `TryNormalizePlacementForDefinition`,
  `EvaluatePlacement`, der vollständige interne Evaluator, die read-only
  Capacity-Abfragen und die effektive Max-Stack-Regel. Insgesamt wurden 973
  Member-Implementierungszeilen aus der Manager-Hauptdatei verschoben.
- Der Evaluator deckt weiterhin Add, Move, Equip, Split, Transfer und Restore,
  Exact sowie FirstFit, Merge, Place, Swap und NoOp ab. Source-Snapshots,
  Graph-/Depth-/Containerregeln, Scratch-Occupancy, Entry-Kapazität und
  Revisionen werden unverändert gelesen; der Rules-TU besitzt weiterhin
  keinen eigenen Zustand und keine Commit-Autorität.
- Die kanonische Container-vor-Traits-Max-Stack-Policy besitzt jetzt genau
  eine Implementierung im Rules-TU. Der noch von Storage und Transactions
  benötigte dateilokale Component-Helfer delegiert auf diese statische
  Manager-Regel. Capacity-Setter, GAS-Attribut-Binding, Replikation,
  Mutation-Lock, Replay/Epoch und sämtliche Commit-Pfade verbleiben bewusst
  auf der Manager-Component.
- Ein unabhängiger Paritätscheck bestätigte die 934 Evaluatorzeilen nach den
  acht notwendigen, Unity-sicheren Rules-Helper-Umbenennungen bytegenau.
  Die verschobenen Capacity-Abfragen sind nach Whitespace-Normalisierung
  tokenidentisch; der Max-Stack-Callgraph ist azyklisch und behält Container,
  Traits sowie den Fallbackwert `1` unverändert bei.
- Der unbenutzte alte Container-Regel-Helper wurde entfernt. Beide
  Übersetzungseinheiten besitzen ihre direkten Includes; insbesondere
  `AbilitySystemComponent`, Spatial-/Traits-Typen und UObject-Globals werden
  nicht mehr durch Unity- oder Player-Includes verborgen.
- Die Manager-Hauptimplementierung umfasst nun 5.310 statt ursprünglich
  7.510 Zeilen. Der Rules/Planner-TU ist von 680 auf 1.790 Zeilen gewachsen.
  Alle reflektierten APIs, Properties, Save-DTOs und Enum-Ordnungen bleiben
  unverändert.
- `SurvivalRpgEditor Win64 Development` unter der zum Projekt gehörenden
  UE-5.8-Toolchain baute den finalen separaten Stand mit 4 von 4 Actions und
  den echten `-ForceUnity -DisableAdaptiveUnity`-Stand mit 10 von 10 Actions;
  beide endeten mit `Result: Succeeded`.
- `SurvivalRpg.Inventory.PlacementEvaluator` meldete 4 von 4 und der
  vollständige Filter `SurvivalRpg.Inventory` 145 von 145 Automationtests
  erfolgreich. Damit sind insbesondere Plan-Purity, alle Operationen,
  Rotation, Root-/ItemOwned-Handles, Subtree-Kapazität und Restore-Scratch
  auf dem verschobenen Pfad abgedeckt.
- Es wurden keine Editor-Assets oder Blueprint-Verträge geändert. Im Editor
  ist kein Resave und keine manuelle MVVM-/Widget-Anpassung nötig; nach einem
  noch offenen Hot-Reload-Prozess sollte vor manueller QA weiterhin ein
  vollständiger Editor-Neustart erfolgen.
- Der Manager-Checklistenpunkt bleibt bis zu den vollständigen
  Rules/Planner-, Transactions- und Storage-Schnitten offen. Phase 6 bleibt
  bei 20 von 22 Punkten (90,9 %), der Gesamtplan bei 86 von 97 Punkten
  (88,7 %), 11 Punkte sind offen.
- Nächster Schnitt: die öffentlichen CanAdd-/CanReceive- und
  RequiredNewEntryCount-Preflights samt ausschließlich benötigter
  Identitätsauflösung in den Rules-TU verschieben. Dabei soll ein gemeinsamer
  `PublicPreflightParityAndPurity`-Test die bisher fehlenden positiven
  Transfer-Preflights, wiederholte deterministische Planung, Revision,
  Mutation-Epoch und Ownership explizit einfrieren.

Verifizierter Phase-6L2C-Zwischenstand vom 2026-07-24:

- Sämtliche öffentlichen read-only Placement-Preflights liegen jetzt beim
  kanonischen Evaluator im Rules/Planner-TU: beide
  `GetRequiredNewEntryCount`-Varianten, CanAdd FirstFit/Exact, CanReceive
  FirstFit/Exact/IgnoringItem, die Single-Overlap-Abfrage sowie die
  actorweite Managed-/Identity-Prüfung.
- 368 Member-Implementierungszeilen und der ausschließlich von den
  Transfer-Preflights benötigte 35-zeilige Managed-Inventory-Helfer wurden
  aus der Manager-Hauptdatei verschoben. Der alte anonyme Helfer ist
  vollständig entfernt; die Rules-Version besitzt einen eindeutigen Namen
  und einen direkten `GameFramework/Actor.h`-Include.
- Der verhaltensneutrale Anteil wurde unabhängig gegen den vorherigen Stand
  geprüft: Alle zwölf Memberdefinitionen und der Helper sind bis auf genau
  zwei erwartete Rules-Helper-Umbenennungen und Zeilenumbruchformatierung
  identisch. Jede Definition existiert repo-weit genau einmal; Component-
  Helper und direkte Includes besitzen weiterhin reale Nutzer.
- Ein dabei gefundener echter Preflight-Fehler ist behoben:
  `CanReceiveTransferredItemInstanceToPlacementIgnoringItem` akzeptiert nur
  noch ein tatsächlich von einem anderen Inventory verwaltetes Incoming
  Item und lässt bei vollem Entry-Budget nur ein Zielitem als Ersatz gelten,
  dessen Placement den normalisierten Zielbereich wirklich überlappt. Ein
  beliebiges Item an anderer Stelle kann damit keine freie Kapazität mehr
  vortäuschen; der echte überlappende Replacement-Fall bleibt erlaubt.
- `PublicPreflightParityAndPurity` friert CanAdd und CanReceive für FirstFit
  und Exact gegen den gemeinsamen Evaluator ein, einschließlich Merge plus
  Rest-Placement, RequiredNewEntryCount, vollständiger Plan-Signatur und
  wiederholter Deterministik. Vor und nach allen Queries bleiben Source- und
  Target-Graph, Revision, Mutation-Epoch, Stackmengen, UObject-Outer,
  ItemIds, Definitionen, Runtime-Stack-Keys und Membership unverändert.
- `IgnoredNonOverlapDoesNotBypassCapacity` deckt den behobenen Safety-Fall
  separat ab: echter überlappender Ersatz bei vollem FixedEntries-Budget ist
  gültig; detached Incoming Items und nicht überlappende Ignored Items
  werden abgelehnt, ohne Source- oder Target-Graph zu verändern.
- Bewusste Purity-Grenze: Die Definition-Preflights erzeugen weiterhin
  transiente Instanzen und rufen `Fragment->OnInstanceCreated` auf. Die
  Tests beweisen deshalb Inventory-/Graph-State-Purity, nicht globale
  Nebenwirkungsfreiheit beliebiger Fragment-Hooks. Der als `BlueprintPure`
  exponierte `GetRequiredNewEntryCountForItemDefinition` bleibt wegen
  potenziellem UObject-/GUID-Churn ein dokumentierter Folgekandidat für eine
  staging-sichere Default-State-Factory.
- Die Manager-Hauptimplementierung umfasst jetzt 4.894 statt ursprünglich
  7.510 Zeilen; der Rules/Planner-TU umfasst 2.222 Zeilen. Header, UHT,
  UFUNCTION-Flags, Blueprint-Pins, Save-DTOs, Enum-Ordnungen und
  Editor-Assets blieben unverändert. Es sind keine CoreRedirects, Resaves
  oder manuellen MVVM-/Widget-Anpassungen nötig.
- Der separate UE-5.8-Build kompilierte Manager, Rules/Planner und die
  Testdatei eigenständig und endete mit 12 von 12 Actions erfolgreich. Der
  echte `-ForceUnity -DisableAdaptiveUnity`-Build endete mit 5 von 5 Actions;
  beide meldeten `Result: Succeeded`.
- `SurvivalRpg.Inventory.PlacementEvaluator` meldete nun 6 von 6 und der
  vollständige Filter `SurvivalRpg.Inventory` 147 von 147 Automationtests
  erfolgreich.
- Der Manager-Checklistenpunkt bleibt bis zum vollständigen Planner-,
  Transactions- und Storage-Schnitt offen. Phase 6 bleibt bei 20 von
  22 Punkten (90,9 %), der Gesamtplan bei 86 von 97 Punkten (88,7 %),
  11 Punkte sind offen.
- Nächster Rules-Schnitt: PlacementSubject-Factories,
  `BuildMoveMutationRequest`, `PlanMoveItem`, `PlanDropItem` und
  `PlanInventoryMutation` in den read-only Planner-TU verschieben; Execute-,
  Replay- und Commit-Pfade bleiben für den anschließenden Transactions-
  Schnitt auf der Manager-Autorität.

Verifizierter Phase-6L2D-Zwischenstand vom 2026-07-24:

- Der eigentliche Mutation-Planner ist nun ebenfalls im Rules/Planner-TU
  gebündelt: alle sechs `FRpgInventoryPlacementSubject`-Factories,
  subtree-sichere Removal-Deltas, Consume- und Legacy-Move-Previews,
  Move-/Equip-/Transfer-Request-Builder sowie PlanMove, PlanEquipment,
  PlanDrop und der vollständige lokale `PlanInventoryMutation`-Kernel.
- 16 Definitionen mit zusammen 638 Implementierungs-/Factory-Zeilen
  beziehungsweise 654 physischen Zeilen inklusive Trennern wurden aus der
  Manager-Hauptdatei verschoben. Der einzige neue private Rules-Helper baut
  abgelehnte Intent-Ergebnisse; die Source-Snapshot- und Placement-Vergleiche
  verwenden die bereits kanonischen Rules-Helper.
- Der unabhängige Paritätscheck bestätigte alle 16 Definitionen nach genau
  drei erwarteten Helper-Umbenennungen tokenidentisch zum vorherigen Stand.
  Jede Definition existiert repo-weit genau einmal. Die gleichartigen
  Component-Helper bleiben bewusst bestehen, weil Execute-, Transfer- und
  Replay-Pfade sie weiterhin benötigen.
- Der Planner schreibt weder FastArray/InventoryList noch Revision,
  Mutation-Epoch, Replay-Cache oder UObject-Ownership. Fremde Inventories
  werden nur synchron über vollständige Source-Snapshots gelesen; Authority,
  Same-World, Replay, Revalidation und Commit bleiben ausschließlich bei den
  Execute-/Transactions-Pfaden. Der Rules-TU ist Game-Thread-only und keine
  Async-Lifetime-Grenze.
- `PlacementSubject.FactoryContract` friert alle Felder der sechs Factories
  ein: Owned-Default-/Teilmenge, Incoming-Provenienz, Definition-only,
  Detached versus Generated sowie das StagedRestore-ItemId-Override.
- Zwei direkte Planner-Contracts ergänzen die vorher nur transitive
  Abdeckung: Move prüft Same-Cell-NoOp versus Rotate, Delta-Semantik und stale
  EntryId-/Placement-/Quantity-Snapshots; Drop prüft gewöhnliche
  Partial-Removal-Deltas sowie stale und übergroße Mengen. Beide beweisen
  unveränderten Graph, Revision und Mutation-Epoch.
- Bewusste Read-only-Grenze: By-Value-Planaufrufe erzeugen bei fehlender
  RequestId weiterhin lokal eine GUID. Insbesondere die als `BlueprintPure`
  exponierten CanConsume-/CanMove-Wrapper verwerfen diese GUID wieder. Das
  mutiert keinen Inventory-State, verursacht aber unnötigen GUID-Churn und
  macht Result-IDs ohne mitgelieferte RequestId nicht deterministisch. Ein
  ID-freier Planner-Core mit RequestId-Erzeugung ausschließlich an Command-
  Grenzen bleibt als gezielter Folgeschnitt dokumentiert.
- Die Manager-Hauptimplementierung umfasst jetzt 4.240 statt ursprünglich
  7.510 Zeilen; `RpgInventoryManagerRulesPlanner.cpp` umfasst 2.891 Zeilen.
  Der Rules/Planner-Domänenschnitt ist damit physisch vollständig; die
  öffentliche und replizierte Autorität bleibt weiterhin ausschließlich
  `URpgInventoryManagerComponent`.
- Der finale separate UE-5.8-Build kompilierte beide Produktionseinheiten
  und beide geänderten Testdateien eigenständig und endete mit 7 von 7
  Actions erfolgreich. Der echte `-ForceUnity -DisableAdaptiveUnity`-Build
  endete mit 5 von 5 Actions; beide meldeten `Result: Succeeded`.
- Die neuen gezielten Filter meldeten 2 von 2 Intent-Planner- und 1 von 1
  PlacementSubject-Test erfolgreich. Der vollständige Filter
  `SurvivalRpg.Inventory` meldete 150 von 150 Automationtests erfolgreich.
- Header, Reflection, Blueprint-/MVVM-Verträge, Save-DTOs und Editor-Assets
  blieben unverändert. Es sind keine CoreRedirects, Resaves oder manuellen
  Editor-Anpassungen nötig; nach offenem Live Coding sollte der Editor vor
  manueller QA vollständig neu gestartet werden.
- Der Manager-Checklistenpunkt bleibt offen, bis Transactions und Storage
  ebenfalls getrennt und der Gesamt-Facade-Schnitt geprüft sind. Phase 6
  bleibt bei 20 von 22 Punkten (90,9 %), der Gesamtplan bei 86 von
  97 Punkten (88,7 %), 11 Punkte sind offen.
- Nächster Domänenschnitt: autoritative Grant-/Bootstrap-/Removal- und
  Intent-Commitpfade in `RpgInventoryManagerTransactions.cpp` verschieben.
  Replikationslifecycle, Capacity-Binding und die schmale öffentliche
  Manager-Fassade bleiben in der Core-Datei; der große Cross-Inventory-
  Commit wird als eigener grüner Transactions-Unterschnitt behandelt.

Verifizierter Phase-6L2E1-Zwischenstand vom 2026-07-24:

- Der erste autoritative Transactions-Cluster ist physisch aus der
  Manager-Hauptdatei nach `RpgInventoryManagerTransactions.cpp` verschoben:
  Add-Plan-Commit, Grant, Bootstrap samt Preflight, Owned-Instance-Insert,
  sechs Legacy-Add-/Stack-Wrapper, Removal-Delta-Commit, beide
  Remove-Wrapper sowie die beiden öffentlichen Consume-Fassaden.
- 16 Definitionen mit zusammen 712 Implementierungszeilen wurden aus dem
  Core-TU verschoben. Gegen den vorherigen Stand sind sie bis auf die
  beabsichtigte, Unity-sichere Umbenennung des TU-lokalen Stacksize-Helpers
  unverändert. Der Helper liegt nun eindeutig in
  `RpgInventoryManagerTransactionsPrivate`; `Templates/Greater.h` ist für
  `TGreater<int32>` explizit eingebunden.
- Die Abhängigkeitsrichtung bleibt unverändert: Authority, Mutation-Lock,
  FastArray, Revision, replizierte Subobject-Registrierung und
  Fragment-Lifecycle gehören weiterhin derselben
  `URpgInventoryManagerComponent`-Instanz. Der neue TU ist nur eine
  Implementierungsgrenze und weder ein zweiter Manager noch ein neues
  UObject, Subsystem oder replizierter State-Owner.
- `BootstrapSameOwnerReuseAndPreflightPurity` deckt erstmals den erfolgreichen
  Same-Owner-Rebootstrap ab: wiederholte Preflights lassen Graph, Revision,
  Mutation-Epoch, UObject, ItemId, Outer, Definition und Runtime-State
  unverändert; der Commit verwendet dasselbe detached UObject und dieselbe
  persistente Identität wieder.
- `ConsumeFacadePreflightCommitParityAndPurity` deckt erstmals die öffentliche
  Consume-Preflight-Fassade funktional ab: gültige, übergroße, Null- und
  ungültige Mengen, read-only Preflights sowie ein partieller Commit mit
  vollständigem Result-, Delta-, Placement-, Identity- und
  Revisionsvertrag.
- Drei bereits vorher bestehende Risiken bleiben bewusst als Debt sichtbar:
  `CanBootstrapItemInstance` ist `BlueprintPure`, erzeugt für fremde detached
  Items aber ein transientes UObject samt GUID und ruft Fragment-Hooks auf;
  Pure bedeutet hier nur Inventory-State-Purity, nicht globale Hook-Purity.
  Zusätzliche Grant-Split-Instanzen führen Fragment-Hooks nach der
  Plan-/Revisionsprüfung ohne zweiten Revalidation-Gate aus. Außerdem
  broadcastet `CommitRemovalDeltas` den neuen Graph noch vor
  `MarkInventoryStateDirty`, sodass synchrone Listener kurz den neuen Graph
  mit der alten Revision sehen und reentrant mutieren können. Diese
  Semantiken wurden im mechanischen TU-Schnitt nicht nebenbei verändert.
- Die Manager-Hauptimplementierung umfasst jetzt 3.512 statt ursprünglich
  7.510 Zeilen. `RpgInventoryManagerTransactions.cpp` umfasst 752 und
  `RpgInventoryManagerRulesPlanner.cpp` weiterhin 2.891 Zeilen. Header,
  UHT/Reflection, Blueprint-/MVVM-Verträge, Save-DTOs und Editor-Assets
  blieben unverändert; es sind keine CoreRedirects, Resaves oder manuellen
  Editor-Anpassungen nötig.
- Der adaptive UE-5.8-Build kompilierte Core, Transactions und die geänderte
  Testdatei eigenständig und endete mit 8 von 8 Actions erfolgreich. Der
  echte `-ForceUnity -DisableAdaptiveUnity`-Build endete mit 6 von 6 Actions;
  beide meldeten `Result: Succeeded`.
- Die beiden neuen Tests meldeten einzeln jeweils 1 von 1, der vollständige
  Filter `SurvivalRpg.Inventory.Transaction` 18 von 18 und
  `SurvivalRpg.Inventory` 152 von 152 Automationtests erfolgreich.
- Der Manager-Checklistenpunkt bleibt offen, bis Intent-Execute/Replay,
  Cross-Inventory und Storage ebenfalls physisch getrennt und der
  Gesamt-Facade-Schnitt geprüft sind. Phase 6 bleibt bei 20 von 22 Punkten
  (90,9 %), der Gesamtplan bei 86 von 97 Punkten (88,7 %), 11 Punkte sind
  offen.
- Nächster Transactions-Schnitt: typed Move/Equip/Transfer/Drop-Fassaden,
  Replay-Cache und `ExecuteInventoryMutation` als zusammenhängenden
  Commit-Cluster in den Transactions-TU verschieben. Der mehr als tausend
  Zeilen große `ExecuteCrossInventoryTransfer`-Commit bleibt ein eigener
  grüner Unterschnitt, damit Fehlerursachen und Build-Gates eng bleiben.

Verifizierter Phase-6L2E2-Zwischenstand vom 2026-07-24:

- Der zweite autoritative Transactions-Cluster ist physisch aus der
  Manager-Hauptdatei nach `RpgInventoryManagerTransactions.cpp` verschoben:
  die typed Move-/Equipment-/Transfer-/Pickup-/Drop-Fassaden samt gemeinsamem
  Transfer-Einstieg, die drei Legacy-Sort-/Index-/Placement-Fassaden,
  Request-Äquivalenz, Single-Mutation-Replay und -Cache sowie
  `ExecuteInventoryMutation`.
- 13 Definitionen mit zusammen 490 fortlaufenden Clusterzeilen wurden aus dem
  Core-TU verschoben. Gegen den vorherigen Stand sind sie token-identisch bis
  auf genau sechs beabsichtigte, Unity-sichere Umbenennungen TU-lokaler
  Helper beziehungsweise Konstanten:
  `TransactionsMaxRecentMutationResults`,
  `MakeTransactionsRejectedIntentResult`,
  `IsTransactionsCompleteSourceSnapshot`,
  `IsTransactionsCompletelyUnsetPlacement`,
  `AreTransactionsPlacementSnapshotsExactlyEqual` und
  `IsTransactionsItemOwnedHandleDepthOverflow`. Die Namen liegen eindeutig in
  `RpgInventoryManagerTransactionsPrivate`; die vom verbleibenden
  Cross-Inventory-Commit weiterhin benötigten Core-Gegenstücke bleiben bis
  zum nächsten Schnitt bewusst bestehen.
- Die Autoritäts- und Ownership-Grenze bleibt unverändert:
  `URpgInventoryManagerComponent` ist weiterhin die einzige autoritative und
  replizierte Instanz. Typed Single-Inventory-Intents enden im
  Authority-/Mutation-Lock-Gate von `ExecuteInventoryMutation`; Transfer,
  Pickup und Drop delegieren ausschließlich an
  `ExecuteCrossInventoryTransfer`, das beide Inventories, beide Owner,
  Same-World, Operation und Partial-Policy erneut prüft. Der Transactions-TU
  ist nur eine Implementierungsgrenze und kein zweiter Manager, UObject,
  Subsystem oder State-Owner.
- `AuthorityAndLegacyFacadeRevisionContract` deckt die autorisierten
  Legacy-Sort-, Index- und Placement-Fassaden einschließlich exakter
  Revisionsfortschreibung und No-op-Verhalten ab. Dieselben Legacy-Fassaden,
  typed Move, trusted Equipment-Move, generischer Execute und Transfer werden
  ohne Authority atomar abgelehnt; Quell-/Zielgraph, Revision und
  Mutation-Epoch bleiben unverändert.
- `ForeignWorldTargetIsRejectedAtomically` deckt den Same-World-Vertrag der
  typed Transfer-Fassade ab. Ein fremder Ziel-World wird mit stabiler
  Request-Korrelation abgelehnt, der identische Retry liefert den gecachten
  Reject, und beide Graphen, Revisionen und Mutation-Epochen bleiben
  unverändert.
- Der mechanische TU-Schnitt behebt bewusst keine bereits vorhandene
  Reentrancy- oder Replay-Schuld. `ExecuteInventoryMutation` prüft bestehende
  Batch-/Cross-/Restore-Locks, setzt aber keinen eigenen
  Single-Mutation-Guard; insbesondere Split-Fragment-Hooks laufen nach der
  Planung ohne vollständiges Revalidation-Gate und können synchron
  reentrant mutieren. Auch Single-Inventory-Broadcast und Replay-Cache werden
  schwächer koordiniert als im beidseitig geguardeten Cross-Inventory-Commit.
- Zwei weitere bestehende Replay-Risiken bleiben dokumentiert: Wird das
  `TWeakObjectPtr`-Ziel eines Cache-Eintrags zerstört, entfernt der
  Epoch-/Identity-Check den Eintrag und dieselbe RequestId kann anschließend
  gegen ein neues Ziel wieder frisch ausgeführt werden. Außerdem begrenzen
  Single-Mutation- und Collect-Batch-Code denselben Cache über getrennte
  lokale 64er-Konstanten, die bei späteren Änderungen auseinanderlaufen
  können. Die in Phase 6L2E1 dokumentierten Grant-, Bootstrap-,
  Fragment-Hook- und Removal-Broadcast-Schulden bleiben ebenfalls offen.
- Die Manager-Hauptimplementierung umfasst jetzt 2.997 statt ursprünglich
  7.510 Zeilen. `RpgInventoryManagerTransactions.cpp` umfasst 1.310 und
  `RpgInventoryManagerRulesPlanner.cpp` weiterhin 2.891 Zeilen. Öffentliche
  API, Header, UHT/Reflection, Blueprint-/MVVM-Verträge und Editor-Assets
  blieben unverändert; es sind keine CoreRedirects, Resaves oder manuellen
  Editor-Anpassungen nötig.
- Die adaptiven UE-5.8-Builds endeten zunächst mit 9 von 9 und nach der
  finalen Testergänzung mit 6 von 6 Actions erfolgreich. Der echte
  `-ForceUnity -DisableAdaptiveUnity`-Build endete mit 5 von 5 Actions; alle
  drei Builds meldeten `Result: Succeeded`.
- Die beiden neuen Einzeltests meldeten jeweils 1 von 1 erfolgreich. Die
  vollständigen Filter `SurvivalRpg.Inventory.Intent`,
  `SurvivalRpg.Inventory.Transaction` und `SurvivalRpg.Inventory` meldeten
  20 von 20, 19 von 19 beziehungsweise 154 von 154 Automationtests
  erfolgreich.
- Der Manager-Checklistenpunkt bleibt offen, bis Cross-Inventory und Storage
  ebenfalls physisch getrennt und der Gesamt-Facade-Schnitt geprüft sind.
  Phase 6 bleibt bei 20 von 22 Punkten (90,9 %), der Gesamtplan bei 86 von
  97 Punkten (88,7 %), 11 Punkte sind offen.
- Nächster Transactions-Schnitt 6L2E3:
  `ExecuteCrossInventoryTransfer` als unteilbare, ungefähr 1.051 Zeilen große
  Funktion in den Transactions-TU verschieben. Lokale Prepare-Strukturen,
  Revalidation, beidseitige Guards und die No-fail-Commitregion bleiben für
  diesen mechanischen Schnitt zusammen; eine interne Zerlegung erfolgt nicht
  nebenbei.

Verifizierter Phase-6L2E3-Zwischenstand vom 2026-07-24:

- Der vollständige Cross-Inventory-Commit ist jetzt physisch aus der
  Manager-Hauptdatei nach `RpgInventoryManagerTransactions.cpp` verschoben.
  `ExecuteCrossInventoryTransfer` blieb dabei als geschlossene Funktion mit
  1.051 Definitionszeilen erhalten; Prepare-Strukturen, beidseitige Guards,
  Revalidation, No-fail-Commitregion und Notification-Reihenfolge wurden
  nicht nebenbei zerlegt.
- Gegen den vorherigen Stand ist die Funktion token-identisch bis auf genau
  fünf beabsichtigte, Unity-sichere Helper-Renames für Stackgröße,
  Unset-Placement, Placement-Gleichheit, Tiefenüberlauf und
  Runtime-State-Gleichheit. Der neue
  `AreTransactionsInventoryRuntimeStatesExactlyEqual`-Helper entspricht
  semantisch exakt dem vorherigen Core-Helper und liegt gemeinsam mit den
  bereits vorhandenen Gegenstücken eindeutig in
  `RpgInventoryManagerTransactionsPrivate`.
- Der Core-TU benötigt die vier Cross-spezifischen Helper
  `IsCompletelyUnsetPlacement`, `ArePlacementSnapshotsExactlyEqual`,
  `IsItemOwnedHandleDepthOverflow` und
  `AreInventoryRuntimeStatesExactlyEqual` nicht mehr. Ebenfalls vollständig
  nach Transactions verlagert beziehungsweise dort bereits vorhanden sind
  die direkten Abhängigkeiten `Templates/Greater.h` und
  `UObject/StrongObjectPtr.h`.
- `RpgInventoryFragment_ItemContainer.h` muss dagegen bewusst in beiden TUs
  eingebunden bleiben. Transactions benötigt den Fragmenttyp für Consume und
  Cross-Transfer; Core konstruiert weiterhin eine vollständige
  `FRpgInventoryItemContainerDefinition` in den verbleibenden
  FRpgInventoryList-Regeln. Der adaptive Build hat diese echte
  Include-Grenze sichtbar gemacht und damit die zunächst angenommene
  Core-Entfernung als falsch widerlegt.
- Die Architekturgrenze bleibt unverändert:
  `URpgInventoryManagerComponent` besitzt weiterhin allein InventoryList,
  Revision, ReplicationPolicy, Replay-Cache, Mutation-Epoch,
  Cross-Reentrancy-Flag und Subobject-Lifecycle. Der verschobene Commit
  arbeitet weiterhin direkt auf derselben autoritativen Component-Instanz;
  es entstand weder ein zweiter Manager noch ein neues UObject, Subsystem
  oder replizierter State-Owner.
- Der vollständige Paritäts- und Correctness-Audit blieb grün. Beide Owner
  müssen Authority besitzen und derselben World angehören; der Replay-
  Fingerprint bindet Request, konkretes Weak-Target und Partial-Policy.
  Beide Inventories bleiben bis nach dem Cache und allen Notifications
  geguardet. Source-/Target-Revision, Mutation-Epoch, Projected-Graph-
  Revalidation, FastArray-Dirtiness, Subobject-Abmeldung/-Registrierung sowie
  die tiefste-zuerst Source- und flachste-zuerst Target-Benachrichtigung
  entsprechen exakt dem vorherigen Verhalten.
- Der erste adaptive UE-5.8-Build schlug nachvollziehbar fehl, weil
  `RpgInventoryFragment_ItemContainer.h` zunächst irrtümlich aus Core
  entfernt worden war und dort der vollständige
  `FRpgInventoryItemContainerDefinition`-Typ fehlte. Transactions hatte in
  diesem Lauf bereits eigenständig erfolgreich kompiliert. Nach
  Wiederherstellung des notwendigen Includes endete der finale adaptive Build
  mit 4 von 4 Actions und `Result: Succeeded`.
- Der echte `-ForceUnity -DisableAdaptiveUnity`-Build endete mit 5 von 5
  Actions und `Result: Succeeded`. Der gezielte Filter
  `SurvivalRpg.Inventory.TransferDelta` meldete 5 von 5 und der vollständige
  Filter `SurvivalRpg.Inventory` 154 von 154 Automationtests erfolgreich.
- Die Manager-Hauptimplementierung umfasst jetzt 1.893 statt ursprünglich
  7.510 Zeilen. `RpgInventoryManagerTransactions.cpp` umfasst 2.385 und
  `RpgInventoryManagerRulesPlanner.cpp` weiterhin 2.891 Zeilen. Öffentliche
  API, Header, UHT/Reflection, Blueprint-/MVVM-Verträge und Editor-Assets
  blieben unverändert; es sind keine CoreRedirects, Resaves oder manuellen
  Editor-Anpassungen nötig.
- Der mechanische Schnitt behebt bewusst keine bereits vorhandene
  Cross-Inventory-Schuld. Fragment-Hooks können weiterhin externe
  Seiteneffekte oder Target-Lifetime-Änderungen auslösen, ohne dass
  Target-Validity, Authority und Same-World nach den Hooks vollständig neu
  geprüft werden. Zwei authority-fähige Owner ohne World bestehen weiterhin
  den bloßen Null-World-Gleichheitscheck; ein direkt mit Null-Target
  aufgerufener Legacy-Kernel liefert wegen der Prüf-Reihenfolge
  `AuthorityRequired`, während die typed Fassade `InvalidRequest` liefert.
- Ebenfalls offen bleiben die Wiederverwendung einer RequestId nach
  verschwundenem Weak-Target, die getrennt gepflegten 64er-Grenzen des
  gemeinsamen Replay-Caches, ein echter Netzwerk-PIE-Nachweis für
  FastArray-Transport, OwnerOnly versus ActorRelevant und Late Join sowie
  mögliche registrierte Subobject-Condition-Drift, wenn
  `SetReplicationPolicy` nach begonnenem Replication-Lifecycle geändert wird.
  Keine dieser Schulden wurde durch die physische Verschiebung als behoben
  markiert.
- Der Manager-Checklistenpunkt bleibt offen, bis Storage, Persistence und die
  verbleibende Fassade abschließend getrennt und gemeinsam verifiziert sind.
  Phase 6 bleibt bei 20 von 22 Punkten (90,9 %), der Gesamtplan bei 86 von
  97 Punkten (88,7 %), 11 Punkte sind offen.
- Nächster Manager-Schnitt: Storage-, Persistence- und Facade-Verantwortung
  der verbleibenden Hauptdatei prüfen und physisch auf die bereits
  etablierten Domänengrenzen reduzieren. Die replizierte
  `URpgInventoryManagerComponent` bleibt dabei weiterhin der einzige
  öffentliche und autoritative Einstieg.

Verifizierter Phase-6L2F-Zwischenstand vom 2026-07-25:

- Der Storage-Schnitt ist jetzt physisch aus der Manager-Hauptdatei nach
  `RpgInventoryManagerStorage.cpp` verschoben. Der mechanische Block umfasst
  1.438 Zeilen mit 46 Definitionen von `FRpgInventoryEntry` und
  `FRpgInventoryList` sowie acht TU-privaten Storage-Helpern.
- Die Architektur- und Ownership-Grenze bleibt unverändert:
  `URpgInventoryManagerComponent` ist weiterhin die einzige autoritative und
  replizierte Inventory-Instanz. Der neue Storage-TU arbeitet auf derselben
  Component und ist weder eine neue State-Owner-Klasse noch eine neue
  öffentliche API, ein UObject, Subsystem oder paralleler Manager.
- Der gemeinsam verwendete Stack-Changed-GameplayTag wird über
  `RpgInventoryManagerMessageTags.h` deklariert und genau einmal in
  `RpgInventoryManagerComponent.cpp` definiert. Core und Storage verwenden
  dadurch denselben Tag, ohne unter Unity eine zweite native
  Tag-Registrierung zu erzeugen.
- Die Manager-Hauptimplementierung umfasst jetzt 454 statt ursprünglich
  7.510 Zeilen. `RpgInventoryManagerStorage.cpp` umfasst 1.465,
  `RpgInventoryManagerRulesPlanner.cpp` 2.891,
  `RpgInventoryManagerTransactions.cpp` 2.385 und
  `RpgInventoryManagerPersistence.cpp` 662 Zeilen.
- Der adaptive UE-5.8-Build endete mit 7 von 7 Actions erfolgreich. Der echte
  `-ForceUnity -DisableAdaptiveUnity`-Build endete mit 5 von 5 Actions; beide
  meldeten `Result: Succeeded`.
- Der vollständige Filter `SurvivalRpg.Inventory` meldete 154 von 154 und
  `SurvivalRpg.Save` 2 von 2 Automationtests erfolgreich.
- Der Manager-Checklistenpunkt ist damit abgeschlossen: Storage,
  Rules/Planner, Transactions und Persistence besitzen klare physische
  Implementierungsgrenzen, ohne die öffentliche oder replizierte Autorität zu
  vervielfachen. Phase 6 steht bei 21 von 22 Punkten (95,5 %), der Gesamtplan
  bei 87 von 97 Punkten (89,7 %); 10 Punkte sind offen.
- Nächster Phase-6-Schnitt: die großen UI-Sammeldateien in eine Klasse pro
  Datei aufteilen und die bestehenden MVVM-, Presenter- und Widget-Verträge
  dabei unverändert halten.

## Phase 7 – Legacy endgültig entfernen

Status: **In Arbeit**

- [x] Asset-Registry-Referenzbericht für deprecated Klassen, APIs und Assets
      erzeugen.
- [ ] Blueprints resaven und notwendige Save-/CoreRedirect-Migrationen
      festhalten.
- [ ] Snapshot, Index-/Global-Sort, Compatibility-Includes und tote
      BlueprintCallable-Mutatoren entfernen.
- [x] Legacy-`CUI_Inventory` samt TileView-/SlotEntry-Pfad nach abgeschlossener
      Crafting-Migration sowie Referenz-, Cook- und
      Packaged-Verifikation löschen.
- [ ] Das referenzlose `CUI_AddressSlotEntry` nach Carry-Migration,
      Asset-Registry-, Cook- und Packaged-Prüfung entweder einem echten
      Consumer zuordnen oder löschen.
- [ ] Ungenutzte Spatial-Item-VM-Variablen, leere Setter-Graphwriter und den
      wirkungslosen Drag-State-Switch nach Asset-Resave entfernen.
- [ ] Verwaiste `_Old`-, Test- und Self-only-Assets nach Referenzprüfung
      entfernen. Der erste verifizierte Schnitt hat `CUI_Hotbar_Old`,
      `CUI_ActionBar_Old` und die oben dokumentierten Self-only-Rollback-Assets
      entfernt; weitere Kandidaten bleiben separat zu prüfen.
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

## Phase 8 – Abschlussauswertung und Editor-Handbuch

Status: **Vorgemerkt**

Diese Phase wird erst nach den Implementierungs- und Legacy-Phasen
abgeschlossen. Ihr Inhalt wird aus dem dann tatsächlich verifizierten
Repository-, Asset- und Build-Stand erzeugt, nicht aus veralteten
Zwischenannahmen.

- [ ] Eine vollständige Abschlussauswertung ergänzen: umgesetzte Änderungen,
      entfernte Legacy-Pfade, verbleibende bewusste Grenzen und die endgültigen
      Zuständigkeiten für Gameplay-Autorität, ViewModels, Presenter und Widgets
      nachvollziehbar nach Systembereichen zusammenfassen.
- [ ] Die erreichten Verbesserungen als Vorher-/Nachher-Auswertung festhalten:
      Korrektheit, Netzwerk- und Lifecycle-Sicherheit, Editor-Verständlichkeit,
      UI-Komposition, MVVM-Konsistenz, Wartbarkeit, Testabdeckung und bekannte
      Restrisiken konkret benennen.
- [ ] Eine ausführbare Editor-/Blueprint-Checkliste erstellen: notwendige
      Asset-Resaves, MVVM-Sources und Bindings, Widget- und Screen-Registry-
      Zuordnungen, Input-/Focus-/Drag-Konfiguration, zu löschende oder zu
      ersetzende Legacy-Assets sowie die abschließende Compile-, Cook-,
      Package- und Playtest-Reihenfolge in Pflicht-, Empfehlungs- und
      „keine manuelle Änderung nötig“-Punkte gliedern.
