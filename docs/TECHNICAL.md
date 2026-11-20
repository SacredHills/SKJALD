# SKJALD technical documentation

This document records the DSP design decisions, the measured accuracy and the known limitations. All figures come from the automated tests (`Tests/`), the plug-in host tests and `scripts/verify_true_peak.py`.

## 1. Architecture

```
            ┌──────────────────────── MasteringEngine (pure C++20, no JUCE) ───────────────────────┐
 input ──┬─►│ Equalizer (8 bands, 0 latency) ─► Maximizer (constant latency L) ─► meters │─► mix ─► output
         │  └──────────────────────────────────────────────────────────────────────────────────────┘    ▲
         └────────────────────────────── delay L (latency-compensated global bypass) ─────────────────────┘
```

- `Source/DSP`, `Source/Metering`, `Source/Engine` form the `skjald_dsp` static library. It has no JUCE dependency and no GUI code, and it is shared by the plug-in, the tests and the offline tool, so what is tested is exactly what runs in the DAW.
- The plug-in (`Source/Plugin`) only converts parameters to an `EngineParams` struct (lock-free atomic reads, once per block), calls the engine and exposes meters.
- The GUI reads meters through atomics and two lock-free single-producer/single-consumer FIFOs (spectrum samples, gain-reduction points). The audio thread never blocks, allocates, locks or touches files. When the GUI does not read, FIFO data is dropped.
- The engine processes any host block size in internal chunks of 256 samples, so all buffers are allocated in `prepare()`. The tests prove output is bit-identical for block sizes 1, 7, 64, 333, 512, 1024 and 4096.
- Denormals: FTZ/DAZ is enabled for the processing scope (SSE CSR on x86, FPCR on ARM64), as well as `juce::ScopedNoDenormals`.
- 64-bit host buffers are converted to 32-bit through a preallocated buffer. Filter state and coefficient maths in the EQ, the loudness meter and the box-filter running sums are kept in double precision.

## 2. Equalizer

**Filter design.** Every band is designed in the analog domain and converted with a *magnitude-matched* method. The poles are impulse invariant, and the numerator is solved so the digital magnitude equals the analog one at DC, at the band frequency and at Nyquist (the approach published by M. Vicanek, 2016). The numerator is then reflected to minimum phase. Compared with the bilinear transform (RBJ cookbook), this removes the "cramping" of bells and shelves near Nyquist. The test `EQ/No cramping near Nyquist` measures the maximum deviation from the analog prototype over 20 Hz to Nyquist at 44.1 kHz:

| Bell | matched | bilinear |
|---|---|---|
| 16 kHz +9 dB Q1 | 1.08 dB | 6.47 dB |
| 18 kHz +12 dB Q0.7 | 1.98 dB | 10.80 dB |
| 10 kHz −12 dB Q0.5 | 0.74 dB | 6.82 dB |
| 5 kHz +6 dB Q0.5 | 0.32 dB | 1.33 dB |
| 2 kHz −9 dB Q1 | 0.05 dB | 0.14 dB |

The remaining deviation is inherent to a 2nd-order section whose analog response extends past Nyquist. The graph always draws the true digital response, computed by the same function that designs the audio filters.

Further decisions:

- **Cuts and shelves.** Deep cuts, low-shelf cuts and high-shelf boosts are designed as the exact inverse of the mirrored boost (`H₋g = 1/H_g` for these prototypes). The poles therefore never move above the band frequency, where impulse invariance would alias.
- **Filter types and slopes.** Available types are bell, low and high shelf, high and low pass, and notch. Slopes are 6, 12, 18, 24, 36 and 48 dB/oct. High and low passes use Butterworth sections, and Q scales the highest-Q section, so Q = 0.707 gives a pure Butterworth. Shelves above 12 dB/oct are a cascade of shelves with Butterworth-distributed Q and gain split by section order, which keeps the half-gain point at the band frequency for every slope. With a 6 dB/oct slope, shelves and passes are first-order and have no Q.
- **Measured accuracy.** Bell centre gain is within ±0.05 dB from 20 Hz to 20 kHz at 44.1 to 192 kHz for every gain and Q. High- and low-pass corners are at −3.01 dB, and asymptotic slopes are within 0.6 dB/oct of nominal. 86,400 designed sections were all stable.
- **Filter structure.** Direct Form I in double precision.
- **Smoothing.** Frequency is smoothed in log2, gain in dB and Q in log over 50 ms, with coefficients recomputed every 16 samples. Type, slope and enable changes crossfade from the old to the new cascade over 30 ms, and EQ bypass crossfades over 20 ms. No clicks: test `EQ/Parameter smoothing`.
- **Stereo.** Linked parameters with independent filter state per channel.
- **Latency.** Zero (minimum phase).

