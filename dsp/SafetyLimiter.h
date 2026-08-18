#pragma once

#include <atomic>

class SafetyLimiter
{
public:
    void prepare(double sampleRate);
    void reset();
    float processSample(float x) noexcept;

    void setCeiling(float linear) noexcept;
    void setOutputGainDb(float gainDb) noexcept;

private:
    float envelope = 0.0f;
    float gain = 1.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    std::atomic<float> ceiling { 0.89f };
    std::atomic<float> outputGain { 1.0f };
};
