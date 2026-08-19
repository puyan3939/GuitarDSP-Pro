#pragma once
#include <JuceHeader.h>
#include <vector>
#include <cmath>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
// Fixed-octave, dual-head granular pitch shifter. This is intentionally separate
// from the nonlinear Octave Fuzz: it preserves the input waveform and creates
// clean +/-1 octave voices with overlapping Hann windows and cubic interpolation.
//
// The external rig can remain at 48 kHz while this pitch core runs at 16x
// (768 kHz at a 48 kHz device rate). This improves moving-read-head interpolation
// and pushes resampling images far above the final audio Nyquist frequency.
class PitchOctaverHQ
{
public:
    struct Params
    {
        float octaveUp = 0.65f;
        float octaveDown = 0.0f;
        float dry = 0.65f;
        float tracking = 0.45f; // 0 = short/tight window, 1 = long/smooth
        float tone = 0.72f;
        float smooth = 0.55f;
    };

    PitchOctaverHQ() : oversampling(4) {} // 16x internal pitch rate

    void prepare(double sampleRate, int maximumBlockSize)
    {
        externalFs = sampleRate;
        oversampling.prepare(sampleRate, maximumBlockSize);
        fs = oversampling.getInternalSampleRate();

        // Keep the delay memory duration constant in seconds at the 16x rate.
        const int wanted = juce::jmax(8192,
            (int)std::ceil(fs * 0.18) + maximumBlockSize * oversampling.getFactor() + 32);
        int size = 1;
        while (size < wanted) size <<= 1;
        buffer.assign((size_t)size, 0.0f);
        mask = size - 1;

        toneLP.prepare(fs);
        dcBlock.prepare(fs);
        reset();
        updateTone();
    }

    void reset() noexcept
    {
        oversampling.reset();
        std::fill(buffer.begin(), buffer.end(), 0.0f);
        writeIndex = 0;
        upPhase = 0.0f;
        downPhase = 0.0f;
        toneLP.reset();
        dcBlock.reset();
    }

    void setParameters(const Params& p)
    {
        params = p;
        params.octaveUp = juce::jlimit(0.0f, 1.0f, params.octaveUp);
        params.octaveDown = juce::jlimit(0.0f, 1.0f, params.octaveDown);
        params.dry = juce::jlimit(0.0f, 1.0f, params.dry);
        params.tracking = juce::jlimit(0.0f, 1.0f, params.tracking);
        params.tone = juce::jlimit(0.0f, 1.0f, params.tone);
        params.smooth = juce::jlimit(0.0f, 1.0f, params.smooth);
        updateTone();
    }

    void processBlock(juce::AudioBuffer<float>& mono)
    {
        if (mono.getNumSamples() <= 0 || buffer.empty()) return;
        oversampling.process(mono, [this](float x) noexcept { return processInternal(x); });
    }

    int getInternalFactor() const noexcept { return oversampling.getFactor(); }
    double getInternalSampleRate() const noexcept { return fs; }

private:
    float processInternal(float input) noexcept
    {
        buffer[(size_t)(writeIndex & mask)] = input;

        // Window duration is defined in milliseconds, so increasing the internal
        // sample rate improves time resolution without changing playing feel.
        const float windowMs = lerp(18.0f, 56.0f, params.tracking);
        const float windowSamples = juce::jlimit(96.0f, (float)buffer.size() * 0.32f,
                                                 0.001f * windowMs * (float)fs);
        const float minDelay = 12.0f + 0.004f * (float)fs;

        const float up = renderVoice(2.0f, upPhase, windowSamples, minDelay);
        const float down = renderVoice(0.5f, downPhase, windowSamples, minDelay);

        const float shifted = params.octaveUp * up + params.octaveDown * down;
        const float normaliser = 1.0f / juce::jmax(1.0f, params.dry + params.octaveUp + params.octaveDown);
        float y = (params.dry * input + shifted) * normaliser;

        // This filter also runs at the 16x rate; the oversampler's downsampling
        // filters provide the final anti-imaging/anti-alias protection at 48 kHz.
        const float filtered = toneLP.process(y);
        y = lerp(y, filtered, 0.25f + 0.70f * params.smooth);
        ++writeIndex;
        return dcBlock.process(y);
    }

    static float wrap01(float p) noexcept
    {
        p -= std::floor(p);
        return p;
    }

    static float hann(float phase) noexcept
    {
        const float s = std::sin(juce::MathConstants<float>::pi * phase);
        return s * s;
    }

    float readCubic(float absoluteIndex) const noexcept
    {
        const int i1 = (int)std::floor(absoluteIndex);
        const float f = absoluteIndex - (float)i1;
        const float y0 = buffer[(size_t)((i1 - 1) & mask)];
        const float y1 = buffer[(size_t)( i1      & mask)];
        const float y2 = buffer[(size_t)((i1 + 1) & mask)];
        const float y3 = buffer[(size_t)((i1 + 2) & mask)];
        const float a0 = y3 - y2 - y0 + y1;
        const float a1 = y0 - y1 - a0;
        const float a2 = y2 - y0;
        return ((a0 * f + a1) * f + a2) * f + y1;
    }

    float renderVoice(float ratio, float& phase, float windowSamples, float minDelay) noexcept
    {
        const float phaseB = wrap01(phase + 0.5f);
        auto delayFor = [ratio, windowSamples, minDelay](float p) noexcept
        {
            // read-rate = 1 - d(delay)/dn. Up-octave uses decreasing delay;
            // down-octave uses increasing delay.
            return ratio >= 1.0f ? minDelay + windowSamples * (1.0f - p)
                                 : minDelay + windowSamples * p;
        };

        const float dA = delayFor(phase);
        const float dB = delayFor(phaseB);
        const float a = readCubic((float)writeIndex - dA);
        const float b = readCubic((float)writeIndex - dB);
        const float wA = hann(phase);
        const float wB = hann(phaseB);
        const float y = (a * wA + b * wB) / juce::jmax(1.0e-5f, wA + wB);

        const float phaseInc = std::abs(1.0f - ratio) / windowSamples;
        phase = wrap01(phase + phaseInc);
        return y;
    }

    void updateTone()
    {
        toneLP.setHz(lerp(4500.0f, 15500.0f, params.tone));
        dcBlock.setHz(18.0f);
    }

    Params params;
    double externalFs = 48000.0;
    double fs = 768000.0;
    NonlinearOversampler oversampling;
    std::vector<float> buffer;
    int mask = 0;
    int writeIndex = 0;
    float upPhase = 0.0f;
    float downPhase = 0.0f;
    OnePoleLP toneLP;
    OnePoleHP dcBlock;
};
}
