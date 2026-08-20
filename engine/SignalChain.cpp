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
    if (analysisMode) startupFade.setCurrentAndTargetValue(1.0f);
    else { startupFade.setCurrentAndTargetValue(0.0f); startupFade.setTargetValue(1.0f); }

    ampWorkBuffer.setSize(1, maximumBlockSize, false, false, true);
    cleanRouteBuffer.setSize(1, maximumBlockSize, false, false, true);
    subRouteBuffer.setSize(1, maximumBlockSize, false, false, true);
    expressionWorkBuffer.setSize(1, maximumBlockSize, false, false, true);
    ampEngine.prepare(sampleRate, maximumBlockSize);
    hqAmpEngine.prepare(sampleRate, maximumBlockSize);
    hqEffects.prepare(sampleRate, maximumBlockSize);
    cabMic.prepare(sampleRate, maximumBlockSize);
    cabMic.setEnabled(false);
    parallelRig.prepare(sampleRate, maximumBlockSize);
    for(auto& p:expressionPitch) p.prepare(sampleRate,maximumBlockSize);
    for(auto& l:inputLoading) l.prepare(sampleRate);
    dualDelay.prepare(sampleRate,maximumBlockSize);
    limiter.prepare(sampleRate);
    limiter.setCeiling(0.28f);
    limiter.setOutputGainDb(outputGainDb.load(std::memory_order_relaxed));
}

void SignalChain::reset()
{
    ampEngine.reset(); hqAmpEngine.reset(); hqEffects.reset(); cabMic.reset(); parallelRig.reset();
    for(auto& p:expressionPitch) p.reset(); for(auto& l:inputLoading) l.reset(); dualDelay.reset(); limiter.reset();
    ampWorkBuffer.clear(); cleanRouteBuffer.clear(); subRouteBuffer.clear(); expressionWorkBuffer.clear();
}

void SignalChain::pushTap(SignalTapBuffer::TapPoint point,const juce::AudioBuffer<float>& buffer,int startSample,int numSamples) noexcept
{
    if(analyzerTaps!=nullptr) analyzerTaps->pushBlock(point,buffer,startSample,numSamples);
}

void SignalChain::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels=juce::jmin(2,buffer.getNumChannels()); if(channels<=0||numSamples<=0)return;
    pushTap(SignalTapBuffer::TapPoint::input,buffer,startSample,numSamples);
    applySceneRequests();

    if(monoInputToStereo.load(std::memory_order_relaxed)&&channels>=2)
        copyDetectedMonoToStereo(buffer,startSample,numSamples);

    if(!bypass.load(std::memory_order_relaxed))
    {
        applyInputLoading(buffer,startSample,numSamples);
        applyInputGain(buffer,startSample,numSamples);

        const auto& routing=hqEffects.parallelRigControl();
        const bool useParallelRig=routing.enabled.load(std::memory_order_relaxed);
        if(useParallelRig)
        {
            captureParallelTaps(buffer,startSample,numSamples);
            processExpressionPitchRoute(cleanRouteBuffer,0,numSamples,guitardsp::hq::HQEffectsRack::routeClean,1);
            processExpressionPitchRoute(subRouteBuffer,0,numSamples,guitardsp::hq::HQEffectsRack::routeSub,2);
            hqEffects.processPedalRoute(cleanRouteBuffer,0,numSamples,guitardsp::hq::HQEffectsRack::routeClean);
            hqEffects.processPedalRoute(subRouteBuffer,0,numSamples,guitardsp::hq::HQEffectsRack::routeSub);
        }

        processExpressionPitchRoute(buffer,startSample,numSamples,guitardsp::hq::HQEffectsRack::routeMain,0);
        hqEffects.processPreAmp(buffer,startSample,numSamples);
        pushTap(SignalTapBuffer::TapPoint::postPedals,buffer,startSample,numSamples);

        if(getAmpMode()==AmpMode::hq) processHQAmp(buffer,startSample,numSamples); else processLegacyAmp(buffer,startSample,numSamples);
        pushTap(SignalTapBuffer::TapPoint::postAmp,buffer,startSample,numSamples);

        cabMic.process(buffer,startSample,numSamples);
        pushTap(SignalTapBuffer::TapPoint::postCab,buffer,startSample,numSamples);

        if(useParallelRig)
            parallelRig.processBuses(cleanRouteBuffer,subRouteBuffer,buffer,startSample,numSamples,routing);

        hqEffects.processPostAmp(buffer,startSample,numSamples);
        if(getOutputMode()==OutputMode::stereoMix)
            dualDelay.process(buffer,startSample,numSamples,dualDelayControl);
        else if(useParallelRig)
            applyStemOutput(buffer,startSample,numSamples);
    }
    else
    {
        pushTap(SignalTapBuffer::TapPoint::postPedals,buffer,startSample,numSamples);
        pushTap(SignalTapBuffer::TapPoint::postAmp,buffer,startSample,numSamples);
        pushTap(SignalTapBuffer::TapPoint::postCab,buffer,startSample,numSamples);
    }

    applyStartupFadeAndLimiter(buffer,startSample,numSamples);
    pushTap(SignalTapBuffer::TapPoint::output,buffer,startSample,numSamples);
}

