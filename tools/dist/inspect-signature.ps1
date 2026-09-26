#requires -version 5.1
<#
.SYNOPSIS
    Reports how a Windows binary or an installed-app folder was signed, what
    certificate authority was used, and whether that CA is trusted locally.

.DESCRIPTION
    Answers the practical questions:
      * Is this EXE/DLL Authenticode-signed at all?
      * Who issued the certificate (which CA)?
      * Is that CA in the local Trusted Root store, or is it self-signed?
      * Was it timestamped (required for the signature to outlive the cert)?
      * Which signing *method* was used: Microsoft Store/MSIX, a hardware-token
        CA certificate, a Microsoft-trusted signing service, or nothing?

    IMPORTANT: be clear about what this proves and what it does not.
      * PROVES the file is Authenticode-signed and that the CA is trusted locally.
      * DOES NOT prove SmartScreen will be silent for a fresh download. SmartScreen
        reputation is a per-hash cloud signal, not a signature property.
      * A file can be perfectly Valid and still show SmartScreen on first download.

.PARAMETER Path
    A .exe/.dll/.msi, or a folder (typically an installed app folder) to scan
    recursively. Defaults to the repository's release artefact folder.

.PARAMETER MaxFiles
    Cap on how many binaries to report in detail. Default 40.

.PARAMETER ScanCatalogs
    Also inspect .cat catalog files (used for driver and some inbox signing).

.EXAMPLE
    .\tools\dist\inspect-signature.ps1 -Path "C:\Program Files\VideoLAN\VLC\vlc.exe"

.EXAMPLE
    .\tools\dist\inspect-signature.ps1 -Path "C:\Program Files\SomeApp"
