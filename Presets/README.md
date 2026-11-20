# Presets

Factory presets are compiled into the plug-in (`Source/Presets/FactoryPresets.cpp`) so they can never go missing:

- **EQ (9):** Clean Master, Warm Master, Bright Master, Low-End Control, Mud Removal, Air and Presence, Gentle Mastering, Modern Metal EQ, Rock Master EQ
- **Maximizer (11):** Transparent Master, Gentle Limiting, Modern Loudness, Loud Metal, Modern Metal, Punchy Rock, Heavy Rock, Electronic Master, Acoustic Master, Smooth Master, High Loudness
- **Global (8, EQ + Maximizer):** Clean Master, Warm Master, Rock Master, Metal Master, Loud Metal Master, Electronic Master, Acoustic Master, Streaming Safe

User presets are `.skjaldpreset` XML files (see docs/INSTALL.md for the folders). Example:

```xml
<SKJALDPRESET name="My Master" category="2" version="1">
  <PARAM id="mode" value="1"/>
  <PARAM id="threshold" value="-6.5"/>
  <PARAM id="ceiling" value="-1"/>
</SKJALDPRESET>
```

`category`: 0 = Global, 1 = EQ, 2 = Maximizer. Values are in real units (dB, Hz, ms, %, choice index). Missing parameters take their default values.
