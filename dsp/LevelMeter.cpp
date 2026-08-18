#include "LevelMeter.h"
#include <algorithm>
#include <cmath>

void LevelMeter::reset() noexcept
{
    peakDb.store(-100.0f);
}

void LevelMeter::pushPeak(float linearPeak) noexcept
{
    const auto safe = std::max(linearPeak, 1.0e-8f);
    const auto db = 20.0f * std::log10(safe);

    const auto previous = peakDb.load();
    peakDb.store(db > previous ? db : previous * 0.90f + db * 0.10f);
}

float LevelMeter::getDb() const noexcept
{
    return peakDb.load();
}