#>
[CmdletBinding()]
param(
    [string] $Path,
    [int]    $MaxFiles = 40,
    [switch] $ScanCatalogs
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if (-not $Path) {
    $here = if ($PSScriptRoot) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
    $repoRoot = (Resolve-Path -LiteralPath (Join-Path $here '..\..')).ProviderPath
    $Path = Join-Path $repoRoot 'build\Convolver_artefacts\Release'
}

$script:BinaryExtensions = @('.exe', '.dll', '.msi', '.sys', '.ocx', '.node', '.pyd', '.efi')

# Well-known CA subject fragments, mapped to how they sign and what they imply.
# Used only for a friendly label; the authoritative trust check is Test-Certificate
# against the local root store.
$script:KnownCAs = @(
    @{ Match = 'DigiCert';                Label = 'DigiCert (commercial OV/EV, HSM or token)' }
    @{ Match = 'Sectigo';                 Label = 'Sectigo (commercial OV/EV, HSM or token)' }
    @{ Match = 'USERTrust';               Label = 'Sectigo/USERTrust (commercial OV/EV)' }
    @{ Match = 'COMODO';                  Label = 'Sectigo/Comodo (commercial OV/EV)' }
    @{ Match = 'GlobalSign';              Label = 'GlobalSign (commercial OV/EV)' }
    @{ Match = 'Certum';                  Label = 'Certum (incl. Open Source Code Signing tier)' }
    @{ Match = 'Entrust';                 Label = 'Entrust (commercial)' }
    @{ Match = 'SSL.com';                 Label = 'SSL.com (commercial)' }
    @{ Match = 'GoDaddy';                 Label = 'GoDaddy (commercial)' }
    @{ Match = 'Microsoft';               Label = 'Microsoft (own binaries or Store re-signing)' }
    @{ Match = 'Windows Third Party';     Label = 'Microsoft Windows Third Party Component CA' }
    @{ Match = 'Azure';                   Label = 'Microsoft Azure (Artifact/Trusted Signing service)' }
    @{ Match = 'SignPath';                Label = 'SignPath (used by the SignPath Foundation OSS programme)' }
)

function Get-ChainInfo {
    <# Builds the certificate chain from the signer and reports:
         Chained  - does it terminate in a root trusted by THIS machine?
         RootCA   - the root-most subject
         SelfSigned - is the signer its own issuer (i.e. a self-signed cert)?
         NotTimeValid - is the signer cert outside its validity window right now?

       We deliberately build with IgnoreNotTimeValid. Rationale, confirmed against
       real software on this machine: a code signing certificate typically lives
       1-3 years, so MOST validly signed commercial software has an already-expired
       signer certificate. That is normal and the signature stays valid forever
       because it was timestamped. Asking "is this cert valid today?" would produce
       a flood of false negatives - do not use Test-Certificate for this.

       The authoritative answer on "is this signature still good?" is the Signature
       Status from Get-AuthenticodeSignature, which already accounts for timestamping. #>
    param([Parameter(Mandatory)] [System.Security.Cryptography.X509Certificates.X509Certificate2] $Cert)

    $chain = New-Object System.Security.Cryptography.X509Certificates.X509Chain
    try {
        $chain.ChainPolicy.RevocationMode =
            [System.Security.Cryptography.X509Certificates.X509RevocationMode]::NoCheck
        $chain.ChainPolicy.VerificationFlags =
            [System.Security.Cryptography.X509Certificates.X509VerificationFlags]::IgnoreNotTimeValid

        $built = $chain.Build($Cert)
        $statuses = @($chain.ChainStatus | ForEach-Object { "$($_.Status)" })
        $root = if ($chain.ChainElements.Count -gt 0) {
            $chain.ChainElements[$chain.ChainElements.Count - 1].Certificate.Subject
        } else { $Cert.Issuer }

        return [pscustomobject]@{
            Chained      = $built
            RootCA       = $root
            SelfSigned   = ($Cert.Subject -eq $Cert.Issuer)
            NotTimeValid = ($statuses -contains 'NotTimeValid')
            Statuses     = $statuses
        }
    } finally { $chain.Dispose() }
}

function Get-SignatureVerdict {
    param([Parameter(Mandatory)] [System.Management.Automation.Signature] $Sig)

    $status = "$($Sig.Status)"
    $cert   = $Sig.SignerCertificate

    if (-not $cert) {
        return [pscustomobject]@{
            Method      = 'Unsigned'
            Trusted     = $false
            Detail      = switch -Regex ("$status") {
                'HashMismatch' { 'Signature present but the file was MODIFIED after signing (broken)' }
                'NotSigned'    { 'No Authenticode signature and no catalog match' }
                'UnknownError' { 'Signature could not be validated' }
                default        { "$status" }
            }
            RootCA       = $null
            Timestamped  = $false
            CertExpired  = $false
            SelfSigned   = $false
        }
    }

    $chain = Get-ChainInfo -Cert $cert
    $timestamped = [bool]$Sig.TimeStamperCertificate

    # Label the CA for a human-readable "which method" answer.
    $label = 'unrecognised CA'
    foreach ($k in $script:KnownCAs) {
        if ($chain.RootCA -match $k.Match -or $cert.Issuer -match $k.Match) { $label = $k.Label; break }
    }

    # Classify the signing method.
    $method = if ($chain.SelfSigned) {
        'Self-signed certificate (NOT trusted by Windows - same effect as unsigned)'
    } elseif (-not $chain.Chained) {
        'Signed by a CA that is NOT in the local Trusted Root store'
    } elseif ("$($Sig.IsOSBinary)" -eq 'True') {
        'Microsoft OS binary / inbox component'
    } elseif ($label -match 'Microsoft') {
        'Signed by Microsoft (own binary or Store re-sign)'
    } else {
        'Signed by a trusted commercial/organisational CA'
    }

    return [pscustomobject]@{
        Method      = $method
        Trusted     = $chain.Chained
        Detail      = $label
        RootCA      = $chain.RootCA
        Timestamped = $timestamped
        CertExpired = $chain.NotTimeValid
        SelfSigned  = $chain.SelfSigned
    }
}

function Get-PeMetadata {
    <# Reads the PE version resource. Useful precisely for UNSIGNED binaries:
       CompanyName etc. is what UAC and the Attachment Manager show as the
       publisher column ("Unknown Publisher" when absent). #>
    param([Parameter(Mandatory)] [string] $LiteralPath)
    try {
        $vi = (Get-Item -LiteralPath $LiteralPath).VersionInfo
        return [pscustomobject]@{
            Company     = $vi.CompanyName
            Product     = $vi.ProductName
            FileVersion = $vi.FileVersion
            Copyright   = $vi.LegalCopyright
        }
    } catch {
        return $null
    }
}

function Test-MsixProvenance {
    <# Detects Store/MSIX installs, which are re-signed by Microsoft and therefore
       never subject to the SmartScreen download prompt. #>
    param([Parameter(Mandatory)] [string] $LiteralPath)
    try {
        $pkg = Get-AppxPackage -ErrorAction Stop |
               Where-Object { $_.InstallLocation -and $LiteralPath.StartsWith($_.InstallLocation, 'OrdinalIgnoreCase') } |
               Select-Object -First 1
        if ($pkg) { return $pkg.PackageFullName }
    } catch { }
    return $null
}

# ------------------------------------------------------------------ main ----

$resolved = Resolve-Path -LiteralPath $Path -ErrorAction SilentlyContinue
if (-not $resolved) { Write-Error "Path not found: $Path"; exit 2 }
$target = $resolved.ProviderPath

Write-Host ''
Write-Host 'Signature / provenance report' -ForegroundColor Cyan
Write-Host ('=' * 74)
Write-Host "Target : $target"

if (Test-Path -LiteralPath $target -PathType Container) {
    $all = @(Get-ChildItem -LiteralPath $target -Recurse -File -ErrorAction SilentlyContinue)
    $binaries = @($all | Where-Object { $script:BinaryExtensions -contains $_.Extension.ToLowerInvariant() })
    Write-Host ("Folder : {0} files scanned, {1} binary payloads found" -f $all.Count, $binaries.Count)
    if ($binaries.Count -gt $MaxFiles) {
        Write-Host ("Note   : reporting the first {0}; pass -MaxFiles to see more" -f $MaxFiles) -ForegroundColor DarkYellow
        $binaries = $binaries | Select-Object -First $MaxFiles
    }
} else {
    $binaries = @(Get-Item -LiteralPath $target)
    Write-Host 'Single binary'
}

if ($binaries.Count -eq 0) { Write-Warning 'No binaries to inspect.'; exit 0 }

# MSIX provenance is a folder-level fact.
$msix = Test-MsixProvenance -LiteralPath $target
if ($msix) {
    Write-Host ''
    Write-Host "MSIX / Microsoft Store package detected: $msix" -ForegroundColor Green
    Write-Host 'Installed Store packages are re-signed by Microsoft and bypass SmartScreen.' -ForegroundColor Green
}

$rows = @()
foreach ($f in $binaries) {
    $sig = Get-AuthenticodeSignature -LiteralPath $f.FullName
    $v = Get-SignatureVerdict -Sig $sig

    $rows += [pscustomobject]@{
        File        = $f.Name
        Status      = "$($sig.Status)"
        Trusted     = $v.Trusted
        Timestamped = $v.Timestamped
        CertExpired = $v.CertExpired
        SelfSigned  = $v.SelfSigned
        Method      = $v.Method
        RootCA      = $v.RootCA
        Detail      = $v.Detail
        Meta        = Get-PeMetadata -LiteralPath $f.FullName
        Full        = $f.FullName
    }
}

Write-Host ''
Write-Host 'Summary by signature status' -ForegroundColor Cyan
Write-Host ('-' * 74)
$rows | Group-Object Status | Sort-Object Count -Descending | ForEach-Object {
    $colour = switch ($_.Name) { 'Valid' { 'Green' } 'NotSigned' { 'Yellow' } default { 'Red' } }
    Write-Host ("  {0,-14} {1,3} file(s)" -f $_.Name, $_.Count) -ForegroundColor $colour
}

Write-Host ''
Write-Host 'Detail' -ForegroundColor Cyan
Write-Host ('-' * 74)
foreach ($r in $rows) {
    $colour = if ($r.Status -eq 'Valid' -and $r.Trusted) { 'Green' }
              elseif ($r.Status -eq 'Valid')            { 'DarkYellow' }
              else                                      { 'Yellow' }
    Write-Host ("{0}" -f $r.File) -ForegroundColor White
    Write-Host ("    status      : {0}" -f $r.Status) -ForegroundColor $colour
    Write-Host ("    method      : {0}" -f $r.Method)
    if ($r.RootCA) {
        Write-Host ("    root CA     : {0}" -f $r.RootCA)
        Write-Host ("    CA identity : {0}" -f $r.Detail)
    }
    Write-Host ("    chains to trusted root : {0}" -f $r.Trusted)
    if ($r.Status -eq 'Valid') {
        Write-Host ("    timestamped            : {0}" -f $r.Timestamped)
        if ($r.CertExpired) {
            $explain = if ($r.Timestamped) { 'normal - timestamp keeps the signature valid' }
                       else                { 'WARNING - no timestamp to hold it valid' }
            Write-Host ("    signer cert expired    : yes ({0})" -f $explain) -ForegroundColor DarkGray
        }
    }
    if ($r.Meta) {
        $company = if ($r.Meta.Company) { $r.Meta.Company } else { '(empty - shows as Unknown Publisher)' }
        Write-Host ("    CompanyName : {0}" -f $company)
    }
    Write-Host ''
}

# Catalog files, if asked for.
if ($ScanCatalogs -and (Test-Path -LiteralPath $target -PathType Container)) {
    $cats = @(Get-ChildItem -LiteralPath $target -Recurse -File -Filter '*.cat' -ErrorAction SilentlyContinue)
    if ($cats.Count -gt 0) {
        Write-Host 'Catalog files (.cat)' -ForegroundColor Cyan
        Write-Host ('-' * 74)
        foreach ($c in $cats) {
            $cs = Get-AuthenticodeSignature -LiteralPath $c.FullName
            $cv = Get-SignatureVerdict -Sig $cs
            Write-Host ("{0}" -f $c.Name)
            Write-Host ("    status : {0}" -f $cs.Status) -ForegroundColor Gray
            Write-Host ("    method : {0}" -f $cv.Method)
            if ($cv.RootCA) { Write-Host ("    root CA: {0}" -f $cv.RootCA) }
            Write-Host ''
        }
    } else {
        Write-Host 'No .cat catalog files found.' -ForegroundColor DarkGray
    }
}

# Verdict + the caveat that matters.
Write-Host ('=' * 74)
$signedTrusted = @($rows | Where-Object { $_.Status -eq 'Valid' -and $_.Trusted }).Count
$anySigned     = @($rows | Where-Object { $_.Status -eq 'Valid' }).Count
$broken        = @($rows | Where-Object { $_.Status -eq 'HashMismatch' }).Count

Write-Host 'Verdict' -ForegroundColor Cyan
Write-Host ("  {0} of {1} binaries have a valid, locally-trusted signature." -f $signedTrusted, $rows.Count)
if ($broken -gt 0) {
    Write-Warning ("  {0} file(s) have a BROKEN signature (modified after signing)." -f $broken)
}
if ($anySigned -eq 0) {
    Write-Host '  Nothing here is signed. Expect "Unknown Publisher" in UAC and the' -ForegroundColor Yellow
    Write-Host '  Attachment Manager prompt, plus a SmartScreen prompt on first download.' -ForegroundColor Yellow
} else {
    Write-Host ''
    Write-Host '  Reminder: a valid signature does NOT guarantee a silent SmartScreen.' -ForegroundColor DarkYellow
    Write-Host '  SmartScreen reputation is a per-hash cloud signal. A newly built, validly' -ForegroundColor DarkYellow
    Write-Host '  signed binary still prompts until it accumulates download history.' -ForegroundColor DarkYellow
    Write-Host '  A Microsoft Store (MSIX) install is the only free provenance that never prompts.' -ForegroundColor DarkYellow
}
Write-Host ''
exit 0
