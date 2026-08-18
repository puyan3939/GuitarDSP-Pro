# GuitarDSP-Pro HQ DSP Preload

This package is a **quality-focused DSP redesign** intended to sit beside the current Phase 1 Amp-only build until the basic application has been validated on the Raspberry Pi.

It does not claim to beat Helix/Stadium before listening tests. The point of this package is to remove obvious architectural quality limits before A/B testing.

## Quality policy

- Nonlinear amp/pedal paths: 4x oversampling by default.
- Per-model pre-emphasis and post-emphasis rather than one generic waveshaper.
- Dynamic memory in tube stages and power supply.
- Real feedback path for NFB/damping instead of a post-EQ approximation.
- Fractional delay interpolation for modulation/delay.
- Reverb families have different topologies/behaviour, not just different decay presets.
- No heap allocation or coefficient-object creation in the per-sample nonlinear inner loop.
- Expensive coefficient updates belong in prepare/setParameters, not processSample.

## Files

- `dsp/common/HQDSP.h`
- `dsp/amp/AmpEngineHQ.*`
- `dsp/pedals/PedalEngineHQ.h`
- `dsp/dynamics/DynamicsHQ.h`
- `dsp/modulation/ModulationHQ.h`
- `dsp/delay/DelayHQ.h`
- `dsp/reverb/ReverbHQ.h`
- `docs/PARAMETER_MANUAL.md`
- `docs/HQ_REVIEW.md`
- `tests/QUALITY_TEST_PLAN.md`
