#requires -version 5.1
<#
.SYNOPSIS
    Packages an already-built Release of Convolver into a Windows installer with
    Inno Setup.

.DESCRIPTION
    This script does NOT build anything. It takes the Release executable that is
    already on disk, reads its identity out of the PE version resource, and hands
    those values to Inno Setup as preprocessor defines.

    The reason for that split is that the build and the packaging have different
    inputs and are verified at different moments. You build and check the app in
    VS Code; only then do you package that exact binary. Nothing here can
    silently pull in a different one:

      * The version, publisher and product name are absorbed from the
        executable, never declared again. Change the version in CMakeLists.txt,
        rebuild, and the installer follows.
      * A freshness check compares every compiled input against the executable's
        timestamp and warns if the binary looks older than the source it should
        have been built from.
      * The SHA-256 of both the executable and the finished installer are
        printed, together with the git revision, so a given installer can be
        traced back to the tree it came from.

    Values that differ between applications live in packaging/app.json; the
    Inno script packaging/installer.iss is a template with no app-specific
    content.

.PARAMETER AppConfigPath
    Application manifest. Defaults to packaging/app.json in the repository root.

.PARAMETER IsccPath
    Full path to ISCC.exe. When omitted the script locates Inno Setup through
    the registry, then the usual install locations, then PATH.

.PARAMETER OutputDirectory
    Where to write the installer. Defaults to the repository's dist/ folder.

.PARAMETER CheckOnly
    Validate the manifest, the executable and the Inno Setup installation, then
    report exactly what would be packaged, without compiling anything.

.PARAMETER Force
    Suppress the staleness warning and package anyway, even if a compiled input
    is newer than the executable. Intended for the case where git has just
    rewritten file timestamps and nothing actually needs rebuilding.

.EXAMPLE
    .\tools\dist\build-installer.ps1 -CheckOnly

.EXAMPLE
    .\tools\dist\build-installer.ps1

.EXAMPLE
    .\tools\dist\build-installer.ps1 -Force -OutputDirectory D:\releases
