# MIYAVI / complex multi-rig architecture

This branch is based on `agent/signal-analyzer`, not the older `main` snapshot. The goal is to preserve the newest JVM measurement work, Factory IR cabinet system, analyzer/measurement tools and latest dynamics while adding the routing features required by split guitar/bass-style rigs.

## Source-derived MIYAVI architecture vs inference

Public rig descriptions and the user's supplied board/article references support a split-rig approach: octave-down can feed a bass amplifier while the guitar path uses distortion/fuzz/overdrive and separate guitar amplification. They also support use of octave/pitch, TS-style overdrive, extreme fuzz, compressor, switching and spatial effects. They do **not** establish the exact studio settings or exact pedal order for `Dancing With My Fingers`; preset values remain tuning hypotheses.

## Implemented architecture

- MAIN / CLEAN / SUB parallel buses.
- Independent nonlinear pedal state on MAIN, CLEAN and SUB.
- Per-pedal routing mask: MAIN=1, CLEAN=2, SUB=4, including multi-destination routing.
- CLEAN high-headroom amp role with HP/LP, Bass/Mid/Treble, Presence, Drive, delay and polarity.
- SUB fixed octave-down into a dedicated bass amp role with HP/LP, Body, Bass/Mid/Treble, Drive, pitch Tracking/Tone/Smooth, delay and polarity.
- Automatic main/clean alignment against the octave path, plus manual path delays and polarity inversion.
- Expression pitch / Whammy-style +/-24 semitone processing with route selection.
- Stereo dual delay with independent L/R time/feedback and cross-feedback.
- Eight performance scenes.
- Stereo mix or physical stem output (`OUT1 MAIN+CLEAN`, `OUT2 SUB`).
- Pickup/cable/input-impedance loading model ahead of gain and fuzz.
- Analyzer taps retained at input, post-pedals, post-amp, post-cab and output.

## Preset compatibility

Preset schema v5 adds routing, performance and factory-IR identity while retaining all v4 fields. Loading a pre-v5 preset explicitly disables multi-rig, input loading, expression pitch and dual delay, restores stereo output, and sends all pedals to MAIN. This is intentional so old presets keep the old serial topology instead of inheriting state from a previously loaded v5 preset.

Factory cabinet presets now store both the external path and `factoryIrIndex`. The index is resolved through `FactoryIrCatalog`, so a preset can find the same measured Factory IR after moving between machines/install paths.

## Initial Dancing With My Fingers tuning direction

The first multibus experiment validated the usefulness of the SUB octave/bass path but its distortion character was not close enough. The revised starting point therefore keeps the SUB architecture while replacing the old Legacy/fuzz-only assumption with:

- MAIN: TS-style mid overdrive -> dense silicon fuzz -> current measured/fitted JVM410H OD1 model -> measured Factory IR.
- CLEAN: low-drive clean boost into the high-headroom clean role to retain metallic string attack independently from the high-gain MAIN path.
- SUB: mild preconditioning -> octave down -> bass amp role.
- Current HQ Guitar Compressor on MAIN with a slower attack and parallel blend to keep the first transient.

The selected Factory IR in the initial preset is an audition starting point only; no source establishes it as MIYAVI's actual cabinet capture.
