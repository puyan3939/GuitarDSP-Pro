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
    amp.prepare(sr,block,2);
    juce::AudioBuffer<float> b(2,block); fillSine(b);
    const float inRms=rms(b); amp.process(b,0,block);
    ok &= require(sane(b),"HQ amp finite/bounded");
    ok &= require(rms(b)>1.0e-6f && std::abs(rms(b)-inRms)>1.0e-5f,"HQ amp changes signal");

    guitardsp::hq::HQEffectsRack rack; rack.prepare(sr,block);
    for(int model=0;model<9;++model){
        rack.reset(); auto& slot=rack.pedalSlot(0); slot.enabled.store(true); slot.model.store(model); slot.drive.store(2.0f); slot.mix.store(1.0f);
        fillSine(b); rack.processPreAmp(b,0,block);
        const std::string name="Pedal model "+std::to_string(model)+" finite";
        ok &= require(sane(b),name.c_str());
        slot.enabled.store(false);
    }
    rack.setDynamicsMode(guitardsp::hq::HQEffectsRack::DynamicsMode::studioCompressor);
    fillSine(b,0.3f); rack.processPreAmp(b,0,block); ok &= require(sane(b),"Studio compressor finite");
    rack.setDynamicsMode(guitardsp::hq::HQEffectsRack::DynamicsMode::off);

    for(int mode=1;mode<=5;++mode){
        rack.reset(); rack.setModulationMode((guitardsp::hq::HQEffectsRack::ModulationMode)mode); fillSine(b); rack.processPostAmp(b,0,block);
        const std::string name="Modulation mode "+std::to_string(mode)+" finite";
        ok &= require(sane(b),name.c_str());
    }
    rack.setModulationMode(guitardsp::hq::HQEffectsRack::ModulationMode::off);
    rack.reset(); rack.setDelayEnabled(true); fillSine(b); rack.processPostAmp(b,0,block); ok &= require(sane(b),"Delay finite"); rack.setDelayEnabled(false);
    rack.reset(); rack.setReverbEnabled(true); fillSine(b); rack.processPostAmp(b,0,block); ok &= require(sane(b),"Reverb finite"); rack.setReverbEnabled(false);
    return ok ? 0 : 1;
}
