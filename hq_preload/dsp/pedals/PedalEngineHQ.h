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
    PedalEngineHQ(): oversampling(3) {} // 8x for fuzz/high-gain nonlinearities
    void prepare(double fs,int maxBlock)
    {
        sampleRate=fs; maxSamples=maxBlock; oversampling.prepare(fs,maxBlock); work.setSize(1,maxBlock);
        const double internalFs=oversampling.getInternalSampleRate();
        preHP.prepare(internalFs); postLP.prepare(internalFs); midPeak.reset();
        geMemory.prepare(internalFs,2.5f); starveEnv.prepare(internalFs,12.0f); octaveDc.prepare(internalFs,8.0f);
        updateFilters(); reset();
    }
    void reset(){oversampling.reset();preHP.reset();postLP.reset();midPeak.reset();env=0.0f;geMemory.reset();starveEnv.reset();octaveDc.reset();velcroGateOpen=false;}
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
        midPeak.setPeak(oversampling.getInternalSampleRate(),params.focusHz,0.75f,params.midDb);
    }
    float germanium(float x,float d)
    {
        // Two transistor-like stages with low headroom, leakage/bias memory and supply compression.
        const float memory=geMemory.process(x);
        const float bias=params.bias+0.035f+0.055f*memory;
        const float q1=asymSat(x*(2.0f+18.0f*d),bias,0.82f);
        const float demand=juce::jlimit(0.0f,1.0f,std::abs(q1));
        const float supply=0.62f-0.12f*demand;
        const float q2=asymSat(q1*(1.6f+7.5f*d)/supply,-0.025f+bias*0.45f,0.92f)*supply;
        return postLP.process(q2*0.95f);
    }
    float octaveFuzz(float x,float d)
    {
        const float pre=asymSat(x*(2.0f+21.0f*d),0.045f,0.72f);
        const float rect=std::abs(pre);
        const float dc=octaveDc.process(rect);
        const float octaveSignal=(rect-dc)*2.25f;
        const float combined=lerp(pre,octaveSignal,juce::jlimit(0.0f,1.0f,params.octave));
        return postLP.process(asymSat(combined*(1.2f+3.0f*d),0.018f,0.55f));
    }
    float velcro(float x,float d)
    {
        const float demand=starveEnv.process(std::abs(x)*(0.7f+1.8f*d));
        const float supply=juce::jlimit(0.10f,1.0f,1.0f-params.starve*(0.55f+0.72f*demand));
        const float openTh=0.0025f+0.050f*params.gate;
        const float closeTh=openTh*0.55f;
        if(!velcroGateOpen && std::abs(x)>openTh) velcroGateOpen=true;
        else if(velcroGateOpen && std::abs(x)<closeTh) velcroGateOpen=false;
        const float sputter=velcroGateOpen ? 1.0f : juce::jlimit(0.0f,1.0f,std::abs(x)/(openTh+1.0e-6f));
        float y=asymSat(x*(3.5f+42.0f*d)/supply,0.055f+0.20f*params.starve,0.96f)*supply;
        y*=sputter;
        return postLP.process(y);
    }
    float nonlinear(float x)
    {
        x=preHP.process(x); const float d=params.drive;
        switch(type)
        {
            case PedalType::cleanBoost: return softSat(x*(1.0f+5.0f*d)*0.45f)/0.45f;
            case PedalType::trebleBoost: x=midPeak.process(x*(1.0f+8.0f*d)); return postLP.process(asymSat(x,0.03f,0.35f));
            case PedalType::midOD: x=midPeak.process(x*(1.0f+10.0f*d)); return postLP.process(asymSat(x,0.045f,0.5f));
            case PedalType::transparentOD:{const float dry=x;const float wet=postLP.process(asymSat(x*(1.0f+7.0f*d),0.01f,0.18f));return lerp(wet,dry,params.cleanMix);}
            case PedalType::hardDistortion: x*=1.0f+18.0f*d; x=juce::jlimit(-1.0f,1.0f,x); return postLP.process(x);
            case PedalType::germaniumFuzz: return germanium(x,d);
            case PedalType::siliconFuzz:{float y=std::tanh(x*(3.0f+42.0f*d));y=midPeak.process(y);return postLP.process(y);}
            case PedalType::octaveFuzz: return octaveFuzz(x,d);
            case PedalType::velcroFuzz: return velcro(x,d);
        }
        return x;
    }
    PedalType type=PedalType::midOD; PedalParams params; double sampleRate=48000; int maxSamples=512; float env=0;
    NonlinearOversampler oversampling; juce::AudioBuffer<float> work; OnePoleHP preHP; OnePoleLP postLP; Biquad midPeak;
    Slew geMemory, starveEnv, octaveDc; bool velcroGateOpen=false;
};
}
