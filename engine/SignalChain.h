#pragma once
#include <JuceHeader.h>
#include "../dsp/SafetyLimiter.h"
#include "../dsp/amp/AmpEngine.h"
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"
#include "../hq_preload/dsp/HQEffectsRack.h"

class SignalChain
{
public:
    enum class AmpMode { legacy = 0, hq = 1 };

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();

    void process(juce::AudioBuffer<float>& buffer,
                 int startSample,
                 int numSamples);

    void setInputGainDb(float gainDb) noexcept;
    void setOutputGainDb(float gainDb) noexcept;
    void setBypass(bool shouldBypass) noexcept;
    void setMonoInputToStereo(bool enabled) noexcept;
    void setAmpMode(AmpMode mode) noexcept { ampMode.store((int)mode, std::memory_order_relaxed); }
    AmpMode getAmpMode() const noexcept { return (AmpMode)ampMode.load(std::memory_order_relaxed); }

    AmpEngine& getAmpEngine() noexcept { return ampEngine; }
    guitardsp::hq::AmpEngineHQ& getHQAmpEngine() noexcept { return hqAmpEngine; }
    guitardsp::hq::HQEffectsRack& getHQEffectsRack() noexcept { return hqEffects; }

private:
    void copyDetectedMonoToStereo(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyInputGain(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void processLegacyAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyStartupFadeAndLimiter(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    juce::SmoothedValue<float> inputGain;
    juce::SmoothedValue<float> startupFade;

    AmpEngine ampEngine;
    guitardsp::hq::AmpEngineHQ hqAmpEngine;
    guitardsp::hq::HQEffectsRack hqEffects;
    SafetyLimiter limiter;

    std::atomic<float> inputGainDb { -6.0f };
    std::atomic<bool> bypass { false };
    std::atomic<bool> monoInputToStereo { true };
    std::atomic<int> ampMode { (int)AmpMode::legacy };
};
