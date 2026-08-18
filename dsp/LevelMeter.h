#pragma once

#include <atomic>

class LevelMeter
{
public:
    void reset() noexcept;
    void pushPeak(float linearPeak) noexcept;
    float getDb() const noexcept;

private:
    std::atomic<float> peakDb { -100.0f };
};