## 3. Maximizer

### 3.1 Signal flow

```
drive (input gain − threshold, smoothed)
  → leveler (RMS, base rate; algorithm dependent)
  → transient detector (attack-ness per sample)
  → upsample 1x..16x (halfband cascade)
      → clipper (optional / built-in) + transient-selective clipper
      → detector (|x| or true-peak reconstruction) → required gain r = ceiling / peak (soft knee optional)
      → stereo link → sliding minimum (attack window) → program-dependent release
      → 1-3 cascaded moving averages (attack shape) → × input delayed by a fixed amount
  → downsample → latency pad
  → true-peak guard (output verification, ~1 ms lookahead)
  → ceiling clamp (numerical safety net)
  → auto gain compensation / delta / bypass mix
```

**Ceiling guarantee.** Each moving-average output is an average of sliding-minimum values whose windows all contain the aligned sample. So the smoothed gain is never above the gain that sample requires, and the ceiling holds by construction rather than by clipping. The final clamp only absorbs float rounding. The ceiling is carried as a per-sample stream through the same delays as the audio, so ceiling automation stays aligned.

**Threshold and ceiling.** Threshold works like the classic "drive into the ceiling" control. The signal is raised by −threshold dB (plus input gain) and limited at the ceiling, so a lower threshold means more limiting.

### 3.2 The five algorithms

They differ in structure, not only in parameter values (`Source/DSP/Maximizer/LimiterModes.h`):

| | Transparent | Modern | Aggressive | Punch | Smooth |
|---|---|---|---|---|---|
| Detector | peak | peak + RMS leveler | peak + RMS leveler | peak + RMS leveler with slow attack + transient detector | peak with 2 dB soft knee + gentle leveler |
| Attack shape (share of lookahead) | 2-stage S-curve (100 %) | 2-stage (75 %) | linear, 1 stage (35 %) | 2-stage (30 %): onset not pre-ducked | 3-stage near-Gaussian (100 %) |
| Release | 2nd-order (S-shaped) | 1st-order, 0.8× | 1st-order, 0.5× | 1st-order | 2nd-order, 1.5× |
| Leveler (threshold below ceiling / ratio / attack / release / max GR) | – | 9 dB / 1.5 / 20 ms / 250 ms / 3 dB | 11 dB / 2 / 10 ms / 150 ms / 4.5 dB | 10 dB / 1.7 / 45 ms / 120 ms / 3.5 dB | 6 dB / 1.25 / 50 ms / 500 ms / 1.5 dB |
| Clipping | optional | optional | built-in (ceiling +1 dB) | transient-selective (60 %) | optional (soft, +2.5 dB) |
| Auto release base | 150 ms | 100 ms | 50 ms | 80 ms | 300 ms |

Test `Limiter/Algorithms are genuinely different`: all outputs differ pairwise by more than −40 dB residual. Test `Transient emphasis and Punch preserve drum attacks`: the transient-to-body ratio (input 6.6 dB) is 2.1 dB with Transparent, and 4.2 dB with both Punch and Transparent + 100 % Transient Emphasis, at the same threshold.

### 3.3 Controls

