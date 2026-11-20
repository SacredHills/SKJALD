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
  - The remaining worst case (hard-clipped white noise) is documented in TECHNICAL.md §3.5.
- Folding symmetric halfband taps halved their multiplies, which brought CPU back to about 8 % of one core at the default 4x.

### Open items
- Manual testing in commercial DAWs (Windows/macOS) beyond pluginval and the host tests.
- Code signing and notarisation for macOS distribution; installer packages.
- Choosing a project licence, which is required by the JUCE licence terms (see README).
