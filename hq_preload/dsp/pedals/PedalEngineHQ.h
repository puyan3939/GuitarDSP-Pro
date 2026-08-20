#pragma once
#include <JuceHeader.h>
#include <memory>
#include "../common/HQDSP.h"
#include "PitchOctaverHQ.h"

namespace guitardsp::hq
{
enum class PedalType { cleanBoost, trebleBoost, midOD, transparentOD, hardDistortion, germaniumFuzz, siliconFuzz, octaveFuzz, velcroFuzz, hqOctaver };

struct PedalParams
{
    float drive=0.5f, tone=0.5f, levelDb=0.0f;
    float lowCutHz=80.0f, focusHz=900.0f, midDb=0.0f, presenceDb=0.0f;
    float cleanMix=0.0f, bias=0.0f, scoop=0.0f, octave=0.6f, starve=0.0f, gate=0.0f;
};

class PedalEngineHQ
{
public:
    PedalEngineHQ(): oversampling(4) {} // 16x for nonlinear pedal topologies (48 kHz -> 768 kHz internal)

    void prepare(double fs,int maxBlock)
    {
        sampleRate=fs; maxSamples=maxBlock; oversampling.prepare(fs,maxBlock); work.setSize(1,maxBlock);
        pitchOctaver.prepare(fs,maxBlock);
        const double internalFs=oversampling.getInternalSampleRate();

        inputHP.prepare(internalFs); inputLP.prepare(internalFs);
        couplingHP1.prepare(internalFs); couplingHP2.prepare(internalFs);
        stageLP1.prepare(internalFs); stageLP2.prepare(internalFs); outputLP.prepare(internalFs);
        postDc.prepare(fs);
        midPeak.reset();

        biasMemory.prepare(internalFs,3.5f);
        supplyEnv.prepare(internalFs,18.0f);
        octaveDc.prepare(internalFs,8.0f);
        gateEnv.prepare(internalFs,4.0f);

        updateFilters(); reset();
    }

    void reset()
    {
        oversampling.reset();
        pitchOctaver.reset();
        inputHP.reset(); inputLP.reset(); couplingHP1.reset(); couplingHP2.reset();
        stageLP1.reset(); stageLP2.reset(); outputLP.reset(); postDc.reset(); midPeak.reset();
        biasMemory.reset(); supplyEnv.reset(); octaveDc.reset(); gateEnv.reset();
        transistorFeedback=0.0f; fuzzFeedback=0.0f; velcroGateOpen=false;
    }

    void setType(PedalType t)
    {
        if (t != type)
        {
            type=t;
            reset();
        }
        updateFilters();
    }

    void setParameters(const PedalParams&p)
    {
        params=p;
        updateFilters();
        if(type==PedalType::hqOctaver)
        {
            PitchOctaverHQ::Params q;
            q.octaveUp=juce::jlimit(0.0f,1.0f,params.drive);
            q.octaveDown=juce::jlimit(0.0f,1.0f,params.tone);
            q.dry=juce::jlimit(0.0f,1.0f,params.cleanMix);
            q.tracking=juce::jlimit(0.0f,1.0f,(params.lowCutHz-40.0f)/260.0f);
            q.tone=juce::jlimit(0.0f,1.0f,(params.focusHz-300.0f)/2200.0f);
            q.smooth=juce::jlimit(0.0f,1.0f,(params.midDb+12.0f)/24.0f);
            pitchOctaver.setParameters(q);
        }
    }

    void process(juce::AudioBuffer<float>& mono)
    {
        const int n=mono.getNumSamples(); if(n<=0)return;

        if(type==PedalType::hqOctaver)
        {
            pitchOctaver.processBlock(mono);
            mono.applyGain(dbToGain(params.levelDb));
            return;
        }

        work.setSize(1,n,false,false,true); work.copyFrom(0,0,mono,0,0,n);
        oversampling.process(work,[this](float x){return nonlinear(x);});
        mono.copyFrom(0,0,work,0,0,n);
        auto* d=mono.getWritePointer(0);
        for(int i=0;i<n;++i) d[i]=postDc.process(d[i]);
        mono.applyGain(dbToGain(params.levelDb));
    }

private:
    static float diodePair(float x,float knee,float asym=0.0f) noexcept
    {
        const float kp=juce::jmax(0.06f,knee*(1.0f+0.30f*asym));
        const float kn=juce::jmax(0.06f,knee*(1.0f-0.22f*asym));
        return x>=0.0f ? kp*std::tanh(x/kp) : kn*std::tanh(x/kn);
    }

    static float transistor(float x,float gain,float bias,float asym,float headroom) noexcept
    {
        const float h=juce::jmax(0.08f,headroom);
        return h*asymSat((x*gain)/h,bias,asym);
    }

