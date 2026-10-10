<#
.SYNOPSIS
Generates the key-cap textures T_UI_Key_<Key>.png that CommonInputData_Keyboard maps (UI-06).

.DESCRIPTION
Draws a dark cap with a gold rim and a bone white Roboto Bold label, or a mouse glyph, at 96 px height.
Single characters use 50 px text, labels up to three characters 34 px and longer labels 30 px; the cap
widens with its label in steps of 8 px. Import the PNGs as described in README.md.

.PARAMETER OutDir
Folder that receives a keys subfolder with the PNGs.

.PARAMETER EngineDir
Unreal Engine root; the script draws with the engine's Roboto Bold.
#>
param(
    [Parameter(Mandatory = $true)][string]$OutDir,
    [string]$EngineDir = 'D:/Programme/UE_5.8'
)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
New-Item -ItemType Directory -Force (Join-Path $OutDir 'keys') | Out-Null

$fonts = New-Object System.Drawing.Text.PrivateFontCollection
$fonts.AddFontFile((Join-Path $EngineDir "Engine/Content/Slate/Fonts/Roboto-Bold.ttf"))
$family = $fonts.Families[0]

function C([string]$hex, [int]$a = 255) {
    $c = [System.Drawing.ColorTranslator]::FromHtml($hex)
    return [System.Drawing.Color]::FromArgb($a, $c.R, $c.G, $c.B)
}
$Bone = C '#DDD4C2'
$GoldBright = C '#D9B979'
$CapTop = C '#2B2723'
$CapBottom = C '#141210'
$CapEdge = C '#8C7A52'

function RoundedPath([single]$x, [single]$y, [single]$w, [single]$h, [single]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc($x, $y, $d, $d, 180, 90)
    $p.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $p.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $p.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-Canvas([int]$w, [int]$h) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'
    $g.TextRenderingHint = 'AntiAliasGridFit'
    $g.PixelOffsetMode = 'HighQuality'
    $g.Clear([System.Drawing.Color]::Transparent)
    return @($bmp, $g)
}

function Draw-Cap($g, [int]$w, [int]$h) {
    $path = RoundedPath 4 4 ($w - 8) ($h - 8) 14
    $rect = New-Object System.Drawing.RectangleF 4, 4, ($w - 8), ($h - 8)
    $fill = New-Object System.Drawing.Drawing2D.LinearGradientBrush $rect, $CapTop, $CapBottom, 90
    $g.FillPath($fill, $path)
    # Bottom lip gives the cap a little depth.
    $lip = New-Object System.Drawing.Pen ((C '#000000' 140)), 4
    $g.DrawLine($lip, 18, ($h - 7), ($w - 18), ($h - 7))
    $pen = New-Object System.Drawing.Pen $CapEdge, 4
    $g.DrawPath($pen, $path)
    $hi = New-Object System.Drawing.Pen ((C '#FFFFFF' 28)), 2
    $g.DrawLine($hi, 18, 9, ($w - 18), 9)
}

function Save-Key([string]$name, [string]$label) {
    $size = if ($label.Length -le 1) { 50 } elseif ($label.Length -le 3) { 34 } else { 30 }
    $font = New-Object System.Drawing.Font $family, $size, ([System.Drawing.FontStyle]::Bold), ([System.Drawing.GraphicsUnit]::Pixel)
    $probe = New-Object System.Drawing.Bitmap 4, 4
    $pg = [System.Drawing.Graphics]::FromImage($probe)
    $tw = $pg.MeasureString($label, $font).Width
    $h = 96
    $w = [int][Math]::Max(96, [Math]::Ceiling(($tw + 44) / 8) * 8)
    $canvas = New-Canvas $w $h; $bmp = $canvas[0]; $g = $canvas[1]
    Draw-Cap $g $w $h
    $fmt = New-Object System.Drawing.StringFormat
    $fmt.Alignment = 'Center'; $fmt.LineAlignment = 'Center'
    $brush = New-Object System.Drawing.SolidBrush $Bone
    $g.DrawString($label, $font, $brush, (New-Object System.Drawing.RectangleF 0, 2, $w, $h), $fmt)
    $bmp.Save("$OutDir\keys\T_UI_Key_$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

function Save-Mouse([string]$name, [string]$part) {
    $canvas = New-Canvas 96 96; $bmp = $canvas[0]; $g = $canvas[1]
    Draw-Cap $g 96 96
    $body = RoundedPath 30 18 36 60 16
    $pen = New-Object System.Drawing.Pen $Bone, 4
    $accent = New-Object System.Drawing.SolidBrush $GoldBright
    if ($part -eq 'Left') {
        $clip = New-Object System.Drawing.Region $body
        $g.SetClip($clip, 'Replace'); $g.FillRectangle($accent, 30, 18, 18, 24); $g.ResetClip()
    } elseif ($part -eq 'Right') {
        $clip = New-Object System.Drawing.Region $body
        $g.SetClip($clip, 'Replace'); $g.FillRectangle($accent, 48, 18, 18, 24); $g.ResetClip()
    }
    $g.DrawPath($pen, $body)
    $g.DrawLine($pen, 30, 42, 66, 42)
    $g.DrawLine($pen, 48, 18, 48, 42)
    if ($part -eq 'Middle' -or $part -eq 'Wheel') {
        $g.FillRectangle($accent, 45, 23, 6, 14)
    }
    if ($part -eq 'Wheel') {
        $pts1 = @((New-Object System.Drawing.PointF 48, 6), (New-Object System.Drawing.PointF 42, 13), (New-Object System.Drawing.PointF 54, 13))
        $pts2 = @((New-Object System.Drawing.PointF 48, 90), (New-Object System.Drawing.PointF 42, 83), (New-Object System.Drawing.PointF 54, 83))
        $g.FillPolygon($accent, $pts1); $g.FillPolygon($accent, $pts2)
    }
    $bmp.Save("$OutDir\keys\T_UI_Key_$name.png", [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
}

# Keyboard keys: letters, digits, named keys.
foreach ($c in [char[]]'ABCDEFGHIJKLMNOPQRSTUVWXYZ') { Save-Key "$c" "$c" }
foreach ($d in 0..9) { Save-Key "$d" "$d" }
$named = [ordered]@{ Escape = 'Esc'; Enter = 'Enter'; Tab = 'Tab'; LeftShift = 'Shift'; LeftControl = 'Ctrl'; LeftAlt = 'Alt';
    SpaceBar = 'Space'; BackSpace = 'Back'; Delete = 'Del'; Up = [string][char]0x2191; Down = [string][char]0x2193;
    Left = [string][char]0x2190; Right = [string][char]0x2192; F1 = 'F1'; F2 = 'F2'; F3 = 'F3'; F4 = 'F4'; F5 = 'F5' }
foreach ($k in $named.Keys) { Save-Key $k $named[$k] }
Save-Mouse 'LeftMouseButton' 'Left'
Save-Mouse 'RightMouseButton' 'Right'
Save-Mouse 'MiddleMouseButton' 'Middle'
Save-Mouse 'MouseWheel' 'Wheel'

"keys: $((Get-ChildItem (Join-Path $OutDir 'keys')).Count)"
