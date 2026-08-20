#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "SignalChain.h"
#include "../dsp/LevelMeter.h"

class AudioEngine : private juce::AudioIODeviceCallback
{
public:
    AudioEngine();
    ~AudioEngine() override;

    void initialise();
    void shutdown();
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
    guitardsp::hq::CabMicEngineHQ& getCabMicEngine() noexcept { return signalChain.getCabMicEngine(); }
    guitardsp::hq::InputLoadingControl& getInputLoadingControl() noexcept { return signalChain.getInputLoadingControl(); }
    guitardsp::hq::ExpressionPitchControl& getExpressionPitchControl() noexcept { return signalChain.getExpressionPitchControl(); }
    guitardsp::hq::DualDelayControl& getDualDelayControl() noexcept { return signalChain.getDualDelayControl(); }
    guitardsp::hq::SceneSwitcherHQ& getSceneSwitcher() noexcept { return signalChain.getSceneSwitcher(); }

    void setAmpMode(SignalChain::AmpMode mode) noexcept { signalChain.setAmpMode(mode); }
    SignalChain::AmpMode getAmpMode() const noexcept { return signalChain.getAmpMode(); }
    void setOutputMode(SignalChain::OutputMode mode) noexcept { signalChain.setOutputMode(mode); }
    SignalChain::OutputMode getOutputMode() const noexcept { return signalChain.getOutputMode(); }

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    float getInputPeak(int channel) const noexcept;
    float getInputRms(int channel) const noexcept;
    float getOutputPeak(int channel) const noexcept;
    float getOutputRms(int channel) const noexcept;

private:
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,int numInputChannels,float* const* outputChannelData,int numOutputChannels,int numSamples,const juce::AudioIODeviceCallbackContext&) override;
    static void measureBlock(const juce::AudioBuffer<float>& buffer,int startSample,int numSamples,std::array<LevelMeter, 2>& peakMeters,std::array<std::atomic<float>, 2>& rmsDb);

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> ioBuffer;
    SignalChain signalChain;
    std::array<LevelMeter, 2> inputMeters;
    std::array<LevelMeter, 2> outputMeters;
    std::array<std::atomic<float>, 2> inputRmsDb { std::atomic<float>{-100.0f}, std::atomic<float>{-100.0f} };
    std::array<std::atomic<float>, 2> outputRmsDb { std::atomic<float>{-100.0f}, std::atomic<float>{-100.0f} };
    std::atomic<bool> deviceCallbackAttached { false };
};
