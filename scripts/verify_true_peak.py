#!/usr/bin/env python3
"""Independent true-peak and loudness check of a WAV file (does not share any code with
SKJALD). True peak is measured three ways:

  - "itu":   the ITU-R BS.1770-4 Annex 2 reference meter (4x, 48-tap polyphase filter with the
             coefficients published in the Recommendation). This is the compliance measurement.
  - "ideal": 16x band-limited reconstruction of the whole file, computed by FFT on a
             zero-padded copy (linear, not circular: without padding the end of the file wraps
             onto its start and the step creates a false over).
  - "poly":  4x scipy Kaiser polyphase FIR with its cutoff exactly at Nyquist (informational
             only: images of content just below Nyquist leak through it, so it over-reads
             full-band material relative to the ideal reconstruction).

Usage: python3 verify_true_peak.py <file.wav> [--ceiling dB] [--tolerance dB] [--ideal-tolerance dB]
Exit code 1 if a ceiling is given and the ITU true peak exceeds it by more than the tolerance,
or the ideal reconstruction exceeds it by more than the ideal tolerance.
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


# ITU-R BS.1770-4 Annex 2, Table 1: 4x oversampling interpolation filter, one row per phase
ITU_PHASES = np.array([
    [0.0017089843750, 0.0109863281250, -0.0196533203125, 0.0332031250000, -0.0594482421875, 0.1373291015625,
     0.9721679687500, -0.1022949218750, 0.0476074218750, -0.0266113281250, 0.0148925781250, -0.0083007812500],
    [-0.0291748046875, 0.0292968750000, -0.0517578125000, 0.0891113281250, -0.1665039062500, 0.4650878906250,
     0.7797851562500, -0.2003173828125, 0.1015625000000, -0.0582275390625, 0.0330810546875, -0.0189208984375],
    [-0.0189208984375, 0.0330810546875, -0.0582275390625, 0.1015625000000, -0.2003173828125, 0.7797851562500,
     0.4650878906250, -0.1665039062500, 0.0891113281250, -0.0517578125000, 0.0292968750000, -0.0291748046875],
    [-0.0083007812500, 0.0148925781250, -0.0266113281250, 0.0476074218750, -0.1022949218750, 0.9721679687500,
     0.1373291015625, -0.0594482421875, 0.0332031250000, -0.0196533203125, 0.0109863281250, 0.0017089843750],
])


def tp_itu(x):
    peak = 0.0
    for ch in x.T:
        for ph in ITU_PHASES:
            peak = max(peak, np.max(np.abs(np.convolve(ch, ph))))
    return peak


def tp_ideal(x, factor=16, pad=16384):
    peak = 0.0
    for ch in x.T:
        n = len(ch)
        padded = np.concatenate([np.zeros(pad), ch, np.zeros(pad)])
        y = np.abs(np.fft.irfft(np.fft.rfft(padded), len(padded) * factor) * factor)
        # parabolic refinement of every local maximum (16x alone can miss a crest near
        # Nyquist by ~0.03 dB)
        a, b, c = y[:-2], y[1:-1], y[2:]
        den = a - 2 * b + c
        m = (b >= a) & (b >= c) & (den < 0)
        off = np.where(m, 0.5 * (a - c) / np.where(m, den, -1.0), 0.0)
        v = np.where(m, b - 0.25 * (a - c) * off, b)
        peak = max(peak, np.max(v), np.max(y))
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
    ap.add_argument("--ideal-tolerance", type=float, default=0.05)
    args = ap.parse_args()

    fs, x = wavfile.read(args.wav)
    x = to_float(x)
    if x.ndim == 1:
        x = x[:, None]

    sp = db(np.max(np.abs(x)))
    tpu = db(tp_itu(x))
    tpi = db(tp_ideal(x))
    tp4 = db(tp_polyphase(x))
    lufs = integrated_lufs(x, fs)
    print(f"{args.wav}: sample peak {sp:.3f} dBFS, true peak ITU {tpu:.3f} dBTP, ideal {tpi:.3f} dBTP, "
          f"scipy-4x {tp4:.3f} dBTP (info), integrated {lufs:.2f} LUFS")

    if args.ceiling is not None:
        ok = tpu <= args.ceiling + args.tolerance and tpi <= args.ceiling + args.ideal_tolerance
        print(f"ceiling {args.ceiling:.2f} dBTP: {'PASS' if ok else 'FAIL'} "
              f"(excess ITU {tpu - args.ceiling:+.3f} dB, ideal {tpi - args.ceiling:+.3f} dB)")
        return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
