#include <JuceHeader.h>
#include <cmath>
#include <iostream>
#include "../hq_preload/dsp/HQEffectsRack.h"

namespace{
void fill(juce::AudioBuffer<float>&b,float a,float hz,double sr){for(int ch=0;ch<b.getNumChannels();++ch){auto*d=b.getWritePointer(ch);for(int i=0;i<b.getNumSamples();++i)d[i]=a*std::sin(juce::MathConstants<double>::twoPi*hz*i/sr);}}
float diff(const juce::AudioBuffer<float>&a,const juce::AudioBuffer<float>&b){float m=0;for(int ch=0;ch<a.getNumChannels();++ch)for(int i=0;i<a.getNumSamples();++i)m=juce::jmax(m,std::abs(a.getSample(ch,i)-b.getSample(ch,i)));return m;}
bool sane(const juce::AudioBuffer<float>&b){for(int ch=0;ch<b.getNumChannels();++ch)for(int i=0;i<b.getNumSamples();++i)if(!std::isfinite(b.getSample(ch,i)))return false;return true;}
bool req(bool v,const char*t){std::cout<<(v?"PASS ":"FAIL ")<<t<<'\n';return v;}
}
int main(){constexpr double sr=48000;constexpr int n=256;bool ok=true;guitardsp::hq::HQEffectsRack rack;rack.prepare(sr,n);auto&s=rack.pedalSlot(0);s.enabled.store(true);s.model.store((int)guitardsp::hq::PedalType::siliconFuzz);s.drive.store(.75f);s.routeMask.store(guitardsp::hq::HQEffectsRack::routeClean);
juce::AudioBuffer<float>source(2,n),main(2,n),clean(2,n),sub(2,n);fill(source,.05f,440,sr);main.makeCopyOf(source);clean.makeCopyOf(source);sub.makeCopyOf(source);
rack.processPedalRoute(main,0,n,guitardsp::hq::HQEffectsRack::routeMain);rack.processPedalRoute(clean,0,n,guitardsp::hq::HQEffectsRack::routeClean);rack.processPedalRoute(sub,0,n,guitardsp::hq::HQEffectsRack::routeSub);
ok&=req(diff(source,main)<1e-8f,"CLEAN-only pedal leaves MAIN untouched");ok&=req(diff(source,sub)<1e-8f,"CLEAN-only pedal leaves SUB untouched");ok&=req(diff(source,clean)>1e-5f&&sane(clean),"CLEAN-only pedal processes CLEAN route");
s.routeMask.store(guitardsp::hq::HQEffectsRack::routeMain|guitardsp::hq::HQEffectsRack::routeSub);main.makeCopyOf(source);sub.makeCopyOf(source);rack.processPedalRoute(main,0,n,guitardsp::hq::HQEffectsRack::routeMain);rack.processPedalRoute(sub,0,n,guitardsp::hq::HQEffectsRack::routeSub);ok&=req(diff(source,main)>1e-5f&&diff(source,sub)>1e-5f,"One slot can feed multiple independent routes");return ok?0:1;}
