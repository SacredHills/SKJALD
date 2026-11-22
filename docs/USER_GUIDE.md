# SKJALD user guide

## Window

- **Header:** tabs **EQUALIZER | MAXIMIZER**, global presets, **A / B** settings and copy (A>B), **BYPASS** (global, latency compensated). Switching tabs never affects the sound.
- **Meters (right):** input and output peak meters, input/output peak hold, true peak, integrated and short-term loudness. Click the values to reset holds and integrated loudness.
- **Resizing:** drag the bottom-right corner; the whole interface scales.

## Equalizer

- **Nodes:** drag horizontally for frequency, vertically for gain. Mouse wheel over a node changes Q. Hold **Shift** for fine adjustment.
- **Double-click** a node to reset it, or empty space to add a band there.
- **Alt-click** a node to switch it on or off. **Right-click** for the type, slope, enable and reset menu.
- **Band panel:** select a band (1-8) to edit its on/off state, filter type, slope, frequency, gain and Q with knobs. Double-click a knob to reset it.
- **Toolbar:** EQ bypass, Reset EQ, analyzer mode (pre+post / post / off), display range (±6, ±12 or ±24 dB) and EQ presets.

## Maximizer

- **Algorithm:**
  - Transparent: clean, natural.
  - Modern: balanced commercial loudness.
  - Aggressive: dense, loud.
  - Punch: keeps kick and snare impact.
  - Smooth: gentle, for acoustic material.
- **Input Gain / Threshold:** drive into the limiter. Lowering the threshold raises the level and the amount of limiting.
- **Ceiling:** maximum output level, in dBTP when TRUE PEAK is on.
- **Release / AUTO:** manual release time, or automatic release based on the program material.
- **Stereo Link:** 0 % independent, 100 % fully linked.
- **Transient:** transient emphasis. More attack is kept by clipping only the tips of drum hits.
- **TRUE PEAK, Oversampling 1x-16x:** inter-sample peak control and processing quality.
- **ADVANCED:** lookahead, adaptive release, auto gain (level-matched A/B).
- **DELTA:** listen to what the limiter removes. **MAX BYPASS** bypasses the maximizer only.
- **Gain-reduction history:** the fill shows peak gain reduction and the line shows the average.

## Typical starting points

| Goal | Settings |
|---|---|
| Streaming master | Modern, ceiling −1 dBTP, lower the threshold until integrated loudness is near −14 LUFS |
| Loud rock/metal | Punch or Aggressive, Transient 20-40 %, 8x oversampling |
| Acoustic / classical | Smooth or Transparent, threshold −1 to −3 dB, release 200-400 ms |