    float conditionInput(float x) noexcept
    {
        return inputLP.process(inputHP.process(x));
    }

    void updateFilters()
    {
        const float tone=juce::jlimit(0.0f,1.0f,params.tone);
        const float inputCut=juce::jlimit(20.0f,900.0f,params.lowCutHz);

        inputHP.setHz(inputCut);
        const bool highGain = type==PedalType::hardDistortion || type==PedalType::germaniumFuzz ||
                              type==PedalType::siliconFuzz || type==PedalType::octaveFuzz ||
                              type==PedalType::velcroFuzz;
        inputLP.setHz(highGain ? lerp(6500.0f,12500.0f,tone)
                               : lerp(10000.0f,18000.0f,tone));
        postDc.setHz(18.0f);

        couplingHP1.setHz(juce::jlimit(35.0f,1200.0f,inputCut*1.35f));
        couplingHP2.setHz(juce::jlimit(25.0f,1000.0f,inputCut*0.90f));

        stageLP1.setHz(lerp(6500.0f,18000.0f,tone));
        stageLP2.setHz(lerp(4200.0f,14500.0f,tone));
        outputLP.setHz(lerp(3200.0f,12500.0f,tone));

        midPeak.setPeak(oversampling.getInternalSampleRate(),
                        juce::jlimit(250.0f,3500.0f,params.focusHz),
                        0.75f,
                        params.midDb);
    }

    float cleanBoost(float x,float d)
    {
        x=conditionInput(x);
        const float q1=transistor(x,1.0f+2.4f*d,0.008f,0.16f,1.35f);
        const float coupled=couplingHP2.process(q1);
        const float buffer=transistor(coupled,1.0f+0.45f*d,0.0f,0.06f,1.8f);
        return stageLP1.process(buffer);
    }

    float trebleBoost(float x,float d)
    {
        x=conditionInput(x);
        x=couplingHP1.process(x);
        const float dynamicBias=0.025f+0.05f*params.bias+0.025f*biasMemory.process(x);
        const float q1=transistor(x,1.4f+5.0f*d,dynamicBias,0.72f,0.82f);
        return stageLP2.process(q1);
    }

    float midOverdrive(float x,float d)
    {
        x=conditionInput(x);
        const float pre=midPeak.process(couplingHP1.process(x));
        const float opGain=1.2f+5.2f*d;
        const float op=pre*opGain;
        const float clipped=diodePair(op,0.48f-0.14f*d,0.10f+0.15f*params.bias);
        const float recovered=0.72f*clipped+0.28f*pre;
        return outputLP.process(couplingHP2.process(recovered));
    }

    float transparentOverdrive(float x,float d)
    {
        const float clean=conditionInput(x);
        const float dirtyIn=couplingHP1.process(clean);
        const float op=dirtyIn*(1.0f+4.2f*d);
        const float clipped=diodePair(op,0.60f-0.16f*d,0.04f);
        const float dirty=stageLP1.process(couplingHP2.process(clipped));
        const float cleanBlend=juce::jlimit(0.0f,1.0f,params.cleanMix);
        return lerp(dirty,clean,cleanBlend);
    }

    float hardDistortion(float x,float d)
    {
        x=conditionInput(x);
        const float pre=transistor(x,1.4f+3.4f*d,0.012f,0.22f,1.25f);
        const float coupled=couplingHP1.process(pre);
        const float gain2=coupled*(1.5f+4.5f*d);
        const float shunt=diodePair(gain2,0.34f-0.08f*d,0.18f);
        return outputLP.process(couplingHP2.process(shunt));
    }

    float germaniumFuzz(float x,float d)
    {
        x=conditionInput(x);
        const float memory=biasMemory.process(transistorFeedback);
        const float q1Bias=params.bias*0.22f+0.020f+0.035f*memory;
        const float q1In=x-transistorFeedback*(0.055f+0.095f*d);
        const float q1=transistor(q1In,1.6f+3.5f*d,q1Bias,0.78f,0.72f);
        const float coupled=couplingHP2.process(q1);
        const float demand=supplyEnv.process(std::abs(q1));
        const float supply=juce::jlimit(0.46f,0.78f,0.72f-0.12f*demand);
        const float q2=transistor(coupled,1.8f+4.4f*d,-0.018f+0.35f*q1Bias,0.90f,supply);
        transistorFeedback=0.985f*transistorFeedback+0.015f*q2;
        return outputLP.process(q2);
    }

