# KickForge

Kick drum synthesizer plugin (VST3 + Standalone) built with [JUCE 8](https://juce.com).
Pick a genre preset — Techno, Hard Techno, Hardstyle, EDM or Trance — then sculpt every
part of the kick.

## Features

- **Three-layer architecture** : **attack** (filtered noise click), **punch** (dual
  oscillator body with exponential pitch envelope and its own drive) and **crunch**
  (sustained saturated tail with a dedicated distortion) — each with its own level,
  envelope and waveform display.
- Oscillators : sin / tri / square / saw with PolyBLEP anti-aliasing, optional
  detunable second oscillator.
- Full processing chain : drive (soft/hard/fold, 2× oversampled), multimode SVF
  filter, 3-band fully parametric EQ with interactive curve, parallel distortion
  (tube/fuzz/bitcrush), chorus, reverb (send fed by attack+punch only — the crunch
  stays dry), macro compressor with GR meter, final clipper, mid/side width with
  mono sub below 150 Hz.
- 5 genre presets, musical randomizer, per-layer waveform visualisation, built-in
  play button and preview loop, offline WAV export (48 kHz / 24-bit).
- Real-time safe : no allocation, locks or I/O on the audio thread.

## Building

Requires CMake ≥ 3.22 and a C++20 compiler. JUCE is fetched automatically via CPM.

```sh
# Linux (deps: alsa-lib, freetype, fontconfig, X11/Xext/Xrandr/Xcursor/Xinerama, mesa GL)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Windows (Visual Studio 2022)
cmake -B build
cmake --build build --config Release
```

Artefacts land in `build/KickForge_artefacts/Release/` (`VST3/` and `Standalone/`).
Run the unit tests with `ctest --test-dir build`. CI validates both platforms with
[pluginval](https://github.com/Tracktion/pluginval) at strictness 10.

## License

KickForge is released under the **GNU AGPLv3** (see `LICENSE`), the license required
by the JUCE 8 open-source tier. © Bebop Tech.
