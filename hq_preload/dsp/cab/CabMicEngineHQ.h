#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
enum class CabType { open1x12, vintage2x12, vintage4x12, modern4x12 };
enum class MicType { dynamic57, ribbon121, condenser67 };
enum class CabIrEngine { classic, advanced, external };
enum class ExternalIrSize { samples1024, samples2048, full };

struct CabMicParams
{
    CabType cab = CabType::vintage4x12;
    MicType mic = MicType::dynamic57;
    CabIrEngine irEngine = CabIrEngine::classic;
    ExternalIrSize externalIrSize = ExternalIrSize::samples2048;
    float position = 0.42f;
    float distance = 0.18f;
    float resonance = 0.55f;
    float lowCutHz = 70.0f;
    float highCutHz = 9000.0f;
    float mix = 1.0f;
    float lowVolumeFeel = 0.0f;
    float irLevelDb = 0.0f;
    bool polarityInvert = false;
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
    CabMicParams getParameters() const;
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    bool loadExternalImpulse(const juce::File& file);
    void clearExternalImpulse();
    bool hasExternalImpulse() const;
    juce::File getExternalIrFile() const;
    juce::String getExternalIrName() const;
    int getCurrentIrSize() const noexcept { return convolution[0] ? convolution[0]->getCurrentIRSize() : 0; }

private:
    juce::AudioBuffer<float> makeClassicImpulse() const;
    juce::AudioBuffer<float> makeAdvancedImpulse() const;
    juce::AudioBuffer<float> makeImpulse() const;
    juce::AudioBuffer<float> makeBypassImpulse() const;
    size_t externalIrTargetSize() const noexcept;
    void applyPendingConfiguration();
    void rebuildImpulse();
    void updateFilters();
    void updateFeelFilters();
    static CabMicParams sanitise(CabMicParams p) noexcept;

    double fs = 48000.0;
    int maxBlock = 512;

    // UI/message-thread desired state and audio-thread active state are separated.
    // The audio thread never waits for the UI: it uses a try-lock at block boundaries
    // and keeps the previous configuration if the message thread is updating it.
    mutable juce::SpinLock configLock;
    CabMicParams params;
    CabMicParams activeParams;
    juce::File externalIrFile;
    juce::File activeExternalIrFile;
    std::atomic<std::uint64_t> configVersion { 1 };
    std::uint64_t activeConfigVersion = 0;

    std::array<std::unique_ptr<juce::dsp::Convolution>, 2> convolution;
    std::array<OnePoleHP, 2> lowCut;
    std::array<OnePoleLP, 2> highCut;
    std::array<Biquad, 2> feelBody;
    std::array<Biquad, 2> feelLowMid;
    std::array<Biquad, 2> feelPresence;
    juce::AudioBuffer<float> work;
    juce::AudioBuffer<float> dryWork;
    std::atomic<bool> enabled { false };
};
}
