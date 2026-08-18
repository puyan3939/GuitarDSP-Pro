#pragma once
#include <JuceHeader.h>
#include <array>
#include "SignalChain.h"
#include "../dsp/LevelMeter.h"

class AudioEngine
{
public:
    AudioEngine();
    ~AudioEngine();

    void prepare(double sampleRate, int maximumBlockSize);
    void release();
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void setInputGainDb(float gainDb) noexcept;
    void setOutputGainDb(float gainDb) noexcept;
    void setBypass(bool enabled) noexcept;
    void setMonoInputToStereo(bool enabled) noexcept;
    AmpEngine& getAmpEngine() noexcept { return signalChain.getAmpEngine(); }
    guitardsp::hq::AmpEngineHQ& getHQAmpEngine() noexcept { return signalChain.getHQAmpEngine(); }
    guitardsp::hq::HQEffectsRack& getHQEffectsRack() noexcept { return signalChain.getHQEffectsRack(); }
    void setAmpMode(SignalChain::AmpMode mode) noexcept { signalChain.setAmpMode(mode); }
    SignalChain::AmpMode getAmpMode() const noexcept { return signalChain.getAmpMode(); }

    float getInputPeak(int channel) const noexcept;
    float getInputRms(int channel) const noexcept;
    float getOutputPeak(int channel) const noexcept;
    float getOutputRms(int channel) const noexcept;

private:
    static void measureBlock(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples, std::array<LevelMeter, 2>& meters);
    SignalChain signalChain;
    std::array<LevelMeter, 2> inputMeters;
    std::array<LevelMeter, 2> outputMeters;
};
