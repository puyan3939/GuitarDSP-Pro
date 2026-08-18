#include "HQEffectsRack.h"

namespace guitardsp::hq {

void HQEffectsRack::prepare(double sampleRate, int maximumBlockSize) {
    for (auto& pedal : pedals) pedal.prepare(sampleRate, maximumBlockSize);
    noiseGate.prepare(sampleRate); studioComp.prepare(sampleRate); guitarComp.prepare(sampleRate);
    chorus.prepare(sampleRate, maximumBlockSize); flanger.prepare(sampleRate, maximumBlockSize);
    phaser.prepare(sampleRate); tremolo.prepare(sampleRate); vibrato.prepare(sampleRate, maximumBlockSize);
    delayFx.prepare(sampleRate, maximumBlockSize); reverbFx.prepare(sampleRate, maximumBlockSize);
    reset();
}

void HQEffectsRack::reset() {
    for (auto& pedal : pedals) pedal.reset();
    noiseGate.reset(); studioComp.reset(); guitarComp.reset();
    chorus.reset(); flanger.reset(); phaser.reset(); tremolo.reset(); vibrato.reset();
    delayFx.reset(); reverbFx.reset();
}

void HQEffectsRack::updateDynamicParameters() {
    GateParams g;
    g.thresholdDb=gateControls.thresholdDb.load(); g.rangeDb=gateControls.rangeDb.load(); g.attackMs=gateControls.attackMs.load();
    g.holdMs=gateControls.holdMs.load(); g.releaseMs=gateControls.releaseMs.load(); g.hysteresisDb=gateControls.hysteresisDb.load();
    noiseGate.set(g);
    CompressorParams c;
    c.thresholdDb=studioControls.thresholdDb.load(); c.ratio=studioControls.ratio.load(); c.attackMs=studioControls.attackMs.load();
    c.releaseMs=studioControls.releaseMs.load(); c.kneeDb=studioControls.kneeDb.load(); c.makeupDb=studioControls.makeupDb.load();
    c.mix=studioControls.mix.load(); c.rms=studioControls.rms.load(); studioComp.set(c);
    GuitarCompParams gc;
    gc.sustain=guitarControls.sustain.load(); gc.attack=guitarControls.attack.load(); gc.blend=guitarControls.blend.load(); gc.levelDb=guitarControls.levelDb.load();
    guitarComp.set(gc);
}

void HQEffectsRack::processPreAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    updateDynamicParameters();
    applyDynamics(buffer, startSample, numSamples);
    for (int slot = 0; slot < pedalSlots; ++slot) {
        auto& control = pedalControls[(size_t)slot];
        if (!control.enabled.load(std::memory_order_relaxed)) continue;
        PedalParams p;
        p.drive=control.drive.load(); p.tone=control.tone.load(); p.levelDb=control.levelDb.load(); p.mix=control.mix.load();
        p.aux1=control.aux1.load(); p.aux2=control.aux2.load(); p.aux3=control.aux3.load();
        pedals[(size_t)slot].setModel((PedalModel)juce::jlimit(0,8,control.model.load()));
        pedals[(size_t)slot].setParams(p);
        pedals[(size_t)slot].process(buffer,startSample,numSamples);
    }
}

void HQEffectsRack::applyDynamics(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    const auto mode=getDynamicsMode(); if(mode==DynamicsMode::off)return;
    const int channels=juce::jmin(2,buffer.getNumChannels());
    for(int ch=0;ch<channels;++ch){auto* d=buffer.getWritePointer(ch,startSample);for(int i=0;i<numSamples;++i){
        switch(mode){case DynamicsMode::gate:d[i]=noiseGate.processSample(ch,d[i]);break;case DynamicsMode::studioCompressor:d[i]=studioComp.processSample(ch,d[i]);break;case DynamicsMode::guitarCompressor:d[i]=guitarComp.processSample(ch,d[i]);break;default:break;}
    }}
}

void HQEffectsRack::updatePostParameters() {
    ModParams m;
    m.rateHz=modControls.rateHz.load();m.depth=modControls.depth.load();m.mix=modControls.mix.load();m.feedback=modControls.feedback.load();m.manual=modControls.manual.load();m.shape=modControls.shape.load();
    chorus.set(m);flanger.set(m);phaser.set(m);tremolo.set(m);vibrato.set(m);
    DelayParams d;
    d.timeMs=delayControls.timeMs.load();d.feedback=delayControls.feedback.load();d.mix=delayControls.mix.load();d.lowCutHz=delayControls.lowCutHz.load();d.highCutHz=delayControls.highCutHz.load();
    d.drive=delayControls.drive.load();d.wow=delayControls.wow.load();d.flutter=delayControls.flutter.load();d.age=delayControls.age.load();
    delayFx.setFlavor((DelayFlavor)juce::jlimit(0,3,delayControls.flavor.load()));delayFx.set(d);
    ReverbParams r;
    r.size=reverbControls.size.load();r.decay=reverbControls.decay.load();r.damping=reverbControls.damping.load();r.preDelayMs=reverbControls.preDelayMs.load();
    r.mix=reverbControls.mix.load();r.mod=reverbControls.mod.load();r.drip=reverbControls.drip.load();
    reverbFx.setFlavor((ReverbFlavor)juce::jlimit(0,3,reverbControls.flavor.load()));reverbFx.set(r);
}

void HQEffectsRack::processPostAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    updatePostParameters(); applyModulation(buffer,startSample,numSamples);
    const int channels=juce::jmin(2,buffer.getNumChannels()); const bool useDelay=isDelayEnabled(),useReverb=isReverbEnabled();
    if(!useDelay&&!useReverb)return;
    std::array<float*,2> data{};for(int ch=0;ch<channels;++ch)data[(size_t)ch]=buffer.getWritePointer(ch,startSample);
    for(int i=0;i<numSamples;++i)for(int ch=0;ch<channels;++ch){float& x=data[(size_t)ch][i];if(useDelay)x=delayFx.processSample(ch,x);if(useReverb)x=reverbFx.processSample(ch,x);}
}

void HQEffectsRack::applyModulation(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    const auto mode=getModulationMode();if(mode==ModulationMode::off)return;
    const int channels=juce::jmin(2,buffer.getNumChannels());std::array<float*,2>data{};for(int ch=0;ch<channels;++ch)data[(size_t)ch]=buffer.getWritePointer(ch,startSample);
    for(int i=0;i<numSamples;++i)for(int ch=0;ch<channels;++ch){float& x=data[(size_t)ch][i];switch(mode){case ModulationMode::chorus:x=chorus.processSample(ch,x);break;case ModulationMode::flanger:x=flanger.processSample(ch,x);break;case ModulationMode::phaser:x=phaser.processSample(ch,x);break;case ModulationMode::tremolo:x=tremolo.processSample(ch,x);break;case ModulationMode::vibrato:x=vibrato.processSample(ch,x);break;default:break;}}
}

}