#>
[CmdletBinding()]
param(
    [string] $AppConfigPath,
    [string] $IsccPath,
    [string] $OutputDirectory,
    [switch] $CheckOnly,
    [switch] $Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# $PSScriptRoot is not reliably populated under -File on some hosts, so derive
# the repository root defensively.
$here = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
$repoRoot = (Resolve-Path -LiteralPath (Join-Path $here '..\..')).ProviderPath

function Write-Step  { param([string] $Message) Write-Host $Message -ForegroundColor Cyan }
function Write-Fail  { param([string] $Message) Write-Host $Message -ForegroundColor Red }

# ------------------------------------------------------------- manifest ----
if (-not $AppConfigPath) { $AppConfigPath = Join-Path $repoRoot 'packaging\app.json' }

if (-not (Test-Path -LiteralPath $AppConfigPath)) {
    Write-Fail "Application manifest not found: $AppConfigPath"
    exit 2
}

$app = Get-Content -LiteralPath $AppConfigPath -Raw | ConvertFrom-Json
$presentProperties = @($app.PSObject.Properties.Name)

# StrictMode turns a missing property into an exception, so check explicitly and
# report every problem at once rather than dying on the first one.
$missing = @()
foreach ($field in @('appId', 'appName', 'exeName', 'artefactDir', 'iconFile', 'runtimeLogName')) {
    if ($field -notin $presentProperties -or [string]::IsNullOrWhiteSpace([string] $app.$field)) {
        $missing += $field
    }
}
if ($missing.Count -gt 0) {
    Write-Fail ("Manifest {0} is missing required field(s): {1}" -f $AppConfigPath, ($missing -join ', '))
    exit 2
}

$sourceDir = Join-Path $repoRoot ($app.artefactDir -replace '/', '\')
$exePath   = Join-Path $sourceDir $app.exeName

Write-Host ''
Write-Host '=== Convolver :: build installer ===' -ForegroundColor Cyan
Write-Host ("Manifest : {0}" -f $AppConfigPath)
Write-Host ("App      : {0}  ({1})" -f $app.appName, $app.appId)
Write-Host ''

# ----------------------------------------------------- built executable ----
if (-not (Test-Path -LiteralPath $exePath)) {
    Write-Fail "Release executable not found: $exePath"
    Write-Host ''
    Write-Host 'Build it first, in VS Code or with:' -ForegroundColor Yellow
    Write-Host '  cmake --preset x64' -ForegroundColor Yellow
    Write-Host '  cmake --build --preset x64-release' -ForegroundColor Yellow
    exit 3
}

$exeItem = Get-Item -LiteralPath $exePath
$versionInfo = $exeItem.VersionInfo

# Absorbed from the binary; never re-declared. See CMakeLists.txt.
$productVersion = [string] $versionInfo.ProductVersion
$publisher      = [string] $versionInfo.CompanyName
$productName    = [string] $versionInfo.ProductName

if ([string]::IsNullOrWhiteSpace($publisher)) {
    Write-Fail "The executable has no CompanyName in its version resource."
    Write-Host 'Set COMPANY_NAME in the juce_add_gui_app() call in CMakeLists.txt and rebuild.'
    exit 3
}

if ([string]::IsNullOrWhiteSpace($productVersion)) {
    Write-Fail "The executable has no ProductVersion in its version resource."
    Write-Host 'Set the version in the project() call in CMakeLists.txt and rebuild.'
    exit 3
}

# Inno's VersionInfoVersion directive wants digits and dots only, so take the
# leading numeric run out of something like "0.1.0" or "0.1.0-beta".
$numericVersion = [regex]::Match($productVersion, '^\d+(\.\d+){0,3}').Value
if ([string]::IsNullOrWhiteSpace($numericVersion)) {
    Write-Fail "Cannot derive a numeric version from ProductVersion '$productVersion'."
    exit 3
}

Write-Step 'Built executable'
Write-Host ("  Path      : {0}" -f $exePath)
Write-Host ("  Size      : {0:N2} MB" -f ($exeItem.Length / 1MB))
Write-Host ("  Built     : {0}" -f $exeItem.LastWriteTime)
Write-Host ("  Version   : {0}  (from the PE version resource)" -f $productVersion)
Write-Host ("  Publisher : {0}" -f $publisher)
if ($productName -and $productName -ne $app.appName) {
    Write-Host ("  Note      : ProductName is '{0}' but the manifest says '{1}'." -f $productName, $app.appName) -ForegroundColor Yellow
}
Write-Host ''

# -------------------------------------------------------- freshness check ----
# Only inputs that are actually compiled into the executable count. Documentation,
# the Inno script and this driver are deliberately excluded so that editing them
# never triggers the warning.
$compiledInputs = @()

$sourceRoot = Join-Path $repoRoot 'Source'
if (Test-Path -LiteralPath $sourceRoot) {
    $compiledInputs += Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Include *.cpp, *.h -ErrorAction SilentlyContinue
}

$cmakeLists = Join-Path $repoRoot 'CMakeLists.txt'
if (Test-Path -LiteralPath $cmakeLists) { $compiledInputs += Get-Item -LiteralPath $cmakeLists }

# Even among the icon assets, only some end up inside the executable: the SVG is
# embedded as binary data and the PNG becomes the executable's icon resource.
# The .ico is read by Inno Setup alone (SetupIconFile) and never touches the
# binary, so it must not be able to raise the staleness warning.
$iconDirectory = Split-Path -Parent (Join-Path $repoRoot ($app.iconFile -replace '/', '\'))
if (Test-Path -LiteralPath $iconDirectory) {
    $compiledInputs += Get-ChildItem -LiteralPath $iconDirectory -Recurse -File -ErrorAction SilentlyContinue |
                       Where-Object { $_.Extension -ne '.ico' }
}

$newestInput = $compiledInputs | Sort-Object LastWriteTime -Descending | Select-Object -First 1

if ($newestInput -and $newestInput.LastWriteTime -gt $exeItem.LastWriteTime) {
    if ($Force) {
        Write-Host ("Staleness check skipped (-Force). Newest input is {0} at {1}." -f `
            $newestInput.FullName.Replace($repoRoot + '\', ''), $newestInput.LastWriteTime) -ForegroundColor DarkGray
    } else {
        Write-Warning 'The executable looks older than the source it should have been built from.'
        Write-Host ("  Newest compiled input : {0}" -f $newestInput.FullName.Replace($repoRoot + '\', '')) -ForegroundColor Yellow
        Write-Host ("                          {0}" -f $newestInput.LastWriteTime) -ForegroundColor Yellow
        Write-Host ("  Executable            : {0}" -f $exeItem.LastWriteTime) -ForegroundColor Yellow
        Write-Host '  Rebuild before releasing, or pass -Force if this is a git timestamp artefact.' -ForegroundColor Yellow
        Write-Host '  Packaging the existing binary anyway.' -ForegroundColor Yellow
    }
}

# ------------------------------------------------------------ Inno Setup ----
function Find-Iscc {
    param([string] $Explicit)

    if ($Explicit) {
        if (Test-Path -LiteralPath $Explicit) { return (Resolve-Path -LiteralPath $Explicit).ProviderPath }
        return $null
    }

    # The uninstall registry entries are the most reliable source: they cover
    # Inno Setup 6 and 7, machine-wide and per-user, 32- and 64-bit.
    $uninstallRoots = @(
        'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKLM:\SOFTWARE\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall',
        'HKCU:\SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall'
    )
    foreach ($root in $uninstallRoots) {
        if (-not (Test-Path $root)) { continue }
        foreach ($key in Get-ChildItem $root -ErrorAction SilentlyContinue) {
            $entry = Get-ItemProperty -LiteralPath $key.PSPath -ErrorAction SilentlyContinue
            if (-not $entry) { continue }

            # Not every Uninstall subkey carries these values, and StrictMode
            # turns a missing property into a terminating error, so read them
            # through the property bag instead of dereferencing directly.
            $displayName     = $entry.PSObject.Properties['DisplayName']
            $installLocation = $entry.PSObject.Properties['InstallLocation']

            if (-not $displayName -or $displayName.Value -notlike 'Inno Setup*') { continue }
            if (-not $installLocation -or [string]::IsNullOrWhiteSpace([string] $installLocation.Value)) { continue }

            $candidate = Join-Path $installLocation.Value 'ISCC.exe'
            if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).ProviderPath }
        }
    }

    foreach ($base in @($env:ProgramFiles, ${env:ProgramFiles(x86)}, (Join-Path $env:LOCALAPPDATA 'Programs'))) {
        if (-not $base) { continue }
        foreach ($edition in @('Inno Setup 7', 'Inno Setup 6')) {
            $candidate = Join-Path $base (Join-Path $edition 'ISCC.exe')
            if (Test-Path -LiteralPath $candidate) { return (Resolve-Path -LiteralPath $candidate).ProviderPath }
        }
    }

    $onPath = Get-Command iscc.exe -ErrorAction SilentlyContinue
    if ($onPath) { return $onPath.Source }

    return $null
}

$compiler = Find-Iscc -Explicit $IsccPath
if (-not $compiler) {
    Write-Fail 'ISCC.exe (Inno Setup compiler) not found.'
    Write-Host 'Install Inno Setup from https://jrsoftware.org/isdl.php, or pass -IsccPath.' -ForegroundColor Yellow
    exit 4
}

$compilerVersionInfo = (Get-Item -LiteralPath $compiler).VersionInfo
$compilerVersion = if ($compilerVersionInfo.FileVersion) { $compilerVersionInfo.FileVersion }
                   elseif ($compilerVersionInfo.ProductVersion) { $compilerVersionInfo.ProductVersion }
                   else { 'unknown' }
Write-Step 'Inno Setup'
Write-Host ("  Compiler  : {0}" -f $compiler)
Write-Host ("  Version   : {0}" -f $compilerVersion)
Write-Host ''

# ------------------------------------------------------------ packaging ----
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $repoRoot 'dist' }
if (-not (Test-Path -LiteralPath $OutputDirectory)) {
    New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
}
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).ProviderPath

$outputBaseFilename = '{0}-{1}-win64-setup' -f $app.appName, $numericVersion
$issPath = Join-Path $repoRoot 'packaging\installer.iss'

# SetupIconFile needs a real .ico; app.json's iconFile points at the one
# tools/icons/make-app-icon.ps1 generates (the .exe icon comes from the .png via
# ICON_BIG, and the runtime icons from the .svg).
$iconPath = Join-Path $repoRoot ($app.iconFile -replace '/', '\')

# Passing the values as /D defines keeps installer.iss free of app details. Each
# element is one argv entry, so a value containing spaces needs no inner quotes:
# Inno's preprocessor takes everything after '=' as the value.
$defines = @(
    "/DAppId=$($app.appId)",
    "/DAppName=$($app.appName)",
    "/DAppVersion=$numericVersion",
    "/DAppPublisher=$publisher",
    "/DExeName=$($app.exeName)",
    "/DRuntimeLogName=$($app.runtimeLogName)",
    "/DSourceDir=$sourceDir",
    "/DIconFile=$iconPath",
    "/DOutputDir=$OutputDirectory",
    "/DOutputBaseFilename=$outputBaseFilename"
)

if ($app.licenseFile) {
    $licensePath = Join-Path $repoRoot ($app.licenseFile -replace '/', '\')
    if (Test-Path -LiteralPath $licensePath) { $defines += "/DLicenseFile=$licensePath" }
}

Write-Step 'Would package'
Write-Host ("  Payload   : {0}" -f $exePath)
Write-Host ("  Template  : {0}" -f $issPath)
Write-Host ("  Output    : {0}\{1}.exe" -f $OutputDirectory, $outputBaseFilename)
Write-Host ''

if ($CheckOnly) {
    Write-Host 'Defines that would be passed to ISCC:' -ForegroundColor Cyan
    foreach ($d in $defines) { Write-Host ("  {0}" -f $d) -ForegroundColor Gray }
    Write-Host ''
    Write-Host 'CheckOnly specified - nothing was compiled.' -ForegroundColor Cyan
    exit 0
}

Write-Step 'Compiling installer'
$isccOutput = & $compiler @defines $issPath 2>&1
$isccExit = $LASTEXITCODE
$isccOutput | ForEach-Object { Write-Host ("  {0}" -f $_) -ForegroundColor Gray }

if ($isccExit -ne 0) {
    Write-Host ''
    Write-Fail ("ISCC.exe failed with exit code {0}." -f $isccExit)
    exit 5
}

$installerPath = Join-Path $OutputDirectory ($outputBaseFilename + '.exe')
if (-not (Test-Path -LiteralPath $installerPath)) {
    Write-Fail "ISCC reported success but no installer appeared at $installerPath"
    exit 5
}

$installerItem = Get-Item -LiteralPath $installerPath

# ------------------------------------------------------------- reporting ----
$gitRevision = 'unavailable'
$gitState = ''
try {
    $head = & git -C $repoRoot rev-parse --short HEAD 2>$null
    if ($LASTEXITCODE -eq 0 -and $head) {
        $gitRevision = "$head".Trim()
        $porcelain = & git -C $repoRoot status --porcelain 2>$null
        if ($porcelain) { $gitState = ' + uncommitted changes' }
    }
} catch { $gitRevision = 'unavailable' }

Write-Host ''
Write-Host '=== Done ===' -ForegroundColor Green
Write-Host ("Installer : {0}" -f $installerItem.FullName)
Write-Host ("  Size    : {0:N2} MB" -f ($installerItem.Length / 1MB))
Write-Host ''
Write-Host 'Provenance - what actually went in:' -ForegroundColor Cyan
Write-Host ("  {0}" -f $exePath)
Write-Host ("    {0:N2} MB  version {1}" -f ($exeItem.Length / 1MB), $productVersion)
Write-Host ("    SHA256 {0}" -f (Get-FileHash -LiteralPath $exePath -Algorithm SHA256).Hash)
Write-Host ("  git {0}{1}" -f $gitRevision, $gitState)
Write-Host ''
Write-Host ("  SHA256 {0}" -f (Get-FileHash -LiteralPath $installerItem.FullName -Algorithm SHA256).Hash)
Write-Host ''
Write-Host 'Next: install it on a clean machine and confirm that Convolver.exe in' -ForegroundColor Cyan
Write-Host 'the install folder carries no Mark-of-the-Web:' -ForegroundColor Cyan
Write-Host '  .\tools\dist\unblock-distribution.ps1 -Path "<install folder>" -CheckOnly' -ForegroundColor Gray
exit 0
