# Convolver

Convolver is a desktop JUCE application for **offline batch WAV-file convolution**.

It lets you load a single WAV file as an **impulse response (IR)** and apply it to one or many other WAV files in one pass. The app is aimed at practical workflows such as reverb rendering, speaker/cabinet processing, measured impulse responses, and Farina ESS deconvolution-style processing.

## Features

- Batch-process multiple WAV files with one IR
- Offline FFT-based convolution using `juce::dsp::FFT`
- Simple desktop GUI built with JUCE
- Output file prefix support
- Selectable output directory
- Optional **maintain relative level between files** mode
- Runtime logging for JUCE assertions and app diagnostics
- Windows dark-titlebar support

## Typical use cases

- Apply room, hall, plate, or spring reverb IRs to dry recordings
- Run speaker, cabinet, or microphone-chain impulse responses on source material
- Process batches of related assets such as stems, multisamples, or dialogue takes
- Use measured IRs produced by the **Farina Exponential Sine Sweep (ESS)** method
- Explore sound-design textures by convolving arbitrary WAV files together

## How it works

The processing engine performs frequency-domain convolution:

1. Load the input WAV and IR WAV
2. Zero-pad both signals to the required FFT length
3. Transform both with the FFT
4. Multiply the spectra bin-by-bin
5. Inverse transform back to the time domain
6. Normalize the rendered result

This follows the same core pattern used in the **Phase C deconvolution** stage documented in `Docs/farina_algorithm_coding_reference.md`.

## Requirements

- Windows
- CMake **3.22** or newer
- A C++20-capable compiler
- Visual Studio generator support for **Visual Studio 18 2026**
- JUCE installed locally, by default at:
  - `C:\JUCE`

If JUCE is installed somewhere else, pass `-DJUCE_DIR=<path>` when configuring CMake.

## Build

From the repository root:

```powershell
cmake -B build -G "Visual Studio 18 2026" -A x64
cmake --build build --config Debug
```

The project is configured in `CMakeLists.txt` as a standalone JUCE GUI app.

## Run

### Recommended

Use the provided runner script so runtime logs and JUCE assertions are captured:

```powershell
.\tools\agent\run_capture_assertions.ps1
```

With an explicit executable path or custom timeout:

```powershell
.\tools\agent\run_capture_assertions.ps1 -ExePath .\build\Convolver_artefacts\Debug\Convolver.exe -TimeoutSeconds 15
```

### Typical executable locations

Depending on how CMake was invoked, the executable is usually found in one of these locations:

- `build\Convolver_artefacts\Debug\Convolver.exe`
- `build\Convolver_artefacts\Release\Convolver.exe`
- `out\build\x64-debug\Convolver_artefacts\Debug\Convolver.exe`
- `out\build\x64-release\Convolver_artefacts\Release\Convolver.exe`

## Using the app

1. Click **Add** to choose one or more source WAV files
2. Click **Select** in **With this file** to choose the IR WAV
3. Click **Select** in **Output Path** to choose the destination folder
4. Optionally change **Output Prefix**
5. Decide whether to keep **Maintain relative level between files** enabled
6. Click **Convolve**

### Maintain relative level between files

When enabled:

- all files are convolved first
- the loudest rendered result determines a shared normalization gain
- the same gain is applied to every output
- level relationships between files are preserved

When disabled:

- each rendered file is normalized independently
- outputs may end up similarly loud even if the inputs were not

## Repository layout

```text
.
├─ CMakeLists.txt                     # Build definition
├─ Source/                            # Application source
│  ├─ Main.cpp                        # JUCE app entry point and runtime logger setup
│  ├─ MainComponent.*                 # Main GUI and batch workflow
│  ├─ ConvolutionEngine.*             # FFT-based WAV convolution engine
│  ├─ HelpWindow.*                    # In-app help window
│  ├─ DarkLookAndFeel.*               # Custom UI styling
│  └─ AppIcon.h                       # Application icon
├─ Docs/
│  ├─ farina_algorithm_coding_reference.md
│  └─ farina_ess_measurement_method.md
├─ tools/agent/                       # Build/run helper scripts and logs
└─ .github/copilot-instructions.md    # Repository-specific automation guidance
```

## Logs and troubleshooting

The application creates a runtime log next to the executable:

- `<exe_dir>\convolver_runtime.log`

The runner script also copies and stores logs here:

- `tools\agent\run.log`
- `tools\agent\logs\run_stdout.log`
- `tools\agent\logs\run_stderr.log`
- `tools\agent\logs\cdb_*.log` when `cdb.exe` is available

Useful things to search for in logs:

- `JUCE Assertion failure in`
- `Unhandled exception`
- `terminate called`
- `exception:`

## Notes

- This project is focused on **offline** processing, not real-time plug-in use
- Generated build artifacts should not be edited by hand
- The current repository does not define automated tests

## Further reading

- `Docs/farina_algorithm_coding_reference.md`
- `Docs/farina_ess_measurement_method.md`
- JUCE documentation: https://docs.juce.com/master/index.html
