# Farina ESS — Algorithmic Coding Reference
### Step-by-step instructions for implementing the measurement pipeline in C++ / JUCE

---

## Document Purpose

This document describes every computational step required to implement the Farina Exponential Sine Sweep (ESS) measurement. It is structured around **software objects** and the **order in which they are invoked** during a measurement session. No acoustics theory is included — only what the code must do, in what order, with what data.

The pipeline has four distinct execution phases:

```
PHASE A — Pre-computation  (runs once, before any audio)
PHASE B — Audio I/O        (runs during the measurement pass)
PHASE C — Deconvolution    (runs once, immediately after recording stops)
PHASE D — Analysis         (runs on demand, after deconvolution)
```

Each phase maps to one or more software objects. Data flows forward only — no phase reads data from a later one.

---

## Data Glossary

These names are used consistently throughout all phases.

| Name | Type | Description |
|------|------|-------------|
| `fs` | float | Sample rate in Hz (e.g. 48000.0f) |
| `f1` | float | Sweep start frequency in Hz (e.g. 20.0f) |
| `f2` | float | Sweep end frequency in Hz (e.g. 20000.0f) |
| `sweepDuration` | float | Sweep duration in seconds |
| `sweepSilenceDuration` | float | Silent tail duration in seconds |
| `sweepRate` | float | Sweep rate constant: `sweepDuration / ln(f2/f1)` |
| `N_sweep` | int | Samples in the sweep: `round(sweepDuration × fs)` |
| `N_silence` | int | Samples in the tail: `round(T_silence × fs)` |
| `N_total` | int | `N_sweep + N_silence` — total playback/record buffer length |
| `N_fft` | int | FFT size — next power of two ≥ `N_sweep + N_total - 1` |
| `sweep[]` | float[N_total] | The ESS signal followed by zeros |
| `inv_filter[]` | float[N_sweep] | The inverse (deconvolution) filter |
| `recorded[]` | float[N_total] | The captured system response |
| `h_full[]` | float[N_fft] | Full deconvolved buffer (output of IFFT) |
| `h1[]` | float[N_irwindow] | Extracted linear impulse response |
| `hN[]` | float[N_hwindow] | Extracted Nth-order harmonic distortion IR |

---

## PHASE A — Pre-computation

**Runs once when the user confirms parameters. Must complete before any audio starts.**  
**Responsible object: `SweepGenerator`**

---

### A.1 — Compute Derived Constants

From the user-supplied parameters `fs`, `f1`, `f2`, `sweepDuration`, `sweepSilenceDuration`, compute:

```
sweepRate = sweepDuration / ln(f2 / f1)
N_sweep   = round(sweepDuration × fs)
N_silence = round(sweepSilenceDuration × fs)
N_total   = N_sweep + N_silence
N_fft     = nextPowerOfTwo(N_sweep + N_total - 1)
```

Store all of these. They are needed in every subsequent phase.

---

### A.2 — Generate the ESS Buffer (`sweep[]`)

Allocate `sweep[]` of length `N_total`, initialised to zero.

For each sample index `n` from `0` to `N_sweep - 1`:

```
t       = n / fs
phase   = 2π × f1 × sweepRate × (exp(t / sweepRate) - 1)
sweep[n] = sin(phase)
```

**Critical:** Compute `phase` fresh from the formula for every sample. Do not accumulate it incrementally across iterations — floating-point drift over millions of samples will corrupt the sweep.

Samples `N_sweep` to `N_total - 1` remain zero (the silence tail).

---

### A.3 — Apply Fade-in Window

Apply a half-Hann (raised cosine) fade-in to the first `N_fadein` samples of `sweep[]`.

```
N_fadein = round(1.0 / f1 × fs)   // one full period of f1; e.g. 2400 samples at 48kHz for f1=20Hz

for n = 0 to N_fadein - 1:
    window = 0.5 × (1 - cos(π × n / N_fadein))
    sweep[n] *= window
```

This prevents a discontinuity at sample 0, which would otherwise smear energy across the entire spectrum.

---

### A.4 — Apply Fade-out Window

