#include "CabMicEngineHQ.h"
#include <cmath>

namespace guitardsp::hq
{
CabMicEngineHQ::CabMicEngineHQ()
{
    for (auto& c : convolution) c = std::make_unique<juce::dsp::Convolution>();
}
CabMicEngineHQ::~CabMicEngineHQ() = default;

void CabMicEngineHQ::prepare(double sampleRate, int maximumBlockSize)
{
    fs = sampleRate; maxBlock = maximumBlockSize;
    work.setSize(1, maximumBlockSize, false, false, true);
    dryWork.setSize(1, maximumBlockSize, false, false, true);
    juce::dsp::ProcessSpec spec { fs, (juce::uint32) maximumBlockSize, 1 };
    for (int ch = 0; ch < 2; ++ch)
    {
        convolution[(size_t)ch]->prepare(spec);
        lowCut[(size_t)ch].prepare(fs);
        highCut[(size_t)ch].prepare(fs);
    }
    updateFilters();
    rebuildImpulse();
    reset();
}

void CabMicEngineHQ::reset()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        convolution[(size_t)ch]->reset();
        lowCut[(size_t)ch].reset();
        highCut[(size_t)ch].reset();
    }
    work.clear();
    dryWork.clear();
}

void CabMicEngineHQ::setParameters(const CabMicParams& p)
{
    params = p;
    params.position = juce::jlimit(0.0f, 1.0f, params.position);
    params.distance = juce::jlimit(0.0f, 1.0f, params.distance);
    params.resonance = juce::jlimit(0.0f, 1.0f, params.resonance);
    params.mix = juce::jlimit(0.0f, 1.0f, params.mix);
    updateFilters();
    if (convolution[0]) rebuildImpulse();
}

void CabMicEngineHQ::updateFilters()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        lowCut[(size_t)ch].setHz(juce::jlimit(25.0f, 350.0f, params.lowCutHz));
        highCut[(size_t)ch].setHz(juce::jlimit(2500.0f, 18000.0f, params.highCutHz));
    }
}

juce::AudioBuffer<float> CabMicEngineHQ::makeClassicImpulse() const
{
    const int length = juce::jlimit(1024, 8192, (int) std::round(fs * 0.075));
    juce::AudioBuffer<float> ir(1, length);
    ir.clear();
    auto* d = ir.getWritePointer(0);

    std::array<float, 5> modes{};
    switch (params.cab)
    {
        case CabType::open1x12:  modes = { 92, 185, 620, 1650, 3600 }; break;
        case CabType::vintage2x12: modes = { 78, 145, 520, 1350, 3150 }; break;
        case CabType::vintage4x12: modes = { 72, 128, 430, 1180, 2850 }; break;
        case CabType::modern4x12: modes = { 82, 155, 510, 1500, 3550 }; break;
    }

    float micBright = 1.0f, micBody = 1.0f;
    switch (params.mic)
    {
        case MicType::dynamic57: micBright = 1.18f; micBody = 0.90f; break;
        case MicType::ribbon121: micBright = 0.68f; micBody = 1.22f; break;
        case MicType::condenser67: micBright = 0.98f; micBody = 1.08f; break;
    }

    const float cap = params.position;
    const float distance = params.distance;
    const int delay = (int) std::round((0.00025 + 0.0022 * distance) * fs);
    const float decayRate = 42.0f + 38.0f * distance;
    const float resonance = 0.45f + 0.75f * params.resonance;

    if (delay < length) d[delay] = (0.35f + 0.38f * cap) * micBright;
    for (int n = delay; n < length; ++n)
    {
        const float t = (float)(n - delay) / (float)fs;
        const float env = std::exp(-decayRate * t);
        float y = 0.0f;
        for (size_t k = 0; k < modes.size(); ++k)
        {
            const float harmonicWeight = 1.0f / (1.0f + 0.72f * (float)k);
            const float brightness = k >= 3 ? (0.45f + 0.95f * cap) * micBright : micBody;
            const float phase = 0.23f * (float)k;
            y += std::sin(juce::MathConstants<float>::twoPi * modes[k] * t + phase) * harmonicWeight * brightness;
        }
        d[n] += y * env * 0.075f * resonance;
    }
    return ir;
}

