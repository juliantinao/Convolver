---
name: capture-assertion
description: >
  Provide a reproducible, automatable way to run the built Convolver executable and capture any JUCE assertion output into a log file — without requiring a native debugger.
---

How it works
- The application is built with `JUCE_LOG_ASSERTIONS=1` (set in CMakeLists.txt), which routes assertion messages through `Logger::writeToLog()` instead of `OutputDebugString`.
- `Main.cpp` sets up a `juce::FileLogger` in the `ConvolverApplication` constructor that writes all logged messages (including assertion failures) to `convolver_runtime.log` next to the executable.
- The script `run_capture_assertions.ps1`:
  1. Auto-detects the built executable in common output paths (`out/build/x64-debug/...` for VS CMake integration, `build/...` for standalone CMake).
  2. Clears any previous runtime log.
  3. Launches the app with a configurable timeout.
  4. After the app exits, reads `convolver_runtime.log` and scans for assertion patterns (`JUCE Assertion failure in <file>:<line>`).
  5. Copies the runtime log to `tools/agent/run.log` so agents can read it.
  6. Reports a summary of any assertions or errors found, plus the exit code.
- If `cdb.exe` (Debugging Tools for Windows) is found, the script uses it as an enhanced debugger path that also captures stack traces on breakpoints.

Usage
- From repository root (PowerShell):
  ```powershell
  .\tools\agent\run_capture_assertions.ps1
  ```
- With explicit exe path:
  ```powershell
  .\tools\agent\run_capture_assertions.ps1 -ExePath .\build\Convolver_artefacts\Debug\Convolver.exe
  ```
- Custom timeout (default 10 seconds):
  ```powershell
  .\tools\agent\run_capture_assertions.ps1 -TimeoutSeconds 15
  ```

Output locations
- `<exe_dir>/convolver_runtime.log` — raw FileLogger output (created by the app).
- `tools/agent/run.log` — copy of the runtime log for agent consumption.
- `tools/agent/logs/run_stdout.log`, `run_stderr.log` — stdout/stderr capture.
- `tools/agent/logs/cdb_*.log` — cdb session log (only when cdb.exe is available).

Agent workflow
1. Build the project: `.\tools\agent\build_vs.ps1`
2. Run with assertion capture: `.\tools\agent\run_capture_assertions.ps1`
3. Read results: `Get-Content tools\agent\run.log`
4. Look for lines matching `JUCE Assertion failure in <filename>:<line>`

Notes
- `JUCE_LOG_ASSERTIONS=1` makes assertions log-and-continue (no `__debugbreak()`) when NOT running under a debugger. When running under the VS debugger (green play button), assertions still trigger a breakpoint as expected.
- The FileLogger is set up in the `ConvolverApplication` constructor, so assertions during `initialise()` or window creation are captured.
- Do not modify generated files under `out/` or `Convolver_artefacts/JuceLibraryCode/`.
