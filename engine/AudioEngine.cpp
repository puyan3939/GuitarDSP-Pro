#include "AudioEngine.h"

AudioEngine::AudioEngine() = default;

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    deviceManager.initialise(1, 2, nullptr, true);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup setup;
        deviceManager.getAudioDeviceSetup(setup);
        setup.sampleRate = 48000.0;
        setup.bufferSize = 256;
        deviceManager.setAudioDeviceSetup(setup, true);
    }

    deviceManager.addAudioCallback(this);
}

void AudioEngine::shutdown()
{
    deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice();
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device == nullptr)
        return;

    const auto sampleRate = device->getCurrentSampleRate();
    const auto blockSize = device->getCurrentBufferSizeSamples();

    monoBuffer.setSize(1, blockSize, false, false, true);
    signalChain.prepare(sampleRate, blockSize);
}

void AudioEngine::audioDeviceStopped()
{
    signalChain.reset();
    monoBuffer.setSize(1, 0);
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                                   int numInputChannels,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&)
{
    monoBuffer.setSize(1, numSamples, false, false, true);
    auto* mono = monoBuffer.getWritePointer(0);

    if (numInputChannels > 0 && inputChannelData[0] != nullptr)
        juce::FloatVectorOperations::copy(mono, inputChannelData[0], numSamples);
    else
        juce::FloatVectorOperations::clear(mono, numSamples);

    signalChain.processMono(monoBuffer);

    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] == nullptr)
            continue;

        juce::FloatVectorOperations::copy(outputChannelData[ch], mono, numSamples);
    }
}
