# GuitarDSP-Pro CI

The GitHub Actions workflow builds the **currently connected Amp20 application** on Linux against multiple JUCE/compiler combinations.

## Matrix

- JUCE 8.0.15 + GCC
- JUCE 9.0.0 + GCC
- JUCE 9.0.0 + Clang

This is deliberately a compile/link gate first. The `hq_preload/` directory is not yet connected to `CMakeLists.txt`, so experimental HQ DSP cannot break the Amp-only validation build.

## Local Linux / Raspberry Pi check

```bash
GUITARDSP_JUCE_PATH=$HOME/JUCE ./ci/build-linux.sh
```

or:

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DGUITARDSP_JUCE_PATH=$HOME/JUCE
cmake --build build -j2
```

## What CI proves

- CMake configures against the selected JUCE version.
- Project source files compile on Linux.
- The application links successfully.
- GCC/Clang portability issues are detected.

## What CI does NOT prove

- Raspberry Pi ARM performance/xruns.
- WAVIO/ALSA device behaviour.
- Real audio quality.
- Stability of every parameter extreme.
- HQ preload compile status until those modules are integrated.

Those require dedicated DSP tests and Pi/reamp validation.