Apply a half-Hann fade-out to the last `N_fadeout` samples of the active sweep region (i.e. indices `N_sweep - N_fadeout` to `N_sweep - 1`).

```
N_fadeout = round(0.020 × fs)   // 20 ms is sufficient

for n = 0 to N_fadeout - 1:
    idx = N_sweep - N_fadeout + n
    window = 0.5 × (1 + cos(π × n / N_fadeout))
    sweep[idx] *= window
```

This prevents a step discontinuity at the end of the active portion, which would otherwise produce ripple at high frequencies.

---

### A.5 — Normalise Amplitude

Find the peak absolute value in `sweep[]` and scale the entire buffer so the peak is at `0.9` (leaving approximately 1 dB of headroom below full scale):

```
peak = max(|sweep[n]|) for n = 0 to N_sweep - 1
scale = 0.9 / peak
sweep[n] *= scale   for all n
```

---

### A.6 — Build the Inverse Filter (`inv_filter[]`)

Allocate `inv_filter[]` of length `N_sweep`.

**Step 1 — Time-reverse the sweep:**

```
for n = 0 to N_sweep - 1:
    inv_filter[n] = sweep[N_sweep - 1 - n]
```

Note: use only the active sweep region (`sweep[0]` to `sweep[N_sweep-1]`), not the silence tail.

**Step 2 — Apply amplitude compensation envelope:**

The ESS has unequal energy per Hz (more at high frequencies). The inverse filter must compensate by applying a decaying exponential envelope that reduces amplitude by 6 dB per octave across the reversed sweep:

```
for n = 0 to N_sweep - 1:
    inv_filter[n] *= exp(-n / (fs × sweepRate))
```

Because the reversed sweep starts at frequency `f2` and sweeps down, this envelope applies lower amplitude to the high-frequency end (start of the reversed signal) and higher amplitude to the low-frequency end (end of the reversed signal), resulting in a spectrally flat deconvolution output.

**Step 3 — Normalise the inverse filter:**

Scale `inv_filter[]` so that the peak amplitude is 1.0 (or any consistent reference). This controls the absolute amplitude of the recovered IR.

---

### A.7 — Self-Test (Recommended)

Before any hardware is involved, verify the inverse filter is correct by convolving `sweep[]` (active portion only, length `N_sweep`) with `inv_filter[]` in the frequency domain and checking that the result is a clean Dirac-like impulse.

If the result has spectral tilt or visible artefacts, the amplitude envelope in A.6 is wrong.

---

## PHASE B — Audio I/O

**Runs during the measurement. Controlled by the JUCE audio callback.**  
**Responsible object: `MeasurementAudioProcessor` (or equivalent AudioIODeviceCallback)**

---

### B.1 — Allocate the Record Buffer

Before audio starts, allocate `recorded[]` of length `N_total`, initialised to zero.

Set a write-head counter: `write_pos = 0`.

---

### B.2 — Audio Callback: Output (Playback)

In each audio callback, write the next block of samples from `sweep[]` into the output buffer:

```
for each sample i in current callback block:
    if play_pos < N_total:
        output[i] = (float) sweep[play_pos]
        play_pos++
    else:
        output[i] = 0.0f
```

Playback ends when `play_pos == N_total`.

---

### B.3 — Audio Callback: Input (Recording)

In the same callback, simultaneously read from the input buffer into `recorded[]`:

```
for each sample i in current callback block:
    if write_pos < N_total:
        recorded[write_pos] = (double) input[i]
        write_pos++
```

**Critical:** Output and input are serviced in the **same callback pass**. This guarantees sample-accurate synchronisation between playback and recording with zero offset, as long as a single full-duplex audio device is used.

---

### B.4 — Detect End of Measurement

When `play_pos == N_total` (all samples have been sent to output), signal the main thread that recording is complete. The callback should continue reading input until `write_pos == N_total` as well.

Once both conditions are met, the audio device can be stopped and Phase C begins.

---

## PHASE C — Deconvolution

**Runs once, immediately after recording completes. CPU-intensive — run on a background thread.**  
**Responsible object: `DeconvolutionEngine`**

---

### C.1 — Zero-pad the Recorded Signal

