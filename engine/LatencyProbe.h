#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <vector>

namespace guitardsp
{
struct LatencyProbe
{
    static std::vector<float> makeSequence(int length=511, float amplitude=0.08f)
    {
        length = juce::jlimit(63, 2047, length);
        std::vector<float> sequence((size_t)length, 0.0f);
        unsigned int state = 0x1ffu;
        for (int i = 0; i < length; ++i)
        {
            const unsigned int feedback = ((state >> 8u) ^ (state >> 4u)) & 1u;
            state = ((state << 1u) | feedback) & 0x1ffu;
            if (state == 0u) state = 0x1ffu;
            sequence[(size_t)i] = (state & 1u) != 0u ? amplitude : -amplitude;
        }
        return sequence;
    }

    static int estimateDelaySamples(const std::vector<float>& reference,
                                    const float* capture,
                                    int captureSamples,
                                    float& bestCorrelation)
    {
        bestCorrelation = 0.0f;
        if (capture == nullptr || reference.empty() || captureSamples <= (int)reference.size())
            return -1;

        double refEnergy = 0.0;
        for (float x : reference) refEnergy += (double)x * (double)x;
        if (refEnergy <= 1.0e-20) return -1;

        float bestAbs = 0.0f;
        int bestStart = -1;
        const int n = (int)reference.size();
        for (int start = 0; start + n <= captureSamples; ++start)
        {
            double dot = 0.0, captureEnergy = 0.0;
            for (int i = 0; i < n; ++i)
            {
                const double x = reference[(size_t)i];
                const double y = capture[start + i];
                dot += x * y;
                captureEnergy += y * y;
            }
            const float corr = (float)(dot / std::sqrt(juce::jmax(1.0e-24, refEnergy * captureEnergy)));
            if (std::abs(corr) > bestAbs)
            {
                bestAbs = std::abs(corr);
                bestCorrelation = corr;
                bestStart = start;
            }
        }
        return bestStart;
    }
};
}
