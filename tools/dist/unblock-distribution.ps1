#requires -version 5.1
<#
.SYNOPSIS
    Removes the Mark-of-the-Web (Zone.Identifier) from a distribution folder so
    Windows Defender SmartScreen stops showing "Windows protected your PC" for
    Convolver.exe and its DLLs.

.DESCRIPTION
    SmartScreen does NOT trigger merely because Convolver.exe is unsigned. It
    triggers because the file carries the Mark-of-the-Web (MOTW): the NTFS
    alternate data stream "Zone.Identifier", which Windows attaches to files
    downloaded from the Internet (browser download, mail attachment, cloud sync).

    Signature status only decides what the prompt SAYS:
      * unsigned          -> "Unknown publisher"
      * signed but new    -> "Unrecognized app" + your verified publisher name

    Deleting the MOTW stream removes the trigger entirely.

    IMPORTANT LIMITS
      * This only helps the machine that already has the files. A user who
        downloads the EXE from the web gets a fresh MOTW. Shipping an installer
        is the better default: see Docs/windows_trust_and_distribution.md.
      * This does nothing on machines running Windows 11 Smart App Control in
        enforcement mode: Smart App Control blocks unsigned binaries regardless
        of MOTW. -CheckOnly reports the state.

.PARAMETER Path
    Folder to clean (recursive) or a single file. Defaults to the repository's
    release artefact folder.

.PARAMETER CheckOnly
    Report what would change, plus Windows security-feature state, without
    modifying anything.

.PARAMETER SkipSecurityReport
    Suppress the Windows security-feature report.

.EXAMPLE
    .\tools\dist\unblock-distribution.ps1 -CheckOnly

.EXAMPLE
    .\tools\dist\unblock-distribution.ps1 -Path "D:\Convolver-0.1.0-win64"

.EXAMPLE
    .\tools\dist\unblock-distribution.ps1 -Path "C:\Users\me\Desktop\Convolver.exe"
#>
[CmdletBinding()]
param(
    [string] $Path,
    [switch] $CheckOnly,
    [switch] $SkipSecurityReport
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# $PSScriptRoot is not reliably populated under -File on some hosts, so derive
# the repository root defensively.
if (-not $Path) {
    $here = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $here '..\..')).ProviderPath
    $Path = Join-Path $repoRoot 'build\Convolver_artefacts\Release'
}

# Extensions Windows treats as executable / zone-aware. Other dist content
# (WAV, PDF, TXT, IR files) is harmless and is deliberately left alone.
$script:PayloadExtensions = @(
    '.exe', '.dll', '.msi', '.msix', '.msixbundle', '.appx', '.cab',
    '.ps1', '.bat', '.cmd', '.vbs', '.js', '.scr', '.com', '.cpl', '.sys'
)

function Get-SecurityState {
    <# Reads the Windows switches that decide whether an unsigned app is merely
       warned about or hard-blocked. #>
    $state = [ordered]@{}

    # Smart App Control: 0 = off, 1 = enforcement, 2 = evaluation
    $sac = $null
    try {
        $sac = (Get-ItemProperty -Path 'HKLM:\SYSTEM\CurrentControlSet\Control\CI\Policy' `
                    -Name 'VerifiedAndReputablePolicyState' -ErrorAction Stop).VerifiedAndReputablePolicyState
    } catch { $sac = $null }

    $state['SmartAppControl'] = switch ("$sac") {
        '0'     { 'Off' }
        '1'     { 'ENFORCEMENT - blocks unsigned binaries' }
        '2'     { 'Evaluation' }
        default { 'Not present / unknown' }
    }

    # SmartScreen for Explorer - owns the "Windows protected your PC" dialog
    $ss = $null
    try {
        $ss = (Get-ItemProperty -Path 'HKLM:\SOFTWARE\Microsoft\Windows\CurrentVersion\Explorer' `
                    -Name 'SmartScreenEnabled' -ErrorAction Stop).SmartScreenEnabled
    } catch { $ss = $null }
    $state['SmartScreen'] = if ($ss) { $ss } else { 'Not configured (default = Warn)' }

    # Defender state - relevant only if the report was a real malware detection
    try {
        $mp = Get-MpComputerStatus -ErrorAction Stop
        $state['DefenderRealTime']   = $mp.RealTimeProtectionEnabled
        $state['DefenderAntivirus']  = $mp.AntivirusEnabled
        $state['DefenderSigVersion'] = $mp.AntivirusSignatureVersion
    } catch {
        $state['DefenderRealTime'] = 'Unavailable (Get-MpComputerStatus failed; non-Defender AV?)'
    }

    return $state
}

