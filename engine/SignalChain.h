#pragma once
#include <JuceHeader.h>
#include <array>
#include "../dsp/SafetyLimiter.h"
#include "../dsp/amp/AmpEngine.h"
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"
#include "../hq_preload/dsp/HQEffectsRack.h"
#include "../hq_preload/dsp/cab/CabMicEngineHQ.h"
#include "../hq_preload/dsp/routing/ParallelRigHQ.h"
#include "../hq_preload/dsp/performance/PerformanceToolsHQ.h"
#include "SignalTapBuffer.h"

class SignalChain
{
public:
    enum class AmpMode { legacy = 0, hq = 1 };
    enum class OutputMode { stereoMix = 0, mainLeftSubRight = 1 };

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    void setInputGainDb(float gainDb) noexcept;
    void setOutputGainDb(float gainDb) noexcept;
    void setBypass(bool shouldBypass) noexcept;
    void setMonoInputToStereo(bool enabled) noexcept;
    void setAmpMode(AmpMode mode) noexcept { ampMode.store((int)mode, std::memory_order_relaxed); }
    AmpMode getAmpMode() const noexcept { return (AmpMode)ampMode.load(std::memory_order_relaxed); }
    void setOutputMode(OutputMode m) noexcept { outputMode.store((int)m,std::memory_order_relaxed); }
    OutputMode getOutputMode() const noexcept { return (OutputMode)outputMode.load(std::memory_order_relaxed); }

    void setAnalyzerTaps(SignalTapBuffer* taps) noexcept { analyzerTaps = taps; }
    void setAnalysisMode(bool enabled) noexcept { analysisMode = enabled; }
    void copySettingsTo(SignalChain& destination);

    AmpEngine& getAmpEngine() noexcept { return ampEngine; }
    guitardsp::hq::AmpEngineHQ& getHQAmpEngine() noexcept { return hqAmpEngine; }
    guitardsp::hq::HQEffectsRack& getHQEffectsRack() noexcept { return hqEffects; }
    guitardsp::hq::CabMicEngineHQ& getCabMicEngine() noexcept { return cabMic; }
    guitardsp::hq::InputLoadingControl& getInputLoadingControl() noexcept { return inputLoadingControl; }
    guitardsp::hq::ExpressionPitchControl& getExpressionPitchControl() noexcept { return expressionPitchControl; }
    guitardsp::hq::DualDelayControl& getDualDelayControl() noexcept { return dualDelayControl; }
    guitardsp::hq::SceneSwitcherHQ& getSceneSwitcher() noexcept { return scenes; }

private:
    void copyDetectedMonoToStereo(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyInputLoading(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyInputGain(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void captureParallelTaps(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void processExpressionPitchRoute(juce::AudioBuffer<float>& buffer, int startSample, int numSamples, int routeBit, int stateIndex);
    void processLegacyAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void processHQAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applySceneRequests();
    void applyStemOutput(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyStartupFadeAndLimiter(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void pushTap(SignalTapBuffer::TapPoint point, const juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept;

    juce::SmoothedValue<float> inputGain;
    juce::SmoothedValue<float> startupFade;
    juce::AudioBuffer<float> ampWorkBuffer;
    juce::AudioBuffer<float> cleanRouteBuffer;
    juce::AudioBuffer<float> subRouteBuffer;
    juce::AudioBuffer<float> expressionWorkBuffer;

    AmpEngine ampEngine;
    guitardsp::hq::AmpEngineHQ hqAmpEngine;
    guitardsp::hq::HQEffectsRack hqEffects;
    guitardsp::hq::CabMicEngineHQ cabMic;
    guitardsp::hq::ParallelRigHQ parallelRig;
    guitardsp::hq::InputLoadingHQ inputLoading[2];
    guitardsp::hq::InputLoadingControl inputLoadingControl;
    std::array<guitardsp::hq::ExpressionPitchHQ, 3> expressionPitch;
    guitardsp::hq::ExpressionPitchControl expressionPitchControl;
    guitardsp::hq::DualDelayStereoHQ dualDelay;
    guitardsp::hq::DualDelayControl dualDelayControl;
    guitardsp::hq::SceneSwitcherHQ scenes;
    SafetyLimiter limiter;

    std::atomic<float> inputGainDb { -6.0f };
    std::atomic<float> outputGainDb { -18.0f };
    std::atomic<bool> bypass { false };
    std::atomic<bool> monoInputToStereo { true };
    std::atomic<int> ampMode { (int)AmpMode::legacy };
    std::atomic<int> outputMode { (int)OutputMode::stereoMix };
    SignalTapBuffer* analyzerTaps = nullptr;
    bool analysisMode = false;
};
