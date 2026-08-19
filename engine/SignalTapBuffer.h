#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

class SignalTapBuffer
{
public:
    enum class TapPoint
    {
        input = 0,
        postPedals,
        postAmp,
        postCab,
        output,
        count
    };

    static constexpr int capacity = 8192;
    static constexpr int tapCount = static_cast<int>(TapPoint::count);

    SignalTapBuffer()
    {
        clear();
    }

    void clear() noexcept
    {
        for (auto& tap : buffers)
            for (auto& sample : tap)
                sample.store(0.0f, std::memory_order_relaxed);
        for (auto& index : writeIndices)
            index.store(0, std::memory_order_relaxed);
    }

    void pushBlock(TapPoint point,
                   const juce::AudioBuffer<float>& buffer,
                   int startSample,
                   int numSamples) noexcept
    {
        if (numSamples <= 0 || buffer.getNumChannels() <= 0)
            return;

        const int tap = static_cast<int>(point);
        if (tap < 0 || tap >= tapCount)
            return;

        const int channels = juce::jmin(2, buffer.getNumChannels());
        int sourceChannel = 0;
        if (channels > 1)
        {
            const float leftPeak = buffer.getMagnitude(0, startSample, numSamples);
            const float rightPeak = buffer.getMagnitude(1, startSample, numSamples);
            sourceChannel = rightPeak > leftPeak ? 1 : 0;
        }

        const auto* source = buffer.getReadPointer(sourceChannel, startSample);
        auto write = writeIndices[(size_t)tap].load(std::memory_order_relaxed);
        auto& target = buffers[(size_t)tap];

        for (int i = 0; i < numSamples; ++i)
        {
            target[(size_t)(write & static_cast<std::uint64_t>(capacity - 1))].store(source[i], std::memory_order_relaxed);
            ++write;
        }

        writeIndices[(size_t)tap].store(write, std::memory_order_release);
    }

    void copyLatest(TapPoint point, std::vector<float>& destination, int requestedSamples) const
    {
        const int tap = static_cast<int>(point);
        if (tap < 0 || tap >= tapCount)
        {
            destination.clear();
            return;
        }

        const int count = juce::jlimit(1, capacity, requestedSamples);
        destination.resize((size_t)count);

        const auto end = writeIndices[(size_t)tap].load(std::memory_order_acquire);
        const auto start = end - static_cast<std::uint64_t>(count);
        const auto& source = buffers[(size_t)tap];

        for (int i = 0; i < count; ++i)
        {
            const auto absolute = start + static_cast<std::uint64_t>(i);
            destination[(size_t)(i)] = source[(size_t)(absolute & static_cast<std::uint64_t>(capacity - 1))].load(std::memory_order_relaxed);
        }
    }

private:
    static_assert((capacity & (capacity - 1)) == 0, "capacity must be a power of two");
    using TapStorage = std::array<std::atomic<float>, capacity>;
    std::array<TapStorage, tapCount> buffers {};
    std::array<std::atomic<std::uint64_t>, tapCount> writeIndices {};
};
