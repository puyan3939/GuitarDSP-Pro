#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <memory>
#include "common/HQDSP.h"
#include "amp/AmpEngineHQ.h"
#include "pedals/PedalEngineHQ.h"
#include "dynamics/DynamicsHQ.h"
#include "modulation/ModulationHQ.h"
#include "delay/DelayHQ.h"
#include "reverb/ReverbHQ.h"
#include "routing/ParallelRigHQ.h"

namespace guitardsp::hq {

class HQEffectsRack {
public:
    static constexpr int pedalSlots = 4;
    static constexpr int stereoChannels = 2;
    enum class DynamicsMode { off, gate, studioCompressor, guitarCompressor };
    enum class ModulationMode { off, chorus, flanger, phaser, tremolo, vibrato };

    struct PedalSlotControl {
        std::atomic<bool> enabled{false};
        std::atomic<int> model{(int)PedalType::midOD};
        std::atomic<float> drive{0.5f}, tone{0.5f}, levelDb{0.0f}, mix{1.0f};
        std::atomic<float> aux1{0.5f}, aux2{0.5f}, aux3{0.5f};
    };
    struct GateControl {
        std::atomic<float> thresholdDb{-55}, rangeDb{-60}, ratio{4};
        std::atomic<float> attackMs{1}, holdMs{35}, releaseMs{180}, hysteresisDb{4};
        std::atomic<float> sidechainHpHz{55}, sidechainLpHz{6500};
    };
    struct StudioCompControl { std::atomic<float> thresholdDb{-18}, ratio{4}, attackMs{10}, releaseMs{120}, kneeDb{6}, makeupDb{0}, mix{1}; std::atomic<bool> rms{true}; };
    struct GuitarCompControl { std::atomic<float> sustain{.55f}, attack{.45f}, blend{.8f}, levelDb{0}; };
    struct ModulationControl { std::atomic<float> rateHz{.7f}, depth{.5f}, mix{.5f}, feedback{0}, manual{.5f}, shape{.5f}; };
    struct DelayControl { std::atomic<int> flavor{(int)DelayType::digital}; std::atomic<float> timeMs{380}, feedback{.35f}, mix{.28f}, lowCutHz{80}, highCutHz{6500}, drive{.1f}, wow{.15f}, flutter{.08f}, age{.2f}; };
    struct ReverbControl { std::atomic<int> flavor{(int)ReverbType::room}; std::atomic<float> size{.55f}, decay{.55f}, damping{.5f}, preDelayMs{18}, mix{.22f}, mod{.15f}, drip{.35f}; };

    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void processPreAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void processPostAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    PedalSlotControl& pedalSlot(int index) noexcept { return pedalControls[(size_t)juce::jlimit(0, pedalSlots - 1, index)]; }
    GateControl& gateControl() noexcept { return gateControls; }
    StudioCompControl& studioCompControl() noexcept { return studioControls; }
    GuitarCompControl& guitarCompControl() noexcept { return guitarControls; }
    ModulationControl& modulationControl() noexcept { return modControls; }
    DelayControl& delayControl() noexcept { return delayControls; }
    ReverbControl& reverbControl() noexcept { return reverbControls; }
    ParallelRigControl& parallelRigControl() noexcept { return parallelControls; }
    const ParallelRigControl& parallelRigControl() const noexcept { return parallelControls; }

    void setDynamicsMode(DynamicsMode m) noexcept { dynamicsMode.store((int)m, std::memory_order_relaxed); }
    DynamicsMode getDynamicsMode() const noexcept { return (DynamicsMode)dynamicsMode.load(std::memory_order_relaxed); }
    void setModulationMode(ModulationMode m) noexcept { modulationMode.store((int)m, std::memory_order_relaxed); }
    ModulationMode getModulationMode() const noexcept { return (ModulationMode)modulationMode.load(std::memory_order_relaxed); }
    void setDelayEnabled(bool e) noexcept { delayEnabled.store(e, std::memory_order_relaxed); }
    bool isDelayEnabled() const noexcept { return delayEnabled.load(std::memory_order_relaxed); }
    void setReverbEnabled(bool e) noexcept { reverbEnabled.store(e, std::memory_order_relaxed); }
    bool isReverbEnabled() const noexcept { return reverbEnabled.load(std::memory_order_relaxed); }

private:
    void applyDynamics(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyModulation(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void updateDynamicParameters();
    void updatePostParameters();

    std::array<std::array<PedalEngineHQ, pedalSlots>, stereoChannels> pedals;
    std::array<PedalSlotControl, pedalSlots> pedalControls;
    std::array<NoiseGate, stereoChannels> noiseGate;
    std::array<StudioCompressor, stereoChannels> studioComp;
    std::array<GuitarCompressor, stereoChannels> guitarComp;
    GateControl gateControls;
    StudioCompControl studioControls;
    GuitarCompControl guitarControls;
    std::array<ChorusHQ, stereoChannels> chorus;
    std::array<FlangerHQ, stereoChannels> flanger;
    std::array<PhaserHQ, stereoChannels> phaser;
    std::array<TremoloHQ, stereoChannels> tremolo;
    std::array<VibratoHQ, stereoChannels> vibrato;
    ModulationControl modControls;
    std::array<DelayHQ, stereoChannels> delayFx;
    std::array<ReverbHQ, stereoChannels> reverbFx;
    DelayControl delayControls;
    ReverbControl reverbControls;
    ParallelRigControl parallelControls;

    // Realtime work buffers are allocated once in prepare(). Never allocate on the audio thread.
    juce::AudioBuffer<float> pedalMonoWork;
    juce::AudioBuffer<float> gateCleanKey;
    int preparedMaxBlock = 0;

    std::atomic<int> dynamicsMode{(int)DynamicsMode::off};
    std::atomic<int> modulationMode{(int)ModulationMode::off};
    std::atomic<bool> delayEnabled{false};
    std::atomic<bool> reverbEnabled{false};
};

}
