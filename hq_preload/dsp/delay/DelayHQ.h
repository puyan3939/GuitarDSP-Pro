#pragma once
#include <JuceHeader.h>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
enum class DelayType { digital, analog, tape };
class DelayHQ
{
public:
    struct Params { DelayType type=DelayType::digital; float timeMs=380, feedback=0.35f, mix=0.25f, lowCutHz=80, highCutHz=7000, drive=0.25f, age=0.25f, wow=0.15f, flutter=0.08f; };
    DelayHQ(): delay(192000) {}
    void prepare(double fs,int maxBlock)
    {
        sampleRate=fs; juce::dsp::ProcessSpec s{fs,(juce::uint32)maxBlock,1}; delay.prepare(s); delay.reset();
        hp.prepare(fs); lp.prepare(fs); hp.setHz(params.lowCutHz); lp.setHz(params.highCutHz);
    }
    void setParameters(const Params& p) { params=p; hp.setHz(p.lowCutHz); lp.setHz(p.highCutHz); }
    float process(float x)
    {
        float mod=0.0f;
        if(params.type==DelayType::tape)
        {
            mod=std::sin(phase)*params.wow*2.5f+std::sin(phase*7.13f)*params.flutter*0.8f;
            phase+=2.0f*juce::MathConstants<float>::pi*0.55f/(float)sampleRate; if(phase>6.283185f) phase-=6.283185f;
        }
        delay.setDelay(juce::jlimit(1.0f,(float)(sampleRate*3.9),0.001f*(params.timeMs+mod)*(float)sampleRate));
        float wet=delay.popSample(0);
        float fb=hp.process(wet); fb=lp.process(fb);
        if(params.type==DelayType::analog) fb=softSat(fb*(1.0f+2.0f*params.drive))/(1.0f+0.7f*params.drive);
        if(params.type==DelayType::tape) { fb=softSat(fb*(1.0f+2.8f*params.drive)); fb*=1.0f-0.22f*params.age; }
        delay.pushSample(0,x+fb*juce::jlimit(-0.97f,0.97f,params.feedback));
        return lerp(x,wet,params.mix);
    }
private:
    Params params; double sampleRate=48000.0; float phase=0; OnePoleHP hp; OnePoleLP lp;
    juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay;
};
}
