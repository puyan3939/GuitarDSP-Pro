# GuitarDSP-Pro Amp20 Manual

## Why 20 stages?

The goal is not "more stages = more distortion". The goal is to expose *where* tone, headroom, asymmetry, compression and bandwidth change inside an amplifier.

A real amplifier is a chain of gain stages, coupling networks, impedance-loading networks, tone networks, phase inversion, power supply behaviour, power tubes, feedback and output magnetics. Two stages can both have a Drive control but sound different because the signal reaching them and the bandwidth around them are different.

The original 12-stage GuitarDSP model is kept as the tonal reference. New stages are inserted mostly as near-neutral shaping points, so the architecture can be explored without immediately changing the legacy sound.

---

## Stage map

### 01 Input DC / Rumble
**Role:** removes DC and subsonic movement before the amp.

**Main control:** Pre HPF.

**Listen for:** tighter handling noise and less speaker excursion. This stage should not normally create audible distortion.

### 02 Input Bandwidth
**Role:** defines the usable guitar bandwidth before the first valve model.

**Main controls:** Pre HPF / Pre LPF.

**Listen for:** overall openness versus a deliberately restricted vintage bandwidth.

### 03 Grid / Bright Entry
**Role:** represents the frequency shaping immediately before V1A.

**Main controls:** Pre HPF / Pre LPF / Output.

**Listen for:** pick attack and how much low-frequency energy is allowed to hit V1A.

### 04 V1A Triode
**Role:** first true voltage-gain/nonlinear stage.

**Main controls:** Drive / Bias / Nonlinear / Clip Shape.

**Listen for:** touch sensitivity and the transition from clean to edge-of-breakup. Increasing this stage changes what every later stage receives.

### 05 V1A Coupling
**Role:** coupling-capacitor/interstage low-frequency shaping after V1A.

**Main control:** Pre HPF.

**Listen for:** bass looseness versus tightness. Raising HPF here can allow more preamp distortion without low-frequency mud.

### 06 V1B Triode
**Role:** second voltage-gain stage.

**Main controls:** Drive / Bias / Clip Shape.

**Listen for:** denser preamp saturation than V1A. V1A and V1B should not be treated as two identical distortion pedals: V1B receives an already conditioned/nonlinear signal.

### 07 Interstage Low Cut
**Role:** determines what low-frequency content is allowed to hit the cold clipper.

**Main control:** Pre HPF.

**Listen for:** palm-mute definition and whether the next stage splats or stays controlled.

### 08 Cold Clipper
**Role:** deliberately asymmetric, low-headroom preamp clipping.

**Main controls:** Drive / Bias / Clip Shape.

**Listen for:** aggressive crunch and asymmetric harmonic structure. This is one of the strongest preamp character controls.

### 09 Cathode Follower
**Role:** buffer/driver stage between high-gain preamp and tone network.

**Main controls:** mild Drive / Output / LPF.

**Listen for:** compression and attack rather than large amounts of distortion. Keep Drive changes relatively small.

### 10 Tone Stack Entry
**Role:** conditions the signal entering the tone stack.

**Main controls:** HPF / LPF / Output.

**Listen for:** how hard the tone network is driven and the frequency range available to it.

### 11 Tone Stack Core
**Role:** dedicated Bass / Mid / Treble network.

**Main controls:** Bass / Mid / Treble global controls.

**Listen for:** broad spectral balance. Do not use this stage's Drive as the main distortion control; the dedicated tone-stack model will replace the generic stage core here.

### 12 Recovery Triode
**Role:** restores level lost in the tone stack and can add post-EQ valve saturation.

**Main controls:** Drive / Bias / Output.

**Listen for:** distortion *after* EQ. This sounds different from V1A/V1B because the spectrum has already been reshaped.

### 13 Master / Level
**Role:** controls how hard the phase inverter is driven.

**Main control:** Output.

**Listen for:** the distinction between preamp distortion and PI/power-amp distortion. Lower Master while increasing V1A/V1B for preamp-heavy tones; do the reverse to expose the power section.

