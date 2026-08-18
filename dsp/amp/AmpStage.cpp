#include "AmpStage.h"

void AmpStage::prepare(double sampleRate)
{
    fs = sampleRate;
    reset();
    updateCoefficients();
}

void AmpStage::reset()
{
    hpX1 = hpY1 = 0.0f;
    lpY1 = postLpY1 = 0.0f;
}

void AmpStage::setParameters(const AmpStageParameters& newParams)
{
    params = newParams;
    updateCoefficients();
}

void AmpStage::updateCoefficients()
{
    hpA = std::exp(-2.0f * juce::MathConstants<float>::pi * params.preHpHz / (float) fs);
    lpA = std::exp(-2.0f * juce::MathConstants<float>::pi * params.preLpHz / (float) fs);
    postLpA = std::exp(-2.0f * juce::MathConstants<float>::pi * params.postLpHz / (float) fs);
}
