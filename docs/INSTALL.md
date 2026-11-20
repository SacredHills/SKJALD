# Installing SKJALD

Copy `SKJALD.vst3` (a folder/bundle, copy it whole) into the VST3 folder of your system, then rescan plug-ins in your DAW. SKJALD appears as **SacredHills › SKJALD** in the *Fx / Mastering* category.

| System | Folder (all users) | Folder (current user) |
|---|---|---|
| Windows | `C:\Program Files\Common Files\VST3\` | – |
| macOS | `/Library/Audio/Plug-Ins/VST3/` | `~/Library/Audio/Plug-Ins/VST3/` |
| Linux | `/usr/lib/vst3/` or `/usr/local/lib/vst3/` | `~/.vst3/` |

### macOS: unsigned builds

CI builds are ad-hoc signed, not notarised. If macOS blocks the plug-in after downloading, remove the quarantine flag:

```bash
xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/SKJALD.vst3
```

### User presets

User presets are stored as `.skjaldpreset` (XML) files in:

- Windows: `%APPDATA%\SKJALD\Presets\{Global,EQ,Maximizer}\`
- macOS: `~/Library/SKJALD/Presets/...`
- Linux: `~/.config/SKJALD/Presets/...`

The preset menu has an *Open user preset folder* entry.

### Offline processor

`skjald_offline` (included in the packages) processes WAV files from the command line with the same DSP as the plug-in. Run `skjald_offline --help`.

### Uninstall

Delete `SKJALD.vst3` from the VST3 folder (and the `SKJALD` preset folder if you want to remove user presets).
