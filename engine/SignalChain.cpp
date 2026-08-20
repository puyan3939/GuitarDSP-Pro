#include "SignalChain.h"
#include <array>
#include <cmath>

void SignalChain::prepare(double sampleRate, int maximumBlockSize)
{
    inputGain.reset(sampleRate, 0.040);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));
    startupFade.reset(sampleRate, 0.500);
    startupFade.setCurrentAndTargetValue(0.0f);
    startupFade.setTargetValue(1.0f);

    ampWorkBuffer.setSize(1, maximumBlockSize, false, false, true);
    parallelTapBuffer.setSize(1, maximumBlockSize, false, false, true);
    ampEngine.prepare(sampleRate, maximumBlockSize);
    hqAmpEngine.prepare(sampleRate, maximumBlockSize);
    hqEffects.prepare(sampleRate, maximumBlockSize);
    cabMic.prepare(sampleRate, maximumBlockSize);
    cabMic.setEnabled(false);
    parallelRig.prepare(sampleRate, maximumBlockSize);
    limiter.prepare(sampleRate);
    limiter.setCeiling(0.28f);
    limiter.setOutputGainDb(-18.0f);
}

void SignalChain::reset()
{
    ampEngine.reset();
    hqAmpEngine.reset();
    hqEffects.reset();
    cabMic.reset();
    parallelRig.reset();
    limiter.reset();
    ampWorkBuffer.clear();
    parallelTapBuffer.clear();
}

void SignalChain::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(2, buffer.getNumChannels());
    if (channels <= 0 || numSamples <= 0) return;

    if (monoInputToStereo.load(std::memory_order_relaxed) && channels >= 2)
        copyDetectedMonoToStereo(buffer, startSample, numSamples);

    if (!bypass.load(std::memory_order_relaxed))
    {
        applyInputGain(buffer, startSample, numSamples);

        const auto& routing = hqEffects.parallelRigControl();
        const bool useParallelRig = routing.enabled.load(std::memory_order_relaxed);
        if (useParallelRig)
            captureParallelTap(buffer, startSample, numSamples);

        hqEffects.processPreAmp(buffer, startSample, numSamples);
        if (getAmpMode() == AmpMode::hq) processHQAmp(buffer, startSample, numSamples);
        else processLegacyAmp(buffer, startSample, numSamples);
        cabMic.process(buffer, startSample, numSamples);

        // The split tap is taken immediately after input gain, before pedals and
        // amp distortion.  MAIN remains the normal rig; CLEAN and SUB are built
        // from the untouched tap and recombined before global post effects.
        if (useParallelRig)
            parallelRig.process(parallelTapBuffer, buffer, startSample, numSamples, routing);

        hqEffects.processPostAmp(buffer, startSample, numSamples);
    }

    applyStartupFadeAndLimiter(buffer, startSample, numSamples);
}

void SignalChain::copyDetectedMonoToStereo(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    auto* left = buffer.getWritePointer(0, startSample);
    auto* right = buffer.getWritePointer(1, startSample);
    float peakLeft = 0.0f, peakRight = 0.0f;
    for (int i = 0; i < numSamples; ++i){peakLeft = juce::jmax(peakLeft, std::abs(left[i]));peakRight = juce::jmax(peakRight, std::abs(right[i]));}
    const bool sourceIsRight = peakRight > peakLeft;
    auto* source = sourceIsRight ? right : left;
    auto* destination = sourceIsRight ? left : right;
    juce::FloatVectorOperations::copy(destination, source, numSamples);
}

void SignalChain::applyInputGain(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i){const float gain = inputGain.getNextValue();for (int ch = 0; ch < channels; ++ch) buffer.getWritePointer(ch, startSample)[i] *= gain;}
}

void SignalChain::captureParallelTap(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    // Amp processing is mono downstream, so the routing tap is intentionally one
    // channel as well.  copyDetectedMonoToStereo() has already selected the live
    // input when mono-to-stereo mode is active.
    parallelTapBuffer.copyFrom(0, 0, buffer, 0, startSample, numSamples);
}

void SignalChain::processLegacyAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    ampWorkBuffer.setSize(1, numSamples, false, false, true);ampWorkBuffer.copyFrom(0, 0, buffer, 0, startSample, numSamples);ampEngine.process(ampWorkBuffer);
    const int channels = juce::jmin(2, buffer.getNumChannels());for (int ch = 0; ch < channels; ++ch) buffer.copyFrom(ch, startSample, ampWorkBuffer, 0, 0, numSamples);
}

void SignalChain::processHQAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    ampWorkBuffer.setSize(1, numSamples, false, false, true);ampWorkBuffer.copyFrom(0, 0, buffer, 0, startSample, numSamples);hqAmpEngine.process(ampWorkBuffer);
    const int channels = juce::jmin(2, buffer.getNumChannels());for (int ch = 0; ch < channels; ++ch) buffer.copyFrom(ch, startSample, ampWorkBuffer, 0, 0, numSamples);
}

void SignalChain::applyStartupFadeAndLimiter(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int i = 0; i < numSamples; ++i){const float fade = startupFade.getNextValue();for (int ch = 0; ch < channels; ++ch){float& sample = buffer.getWritePointer(ch, startSample)[i];sample = limiter.processSample(sample * fade);}}
}

void SignalChain::setInputGainDb(float gainDb) noexcept { inputGainDb.store(juce::jlimit(-36.0f, 18.0f, gainDb), std::memory_order_relaxed); }
void SignalChain::setOutputGainDb(float gainDb) noexcept { limiter.setOutputGainDb(gainDb); }
void SignalChain::setBypass(bool shouldBypass) noexcept { bypass.store(shouldBypass, std::memory_order_relaxed); }
void SignalChain::setMonoInputToStereo(bool enabled) noexcept { monoInputToStereo.store(enabled, std::memory_order_relaxed); }
