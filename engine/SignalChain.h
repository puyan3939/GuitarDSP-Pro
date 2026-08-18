#pragma once

#include <JuceHeader.h>
#include <atomic>
#include "../dsp/SafetyLimiter.h"
#include "../dsp/LevelMeter.h"
#include "../dsp/amp/AmpEngine.h"

class SignalChain
{
public:
    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void processMono(juce::AudioBuffer<float>& monoBuffer);

    void setInputGainDb(float db) noexcept { inputGainDb.store(db); }
    void setOutputGainDb(float db) noexcept { outputGainDb.store(db); }
    void setBypassed(bool shouldBypass) noexcept { bypassed.store(shouldBypass); }

    bool isBypassed() const noexcept { return bypassed.load(); }
    float getInputLevelDb() const noexcept { return inputMeter.getDb(); }
    float getOutputLevelDb() const noexcept { return outputMeter.getDb(); }

    AmpEngine& getAmpEngine() noexcept { return ampEngine; }
    const AmpEngine& getAmpEngine() const noexcept { return ampEngine; }

private:
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { -6.0f };
    std::atomic<bool> bypassed { false };

    juce::SmoothedValue<float> inputGain;
    juce::SmoothedValue<float> outputGain;
    juce::SmoothedValue<float> startupGain;

    AmpEngine ampEngine;
    SafetyLimiter limiter;
    LevelMeter inputMeter;
    LevelMeter outputMeter;
};
