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
    limiter.setOutputGainDb(-18.0f);
}

void SignalChain::reset()
{
    ampEngine.reset(); hqAmpEngine.reset(); hqEffects.reset(); cabMic.reset(); parallelRig.reset();
    for(auto& p:expressionPitch) p.reset(); for(auto& l:inputLoading) l.reset(); dualDelay.reset(); limiter.reset();
    ampWorkBuffer.clear(); cleanRouteBuffer.clear(); subRouteBuffer.clear(); expressionWorkBuffer.clear();
}

void SignalChain::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    juce::ScopedNoDenormals noDenormals;
    const int channels=juce::jmin(2,buffer.getNumChannels()); if(channels<=0||numSamples<=0)return;

    applySceneRequests();

    if(monoInputToStereo.load(std::memory_order_relaxed)&&channels>=2)
        copyDetectedMonoToStereo(buffer,startSample,numSamples);

    if(!bypass.load(std::memory_order_relaxed))
    {
        // Source loading belongs before gain/boost/fuzz so pickup/cable resonance can interact with the rig.
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
        if(getAmpMode()==AmpMode::hq) processHQAmp(buffer,startSample,numSamples); else processLegacyAmp(buffer,startSample,numSamples);
        cabMic.process(buffer,startSample,numSamples);

        if(useParallelRig)
            parallelRig.processBuses(cleanRouteBuffer,subRouteBuffer,buffer,startSample,numSamples,routing);

        hqEffects.processPostAmp(buffer,startSample,numSamples);

        // Dual delay is a stereo mix effect. Stem mode intentionally bypasses it so the
        // dedicated physical bass output does not inherit a guitar stereo delay.
        if(getOutputMode()==OutputMode::stereoMix)
            dualDelay.process(buffer,startSample,numSamples,dualDelayControl);
        else if(useParallelRig)
            applyStemOutput(buffer,startSample,numSamples);
    }

    applyStartupFadeAndLimiter(buffer,startSample,numSamples);
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
    // SUB level is already a mixer concept in stereo mode. In stem mode keep the
    // dedicated bass path at its configured bus level before the global safety limiter.
    parallelRig.copySubStemTo(buffer,1,startSample,numSamples,hqEffects.parallelRigControl().subLevelDb.load());
}

void SignalChain::applyStartupFadeAndLimiter(juce::AudioBuffer<float>& buffer,int startSample,int numSamples)
{
    const int channels=juce::jmin(2,buffer.getNumChannels());for(int i=0;i<numSamples;++i){const float fade=startupFade.getNextValue();for(int ch=0;ch<channels;++ch){float&sample=buffer.getWritePointer(ch,startSample)[i];sample=limiter.processSample(sample*fade);}}
}

void SignalChain::setInputGainDb(float gainDb) noexcept { inputGainDb.store(juce::jlimit(-36.0f,18.0f,gainDb),std::memory_order_relaxed); }
void SignalChain::setOutputGainDb(float gainDb) noexcept { limiter.setOutputGainDb(gainDb); }
void SignalChain::setBypass(bool shouldBypass) noexcept { bypass.store(shouldBypass,std::memory_order_relaxed); }
void SignalChain::setMonoInputToStereo(bool enabled) noexcept { monoInputToStereo.store(enabled,std::memory_order_relaxed); }
