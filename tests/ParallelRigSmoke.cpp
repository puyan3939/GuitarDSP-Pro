#include <JuceHeader.h>
#include <cmath>
#include <iostream>
#include "../hq_preload/dsp/routing/ParallelRigHQ.h"
#include "../hq_preload/dsp/performance/PerformanceToolsHQ.h"

namespace
{
float rms(const juce::AudioBuffer<float>& b){double s=0;int n=0;for(int ch=0;ch<b.getNumChannels();++ch){const auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i){s+=(double)d[i]*d[i];++n;}}return n?(float)std::sqrt(s/n):0;}
bool sane(const juce::AudioBuffer<float>& b,float limit=8){for(int ch=0;ch<b.getNumChannels();++ch){const auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i)if(!std::isfinite(d[i])||std::abs(d[i])>limit)return false;}return true;}
void sine(juce::AudioBuffer<float>&b,float a,float hz,double sr){for(int ch=0;ch<b.getNumChannels();++ch){auto*d=b.getWritePointer(ch);for(int i=0;i<b.getNumSamples();++i)d[i]=a*std::sin(juce::MathConstants<double>::twoPi*hz*i/sr);}}
bool req(bool v,const char*t){std::cout<<(v?"PASS ":"FAIL ")<<t<<'\n';return v;}
}

int main()
{
    constexpr double sr=48000;constexpr int block=256;bool ok=true;
    guitardsp::hq::ParallelRigHQ rig;rig.prepare(sr,block);guitardsp::hq::ParallelRigControl c;juce::AudioBuffer<float>dry(1,block),main(2,block);
    sine(dry,.08f,220,sr);sine(main,.05f,660,sr);const float before=rms(main);rig.process(dry,main,0,block,c);ok&=req(std::abs(rms(main)-before)<1e-8f,"Parallel rig disabled transparent");
    c.enabled.store(true);c.cleanEnabled.store(true);c.subEnabled.store(true);c.cleanLevelDb.store(-10);c.subLevelDb.store(-7);c.autoLatencyComp.store(true);
    for(int k=0;k<12;++k){sine(dry,.08f,220,sr);sine(main,.05f,660,sr);rig.process(dry,main,0,block,c);}ok&=req(sane(main)&&rms(main)>1e-6f,"Multi-amp mix finite and active");
    juce::AudioBuffer<float>stems(2,block);stems.clear();rig.copyMainCleanStemTo(stems,0,0,block);rig.copySubStemTo(stems,1,0,block);ok&=req(rms(stems)>1e-7f,"Independent MAIN+CLEAN and SUB stems available");
    rig.reset();dry.clear();main.clear();for(int k=0;k<12;++k)rig.process(dry,main,0,block,c);ok&=req(sane(main,.01f)&&rms(main)<1e-7f,"Parallel rig preserves digital silence");

    guitardsp::hq::ExpressionPitchHQ pitch;guitardsp::hq::ExpressionPitchControl pc;pitch.prepare(sr,block);pc.enabled.store(true);pc.semitones.store(12);pc.expression.store(1);juce::AudioBuffer<float>mono(1,block);sine(mono,.05f,440,sr);for(int k=0;k<12;++k)pitch.process(mono,pc);ok&=req(sane(mono)&&rms(mono)>1e-7f,"Expression pitch finite and active");ok&=req(pitch.estimatedLatencySamples(pc)>0,"Expression pitch reports latency");
    pitch.reset();mono.clear();for(int k=0;k<8;++k)pitch.process(mono,pc);ok&=req(rms(mono)<1e-7f,"Expression pitch preserves silence");

    guitardsp::hq::InputLoadingHQ load;guitardsp::hq::InputLoadingControl lc;load.prepare(sr);lc.enabled.store(true);sine(mono,.05f,2500,sr);const float loadBefore=rms(mono);auto*d=mono.getWritePointer(0);for(int i=0;i<block;++i)d[i]=load.process(d[i],lc);ok&=req(sane(mono)&&std::abs(rms(mono)-loadBefore)>1e-6f,"Pickup/cable/input load changes response");load.reset();mono.clear();for(int i=0;i<block;++i)d[i]=load.process(d[i],lc);ok&=req(rms(mono)<1e-9f,"Input loading preserves silence");

    guitardsp::hq::DualDelayStereoHQ dual;guitardsp::hq::DualDelayControl dc;dual.prepare(sr,block);dc.enabled.store(true);dc.timeLms.store(1);dc.timeRms.store(3);dc.mix.store(1);juce::AudioBuffer<float>stereo(2,block);stereo.clear();stereo.setSample(0,0,.5f);stereo.setSample(1,0,.5f);dual.process(stereo,0,block,dc);ok&=req(sane(stereo),"Dual delay finite");float diff=0;for(int i=0;i<block;++i)diff+=std::abs(stereo.getSample(0,i)-stereo.getSample(1,i));ok&=req(diff>1e-5f,"Dual delay keeps independent L/R times");

    guitardsp::hq::SceneSwitcherHQ scenes;auto&s=scenes.scene(3);s.parallelEnabled=true;s.pitchSemitones=12;s.pedalRouteMask[0]=5;scenes.request(3);int idx=-1;ok&=req(scenes.consumeRequest(idx)&&idx==3&&scenes.scene(idx).pedalRouteMask[0]==5,"Scene switch request deterministic");ok&=req(!scenes.consumeRequest(idx),"Scene request consumed once");
    return ok?0:1;
}
