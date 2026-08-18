#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
enum class CabType { open1x12, vintage2x12, vintage4x12, modern4x12 };
enum class MicType { dynamic57, ribbon121, condenser67 };
enum class CabIrEngine { classic, advanced };

struct CabMicParams
{
    CabType cab = CabType::vintage4x12;
    MicType mic = MicType::dynamic57;
    CabIrEngine irEngine = CabIrEngine::classic;
    float position = 0.42f;      // 0=edge, 1=cap
    float distance = 0.18f;      // 0=close, 1=far
    float resonance = 0.55f;
    float lowCutHz = 70.0f;
    float highCutHz = 9000.0f;
    float mix = 1.0f;
};

class CabMicEngineHQ
{
public:
    CabMicEngineHQ();
    ~CabMicEngineHQ();
    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void setEnabled(bool e) noexcept { enabled.store(e, std::memory_order_relaxed); }
    bool isEnabled() const noexcept { return enabled.load(std::memory_order_relaxed); }
    void setParameters(const CabMicParams& p);
    const CabMicParams& getParameters() const noexcept { return params; }
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

private:
    juce::AudioBuffer<float> makeClassicImpulse() const;
    juce::AudioBuffer<float> makeAdvancedImpulse() const;
    juce::AudioBuffer<float> makeImpulse() const;
    void rebuildImpulse();
    void updateFilters();

    double fs = 48000.0;
    int maxBlock = 512;
    CabMicParams params;
    std::array<std::unique_ptr<juce::dsp::Convolution>, 2> convolution;
    std::array<OnePoleHP, 2> lowCut;
    std::array<OnePoleLP, 2> highCut;
    juce::AudioBuffer<float> work;
    juce::AudioBuffer<float> dryWork;
    std::atomic<bool> enabled { false };
};
}
