#requires -version 5.1
<#
.SYNOPSIS
    Generates the Convolver application icon assets.

.DESCRIPTION
    This script is the single source of truth for the application artwork. It
    emits two files from one geometry definition, both consumed by the build:

      packaging/icon/appicon.svg   Vector. CMake embeds it via
                                   juce_add_binary_data, and Source/AppIcon.h
                                   rasterises it at whatever pixel size the
                                   window, taskbar or dialog needs. Because the
                                   source is vector, every size is rendered
                                   natively instead of resampled from a bitmap,
                                   which is what keeps the 16px taskbar icon
                                   crisp.

      packaging/icon/appicon.png   512x512 raster. CMake passes it to
                                   juce_add_gui_app(ICON_BIG), which makes
                                   juceaide build the multi-resolution
                                   Convolver.exe icon (16/32/48/256) and embed
                                   it in the PE resource section.

    WHY ONE FORMAT IS NOT ENOUGH
      * ICON_BIG cannot take an SVG. juce_Icons.cpp resolves the file through
        Drawable::createFromImageFile() and then calls getWidth() to decide
        which resolutions to emit. A raster file becomes a DrawableImage, whose
        setImageInternal() calls setBounds(), so getWidth() is correct. An SVG
        becomes a DrawableComposite, which only fills its own internal
        bounds/contentArea members and never calls Component::setBounds(), so
        getWidth() returns 0, every size fails the "is the source big enough"
        test, and the generated .ico comes out empty. JUCE's own GUI apps pass
        a 512x512 PNG for this reason (DemoRunner, AudioPluginHost).
      * The runtime window icons have the opposite requirement: they want vector
        so they are rendered natively at each size rather than downscaled.

    Both files are therefore generated together from the numbers below, so they
    cannot drift apart.

    The drawing reproduces the original programmatic design that used to live in
    Source/AppIcon.h: a dark blue-grey disc with a white three-cycle sine wave
    across it.

.PARAMETER Size
    Coordinate space and PNG edge length. Defaults to 512, comfortably above the
    256 threshold Windows icon generation requires. Do not go below 256.

.PARAMETER OutputDirectory
    Where to write the assets. Defaults to packaging/icon in the repository root.

.EXAMPLE
    .\tools\icons\make-app-icon.ps1

.EXAMPLE
    .\tools\icons\make-app-icon.ps1 -Size 1024 -OutputDirectory .\build\icons-1024
#>
[CmdletBinding()]
param(
    [ValidateRange(256, 4096)]
    [int] $Size = 512,

    [string] $OutputDirectory
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $OutputDirectory) {
    $here = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $here '..\..')).ProviderPath
    $OutputDirectory = Join-Path $repoRoot 'packaging\icon'
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$svgPath = Join-Path $OutputDirectory 'appicon.svg'
$pngPath = Join-Path $OutputDirectory 'appicon.png'

# SVG numbers must use '.' as the decimal separator regardless of the machine's
# locale, or the generated document is unparseable in comma-decimal cultures.
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
function Format-SvgNumber([double] $value) { return $value.ToString('0.###', $invariant) }

# ---------------------------------------------------------------- design ----
# Expressed as fractions of Size so both assets stay in step. The factors are
# taken verbatim from the original AppIcon.h implementation, which was written
# against a 32px canvas: centre 0.5, amplitude 0.25, x starting at 0.15 and
# spanning 0.7, stroke 1.8/32 of the canvas, 40 segments over 3 cycles.
$centre    = $Size * 0.5
$amplitude = $Size * 0.25
$startX    = $Size * 0.15
$spanX     = $Size * 0.7
$strokeW   = 1.8 * ($Size / 32.0)
$steps     = 40
$cycles    = 3.0
$radius    = $Size * 0.5

$discHex   = '#3A3A5C'
$waveHex   = '#FFFFFF'

# Waveform vertices, shared by both outputs.
$points = New-Object 'System.Collections.Generic.List[object]'
$points.Add([pscustomobject]@{ X = $startX; Y = $centre })

for ($i = 1; $i -le $steps; $i++) {
    $t        = $i / [double] $steps
    $x        = $startX + $t * $spanX
    $envelope = [Math]::Sin($t * [Math]::PI)
    $y        = $centre + [Math]::Sin($t * 2.0 * [Math]::PI * $cycles) * $amplitude * $envelope
    $points.Add([pscustomobject]@{ X = $x; Y = $y })
}

# ------------------------------------------------------------------- SVG ----
# width/height and viewBox are deliberately identical. A viewBox that differs
# from width/height makes JUCE's SVG parser fold a placement transform into the
# drawable (juce_SVGParser.cpp), which would then interact with our own
# drawWithin() scaling at runtime. Keeping them equal leaves the content area
# equal to the coordinate space and the transform identity.
$pathData = ($points | ForEach-Object {
    '{0} {1}' -f (Format-SvgNumber $_.X), (Format-SvgNumber $_.Y)
}) -join ' L '

$svg = @"
<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated by tools/icons/make-app-icon.ps1 - do not edit by hand. -->
<svg xmlns="http://www.w3.org/2000/svg" width="$Size" height="$Size" viewBox="0 0 $Size $Size">
  <circle cx="$(Format-SvgNumber $centre)" cy="$(Format-SvgNumber $centre)" r="$(Format-SvgNumber $radius)" fill="$discHex"/>
  <path d="M $pathData" fill="none" stroke="$waveHex" stroke-width="$(Format-SvgNumber $strokeW)" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
"@

# UTF-8 without a BOM, written explicitly. PowerShell 5.1's `-Encoding UTF8`
# emits a BOM, and although JUCE handles one, a byte-order mark sitting in front
# of the XML declaration is not something this file should ever contain.
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($svgPath, $svg, $utf8NoBom)

# ------------------------------------------------------------------- PNG ----
Add-Type -AssemblyName System.Drawing

$bitmap = New-Object System.Drawing.Bitmap $Size, $Size
try {
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::Transparent)

        $discBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 0x3A, 0x3A, 0x5C))
        try {
            $graphics.FillEllipse($discBrush, 0, 0, $Size, $Size)
        } finally { $discBrush.Dispose() }

        $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), ([single] $strokeW)
        try {
            $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
            $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
            $pen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round

            $drawPoints = New-Object 'System.Collections.Generic.List[System.Drawing.PointF]'
            foreach ($p in $points) {
                $drawPoints.Add([System.Drawing.PointF]::new([single] $p.X, [single] $p.Y))
            }
            $graphics.DrawLines($pen, $drawPoints.ToArray())
        } finally { $pen.Dispose() }
    } finally { $graphics.Dispose() }

    $bitmap.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
} finally { $bitmap.Dispose() }

# ---------------------------------------------------------------- report ----
Write-Host ''
foreach ($p in @($svgPath, $pngPath)) {
    $f = Get-Item -LiteralPath $p
    Write-Host ("Wrote {0}" -f $f.FullName) -ForegroundColor Green
    Write-Host ("  {0,8:N1} KB   SHA256 {1}" -f ($f.Length / 1KB), (Get-FileHash -LiteralPath $f.FullName -Algorithm SHA256).Hash) -ForegroundColor DarkGray
}
Write-Host ''
Write-Host 'appicon.svg -> juce_add_binary_data -> window/taskbar/dialog icons (vector, any size)' -ForegroundColor Cyan
Write-Host 'appicon.png -> juce_add_gui_app(ICON_BIG) -> Convolver.exe icon (16/32/48/256)'      -ForegroundColor Cyan
Write-Host 'Rebuild to refresh both.' -ForegroundColor Cyan
exit 0
