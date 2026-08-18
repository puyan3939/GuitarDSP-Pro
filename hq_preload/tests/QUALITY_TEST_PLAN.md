# GuitarDSP-Pro HQ validation plan

Do not claim victory over a commercial modeller from source inspection alone.

## Objective tests

1. 1 kHz sine sweep through every nonlinear model; inspect harmonics and alias foldback at 44.1/48/96 kHz.
2. THD+N vs input level for each amp nonlinear stage and pedal.
3. Two-tone IMD (e.g. 440 Hz + 3 kHz) through nonlinear paths.
4. Amp step/burst test: measure sag drop and recovery.
5. NFB stability test at minimum/maximum damping and presence settings.
6. Compressor static I/O curve and attack/release timing test.
7. Delay modulation sideband test for zipper/interpolation artefacts.
8. Reverb impulse test: RT60 by band, early/late energy and metallic resonances.
9. CPU benchmark on Raspberry Pi 4B at 48 kHz / 64, 128, 256 samples.
10. xrun test while changing parameters rapidly.

## Listening tests

Reamp the same DI into GuitarDSP-Pro and the reference modeller.
Level-match within ~0.1 dB before blind comparison.
Test clean, edge-of-breakup, palm-muted high gain, chords, single notes, pick dynamics and guitar-volume cleanup.

Keep the DI and rendered WAV files in a test corpus so every DSP revision can be regression-tested.