### 14 PI Input Shaping
**Role:** coupling/bandwidth immediately before the phase inverter.

**Main controls:** HPF / LPF.

**Listen for:** whether the PI is hit by large low-frequency peaks and how open its top end is.

### 15 Phase Inverter
**Role:** phase-inverter nonlinear behaviour.

**Main controls:** Drive / Bias / Output.

**Listen for:** open, dynamic crunch that occurs later than preamp clipping. Use this when you want power-section character without simply increasing EL34 Drive.

### 16 Supply Sag
**Role:** reserved for dynamic power-supply voltage drop and recovery.

**Main control:** dedicated Sag control (future dedicated model).

**Listen for:** note attack followed by slight compression/drop under sustained loud playing, then recovery. This should be dynamic, not just another Drive stage.

### 17 Power Grid / Presence Feed
**Role:** shapes what reaches the power tubes and provides a location for presence/feedback interaction.

**Main controls:** HPF / LPF now; Presence later.

**Listen for:** low-end blocking behaviour and how aggressively high-frequency transients enter the power section.

### 18 EL34 Power Tubes
**Role:** main power-tube nonlinear stage.

**Main controls:** Drive / Bias / Output.

**Listen for:** thicker, broader saturation than the cold clipper. Compare this directly against Stage 08 to learn the difference between preamp and power-amp clipping.

### 19 NFB / Damping
**Role:** reserved for negative feedback and damping around the power section.

**Main control:** dedicated NFB/Damping control (future dedicated model).

**Listen for:** tighter/cleaner response with more feedback versus looser/more harmonically active response with less feedback.

### 20 Output Transformer
**Role:** final magnetic/bandwidth behaviour before the cabinet.

**Main controls:** LPF / mild Drive / Output.

**Listen for:** high-frequency smoothing and final power-section density. This should normally be subtle.

---

## Five stages to learn first

If twenty stages feel excessive, start here:

1. **04 V1A** — early touch-sensitive breakup.
2. **06 V1B** — accumulated preamp saturation.
3. **08 Cold Clipper** — aggressive asymmetric crunch.
4. **15 Phase Inverter** — late-stage open crunch.
5. **18 EL34 Power Tubes** — broad power saturation.

Change only one Drive at a time and level-match with that stage's Output. Loudness can otherwise make the more distorted setting appear automatically "better".

Then experiment with Stages 05 and 07. Those two demonstrate why a non-distorting stage can drastically alter distortion quality: changing the low-frequency content *before* clipping changes the harmonics generated by the nonlinear stage.

---

## Parameter meaning

### Pre HPF
High-pass frequency before the stage. Raising it removes low frequencies *before* nonlinear processing. This is one of the most important controls for distortion tightness.

### Pre LPF
Low-pass frequency before the stage. Lowering it prevents upper harmonics/noise from entering the nonlinear section.

### Drive
Signal gain into the nonlinear function. Drive only has a major conceptual role on actual gain/nonlinear stages.

### Bias
Moves the nonlinear operating point away from symmetry. It changes even-order harmonics and the way positive/negative waveform peaks clip.

### Nonlinear
Controls how strongly the stage departs from a linear transfer. At low settings the stage approaches a gain block; high values increasingly compress peaks.

### Clip Shape
Morphs between smoother saturation and a more abrupt clipping law.

### Post LPF
Bandwidth after nonlinear processing. This is different from Pre LPF: harmonics have already been generated, so lowering Post LPF removes generated high harmonics rather than preventing their generation.

### Output
Level leaving the stage. Use this for level matching when comparing Drive changes.

---

## Important workflow

Do not tune all 20 stages simultaneously.

- Pick the behaviour you want to study.
- Change one relevant stage.
- Match its output level.
- Bypass/compare.
- Then alter the coupling stage immediately before or after it.

This makes the amp an educational circuit-like signal chain instead of twenty arbitrary distortion knobs.
