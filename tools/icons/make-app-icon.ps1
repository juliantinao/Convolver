#requires -version 5.1
<#
.SYNOPSIS
    Generates the Convolver application artwork assets.

.DESCRIPTION
    This script is the single source of truth for the application icon. It emits
    three files from one geometry definition, and each is consumed by a
    different part of the build:

      packaging/icon/appicon.svg   Vector. CMake embeds it via
                                   juce_add_binary_data and Source/AppIcon.h
                                   rasterises it at whatever pixel size a window,
                                   the taskbar or a dialog needs. Because the
                                   source is vector, every size is rendered
                                   natively instead of resampled from a bitmap,
                                   which is what keeps the 16px taskbar icon
                                   crisp.

      packaging/icon/appicon.png   512x512 raster. CMake passes it to
                                   juce_add_gui_app(ICON_BIG), which makes
                                   juceaide build the multi-resolution
                                   Convolver.exe icon (16/32/48/256) and embed
                                   it as a PE resource.

      packaging/icon/appicon.ico   Multi-resolution icon (16/32/48/64/128/256)
                                   for Inno Setup's SetupIconFile, which accepts
                                   only .ico files. Sizes are drawn natively
                                   rather than resampled, and the set includes
                                   the 64x64 that Inno recommends and juceaide
                                   does not emit.

    WHY ICON_BIG CANNOT TAKE THE SVG
      juce_Icons.cpp resolves ICON_BIG through Drawable::createFromImageFile()
      and then calls getWidth() to decide which resolutions to emit. A raster
      file becomes a DrawableImage, whose setImageInternal() calls setBounds(),
      so getWidth() is correct. An SVG becomes a DrawableComposite, which only
      fills its own internal bounds/contentArea members and never calls
      Component::setBounds(), so getWidth() returns 0, every size fails the "is
      the source big enough" test, and the generated .ico comes out empty.
      JUCE's own GUI apps pass a 512x512 PNG for this reason (DemoRunner,
      AudioPluginHost).

    The drawing reproduces the original programmatic design that used to live in
    Source/AppIcon.h: a dark blue-grey disc with a white three-cycle sine wave
    across it.

.PARAMETER Size
    Coordinate space and PNG master edge length. Defaults to 512, comfortably
    above the 256 threshold Windows icon generation requires. Do not go below 256.

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
$icoPath = Join-Path $OutputDirectory 'appicon.ico'

Add-Type -AssemblyName System.Drawing

# SVG numbers must use '.' as the decimal separator regardless of the machine's
# locale, or the generated document is unparseable in comma-decimal cultures.
$invariant = [System.Globalization.CultureInfo]::InvariantCulture
function Format-SvgNumber([double] $value) { return $value.ToString('0.###', $invariant) }

function Write-FileIfChanged {
    <#
      Writes the file only when its bytes would actually differ.

      This matters beyond tidiness: tools/dist/build-installer.ps1 warns when a
      compiled input is newer than the built executable, and those assets are
      compiled inputs. Rewriting identical bytes would bump their timestamps on
      every run and turn that warning into noise that people learn to ignore.
    #>
    param(
        [string] $Path,
        [byte[]] $Bytes
    )

    if (Test-Path -LiteralPath $Path) {
        $existing = [System.IO.File]::ReadAllBytes($Path)
        if ($existing.Length -eq $Bytes.Length) {
            $identical = $true
            for ($i = 0; $i -lt $existing.Length; $i++) {
                if ($existing[$i] -ne $Bytes[$i]) { $identical = $false; break }
            }
            if ($identical) { return $false }
        }
    }

    [System.IO.File]::WriteAllBytes($Path, $Bytes)
    return $true
}

# ---------------------------------------------------------------- design ----
# Expressed as fractions of the canvas so every output stays in step. The factors
# are taken verbatim from the original AppIcon.h implementation, which was
# written against a 32px canvas: centre 0.5, amplitude 0.25, x starting at 0.15
# and spanning 0.7, stroke 1.8/32 of the canvas, 40 segments over 3 cycles.
$steps   = 40
$cycles  = 3.0
$disc    = [System.Drawing.Color]::FromArgb(255, 0x3A, 0x3A, 0x5C)
$discHex = '#3A3A5C'
$waveHex = '#FFFFFF'

# Waveform vertices in the given coordinate space.
function Get-WavePoints([double] $canvas) {
    $centre    = $canvas * 0.5
    $amplitude = $canvas * 0.25
    $startX    = $canvas * 0.15
    $spanX     = $canvas * 0.7

    $points = New-Object 'System.Collections.Generic.List[object]'
    $points.Add([pscustomobject]@{ X = $startX; Y = $centre })

    for ($i = 1; $i -le $steps; $i++) {
        $t        = $i / [double] $steps
        $x        = $startX + $t * $spanX
        $envelope = [Math]::Sin($t * [Math]::PI)
        $y        = $centre + [Math]::Sin($t * 2.0 * [Math]::PI * $cycles) * $amplitude * $envelope
        $points.Add([pscustomobject]@{ X = $x; Y = $y })
    }
    return $points
}

