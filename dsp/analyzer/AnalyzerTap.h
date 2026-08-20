#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

class AnalyzerTap
{
public:
    static constexpr int capacity = 8192;

    void reset() noexcept
    {
        inputFifo.reset();
        outputFifo.reset();
        inputStorage.fill(0.0f);
        outputStorage.fill(0.0f);
    }

    void pushInput(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        pushDominantMono(buffer, startSample, numSamples, inputFifo, inputStorage);
    }

    void pushOutput(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples) noexcept
    {
        pushDominantMono(buffer, startSample, numSamples, outputFifo, outputStorage);
    }

    int readInput(float* destination, int maxSamples) noexcept
    {
        return read(inputFifo, inputStorage, destination, maxSamples);
    }

    int readOutput(float* destination, int maxSamples) noexcept
    {
        return read(outputFifo, outputStorage, destination, maxSamples);
    }

private:
    template <size_t N>
    static void pushDominantMono(const juce::AudioBuffer<float>& buffer,
                                 int startSample,
                                 int numSamples,
                                 juce::AbstractFifo& fifo,
                                 std::array<float, N>& storage) noexcept
    {
        const int channels = juce::jmin(2, buffer.getNumChannels());
        if (channels <= 0 || numSamples <= 0)
            return;

        int sourceChannel = 0;
        if (channels > 1)
        {
            float peak0 = 0.0f;
            float peak1 = 0.0f;
            const auto* p0 = buffer.getReadPointer(0, startSample);
            const auto* p1 = buffer.getReadPointer(1, startSample);
            for (int i = 0; i < numSamples; ++i)
            {
                peak0 = juce::jmax(peak0, std::abs(p0[i]));
                peak1 = juce::jmax(peak1, std::abs(p1[i]));
            }
            sourceChannel = peak1 > peak0 ? 1 : 0;
        }

        const auto* source = buffer.getReadPointer(sourceChannel, startSample);
        int remaining = numSamples;
        int sourceOffset = 0;

        while (remaining > 0)
        {
            const int writable = juce::jmin(remaining, fifo.getFreeSpace());
            if (writable <= 0)
                break; // Observation must never block the realtime audio thread.

            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToWrite(writable, start1, size1, start2, size2);
            if (size1 > 0)
                juce::FloatVectorOperations::copy(storage.data() + start1, source + sourceOffset, size1);
            if (size2 > 0)
                juce::FloatVectorOperations::copy(storage.data() + start2, source + sourceOffset + size1, size2);
            fifo.finishedWrite(size1 + size2);
            sourceOffset += size1 + size2;
            remaining -= size1 + size2;
        }
    }

    template <size_t N>
    static int read(juce::AbstractFifo& fifo,
                    const std::array<float, N>& storage,
                    float* destination,
                    int maxSamples) noexcept
    {
        if (destination == nullptr || maxSamples <= 0)
            return 0;

        const int available = juce::jmin(maxSamples, fifo.getNumReady());
        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead(available, start1, size1, start2, size2);
        if (size1 > 0)
            juce::FloatVectorOperations::copy(destination, storage.data() + start1, size1);
        if (size2 > 0)
            juce::FloatVectorOperations::copy(destination + size1, storage.data() + start2, size2);
        fifo.finishedRead(size1 + size2);
        return size1 + size2;
    }

    juce::AbstractFifo inputFifo { capacity };
    juce::AbstractFifo outputFifo { capacity };
    std::array<float, capacity> inputStorage {};
    std::array<float, capacity> outputStorage {};
};
