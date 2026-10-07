# UI roadmap

This roadmap moves the game's screens and HUD to one shared dark-fantasy style.
It records the accepted decisions and the task sequence. The style itself
(tokens, assets, rules) is in [ui-style-guide.md](ui-style-guide.md).

## Problem

- **No shared style:** every screen typed its own colours and fonts into its
  Widget Blueprints, including the skill screen. There were no UI materials and
  no CommonUI default styles.
- **Crafting:** the screen looked flat and had no fantasy atmosphere. Most of
  its view model data (recipe states, output preview, job state, filters) was
  never shown.
- **Inventory:** `CUI_PlayerInventoryPane` scales down to fit, so it shrinks
  inside the storage and crafting screens. Its cells then differ in size from
  the storage grid next to it.
- **Equipment:** the gear slots had no empty-slot silhouette, label or frame.
- **HUD:** three plain progress bars, of which only health was bound.

## Decisions

| Topic | Decision |
| --- | --- |
| Base style | The skill screen (HARV-09b): subtle, easy to restyle and extend. |
| Mood | Darker: a dark-fantasy open-world RPG. Near-black warm panels, tarnished gold instead of bright gold, bone and ash text, blood red, stronger vignette. |
| Fonts | Headings in Cinzel (SIL Open Font License, [third-party-notices.md](third-party-notices.md)), body text and numbers in Roboto. KnightsQuest has an unclear licence and is not used in new work. |
| Materials | Subtle procedural UI materials driven by one palette (`MPC_UI_Palette`), restyled through material instance parameters. The `fantasy_gui_4` marketplace frames are not used. |
| Inventory | Tarkov-style paper doll and container grids with one cell size everywhere. The standalone inventory shows three columns: equipment, containers and character stats. |
| HUD | Health, stamina and mana bars; mana is prepared but hidden until a pawn has a mana attribute. An XP bar. Positions may change so the HUD stays out of the way during play. |
| Controller support | Wanted later; every task keeps visible focus states, no mouse-only actions and scroll-into-view on focus (see the style guide). |
| Base terminal | Out of scope: it is a leftover of the old inventory whose screen route was removed ([physical-storage-implementation.md](physical-storage-implementation.md)). |

## C++ boundary

- **Classification:** designer-owned presentation on top of existing view
  models. Fonts, palette, materials, styles and layouts are assets.
- **Native work:** read models only where data is missing (UI-03 character
  stats, UI-04 station name, UI-05 mana), plus test updates where tests froze
  presentation structure.

## Tasks

| ID | Scope | Status |
| --- | --- | --- |
| UI-01 | Style foundation: Cinzel font, palette collection, UI materials, CommonUI text, button and border styles, editor template styles, skill screen as proof, style guide | In progress |
| UI-02 | Tarkov inventory: pane in columns, one shared cell size, new gear and carry slots, storage and crafting hosting, presentation-only test contracts relaxed | Planned |
| UI-03 | Character stats column (level, XP, load, health, stamina, armour), rarity frames on gear slots, MVVM toolset | Planned |
| UI-04 | Crafting screen: layout, station name, recipe states, categories and search, output preview, job state | Planned |
| UI-05 | HUD: new arrangement, material bars for health, stamina and mana, XP bar, context fading, action bar and Q/E/R, enemy health bar | Planned |
| UI-06 | Tooltip, context menu, split and drop dialogs, toasts, drag visual | Planned |
| UI-07 | Menus (game menu tabs, main menu, settings, respawn), then remove KnightsQuest | Planned |

## Style foundation (UI-01)

- **Assets:** the palette, materials, fonts and styles listed in the style
  guide, under `Content/SurvivalRpg/UI/Styles` and
  `Content/SurvivalRpg/UI/Fonts/Cinzel`.
- **Editor defaults:** `Config/DefaultEditor.ini` sets the CommonUI template
  styles, so newly placed text blocks, buttons and borders start with the
  shared Body, Default and Panel styles.
- **Skill screen:** `CUI_Skills`, `CUI_SkillTreeNode`, `CUI_TradeSkillEntry`
  and `CUI_SkillTreeListEntry` now use the shared text styles and material
  brushes. Their layout and graphs are unchanged.
  - The node keeps `SetBackgroundColor` per state. Its brushes come from the
    tinted frame instances, which use a dark neutral fill, so each state colour
    reads as a dark tone of itself.
  - The grid links use the palette colours.
- **Palette at runtime:** the palette is a material parameter collection, so a
  later system (for example corruption) can recolour every UI material at
  runtime. A PIE test changed two palette colours and the skill screen followed
  at once.
