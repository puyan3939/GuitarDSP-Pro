#pragma once
#include <JuceHeader.h>
#include <array>
#include "../common/HQDSP.h"

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
    float biasExcursion=0.20f;
    float transformerSaturation=0.25f;
    float outputDb=-12.0f;
};

class AmpEngineHQ
{
public:
    AmpEngineHQ();
    ~AmpEngineHQ();
    void prepare(double sampleRate,int maxBlockSize);
    void reset();
    void setParameters(const AmpHQParams& p);
    const AmpHQParams& getParameters() const noexcept { return params; }
    void process(juce::AudioBuffer<float>& mono);

    // Circuit-derived reference voicing. This is a calibrated model target, not a
    // claim of measurement-matching any individual vintage amplifier specimen.
    static AmpHQParams makeBassman5F6AReference();

    // Marshall JVM410H OD1 target used by the offline measured-data fitter.
    // The public DAFx23 dataset was recorded at Gain=5; bass/middle/treble are
    // normalised 0..1 controls corresponding to the published 0..10 settings.
    // Until the measured-data calibration workflow has been applied, this is a
    // topology-informed starting point rather than a measurement-match claim.
    static AmpHQParams makeJVM410HOD1Reference(float bass=0.5f,
                                               float middle=0.5f,
                                               float treble=0.5f);

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
