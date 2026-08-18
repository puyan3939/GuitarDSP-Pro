#include "SafetyLimiter.h"
#include <algorithm>
#include <cmath>

void SafetyLimiter::prepare(double sampleRate)
{
    attackCoeff = std::exp(-1.0f / static_cast<float>(sampleRate * 0.001));
    releaseCoeff = std::exp(-1.0f / static_cast<float>(sampleRate * 0.080));
    reset();
}

void SafetyLimiter::reset()
{
    envelope = 0.0f;
    gain = 1.0f;
}

float SafetyLimiter::processSample(float x) noexcept
{
    const auto ax = std::abs(x);
    const auto coeff = ax > envelope ? attackCoeff : releaseCoeff;
    envelope = coeff * envelope + (1.0f - coeff) * ax;

    constexpr float ceiling = 0.89f; // about -1 dBFS
    const auto targetGain = envelope > ceiling ? ceiling / std::max(envelope, 1.0e-6f) : 1.0f;

    const auto gainCoeff = targetGain < gain ? attackCoeff : releaseCoeff;
    gain = gainCoeff * gain + (1.0f - gainCoeff) * targetGain;

    // Final soft guard. This is deliberately conservative in Phase 0.
    const auto y = x * gain;
    return std::tanh(y / ceiling) * ceiling;
}
