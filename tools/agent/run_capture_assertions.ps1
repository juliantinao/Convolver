<#
.SYNOPSIS
  Run the Convolver executable and capture any JUCE assertion messages.
.DESCRIPTION
  The application writes assertion messages (and all DBG output) to a FileLogger
  at <exe_dir>/convolver_runtime.log (requires JUCE_LOG_ASSERTIONS=1 in the build).
  This script:
    1. Locates the built executable (auto-detects common build output paths).
    2. Clears the previous runtime log.
    3. Launches the app with a timeout.
    4. After exit, reads the runtime log and scans for assertion patterns.
    5. Copies results to tools/agent/run.log for agent consumption.
  If cdb.exe (Windows Debugging Tools) is available it is used for richer output
  including stack traces on breakpoints.
.PARAMETER ExePath
  Explicit path to Convolver.exe. If empty, the script auto-detects.
.PARAMETER TimeoutSeconds
  How long to let the application run before closing it (default 10).
#>
param(
    [string]$ExePath = "",
    [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = "Continue"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot  = Resolve-Path (Join-Path $scriptDir "..\..") -ErrorAction SilentlyContinue
if ($repoRoot) { $repoRoot = $repoRoot.Path } else { $repoRoot = $scriptDir }

# ── Locate executable ────────────────────────────────────────────────────────
function Find-Exe {
    param([string]$Explicit, [string]$Root)
    # If the caller supplied an explicit path, try it first
    if ($Explicit -and (Test-Path $Explicit)) { return (Resolve-Path $Explicit).Path }

    # Auto-detect common build output locations (VS CMake integration + standalone CMake)
    $candidates = @(
        "$Root\out\build\x64-debug\Convolver_artefacts\Debug\Convolver.exe",
        "$Root\build\Convolver_artefacts\Debug\Convolver.exe",
        "$Root\out\build\x64-release\Convolver_artefacts\Release\Convolver.exe",
        "$Root\build\Convolver_artefacts\Release\Convolver.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return (Resolve-Path $c).Path }
    }
    return $null
}

$exeFull = Find-Exe -Explicit $ExePath -Root $repoRoot
if (-not $exeFull) {
    Write-Error "Executable not found. Build the project first or pass -ExePath."
    exit 2
}

$exeDir  = Split-Path -Parent $exeFull
$runtimeLog = Join-Path $exeDir "convolver_runtime.log"

Write-Output "=== Convolver Assertion Capture ==="
Write-Output "Exe      : $exeFull"
Write-Output "Runtime log: $runtimeLog"
Write-Output "Timeout  : ${TimeoutSeconds}s"
Write-Output ""

# ── Clear previous runtime log ───────────────────────────────────────────────
if (Test-Path $runtimeLog) {
    Remove-Item $runtimeLog -Force -ErrorAction SilentlyContinue
    Write-Output "Cleared previous runtime log."
}

# ── Locate cdb.exe (optional, for enhanced debugging) ────────────────────────
function Find-Cdb {
    $candidates = @(
        "C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe",
        "C:\Program Files (x86)\Windows Kits\10\Debuggers\x86\cdb.exe",
        "C:\Program Files (x86)\Windows Kits\8.1\Debuggers\x64\cdb.exe"
    )
    foreach ($c in $candidates) {
        if (Test-Path $c) { return $c }
    }
    $which = Get-Command cdb.exe -ErrorAction SilentlyContinue
    if ($which) { return $which.Source }
    return $null
}

$cdb = Find-Cdb
$logsDir = Join-Path $scriptDir "logs"
New-Item -ItemType Directory -Force -Path $logsDir | Out-Null

# ── Run ──────────────────────────────────────────────────────────────────────
$exitCode = -1

if ($cdb) {
    # ── Enhanced path: run under cdb ─────────────────────────────────────────
    $timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
    $cdbLog = Join-Path $logsDir "cdb_$timestamp.log"

    Write-Output "[cdb] Found: $cdb"
    Write-Output "[cdb] Session log: $cdbLog"

    # cdb script: open log, on any breakpoint print stack and quit
    $cdbInitScript = Join-Path $logsDir "cdb_init_$timestamp.txt"
    $cdbCommands = @"
.logopen $cdbLog
sxe -c "kp; .logclose; q" bpe
sxe -c "kp; .logclose; q" *
g
"@
    Set-Content -Path $cdbInitScript -Value $cdbCommands -Encoding ASCII

    $psi = New-Object System.Diagnostics.ProcessStartInfo
    $psi.FileName = $cdb
    $psi.Arguments = "-o -cf `"$cdbInitScript`" `"$exeFull`""
    $psi.RedirectStandardOutput = $true
    $psi.RedirectStandardError  = $true
    $psi.UseShellExecute = $false
    $psi.CreateNoWindow   = $true
    $p = [System.Diagnostics.Process]::Start($psi)

    $start = Get-Date
    while (-not $p.HasExited -and ((Get-Date) - $start).TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Milliseconds 300
    }
    if (-not $p.HasExited) {
        Write-Output "[cdb] Timeout reached — killing debugger + child."
        $p.Kill()
    }
    $p.WaitForExit(3000)
    $exitCode = $p.ExitCode

    $cdbStdout = $p.StandardOutput.ReadToEnd()
    $cdbStderr = $p.StandardError.ReadToEnd()
    Write-Output "[cdb] Exit code: $exitCode"
    if ($cdbStdout) { Write-Output "[cdb] stdout:"; Write-Output $cdbStdout }
    if ($cdbStderr) { Write-Output "[cdb] stderr:"; Write-Output $cdbStderr }
    if (Test-Path $cdbLog) { Write-Output "[cdb] Log:"; Get-Content $cdbLog | Write-Output }
} else {
    # ── Standard path: run directly, rely on FileLogger ──────────────────────
    Write-Output "[run] No cdb found — running directly (FileLogger captures assertions)."
    $stdoutLog = Join-Path $logsDir "run_stdout.log"
    $stderrLog = Join-Path $logsDir "run_stderr.log"

    $proc = Start-Process -FilePath $exeFull `
                          -RedirectStandardOutput $stdoutLog `
                          -RedirectStandardError  $stderrLog `
                          -PassThru

    $start = Get-Date
    while (-not $proc.HasExited -and ((Get-Date) - $start).TotalSeconds -lt $TimeoutSeconds) {
        Start-Sleep -Milliseconds 300
    }
    if (-not $proc.HasExited) {
        Write-Output "[run] Timeout reached — requesting graceful close."
        $proc.CloseMainWindow() | Out-Null
        Start-Sleep -Seconds 2
        if (-not $proc.HasExited) {
            Write-Output "[run] Force-killing process."
            $proc.Kill()
        }
    }
    $proc.WaitForExit(3000)
    $exitCode = $proc.ExitCode
    Write-Output "[run] Exit code: $exitCode"

    $stdoutContent = Get-Content $stdoutLog -Raw -ErrorAction SilentlyContinue
    $stderrContent = Get-Content $stderrLog -Raw -ErrorAction SilentlyContinue
    if ($stdoutContent) { Write-Output "[run] stdout:"; Write-Output $stdoutContent }
    if ($stderrContent) { Write-Output "[run] stderr:"; Write-Output $stderrContent }
}

# ── Read and analyse the runtime log (FileLogger output) ─────────────────────
Write-Output ""
Write-Output "=== Runtime Log Analysis ==="

if (Test-Path $runtimeLog) {
    $logContent = Get-Content $runtimeLog -Raw -ErrorAction SilentlyContinue
    $logLines   = Get-Content $runtimeLog -ErrorAction SilentlyContinue

    # Copy to expected agent log location
    $agentRunLog = Join-Path $scriptDir "run.log"
    Copy-Item $runtimeLog $agentRunLog -Force
    Write-Output "Runtime log copied to: $agentRunLog"
    Write-Output ""

    # Scan for assertion patterns
    $assertionPattern = "JUCE Assertion failure|jassertfalse|Assertion failed|JUCE_ASSERT"
    $assertions = $logLines | Select-String -Pattern $assertionPattern
    if ($assertions) {
        Write-Output "*** ASSERTIONS DETECTED ($($assertions.Count)) ***"
        foreach ($a in $assertions) {
            Write-Output "  >> $($a.Line)"
        }
        Write-Output ""
    } else {
        Write-Output "No assertion messages found in runtime log."
    }

    # Also scan for exceptions / errors
    $errorPattern = "exception:|Unhandled exception|terminate called|Error:|FATAL"
    $errors = $logLines | Select-String -Pattern $errorPattern
    if ($errors) {
        Write-Output "*** ERRORS DETECTED ($($errors.Count)) ***"
        foreach ($e in $errors) {
            Write-Output "  >> $($e.Line)"
        }
        Write-Output ""
    }

    # Print full log
    Write-Output "--- Full runtime log ---"
    Write-Output $logContent
    Write-Output "--- End runtime log ---"
} else {
    Write-Output "WARNING: Runtime log not found at $runtimeLog"
    Write-Output "  The app may have crashed before initialising the FileLogger,"
    Write-Output "  or the build does not include JUCE_LOG_ASSERTIONS=1."

    # Still copy stderr to run.log if available
    $fallbackErr = Join-Path $logsDir "run_stderr.log"
    if (Test-Path $fallbackErr) {
        $errContent = Get-Content $fallbackErr -Raw -ErrorAction SilentlyContinue
        if ($errContent) {
            $agentRunLog = Join-Path $scriptDir "run.log"
            Copy-Item $fallbackErr $agentRunLog -Force
            Write-Output "Stderr log copied to: $agentRunLog"
        }
    }
}

# ── Summary ──────────────────────────────────────────────────────────────────
Write-Output ""
if ($exitCode -eq 0 -or $exitCode -eq -1) {
    Write-Output "=== RESULT: App exited normally (code $exitCode) ==="
} elseif ($exitCode -eq 0x80000003 -or $exitCode -eq -2147483645) {
    Write-Output "=== RESULT: App hit a breakpoint / assertion (STATUS_BREAKPOINT, code $exitCode) ==="
} else {
    Write-Output "=== RESULT: App exited abnormally (code $exitCode) ==="
}

exit $exitCode
