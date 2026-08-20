#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include "SignalChain.h"
#include "SignalTapBuffer.h"
#include "LatencyProbe.h"
#include "../dsp/LevelMeter.h"

class AudioEngine : private juce::AudioIODeviceCallback
{
public:
    enum class AnalyzerTestMode { sine, sweep };

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
    void setLiveAnalyzerEnabled(bool enabled) noexcept;

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

    double getCurrentSampleRate() const noexcept { return currentSampleRate.load(std::memory_order_relaxed); }
    void copyLiveAnalyzerTap(SignalTapBuffer::TapPoint point, std::vector<float>& destination, int samples) const;
    void copyTestAnalyzerTap(SignalTapBuffer::TapPoint point, std::vector<float>& destination, int samples) const;
    void renderAnalyzerTest(AnalyzerTestMode mode,
                            float frequencyHz,
                            float levelDb,
                            float sweepStartHz = 20.0f,
                            float sweepEndHz = 20000.0f);

    bool startRoundTripLatencyMeasurement();
    void finaliseRoundTripLatencyMeasurement();
    void restoreAudioAfterLatencyMeasurement() noexcept;
    bool isRoundTripLatencyMeasurementActive() const noexcept { return latencyState.load(std::memory_order_acquire) == 1; }
    bool isRoundTripLatencyCaptureReady() const noexcept { return latencyState.load(std::memory_order_acquire) == 2; }
    bool hasRoundTripLatencyResult() const noexcept { return latencyState.load(std::memory_order_acquire) >= 3; }
    bool isLatencyProbeMutingAudio() const noexcept
    {
        const int state = latencyState.load(std::memory_order_acquire);
        return state >= 1 && state <= 3;
    }
    int getMeasuredRoundTripLatencySamples() const noexcept { return measuredRoundTripSamples.load(std::memory_order_relaxed); }
    float getLatencyMeasurementCorrelation() const noexcept { return latencyCorrelation.load(std::memory_order_relaxed); }
    int getReportedInputLatencySamples() const noexcept { return reportedInputLatencySamples.load(std::memory_order_relaxed); }
    int getReportedOutputLatencySamples() const noexcept { return reportedOutputLatencySamples.load(std::memory_order_relaxed); }

    int measureCurrentDspLatencySamples();

private:
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,int numInputChannels,float* const* outputChannelData,int numOutputChannels,int numSamples,const juce::AudioIODeviceCallbackContext&) override;
    void processLatencyProbeBlock(const float* const* inputChannelData,int numInputChannels,float* const* outputChannelData,int numOutputChannels,int numSamples) noexcept;
    static void clearOutputs(float* const* outputChannelData,int numOutputChannels,int numSamples) noexcept;
    static void measureBlock(const juce::AudioBuffer<float>& buffer,int startSample,int numSamples,std::array<LevelMeter, 2>& peakMeters,std::array<std::atomic<float>, 2>& rmsDb);

    static constexpr int analyzerRenderSamples = 4096;
    static constexpr int latencyProbeLeadIn = 2048;
    static constexpr int latencyCaptureLength = 16384;

    juce::AudioDeviceManager deviceManager;
    juce::AudioBuffer<float> ioBuffer;
    SignalChain signalChain;
    SignalTapBuffer liveAnalyzerTaps;

    SignalChain analyzerSignalChain;
    SignalTapBuffer testAnalyzerTaps;
    juce::AudioBuffer<float> analyzerBuffer;
    juce::CriticalSection analyzerLock;
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<bool> analyzerPrepared { false };

    std::vector<float> latencyProbe;
    juce::AudioBuffer<float> latencyCapture;
    int latencyWriteIndex = 0;
    std::atomic<int> latencyState { 0 };
    std::atomic<int> measuredRoundTripSamples { -1 };
    std::atomic<float> latencyCorrelation { 0.0f };
    std::atomic<int> reportedInputLatencySamples { 0 };
    std::atomic<int> reportedOutputLatencySamples { 0 };

    std::array<LevelMeter, 2> inputMeters;
    std::array<LevelMeter, 2> outputMeters;
    std::array<std::atomic<float>, 2> inputRmsDb { std::atomic<float>{-100.0f}, std::atomic<float>{-100.0f} };
    std::array<std::atomic<float>, 2> outputRmsDb { std::atomic<float>{-100.0f}, std::atomic<float>{-100.0f} };
    std::atomic<bool> deviceCallbackAttached { false };
};
