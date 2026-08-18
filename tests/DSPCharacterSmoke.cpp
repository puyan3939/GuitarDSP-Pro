#include <JuceHeader.h>
#include <cmath>
#include <iostream>
#include <array>
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"
#include "../hq_preload/dsp/HQEffectsRack.h"
#include "../hq_preload/dsp/cab/CabMicEngineHQ.h"

namespace {
void fillSine(juce::AudioBuffer<float>& b,float amplitude=0.08f,float hz=440.0f,double sr=48000.0){for(int ch=0;ch<b.getNumChannels();++ch){auto*d=b.getWritePointer(ch);for(int i=0;i<b.getNumSamples();++i)d[i]=amplitude*std::sin(juce::MathConstants<double>::twoPi*hz*(double)i/sr);}}
bool sane(const juce::AudioBuffer<float>& b,float maxAbs=20.0f){for(int ch=0;ch<b.getNumChannels();++ch){const auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i)if(!std::isfinite(d[i])||std::abs(d[i])>maxAbs)return false;}return true;}
float channelRms(const juce::AudioBuffer<float>& b,int ch){if(ch<0||ch>=b.getNumChannels())return 0;double s=0;const auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i)s+=(double)d[i]*d[i];return b.getNumSamples()>0?(float)std::sqrt(s/(double)b.getNumSamples()):0;}
float rms(const juce::AudioBuffer<float>& b){double s=0;int n=0;for(int ch=0;ch<b.getNumChannels();++ch){auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i){s+=(double)d[i]*d[i];++n;}}return n>0?(float)std::sqrt(s/(double)n):0;}
float acRms(const juce::AudioBuffer<float>& b){double s=0;int n=0;for(int ch=0;ch<b.getNumChannels();++ch){const auto*d=b.getReadPointer(ch);double mean=0;for(int i=0;i<b.getNumSamples();++i)mean+=d[i];mean/=juce::jmax(1,b.getNumSamples());for(int i=0;i<b.getNumSamples();++i){const double v=(double)d[i]-mean;s+=v*v;++n;}}return n>0?(float)std::sqrt(s/(double)n):0;}
bool require(bool cond,const char* name){std::cout<<(cond?"PASS ":"FAIL ")<<name<<'\n';return cond;}
}