- **Release.** Manual (5 to 1000 ms) or Auto (algorithm base time). *Adaptive release* compares the gain reduction currently applied with the 400 ms average of the *demanded* gain reduction. A short burst recovers up to 2.5× faster (measured: 290 ms instead of 522 ms after an isolated 6 dB peak), while sustained demand stretches the release up to 2× to avoid low-frequency modulation distortion and pumping.
- **Lookahead (0 to 10 ms).** Sets the attack window. The signal delay is fixed at the 10 ms maximum, so changing lookahead never changes latency. Window changes are slewed one sample at a time without dropping any detector value.
- **Stereo link (0 to 100 %).** A convex blend of each channel's required gain towards the minimum of both, so it is never louder than unlinked. *Split* link mode keeps the sustained (leveler) stage fully linked so the image is stable, and applies the knob only to the peak stage.
- **Transient Emphasis (0 to 100 %).** Raises the amount of transient-selective clipping: only the tips of detected attacks are soft-clipped (oversampled) instead of being gain-reduced over the whole window. It also shortens the attack window by up to 60 % and slows the leveler attack up to 3× so hits pass the sustained stage.
- **Soft Clip.** Oversampled tanh clipper ahead of the peak stage; the level depends on the algorithm.
- **Delta.** Outputs (driven input − limited output), sample-aligned. The test shows wet + delta reconstructs the driven input to within 6e-8.
- **Auto Gain Compensation.** K-weighted, 3 s, attenuation only. It matches the output loudness to the input for level-fair A/B comparison (measured: −20.82 LUFS in, −20.78 LUFS out). Because it only attenuates, it can never exceed the ceiling.
- **Bypass.** Maximizer bypass and global bypass crossfade over 20 ms to the input, delayed by the full latency, so they are latency compensated. Error against the delayed input is below 1e-6. The host's bypass button is mapped to the global bypass.

### 3.4 Oversampling

The oversampler is a cascade of linear-phase halfband FIR stages (Kaiser window, 100 dB). It passes up to 0.4535·fs (20 kHz at 44.1 kHz) within ±0.001 dB and stops from 0.5465·fs. The round-trip latency is padded to an integer number of base samples, and every factor is padded to the longest one, so **latency is identical for every setting**.

Switching factors starts the new path in parallel, lets it fill, then crossfades over 10 ms, with no gap and no click (test `Oversampling switch mid-stream`).

### 3.5 True peak

- **Core detection.** At 1x to 4x, an interpolating detector reconstructs at least 8x of the base rate. At 8x and 16x the oversampled samples themselves are used.
- **Output guard.** After decimation the limiter's harmonics are filtered away, which can create new inter-sample overs. The guard measures the decimated output with two detectors and uses the larger reading:
  1. an 8x halfband cascade whose transition band is only 0.495 to 0.505·fs, accurate to within 0.01 dB for content up to 0.49·fs;
  2. a short interpolator with its cutoff 12 % above Nyquist, which deliberately *over*-reads content closest to Nyquist (the safe side).

  It then applies a fully linked 1 ms lookahead correction with the same min/average guarantee. Ceiling margins are 0.03 dB for the core and 0.01 dB for the guard.
- **Metering.** The meter uses the same halfband reconstruction at ≥ 352.8 kHz (8x at 44.1 or 48 kHz, 4x at 88.2 or 96 kHz, 2x at 176.4 or 192 kHz). That exceeds the BS.1770-4 Annex 2 example filter.

**Measured tolerance.** Ceiling −1 dBTP, all 5 algorithms × 5 oversampling factors × threshold −6 and −18 dB, synthetic drum/bass/noise mix:

| Measurement | Worst excess over the ceiling |
|---|---|
| SKJALD BS.1770 true-peak meter | −0.005 dB |
| Independent long-kernel ideal reconstruction (C++ test, separate code) | +0.025 dB |
| Same material low-passed at 18 kHz, ideal reconstruction | ≤ +0.02 dB |
| `verify_true_peak.py`, 4x polyphase (scipy), drum mix and pink-noise bursts | ≤ +0.02 dB |

Sample-peak mode keeps the sample peak at or below the ceiling exactly, tested at every factor and algorithm.