# Draws the artwork natively at the requested edge length. Every size is
# rendered from the geometry rather than scaled from a master, so small icons
# stay clean.
function New-IconBitmap([int] $canvas) {
    $bitmap = New-Object System.Drawing.Bitmap $canvas, $canvas
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    try {
        $graphics.SmoothingMode     = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode   = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.Clear([System.Drawing.Color]::Transparent)

        $brush = New-Object System.Drawing.SolidBrush $disc
        try { $graphics.FillEllipse($brush, 0, 0, $canvas, $canvas) } finally { $brush.Dispose() }

        $pen = New-Object System.Drawing.Pen ([System.Drawing.Color]::White), ([single](1.8 * ($canvas / 32.0)))
        try {
            $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Round
            $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Round
            $pen.EndCap   = [System.Drawing.Drawing2D.LineCap]::Round

            $drawPoints = New-Object 'System.Collections.Generic.List[System.Drawing.PointF]'
            foreach ($p in (Get-WavePoints $canvas)) {
                $drawPoints.Add([System.Drawing.PointF]::new([single] $p.X, [single] $p.Y))
            }
            $graphics.DrawLines($pen, $drawPoints.ToArray())
        } finally { $pen.Dispose() }
    } finally { $graphics.Dispose() }
    return $bitmap
}

# ------------------------------------------------------------ ICO writer ----
function ConvertTo-IcoEntryBytes {
    <#
      Encodes one image as an ICO directory payload.

      Sizes of 128 and above are stored PNG-compressed, which is the convention
      modern icon tools follow and which Windows has understood since Vista.
      Smaller sizes are stored as an uncompressed 32bpp DIB, because that is what
      every consumer expects there: a BITMAPINFOHEADER whose height is twice the
      image (the XOR bitmap plus the AND mask), bottom-up BGRA pixels, then an
      all-zero AND mask, since transparency is carried by the alpha channel.
    #>
    param([System.Drawing.Bitmap] $Bitmap)

    $dimension = $Bitmap.Width
    $stream = New-Object System.IO.MemoryStream
    try {
        if ($dimension -ge 128) {
            $Bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        } else {
            $writer = New-Object System.IO.BinaryWriter($stream)
            $xorSize   = $dimension * $dimension * 4
            $andStride = [int]([Math]::Floor(($dimension + 31) / 32) * 4)
            $andSize   = $andStride * $dimension

            $writer.Write([int] 40)                    # biSize
            $writer.Write([int] $dimension)            # biWidth
            $writer.Write([int] ($dimension * 2))      # biHeight: XOR + AND
            $writer.Write([int16] 1)                   # biPlanes
            $writer.Write([int16] 32)                  # biBitCount
            $writer.Write([int] 0)                     # biCompression: BI_RGB
            $writer.Write([int] ($xorSize + $andSize)) # biSizeImage
            $writer.Write([int] 0)                     # biXPelsPerMeter
            $writer.Write([int] 0)                     # biYPelsPerMeter
            $writer.Write([int] 0)                     # biClrUsed
            $writer.Write([int] 0)                     # biClrImportant

            for ($y = $dimension - 1; $y -ge 0; $y--) {
                for ($x = 0; $x -lt $dimension; $x++) {
                    $c = $Bitmap.GetPixel($x, $y)
                    $writer.Write([byte] $c.B)
                    $writer.Write([byte] $c.G)
                    $writer.Write([byte] $c.R)
                    $writer.Write([byte] $c.A)
                }
            }

            $emptyRow = New-Object byte[] $andStride
            for ($y = 0; $y -lt $dimension; $y++) { $writer.Write($emptyRow) }
            $writer.Flush()
        }
        return $stream.ToArray()
    } finally { $stream.Dispose() }
}

