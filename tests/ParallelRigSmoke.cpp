#include <JuceHeader.h>
#include <cmath>
#include <iostream>
#include "../hq_preload/dsp/routing/ParallelRigHQ.h"

namespace
{
float rms(const juce::AudioBuffer<float>& b)
{
    double sum=0.0; int n=0;
    for(int ch=0;ch<b.getNumChannels();++ch){const auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i){sum+=(double)d[i]*d[i];++n;}}
    return n>0?(float)std::sqrt(sum/(double)n):0.0f;
}
bool sane(const juce::AudioBuffer<float>& b,float limit=8.0f)
{
    for(int ch=0;ch<b.getNumChannels();++ch){const auto*d=b.getReadPointer(ch);for(int i=0;i<b.getNumSamples();++i)if(!std::isfinite(d[i])||std::abs(d[i])>limit)return false;}
    return true;
}
void fillSine(juce::AudioBuffer<float>& b,float amp,float hz,double sr)
{
    for(int ch=0;ch<b.getNumChannels();++ch){auto*d=b.getWritePointer(ch);for(int i=0;i<b.getNumSamples();++i)d[i]=amp*std::sin(juce::MathConstants<double>::twoPi*hz*(double)i/sr);}
}
bool req(bool ok,const char* text){std::cout<<(ok?"PASS ":"FAIL ")<<text<<'\n';return ok;}
}

int main()
{
    constexpr double sr=48000.0; constexpr int block=256;
    bool ok=true;
    guitardsp::hq::ParallelRigHQ rig; rig.prepare(sr,block);
    guitardsp::hq::ParallelRigControl c;
    juce::AudioBuffer<float> dry(1,block), main(2,block);

    fillSine(dry,0.08f,220.0f,sr); fillSine(main,0.05f,660.0f,sr);
    const float disabledBefore=rms(main);
    rig.process(dry,main,0,block,c);
    ok&=req(std::abs(rms(main)-disabledBefore)<1.0e-8f,"Parallel rig disabled is transparent");

    c.enabled.store(true); c.cleanEnabled.store(true); c.subEnabled.store(true);
    c.cleanLevelDb.store(-10.0f); c.subLevelDb.store(-7.0f);
    fillSine(dry,0.08f,220.0f,sr); fillSine(main,0.05f,660.0f,sr);
    const float enabledBefore=rms(main);
    for(int i=0;i<10;++i) rig.process(dry,main,0,block,c);
    ok&=req(sane(main),"Parallel rig output finite/bounded");
    ok&=req(std::abs(rms(main)-enabledBefore)>1.0e-4f,"Parallel rig changes main mix");
    ok&=req(std::abs(rms(main)-std::sqrt(2.0f)*0.0f)>1.0e-7f,"Parallel rig produces audio");

    rig.reset(); dry.clear(); main.clear();
    for(int i=0;i<12;++i) rig.process(dry,main,0,block,c);
    ok&=req(sane(main,0.01f)&&rms(main)<1.0e-7f,"Parallel rig preserves digital silence");

    rig.reset(); c.cleanEnabled.store(false); c.subEnabled.store(false); c.mainLevelDb.store(0.0f); c.mainDelayMs.store(0.0f);
    fillSine(dry,0.08f,220.0f,sr); fillSine(main,0.05f,660.0f,sr);
    juce::AudioBuffer<float> reference(2,block); reference.makeCopyOf(main);
    rig.process(dry,main,0,block,c);
    float maxDiff=0.0f; for(int ch=0;ch<2;++ch){const auto*a=main.getReadPointer(ch);const auto*b=reference.getReadPointer(ch);for(int i=0;i<block;++i)maxDiff=juce::jmax(maxDiff,std::abs(a[i]-b[i]));}
    ok&=req(maxDiff<1.0e-7f,"MAIN-only parallel rig is unity at zero delay");

    return ok?0:1;
}