Create a zero-padded buffer `Y_padded[]` of length `N_fft`:

```
Y_padded[n] = recorded[n]   for n = 0 to N_total - 1
Y_padded[n] = 0.0           for n = N_total to N_fft - 1
```

---

### C.2 — Zero-pad the Inverse Filter

Create a zero-padded buffer `F_padded[]` of length `N_fft`:

```
F_padded[n] = inv_filter[n]   for n = 0 to N_sweep - 1
F_padded[n] = 0.0             for n = N_sweep to N_fft - 1
```

---

### C.3 — Forward FFT of Both Signals

```
Y[k] = FFT(Y_padded)     // complex array, length N_fft
F[k] = FFT(F_padded)     // complex array, length N_fft
```

Both FFTs use the same `N_fft`. Use the same FFT library (e.g. FFTW) for both.

---

### C.4 — Frequency-Domain Multiplication

For each bin `k` from `0` to `N_fft - 1`:

```
H[k] = Y[k] × F[k]          // complex multiplication, not division
```

This is standard linear convolution in the frequency domain. It is multiplication — not spectral division — because the inverse filter was analytically constructed to be the spectral inverse of the sweep.

---

### C.5 — Inverse FFT

```
h_full_complex[] = IFFT(H)
h_full[n] = Re(h_full_complex[n])    for n = 0 to N_fft - 1
```

Take only the real part. Discard the imaginary part (it should be near zero; any significant imaginary component indicates an FFT normalisation or buffer alignment error).

---

### C.6 — Understand the Output Buffer Layout

`h_full[]` now contains the complete deconvolved result. Its time structure is:

```
Index 0                              Index N_fft-1
|----|----|----|----|-----------------|
 hN   ...  h3   h2  [silence]  h1    [zero tail]
```

Reading from left to right in time:
- **Higher harmonic distortion IRs** appear first (earliest in time), starting near index 0.
- **Lower harmonic distortion IRs** follow.
- **The linear impulse response h1** appears last, near index `N_sweep - 1`.
- A zero/noise tail follows after the linear IR.

The linear IR is always the rightmost (latest) significant peak.

---

### C.7 — Locate the Linear IR Position

The theoretical position of the linear IR peak is:

```
n_linear = N_sweep - 1
```

In practice, confirm this by searching for the sample with the maximum absolute value within a window of ±500 samples around `N_sweep - 1`. Use that confirmed peak position for all subsequent windowing operations.

---

## PHASE D — Analysis

**Runs on demand after deconvolution. Can be re-run with different parameters without re-measuring.**  
**Responsible object: `AnalysisEngine`**

---

### D.1 — Extract the Linear Impulse Response (`h1[]`)

Define an extraction window of length `N_irwindow` starting at `n_linear`:

```
N_irwindow = desired IR length in samples (e.g. round(2.0 × fs) for a 2-second IR)

h1[n] = h_full[n_linear + n]    for n = 0 to N_irwindow - 1
```

**Guard:** Ensure `n_linear + N_irwindow - 1 < N_fft`. If not, shorten `N_irwindow`.

**Guard:** Ensure the start of the extraction window `n_linear` does not overlap the 2nd-order harmonic peak (see D.3 for its position). The linear IR extraction window must start to the right of all harmonic peaks.

---

### D.2 — Compute the Frequency Response of h1

```
H1[k] = FFT(h1)                          // length N_irwindow, or zero-padded to next power of two
magnitude_dB[k] = 20 × log10(|H1[k]|)
phase_rad[k]    = atan2(Im(H1[k]), Re(H1[k]))
```

Frequency axis:

```
freq[k] = k × fs / N_fft_analysis        // Hz per bin
```

---

### D.3 — Locate Harmonic Distortion IRs

For each harmonic order `N` (N = 2, 3, 4, ...):

```
delta_t_N   = T × ln(N) / ln(f2 / f1)          // seconds before the linear peak
delta_n_N   = round(delta_t_N × fs)             // samples before the linear peak
n_harmonic_N = n_linear - delta_n_N             // index in h_full[]
```

Numerical example for T=10s, f1=20Hz, f2=20kHz, fs=48000:

