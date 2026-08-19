#include "SignalChain.h"
#include <array>
#include <cmath>

namespace
{
template <typename T>
void copyAtomic(const std::atomic<T>& source, std::atomic<T>& destination)
{
    destination.store(source.load(std::memory_order_relaxed), std::memory_order_relaxed);
}
}

void SignalChain::prepare(double sampleRate, int maximumBlockSize)
{
    inputGain.reset(sampleRate, 0.040);
    inputGain.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));
    startupFade.reset(sampleRate, 0.500);
    if (analysisMode)
        startupFade.setCurrentAndTargetValue(1.0f);
    else
    {
        startupFade.setCurrentAndTargetValue(0.0f);
        startupFade.setTargetValue(1.0f);
    }

    ampWorkBuffer.setSize(1, maximumBlockSize, false, false, true);
    ampEngine.prepare(sampleRate, maximumBlockSize);
    hqAmpEngine.prepare(sampleRate, maximumBlockSize);
    hqEffects.prepare(sampleRate, maximumBlockSize);
    cabMic.prepare(sampleRate, maximumBlockSize);
    cabMic.setEnabled(false);
    limiter.prepare(sampleRate);
    limiter.setCeiling(0.28f);
    limiter.setOutputGainDb(outputGainDb.load(std::memory_order_relaxed));
}

void SignalChain::reset()
{
    ampEngine.reset();
    hqAmpEngine.reset();
    hqEffects.reset();
    cabMic.reset();
    limiter.reset();
    ampWorkBuffer.clear();
}

void SignalChain::pushTap(SignalTapBuffer::TapPoint point,
                          const juce::AudioBuffer<float>& buffer,
                          int startSample,
                          int numSamples) noexcept
{
    if (analyzerTaps != nullptr)
        analyzerTaps->pushBlock(point, buffer, startSample, numSamples);
}

void SignalChain::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels = juce::jmin(2, buffer.getNumChannels());
    if (channels <= 0 || numSamples <= 0) return;

    pushTap(SignalTapBuffer::TapPoint::input, buffer, startSample, numSamples);

    if (monoInputToStereo.load(std::memory_order_relaxed) && channels >= 2)
        copyDetectedMonoToStereo(buffer, startSample, numSamples);

    if (!bypass.load(std::memory_order_relaxed))
    {
        applyInputGain(buffer, startSample, numSamples);
        hqEffects.processPreAmp(buffer, startSample, numSamples);
        pushTap(SignalTapBuffer::TapPoint::postPedals, buffer, startSample, numSamples);

        if (getAmpMode() == AmpMode::hq) processHQAmp(buffer, startSample, numSamples);
        else processLegacyAmp(buffer, startSample, numSamples);
        pushTap(SignalTapBuffer::TapPoint::postAmp, buffer, startSample, numSamples);

        cabMic.process(buffer, startSample, numSamples);
        pushTap(SignalTapBuffer::TapPoint::postCab, buffer, startSample, numSamples);

        hqEffects.processPostAmp(buffer, startSample, numSamples);
    }
    else
    {
        pushTap(SignalTapBuffer::TapPoint::postPedals, buffer, startSample, numSamples);
        pushTap(SignalTapBuffer::TapPoint::postAmp, buffer, startSample, numSamples);
        pushTap(SignalTapBuffer::TapPoint::postCab, buffer, startSample, numSamples);
    }

    applyStartupFadeAndLimiter(buffer, startSample, numSamples);
    pushTap(SignalTapBuffer::TapPoint::output, buffer, startSample, numSamples);
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
void SignalChain::setOutputGainDb(float gainDb) noexcept { outputGainDb.store(gainDb, std::memory_order_relaxed); limiter.setOutputGainDb(gainDb); }
void SignalChain::setBypass(bool shouldBypass) noexcept { bypass.store(shouldBypass, std::memory_order_relaxed); }
void SignalChain::setMonoInputToStereo(bool enabled) noexcept { monoInputToStereo.store(enabled, std::memory_order_relaxed); }

