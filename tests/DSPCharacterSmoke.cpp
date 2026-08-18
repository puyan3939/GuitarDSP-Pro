#include <JuceHeader.h>
#include <cmath>
#include <iostream>
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"
#include "../hq_preload/dsp/HQEffectsRack.h"

namespace {
void fillSine(juce::AudioBuffer<float>& b, float amplitude=0.08f, float hz=440.0f, double sr=48000.0) {
    for (int ch=0; ch<b.getNumChannels(); ++ch) {
        auto* d=b.getWritePointer(ch);
        for (int i=0; i<b.getNumSamples(); ++i)
            d[i]=amplitude*std::sin(juce::MathConstants<double>::twoPi*hz*(double)i/sr);
    }
}

bool sane(const juce::AudioBuffer<float>& b, float maxAbs=20.0f) {
    for (int ch=0; ch<b.getNumChannels(); ++ch) {
        const auto* d=b.getReadPointer(ch);
        for (int i=0; i<b.getNumSamples(); ++i)
            if (!std::isfinite(d[i]) || std::abs(d[i]) > maxAbs) return false;
    }
    return true;
}

float channelRms(const juce::AudioBuffer<float>& b, int ch) {
    if (ch < 0 || ch >= b.getNumChannels()) return 0.0f;
    double s=0.0;
    const auto* d=b.getReadPointer(ch);
    for(int i=0;i<b.getNumSamples();++i) s+=(double)d[i]*d[i];
    return b.getNumSamples()>0?(float)std::sqrt(s/(double)b.getNumSamples()):0.0f;
}

float rms(const juce::AudioBuffer<float>& b) {
    double s=0.0; int n=0;
    for(int ch=0;ch<b.getNumChannels();++ch){auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i){s+=(double)d[i]*d[i];++n;}}
    return n>0?(float)std::sqrt(s/(double)n):0.0f;
}

bool require(bool cond,const char* name){std::cout<<(cond?"PASS ":"FAIL ")<<name<<'\n';return cond;}
}

int main() {
    constexpr double sr=48000.0; constexpr int block=256;
    bool ok=true;

    guitardsp::hq::AmpEngineHQ amp;
    amp.prepare(sr,block);
    juce::AudioBuffer<float> b(2,block);
    fillSine(b);
    const float inRms=rms(b);
    amp.process(b);
    ok &= require(sane(b),"HQ amp finite/bounded");
    ok &= require(rms(b)>1.0e-6f && std::abs(rms(b)-inRms)>1.0e-5f,"HQ amp changes signal");

    guitardsp::hq::HQEffectsRack rack; rack.prepare(sr,block);
    for(int model=0;model<9;++model){
        rack.reset();
        auto& slot=rack.pedalSlot(0);
        slot.enabled.store(true);
        slot.model.store(model);
        slot.drive.store(0.8f);
        slot.mix.store(1.0f);
        fillSine(b);
        rack.processPreAmp(b,0,block);
        const std::string name="Pedal model "+std::to_string(model)+" finite";
        ok &= require(sane(b),name.c_str());
        slot.enabled.store(false);
    }

    rack.setDynamicsMode(guitardsp::hq::HQEffectsRack::DynamicsMode::studioCompressor);
    fillSine(b,0.3f);
    rack.processPreAmp(b,0,block);
    ok &= require(sane(b),"Studio compressor finite");
    rack.setDynamicsMode(guitardsp::hq::HQEffectsRack::DynamicsMode::off);

    rack.modulationControl().rateHz.store(4.0f);
    rack.modulationControl().depth.store(0.7f);
    rack.modulationControl().mix.store(1.0f);
    for(int mode=1;mode<=5;++mode){
        rack.reset();
        rack.setModulationMode((guitardsp::hq::HQEffectsRack::ModulationMode)mode);
        fillSine(b);
        const float before=rms(b);
        rack.processPostAmp(b,0,block);
        const std::string finiteName="Modulation mode "+std::to_string(mode)+" finite";
        const std::string activeName="Modulation mode "+std::to_string(mode)+" changes signal";
        ok &= require(sane(b),finiteName.c_str());
        ok &= require(std::abs(rms(b)-before)>1.0e-7f,activeName.c_str());
    }
    rack.setModulationMode(guitardsp::hq::HQEffectsRack::ModulationMode::off);

    rack.reset(); rack.setDelayEnabled(true); fillSine(b); rack.processPostAmp(b,0,block);
    ok &= require(sane(b),"Delay finite"); rack.setDelayEnabled(false);
    rack.reset(); rack.setReverbEnabled(true); fillSine(b); rack.processPostAmp(b,0,block);
    ok &= require(sane(b),"Reverb finite"); rack.setReverbEnabled(false);

    guitardsp::hq::HQEffectsRack isoRack; isoRack.prepare(sr,block);
    isoRack.delayControl().timeMs.store(1.0f);
    isoRack.delayControl().mix.store(1.0f);
    isoRack.delayControl().feedback.store(0.0f);
    isoRack.setDelayEnabled(true);
    b.clear(); b.setSample(0,0,0.5f);
    isoRack.processPostAmp(b,0,block);
    ok &= require(channelRms(b,1) < 1.0e-8f,"Delay L/R state isolation");

    guitardsp::hq::HQEffectsRack reverbIso; reverbIso.prepare(sr,block);
    reverbIso.reverbControl().preDelayMs.store(0.0f);
    reverbIso.reverbControl().mix.store(1.0f);
    reverbIso.setReverbEnabled(true);
    b.clear(); b.setSample(0,0,0.5f);
    reverbIso.processPostAmp(b,0,block);
    ok &= require(channelRms(b,1) < 1.0e-8f,"Reverb L/R state isolation");

    guitardsp::hq::HQEffectsRack silentRack; silentRack.prepare(sr,block);
    b.clear(); silentRack.processPreAmp(b,0,block); silentRack.processPostAmp(b,0,block);
    ok &= require(sane(b,1.0f) && rms(b) < 1.0e-9f,"Bypass rack silence stability");

    return ok ? 0 : 1;
}
