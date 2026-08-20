#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <vector>
#include <cmath>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
struct InputLoadingControl
{
    std::atomic<bool> enabled{false};
    std::atomic<float> pickupResistanceOhm{6500.0f};
    std::atomic<float> pickupInductanceH{3.2f};
    std::atomic<float> cableCapacitancePf{470.0f};
    std::atomic<float> inputImpedanceOhm{1000000.0f};
    std::atomic<float> trimDb{0.0f};
};

class InputLoadingHQ
{
public:
    void prepare(double sampleRate) { fs=sampleRate; resonance.reset(); dc.prepare(fs); dc.setHz(12.0f); }
    void reset() { resonance.reset(); dc.reset(); }
    float process(float x,const InputLoadingControl& c) noexcept
    {
        if(!c.enabled.load(std::memory_order_relaxed)) return x;
        update(c); return dc.process(resonance.process(x)*linearGain);
    }
private:
    void update(const InputLoadingControl& c) noexcept
    {
        const float rp=juce::jlimit(1500.0f,30000.0f,c.pickupResistanceOhm.load());
        const float L=juce::jlimit(0.2f,12.0f,c.pickupInductanceH.load());
        const float Cpf=juce::jlimit(50.0f,3000.0f,c.cableCapacitancePf.load());
        const float Rin=juce::jlimit(22000.0f,10000000.0f,c.inputImpedanceOhm.load());
        const float C=Cpf*1.0e-12f;
        const float f0=juce::jlimit(350.0f,18000.0f,1.0f/(2.0f*juce::MathConstants<float>::pi*std::sqrt(L*C)));
        const float parallelR=(rp*Rin)/(rp+Rin);
        const float q=juce::jlimit(0.30f,4.5f,parallelR*std::sqrt(C/L));
        const float load=Rin/(Rin+rp);
        const float resonanceDb=juce::jlimit(-3.0f,8.0f,20.0f*std::log10(1.0f+1.6f*q));
        resonance.setPeak(fs,f0,juce::jlimit(0.35f,3.0f,q),resonanceDb);
        linearGain=load*dbToGain(juce::jlimit(-12.0f,12.0f,c.trimDb.load()));
    }
    double fs=48000.0; Biquad resonance; OnePoleHP dc; float linearGain=1.0f;
};

struct ExpressionPitchControl
{
    std::atomic<bool> enabled{false};
    std::atomic<float> semitones{0.0f},expression{1.0f},wet{1.0f},dry{0.0f};
    std::atomic<float> tracking{0.55f},tone{0.75f},smooth{0.60f};
};

class ExpressionPitchHQ
{
public:
    ExpressionPitchHQ(): oversampling(3) {}
    void prepare(double sampleRate,int maxBlock)
    {
        externalFs=sampleRate; oversampling.prepare(sampleRate,maxBlock); fs=oversampling.getInternalSampleRate();
        const int wanted=juce::jmax(8192,(int)std::ceil(fs*0.20)+maxBlock*oversampling.getFactor()+64);
        int size=1; while(size<wanted)size<<=1; memory.assign((size_t)size,0.0f); mask=size-1;
        toneLp.prepare(fs);dc.prepare(fs);dc.setHz(15.0f);ratioSmooth.reset(fs,0.010);ratioSmooth.setCurrentAndTargetValue(1.0f);reset();
    }
    void reset() noexcept
    {
        oversampling.reset();std::fill(memory.begin(),memory.end(),0.0f);write=0;phase=0;toneLp.reset();dc.reset();ratioSmooth.setCurrentAndTargetValue(1.0f);
    }
    float estimatedLatencySamples(const ExpressionPitchControl& c) const noexcept
    {
        const float wMs=lerp(14.0f,54.0f,juce::jlimit(0.0f,1.0f,c.tracking.load()));
        return 0.5f*0.001f*wMs*(float)externalFs+oversampling.getLatencySamples();
    }
    void process(juce::AudioBuffer<float>& mono,const ExpressionPitchControl& c)
    {
        if(!c.enabled.load(std::memory_order_relaxed)||mono.getNumSamples()<=0)return;
        const float semi=juce::jlimit(-24.0f,24.0f,c.semitones.load())*juce::jlimit(0.0f,1.0f,c.expression.load());
        ratioSmooth.setTargetValue(std::pow(2.0f,semi/12.0f));toneLp.setHz(lerp(4200.0f,17000.0f,juce::jlimit(0.0f,1.0f,c.tone.load())));
        wet=juce::jlimit(0.0f,1.0f,c.wet.load());dry=juce::jlimit(0.0f,1.0f,c.dry.load());tracking=juce::jlimit(0.0f,1.0f,c.tracking.load());smooth=juce::jlimit(0.0f,1.0f,c.smooth.load());
        oversampling.process(mono,[this](float x) noexcept{return processInternal(x);});
    }
private:
    static float wrap(float p) noexcept{p-=std::floor(p);return p;}
    static float hann(float p) noexcept{const float s=std::sin(juce::MathConstants<float>::pi*p);return s*s;}
    float readCubic(float idx)const noexcept
    {
        const int i1=(int)std::floor(idx);const float f=idx-(float)i1;
        const float y0=memory[(size_t)((i1-1)&mask)],y1=memory[(size_t)(i1&mask)],y2=memory[(size_t)((i1+1)&mask)],y3=memory[(size_t)((i1+2)&mask)];
        const float a0=y3-y2-y0+y1,a1=y0-y1-a0,a2=y2-y0;return((a0*f+a1)*f+a2)*f+y1;
    }
    float processInternal(float x)noexcept
    {
        memory[(size_t)(write&mask)]=x;const float ratio=juce::jlimit(0.25f,4.0f,ratioSmooth.getNextValue());
        const float window=juce::jlimit(96.0f,(float)memory.size()*0.30f,0.001f*lerp(14.0f,54.0f,tracking)*(float)fs);const float p2=wrap(phase+0.5f),minDelay=10.0f+0.0035f*(float)fs;
        auto delayFor=[ratio,window,minDelay](float p){return ratio>=1.0f?minDelay+window*(1.0f-p):minDelay+window*p;};
        const float a=readCubic((float)write-delayFor(phase)),b=readCubic((float)write-delayFor(p2)),wa=hann(phase),wb=hann(p2);float shifted=(a*wa+b*wb)/juce::jmax(1.0e-5f,wa+wb);
        phase=wrap(phase+std::abs(1.0f-ratio)/window);++write;const float filt=toneLp.process(shifted);shifted=lerp(shifted,filt,0.15f+0.80f*smooth);const float normal=1.0f/juce::jmax(1.0f,wet+dry);return dc.process((dry*x+wet*shifted)*normal);
    }
    double externalFs=48000,fs=384000;NonlinearOversampler oversampling;std::vector<float>memory;int mask=0,write=0;float phase=0;juce::SmoothedValue<float>ratioSmooth;OnePoleLP toneLp;OnePoleHP dc;float wet=1,dry=0,tracking=.55f,smooth=.6f;
};

