#pragma once

class SafetyLimiter
{
public:
    void prepare(double sampleRate);
    void reset();
    float processSample(float x) noexcept;

private:
    float envelope = 0.0f;
    float gain = 1.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
};
