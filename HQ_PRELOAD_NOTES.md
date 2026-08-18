# HQ preload included

This project contains `hq_preload/` in addition to the connected Amp20 prototype.

Important:

- The currently connected amp remains `dsp/amp/AmpEngine` so the first Amp-only validation path is not changed.
- `hq_preload/` contains the higher-quality DSP redesign: 4x nonlinear oversampling helpers, improved amp core, 9 differentiated pedals, dynamics, modulation, delay and reverb.
- Do not connect everything at once. Validate the existing Amp20 path first, then replace/integrate modules incrementally.
