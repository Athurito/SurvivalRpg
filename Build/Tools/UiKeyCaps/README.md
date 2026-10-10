# UI key caps

`New-KeyCaps.ps1` draws the keyboard and mouse key caps that CommonUI shows
for input actions (UI-06): a dark cap with a gold rim and a bone white Roboto
Bold label, 96 px high. The look is described in
[`docs/ui-style-guide.md`](../../../docs/ui-style-guide.md) under "Art set".

## Add or change a key

1. Add the key to the script: a letter or digit to its loop, a named key to
   `$named` (texture name = Unreal key name, value = label), or a mouse button
   with `Save-Mouse`.
2. Generate the PNGs outside the repository:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File Build/Tools/UiKeyCaps/New-KeyCaps.ps1 -OutDir Saved/UiKeyCaps
```

3. Import the new `T_UI_Key_<Key>.png` into
   `/Game/SurvivalRpg/UI/Art/Keys` through the Unreal MCP workflow, replacing
   an existing texture in place. Copy the texture settings (compression, mip
   generation, LOD group, sRGB, filter, never stream) from an existing cap.
4. Add the key to `InputBrushDataMap` of
   `/Game/SurvivalRpg/UI/Menus/Shared/Platform/CommonInputData_Keyboard`: draw
   as `Image`, 50 units high and as wide as the texture's aspect ratio gives
   (a 128 × 96 cap is 66.7 × 50). Read the size from the PNG, not from the
   texture asset, which reports a 32 × 32 placeholder while it still compiles.
   Right-hand modifiers reuse the left-hand caps, and the mouse wheel axis and
   scroll keys share `T_UI_Key_MouseWheel`.

Generating the existing keys again reproduces the imported PNGs byte for byte.
