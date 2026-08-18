#include "AudioEngine.h"
#include <cmath>

AudioEngine::AudioEngine() = default;
AudioEngine::~AudioEngine() = default;

void AudioEngine::prepare(double sampleRate, int maximumBlockSize)
{
    signalChain.prepare(sampleRate, maximumBlockSize);
    for (auto& meter : inputMeters) meter.reset();
    for (auto& meter : outputMeters) meter.reset();
    for (auto& value : inputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
    for (auto& value : outputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
}

void AudioEngine::release()
{
    signalChain.reset();
    for (auto& meter : inputMeters) meter.reset();
    for (auto& meter : outputMeters) meter.reset();
    for (auto& value : inputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
    for (auto& value : outputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
}

void AudioEngine::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0 || startSample < 0 || startSample + numSamples > buffer.getNumSamples())
        return;

    measureBlock(buffer, startSample, numSamples, inputMeters, inputRmsDb);
    signalChain.process(buffer, startSample, numSamples);
    measureBlock(buffer, startSample, numSamples, outputMeters, outputRmsDb);
}

void AudioEngine::setInputGainDb(float gainDb) noexcept
{
    signalChain.setInputGainDb(gainDb);
}

void AudioEngine::setOutputGainDb(float gainDb) noexcept
{
    signalChain.setOutputGainDb(gainDb);
}

void AudioEngine::setBypass(bool enabled) noexcept
{
    signalChain.setBypass(enabled);
}

void AudioEngine::setMonoInputToStereo(bool enabled) noexcept
{
    signalChain.setMonoInputToStereo(enabled);
}

float AudioEngine::getInputPeak(int channel) const noexcept
{
    return (channel >= 0 && channel < 2) ? inputMeters[(size_t)channel].getDb() : -100.0f;
}

float AudioEngine::getInputRms(int channel) const noexcept
{
    return (channel >= 0 && channel < 2) ? inputRmsDb[(size_t)channel].load(std::memory_order_relaxed) : -100.0f;
}

float AudioEngine::getOutputPeak(int channel) const noexcept
{
    return (channel >= 0 && channel < 2) ? outputMeters[(size_t)channel].getDb() : -100.0f;
}

float AudioEngine::getOutputRms(int channel) const noexcept
{
    return (channel >= 0 && channel < 2) ? outputRmsDb[(size_t)channel].load(std::memory_order_relaxed) : -100.0f;
}

void AudioEngine::measureBlock(const juce::AudioBuffer<float>& buffer,
                               int startSample,
                               int numSamples,
                               std::array<LevelMeter, 2>& peakMeters,
                               std::array<std::atomic<float>, 2>& rmsDb)
{
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int ch = 0; ch < 2; ++ch)
    {
        if (ch >= channels)
        {
            peakMeters[(size_t)ch].pushPeak(0.0f);
            rmsDb[(size_t)ch].store(-100.0f, std::memory_order_relaxed);
            continue;
        }

        const auto* data = buffer.getReadPointer(ch, startSample);
        float peak = 0.0f;
        double sumSquares = 0.0;
        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];
            peak = juce::jmax(peak, std::abs(x));
            sumSquares += static_cast<double>(x) * static_cast<double>(x);
        }

        peakMeters[(size_t)ch].pushPeak(peak);
        const float rms = numSamples > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(numSamples))) : 0.0f;
        rmsDb[(size_t)ch].store(juce::Decibels::gainToDecibels(rms, -100.0f), std::memory_order_relaxed);
    }
}