void SignalChain::copyDetectedMonoToStereo(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    auto* left=buffer.getWritePointer(0,startSample);auto* right=buffer.getWritePointer(1,startSample);float peakLeft=0,peakRight=0;
    for(int i=0;i<numSamples;++i){peakLeft=juce::jmax(peakLeft,std::abs(left[i]));peakRight=juce::jmax(peakRight,std::abs(right[i]));}
    const bool sourceIsRight=peakRight>peakLeft;auto*source=sourceIsRight?right:left;auto*destination=sourceIsRight?left:right;juce::FloatVectorOperations::copy(destination,source,numSamples);
}

void SignalChain::applyInputLoading(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    if(!inputLoadingControl.enabled.load(std::memory_order_relaxed))return;
    const int channels=juce::jmin(2,buffer.getNumChannels());
    for(int ch=0;ch<channels;++ch){auto*d=buffer.getWritePointer(ch,startSample);for(int i=0;i<numSamples;++i)d[i]=inputLoading[ch].process(d[i],inputLoadingControl);}
}

void SignalChain::applyInputGain(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    inputGain.setTargetValue(juce::Decibels::decibelsToGain(inputGainDb.load(std::memory_order_relaxed)));const int channels=juce::jmin(2,buffer.getNumChannels());
    for(int i=0;i<numSamples;++i){const float gain=inputGain.getNextValue();for(int ch=0;ch<channels;++ch)buffer.getWritePointer(ch,startSample)[i]*=gain;}
}

void SignalChain::captureParallelTaps(const juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    cleanRouteBuffer.copyFrom(0,0,buffer,0,startSample,numSamples);
    subRouteBuffer.copyFrom(0,0,buffer,0,startSample,numSamples);
}

void SignalChain::processExpressionPitchRoute(juce::AudioBuffer<float>& buffer,int startSample,int numSamples,int routeBit,int stateIndex)
{
    if(!expressionPitchControl.enabled.load(std::memory_order_relaxed)||(expressionPitchControl.routeMask.load(std::memory_order_relaxed)&routeBit)==0)return;
    stateIndex=juce::jlimit(0,2,stateIndex);expressionWorkBuffer.copyFrom(0,0,buffer,0,startSample,numSamples);expressionPitch[(size_t)stateIndex].process(expressionWorkBuffer,expressionPitchControl);buffer.copyFrom(0,startSample,expressionWorkBuffer,0,0,numSamples);
    const int channels=juce::jmin(2,buffer.getNumChannels());for(int ch=1;ch<channels;++ch)buffer.copyFrom(ch,startSample,buffer,0,startSample,numSamples);
}

