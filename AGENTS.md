# Cullami Agent Notes

## Setup And Build

- DPF is a Git submodule with its own nested `pugl` submodule. Always run `git submodule update --init --recursive` before configuring a fresh checkout.
- Windows x64 uses the installed VS 2022 toolchain: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`, then `cmake --build build --config Release --parallel 2`.
- Outputs are `build/bin/Cullami.clap` and the complete `build/bin/Cullami.vst3/` bundle. For local DAW testing, copy both to `C:\vstplugins`; never copy only the binary inside the VST3 bundle.
- The DAW can retain the loaded DLL and cached parameter metadata. Close it completely, replace the binaries, rescan, and create a new instance when validating parameter metadata changes.
- CI builds VST3 and CLAP for Windows x64, macOS universal, and Linux x64 via `.github/workflows/build.yml`.
- There is no Cullami test suite or formatter target. The minimum focused verification is a Release build of both plugin formats followed by manual DAW checks.

## Code Map

- `src/CullamiPlugin.cpp` owns WAV decoding, sample-rate conversion, looping/crossfades, parameters, meters, and DSP.
- `src/CullamiUI.cpp` owns the NanoVG UI. Dry/Wet and Output are real `NanoSubWidget` controls backed by DPF `KnobEventHandler`.
- `src/CullamiParameters.hpp` is shared parameter ordering. Host projects persist these indices; do not reorder existing entries.
- `src/DistrhoPluginInfo.h` contains stable plugin identity. Do not change unique/brand/CLAP IDs unless intentionally creating an incompatible new plugin.

## DPF Gotchas

- `ParameterRanges` takes `(default, minimum, maximum)`, not `(minimum, maximum, default)`. Wrong ordering produces broken host normalization and apparently unusable knobs.
- A top-level UI override of `onMouse` must call `UI::onMouse(event)` first or child knobs never receive mouse press/drag events; wheel events may still work and hide this bug.
- Keep plugin/UI constructor defaults synchronized with `ParameterRanges::def`.
- The generated `CullamiWavData.hpp` must be included outside `START_NAMESPACE_DISTRHO`; it includes standard headers and including it inside the DPF namespace corrupts `std` on MSVC.

## Audio And Assets

- The root WAV files are embedded by `cmake/EmbedWavs.cmake`; edit the WAV sources, not `build/generated/CullamiWavData.hpp`.
- The loader intentionally accepts PCM 16-bit stereo at 44.1 kHz. Playback advances by `44100 / hostSampleRate` and uses linear interpolation.
- Embedded arrays are split every 16 bytes to stay below MSVC source-line limits. Do not collapse generated arrays onto single lines.
- `run()` is declared real-time safe: samples are decoded before processing; keep allocation, file I/O, and locks out of it.
- Dry/Wet is equal-power (`cos` dry, `sin` wet) and must not add level followers, compression, or other dynamics processing.
- Do not add an activation fade: host stop/play can reactivate the instance. Two-second fades are reserved for loop seams, source changes, and noise bypass transitions.
- `Safe Render` is manual because DPF does not expose uniform offline-render detection for VST3/CLAP. It is off by default and, when enabled, immediately returns dry input at unity, bypassing noise, Dry/Wet, and Output trim.
