#!/usr/bin/env python3
"""Generate reference WAV files for offline / independent testing of SKJALD.

Writes 32-bit float stereo files into the given directory:
  sine_1k_-23dBFS.wav      EBU-style 1 kHz calibration tone (should read -23.0 LUFS)
  isp_fs4_45deg.wav        fs/4 sine with 45 degree phase: +3.01 dB inter-sample peaks
  drums_bass_mix.wav       synthetic kick/snare/hats/bass/pad mix peaking at 0 dBFS
  pink_noise_bursts.wav    pink noise bursts with silence (gating / transients)
  clipped_noise.wav        hard-clipped white noise (worst case for true peak)

Usage: python3 make_test_signals.py <output dir> [sample rate]
"""
import sys
import os
import numpy as np
from scipy.io import wavfile
from scipy import signal


def save(path, data, fs):
    wavfile.write(path, fs, np.asarray(data, dtype=np.float32))
    print("wrote", path)


def main():
    out = sys.argv[1] if len(sys.argv) > 1 else "test-signals"
    fs = int(sys.argv[2]) if len(sys.argv) > 2 else 48000
    os.makedirs(out, exist_ok=True)
    rng = np.random.default_rng(1)

    t = np.arange(int(fs * 20)) / fs
    tone = 10 ** (-23 / 20) * np.sin(2 * np.pi * 1000 * t)
    save(os.path.join(out, "sine_1k_-23dBFS.wav"), np.stack([tone, tone], 1), fs)

    t = np.arange(int(fs * 5)) / fs
    isp = 0.7071 * np.sin(2 * np.pi * fs / 4 * t + np.pi / 4)
    # 50 ms silent lead-in: a tone that starts at full level on sample 0 makes any processed
    # file whose latency has been trimmed begin mid-way through the (linear-phase) onset, and
    # the truncation itself then reads as a false inter-sample over
    isp[: int(0.05 * fs)] = 0.0
    save(os.path.join(out, "isp_fs4_45deg.wav"), np.stack([isp, isp], 1), fs)

    t = np.arange(int(fs * 12)) / fs
    tb = np.mod(t, 0.5)
    kick = np.sin(2 * np.pi * (50 + 120 * np.exp(-tb * 30)) * tb) * np.exp(-tb * 9)
    snare_env = np.where((np.floor(t / 0.5) % 2) == 1, np.exp(-tb * 16), 0.0)
    snare = (rng.standard_normal(t.size) * 0.5) * snare_env
    hats = rng.standard_normal(t.size) * np.exp(-np.mod(t, 0.25) * 60) * 0.2
    bass = 0.35 * np.sin(2 * np.pi * 55 * t)
    pad = 0.08 * (np.sin(2 * np.pi * 220 * t) + np.sin(2 * np.pi * 277.18 * t) + np.sin(2 * np.pi * 329.63 * t))
    left = 0.9 * kick + 0.6 * snare + hats + bass + pad
    right = 0.9 * kick + 0.6 * snare + 1.2 * hats + bass + 0.9 * pad
    mix = np.stack([left, right], 1)
    mix /= np.max(np.abs(mix))
    save(os.path.join(out, "drums_bass_mix.wav"), mix, fs)

    n = int(fs * 10)
    white = rng.standard_normal(n)
    b, a = [0.049922035, -0.095993537, 0.050612699, -0.004408786], [1, -2.494956002, 2.017265875, -0.522189400]
    pink = signal.lfilter(b, a, white)
    gate = (np.mod(np.arange(n) / fs, 1.0) < 0.6).astype(float)
    pink = pink / np.max(np.abs(pink)) * 0.9 * gate
    save(os.path.join(out, "pink_noise_bursts.wav"), np.stack([pink, pink * 0.8], 1), fs)

    noise = np.clip(rng.standard_normal((int(fs * 5), 2)) * 0.6, -0.5, 0.5)
    save(os.path.join(out, "clipped_noise.wav"), noise, fs)


if __name__ == "__main__":
    main()
