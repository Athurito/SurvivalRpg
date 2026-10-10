# UI style guide

The shared dark-fantasy style for every screen and the HUD. The task sequence
is in [ui-roadmap.md](ui-roadmap.md). This guide lists the tokens, which asset
carries each one, and the rules for new widgets.

Assets live in `Content/SurvivalRpg/UI/Styles` (`/Game/SurvivalRpg/UI/Styles`)
and `Content/SurvivalRpg/UI/Fonts/Cinzel`.

## How a change spreads

| To change | Edit | Reaches |
| --- | --- | --- |
| A palette colour | Its vector parameter in `Palette/MPC_UI_Palette` | Every UI material instance that uses the role. It can also change at runtime through the world's parameter collection instance. |
| A font, size or text colour | The text style asset, for example `Text/CUI_TextStyle_Heading` | Every `CommonTextBlock` with that style |
| A button or border look | The button or border style asset, or the material instance it points to | Every widget with that style or brush |
| One widget's native colour default | The Widget Blueprint's class defaults | That widget only; see [Native colour defaults](#native-colour-defaults) |

Text styles cannot read the parameter collection. A text colour is therefore
set once per text style, using the palette value from the table below.

## Palette

`MPC_UI_Palette` holds one vector parameter per role. Materials pick a role by
its number through `MF_UI_PaletteRole`; material instances choose roles, never
colours. The six base colours come from the skill screen concept
([ui-roadmap.md](ui-roadmap.md#skill-screen-concept-ui-01b)); the rest are
derived from them.

| Role | Name | sRGB | Linear value | Use |
| --- | --- | --- | --- | --- |
| 0 | PanelBase | `#101214`, alpha 0.92 | 0.0052, 0.0060, 0.0070 | Charcoal background of screens |
| 1 | PanelInset | `#1B1D20`, alpha 0.94 | 0.0110, 0.0123, 0.0144 | Matte iron: panels, slots, rows, bar tracks |
| 2 | Outline | `#34363A` | 0.0343, 0.0369, 0.0423 | Iron edges |
| 3 | Accent | `#AD915A` | 0.4179, 0.2831, 0.1022 | Matte old gold: learned, selection, accent text |
| 4 | AccentBright | `#D9B979` | 0.6939, 0.4851, 0.1912 | Focus, hovered labels |
| 5 | AccentDim | `#5C4C2E` | 0.1070, 0.0723, 0.0273 | Primary button fill, dividers, tooltip edges |
| 6 | TextPrimary | `#DDD4C2` | 0.7231, 0.6584, 0.5395 | Bone white: main text |
| 7 | TextMuted | `#A39D92` | 0.3663, 0.3372, 0.2874 | Warm ash: secondary text |
| 8 | TextDisabled | `#625D55` | 0.1221, 0.1095, 0.0908 | Disabled text; neutral fill of tinted frames |
| 9 | Blood | `#824747` | 0.2232, 0.0630, 0.0630 | Muted blood red: exclusions, warnings, danger |
| 10 | Valid | `#6E8A55` | 0.1559, 0.2542, 0.0908 | Valid drop, enough resources |
| 11 | Pending | `#B0803F` | 0.4342, 0.2159, 0.0497 | Waiting, load |
| 12 | Health | `#7A1E19` | 0.1946, 0.0130, 0.0097 | Health bar |
| 13 | Stamina | `#6F7A3A` | 0.1590, 0.1946, 0.0423 | Stamina bar |
| 14 | Mana | `#2E3F7A` | 0.0273, 0.0497, 0.1946 | Mana bar |
| 15 | XP | `#AD915A` | 0.4179, 0.2831, 0.1022 | Experience bars |
| 16 | RarityCommon | — | 0.55, 0.53, 0.50 | Item frames |
| 17 | RarityUncommon | — | 0.16, 0.55, 0.20 | Item frames |
| 18 | RarityRare | — | 0.16, 0.33, 0.80 | Item frames |
| 19 | RarityEpic | — | 0.48, 0.18, 0.72 | Item frames |

The rarity roles are darker frame colours; the rarity rings in equipment and
weapon slots use them. The item text colours stay in
`RpgInventoryItemizationFragmentViewModel.cpp`.

## Typography

Headings use `Fonts/Cinzel/Font_Cinzel` (typefaces Regular, Bold and Black);
everything else uses the engine's Roboto. Text styles are in `Text/`.

| Style | Font | Size | Colour | Use |
| --- | --- | --- | --- | --- |
| `CUI_TextStyle_Title` | Cinzel Bold, spacing 80, shadow | 30 | TextPrimary | Screen and tree titles |
| `CUI_TextStyle_Heading` | Cinzel Bold, spacing 40, shadow | 22 | TextPrimary | Section titles |
| `CUI_TextStyle_Subheading` | Cinzel Regular, spacing 30, shadow | 18 | TextPrimary | Selected item names, tab labels |
| `CUI_TextStyle_Body` | Roboto Regular | 16 | TextPrimary | Body text; editor template |
| `CUI_TextStyle_BodyMuted` | Roboto Regular | 16 | TextMuted | Secondary lines |
| `CUI_TextStyle_BodyStrong` | Roboto Bold | 16 | TextPrimary | Row names |
| `CUI_TextStyle_Label` | Roboto Regular | 14 | TextMuted | Descriptions, row values |
| `CUI_TextStyle_Caption` | Roboto Regular | 13 | TextMuted | Small notes |
| `CUI_TextStyle_Hint` | Roboto Italic | 13 | TextMuted | Input hints |
| `CUI_TextStyle_Number` | Roboto Bold | 16 | TextPrimary | Counts and values |
| `CUI_TextStyle_Accent` | Roboto Bold | 18 | Accent | Points, highlights |
| `CUI_TextStyle_AccentSmall` | Roboto Regular | 13 | Accent | Small highlights |
| `CUI_TextStyle_ButtonLabel` | Roboto Bold, spacing 20 | 15 | TextPrimary | Button text |
| `CUI_TextStyle_ButtonLabelHovered` | Roboto Bold, spacing 20 | 15 | AccentBright | Hovered, focused and selected button text |
| `CUI_TextStyle_ButtonLabelDisabled` | Roboto Bold, spacing 20 | 15 | TextDisabled | Disabled button text |
| `CUI_TextStyle_TileLabel` | Roboto Bold, shadow | 13 | TextPrimary | Names inside fixed tiles (skill nodes, slots) |
| `CUI_TextStyle_TileCaption` | Roboto Regular, shadow | 11 | TextMuted | Status lines inside fixed tiles |
| `CUI_TextStyle_TileCaptionBone`, `_TileCaptionGold`, `_TileCaptionBlood` | Roboto Regular, shadow | 11 | TextPrimary, Accent, Blood | Status lines whose colour carries a state (set with `CommonText SetStyle`) |
| `CUI_TextStyle_NodeName` | Cinzel Bold, spacing 10, shadow | 12 | TextPrimary | Skill node names |
| `CUI_TextStyle_StateLabel` | Cinzel Bold, spacing 120, shadow | 18 | Accent | State labels such as LEARNED |
| `CUI_TextStyle_KeyBadge` | Cinzel Bold, shadow | 16 | TextPrimary | Key badges (Q, E, R) |
| `CUI_TextStyle_SectionTitle` | Cinzel Bold, spacing 100, shadow | 15 | TextPrimary | Container titles such as POCKETS and BACKPACK |
| `CUI_TextStyle_SlotCaption` | Cinzel Regular, spacing 140, shadow | 11 | TextMuted | Captions above equipment slots |
| `CUI_TextStyle_StackCount` | Roboto Bold, shadow | 11 | TextPrimary | Stack counts on inventory items |

## Spacing and sizes

- **Spacing steps:** 4, 8, 12, 16, 24, 32, 48 Slate units.
- **Layout:** panel padding 16, column gap 24, screen margin 48.
- **Corners:** 6 for panels, 4 for controls and slots, 2–3 for cells and bars.
  The materials take radii in screen pixels.
- **Edges:** 1 for outlines, 1.5 for selection, 2 for focus.
- **Inventory cells:** 56 with 2 padding, a 58 stride. The class defaults of
  `CUI_SpatialInventoryGrid` set them; no host overrides them, and no
  inventory sits in a scale box, so every grid shows the same cell size.

## Materials

Two master materials in `Materials/`, both User Interface domain and
translucent. They draw a rounded rectangle from a signed distance field in
screen pixels. They multiply by the widget's vertex colour, so a brush tint,
`SetBackgroundColor` or `SetBrushColor` still colours them.

`M_UI_Panel` draws panels, frames, slots and buttons:
- **Shape:** `Radius`, `EdgeWidth`, `EdgeOpacity`, and `InnerGlow`, a band of
  the edge colour glowing inward, in pixels.
- **Fill:** `FillOpacity`, `GradientStrength` (lighter top),
  `VignetteStrength` and `VignetteRadius` (darker, more opaque corners).
- **Grain:** `NoiseStrength`, `NoiseScale`, `NoiseScaleCoarse` and
  `NoiseTexture`, an additive grime grain.
- **Palette:** `FillRole` and `EdgeRole`.

Since UI-01b, `M_UI_Panel` also has:
- `Chamfer`: corner cut as a fraction of the shorter side. 0.29 draws an
  octagon, 0.5 a diamond.
- `TintFillAmount`: how much the widget tint colours the fill. The edge and
  glow always take the tint. A low value lets a state colour show only in the
  frame, as on the skill plaques.

`M_UI_ResourceBar` draws bar fills:
- **Shape:** `Radius`, `EdgeWidth`, `EdgeOpacity` (darkened inner edge).
- **Fill:** `FillOpacity`, `GradientStrength`, `SheenStrength` (a static
  highlight band).
- **Grain:** `NoiseStrength`.
- **Segments:** `SegmentCount`, `SegmentOpacity`.
- **Palette:** `ColorRole`.

A bar is a normal `ProgressBar`. Its background image is
`MI_UI_ResourceBar_Background` and its fill image the bar instance. View models
keep binding only `Percent`. Set `FillColorAndOpacity` to white so the
material's colour shows.

| Instance | Parent | Use |
| --- | --- | --- |
| `MI_UI_Panel_Default` | Panel | Screen panels: grain, vignette, inner bevel |
| `MI_UI_Panel_Inset` | Panel | Sub-panels, list rows |
| `MI_UI_Panel_Tooltip` | Panel | Tooltips |
| `MI_UI_Divider_Accent` | Panel | Thin divider lines |
| `MI_UI_Frame_Cell` | Panel | Inventory grid cells |
| `MI_UI_Frame_Slot`, `_SlotHovered`, `_SlotSelected`, `_SlotFocused` | Panel | Equipment and action slots per state |
| `MI_UI_Frame_Button_Normal`, `_Hovered`, `_Pressed`, `_Disabled` | Panel | Default buttons |
| `MI_UI_Frame_Button_Primary`, `_PrimaryHovered` | Panel | Primary buttons |
| `MI_UI_Frame_Button_Danger` | Panel | Destructive buttons (reset, drop) |
| `MI_UI_Frame_Tinted`, `_TintedHovered`, `_TintedPressed` | Panel | Frames whose state colour comes from the widget, such as skill nodes |
| `MI_UI_Frame_Ring` | Panel | Edge-only ring for selection frames; the widget tints it |
| `MI_UI_ResourceBar_Background` | Panel | Bar track |
| `MI_UI_ResourceBar_Health`, `_Stamina`, `_Mana`, `_XP`, `_Load` | Resource bar | Bar fills |
| `MI_UI_ResourceBar_Ticks` | Resource bar | Segment ticks only, as an overlay above a bar |

| `MI_UI_Plaque`, `_PlaqueHovered`, `_PlaquePressed` | Panel | Octagonal skill plaques; the widget's state colour tints frame and glow |
| `MI_UI_PlaqueRing` | Panel | Outer selection ring around a plaque |
| `MI_UI_Frame_Row` | Panel | List rows; dim iron edge, gold when the widget tints it as selected |
| `MI_UI_Glyph_Lock`, `_Forbidden`, `_Diamond` | Glyph | Lock, forbidden sign and diamond badges |
| `MI_UI_TabMarker`, `_TabMarkerDim` | Glyph | Underline marker with a diamond under active and hovered tabs |
| `MI_UI_GearSlotFill` | Panel | Chamfered iron fill inside equipment and weapon slot frames |
| `MI_UI_GridCellFill` | Panel | Iron fill of an inventory grid cell under its frame |
| `MI_UI_ItemFill` | Panel | Lighter plate behind an item that occupies cells |
| `MI_UI_SlotActive` | Panel | Gold glow of the weapon or off-hand item in hand |
| `MI_UI_SlotStateRing` | Panel | Edge-only ring a slot tints for focus, valid and invalid drops |
| `MI_UI_RarityRing_Uncommon`, `_Rare`, `_Epic` | Panel | Rarity ring inside equipment and weapon slots; edge and inner glow in the palette rarity role (UI-03) |

`M_UI_Glyph` draws small symbols from signed distances:
- **`GlyphType`:** 0 lock, 1 forbidden sign, 2 diamond, 3 tab marker.
- **Other parameters:** `Thickness` and `ColorRole`.

Brushes that use these materials draw as `Image` with a 32 × 32 image size.
The shape adapts to any widget size, so no 9-slice margin is needed.

## Art set

`Content/SurvivalRpg/UI/Art` holds the user's art set, in one engraving
style:
- `Icons/`: skills, professions, categories, crafting and navigation, and
  `Icons/gear` with the equipment glyphs (UI-02). UI-02b adds `Icons/items`
  (item icons), `Icons/building` (buildables and storage upgrades),
  `Icons/stats` (character values, used by the stats column), and crafting
  categories and stations in `Icons/crafting` (used by the crafting screen
  since UI-04).
- `Frames/`: panels, tooltip, slots and controls, and the gear frames
  `T_UI_Gear_Frame_Cell`, `_Equipment`, `_Weapon` and `_Container`.
- `Ornaments/` (including the paper doll `T_UI_Gear_BodySilhouette`) and
  `Backgrounds/`.

Rules:
- **Box and border brushes draw texture margins in texture pixels.** Slate
  sizes the margins of a texture brush drawn as `Box` or `Border` from the
  texture's pixel size; the brush's image size does not scale them (it only
  does for material brushes). A frame texture is therefore exported at the
  size it should appear on screen:
  - grid cell frame 64 × 64, margin 0.16 (10 px corners);
  - equipment slot frame 80 × 104, margin 0.16 × 0.123;
  - weapon slot frame 300 × 64, margin 0.034 × 0.16;
  - container frame 110 × 147, margin 0.16 × 0.12.
- **Equipment glyphs** mark empty slots: bone white at about 24 % opacity, set
  per slot through the `EmptyGlyphTexture` variable of `CUI_GearSlot` and
  `CUI_CarrySlot`.
- **Icons** are engravings with transparency; do not draw extra frames into
  an icon.
  - UI icons (skills, gear glyphs, building, stats, crafting, navigation) are
    bone white. Tint them per state through the image colour.
  - Item icons (`Icons/items`, since UI-02c) carry material colours, so items
    stay distinguishable in a full container. Draw them untinted (white); only
    dim them, for example for an unavailable recipe.
- **Item icons have the shape of the item's footprint:** 1:1 for 1 × 1 and
  2 × 2, 1:2 for 1 × 2, 1:3 for 1 × 3, 2:3 for 2 × 3. The grid stretches an
  icon over its footprint, so a new item with a different shape needs matching
  art or a matching footprint. Item icons are 512 px on the long edge and are
  imported with mips (`SimpleAverage`), because they are drawn far smaller.
- **Fixed icon boxes keep the aspect ratio.** An item icon in a slot, the
  quickbar, the quick access wheel or a crafting entry sits in a `ScaleBox`
  (`ScaleToFit`, named `IconFit` where it was added), and its brush takes the
  texture size (`bMatchSize`). A plain `Image` in a fill slot would squash a
  portrait icon into the box.
- **Weapon slots turn long weapons:** `bTurnPortraitIcons` on `CUI_CarrySlot`
  turns a portrait icon a quarter turn so a sword fills the wide weapon frame.
  Weapon I and II set it; the off hand keeps icons upright.
- **Panel frames:** use a `Border` whose background is
  `T_UI_Frame_PanelWide`, drawn as `Box` with margin 0.074 × 0.11. The
  texture is 969 × 654, so the ornamented corners are about 72 px.
  - Lay the frame over an iron fill (`MI_UI_Panel_Inset`).
  - Use a `Border`, not an `Image`: an image's brush size would force a
    minimum panel size.
  - The textures are trimmed to their band, so the frame sits at the panel
    edge.
- **Slot frames** (`T_UI_Slot_*`) stay square images.
- **`CUI_BorderStyle_MenuBackdrop`** draws the darkened night forest behind
  the game menu.

## CommonUI styles

| Asset | Brushes | Text |
| --- | --- | --- |
| `Buttons/CUI_ButtonStyle_Default` | Button normal, hovered, pressed, disabled; selected uses the slot selection and focus frames | ButtonLabel, hovered label |
| `Buttons/CUI_ButtonStyle_Primary` | Primary frames | ButtonLabel, hovered label |
| `Buttons/CUI_ButtonStyle_Danger` | Danger frame, default hover | ButtonLabel, hovered label |
| `Buttons/CUI_ButtonStyle_Slot` | Slot frames, no padding, at least 48 × 48 | TileLabel |
| `Buttons/CUI_ButtonStyle_Tab` | No normal brush; hover and selection frames | Subheading, hovered label |
| `Buttons/CUI_ButtonStyle_ListRow` | The Tab brushes with 12 × 6 padding, for selectable list rows | ButtonLabel, hovered label |
| `Buttons/CUI_ButtonStyle_GearSlot` | Equipment slot frame, dim at rest and bright on hover or focus, 5 px padding | — |
| `Buttons/CUI_ButtonStyle_WeaponSlot` | Weapon slot frame, the same states | — |
| `Borders/CUI_BorderStyle_Panel`, `_Inset`, `_Tooltip` | The matching panel instance | — |

`Config/DefaultEditor.ini` makes Body, Default and Panel the CommonUI template
styles for newly placed widgets.

## Rules for new widgets

- Use `CommonTextBlock` with a text style; do not type fonts or colours into a
  text block.
- Use the shared brushes or styles for panels, slots and buttons. Do not use
  KnightsQuest or the `fantasy_gui_4` frames in new work.
- Character values (level, experience, health, stamina, armour, load) come from
  `URpgCharacterStatsViewModel`. Add it with the Resolver creation type, which
  hands every widget the local player's shared instance; bind its text and
  progress fields directly.
- Body text is at least 13 pt. Only labels inside fixed tiles may go down to
  11 pt.
- **Filter tabs** use the menu tab styles (`CUI_TabButtonNormalStyle`,
  `_SelectedStyle`): ash at rest, gold with the diamond marker when active.
  - Tabs that tint their label and icon on selection repeat that on
    Construct, because a text style resets the colour when the widget is
    built.
  - For radio behaviour, clear the other tabs with `ClearSelection`;
    `SetIsSelected(false)` only deselects a toggleable button.
- **Category trees** (crafting) are a list of `CUI_ButtonStyle_ListRow` rows:
  - "All" and groups use the strong body style; subcategories are indented
    and plain.
  - Each row ends in a caption count; groups also show "-" or "+" for
    expanded or collapsed. Do not use font-dependent arrow glyphs; Roboto lacks
    "▾".
  - The screen mirrors the active filter into the list selection, so the
    chosen row keeps the gold frame.
- **Section headers inside lists** (tier sections) are a gold tile caption
  followed by a faint gold rule, with space above. They are neither selectable
  nor navigable.
- **Storage choices** in a dropdown use up to three lines: the name with its
  assignments, a caption with free cells, and a caption with the contents. An
  accent tag on the right marks the chest that automatic storing fills next.
- **Dropdowns** are a wide action button that names the current choice, over
  an inline popup list of list rows (dark inset, 4-unit padding). Picking an
  option or opening another dropdown closes the popup. Do not use
  `ComboBoxString`, whose popup sits outside CommonUI input routing.
- **Aligned panels:** panels side by side share their top and bottom edges.
  Give titles above them the same height, and fill a missing bar below with a
  spacer of its height.
- **Station screens** show only the station and use the full width, in four
  columns:
  - categories;
  - recipes;
  - the preview, with a large icon next to the key values;
  - production, with materials, target, quantity, plan and the start button.

  A framed order strip sits below the panel: label and title on the left,
  status with counts, a progress bar in the stats bar style and a hint in the
  middle, Pause and Stop remaining on the right. Long titles end in an
  ellipsis. Material counts and output targets are connected chests; do not
  embed the player inventory.
- **Key values** (`CUI_CraftingDetailRow`) put a caption label on the left
  and a right-aligned strong value on the right; emphasized values, such as
  damage, use the accent style.
- **Panel headers** use the section title with a gold count, and put their
  actions on the right.
- **Tables** (such as crafting requirements) use caption headers and fixed
  right-aligned number columns of the same width in header and rows, with a
  faint gold rule under each row.
- **Availability states** keep the row readable instead of hiding it:
  - available: full icon and bone text;
  - blocked (for example missing materials): icon dimmed to about 60 % and a
    blood-red caption or, in one-line rows, a blood-red "!";
  - locked: icon at about 30 %, the lock glyph (`MI_UI_Glyph_Lock`) on the
    icon and an ash caption.
- **Search fields** draw on `MI_UI_Panel_Inset` with 14 pt Roboto in bone white
  and a hint text.
- **Button labels** of `URpgCraftingActionButtonWidget` take the button
  style's text styles, so a disabled button also dims its label.
- A scroll box inside a scale box needs a capped height; otherwise the scale box
  shrinks the whole scroll content.
- **Controller readiness:**
  - `RenderFocusRule=Never` hides Slate's focus rectangle. Every focusable
    control needs a visible focus state.
  - CommonUI buttons show their hovered brush on gamepad focus. A plain
    `UButton` shows nothing; prefer `UCommonButtonBase` for new buttons.
  - No action may be mouse-only. Give right-click actions a CommonUI input
    action, so the action bar shows them with gamepad icons.
  - Show tooltips on focus too.
  - Scroll boxes scroll the focused widget into view.
  - Interactive targets are at least 48 units.
  - Windows starts with mouse and keyboard (`DefaultInputType` in
    `Config/DefaultGame.ini`), so glyphs, the cursor and hover selection are
    right from the first frame. CommonUI switches to gamepad glyphs and focus
    on the first gamepad input and back on mouse or keyboard input. Never rely
    on the input type at start; follow `OnInputMethodChanged` as the settings
    keybinding page does.

## Native colour defaults

These native widgets declare colour defaults as `UPROPERTY` values. A Widget
Blueprint that adopts the style overrides them in its class defaults with
palette values:
- `RpgSkillTreeGridWidget.h` link and locked-row colours: overridden in
  `CUI_Skills` (UI-01).
- `RpgInventorySpatialGridWidget.h`, `RpgInventorySpatialCellWidget.h`,
  `RpgInventorySpatialItemWidget.h`, `RpgInventoryDragVisualWidget.h`:
  overridden in `CUI_SpatialInventoryGrid`, `_Cell`, `_Item` and
  `CUI_InventoryDragVisual` (UI-02). The drag visual's layout follows in
  UI-06.
- `RpgStorageInventoryWidget.h` title colours: overridden in
  `CUI_StorageSpatial` (UI-02).
- `RpgInventoryCarrySlotWidget.h` `StateIndicatorOpacity`: 1 in
  `CUI_CarrySlot`, because `MI_UI_SlotActive` carries its own opacity (UI-02).
- `RpgInventoryItemTooltipWidget.h`, `RpgInventoryFeedbackToastWidget.h`:
  UI-06.
- `RpgQuickAccessRadialWidget.h`: UI-05.
- `RpgCraftingIngredientEntryWidget.h` `EnoughCountColor` and
  `MissingCountColor`: bone white and blood red in
  `CUI_CraftingIngredientEntrySpatial` (UI-04).

The tooltip colours in `RpgInventoryItemTooltipWidget.cpp` are still fixed in
native code (UI-06).
