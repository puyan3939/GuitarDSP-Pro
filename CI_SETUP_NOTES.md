# CI setup notes

The project now supports three JUCE discovery methods:

1. `-DGUITARDSP_JUCE_PATH=/path/to/JUCE` (preferred for CI)
2. a repository-local `JUCE/` submodule
3. the original Raspberry Pi layout at `~/JUCE`

The GitHub Actions workflow intentionally builds the current connected Amp20 application only. The `hq_preload/` tree is staged future DSP and is not yet linked into the audio callback, so it is not allowed to destabilise the current Amp-only validation build.