function Write-SecurityReport {
    param([Parameter(Mandatory)] [System.Collections.IDictionary] $State)

    Write-Host ''
    Write-Host 'Windows security features on THIS machine' -ForegroundColor Cyan
    Write-Host ('-' * 62)
    foreach ($key in $State.Keys) {
        $value  = "$($State[$key])"
        $colour = 'Gray'
        if ($value -match 'ENFORCEMENT') { $colour = 'Red' }
        elseif ($value -eq 'Off')        { $colour = 'Green' }
        Write-Host ("  {0,-20} {1}" -f $key, $value) -ForegroundColor $colour
    }
    Write-Host ''

    if ("$($State['SmartAppControl'])" -match 'ENFORCEMENT') {
        Write-Warning 'Smart App Control is in ENFORCEMENT mode - unblocking will NOT help; unsigned binaries are blocked outright.'
        Write-Warning 'Fix: Microsoft Store MSIX submission (free, Microsoft re-signs) or a trusted code signing certificate.'
    }
    if ("$($State['DefenderRealTime'])" -eq 'True') {
        Write-Host '  If Windows Defender itself said "virus" (not SmartScreen), that is a false positive:' -ForegroundColor DarkYellow
        Write-Host '  https://www.microsoft.com/en-us/wdsi/filesubmission' -ForegroundColor DarkYellow
    }
}

function Get-ZoneId {
    <# Returns the ZoneId from a file's Zone.Identifier stream:
       3 = Internet, 4 = Restricted Sites (both trigger SmartScreen).
       Returns $null when the file has no Mark-of-the-Web. #>
    param([Parameter(Mandatory)] [string] $LiteralPath)

    $content = $null
    try {
        $content = Get-Content -LiteralPath $LiteralPath -Stream 'Zone.Identifier' -ErrorAction Stop
    } catch {
        return $null
    }
    if (-not $content) { return -1 }   # has the stream, but it is empty

    $match = [regex]::Match(($content -join "`n"), '(?im)^\s*ZoneId\s*=\s*(\d+)\s*$')
    if ($match.Success) { return [int] $match.Groups[1].Value }
    return -1                          # has the stream, but no parseable ZoneId
}

# ------------------------------------------------------------------ main ----

$resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
if (-not $resolved) {
    Write-Error "Path not found: $Path"
    exit 2
}
$target = $resolved.ProviderPath

if (Test-Path -LiteralPath $target -PathType Container) {
    Write-Host "Scanning folder (recursive): $target" -ForegroundColor Cyan
    $files = @(Get-ChildItem -LiteralPath $target -Recurse -File -ErrorAction SilentlyContinue)
} else {
    Write-Host "Scanning single file: $target" -ForegroundColor Cyan
    $files = @(Get-Item -LiteralPath $target)
}

if ($files.Count -eq 0) {
    Write-Warning 'No files found.'
    exit 0
}

# Pair each payload with its zone id once, so we only read the stream once.
$payloads = @(
    $files |
    Where-Object { $script:PayloadExtensions -contains $_.Extension.ToLowerInvariant() } |
    ForEach-Object { [pscustomobject]@{ File = $_; Zone = (Get-ZoneId -LiteralPath $_.FullName) } }
)
$marked = @($payloads | Where-Object { $null -ne $_.Zone })

Write-Host ("  Files scanned       : {0}" -f $files.Count)
Write-Host ("  Executable payloads : {0}" -f $payloads.Count)
Write-Host ("  Carrying MOTW       : {0}" -f $marked.Count)

if ($marked.Count -gt 0) {
    Write-Host ''
    Write-Host 'These files carry downloaded-file provenance and can trigger SmartScreen:' -ForegroundColor Yellow
    foreach ($p in $marked) {
        Write-Host ("  [zone {0}] {1}" -f $p.Zone, $p.File.FullName) -ForegroundColor Yellow
    }
} else {
    Write-Host ''
    Write-Host 'No Mark-of-the-Web on executable payloads - SmartScreen should not prompt.' -ForegroundColor Green
}

# Signature status explains WHAT the prompt would say, not WHETHER it appears.
Write-Host ''
Write-Host 'Authenticode status:' -ForegroundColor Cyan
foreach ($p in $payloads) {
    $sig    = Get-AuthenticodeSignature -LiteralPath $p.File.FullName
    $signer = if ($sig.SignerCertificate) { $sig.SignerCertificate.Subject } else { '(none)' }
    Write-Host ("  {0,-14} {1}" -f $sig.Status, $p.File.Name) -ForegroundColor Gray
    Write-Host ("      signer: {0}" -f $signer) -ForegroundColor DarkGray
}

if (-not $SkipSecurityReport) {
    Write-SecurityReport -State (Get-SecurityState)
}

if ($CheckOnly) {
    Write-Host 'CheckOnly specified - nothing was modified.' -ForegroundColor Cyan
    exit 0
}

if ($marked.Count -eq 0) {
    Write-Host 'Nothing to unblock.' -ForegroundColor Green
    exit 0
}

Write-Host ''
$unblocked = 0
foreach ($p in $marked) {
    try {
        Unblock-File -LiteralPath $p.File.FullName
        $unblocked++
        Write-Host ("  unblocked {0}" -f $p.File.FullName) -ForegroundColor Green
    } catch {
        Write-Warning ("  could not unblock {0}: {1}" -f $p.File.FullName, $_.Exception.Message)
    }
}

Write-Host ''
Write-Host ("Done. {0} file(s) unblocked. Verify with: .\tools\dist\unblock-distribution.ps1 -CheckOnly" -f $unblocked) -ForegroundColor Green
exit 0