function New-IcoBytes {
    param([int[]] $Sizes)

    $payloads = @()
    foreach ($dimension in ($Sizes | Sort-Object)) {
        $bitmap = New-IconBitmap $dimension
        try {
            # The [byte[]] cast is load-bearing. PowerShell unrolls an array
            # returned from a function into Object[], and BinaryWriter.Write
            # then binds to the single-byte overload and silently writes one
            # byte instead of the whole image.
            $payloads += [pscustomobject]@{
                Size  = $dimension
                Bytes = [byte[]] (ConvertTo-IcoEntryBytes -Bitmap $bitmap)
            }
        } finally { $bitmap.Dispose() }
    }

    $stream = New-Object System.IO.MemoryStream
    try {
        $writer = New-Object System.IO.BinaryWriter($stream)

        # ICONDIR
        $writer.Write([int16] 0)              # reserved
        $writer.Write([int16] 1)              # type: 1 = icon
        $writer.Write([int16] $payloads.Count)

        # ICONDIRENTRY, 16 bytes each, followed by the image payloads.
        $offset = 6 + 16 * $payloads.Count
        foreach ($entry in $payloads) {
            # 256 is encoded as 0 in the single-byte width/height fields.
            $dimensionByte = if ($entry.Size -ge 256) { [byte] 0 } else { [byte] $entry.Size }
            $writer.Write($dimensionByte)                # bWidth
            $writer.Write($dimensionByte)                # bHeight
            $writer.Write([byte] 0)                      # bColorCount
            $writer.Write([byte] 0)                      # bReserved
            $writer.Write([int16] 1)                     # wPlanes
            $writer.Write([int16] 32)                    # wBitCount
            $writer.Write([int] $entry.Bytes.Length)     # dwBytesInRes
            $writer.Write([int] $offset)                 # dwImageOffset
            $offset += $entry.Bytes.Length
        }

        foreach ($entry in $payloads) {
            $bytes = [byte[]] $entry.Bytes
            $stream.Write($bytes, 0, $bytes.Length)
        }
        $writer.Flush()

        return [byte[]] $stream.ToArray()
    } finally { $stream.Dispose() }
}

# ------------------------------------------------------------------- SVG ----
# width/height and viewBox are deliberately identical. A viewBox that differs
# from width/height makes JUCE's SVG parser fold a placement transform into the
# drawable, which would then interact with our own drawWithin() scaling at
# runtime. Keeping them equal leaves the content area equal to the coordinate
# space and the transform identity.
$canvas = $Size
$pathData = ((Get-WavePoints $canvas) | ForEach-Object {
    '{0} {1}' -f (Format-SvgNumber $_.X), (Format-SvgNumber $_.Y)
}) -join ' L '

$svg = @"
<?xml version="1.0" encoding="UTF-8"?>
<!-- Generated by tools/icons/make-app-icon.ps1 - do not edit by hand. -->
<svg xmlns="http://www.w3.org/2000/svg" width="$canvas" height="$canvas" viewBox="0 0 $canvas $canvas">
  <circle cx="$(Format-SvgNumber ($canvas * 0.5))" cy="$(Format-SvgNumber ($canvas * 0.5))" r="$(Format-SvgNumber ($canvas * 0.5))" fill="$discHex"/>
  <path d="M $pathData" fill="none" stroke="$waveHex" stroke-width="$(Format-SvgNumber (1.8 * ($canvas / 32.0)))" stroke-linecap="round" stroke-linejoin="round"/>
</svg>
"@

# UTF-8 without a BOM, written explicitly. PowerShell 5.1's `-Encoding UTF8`
# emits a BOM, and although JUCE handles one, a byte-order mark sitting in front
# of the XML declaration is not something this file should ever contain.
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
$svgBytes  = $utf8NoBom.GetBytes($svg)
$svgChanged = Write-FileIfChanged -Path $svgPath -Bytes $svgBytes

# ------------------------------------------------------------------- PNG ----
# The raster master for ICON_BIG. Rendered natively at Size, not upscaled.
$master = New-IconBitmap $Size
try {
    $pngStream = New-Object System.IO.MemoryStream
    try {
        $master.Save($pngStream, [System.Drawing.Imaging.ImageFormat]::Png)
        $pngChanged = Write-FileIfChanged -Path $pngPath -Bytes ([byte[]] $pngStream.ToArray())
    } finally { $pngStream.Dispose() }
} finally { $master.Dispose() }

# ------------------------------------------------------------------- ICO ----
# 64 is included because Inno Setup recommends it for SetupIconFile and juceaide
# does not emit that size.
$icoBytes   = [byte[]] (New-IcoBytes -Sizes @(16, 32, 48, 64, 128, 256))
$icoChanged = Write-FileIfChanged -Path $icoPath -Bytes $icoBytes

# ---------------------------------------------------------------- report ----
Write-Host ''
$results = @(
    @{ Path = $svgPath; Changed = $svgChanged; Job = 'juce_add_binary_data      -> window/taskbar/dialog icons (vector)' },
    @{ Path = $pngPath; Changed = $pngChanged; Job = 'juce_add_gui_app(ICON_BIG) -> Convolver.exe icon (16/32/48/256)' },
    @{ Path = $icoPath; Changed = $icoChanged; Job = 'SetupIconFile             -> installer/uninstaller icon (16/32/48/64/128/256)' }
)

foreach ($r in $results) {
    $f = Get-Item -LiteralPath $r.Path
    $verb = if ($r.Changed) { 'Wrote    ' } else { 'Unchanged' }
    $colour = if ($r.Changed) { 'Green' } else { 'DarkGray' }
    Write-Host ("{0} {1}  ({2:N1} KB)" -f $verb, $f.FullName, ($f.Length / 1KB)) -ForegroundColor $colour
    Write-Host ("           {0}" -f $r.Job) -ForegroundColor DarkGray
}
Write-Host ''
Write-Host 'Rebuild to refresh the executable if any asset changed.' -ForegroundColor Cyan
exit 0
