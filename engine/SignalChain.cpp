#include "SignalChain.h"
#include <cmath>

void SignalChain::prepare(double sampleRate, int maximumBlockSize)
{
    inputGain.reset(sampleRate, 0.020);
    outputGain.reset(sampleRate, 0.020);
    startupGain.reset(sampleRate, 0.100);

    inputGain.setCurrentAndTargetValue(1.0f);
    outputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(-6.0f));
    startupGain.setCurrentAndTargetValue(0.0f);
    startupGain.setTargetValue(1.0f);

    ampEngine.prepare(sampleRate, maximumBlockSize);
    limiter.prepare(sampleRate);
    inputMeter.reset();
    outputMeter.reset();
}

void SignalChain::reset()
{
    ampEngine.reset();
    limiter.reset();
    inputMeter.reset();
    outputMeter.reset();
    startupGain.setCurrentAndTargetValue(0.0f);
    startupGain.setTargetValue(1.0f);
}

void SignalChain::processMono(juce::AudioBuffer<float>& monoBuffer)
{
    auto* samples = monoBuffer.getWritePointer(0);
    const auto numSamples = monoBuffer.getNumSamples();

    if (numSamples <= 0)
        return;

    const auto requestedInputGain = juce::Decibels::decibelsToGain(inputGainDb.load());
    const auto requestedOutputGain = juce::Decibels::decibelsToGain(outputGainDb.load());

    inputGain.setTargetValue(requestedInputGain);
    outputGain.setTargetValue(requestedOutputGain);

    float inPeak = 0.0f;
    float outPeak = 0.0f;

    const bool isBypassed = bypassed.load();

    // Phase 1 Amp20 path:
    // input trim -> 20-stage amp -> safety limiter -> output trim/startup fade.
    if (!isBypassed)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            float x = samples[i];
            inPeak = juce::jmax(inPeak, std::abs(x));
            samples[i] = x * inputGain.getNextValue();
        }

        ampEngine.process(monoBuffer);

        for (int i = 0; i < numSamples; ++i)
        {
            float x = limiter.processSample(samples[i]);
            x *= outputGain.getNextValue();
            x *= startupGain.getNextValue();
            samples[i] = x;
            outPeak = juce::jmax(outPeak, std::abs(x));
        }
    }
    else
    {
        // Bypass intentionally skips the amp, but still uses output gain,
        // startup fade and safety limiter.
        for (int i = 0; i < numSamples; ++i)
        {
            float x = samples[i];
            inPeak = juce::jmax(inPeak, std::abs(x));
            x = limiter.processSample(x);
            x *= outputGain.getNextValue();
            x *= startupGain.getNextValue();
            samples[i] = x;
            outPeak = juce::jmax(outPeak, std::abs(x));
        }
    }

    inputMeter.pushPeak(inPeak);
    outputMeter.pushPeak(outPeak);
}
