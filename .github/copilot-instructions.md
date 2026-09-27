Purpose
- Guidance for Copilot/automation agents working in this repository (JUCE audio app). Code like a JUCE audio specialist.
- This application performs **batch WAV-file convolution** — loading impulse responses and applying them to sets of WAV files offline.
- The convolution algorithm follows the Farina ESS pipeline documented in `Docs/farina_algorithm_coding_reference.md` (specifically the deconvolution / Phase C steps).


Build system
- The project uses **CMake** (minimum 3.22) with JUCE added via `add_subdirectory`.
- JUCE location: `C:\JUCE` (override with `-DJUCE_DIR=<path>`).
- Use the **CMake presets**. There is one multi-config configure preset named `x64`; Debug and Release share a single `build/` tree, so there is no `out/` directory.
  ```
  cmake --preset x64
  cmake --build --preset x64-release     # or: --preset x64-debug
  ```
- Output paths: `build\Convolver_artefacts\Release\Convolver.exe` (what gets packaged) and `build\Convolver_artefacts\Debug\Convolver.exe`.
- **`JuceHeader.h` is generated at build time, not at configure time.** It is a juceaide `CustomBuild` output under `build\Convolver_artefacts\JuceLibraryCode\JuceHeader.h`. Consequence: after "CMake: Delete Cache and Reconfigure" — or any other full clean — the C/C++ extension reports `cannot open source file "JuceHeader.h"` for every header until the project is **built** once. That is expected behaviour, not a broken configuration; do not go hunting for a CMake problem.
- VS Code is pinned to the presets in `.vscode/settings.json` (`cmake.configurePreset: x64`, `cmake.buildPreset: x64-release`). If a preset is ever renamed, update that file too, or CMake Tools silently ends up with no configuration and IntelliSense stops resolving every include.
- **The version is declared exactly once**, on the `project()` line in `CMakeLists.txt`. It reaches the executable's PE version resource and is then read back by the packaging driver. Never hardcode it anywhere else.
- `packaging/icon/appicon.{svg,png,ico}` are generated, committed assets. Regenerate with `tools/icons/make-app-icon.ps1`; the packaging guide's **Paso 6** explains why three formats are required and why none of them is redundant.

