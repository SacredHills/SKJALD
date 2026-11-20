# Building SKJALD

Requirements on every platform: CMake ≥ 3.22, a C++20 compiler and Git. JUCE 8.0.10 is downloaded automatically at configure time (CMake `FetchContent`). To use an existing JUCE checkout instead, pass `-DSKJALD_JUCE_PATH=/path/to/JUCE`.

CMake options:

| Option | Default | Meaning |
|---|---|---|
| `SKJALD_BUILD_PLUGIN` | ON | VST3 plug-in (needs JUCE) |
| `SKJALD_BUILD_TESTS` | ON | `skjald_dsp_tests`, and `skjald_plugin_tests` when the plug-in is built |
| `SKJALD_BUILD_TOOLS` | ON | `skjald_offline` WAV processor |
| `SKJALD_BUILD_UI_PREVIEW` | OFF | headless editor renderer (development) |
| `SKJALD_COPY_PLUGIN_AFTER_BUILD` | OFF | install the VST3 into the user plug-in folder after each build |
| `SKJALD_JUCE_PATH` | empty | local JUCE checkout |

The DSP library, tests and offline tool do not depend on JUCE; with `-DSKJALD_BUILD_PLUGIN=OFF` they build in seconds on any C++20 compiler.

## Windows (Visual Studio 2022, MSVC)

```bat
git clone https://github.com/SacredHills/SKJALD.git
cd SKJALD
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

Output: `build\Source\Plugin\SKJALD_artefacts\Release\VST3\SKJALD.vst3`. Open `build\SKJALD.sln` to work in the IDE.

## macOS (Xcode command-line tools or Xcode)

```bash
xcode-select --install          # if not installed
brew install cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
cmake --build build
ctest --test-dir build --output-on-failure
```

This produces a universal (Apple Silicon + Intel) bundle at `build/Source/Plugin/SKJALD_artefacts/Release/VST3/SKJALD.vst3`. For an Xcode project use `-G Xcode` and `--config Release`. The minimum deployment target is macOS 10.15. Builds are ad-hoc signed; for distribution, sign with a Developer ID and notarise.

## Linux (GCC or Clang)

```bash
sudo apt-get install build-essential cmake ninja-build git \
  libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev \
  libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure   # plug-in tests need a display: use xvfb-run on servers
```

Use `-DCMAKE_CXX_COMPILER=clang++` for Clang. Output: `build/Source/Plugin/SKJALD_artefacts/Release/VST3/SKJALD.vst3`.

## Continuous integration

`.github/workflows/build.yml` builds Windows x64, macOS universal and Linux x86_64 on every push, runs the DSP tests, the plug-in host tests and pluginval (strictness 10), runs the independent Python true-peak verification on Linux, and uploads zipped VST3 packages. Pushing a tag `vX.Y.Z` attaches the packages to a GitHub release.

## Useful commands

```bash
./build/Tests/skjald_dsp_tests                    # all DSP tests
./build/Tests/skjald_dsp_tests True-peak          # filter by name
./build/Tools/OfflineProcessor/skjald_offline --help
./build/Tools/OfflineProcessor/skjald_offline --benchmark 20
python3 scripts/make_test_signals.py signals 48000
python3 scripts/verify_true_peak.py processed.wav --ceiling -1
```