int main(){
    constexpr double sr=48000.0;constexpr int block=256;bool ok=true;
    guitardsp::hq::AmpEngineHQ amp;amp.prepare(sr,block);juce::AudioBuffer<float>b(2,block);fillSine(b);const float inRms=rms(b);amp.process(b);
    ok&=require(sane(b),"HQ amp finite/bounded");ok&=require(rms(b)>1.0e-6f&&std::abs(rms(b)-inRms)>1.0e-5f,"HQ amp changes signal");

    guitardsp::hq::HQEffectsRack rack;rack.prepare(sr,block);
    for(int model=0;model<9;++model){rack.reset();auto&slot=rack.pedalSlot(0);slot.enabled.store(true);slot.model.store(model);slot.drive.store(0.8f);slot.mix.store(1.0f);fillSine(b);rack.processPreAmp(b,0,block);const std::string name="Pedal model "+std::to_string(model)+" finite";ok&=require(sane(b),name.c_str());slot.enabled.store(false);}

    static constexpr std::array<const char*,9> pedalNames{"Clean Boost","Treble Boost","Mid OD","Transparent OD","Hard Distortion","Germanium Fuzz","Silicon Fuzz","Octave Fuzz","Velcro Fuzz"};
    for(int model=0;model<9;++model){
        guitardsp::hq::PedalEngineHQ pedal; pedal.prepare(sr,block);
        guitardsp::hq::PedalParams pp; pp.drive=0.8f; pp.tone=0.55f; pp.levelDb=0.0f; pp.lowCutHz=55.0f; pp.focusHz=900.0f; pp.midDb=0.0f; pp.cleanMix=0.0f; pp.octave=0.65f; pp.starve=0.55f; pp.gate=0.15f;
        pedal.setType((guitardsp::hq::PedalType)model); pedal.setParameters(pp);
        juce::AudioBuffer<float> silent(1,block); silent.clear(); pedal.process(silent);
        const std::string silenceName=std::string(pedalNames[(size_t)model])+" zero-input silence"; ok&=require(sane(silent,0.01f)&&rms(silent)<1.0e-7f,silenceName.c_str());
        pedal.reset(); pedal.setParameters(pp); juce::AudioBuffer<float> tiny(1,block); fillSine(tiny,1.0e-5f,997.0f,sr);
        const float tinyIn=acRms(tiny); pedal.process(tiny); const float gain=acRms(tiny)/(tinyIn+1.0e-12f); const float gainDb=20.0f*std::log10(juce::jmax(gain,1.0e-12f));
        std::cout<<"INFO "<<pedalNames[(size_t)model]<<" low-level AC gain "<<gainDb<<" dB\n";
        const std::string gainName=std::string(pedalNames[(size_t)model])+" low-level AC gain bounded"; ok&=require(sane(tiny,0.1f)&&gain<32.0f,gainName.c_str());
    }

    // Precision gate: loud notes must pass, quiet input must be attenuated, silence stays silent.
    guitardsp::hq::HQEffectsRack gateRack; gateRack.prepare(sr,block); gateRack.setDynamicsMode(guitardsp::hq::HQEffectsRack::DynamicsMode::gate);
    auto& gc=gateRack.gateControl(); gc.thresholdDb.store(-48.0f); gc.rangeDb.store(-60.0f); gc.ratio.store(5.0f); gc.attackMs.store(0.8f); gc.holdMs.store(20.0f); gc.releaseMs.store(90.0f); gc.hysteresisDb.store(4.0f); gc.sidechainHpHz.store(55.0f); gc.sidechainLpHz.store(6500.0f);
    juce::AudioBuffer<float> gateBuf(2,block); fillSine(gateBuf,0.12f,440.0f,sr); const float loudBefore=rms(gateBuf); for(int i=0;i<8;++i)gateRack.processPreAmp(gateBuf,0,block); const float loudAfter=rms(gateBuf); ok&=require(loudAfter>0.65f*loudBefore,"Precision gate passes playing level");
    gateRack.reset(); fillSine(gateBuf,0.00008f,440.0f,sr); const float quietBefore=rms(gateBuf); for(int i=0;i<40;++i)gateRack.processPreAmp(gateBuf,0,block); const float quietAfter=rms(gateBuf); ok&=require(quietAfter<0.35f*quietBefore,"Precision gate attenuates noise floor");
    gateRack.reset(); gateBuf.clear(); for(int i=0;i<8;++i)gateRack.processPreAmp(gateBuf,0,block); ok&=require(rms(gateBuf)<1.0e-10f,"Precision gate preserves digital silence");

    rack.setDynamicsMode(guitardsp::hq::HQEffectsRack::DynamicsMode::studioCompressor);fillSine(b,0.3f);rack.processPreAmp(b,0,block);ok&=require(sane(b),"Studio compressor finite");rack.setDynamicsMode(guitardsp::hq::HQEffectsRack::DynamicsMode::off);
    rack.modulationControl().rateHz.store(4.0f);rack.modulationControl().depth.store(0.7f);rack.modulationControl().mix.store(1.0f);
    for(int mode=1;mode<=5;++mode){rack.reset();rack.setModulationMode((guitardsp::hq::HQEffectsRack::ModulationMode)mode);fillSine(b);const float before=rms(b);rack.processPostAmp(b,0,block);const std::string finiteName="Modulation mode "+std::to_string(mode)+" finite";const std::string activeName="Modulation mode "+std::to_string(mode)+" changes signal";ok&=require(sane(b),finiteName.c_str());ok&=require(std::abs(rms(b)-before)>1.0e-7f,activeName.c_str());}
    rack.setModulationMode(guitardsp::hq::HQEffectsRack::ModulationMode::off);
    rack.reset();rack.setDelayEnabled(true);fillSine(b);rack.processPostAmp(b,0,block);ok&=require(sane(b),"Delay finite");rack.setDelayEnabled(false);
    rack.reset();rack.setReverbEnabled(true);fillSine(b);rack.processPostAmp(b,0,block);ok&=require(sane(b),"Reverb finite");rack.setReverbEnabled(false);

    guitardsp::hq::CabMicEngineHQ cab;cab.prepare(sr,block);guitardsp::hq::CabMicParams cp;cp.cab=guitardsp::hq::CabType::vintage4x12;cp.mic=guitardsp::hq::MicType::dynamic57;cab.setParameters(cp);cab.setEnabled(true);
    fillSine(b);const float cabBefore=rms(b);for(int i=0;i<16;++i)cab.process(b,0,block);ok&=require(sane(b),"Cab/mic convolution finite");ok&=require(std::abs(rms(b)-cabBefore)>1.0e-5f,"Cab/mic changes signal");

    guitardsp::hq::HQEffectsRack isoRack;isoRack.prepare(sr,block);isoRack.delayControl().timeMs.store(1.0f);isoRack.delayControl().mix.store(1.0f);isoRack.delayControl().feedback.store(0.0f);isoRack.setDelayEnabled(true);b.clear();b.setSample(0,0,0.5f);isoRack.processPostAmp(b,0,block);ok&=require(channelRms(b,1)<1.0e-8f,"Delay L/R state isolation");
    guitardsp::hq::HQEffectsRack reverbIso;reverbIso.prepare(sr,block);reverbIso.reverbControl().preDelayMs.store(0.0f);reverbIso.reverbControl().mix.store(1.0f);reverbIso.setReverbEnabled(true);b.clear();b.setSample(0,0,0.5f);reverbIso.processPostAmp(b,0,block);ok&=require(channelRms(b,1)<1.0e-8f,"Reverb L/R state isolation");
    guitardsp::hq::HQEffectsRack silentRack;silentRack.prepare(sr,block);b.clear();silentRack.processPreAmp(b,0,block);silentRack.processPostAmp(b,0,block);ok&=require(sane(b,1.0f)&&rms(b)<1.0e-9f,"Bypass rack silence stability");
    return ok?0:1;
}