```
ln(f2/f1) = ln(1000) = 6.9078

N=2:  delta_t = 10 × 0.6931 / 6.9078 = 1.003 s  →  delta_n = 48144 samples
N=3:  delta_t = 10 × 1.0986 / 6.9078 = 1.590 s  →  delta_n = 76320 samples
N=4:  delta_t = 10 × 1.3863 / 6.9078 = 2.007 s  →  delta_n = 96336 samples
N=5:  delta_t = 10 × 1.6094 / 6.9078 = 2.330 s  →  delta_n = 111840 samples
```

Each harmonic peak is narrower than the linear IR. A safe extraction window is half the spacing between adjacent harmonic peaks, centred on `n_harmonic_N`.

---

### D.4 — Extract Each Harmonic IR (`hN[]`)

For harmonic order N, define an extraction window of length `N_hwindow` centred on `n_harmonic_N`:

```
// Compute spacing to adjacent harmonics to determine safe window size
spacing_to_prev = delta_n_(N-1) - delta_n_N     // samples to the lower-order harmonic
spacing_to_next = delta_n_N - delta_n_(N+1)     // samples to the higher-order harmonic
N_hwindow = min(spacing_to_prev, spacing_to_next) - safety_margin    // safety_margin e.g. 50 samples

half_win = N_hwindow / 2
start_idx = n_harmonic_N - half_win

hN[m] = h_full[start_idx + m]    for m = 0 to N_hwindow - 1
```

---

### D.5 — Compute Each Harmonic's Frequency Response

```
HN[k] = FFT(hN)                           // zero-pad to a power of two if needed
magnitude_N_dB[k] = 20 × log10(|HN[k]|)
```

**Important:** The Nth-order harmonic's frequency response is only valid up to `f2 / N` Hz, because the sweep only generated fundamentals up to `f2`, so the highest fundamental that could produce an Nth harmonic within the sweep range is `f2 / N`.

```
f_max_valid_N = f2 / N
```

Truncate or mask the display above this frequency.

---

### D.6 — Compute THD as a Function of Frequency

For each frequency bin `k`:

```
THD_linear[k] = sqrt( |H2[k]|² + |H3[k]|² + |H4[k]|² + ... ) / |H1[k]|
THD_dB[k]     = 20 × log10(THD_linear[k])
```

Note: The harmonic spectra `H2, H3, ...` have different valid frequency ranges (see D.5). Only combine bins where all contributing harmonics have valid data. In practice this means the THD curve is valid only up to `f2 / N_max` where `N_max` is the highest harmonic order used.

---

## Implementation Notes

### FFT Size and Memory

For a 10-second sweep at 48 kHz: `N_sweep = 480,000`, `N_total ≈ 624,000` (with 3s silence). This yields:

```
N_fft = nextPowerOfTwo(480000 + 624000 - 1) = nextPowerOfTwo(1,103,999) = 1,048,576 × 2 = 2,097,152
```

That is a 2^21 FFT. Each complex double array at that length is `2,097,152 × 16 bytes ≈ 33 MB`. At least three such arrays are needed simultaneously (Y, F, H), so budget approximately 100 MB for the deconvolution step. This is acceptable for a desktop application.

For long sweeps (20–30 s), `N_fft` reaches 2^22–2^23 and memory usage climbs proportionally. Consider exposing this as a user-visible trade-off: longer sweep → better SNR → more memory and longer computation time.

### FFT Normalisation Convention

Choose one convention and apply it consistently throughout. A safe convention is:

```
Forward FFT: no division (raw DFT sum)
Inverse FFT: divide by N_fft
```

This is the default in FFTW (`FFTW_FORWARD` / `FFTW_BACKWARD` with manual 1/N scaling after the inverse). Do not mix conventions between the FFT of Y and the FFT of F.

### Data Precision