juce::AudioBuffer<float> CabMicEngineHQ::makeAdvancedImpulse() const
{
    // Synthetic IR with measurement-like time/phase structure: direct sound,
    // cabinet/cone resonances, baffle diffraction and deterministic early reflections.
    const int length = juce::jlimit(2048, 16384, (int)std::round(fs * 0.120));
    juce::AudioBuffer<float> ir(1, length);
    ir.clear();
    auto* d = ir.getWritePointer(0);

    struct CabProfile
    {
        std::array<float, 10> modes;
        std::array<float, 10> weights;
        float decay;
        float reflectionScale;
        float breakupScale;
    };

    CabProfile profile{};
    switch (params.cab)
    {
        case CabType::open1x12:
            profile = {{ 88, 176, 310, 610, 980, 1540, 2380, 3470, 4680, 6120 },
                       { 1.00f,.72f,.45f,.39f,.31f,.28f,.23f,.20f,.13f,.08f },
                       34.0f, 0.86f, 0.74f};
            break;
        case CabType::vintage2x12:
            profile = {{ 76, 143, 265, 505, 845, 1320, 2130, 3060, 4210, 5550 },
                       { 1.00f,.79f,.50f,.43f,.33f,.30f,.25f,.21f,.14f,.08f },
                       39.0f, 0.76f, 0.68f};
            break;
        case CabType::vintage4x12:
            profile = {{ 70, 126, 238, 425, 720, 1160, 1860, 2790, 3890, 5200 },
                       { 1.00f,.84f,.56f,.47f,.37f,.31f,.26f,.22f,.14f,.075f },
                       43.0f, 0.69f, 0.62f};
            break;
        case CabType::modern4x12:
            profile = {{ 80, 154, 286, 510, 890, 1490, 2440, 3540, 4860, 6420 },
                       { 1.00f,.76f,.51f,.44f,.35f,.33f,.29f,.25f,.17f,.10f },
                       48.0f, 0.62f, 0.82f};
            break;
    }

    float micBright = 1.0f, micBody = 1.0f, micTransient = 1.0f;
    switch (params.mic)
    {
        case MicType::dynamic57: micBright=1.20f; micBody=.90f; micTransient=1.15f; break;
        case MicType::ribbon121: micBright=.66f; micBody=1.25f; micTransient=.78f; break;
        case MicType::condenser67: micBright=1.00f; micBody=1.08f; micTransient=1.02f; break;
    }

    const float cap = params.position;
    const float edge = 1.0f - cap;
    const float distance = params.distance;
    const float resonance = 0.42f + 0.88f * params.resonance;
    const int directDelay = (int)std::round((0.00018 + 0.00235 * distance) * fs);
    const float directGain = (0.48f + 0.34f * cap) * micTransient * (1.0f - 0.18f * distance);
    if (directDelay < length)
        d[directDelay] += directGain;

    // Main modal response. Position changes both spectral weight and phase rather than
    // acting as a simple brightness control.
    for (int n=directDelay; n<length; ++n)
    {
        const float t=(float)(n-directDelay)/(float)fs;
        const float env=std::exp(-(profile.decay + 20.0f*distance)*t);
        float y=0.0f;
        for (size_t k=0;k<profile.modes.size();++k)
        {
            const float high=(float)k/(float)(profile.modes.size()-1);
            const float positionWeight = lerp(0.72f + 0.35f*edge,
                                              0.55f + 1.05f*cap,
                                              high);
            const float micWeight = high > 0.48f ? micBright : micBody;
            const float phase = 0.19f*(float)k + edge*(0.31f + 0.17f*(float)k)
                              + distance*(0.11f*(float)(k+1));
            y += std::sin(juce::MathConstants<float>::twoPi*profile.modes[k]*t + phase)
               * profile.weights[k] * positionWeight * micWeight;
        }
        d[n] += y * env * 0.050f * resonance;
    }

    // Baffle / cone-breakup structures. These short, damped packets create the fine
    // peaks/notches and non-minimum-phase behaviour missing from the Classic engine.
    const std::array<float, 4> breakupFreq { 1850.0f, 2780.0f, 4120.0f, 5750.0f };
    const std::array<float, 4> breakupMs   { 0.16f, 0.31f, 0.53f, 0.81f };
    for (size_t b=0;b<breakupFreq.size();++b)
    {
        const int start=directDelay+(int)std::round(0.001f*(breakupMs[b]+0.34f*distance+0.08f*edge*(float)b)*fs);
        const int packet=(int)std::round(0.0055f*fs);
        for(int i=0;i<packet && start+i<length;++i)
        {
            const float t=(float)i/(float)fs;
            const float env=std::exp(-(520.0f+85.0f*(float)b)*t);
            const float amp=(0.012f+0.011f*cap)*profile.breakupScale*micBright/(1.0f+0.40f*(float)b);
            const float phase=0.7f*(float)b+1.1f*edge;
            d[start+i]+=amp*env*std::sin(juce::MathConstants<float>::twoPi*breakupFreq[b]*t+phase);
        }
    }

    // Deterministic early reflections. Distance increases their level and spacing;
    // cabinet type changes their strength. Alternating polarity gives realistic combing.
    const std::array<float, 7> reflectionMs { 0.37f, 0.71f, 1.14f, 1.83f, 2.74f, 4.05f, 6.20f };
    const std::array<float, 7> reflectionAmp{ .20f, -.14f, .115f, -.085f, .061f, -.043f, .029f };
    for(size_t r=0;r<reflectionMs.size();++r)
    {
        const float spacing = reflectionMs[r]*(0.88f+0.58f*distance) + edge*0.055f*(float)(r+1);
        const int idx=directDelay+(int)std::round(0.001f*spacing*fs);
        if(idx>=length) continue;
        const float farBoost=0.48f+0.92f*distance;
        const float amp=reflectionAmp[r]*profile.reflectionScale*farBoost*micBody;
        d[idx]+=amp;

        // Give each reflection a tiny resonant tail instead of an isolated impulse.
        const float ringHz=profile.modes[(r+2)%profile.modes.size()];
        const int ring=(int)std::round(0.0030f*fs);
        for(int i=1;i<ring && idx+i<length;++i)
        {
            const float t=(float)i/(float)fs;
            d[idx+i]+=amp*0.18f*std::exp(-900.0f*t)
                     *std::sin(juce::MathConstants<float>::twoPi*ringHz*t+0.4f*(float)r);
        }
    }

    // A very small deterministic diffuse tail prevents the IR from looking too periodic
    // without introducing runtime randomness or unstable preset recall.
    uint32_t state = 0x51f2a37bu + (uint32_t)params.cab*7919u + (uint32_t)params.mic*1543u;
    const int tailStart=directDelay+(int)std::round((0.0035f+0.0060f*distance)*fs);
    for(int n=juce::jmax(0,tailStart);n<length;++n)
    {
        state=1664525u*state+1013904223u;
        const float noise=((float)((state>>8)&0x00ffffffu)/8388607.5f)-1.0f;
        const float t=(float)(n-tailStart)/(float)fs;
        d[n]+=noise*0.0016f*profile.reflectionScale*(0.45f+0.75f*distance)*std::exp(-95.0f*t);
    }

    return ir;
}