**Limitation (documented, not hidden).** Content in the last ~1 % below Nyquist cannot be separated from its mirror image by any finite reconstruction filter. For *full-band hard-clipped white noise* driven 10 dB into the limiter (a pathological signal), the output reads up to +0.12 dB over the ceiling with scipy's 4x filter and +0.18 dB with an ideal FFT reconstruction, while BS.1770-style meters with narrower filters read at the ceiling. For delivery to platforms that measure with very wide filters, a −1 dBTP ceiling (the common recommendation) leaves ample headroom.

### 3.6 Latency

Latency is constant for all parameter values and is reported at `prepareToPlay` (an estimate for 48 kHz is reported before preparation).

| Sample rate | Latency |
|---|---|
| 44.1 kHz | 880 samples (19.95 ms) |
| 48 kHz | 923 samples (19.23 ms) |
| 88.2 kHz | 1365 samples (15.48 ms) |
| 96 kHz | 1451 samples (15.11 ms) |
| 176.4 kHz | 2335 samples (13.24 ms) |
| 192 kHz | 2507 samples (13.06 ms) |

It is made up of the 10 ms maximum lookahead, the true-peak detector, the oversampler (padded to the 16x path) and the output guard. The host tests verify an impulse emerges exactly at the reported latency.

## 4. Loudness and metering

- **Loudness.** ITU-R BS.1770-4 / EBU R128. K-weighting coefficients are derived for any sample rate and equal the published 48 kHz values to 1e-8. Momentary uses a 400 ms window, short-term 3 s, both updated every 100 ms. Integrated uses 400 ms blocks with 75 % overlap, an absolute gate at −70 LUFS and a relative gate at −10 LU. Block energies are kept in an exact-energy 0.01 LU histogram: constant memory, no allocation, unlimited duration.
- **Test results.** A 1 kHz tone at −23 dBFS reads −23.0 LUFS (M, S, I) at 44.1 to 192 kHz. EBU Tech 3341 cases 3, 4 and 5 read −23.0 ± 0.1 LUFS. Silence is gated, and blocks straddling a tone/silence edge are counted exactly as the standard defines.
- **Peak meters.** Sample-peak meters on input and output. True-peak hold and integrated loudness are reset by clicking the read-outs; resets are applied by the audio thread through an atomic flag.
- **Gain-reduction history.** Per-chunk peak and average gain reduction (leveler + limiter + guard) is sent over a lock-free FIFO and drawn over 6 s with an auto-ranging scale.

## 5. Parameters, state, presets, A/B

- **Parameters.** 67 automatable parameters (`Source/Plugin/Parameters.cpp`). Parameter IDs are versioned (`ParameterID {id, 1}`).
- **State.** Stores the parameters, both A/B snapshots, the editor size and tab, and the current preset names. The host test verifies a full round trip of all 67 parameters through a hosted VST3.
- **A/B.** Two full snapshots (all parameters except global bypass). Switching applies the other snapshot through the normal parameter path, so all changes are smoothed.
- **Presets.** Factory presets are compiled in: 9 EQ, 11 Maximizer and 8 Global. User presets are XML files. Loading a preset resets its section to defaults first, so presets are complete and reproducible. Bypass and delta switches are not part of presets.

## 6. Performance

Measured on the 2-core cloud VM used for development (a desktop CPU is typically 2 to 4 times faster), 48 kHz stereo, 8 EQ bands active, true peak on:

| Oversampling | % of one core |
|---|---|
| 1x | ~8 % |
| 4x (default) | ~7–8 % |
| 16x | ~14 % |

Run `skjald_offline --benchmark` for figures on your machine.

## 7. Known limitations

- **Formats.** VST3 only; AU and AAX are not built. JUCE supports adding them in `Source/Plugin/CMakeLists.txt` if needed.
- **Channels.** Mono and stereo only; surround layouts are rejected.
- **EQ phase.** The EQ is minimum phase; there is no linear-phase mode.
- **Oversampling switch CPU.** While switching oversampling factors (about the latency plus 10 ms), both paths run, so CPU briefly doubles.
- **Near-Nyquist true peak.** See section 3.5.
- **Platform testing.** Linux builds and passes every test and pluginval locally. Windows and macOS builds are produced and tested by CI. DAW compatibility beyond pluginval and the host tests (for example specific Pro Tools, Logic or Ableton versions) has not been tested manually.
