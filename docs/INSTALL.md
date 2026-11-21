# Installing SKJALD

## Installers (recommended)

Each release has an installer per system (CI builds are unsigned, see the notes below):

| System | File | What it does |
|---|---|---|
| Windows | `SKJALD-<version>-Windows-x64-Setup.exe` | Installs `SKJALD.vst3` to `C:\Program Files\Common Files\VST3` and `skjald_offline.exe` to `C:\Program Files\SKJALD`. Windows SmartScreen may warn because the installer is not code-signed: choose *More info > Run anyway*. |
| macOS | `SKJALD-<version>-macOS-universal.pkg` | Installs the plug-in to `/Library/Audio/Plug-Ins/VST3` and `skjald_offline` to `/usr/local/bin`. Not notarised: right-click the package and choose *Open* the first time. |
| Linux (Debian, Ubuntu) | `SKJALD-<version>-Linux-x86_64.deb` | `sudo apt install ./SKJALD-<version>-Linux-x86_64.deb` installs to `/usr/lib/vst3` and `/usr/bin`. |
| Linux (other) | `SKJALD-<version>-Linux-x86_64.tar.gz` | Unpack and run `./install.sh` (current user, `~/.vst3`) or `sudo ./install.sh --system`. |

## Manual install

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