juce::AudioBuffer<float> CabMicEngineHQ::makeImpulse() const
{
    return params.irEngine == CabIrEngine::advanced ? makeAdvancedImpulse()
                                                     : makeClassicImpulse();
}

void CabMicEngineHQ::rebuildImpulse()
{
    for (int ch = 0; ch < 2; ++ch)
    {
        auto ir = makeImpulse();
        convolution[(size_t)ch]->loadImpulseResponse(std::move(ir), fs,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::yes);
    }
}

void CabMicEngineHQ::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (!isEnabled() || numSamples <= 0) return;
    const int channels = juce::jmin(2, buffer.getNumChannels());
    jassert(numSamples <= work.getNumSamples());
    if (numSamples > work.getNumSamples()) return;

    for (int ch = 0; ch < channels; ++ch)
    {
        work.copyFrom(0, 0, buffer, ch, startSample, numSamples);
        dryWork.copyFrom(0, 0, buffer, ch, startSample, numSamples);
        juce::dsp::AudioBlock<float> fullBlock(work);
        auto block = fullBlock.getSubBlock(0, (size_t)numSamples);
        juce::dsp::ProcessContextReplacing<float> context(block);
        convolution[(size_t)ch]->process(context);
        auto* wet = work.getWritePointer(0);
        const auto* dryData = dryWork.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float y = highCut[(size_t)ch].process(lowCut[(size_t)ch].process(wet[i]));
            wet[i] = lerp(dryData[i], y, params.mix);
        }
        buffer.copyFrom(ch, startSample, work, 0, 0, numSamples);
    }
}
}
