#!/usr/bin/env python3
"""Independent true-peak and loudness check of a WAV file (does not share any code with
SKJALD). True peak is measured two ways:

  - "bs1770": 4x polyphase oversampling with a 48-tap-per-phase Kaiser FIR (in the spirit of
    ITU-R BS.1770-4 Annex 2, but more accurate than its example filter), and
  - "ideal": 16x band-limited (FFT/zero-padding) reconstruction of the whole file.

Usage: python3 verify_true_peak.py <file.wav> [--ceiling dB] [--tolerance dB] [--ideal-tolerance dB]
Exit code 1 if a ceiling is given and the measured true peak exceeds it by more than the
tolerance (BS.1770-style measurement) or the ideal tolerance (ideal reconstruction).
"""
import argparse
import sys
import numpy as np
from scipy.io import wavfile
from scipy import signal


def to_float(x):
    if x.dtype == np.int16:
        return x.astype(np.float64) / 32768.0
    if x.dtype == np.int32:
        return x.astype(np.float64) / 2147483648.0
    return x.astype(np.float64)


def tp_polyphase(x, factor=4, taps_per_phase=48):
    h = signal.firwin(factor * taps_per_phase + 1, 1.0 / factor, window=("kaiser", 9.0)) * factor
    peak = 0.0
    for ch in x.T:
        y = signal.upfirdn(h, ch, up=factor)
        peak = max(peak, np.max(np.abs(y)))
    return peak


def tp_ideal(x, factor=16):
    peak = 0.0
    for ch in x.T:
        n = len(ch)
        spec = np.fft.rfft(ch)
        y = np.fft.irfft(spec, n * factor) * factor
        peak = max(peak, np.max(np.abs(y)))
    return peak


def k_weight(fs):
    # BS.1770 K-weighting for any sample rate (same analog parameters as the 48 kHz reference)
    f0, G, Q = 1681.974450955533, 3.999843853973347, 0.7071752369554196
    K = np.tan(np.pi * f0 / fs)
    Vh, Vb = 10 ** (G / 20), (10 ** (G / 20)) ** 0.4996667741545416
    a0 = 1 + K / Q + K * K
    b1 = [(Vh + Vb * K / Q + K * K) / a0, 2 * (K * K - Vh) / a0, (Vh - Vb * K / Q + K * K) / a0]
    a1 = [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    f0, Q = 38.13547087602444, 0.5003270373238773
    K = np.tan(np.pi * f0 / fs)
    a0 = 1 + K / Q + K * K
    b2, a2 = [1, -2, 1], [1, 2 * (K * K - 1) / a0, (1 - K / Q + K * K) / a0]
    return (b1, a1), (b2, a2)


def integrated_lufs(x, fs):
    (b1, a1), (b2, a2) = k_weight(fs)
    z = signal.lfilter(b2, a2, signal.lfilter(b1, a1, x, axis=0), axis=0)
    block, step = int(0.4 * fs), int(0.1 * fs)
    energies = []
    for s in range(0, len(z) - block + 1, step):
        energies.append(np.sum(np.mean(z[s:s + block] ** 2, axis=0)))
    e = np.array(energies)
    if e.size == 0:
        return -np.inf
    loud = -0.691 + 10 * np.log10(np.maximum(e, 1e-20))
    e = e[loud >= -70]
    if e.size == 0:
        return -np.inf
    gate = -0.691 + 10 * np.log10(np.mean(e)) - 10
    e2 = e[(-0.691 + 10 * np.log10(e)) >= gate]
    return -0.691 + 10 * np.log10(np.mean(e2))


def db(v):
    return 20 * np.log10(max(v, 1e-12))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("wav")
    ap.add_argument("--ceiling", type=float, default=None)
    ap.add_argument("--tolerance", type=float, default=0.02)
    ap.add_argument("--ideal-tolerance", type=float, default=0.15)
    args = ap.parse_args()

    fs, x = wavfile.read(args.wav)
    x = to_float(x)
    if x.ndim == 1:
        x = x[:, None]

    sp = db(np.max(np.abs(x)))
    tp4 = db(tp_polyphase(x))
    tpi = db(tp_ideal(x))
    lufs = integrated_lufs(x, fs)
    print(f"{args.wav}: sample peak {sp:.3f} dBFS, true peak (4x poly) {tp4:.3f} dBTP, "
          f"true peak (16x ideal) {tpi:.3f} dBTP, integrated {lufs:.2f} LUFS")

    if args.ceiling is not None:
        ok = tp4 <= args.ceiling + args.tolerance and tpi <= args.ceiling + args.ideal_tolerance
        print(f"ceiling {args.ceiling:.2f} dBTP: {'PASS' if ok else 'FAIL'} "
              f"(excess {tp4 - args.ceiling:+.3f} dB / {tpi - args.ceiling:+.3f} dB)")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
