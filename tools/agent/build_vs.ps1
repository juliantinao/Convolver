<#
.SYNOPSIS
  Configure and build the Convolver project using CMake + Visual Studio.
.DESCRIPTION
  Generates the VS solution into ./build and builds Debug|x64 by default.
  Override with -Config Release or -Arch Win32.
#>
param(
    [string]$Config = "Debug",
    [string]$Arch   = "x64",
    [string]$JuceDir = "C:/JUCE"
)

$ErrorActionPreference = "Stop"
$buildDir = Join-Path $PSScriptRoot "..\..\build"
$logFile  = Join-Path $PSScriptRoot "build.log"

Write-Host "=== Convolver Build ===" -ForegroundColor Cyan
Write-Host "Config : $Config"
Write-Host "Arch   : $Arch"
Write-Host "JUCE   : $JuceDir"
Write-Host "Log    : $logFile"
Write-Host ""

# Configure
Write-Host ">> CMake configure..." -ForegroundColor Yellow
cmake -B $buildDir -G "Visual Studio 18 2026" -A $Arch "-DJUCE_DIR=$JuceDir" 2>&1 | Tee-Object -FilePath $logFile

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configure FAILED — see $logFile" -ForegroundColor Red
    exit 1
}

# Build
Write-Host ">> CMake build ($Config)..." -ForegroundColor Yellow
cmake --build $buildDir --config $Config 2>&1 | Tee-Object -Append -FilePath $logFile

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build FAILED — see $logFile" -ForegroundColor Red
    exit 1
}

Write-Host "Build succeeded." -ForegroundColor Green