- Signal buffers used for audio I/O and DSP with JUCE (`sweep[]`, `inv_filter[]`, `recorded[]`, `h_full[]`) are `float` to match JUCE `dsp::FFT` and `dsp::Convolution` performance and memory characteristics.
- If you require higher numerical fidelity for analysis or archival storage, keep double-precision copies only for offline analysis and plots (convert to/from `float` where needed).
- FFT intermediate buffers: JUCE's `dsp::FFT` is float-based. For double-precision FFTs use an external library (FFTW, vDSP) and keep buffers as `double` for the deconvolution path if required.
- Final display/plot data (magnitude in dB, frequency axis): `float` is generally sufficient.

### Thread Safety

- Phase A runs on the main/UI thread (before audio starts). It writes `sweep[]` and `inv_filter[]`.
- Phase B runs on the audio thread. It reads `sweep[]` (read-only after Phase A) and writes `recorded[]`.
- Phase C runs on a dedicated background thread after Phase B completes. It reads `recorded[]` and `inv_filter[]`, writes `h_full[]`.
- Phase D runs on the main/UI thread. It reads `h_full[]` (read-only after Phase C) and writes display buffers.

No two phases overlap in time for the same buffer. No mutex is needed as long as this ordering is enforced with a simple state machine.

### Multithreading Within Phase C

The two forward FFTs in C.3 (`FFT(Y_padded)` and `FFT(F_padded)`) are independent and can run concurrently on separate threads. The IFFT in C.5 must wait for both to complete. A simple implementation using `std::async` or JUCE's `ThreadPool` suffices.

---

## Phase Dependency Diagram

```
 User sets parameters
        │
        ▼
┌───────────────┐
│   PHASE A     │  SweepGenerator
│  Pre-compute  │  ─ compute constants
│               │  ─ generate sweep[]
│               │  ─ apply fades
│               │  ─ build inv_filter[]
└──────┬────────┘
       │  sweep[] ready (read-only from here)
       │  inv_filter[] ready (read-only from here)
       ▼
┌───────────────┐
│   PHASE B     │  MeasurementAudioProcessor
│   Audio I/O   │  ─ stream sweep[] → output
│               │  ─ capture input → recorded[]
└──────┬────────┘
       │  recorded[] complete
       ▼
┌───────────────┐
│   PHASE C     │  DeconvolutionEngine (background thread)
│ Deconvolution │  ─ FFT(recorded) × FFT(inv_filter)
│               │  ─ IFFT → h_full[]
└──────┬────────┘
       │  h_full[] ready (read-only from here)
       ▼
┌───────────────┐
│   PHASE D     │  AnalysisEngine
│   Analysis    │  ─ extract h1[], hN[]
│               │  ─ compute spectra, THD
└───────────────┘
       │
       ▼
    Display / Plot
```

---

## Checklist for Verification

Use this checklist to validate each phase before proceeding to the next.

**After Phase A:**
- [ ] `sweep[0]` is approximately 0 (fade-in working)
- [ ] `sweep[N_fadein]` is approximately `sin(2π × f1 × N_fadein/fs)` at full amplitude
- [ ] `sweep[N_sweep-1]` is approximately 0 (fade-out working)
- [ ] `sweep[N_total-1]` is exactly 0.0 (silence tail)
- [ ] Convolving `sweep[]` with `inv_filter[]` produces a single clean peak (self-test A.7)
- [ ] Peak of the self-test result is at sample index `N_sweep - 1`
- [ ] The self-test result is spectrally flat (±3 dB) across `[f1, f2]`

**After Phase B:**
- [ ] `recorded[]` has non-zero values (signal was captured)
- [ ] No clipping (no samples at ±1.0 full scale)
- [ ] The visible waveform in `recorded[]` resembles a time-stretched version of the sweep (visually similar shape, possibly louder or quieter)

**After Phase C:**
- [ ] `h_full[]` contains a clearly visible impulse near index `N_sweep - 1`
- [ ] Regions well before that index may show smaller harmonic peaks
- [ ] The region after the main peak decays to the noise floor
- [ ] The imaginary part of the IFFT output is near zero (< -80 dB relative to peak)

**After Phase D:**
- [ ] `h1[]` starts at or very near the main peak
- [ ] Frequency response of `h1[]` is reasonable for the system under test
- [ ] Harmonic peaks are located at the predicted sample positions (D.3 formula)
- [ ] THD values are physically plausible (< 10% for most audio equipment)
