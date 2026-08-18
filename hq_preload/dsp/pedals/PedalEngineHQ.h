#pragma once
#include <JuceHeader.h>
#include <memory>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
enum class PedalType { cleanBoost, trebleBoost, midOD, transparentOD, hardDistortion, germaniumFuzz, siliconFuzz, octaveFuzz, velcroFuzz };

struct PedalParams
{
    float drive=0.5f, tone=0.5f, levelDb=0.0f;
    float lowCutHz=80.0f, focusHz=900.0f, midDb=0.0f, presenceDb=0.0f;
    float cleanMix=0.0f, bias=0.0f, scoop=0.0f, octave=0.6f, starve=0.0f, gate=0.0f;
};

class PedalEngineHQ
{
public:
    PedalEngineHQ(): oversampling(2) {}
    void prepare(double fs,int maxBlock)
    {
        sampleRate=fs; maxSamples=maxBlock; oversampling.prepare(fs,maxBlock); work.setSize(1,maxBlock);
        preHP.prepare(fs*4.0); postLP.prepare(fs*4.0); midPeak.reset();
    }
    void reset(){oversampling.reset();preHP.reset();postLP.reset();midPeak.reset();env=0.0f;}
    void setType(PedalType t){type=t;updateFilters();}
    void setParameters(const PedalParams&p){params=p;updateFilters();}

    void process(juce::AudioBuffer<float>& mono)
    {
        const int n=mono.getNumSamples(); if(n<=0)return; work.setSize(1,n,false,false,true); work.copyFrom(0,0,mono,0,0,n);
        oversampling.process(work,[this](float x){return nonlinear(x);}); mono.copyFrom(0,0,work,0,0,n);
        mono.applyGain(dbToGain(params.levelDb));
    }
private:
    void updateFilters()
    {
        preHP.setHz(params.lowCutHz); postLP.setHz(lerp(3500.0f,14000.0f,params.tone));
        midPeak.setPeak(sampleRate*4.0,params.focusHz,0.75f,params.midDb);
    }
    float nonlinear(float x)
    {
        x=preHP.process(x); const float d=params.drive;
        switch(type)
        {
            case PedalType::cleanBoost:
                return softSat(x*(1.0f+5.0f*d)*0.45f)/0.45f;
            case PedalType::trebleBoost:
                x=midPeak.process(x*(1.0f+8.0f*d)); return postLP.process(asymSat(x,0.03f,0.35f));
            case PedalType::midOD:
                x=midPeak.process(x*(1.0f+10.0f*d)); return postLP.process(asymSat(x,0.045f,0.5f));
            case PedalType::transparentOD:
            {
                const float dry=x; const float wet=postLP.process(asymSat(x*(1.0f+7.0f*d),0.01f,0.18f));
                return lerp(wet,dry,params.cleanMix);
            }
            case PedalType::hardDistortion:
                x*=1.0f+18.0f*d; x=juce::jlimit(-1.0f,1.0f,x); return postLP.process(x);
            case PedalType::germaniumFuzz:
                return postLP.process(asymSat(x*(2.0f+28.0f*d),params.bias,0.75f)*0.9f);
            case PedalType::siliconFuzz:
            {
                float y=std::tanh(x*(3.0f+42.0f*d)); y=midPeak.process(y); return postLP.process(y);
            }
            case PedalType::octaveFuzz:
            {
                const float f=asymSat(x*(2.0f+24.0f*d),0.05f,0.65f); const float oct=2.0f*std::abs(f)-0.55f; return postLP.process(lerp(f,oct,params.octave));
            }
            case PedalType::velcroFuzz:
            {
                const float a=0.996f; env=a*env+(1.0f-a)*std::abs(x); const float supply=juce::jlimit(0.12f,1.0f,1.0f-0.82f*params.starve*env);
                float y=asymSat(x*(4.0f+38.0f*d)/supply,0.08f+0.18f*params.starve,0.9f)*supply;
                const float th=0.003f+0.06f*params.gate; if(std::abs(x)<th)y*=std::abs(x)/th; return postLP.process(y);
            }
        }
        return x;
    }
    PedalType type=PedalType::midOD; PedalParams params; double sampleRate=48000; int maxSamples=512; float env=0;
    NonlinearOversampler oversampling; juce::AudioBuffer<float> work; OnePoleHP preHP; OnePoleLP postLP; Biquad midPeak;
};
}
