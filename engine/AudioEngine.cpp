#include "AudioEngine.h"
#include <cmath>

namespace
{
void copyAnalyzerExternalIr(SignalChain& source, SignalChain& destination)
{
    auto& sourceCab = source.getCabMicEngine();
    auto& destinationCab = destination.getCabMicEngine();
    if (sourceCab.hasExternalImpulse())
        destinationCab.loadExternalImpulse(sourceCab.getExternalIrFile());
    else
        destinationCab.clearExternalImpulse();
}
}

AudioEngine::AudioEngine()
{
    liveAnalyzerTaps.setEnabled(false);
    latencyProbe = guitardsp::LatencyProbe::makeSequence();
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

void AudioEngine::initialise()
{
    if (deviceCallbackAttached.load(std::memory_order_relaxed)) return;
    const auto error = deviceManager.initialise(1, 2, nullptr, true);
    if (error.isNotEmpty()) DBG("Audio device initialise error: " + error);
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        juce::AudioDeviceManager::AudioDeviceSetup originalSetup; deviceManager.getAudioDeviceSetup(originalSetup); auto requestedSetup = originalSetup;
        const auto sampleRates = device->getAvailableSampleRates(); const auto bufferSizes = device->getAvailableBufferSizes();
        if (sampleRates.contains(48000.0)) requestedSetup.sampleRate = 48000.0;
        if (bufferSizes.contains(256)) requestedSetup.bufferSize = 256;
        const bool needsChange = requestedSetup.sampleRate != originalSetup.sampleRate || requestedSetup.bufferSize != originalSetup.bufferSize;
        if (needsChange)
        {
            const auto setupError = deviceManager.setAudioDeviceSetup(requestedSetup, true);
            if (setupError.isNotEmpty())
            {
                DBG("Requested audio format unavailable: " + setupError + ". Restoring previous setup.");
                const auto restoreError = deviceManager.setAudioDeviceSetup(originalSetup, true);
                if (restoreError.isNotEmpty()) DBG("Could not restore previous audio setup: " + restoreError);
            }
        }
    }
    deviceManager.addAudioCallback(this); deviceCallbackAttached.store(true, std::memory_order_relaxed);
}

void AudioEngine::shutdown()
{
    if (deviceCallbackAttached.exchange(false, std::memory_order_relaxed)) deviceManager.removeAudioCallback(this);
    deviceManager.closeAudioDevice(); release();
}

void AudioEngine::prepare(double sampleRate, int maximumBlockSize)
{
    currentSampleRate.store(sampleRate, std::memory_order_relaxed);
    liveAnalyzerTaps.clear(); signalChain.setAnalyzerTaps(&liveAnalyzerTaps); signalChain.prepare(sampleRate, maximumBlockSize);
    {
        const juce::ScopedLock lock(analyzerLock); analyzerPrepared.store(false, std::memory_order_relaxed); testAnalyzerTaps.clear();
        analyzerSignalChain.setAnalysisMode(true); analyzerSignalChain.setAnalyzerTaps(&testAnalyzerTaps); analyzerSignalChain.prepare(sampleRate, analyzerRenderSamples);
        analyzerBuffer.setSize(2, analyzerRenderSamples, false, false, true); analyzerPrepared.store(true, std::memory_order_release);
    }
    ioBuffer.setSize(2, maximumBlockSize, false, false, true);
    latencyCapture.setSize(1, latencyCaptureLength, false, false, true);
    latencyCapture.clear(); latencyWriteIndex = 0;
    latencyState.store(0, std::memory_order_release);
    measuredRoundTripSamples.store(-1, std::memory_order_relaxed);
    latencyCorrelation.store(0.0f, std::memory_order_relaxed);
    for (auto& meter : inputMeters) meter.reset(); for (auto& meter : outputMeters) meter.reset();
    for (auto& value : inputRmsDb) value.store(-100.0f, std::memory_order_relaxed); for (auto& value : outputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
}

void AudioEngine::release()
{
    latencyState.store(0, std::memory_order_release);
    signalChain.reset(); liveAnalyzerTaps.clear(); ioBuffer.setSize(2, 0);
    { const juce::ScopedLock lock(analyzerLock); analyzerPrepared.store(false, std::memory_order_relaxed); analyzerSignalChain.reset(); testAnalyzerTaps.clear(); analyzerBuffer.setSize(2, 0); }
    latencyCapture.setSize(1, 0);
    for (auto& meter : inputMeters) meter.reset(); for (auto& meter : outputMeters) meter.reset();
    for (auto& value : inputRmsDb) value.store(-100.0f, std::memory_order_relaxed); for (auto& value : outputRmsDb) value.store(-100.0f, std::memory_order_relaxed);
}

void AudioEngine::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0 || startSample < 0 || startSample + numSamples > buffer.getNumSamples()) return;
    measureBlock(buffer, startSample, numSamples, inputMeters, inputRmsDb); signalChain.process(buffer, startSample, numSamples); measureBlock(buffer, startSample, numSamples, outputMeters, outputRmsDb);
}