void SignalChain::processLegacyAmp(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    ampWorkBuffer.setSize(1,numSamples,false,false,true);ampWorkBuffer.copyFrom(0,0,buffer,0,startSample,numSamples);ampEngine.process(ampWorkBuffer);const int channels=juce::jmin(2,buffer.getNumChannels());for(int ch=0;ch<channels;++ch)buffer.copyFrom(ch,startSample,ampWorkBuffer,0,0,numSamples);
}

void SignalChain::processHQAmp(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    ampWorkBuffer.setSize(1,numSamples,false,false,true);ampWorkBuffer.copyFrom(0,0,buffer,0,startSample,numSamples);hqAmpEngine.process(ampWorkBuffer);const int channels=juce::jmin(2,buffer.getNumChannels());for(int ch=0;ch<channels;++ch)buffer.copyFrom(ch,startSample,ampWorkBuffer,0,0,numSamples);
}

void SignalChain::applySceneRequests()
{
    int index=0;if(!scenes.consumeRequest(index))return;const auto&s=scenes.scene(index);auto&r=hqEffects.parallelRigControl();r.enabled.store(s.parallelEnabled);r.mainLevelDb.store(s.mainDb);r.cleanLevelDb.store(s.cleanDb);r.subLevelDb.store(s.subDb);
    expressionPitchControl.enabled.store(s.expressionPitchEnabled);expressionPitchControl.expression.store(s.expression);expressionPitchControl.semitones.store(s.pitchSemitones);dualDelayControl.enabled.store(s.dualDelayEnabled);
    for(int i=0;i<guitardsp::hq::HQEffectsRack::pedalSlots;++i){auto&p=hqEffects.pedalSlot(i);p.enabled.store(s.pedalEnabled[(size_t)i]);p.routeMask.store(juce::jlimit(1,7,s.pedalRouteMask[(size_t)i]));}
}

void SignalChain::applyStemOutput(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    if(buffer.getNumChannels()<2)return;
    parallelRig.copyMainCleanStemTo(buffer,0,startSample,numSamples);
    parallelRig.copySubStemTo(buffer,1,startSample,numSamples,hqEffects.parallelRigControl().subLevelDb.load());
}

void SignalChain::applyStartupFadeAndLimiter(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    const int channels=juce::jmin(2,buffer.getNumChannels());for(int i=0;i<numSamples;++i){const float fade=startupFade.getNextValue();for(int ch=0;ch<channels;++ch){float&sample=buffer.getWritePointer(ch,startSample)[i];sample=limiter.processSample(sample*fade);}}
}

void SignalChain::setInputGainDb(float gainDb) noexcept { inputGainDb.store(juce::jlimit(-36.0f,18.0f,gainDb),std::memory_order_relaxed); }
void SignalChain::setOutputGainDb(float gainDb) noexcept { outputGainDb.store(gainDb,std::memory_order_relaxed); limiter.setOutputGainDb(gainDb); }
void SignalChain::setBypass(bool shouldBypass) noexcept { bypass.store(shouldBypass,std::memory_order_relaxed); }
void SignalChain::setMonoInputToStereo(bool enabled) noexcept { monoInputToStereo.store(enabled,std::memory_order_relaxed); }