struct DualDelayControl
{
    std::atomic<bool> enabled{false};
    std::atomic<float> timeLms{280},timeRms{420},feedbackL{.28f},feedbackR{.32f},crossFeedback{.12f},mix{.20f};
    std::atomic<float> lowCutHz{120},highCutHz{9000},modRateHz{.35f},modDepthMs{.7f};
};

class DualDelayStereoHQ
{
public:
    DualDelayStereoHQ():dl(240000),dr(240000){}
    void prepare(double sampleRate,int maxBlock)
    {
        fs=sampleRate;juce::dsp::ProcessSpec s{fs,(juce::uint32)maxBlock,1};dl.prepare(s);dr.prepare(s);
        hpL.prepare(fs);hpR.prepare(fs);lpL.prepare(fs);lpR.prepare(fs);reset();
    }
    void reset(){dl.reset();dr.reset();phase=0;lastL=lastR=0;hpL.reset();hpR.reset();lpL.reset();lpR.reset();}
    void process(juce::AudioBuffer<float>& b,int start,int n,const DualDelayControl& c)
    {
        if(!c.enabled.load()||b.getNumChannels()<2)return;const float mix=juce::jlimit(0.0f,1.0f,c.mix.load()),xfb=juce::jlimit(-.9f,.9f,c.crossFeedback.load());
        hpL.setHz(c.lowCutHz.load());hpR.setHz(c.lowCutHz.load());lpL.setHz(c.highCutHz.load());lpR.setHz(c.highCutHz.load());auto*L=b.getWritePointer(0,start);auto*R=b.getWritePointer(1,start);
        for(int i=0;i<n;++i)
        {
            const float mod=std::sin(phase)*juce::jlimit(0.0f,8.0f,c.modDepthMs.load());phase+=2.0f*juce::MathConstants<float>::pi*juce::jlimit(.01f,8.0f,c.modRateHz.load())/(float)fs;if(phase>6.283185f)phase-=6.283185f;
            dl.setDelay(juce::jlimit(1.0f,(float)(fs*4.5),.001f*(c.timeLms.load()+mod)*(float)fs));dr.setDelay(juce::jlimit(1.0f,(float)(fs*4.5),.001f*(c.timeRms.load()-mod)*(float)fs));
            const float wetL=lpL.process(hpL.process(dl.popSample(0))),wetR=lpR.process(hpR.process(dr.popSample(0)));dl.pushSample(0,L[i]+juce::jlimit(-.95f,.95f,c.feedbackL.load())*wetL+xfb*lastR);dr.pushSample(0,R[i]+juce::jlimit(-.95f,.95f,c.feedbackR.load())*wetR+xfb*lastL);lastL=wetL;lastR=wetR;L[i]=lerp(L[i],wetL,mix);R[i]=lerp(R[i],wetR,mix);
        }
    }
private:
    double fs=48000;float phase=0,lastL=0,lastR=0;OnePoleHP hpL,hpR;OnePoleLP lpL,lpR;juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Lagrange3rd>dl,dr;
};

struct PerformanceScene
{
    bool parallelEnabled=false,expressionPitchEnabled=false,dualDelayEnabled=false;float expression=1,pitchSemitones=0,mainDb=0,cleanDb=-12,subDb=-8;std::array<int,4>pedalRouteMask{{1,1,1,1}};std::array<bool,4>pedalEnabled{{false,false,false,false}};
};
class SceneSwitcherHQ
{
public:
    static constexpr int sceneCount=8;PerformanceScene&scene(int i)noexcept{return scenes[(size_t)juce::jlimit(0,sceneCount-1,i)];}const PerformanceScene&scene(int i)const noexcept{return scenes[(size_t)juce::jlimit(0,sceneCount-1,i)];}
    void request(int i)noexcept{requested.store(juce::jlimit(0,sceneCount-1,i),std::memory_order_release);}bool consumeRequest(int&index)noexcept{const int r=requested.exchange(-1,std::memory_order_acq_rel);if(r<0)return false;index=r;current.store(r,std::memory_order_relaxed);return true;}int currentScene()const noexcept{return current.load(std::memory_order_relaxed);}
private:std::array<PerformanceScene,sceneCount>scenes{};std::atomic<int>requested{-1},current{0};
};
}
