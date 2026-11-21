# Development log

## 2026-11-20: Initial implementation

**Environment.** Ubuntu 24.04 (cloud VM, 2 cores), GCC 13.3, CMake 3.28, JUCE 8.0.10. Windows and macOS are built and tested by GitHub Actions.

### Milestone 1-2: foundation and EQ
- Separated the DSP into a JUCE-free static library (`skjald_dsp`) so the tests and the offline tool exercise the same code as the plug-in.
- EQ filter design: chose magnitude-matched design (impulse-invariant poles with a 3-point numerator fit) over the bilinear transform. This avoids high-frequency cramping, with the deviation from the analog prototype 4-9 times smaller.
- **Bug found by tests:** odd-order Butterworth Q values were wrong; fixed the pole-angle formula.
- **Bug found by tests:** deep bell cuts near Nyquist lost depth, because a direct numerator fit can't place the notch. They are now designed as the inverse of the mirrored boost.

### Milestone 3-5: maximizer
- Lookahead core: sliding minimum, then program-dependent release, then cascaded moving averages. This gives a structural ceiling guarantee.
- Oversampling: a halfband cascade with integer latency padding, verified with impulses (69/76/78/79 samples for 2/4/8/16x).
- **Finding:** short interpolation kernels under-read full-band material by up to 0.6 dB. Built a dedicated halfband-cascade true-peak detector and an output guard.
- **Finding:** a final clamp using the *current* ceiling clicks when the ceiling is automated, because the samples in the delay line were limited against an older ceiling. The ceiling now travels as a per-sample stream through every delay.
- **Finding:** attack-window changes caused gain discontinuities. Windows now slew one sample per sample, and skipped detector values are pushed explicitly.
- Oversampling factor changes: the new path warms up in parallel, then crossfades (no gap).
- **Adaptive release, first version, didn't work:** it compared the envelope with its own average, which follows it during the release. It now compares the envelope with the averaged *demand*. Measured result: 290 ms vs 522 ms recovery after an isolated burst.
- **Transient emphasis / Punch.**
  - Attempt 1, fast release after transients: lowered the transient-to-body ratio, because the body came back up against a pinned peak.
  - Attempt 2, a pre-limiter attack booster: only added loudness, because the limiter took the boost back.
  - Final: transient-selective oversampled clipping. Transient-to-body ratio 2.1 → 4.2 dB at equal threshold, verified by test.

### Milestone 6: metering
- BS.1770-4 meter with an exact-energy histogram for gating, verified against the K-weighting reference coefficients and the EBU Tech 3341 cases. One test expectation was wrong: blocks straddling the tone/silence edge legitimately count.

### Milestone 7: interface
- Fixed-design-size layout, scaled uniformly for resizing. Custom look and feel for every widget.
- EQ graph draws curves with the same `FilterDesign::designBand` used for the audio.
- Headless UI preview tool renders PNGs for review (hosted VST3 editors can't be snapshotted from the host process).

### Milestone 8: testing and release
- **Plug-in host tests** load the built VST3: latency at 6 sample rates, impulse alignment, mono, random block sizes, 64-bit, state round trip, random automation, CPU, editor.
  - Test fixes: VST3 parameter changes reach the plug-in on the next block, and Delta and Maximizer Bypass legitimately output unlimited audio.
- **pluginval** passes at strictness 10.
- **Independent Python verification** (`scripts/verify_true_peak.py`) found residual overs of about 0.1 dB from energy within 1.5 % of Nyquist.
  - The guard detector's transition was narrowed to 0.495-0.505 fs, and a second, deliberately over-reading near-Nyquist detector was added.
  - The remaining worst case (hard-clipped white noise) was documented in TECHNICAL.md §3.5; resolved in the second session (below).
- Folding symmetric halfband taps halved their multiplies, which brought CPU back to about 8 % of one core at the default 4x.

## 2026-11-20 (second session): true-peak guarantee, CI fix

The Linux CI job failed in the independent verification step: Aggressive mode on the drum mix read +0.086 dB over the ceiling with the script's 4x scipy filter. Investigation:

- **Two measurement bugs in the checker.** The "ideal" FFT reconstruction was circular, so the end of each file wrapped onto its start; this produced a false +0.48 dB on the fs/4 tone. The scipy 4x filter has its cutoff exactly at Nyquist and over-reads full-band material by up to 0.07 dB. The script now uses the ITU-R BS.1770-4 Annex 2 reference filter for compliance and a zero-padded, parabolically refined 16x FFT reconstruction; the scipy reading is informational.
- **Real overs remained** with honest meters: up to +0.07 dB (ideal) on pink-noise bursts, +0.18 dB on clipped noise, and up to +0.10 dB on the ITU meter, whose ripple reads above the ideal reconstruction.
- **Isolated the guard** with a harness (guard alone, fed limited audio +1 dB): it left +0.074 dB. At the over location its detector read −0.011 dB. Cause: the decimators' transition band folds clipping harmonics into the last few per cent below Nyquist, where no finite detector is accurate.
- **Fix, three parts.**
  1. A linear-phase band-limit (flat to 0.4535·fs, ≥ 100 dB from 0.4875·fs) in front of the guard.
  2. A ±8-sample neighbourhood in the guard's gain guarantee, so its own gain ramp cannot create overs.
  3. The ITU Annex 2 detector added to the guard's readings.
  After these, the guard alone still left +0.07 dB on clipped noise: **8x sampling granularity**, as a 20 kHz crest can fall 0.13 dB between oversampled points. Added parabolic refinement of local maxima in the guard's detector (and in both independent checkers, whose 16-point grids under-read by up to 0.03 dB themselves).
- **Result.** Every signal, algorithm, factor and sample rate tested is now at or below the ceiling on all three independent measurements (TECHNICAL.md §3.5). Latency grew by 109 samples at 48 kHz (now 1032). CPU is unchanged within noise (≈ 8 % of one core at 4x).
- **Remaining trace over (+0.02 dB on the fs/4 tone at 48/96 kHz) was a test artefact.** The tone started at full level on sample 0, so the latency-trimmed output began mid-onset. With a 50 ms lead-in it reads −0.045 dB. The signal generator now adds the lead-in.
- New tests:
  - true peak on clipped noise (ITU and ideal);
  - band-limit passband and stopband, symmetry and latency;
  - refined detector accuracy at 20 kHz over 16 phases;
  - the existing true-peak test tightened from +0.15 dB to +0.02 dB (ideal), with an ITU check added.

  CI also verifies clipped noise at every oversampling factor. 45 DSP tests (1232 checks), plug-in host tests and pluginval (strictness 10) pass.

### Open items
- Manual testing in commercial DAWs (Windows/macOS) beyond pluginval and the host tests.
- Code signing and notarisation for macOS distribution; installer packages.
- Choosing a project licence, which is required by the JUCE licence terms (see README).

## Maximizer distortion and Ozone-style rework

See TECHNICAL.md section 3.x. Added Character parameter, rebuilt clipper, transient detector, leveler and release behaviour. 45 DSP tests, pluginval 10 and the 64-run true-peak matrix (modes x signals x Character 0/5/10, plus oversampling 1/2/8/16) all pass.

## Threshold-Ceiling Link and Learn Threshold

LEARN / APPLY with Target LUFS and the LINK toggle added to the Maximizer tab. 47 DSP tests, plugin host tests and pluginval 10 pass.

## Leveler removed

User reported it sounded like a compressor. The pre-limiter RMS leveler was the cause; disabled in all modes. Aggressive built-in clipper depth lowered to 0.25 dB (about -38 dB THD+N on a steady sine at the borderline drive).