void SignalChain::copySettingsTo(SignalChain& destination)
{
    destination.setInputGainDb(inputGainDb.load(std::memory_order_relaxed));
    destination.setOutputGainDb(outputGainDb.load(std::memory_order_relaxed));
    destination.setBypass(bypass.load(std::memory_order_relaxed));
    destination.setMonoInputToStereo(monoInputToStereo.load(std::memory_order_relaxed));
    destination.setAmpMode(getAmpMode()); destination.setOutputMode(getOutputMode());
    destination.getAmpEngine().setParameters(ampEngine.getParameters());
    destination.getHQAmpEngine().setParameters(hqAmpEngine.getParameters());
    destination.getCabMicEngine().setEnabled(cabMic.isEnabled()); destination.getCabMicEngine().setParameters(cabMic.getParameters());

    auto& srcRack=hqEffects; auto& dstRack=destination.getHQEffectsRack();
    dstRack.setDynamicsMode(srcRack.getDynamicsMode()); dstRack.setModulationMode(srcRack.getModulationMode()); dstRack.setDelayEnabled(srcRack.isDelayEnabled()); dstRack.setReverbEnabled(srcRack.isReverbEnabled());
    for(int i=0;i<guitardsp::hq::HQEffectsRack::pedalSlots;++i){auto&s=srcRack.pedalSlot(i);auto&d=dstRack.pedalSlot(i);copyAtomic(s.enabled,d.enabled);copyAtomic(s.model,d.model);copyAtomic(s.drive,d.drive);copyAtomic(s.tone,d.tone);copyAtomic(s.levelDb,d.levelDb);copyAtomic(s.mix,d.mix);copyAtomic(s.aux1,d.aux1);copyAtomic(s.aux2,d.aux2);copyAtomic(s.aux3,d.aux3);copyAtomic(s.routeMask,d.routeMask);}
    {auto&s=srcRack.gateControl();auto&d=dstRack.gateControl();copyAtomic(s.thresholdDb,d.thresholdDb);copyAtomic(s.rangeDb,d.rangeDb);copyAtomic(s.ratio,d.ratio);copyAtomic(s.attackMs,d.attackMs);copyAtomic(s.holdMs,d.holdMs);copyAtomic(s.releaseMs,d.releaseMs);copyAtomic(s.hysteresisDb,d.hysteresisDb);copyAtomic(s.sidechainHpHz,d.sidechainHpHz);copyAtomic(s.sidechainLpHz,d.sidechainLpHz);}
    {auto&s=srcRack.studioCompControl();auto&d=dstRack.studioCompControl();copyAtomic(s.thresholdDb,d.thresholdDb);copyAtomic(s.ratio,d.ratio);copyAtomic(s.attackMs,d.attackMs);copyAtomic(s.releaseMs,d.releaseMs);copyAtomic(s.kneeDb,d.kneeDb);copyAtomic(s.makeupDb,d.makeupDb);copyAtomic(s.mix,d.mix);copyAtomic(s.sidechainHpHz,d.sidechainHpHz);copyAtomic(s.rms,d.rms);copyAtomic(s.autoRelease,d.autoRelease);copyAtomic(s.autoMakeup,d.autoMakeup);}
    {auto&s=srcRack.guitarCompControl();auto&d=dstRack.guitarCompControl();copyAtomic(s.sustain,d.sustain);copyAtomic(s.attack,d.attack);copyAtomic(s.blend,d.blend);copyAtomic(s.levelDb,d.levelDb);}
    {auto&s=srcRack.modulationControl();auto&d=dstRack.modulationControl();copyAtomic(s.rateHz,d.rateHz);copyAtomic(s.depth,d.depth);copyAtomic(s.mix,d.mix);copyAtomic(s.feedback,d.feedback);copyAtomic(s.manual,d.manual);copyAtomic(s.shape,d.shape);}
    {auto&s=srcRack.delayControl();auto&d=dstRack.delayControl();copyAtomic(s.flavor,d.flavor);copyAtomic(s.timeMs,d.timeMs);copyAtomic(s.feedback,d.feedback);copyAtomic(s.mix,d.mix);copyAtomic(s.lowCutHz,d.lowCutHz);copyAtomic(s.highCutHz,d.highCutHz);copyAtomic(s.drive,d.drive);copyAtomic(s.wow,d.wow);copyAtomic(s.flutter,d.flutter);copyAtomic(s.age,d.age);}
    {auto&s=srcRack.reverbControl();auto&d=dstRack.reverbControl();copyAtomic(s.flavor,d.flavor);copyAtomic(s.size,d.size);copyAtomic(s.decay,d.decay);copyAtomic(s.damping,d.damping);copyAtomic(s.preDelayMs,d.preDelayMs);copyAtomic(s.mix,d.mix);copyAtomic(s.mod,d.mod);copyAtomic(s.drip,d.drip);}
    {auto&s=srcRack.parallelRigControl();auto&d=dstRack.parallelRigControl();copyAtomic(s.enabled,d.enabled);copyAtomic(s.autoLatencyComp,d.autoLatencyComp);copyAtomic(s.mainLevelDb,d.mainLevelDb);copyAtomic(s.mainDelayMs,d.mainDelayMs);copyAtomic(s.cleanEnabled,d.cleanEnabled);copyAtomic(s.cleanLevelDb,d.cleanLevelDb);copyAtomic(s.cleanHpHz,d.cleanHpHz);copyAtomic(s.cleanLpHz,d.cleanLpHz);copyAtomic(s.cleanPresenceDb,d.cleanPresenceDb);copyAtomic(s.cleanDrive,d.cleanDrive);copyAtomic(s.cleanBassDb,d.cleanBassDb);copyAtomic(s.cleanMidDb,d.cleanMidDb);copyAtomic(s.cleanTrebleDb,d.cleanTrebleDb);copyAtomic(s.cleanDelayMs,d.cleanDelayMs);copyAtomic(s.cleanInvert,d.cleanInvert);copyAtomic(s.subEnabled,d.subEnabled);copyAtomic(s.subLevelDb,d.subLevelDb);copyAtomic(s.subHpHz,d.subHpHz);copyAtomic(s.subLpHz,d.subLpHz);copyAtomic(s.subBodyDb,d.subBodyDb);copyAtomic(s.subDrive,d.subDrive);copyAtomic(s.subBassDb,d.subBassDb);copyAtomic(s.subMidDb,d.subMidDb);copyAtomic(s.subTrebleDb,d.subTrebleDb);copyAtomic(s.subTracking,d.subTracking);copyAtomic(s.subTone,d.subTone);copyAtomic(s.subSmooth,d.subSmooth);copyAtomic(s.subDelayMs,d.subDelayMs);copyAtomic(s.subInvert,d.subInvert);}
    {auto&s=inputLoadingControl;auto&d=destination.getInputLoadingControl();copyAtomic(s.enabled,d.enabled);copyAtomic(s.pickupResistanceOhm,d.pickupResistanceOhm);copyAtomic(s.pickupInductanceH,d.pickupInductanceH);copyAtomic(s.cableCapacitancePf,d.cableCapacitancePf);copyAtomic(s.inputImpedanceOhm,d.inputImpedanceOhm);copyAtomic(s.trimDb,d.trimDb);}
    {auto&s=expressionPitchControl;auto&d=destination.getExpressionPitchControl();copyAtomic(s.enabled,d.enabled);copyAtomic(s.routeMask,d.routeMask);copyAtomic(s.semitones,d.semitones);copyAtomic(s.expression,d.expression);copyAtomic(s.wet,d.wet);copyAtomic(s.dry,d.dry);copyAtomic(s.tracking,d.tracking);copyAtomic(s.tone,d.tone);copyAtomic(s.smooth,d.smooth);}
    {auto&s=dualDelayControl;auto&d=destination.getDualDelayControl();copyAtomic(s.enabled,d.enabled);copyAtomic(s.timeLms,d.timeLms);copyAtomic(s.timeRms,d.timeRms);copyAtomic(s.feedbackL,d.feedbackL);copyAtomic(s.feedbackR,d.feedbackR);copyAtomic(s.crossFeedback,d.crossFeedback);copyAtomic(s.mix,d.mix);copyAtomic(s.lowCutHz,d.lowCutHz);copyAtomic(s.highCutHz,d.highCutHz);copyAtomic(s.modRateHz,d.modRateHz);copyAtomic(s.modDepthMs,d.modDepthMs);}
}