    float siliconFuzz(float x,float d)
    {
        x=conditionInput(x);
        const float q1In=x-transistorFeedback*(0.035f+0.070f*d);
        const float q1=transistor(q1In,1.8f+4.4f*d,0.010f+0.06f*params.bias,0.48f,0.94f);
        const float coupled=couplingHP1.process(q1);
        const float q2=transistor(coupled,1.7f+5.2f*d,-0.008f,0.62f,0.90f);
        transistorFeedback=0.988f*transistorFeedback+0.012f*q2;
        const float shaped=midPeak.process(q2);
        return outputLP.process(shaped);
    }

    float octaveFuzz(float x,float d)
    {
        x=conditionInput(x);
        const float pre=transistor(x,1.5f+3.8f*d,0.020f,0.50f,0.90f);
        const float rect=std::abs(pre);
        const float dc=octaveDc.process(rect);
        const float octaveSignal=(rect-dc)*1.75f;
        const float octaveAmount=juce::jlimit(0.0f,1.0f,params.octave);
        const float combined=lerp(pre,octaveSignal,octaveAmount);
        const float recovery=transistor(couplingHP2.process(combined),1.3f+2.7f*d,0.012f,0.58f,0.82f);
        return outputLP.process(recovery);
    }

    float velcroFuzz(float x,float d)
    {
        x=conditionInput(x);
        const float envelope=gateEnv.process(std::abs(x));
        const float demand=supplyEnv.process(envelope*(0.8f+1.2f*d));
        const float starve=juce::jlimit(0.0f,1.0f,params.starve);
        const float supply=juce::jlimit(0.12f,0.92f,0.92f-starve*(0.52f+0.55f*demand));

        const float openTh=0.0018f+0.030f*juce::jlimit(0.0f,1.0f,params.gate);
        const float closeTh=openTh*0.48f;
        if(!velcroGateOpen && envelope>openTh) velcroGateOpen=true;
        else if(velcroGateOpen && envelope<closeTh) velcroGateOpen=false;

        // AUX1 arrives through the generic BIAS mapping (-0.2..+0.2). For this
        // topology it is reinterpreted as STABILITY: high = conventional/stable,
        // low = stronger positive feedback and Fuzz-Factory-like edge/ring.
        const float stability=juce::jlimit(0.0f,1.0f,(params.bias+0.2f)/0.4f);
        const float feedbackAmount=(1.0f-stability)*(0.18f+0.50f*d);
        const float feedbackInput=juce::jlimit(-1.5f,1.5f,x+feedbackAmount*fuzzFeedback);

        const float collapsingBias=0.025f+0.22f*starve+(1.0f-supply)*0.18f;
        const float q1=transistor(feedbackInput,1.7f+4.0f*d,collapsingBias,0.95f,supply);
        const float q2=transistor(couplingHP1.process(q1),1.5f+4.6f*d,-0.45f*collapsingBias,0.98f,juce::jmax(0.10f,supply*0.82f));

        // Bounded stateful feedback can ring after a note when STABILITY is low,
        // but exact digital silence remains silent because no noise is injected.
        fuzzFeedback=0.992f*fuzzFeedback+0.008f*q2;
        fuzzFeedback=juce::jlimit(-1.2f,1.2f,fuzzFeedback);

        const float gateGain=velcroGateOpen ? 1.0f : juce::jlimit(0.0f,1.0f,envelope/(openTh+1.0e-8f));
        return outputLP.process(q2*gateGain);
    }

    float nonlinear(float x)
    {
        const float d=juce::jlimit(0.0f,1.0f,params.drive);
        switch(type)
        {
            case PedalType::cleanBoost:        return cleanBoost(x,d);
            case PedalType::trebleBoost:       return trebleBoost(x,d);
            case PedalType::midOD:             return midOverdrive(x,d);
            case PedalType::transparentOD:     return transparentOverdrive(x,d);
            case PedalType::hardDistortion:    return hardDistortion(x,d);
            case PedalType::germaniumFuzz:     return germaniumFuzz(x,d);
            case PedalType::siliconFuzz:       return siliconFuzz(x,d);
            case PedalType::octaveFuzz:        return octaveFuzz(x,d);
            case PedalType::velcroFuzz:        return velcroFuzz(x,d);
            case PedalType::hqOctaver:         return x;
        }
        return x;
    }

    PedalType type=PedalType::midOD;
    PedalParams params;
    double sampleRate=48000;
    int maxSamples=512;

    NonlinearOversampler oversampling;
    PitchOctaverHQ pitchOctaver;
    juce::AudioBuffer<float> work;

    OnePoleHP inputHP,couplingHP1,couplingHP2,postDc;
    OnePoleLP inputLP,stageLP1,stageLP2,outputLP;
    Biquad midPeak;
    Slew biasMemory,supplyEnv,octaveDc,gateEnv;

    float transistorFeedback=0.0f;
    float fuzzFeedback=0.0f;
    bool velcroGateOpen=false;
};
}
