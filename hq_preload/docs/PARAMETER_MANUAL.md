# HQ parameter intent

Parameters should expose **causes**, not duplicate generic tone knobs.

## Amp

- V1A/V1B Drive: early preamp saturation amount.
- Coupling HPF: how much low-frequency energy is allowed to hit the next nonlinear stage.
- Cold Clipper Drive/Bias: sharp asymmetric preamp clipping and harmonic balance.
- Recovery Drive: post-tone-stack makeup and saturation.
- PI Drive: phase-inverter breakup independently of power tube breakup.
- Sag: supply droop under power demand.
- Sag Recovery: how quickly supply voltage returns.
- Power Drive: EL34-style power-stage excitation.
- Bias X: dynamic bias excursion/blocking character under sustained drive.
- Damping/NFB: closed-loop power-stage tightness, not a final level control.
- Transformer Saturation: final magnetic saturation amount.

## Pedals

### Clean Boost
Boost / Low Cut / Bright / Level. Intended to change what hits the amp, not provide fuzz clipping.

### Treble Booster
Boost / Focus / Bite / Level. Aggressively removes low-frequency drive and emphasises upper mids.

### Mid Overdrive
Drive / Mid / Tone / Level. Mid-forward asymmetric clipping.

### Transparent Overdrive
Gain / Bass / Treble / Clean Mix / Level. Parallel clean path retains attack and low-level detail.

### Hard Distortion
Distortion / Tight / Presence / Filter / Level. Tight is a **pre-clipping** high-pass control; Filter is post-clipping.

### Germanium Fuzz
Fuzz / Bias / Tone / Volume. Bias is part of the nonlinear operating point and should audibly change asymmetry/cleanup.

### Silicon Fuzz
Fuzz / Scoop / Tone / Volume. Harder clipping and a deliberately independent mid-scoop control.

### Octave Fuzz
Fuzz / Octave / Focus / Tone / Volume. Octave controls the dedicated full-wave-rectified branch, not generic wet/dry.

### Velcro Fuzz
Fuzz / Starve / Gate / Tone / Volume. Starve alters the nonlinear supply/operating point; Gate controls sputter/decay behaviour.

## Dynamics

Studio Compressor exposes Threshold / Ratio / Attack / Release / Knee / Makeup / Mix because it is a measurement-friendly compressor.
Guitar Compressor exposes Sustain / Attack / Blend / Level because it is intended as a musical pedal control surface.

## Delay

Digital: Time / Feedback / HPF / LPF / Mix.
Analog: Time / Feedback / Drive / Age / Mix; repeats progressively saturate and darken.
Tape: Time / Feedback / Saturation / Wow / Flutter / Age / Mix.
Ping-pong: Time / Feedback / Width / Mix.

## Reverb

Room: Size / Decay / Damping / Predelay / Mix.
Plate: Decay / Brightness / Predelay / Mod / Mix.
Hall: Size / Decay / Damping / Mod / Mix.
Spring: Dwell / Tone / Drip / Decay / Mix.
