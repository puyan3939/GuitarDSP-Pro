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

void SafetyLimiter::setCeiling(float linear) noexcept
{
    ceiling.store(std::clamp(linear, 0.01f, 0.999f), std::memory_order_relaxed);
}

void SafetyLimiter::setOutputGainDb(float gainDb) noexcept
{
    const auto clampedDb = std::clamp(gainDb, -60.0f, 12.0f);
    outputGain.store(std::pow(10.0f, clampedDb / 20.0f), std::memory_order_relaxed);
}

float SafetyLimiter::processSample(float x) noexcept
{
    const float c = ceiling.load(std::memory_order_relaxed);
    const float out = outputGain.load(std::memory_order_relaxed);
    const auto ax = std::abs(x);
    const auto coeff = ax > envelope ? attackCoeff : releaseCoeff;
    envelope = coeff * envelope + (1.0f - coeff) * ax;

    const auto targetGain = envelope > c ? c / std::max(envelope, 1.0e-6f) : 1.0f;
    const auto gainCoeff = targetGain < gain ? attackCoeff : releaseCoeff;
    gain = gainCoeff * gain + (1.0f - gainCoeff) * targetGain;

    const auto y = x * gain;
    return std::tanh(y / c) * c * out;
}
