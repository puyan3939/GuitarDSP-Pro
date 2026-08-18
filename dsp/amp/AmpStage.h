#pragma once

#include <JuceHeader.h>
#include <cmath>

struct AmpStageParameters
{
    float preHpHz = 20.0f;
    float preLpHz = 18000.0f;
    float drive = 1.0f;
    float bias = 0.0f;
    float postLpHz = 18000.0f;
    float output = 1.0f;
    float nonlinear = 0.0f;
    float clipShape = 0.0f;
};

class AmpStage
{
public:
    void prepare(double sampleRate);
    void reset();
    void setParameters(const AmpStageParameters& newParams);
    const AmpStageParameters& getParameters() const noexcept { return params; }

    float processSample(float x) noexcept
    {
        // Pre HPF
        const float hp = hpA * (hpY1 + x - hpX1);
        hpX1 = x;
        hpY1 = hp;

        // Pre LPF
        lpY1 = (1.0f - lpA) * hp + lpA * lpY1;
        float y = lpY1;

        // Nonlinear stage. "nonlinear" is a wet amount, clipShape morphs soft -> harder.
        if (params.nonlinear > 0.0001f)
        {
            const float driven = y * params.drive + params.bias;
            const float soft = std::tanh(driven) - std::tanh(params.bias);
            const float hard = juce::jlimit(-1.0f, 1.0f, driven) - juce::jlimit(-1.0f, 1.0f, params.bias);
            const float shaped = soft + (hard - soft) * juce::jlimit(0.0f, 1.0f, params.clipShape);
            y = y + (shaped - y) * juce::jlimit(0.0f, 1.0f, params.nonlinear);
        }

        // Post LPF
        postLpY1 = (1.0f - postLpA) * y + postLpA * postLpY1;
        return postLpY1 * params.output;
    }

private:
    void updateCoefficients();

    double fs = 48000.0;
    AmpStageParameters params;
    float hpA = 0.0f, hpX1 = 0.0f, hpY1 = 0.0f;
    float lpA = 0.0f, lpY1 = 0.0f;
    float postLpA = 0.0f, postLpY1 = 0.0f;
};