void SignalChain::copySettingsTo(SignalChain& destination)
{
    destination.setInputGainDb(inputGainDb.load(std::memory_order_relaxed));
    destination.setOutputGainDb(outputGainDb.load(std::memory_order_relaxed));
    destination.setBypass(bypass.load(std::memory_order_relaxed));
    destination.setMonoInputToStereo(monoInputToStereo.load(std::memory_order_relaxed));
    destination.setAmpMode(getAmpMode());

    destination.getAmpEngine().setParameters(ampEngine.getParameters());
    destination.getHQAmpEngine().setParameters(hqAmpEngine.getParameters());
    destination.getCabMicEngine().setEnabled(cabMic.isEnabled());
    destination.getCabMicEngine().setParameters(cabMic.getParameters());

    auto& srcRack = hqEffects;
    auto& dstRack = destination.getHQEffectsRack();
    dstRack.setDynamicsMode(srcRack.getDynamicsMode());
    dstRack.setModulationMode(srcRack.getModulationMode());
    dstRack.setDelayEnabled(srcRack.isDelayEnabled());
    dstRack.setReverbEnabled(srcRack.isReverbEnabled());

    for (int i = 0; i < guitardsp::hq::HQEffectsRack::pedalSlots; ++i)
    {
        auto& s = srcRack.pedalSlot(i);
        auto& d = dstRack.pedalSlot(i);
        copyAtomic(s.enabled, d.enabled); copyAtomic(s.model, d.model);
        copyAtomic(s.drive, d.drive); copyAtomic(s.tone, d.tone); copyAtomic(s.levelDb, d.levelDb); copyAtomic(s.mix, d.mix);
        copyAtomic(s.aux1, d.aux1); copyAtomic(s.aux2, d.aux2); copyAtomic(s.aux3, d.aux3);
    }

    {
        auto& s = srcRack.gateControl(); auto& d = dstRack.gateControl();
        copyAtomic(s.thresholdDb,d.thresholdDb); copyAtomic(s.rangeDb,d.rangeDb); copyAtomic(s.ratio,d.ratio);
        copyAtomic(s.attackMs,d.attackMs); copyAtomic(s.holdMs,d.holdMs); copyAtomic(s.releaseMs,d.releaseMs); copyAtomic(s.hysteresisDb,d.hysteresisDb);
        copyAtomic(s.sidechainHpHz,d.sidechainHpHz); copyAtomic(s.sidechainLpHz,d.sidechainLpHz);
    }
    {
        auto& s = srcRack.studioCompControl(); auto& d = dstRack.studioCompControl();
        copyAtomic(s.thresholdDb,d.thresholdDb); copyAtomic(s.ratio,d.ratio); copyAtomic(s.attackMs,d.attackMs); copyAtomic(s.releaseMs,d.releaseMs);
        copyAtomic(s.kneeDb,d.kneeDb); copyAtomic(s.makeupDb,d.makeupDb); copyAtomic(s.mix,d.mix); copyAtomic(s.rms,d.rms);
    }
    {
        auto& s = srcRack.guitarCompControl(); auto& d = dstRack.guitarCompControl();
        copyAtomic(s.sustain,d.sustain); copyAtomic(s.attack,d.attack); copyAtomic(s.blend,d.blend); copyAtomic(s.levelDb,d.levelDb);
    }
    {
        auto& s = srcRack.modulationControl(); auto& d = dstRack.modulationControl();
        copyAtomic(s.rateHz,d.rateHz); copyAtomic(s.depth,d.depth); copyAtomic(s.mix,d.mix); copyAtomic(s.feedback,d.feedback); copyAtomic(s.manual,d.manual); copyAtomic(s.shape,d.shape);
    }
    {
        auto& s = srcRack.delayControl(); auto& d = dstRack.delayControl();
        copyAtomic(s.flavor,d.flavor); copyAtomic(s.timeMs,d.timeMs); copyAtomic(s.feedback,d.feedback); copyAtomic(s.mix,d.mix);
        copyAtomic(s.lowCutHz,d.lowCutHz); copyAtomic(s.highCutHz,d.highCutHz); copyAtomic(s.drive,d.drive); copyAtomic(s.wow,d.wow); copyAtomic(s.flutter,d.flutter); copyAtomic(s.age,d.age);
    }
    {
        auto& s = srcRack.reverbControl(); auto& d = dstRack.reverbControl();
        copyAtomic(s.flavor,d.flavor); copyAtomic(s.size,d.size); copyAtomic(s.decay,d.decay); copyAtomic(s.damping,d.damping);
        copyAtomic(s.preDelayMs,d.preDelayMs); copyAtomic(s.mix,d.mix); copyAtomic(s.mod,d.mod); copyAtomic(s.drip,d.drip);
    }
}
