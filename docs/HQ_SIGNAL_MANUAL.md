# GuitarDSP-Pro HQ signal manual

## Signal order

`Input -> Dynamics -> Pedal slots 1..4 -> Amp (Legacy or HQ) -> Modulation -> Delay -> Reverb -> Safety limiter -> Output`

All newly integrated effects default to **off/bypass**. The default amp remains **Legacy Amp20**, so the first Raspberry Pi test stays comparable to the prepared Amp-only build.

## Amp A/B

- **Legacy Amp20**: use this first on hardware.
- **HQ Amp20**: nonlinear valve/power stages use 4x oversampling. Stage parameters include HPF, LPF, Drive, Bias, Asymmetry, Memory and Output.

The 20 stages are not 20 distortion knobs. Coupling, filtering, supply, feedback and transformer stages are separate because the location of bandwidth/level changes determines what later nonlinear stages generate.

## Pedals

Four pre-amp slots are available and start disabled. Models are Clean Boost, Treble Booster, Mid Overdrive, Transparent OD, Hard Distortion, Germanium Fuzz, Silicon Fuzz, Octave Fuzz and Velcro Fuzz. Each model has model-specific character controls rather than a single generic Drive/Tone layout.

## Dynamics

Noise Gate: Threshold / Range / Attack / Hold / Release / Hysteresis.
Studio Compressor: Threshold / Ratio / Attack / Release / Knee / Makeup / Mix.
Guitar Compressor: Sustain / Attack / Blend / Level.

## Modulation / delay / reverb

Modulation includes Chorus, Flanger, Phaser, Tremolo and Vibrato. Delay includes Digital, Analog, Tape and Ping-Pong. Reverb includes Room, Plate, Hall and Spring. They all start disabled.

## Hardware validation order

1. Legacy Amp20 only.
2. HQ Amp20 A/B at matched output level.
3. One pedal slot: Clean Boost or Mid OD.
4. Hard Distortion, Germanium Fuzz, Octave Fuzz.
5. Guitar Compressor, then Studio Compressor.
6. Chorus.
7. Digital Delay.
8. Room Reverb.
9. Only then combine blocks.

Record CPU, sample rate, buffer size, xruns/dropouts, input/output peak, noise floor and subjective notes. Never judge a model as better only because it is louder.
