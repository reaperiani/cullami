# Cullami

Cullami is a VST3 and CLAP plug-in by **reaperiani** for checking whether a mix holds up under background noise.

## Controls

- `1`, `2`, `3`, `4`, `Cuffie`: choose one of the supplied stereo background loops.
- `Dry/Wet`: equal-power mix control. It does not measure, compress, or otherwise alter the dry signal's dynamics.
- `Output`: post-mix gain, default `-6 dB`.
- `Bypass Noise`: fades the background noise out or in over two seconds.
- `Safe Render`: mutes the background noise immediately and bypasses the audition output trim, preserving the dry signal at unity during export. It is off by default so normal listening remains active.

The five bundled WAV files are PCM stereo at 44.1 kHz. Cullami resamples them in real time for every host sample rate. Loops, source changes, and bypass transitions use two-second equal-power fades; host stop/play does not trigger a fade.

## Build

```sh
git submodule update --init --recursive
cmake -S . -B build
cmake --build build --config Release
```

Build output is written to `build/bin`. GitHub Actions produces Windows x64, macOS universal, and Linux x64 artifacts.
