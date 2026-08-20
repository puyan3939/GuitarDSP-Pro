# MIYAVI rig analysis -> GuitarDSP-Pro capability target

This document separates **documented rig facts** from **GuitarDSP-Pro design inference**. It is not a claim that any one studio track used every listed device or exact setting.

## Documented rig facts used as constraints

Primary interview evidence:

- MIYAVI has described a live split in which one signal feeds a Marshall JCM800, another guitar role uses a Fender clean amp, and a drop-tuned feed goes to an Ampeg bass amp. He describes the bass amp as support for the guitar rather than the source of the characteristic guitar tone.
- He has described a switcher as important to preserve tone while bypassing unused pedals.
- He has publicly listed/mentioned POG2, OC-3, Eventide H9, Pete Cornish fuzz and Z.Vex Fuzz Factory among his tools.
- More recent MIYAVI pedal design commentary emphasizes fuzz **gate** behavior and a bright switch / booster edge for slap articulation.

These constraints explain why a single serial `pedals -> one amp -> one cab` chain can approximate the distortion character but cannot reproduce the same separation of attack, guitar-band distortion and octave-down physical weight.

## Acoustic/DSP interpretation

The target is not simply `more distortion`.

The perceptual result comes from several components surviving simultaneously:

1. **MAIN guitar band**: aggressive fuzz/OD and a mid-forward guitar amp/cab.
2. **CLEAN/ATTACK band**: transient and upper-mid string information that is not forced through the same heavy clipping transfer.
3. **SUB band**: an isolated -1 octave voice with its own bass-head/cab bandwidth and dynamics.
4. **Pitch gestures**: expression-controlled pitch, not just fixed octave voices.
5. **Switcher/scenes**: instantaneous topology changes are part of the instrument.
6. **Parallel timing/phase**: pitch shifters and convolution paths have different latency; summing without alignment can erase the exact brightness/body we are trying to preserve.
7. **Multi-output**: stage rigs can send the bass support to a physically separate amp/PA path.
8. **Source loading**: fuzz/boost response depends on guitar pickup, cable capacitance and input impedance; a fixed ideal voltage source cannot reproduce every vintage/interactive front-end behavior.

## Eight-part implementation target

### 1. True multi-amp

- MAIN: existing Legacy/HQ guitar amp + guitar cab.
- CLEAN: independent high-headroom clean combo model, with its own EQ/drive/cab voicing.
- SUB: independent octave-down bass head + bass cab voicing.
- Independent levels, polarity and delay for every path.

### 2. Pedal routing matrix

Each pedal slot needs a route mask instead of being globally serial:

- PRE SPLIT / MAIN / CLEAN / SUB destinations.
- A slot may feed more than one path.
- Stateful pedals require independent DSP state per destination; sharing one state object between parallel paths is invalid.

### 3. Expression Whammy

- Continuous ratio or semitone control (minimum +/-24 semitones).
- Expression smoothing to prevent zipper noise.
- Independent dry/wet, tracking/window, tone and smooth controls.
- Route destination selection.

### 4. Scene / switcher

- Multiple snapshots of enable states, routing, amp roles, pitch and mix values.
- Realtime-safe scene recall: no allocation or JSON parsing in the audio callback.
- Optional short parameter ramp/crossfade to avoid clicks.

### 5. Automatic latency compensation

- Each path reports algorithmic latency.
- Mixer delays lower-latency paths to the maximum active path latency.
- Keep manual trim for creative phase alignment after automatic compensation.

### 6. Eventide-style dual delay

- Two independent delay taps/engines.
- Independent time, feedback, pan, filter and modulation.
- Tempo ratio support belongs in a later transport/tempo layer; v1 can be millisecond based.

### 7. Multi-output

- Stereo Mix remains the safe default.
- A stem mode should allow MAIN/CLEAN on one physical output and SUB on another when at least two outputs exist.
- Future extension: arbitrary hardware channel map for interfaces with 4+ outputs.

### 8. Pickup / cable / input-impedance model

- Pickup source resistance + inductance.
- Cable capacitance.
- Input/load resistance.
- Resulting resonant peak and loading loss before boost/fuzz.
- Must be optional so existing presets remain compatible.

## Compatibility rule

All architectural features are opt-in. With Parallel Rig and input loading disabled, existing presets must retain the old signal path and behavior.

## Validation rule

A feature is not considered complete merely because it compiles. Tests should cover:

- digital silence stability,
- finite/bounded output,
- route isolation,
- scene recall determinism,
- latency compensation impulse alignment,
- multi-output stem isolation,
- input-impedance frequency-response directionality.
