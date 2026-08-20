# JVM410H OD1 measurement coverage

GuitarDSP-Pro keeps measured and non-measured control axes explicit.

## Public measurement-backed axes

The reference is based on the public Marshall JVM410H OD1 data used by the DAFx23 grey-box work and the ToneTwisT AFx re-publication/pre-processing.

- Channel/mode: OD1 only
- Bass: measured parametric axis
- Middle: measured parametric axis
- Treble: measured parametric axis
- Gain: measured parametric axis (ToneTwisT/NablAFx uses B/M/T/G conditioning)

## Fixed in the available public JVM recordings

These controls are held at the reference operating point in the public data and must not be described as measurement-fitted away from that point:

- Channel Volume: 10
- Master: 5
- Presence: 5
- Resonance: 5
- Other JVM channels/modes: not present in this measurement set

GuitarDSP-Pro may expose physical-model movement away from the fixed point, but those values remain **unverified** until matching multi-axis measurements become available.

## Validation rule

A parameter is promoted to "measured" only when the calibration workflow has both fit examples and unseen/holdout examples for that axis. A lower training error without holdout improvement is not sufficient.