Running (default)
- The recommended, default way to run the application and capture JUCE assertions is the provided runner script `tools/agent/run_capture_assertions.ps1`.
- The build includes `JUCE_LOG_ASSERTIONS=1` (set in CMakeLists.txt), which routes assertion messages through `Logger::writeToLog()` instead of `OutputDebugString`, making them capturable without a native debugger.
- `Main.cpp` sets up a `juce::FileLogger` in the `ConvolverApplication` constructor. It writes to `convolver_runtime.log` **next to the executable when that directory is writable** (which is the case for dev builds), and otherwise falls back to `%APPDATA%\Convolver\convolver_runtime.log`, because the installer places the app somewhere an unelevated process cannot write to.
- `tools/agent/run_capture_assertions.ps1` looks in **both** locations, preferring the one beside the executable.
- The script auto-detects the built executable under `build\Convolver_artefacts\{Debug,Release}\` (both configurations share the `build/` tree, since the `x64` preset uses a multi-config generator), runs the app with a configurable timeout, then reads and parses the FileLogger output for assertion patterns.
- If `cdb.exe` (Debugging Tools for Windows) is available the script uses it as an enhanced path that also captures stack traces on breakpoints.
- `JUCE_LOG_ASSERTIONS=1` makes assertions log-and-continue when NOT running under a debugger. When running under the VS debugger (green play button), assertions still trigger a breakpoint as expected.
- Default invocation (PowerShell):
  ```powershell
  .\tools\agent\run_capture_assertions.ps1
  # auto-detects exe, TimeoutSeconds=10
  ```
- With explicit exe path or custom timeout:
  ```powershell
  .\tools\agent\run_capture_assertions.ps1 -ExePath .\build\Convolver_artefacts\Debug\Convolver.exe -TimeoutSeconds 15
  ```

Tests & lint
- No test or lint targets detected in the repository. If tests are added, place them under Tests/ and document the runner in this file.
- To run a single test: not applicable until a test runner is present—add test-target docs when tests are introduced.

High-level architecture (big picture)
- Project type: Desktop JUCE GUI application (standalone, not a plug-in).
- Source/ — human-authored application source (GUI, audio processing, Main.cpp which calls START_JUCE_APPLICATION).
- CMakeLists.txt — project build definition; links JUCE modules.
- Docs/ — documentation, design notes, and related materials. Includes `farina_algorithm_coding_reference.md` as the algorithmic reference for convolution.
- tools/agent/ — automation helper scripts (build_vs.ps1, run/log helpers) and agent.config.json for automation rules.
- tools/dist/ — distribution scripts: `unblock-distribution.ps1`, `inspect-signature.ps1` and `build-installer.ps1` (the installer driver).
- tools/icons/ — `make-app-icon.ps1`, the single source of truth for the application artwork; generates the committed assets in `packaging/icon/`.
- packaging/ — installer definition: `app.json` (the only app-specific file), `installer.iss` (app-agnostic template) and the generated icon assets.
- dist/ — installer output (gitignored).
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
  - Do not read JuceLibraryCode/ unless it is really necessary, preffer to consult JUCE documentation for understanding module APIs in https://docs.juce.com/master/index.html.
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
- Distribution/trust reference: `Docs/windows_trust_and_distribution.md` — why Windows blocks the unsigned `Convolver.exe` on other PCs (SmartScreen / MOTW vs. UAC vs. Defender vs. Smart App Control), which certificate options are actually worth paying for, and the free distribution paths. Read this before proposing any signing or packaging work.
- Packaging reference: `Docs/windows_packaging_inno_setup.md` — **a step-by-step replication brief (Pasos 0-8) written so an agent can reproduce the whole packaging flow in another JUCE repository.** Covers the three icon assets and why none is redundant, how the version stays in one place, the release and upgrade procedure, a definition of done, and a table of errors already made. Read it before touching anything under `packaging/`, `tools/dist/` or `tools/icons/`.
- JUCE documentation: https://docs.juce.com/master/index.html

Distribution & Windows trust
- Helper script: `tools/dist/unblock-distribution.ps1`. Removes the Mark-of-the-Web (`Zone.Identifier`) from a distribution folder so SmartScreen stops prompting, and reports SmartScreen / Smart App Control / Defender / Authenticode state. Always run it with `-CheckOnly` first:
  ```powershell
  .\tools\dist\unblock-distribution.ps1 -CheckOnly
  .\tools\dist\unblock-distribution.ps1 -Path "D:\Convolver-0.1.0-win64"
  ```
- Never sign releases with a self-signed certificate as a "fix" for SmartScreen — Microsoft documents it as behaving identically to no signature. The only free route to zero warnings is a Microsoft Store MSIX submission.
- When a user reports a Windows block, identify which of the four mechanisms is acting before suggesting a fix; they have completely different remedies. See `Docs/windows_trust_and_distribution.md` §1.
- Never distribute `build\Convolver_artefacts\Debug\Convolver.exe`; use the `Release` build.

Packaging (Inno Setup)
- Full reference: `Docs/windows_packaging_inno_setup.md`. Read it before changing anything under `packaging/` or `tools/dist/`.
- Build the installer with `tools/dist/build-installer.ps1`. **It never compiles anything**: it packages the Release executable that is already on disk, so the binary you verified in VS Code is the binary that ships. Do not add a build step to it.
- The driver absorbs the version, publisher and product name from the executable's PE version resource. Do not re-declare them in `packaging/app.json` or `packaging/installer.iss`.
- **A version bump needs the `.rc` regeneration block in `CMakeLists.txt`.** Without it the executable keeps reporting the previous version, silently and with no error, because JUCE's generated `.rc` does not depend on `Info.txt`. If you replicate this flow in another repository this block is mandatory — see the packaging guide, Paso 4.3.
- That block is also why you must never "clean up" the `file(GLOB ...)` / `IS_NEWER_THAN` logic in `CMakeLists.txt`: it looks like build cruft and is not.
- `packaging/installer.iss` is an app-agnostic template; every app-specific value arrives as a `/D` define. Do not hardcode app names or versions in it.
- `packaging/app.json` is the only app-specific file. Its `appId` must be a unique GUID and **must never change**, or upgrades install alongside the previous release instead of replacing it.
- Run `.\tools\dist\build-installer.ps1 -CheckOnly` first; it prints the plan and the exact defines without compiling.
- Always verify the finished installer on a clean machine: install, confirm `Convolver.exe` in the install folder carries no Mark-of-the-Web, and confirm a version upgrade replaces rather than duplicates.

Other AI/assistant configs checked
- No CONTRIBUTING.md present.
- No CLAUDE.md, AGENTS.md, .cursorrules, .windsurfrules, CONVENTIONS.md, AIDER_CONVENTIONS.md, .clinerules, or .cline_rules found. If any are added later, merge important rules into this file.

Where to look for problems
- Build logs: tools\agent\build.log
- Run logs: tools\agent\run.log (copied from `<exe_dir>/convolver_runtime.log` by the runner script).
- Raw FileLogger output: `<exe_dir>/convolver_runtime.log`, or `%APPDATA%\Convolver\convolver_runtime.log` when the executable's own directory is not writable (i.e. once installed).
- stdout/stderr capture: tools\agent\logs\run_stdout.log, tools\agent\logs\run_stderr.log.
- cdb session logs (when cdb.exe is available): tools\agent\logs\cdb_*.log.
- Runtime assertions and errors: search run.log for `JUCE Assertion failure in`, "Unhandled exception", "terminate called", or "exception:".

Maintaining this file
- Keep this file updated when:
  - CMake configuration changes (update build examples).
  - Tests or lint tooling are added (document runner and single-test commands).
  - tools\agent/ scripts change (update examples and log paths).
  - tools\dist/ scripts or the distribution/trust story change (update the Distribution & Windows trust section).

Summary
- Consolidated Copilot/agent instruction file for the Convolver JUCE project: CMake-based build, architecture overview, JUCE module list, convolution-focused conventions, and automation guidance.
