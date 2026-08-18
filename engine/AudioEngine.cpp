#include "AudioEngine.h"
#include <cmath>

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    if (deviceCallbackAttached.load(std::memory_order_relaxed))
        return;

    const auto error = deviceManager.initialise(1, 2, nullptr, true);
    if (error.isNotEmpty())
        DBG("Audio device initialise error: " + error);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup originalSetup;
        deviceManager.getAudioDeviceSetup(originalSetup);
        auto requestedSetup = originalSetup;

        const auto sampleRates = device->getAvailableSampleRates();
        const auto bufferSizes = device->getAvailableBufferSizes();
        const bool supports48k = sampleRates.contains(48000.0);
        const bool supports256 = bufferSizes.contains(256);

        // Do not force a device reopen unless the requested format is actually
        // advertised by the active driver. Some ALSA devices can fail to restart
        // when an unsupported format is requested or another client owns the device.
        if (supports48k)
            requestedSetup.sampleRate = 48000.0;
        if (supports256)
            requestedSetup.bufferSize = 256;

        const bool needsChange = requestedSetup.sampleRate != originalSetup.sampleRate
                              || requestedSetup.bufferSize != originalSetup.bufferSize;

        if (needsChange)
        {
            const auto setupError = deviceManager.setAudioDeviceSetup(requestedSetup, true);
            if (setupError.isNotEmpty())
            {
                DBG("Requested audio format unavailable: " + setupError + ". Restoring previous setup.");
                const auto restoreError = deviceManager.setAudioDeviceSetup(originalSetup, true);
                if (restoreError.isNotEmpty())
                    DBG("Could not restore previous audio setup: " + restoreError);
            }
        }
    }

    deviceManager.addAudioCallback(this);
    deviceCallbackAttached.store(true, std::memory_order_relaxed);
}

void AudioEngine::shutdown()
{
    if (deviceCallbackAttached.exchange(false, std::memory_order_relaxed))
        deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
    release();
}

void AudioEngine::prepare(double sampleRate, int maximumBlockSize)
{
    signalChain.prepare(sampleRate, maximumBlockSize);
    ioBuffer.setSize(2, maximumBlockSize, false, false, true);
    for (auto& meter : inputMeters) meter.reset();
    for (auto& meter : outputMeters) meter.reset();
    for (auto& value : inputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
    for (auto& value : outputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
}

void AudioEngine::release()
{
    signalChain.reset();
    ioBuffer.setSize(2, 0);
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

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
        prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
}

void AudioEngine::audioDeviceStopped()
{
    release();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    ioBuffer.setSize(2, numSamples, false, false, true);
    ioBuffer.clear();

    const int inputsToCopy = juce::jmin(2, numInputChannels);
    for (int ch = 0; ch < inputsToCopy; ++ch)
        if (inputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::copy(ioBuffer.getWritePointer(ch), inputChannelData[ch], numSamples);

    process(ioBuffer, 0, numSamples);

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] == nullptr)
            continue;
        if (ch < ioBuffer.getNumChannels())
            juce::FloatVectorOperations::copy(outputChannelData[ch], ioBuffer.getReadPointer(ch), numSamples);
        else
            juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }
}

void AudioEngine::setInputGainDb(float gainDb) noexcept { signalChain.setInputGainDb(gainDb); }
void AudioEngine::setOutputGainDb(float gainDb) noexcept { signalChain.setOutputGainDb(gainDb); }
void AudioEngine::setBypass(bool enabled) noexcept { signalChain.setBypass(enabled); }
void AudioEngine::setMonoInputToStereo(bool enabled) noexcept { signalChain.setMonoInputToStereo(enabled); }

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
