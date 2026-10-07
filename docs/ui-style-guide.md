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
colours. Values are linear RGBA starting points and may be tuned.

| Role | Name | Value | Use |
| --- | --- | --- | --- |
| 0 | PanelBase | 0.006, 0.005, 0.005, 0.88 | Screen panels, buttons |
| 1 | PanelInset | 0, 0, 0, 0.6 | Slots, cells, list rows, bar tracks |
| 2 | Outline | 0.12, 0.11, 0.10 | Iron outlines; neutral fill of tinted frames |
| 3 | Accent | 0.62, 0.45, 0.18 | Tarnished gold: hover, selection, learned links |
| 4 | AccentBright | 0.85, 0.64, 0.28 | Focus, highlights, accent text |
| 5 | AccentDim | 0.30, 0.22, 0.10 | Primary button fill, dividers, tooltip edges |
| 6 | TextPrimary | 0.80, 0.76, 0.68 | Bone: body text |
| 7 | TextMuted | 0.50, 0.48, 0.45 | Ash: labels, captions |
| 8 | TextDisabled | 0.28, 0.27, 0.26 | Disabled text |
| 9 | Blood | 0.55, 0.08, 0.05 | Danger, invalid |
| 10 | Valid | 0.25, 0.50, 0.22 | Valid drop, enough resources |
| 11 | Pending | 0.75, 0.42, 0.12 | Waiting, warnings, load |
| 12 | Health | 0.42, 0.03, 0.02 | Health bar |
| 13 | Stamina | 0.33, 0.40, 0.14 | Stamina bar |
| 14 | Mana | 0.10, 0.14, 0.45 | Mana bar |
| 15 | XP | 0.55, 0.40, 0.14 | Experience bars |
| 16 | RarityCommon | 0.55, 0.53, 0.50 | Item frames |
| 17 | RarityUncommon | 0.16, 0.55, 0.20 | Item frames |
| 18 | RarityRare | 0.16, 0.33, 0.80 | Item frames |
| 19 | RarityEpic | 0.48, 0.18, 0.72 | Item frames |

The rarity roles are darker frame colours. The item text colours stay in
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
| `CUI_TextStyle_Accent` | Roboto Bold | 18 | AccentBright | Points, highlights |
| `CUI_TextStyle_AccentSmall` | Roboto Regular | 13 | AccentBright | Small highlights |
| `CUI_TextStyle_ButtonLabel` | Roboto Bold, spacing 20 | 15 | TextPrimary | Button text |
| `CUI_TextStyle_ButtonLabelHovered` | Roboto Bold, spacing 20 | 15 | AccentBright | Hovered, focused and selected button text |
| `CUI_TextStyle_ButtonLabelDisabled` | Roboto Bold, spacing 20 | 15 | TextDisabled | Disabled button text |
| `CUI_TextStyle_TileLabel` | Roboto Bold, shadow | 13 | TextPrimary | Names inside fixed tiles (skill nodes, slots) |
| `CUI_TextStyle_TileCaption` | Roboto Regular, shadow | 11 | TextMuted | Status lines inside fixed tiles |

## Spacing and sizes

- **Spacing steps:** 4, 8, 12, 16, 24, 32, 48 Slate units.
- **Layout:** panel padding 16, column gap 24, screen margin 48.
- **Corners:** 6 for panels, 4 for controls and slots, 2–3 for cells and bars.
  The materials take radii in screen pixels.
- **Edges:** 1 for outlines, 1.5 for selection, 2 for focus.
- **Inventory cells:** 56 with 2 padding, a 58 stride (decided in UI-02 after
  measuring the grids).

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

Brushes that use these materials draw as `Image` with a 32 × 32 image size.
The shape adapts to any widget size, so no 9-slice margin is needed.

## CommonUI styles

| Asset | Brushes | Text |
| --- | --- | --- |
| `Buttons/CUI_ButtonStyle_Default` | Button normal, hovered, pressed, disabled; selected uses the slot selection and focus frames | ButtonLabel, hovered label |
| `Buttons/CUI_ButtonStyle_Primary` | Primary frames | ButtonLabel, hovered label |
| `Buttons/CUI_ButtonStyle_Danger` | Danger frame, default hover | ButtonLabel, hovered label |
| `Buttons/CUI_ButtonStyle_Slot` | Slot frames, no padding, at least 48 × 48 | TileLabel |
| `Buttons/CUI_ButtonStyle_Tab` | No normal brush; hover and selection frames | Subheading, hovered label |
| `Borders/CUI_BorderStyle_Panel`, `_Inset`, `_Tooltip` | The matching panel instance | — |

`Config/DefaultEditor.ini` makes Body, Default and Panel the CommonUI template
styles for newly placed widgets.

## Rules for new widgets

- Use `CommonTextBlock` with a text style; do not type fonts or colours into a
  text block.
- Use the shared brushes or styles for panels, slots and buttons. Do not use
  KnightsQuest or the `fantasy_gui_4` frames in new work.
- Body text is at least 13 pt. Only labels inside fixed tiles may go down to
  11 pt.
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

## Native colour defaults

These native widgets declare colour defaults as `UPROPERTY` values. A Widget
Blueprint that adopts the style overrides them in its class defaults with
palette values:
- `RpgSkillTreeGridWidget.h` link and locked-row colours: overridden in
  `CUI_Skills` (UI-01).
- `RpgInventorySpatialGridWidget.h`, `RpgInventorySpatialCellWidget.h`,
  `RpgInventorySpatialItemWidget.h`, `RpgInventoryDragVisualWidget.h`: UI-02.
- `RpgStorageInventoryWidget.h` title colours: UI-02.
- `RpgInventoryItemTooltipWidget.h`, `RpgInventoryFeedbackToastWidget.h`:
  UI-06.
- `RpgQuickAccessRadialWidget.h`: UI-05.

Two colours are fixed in native code:
- the ingredient count colours in `RpgCraftingIngredientEntryWidget.cpp`, to be
  replaced in UI-04;
- the tooltip colours in `RpgInventoryItemTooltipWidget.cpp`, UI-06.
