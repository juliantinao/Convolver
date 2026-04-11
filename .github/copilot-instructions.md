Purpose
- Guidance for Copilot/automation agents working in this repository (JUCE audio app). Code like a JUCE audio specialist.
- This application performs **batch WAV-file convolution** — loading impulse responses and applying them to sets of WAV files offline.
- The convolution algorithm follows the Farina ESS pipeline documented in `Docs/farina_algorithm_coding_reference.md` (specifically the deconvolution / Phase C steps).


Build system
- The project uses **CMake** (minimum 3.22) with JUCE added via `add_subdirectory`.
- JUCE location: `C:\JUCE` (override with `-DJUCE_DIR=<path>`).
- Configure & build (Visual Studio generator):
  ```
  cmake -B build -G "Visual Studio 18 2026" -A x64
  cmake --build build --config Debug
  ```
- Or open the repo folder in Visual Studio as a CMake project (File → Open → CMake…).

Tests & lint
- No test or lint targets detected in the repository. If tests are added, place them under Tests/ and document the runner in this file.
- To run a single test: not applicable until a test runner is present—add test-target docs when tests are introduced.

High-level architecture (big picture)
- Project type: Desktop JUCE GUI application (standalone, not a plug-in).
- Source/ — human-authored application source (GUI, audio processing, Main.cpp which calls START_JUCE_APPLICATION).
- CMakeLists.txt — project build definition; links JUCE modules.
- Docs/ — documentation, design notes, and related materials. Includes `farina_algorithm_coding_reference.md` as the algorithmic reference for convolution.
- tools/agent/ — automation helper scripts (build_vs.ps1, run/log helpers) and agent.config.json for automation rules.
- Data/logs: expected artifact locations referenced by scripts: logs/ (runtime), tools/agent/*.log, data/measurements/ for measurement artifacts (CSV/WAV).

JUCE modules in use
- juce_core, juce_events, juce_data_structures, juce_graphics
- juce_gui_basics, juce_gui_extra
- juce_audio_basics, juce_audio_formats (WAV read/write)
- juce_audio_devices, juce_audio_utils
- juce_dsp (FFT, Convolution, windowing)

Key conventions and repository-specific rules
- Generated file policy:
  - Never hand-edit generated build files (CMake output, JuceLibraryCode/, etc.).
- Builds and CI:
  - Capture build and run logs (tools\agent\build.log, tools\agent\run.log) and attach them to issues when failures occur.
- Agent behavior expectations:
  - All code must follow JUCE coding standards and project conventions; refer to JUCE documentation for API usage: https://docs.juce.com/master/index.html.
  - Use JUCE API's and classes for all applicable tasks; avoid custom implementations when JUCE provides a solution (e.g., juce::File for file handling, juce::Thread for threading, juce::AudioBuffer for audio processing).
  - JUCE functions and classes are always preferred over custom implementations for common tasks (e.g., file handling, threading, audio processing) to maintain consistency and reliability.
  - Use `juce::dsp::FFT` for all FFT operations and `juce::AudioFormatManager` / `juce::AudioFormatWriter` for WAV I/O.
  - Use tools\agent\agent.config.json for automation rules; modify only with human approval.

Automation runtime warnings
--------------------------
- Warning for automation agents: when running PowerShell commands from automation or helper scripts, do not overwrite or reassign reserved/system variables such as `$PID`.
- Avoid compact backgrounding one-liners that capture output into variables and then try to treat that value as a process id. Examples that caused failures:

  ```powershell
  # ❌ Unsafe — attempts to overwrite built-in $PID and treats command output as a PID
  $output = & "build\Convolver_artefacts\Debug\Convolver.exe" 2>&1 &;
  $pid = $output
  Stop-Process -Id $pid
  ```

- Recommended safe patterns:

  ```powershell
  # 1) Start and control a process object
  $proc = Start-Process -FilePath "build\Convolver_artefacts\Debug\Convolver.exe" -PassThru
  Start-Sleep -Seconds 4
  if (-not $proc.HasExited) {
      $proc.CloseMainWindow() | Out-Null
      $proc.WaitForExit(5000) | Out-Null
  }
  $proc.ExitCode

  # 2) Run blocking and check exit code
  & "build\Convolver_artefacts\Debug\Convolver.exe"
  Write-Output $LASTEXITCODE
  ```

- When implementing automation steps in `tools/agent/*` or in CI tasks, prefer explicit `Start-Process` usage or blocking execution to avoid shell-specific pitfalls. Log outputs to files instead of relying on fragile in-memory captures.

Documentation & existing guidance
- Algorithmic reference: `Docs/farina_algorithm_coding_reference.md` — describes the Farina ESS pipeline. This project focuses on the **convolution steps** (Phase C: deconvolution via frequency-domain multiply, and general-purpose WAV convolution).
- JUCE documentation: https://docs.juce.com/master/index.html

Other AI/assistant configs checked
- No CONTRIBUTING.md present.
- No CLAUDE.md, AGENTS.md, .cursorrules, .windsurfrules, CONVENTIONS.md, AIDER_CONVENTIONS.md, .clinerules, or .cline_rules found. If any are added later, merge important rules into this file.

Where to look for problems
- Build logs: tools\agent\build.log
- Run logs: tools\agent\run.log
- Runtime exceptions and JUCE checks: search run.log for JUCE_ASSERT, JUCE_CHECK, "Assertion failed", "Unhandled exception", "terminate called", or "exception:".

Maintaining this file
- Keep this file updated when:
  - CMake configuration changes (update build examples).
  - Tests or lint tooling are added (document runner and single-test commands).
  - tools\agent/ scripts change (update examples and log paths).

Summary
- Consolidated Copilot/agent instruction file for the Convolver JUCE project: CMake-based build, architecture overview, JUCE module list, convolution-focused conventions, and automation guidance.
