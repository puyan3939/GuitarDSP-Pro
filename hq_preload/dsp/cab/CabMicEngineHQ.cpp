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

juce::AudioBuffer<float> CabMicEngineHQ::makeImpulse() const
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
    work.setSize(1, numSamples, false, false, true);
    for (int ch = 0; ch < channels; ++ch)
    {
        work.copyFrom(0, 0, buffer, ch, startSample, numSamples);
        juce::AudioBuffer<float> dry(1, numSamples);
        dry.copyFrom(0, 0, work, 0, 0, numSamples);
        juce::dsp::AudioBlock<float> block(work);
        juce::dsp::ProcessContextReplacing<float> context(block);
        convolution[(size_t)ch]->process(context);
        auto* wet = work.getWritePointer(0);
        const auto* dryData = dry.getReadPointer(0);
        for (int i = 0; i < numSamples; ++i)
        {
            float y = highCut[(size_t)ch].process(lowCut[(size_t)ch].process(wet[i]));
            wet[i] = lerp(dryData[i], y, params.mix);
        }
        buffer.copyFrom(ch, startSample, work, 0, 0, numSamples);
    }
}
}