void AudioEngine::setLiveAnalyzerEnabled(bool enabled) noexcept
{
    if (enabled) liveAnalyzerTaps.clear();
    liveAnalyzerTaps.setEnabled(enabled);
}

void AudioEngine::copyLiveAnalyzerTap(SignalTapBuffer::TapPoint point, std::vector<float>& destination, int samples) const { liveAnalyzerTaps.copyLatest(point, destination, samples); }
void AudioEngine::copyTestAnalyzerTap(SignalTapBuffer::TapPoint point, std::vector<float>& destination, int samples) const { testAnalyzerTaps.copyLatest(point, destination, samples); }

void AudioEngine::renderAnalyzerTest(AnalyzerTestMode mode, float frequencyHz, float levelDb, float sweepStartHz, float sweepEndHz)
{
    if (!analyzerPrepared.load(std::memory_order_acquire)) return;
    const juce::ScopedLock lock(analyzerLock);
    if (!analyzerPrepared.load(std::memory_order_relaxed) || analyzerBuffer.getNumSamples() < analyzerRenderSamples) return;
    signalChain.copySettingsTo(analyzerSignalChain);
    copyAnalyzerExternalIr(signalChain, analyzerSignalChain);
    analyzerSignalChain.reset(); testAnalyzerTaps.clear(); analyzerBuffer.clear();
    const double fs = juce::jmax(8000.0, currentSampleRate.load(std::memory_order_relaxed));
    const float gain = juce::Decibels::decibelsToGain(juce::jlimit(-80.0f, 0.0f, levelDb)); const float nyquistSafe = static_cast<float>(fs * 0.45);
    frequencyHz = juce::jlimit(1.0f, nyquistSafe, frequencyHz); sweepStartHz = juce::jlimit(1.0f, nyquistSafe, sweepStartHz); sweepEndHz = juce::jlimit(sweepStartHz, nyquistSafe, sweepEndHz);
    double phase = 0.0;
    for (int i = 0; i < analyzerRenderSamples; ++i)
    {
        float frequency = frequencyHz;
        if (mode == AnalyzerTestMode::sweep) { const double t = static_cast<double>(i) / static_cast<double>(analyzerRenderSamples - 1); frequency = static_cast<float>(sweepStartHz * std::pow(sweepEndHz / sweepStartHz, t)); }
        phase += juce::MathConstants<double>::twoPi * static_cast<double>(frequency) / fs; if (phase > juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        const float sample = gain * static_cast<float>(std::sin(phase)); analyzerBuffer.setSample(0, i, sample); analyzerBuffer.setSample(1, i, sample);
    }
    analyzerSignalChain.process(analyzerBuffer, 0, analyzerRenderSamples);
}

bool AudioEngine::startRoundTripLatencyMeasurement()
{
    if (latencyState.load(std::memory_order_acquire) == 1 || latencyCapture.getNumSamples() < latencyCaptureLength) return false;
    latencyCapture.clear(); latencyWriteIndex = 0;
    measuredRoundTripSamples.store(-1, std::memory_order_relaxed); latencyCorrelation.store(0.0f, std::memory_order_relaxed);
    latencyState.store(1, std::memory_order_release); return true;
}

void AudioEngine::processLatencyProbeBlock(const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples) noexcept
{
    for (int ch = 0; ch < numOutputChannels; ++ch) if (outputChannelData[ch] != nullptr) juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    auto* capture = latencyCapture.getNumSamples() >= latencyCaptureLength ? latencyCapture.getWritePointer(0) : nullptr;
    for (int i = 0; i < numSamples; ++i)
    {
        const int index = latencyWriteIndex++;
        if (index >= latencyCaptureLength) break;
        if (capture != nullptr) capture[index] = (numInputChannels > 0 && inputChannelData[0] != nullptr) ? inputChannelData[0][i] : 0.0f;
        const int probeIndex = index - latencyProbeLeadIn;
        if (probeIndex >= 0 && probeIndex < (int)latencyProbe.size() && numOutputChannels > 0 && outputChannelData[0] != nullptr)
            outputChannelData[0][i] = latencyProbe[(size_t)probeIndex];
    }
    if (latencyWriteIndex >= latencyCaptureLength) latencyState.store(2, std::memory_order_release);
}

void AudioEngine::finaliseRoundTripLatencyMeasurement()
{
    if (latencyState.load(std::memory_order_acquire) != 2) return;
    float correlation = 0.0f;
    const int detectedStart = guitardsp::LatencyProbe::estimateDelaySamples(latencyProbe, latencyCapture.getReadPointer(0), latencyCapture.getNumSamples(), correlation);
    const int delay = detectedStart >= 0 ? detectedStart - latencyProbeLeadIn : -1;
    latencyCorrelation.store(correlation, std::memory_order_relaxed);
    measuredRoundTripSamples.store(std::abs(correlation) >= 0.30f && delay >= 0 ? delay : -1, std::memory_order_relaxed);
    latencyState.store(3, std::memory_order_release);
}

int AudioEngine::measureCurrentDspLatencySamples()
{
    if (!analyzerPrepared.load(std::memory_order_acquire)) return -1;
    const juce::ScopedLock lock(analyzerLock);
    if (!analyzerPrepared.load(std::memory_order_relaxed) || analyzerBuffer.getNumSamples() < analyzerRenderSamples) return -1;
    signalChain.copySettingsTo(analyzerSignalChain);
    copyAnalyzerExternalIr(signalChain, analyzerSignalChain);
    analyzerSignalChain.reset(); testAnalyzerTaps.clear(); analyzerBuffer.clear();
    constexpr int impulseIndex = 256;
    analyzerBuffer.setSample(0, impulseIndex, 0.20f); analyzerBuffer.setSample(1, impulseIndex, 0.20f);
    analyzerSignalChain.process(analyzerBuffer, 0, analyzerRenderSamples);
    const auto* data = analyzerBuffer.getReadPointer(0); float peak = 0.0f; int peakIndex = impulseIndex;
    for (int i = impulseIndex; i < analyzerRenderSamples; ++i) { const float magnitude = std::abs(data[i]); if (magnitude > peak) { peak = magnitude; peakIndex = i; } }
    return peak > 1.0e-8f ? peakIndex - impulseIndex : -1;
}

void AudioEngine::audioDeviceAboutToStart(juce::AudioIODevice* device)
{
    if (device != nullptr)
    {
        reportedInputLatencySamples.store(device->getInputLatencyInSamples(), std::memory_order_relaxed);
        reportedOutputLatencySamples.store(device->getOutputLatencyInSamples(), std::memory_order_relaxed);
        prepare(device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());
    }
}

void AudioEngine::audioDeviceStopped()
{
    reportedInputLatencySamples.store(0, std::memory_order_relaxed); reportedOutputLatencySamples.store(0, std::memory_order_relaxed); release();
}

void AudioEngine::audioDeviceIOCallbackWithContext(const float* const* inputChannelData,int numInputChannels,float* const* outputChannelData,int numOutputChannels,int numSamples,const juce::AudioIODeviceCallbackContext&)
{
    if (latencyState.load(std::memory_order_acquire) == 1) { processLatencyProbeBlock(inputChannelData, numInputChannels, outputChannelData, numOutputChannels, numSamples); return; }
    ioBuffer.setSize(2, numSamples, false, false, true); ioBuffer.clear();
    const int inputsToCopy = juce::jmin(2, numInputChannels);
    for (int ch = 0; ch < inputsToCopy; ++ch) if (inputChannelData[ch] != nullptr) juce::FloatVectorOperations::copy(ioBuffer.getWritePointer(ch), inputChannelData[ch], numSamples);
    process(ioBuffer, 0, numSamples);
    for (int ch = 0; ch < numOutputChannels; ++ch)
    {
        if (outputChannelData[ch] == nullptr) continue;
        if (ch < ioBuffer.getNumChannels()) juce::FloatVectorOperations::copy(outputChannelData[ch], ioBuffer.getReadPointer(ch), numSamples);
        else juce::FloatVectorOperations::clear(outputChannelData[ch], numSamples);
    }
}

void AudioEngine::setInputGainDb(float gainDb) noexcept { signalChain.setInputGainDb(gainDb); }
void AudioEngine::setOutputGainDb(float gainDb) noexcept { signalChain.setOutputGainDb(gainDb); }
void AudioEngine::setBypass(bool enabled) noexcept { signalChain.setBypass(enabled); }
void AudioEngine::setMonoInputToStereo(bool enabled) noexcept { signalChain.setMonoInputToStereo(enabled); }
float AudioEngine::getInputPeak(int channel) const noexcept { return (channel >= 0 && channel < 2) ? inputMeters[(size_t)channel].getDb() : -100.0f; }
float AudioEngine::getInputRms(int channel) const noexcept { return (channel >= 0 && channel < 2) ? inputRmsDb[(size_t)channel].load(std::memory_order_relaxed) : -100.0f; }
float AudioEngine::getOutputPeak(int channel) const noexcept { return (channel >= 0 && channel < 2) ? outputMeters[(size_t)channel].getDb() : -100.0f; }
float AudioEngine::getOutputRms(int channel) const noexcept { return (channel >= 0 && channel < 2) ? outputRmsDb[(size_t)channel].load(std::memory_order_relaxed) : -100.0f; }

void AudioEngine::measureBlock(const juce::AudioBuffer<float>& buffer,int startSample,int numSamples,std::array<LevelMeter, 2>& peakMeters,std::array<std::atomic<float>, 2>& rmsDb)
{
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int ch = 0; ch < 2; ++ch)
    {
        if (ch >= channels) { peakMeters[(size_t)ch].pushPeak(0.0f); rmsDb[(size_t)ch].store(-100.0f, std::memory_order_relaxed); continue; }
        const auto* data = buffer.getReadPointer(ch, startSample); float peak = 0.0f; double sumSquares = 0.0;
        for (int i = 0; i < numSamples; ++i) { const float x = data[i]; peak = juce::jmax(peak, std::abs(x)); sumSquares += static_cast<double>(x) * static_cast<double>(x); }
        peakMeters[(size_t)ch].pushPeak(peak); const float rms = numSamples > 0 ? static_cast<float>(std::sqrt(sumSquares / static_cast<double>(numSamples))) : 0.0f;
        rmsDb[(size_t)ch].store(juce::Decibels::gainToDecibels(rms, -100.0f), std::memory_order_relaxed);
    }
}
