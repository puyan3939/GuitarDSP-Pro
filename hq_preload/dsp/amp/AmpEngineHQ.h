#pragma once
#include <JuceHeader.h>
#include <array>
#include <memory>
#include "../common/HQDSP.h"
#include "JvmToneStack.h"

namespace guitardsp::hq
{
struct AmpStageParams
{
    float preHpHz=20.0f, preLpHz=18000.0f, drive=1.0f, bias=0.0f, asymmetry=0.0f, memory=0.0f, postLpHz=18000.0f, output=1.0f;
};

struct AmpHQParams
{
    std::array<AmpStageParams,20> stage{};
    float bassDb=0.0f, midDb=0.0f, trebleDb=0.0f;
    float sag=0.25f, sagRecoveryMs=95.0f;
    float damping=0.45f, presence=0.45f;
    float resonance=0.50f;
    float biasExcursion=0.20f;
    float transformerSaturation=0.25f;
    float outputDb=-12.0f;
    bool jvmToneStackEnabled=false;
    JvmToneStackConfig jvmToneStack;
};

struct JVM410HControls
{
    float bass=0.5f;
    float middle=0.5f;
    float treble=0.5f;
    float gain=0.5f;
    float master=0.5f;
    float presence=0.5f;
    float resonance=0.5f;
};

class AmpEngineHQ
{
public:
    explicit AmpEngineHQ(int oversamplingOrder=4);
    ~AmpEngineHQ();
    void prepare(double sampleRate,int maxBlockSize);
    void reset();
    void setParameters(const AmpHQParams& p);
    const AmpHQParams& getParameters() const noexcept { return params; }
    void process(juce::AudioBuffer<float>& mono);

    int getOversamplingFactor() const noexcept { return oversampling.getFactor(); }
    float getOversamplingLatencySamples() const noexcept { return oversampling.getLatencySamples(); }

    // Circuit-derived reference voicing. This is a calibrated model target, not a
    // claim of measurement-matching any individual vintage amplifier specimen.
    static AmpHQParams makeBassman5F6AReference();

    // Marshall JVM410H OD1 measured reference.
    // B/M/T and Gain are backed by public measurement datasets. Master, Presence
    // and Resonance are physical-model controls until matching multi-axis
    // measurements become publicly available.
    static AmpHQParams makeJVM410HOD1Reference(float bass=0.5f,
                                               float middle=0.5f,
                                               float treble=0.5f,
                                               float gain=0.5f,
                                               float master=0.5f,
                                               float presence=0.5f,
                                               float resonance=0.5f);
    static AmpHQParams makeJVM410HOD1Reference(const JVM410HControls& controls)
    {
        return makeJVM410HOD1Reference(controls.bass, controls.middle, controls.treble,
                                      controls.gain, controls.master,
                                      controls.presence, controls.resonance);
    }

private:
    struct TubeStage;
    struct Channel;
    float processChannel(Channel&,float x);
    void updateFilters();

    double fs=48000.0; int maxBlock=512; AmpHQParams params;
    std::array<std::unique_ptr<Channel>,2> channels;
    NonlinearOversampler oversampling;
    juce::AudioBuffer<float> work;
};
}
