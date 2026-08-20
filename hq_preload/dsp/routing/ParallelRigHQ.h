#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../common/HQDSP.h"
#include "../pedals/PitchOctaverHQ.h"
#include "../dynamics/DynamicsHQ.h"

namespace guitardsp::hq
{
struct ParallelRigControl
{
    std::atomic<bool> enabled{false};
    std::atomic<bool> autoLatencyComp{true};
    std::atomic<float> mainLevelDb{0.0f}, mainDelayMs{0.0f};

    std::atomic<bool> cleanEnabled{true};
    std::atomic<float> cleanLevelDb{-12.0f}, cleanHpHz{650.0f}, cleanLpHz{14500.0f};
    std::atomic<float> cleanPresenceDb{4.0f}, cleanDrive{0.08f};
    std::atomic<float> cleanBassDb{0.0f}, cleanMidDb{-1.0f}, cleanTrebleDb{2.5f};
    std::atomic<float> cleanDelayMs{0.0f}; std::atomic<bool> cleanInvert{false};

    std::atomic<bool> subEnabled{true};
    std::atomic<float> subLevelDb{-8.0f}, subHpHz{32.0f}, subLpHz{3200.0f};
    std::atomic<float> subBodyDb{3.0f}, subDrive{0.34f};
    std::atomic<float> subBassDb{2.0f}, subMidDb{0.5f}, subTrebleDb{-2.0f};
    std::atomic<float> subTracking{0.72f}, subTone{0.62f}, subSmooth{0.68f};
    std::atomic<float> subDelayMs{0.0f}; std::atomic<bool> subInvert{false};

    // High-gain safety gate. Detection is always taken from the clean pre-split
    // signal, while attenuation is applied after MAIN+CLEAN+SUB are recombined.
    // This prevents fuzz/amp-generated hiss from holding the gate open.
    std::atomic<bool> postGateEnabled{true};
    std::atomic<float> postGateThresholdDb{-52.0f}, postGateRangeDb{-72.0f};
    std::atomic<float> postGateRatio{8.0f}, postGateAttackMs{0.8f}, postGateHoldMs{42.0f}, postGateReleaseMs{145.0f};
    std::atomic<float> postGateHysteresisDb{5.0f}, postGateHpHz{70.0f}, postGateLpHz{5200.0f};
};

class ParallelRigHQ
{
public:
    void prepare(double sampleRate, int maximumBlockSize)
    {
        fs=sampleRate; maxBlock=maximumBlockSize;
        cleanWork.setSize(1,maximumBlockSize,false,false,true);
        subWork.setSize(1,maximumBlockSize,false,false,true);
        mainCleanWork.setSize(1,maximumBlockSize,false,false,true);
        octave.prepare(sampleRate,maximumBlockSize);

        cleanHp.prepare(fs); cleanLp.prepare(fs); cleanDc.prepare(fs);
        cleanCoupling1.prepare(fs); cleanCoupling2.prepare(fs);
        cleanInterstageLp.prepare(fs); cleanRecoveryLp.prepare(fs); cleanTransformerLp.prepare(fs);
        cleanBass.reset(); cleanMid.reset(); cleanTreble.reset(); cleanPresence.reset();
        cleanSag.prepare(fs,34.0f); cleanBiasMemory.prepare(fs,5.0f); cleanNfbLow.prepare(fs);

        subHp.prepare(fs); subLp.prepare(fs); subDc.prepare(fs);
        subCoupling1.prepare(fs); subCoupling2.prepare(fs);
        subInterstageLp.prepare(fs); subRecoveryLp.prepare(fs); subTransformerLp.prepare(fs);
        subBass.reset(); subMid.reset(); subTreble.reset(); subBody.reset();
        subSag.prepare(fs,70.0f); subBiasMemory.prepare(fs,7.0f); subNfbLow.prepare(fs);

        postGate.prepare(fs);
        mainDelay.prepare(fs,90.0f); cleanDelay.prepare(fs,90.0f); subDelay.prepare(fs,90.0f);
        reset();
    }

    void reset()
    {
        octave.reset();
        cleanHp.reset(); cleanLp.reset(); cleanDc.reset(); cleanCoupling1.reset(); cleanCoupling2.reset();
        cleanInterstageLp.reset(); cleanRecoveryLp.reset(); cleanTransformerLp.reset();
        cleanBass.reset(); cleanMid.reset(); cleanTreble.reset(); cleanPresence.reset(); cleanSag.reset(); cleanBiasMemory.reset(); cleanNfbLow.reset();
        subHp.reset(); subLp.reset(); subDc.reset(); subCoupling1.reset(); subCoupling2.reset();
        subInterstageLp.reset(); subRecoveryLp.reset(); subTransformerLp.reset();
        subBass.reset(); subMid.reset(); subTreble.reset(); subBody.reset(); subSag.reset(); subBiasMemory.reset(); subNfbLow.reset();
        cleanNfbHistory=0.0f; subNfbHistory=0.0f; postGate.reset();
        mainDelay.reset(); cleanDelay.reset(); subDelay.reset();
        cleanWork.clear(); subWork.clear(); mainCleanWork.clear();
    }

    void process(const juce::AudioBuffer<float>& dryInput,juce::AudioBuffer<float>& processedMain,int startSample,int numSamples,const ParallelRigControl& c)
    {
        processBuses(dryInput,dryInput,processedMain,startSample,numSamples,c);
    }

    void processBuses(const juce::AudioBuffer<float>& cleanInput,const juce::AudioBuffer<float>& subInput,juce::AudioBuffer<float>& processedMain,int startSample,int numSamples,const ParallelRigControl& c)
    {
        if(!c.enabled.load(std::memory_order_relaxed)||numSamples<=0)return;
        jassert(numSamples<=maxBlock);
        if(cleanInput.getNumChannels()<=0||subInput.getNumChannels()<=0||processedMain.getNumChannels()<=0)return;
        updateFilters(c);
        updatePostGate(c);

        cleanWork.copyFrom(0,0,cleanInput,0,0,numSamples);
        subWork.copyFrom(0,0,subInput,0,0,numSamples);
        if(c.cleanEnabled.load()) processCleanAmp(cleanWork,numSamples,c); else cleanWork.clear();
        if(c.subEnabled.load()) processBassAmp(subWork,numSamples,c); else subWork.clear();

        const float mainGain=dbToGain(juce::jlimit(-60.0f,12.0f,c.mainLevelDb.load()));
        const float cleanGain=dbToGain(juce::jlimit(-60.0f,12.0f,c.cleanLevelDb.load()));
        const float subGain=dbToGain(juce::jlimit(-60.0f,12.0f,c.subLevelDb.load()));
        const float cleanPolarity=c.cleanInvert.load()?-1.0f:1.0f;
        const float subPolarity=c.subInvert.load()?-1.0f:1.0f;

        float mainMs=c.mainDelayMs.load(),cleanMs=c.cleanDelayMs.load(),subMs=c.subDelayMs.load();
        if(c.autoLatencyComp.load()&&c.subEnabled.load())
        {
            const float subLatencyMs=1000.0f*octave.getEstimatedLatencySamples(c.subTracking.load())/(float)fs;
            mainMs+=subLatencyMs; cleanMs+=subLatencyMs;
        }
        mainDelay.setDelayMs(mainMs); cleanDelay.setDelayMs(cleanMs); subDelay.setDelayMs(subMs);

        const auto* clean=cleanWork.getReadPointer(0);
        const auto* sub=subWork.getReadPointer(0);
        const auto* detector=cleanInput.getReadPointer(0);
        auto* mainOut=processedMain.getWritePointer(0,startSample);
        auto* stem=mainCleanWork.getWritePointer(0);
        const bool gateOn=c.postGateEnabled.load(std::memory_order_relaxed);

        for(int i=0;i<numSamples;++i)
        {
            const float mainClean=mainGain*mainDelay.process(mainOut[i])+cleanGain*cleanPolarity*cleanDelay.process(clean[i]);
            stem[i]=mainClean;
            float mixed=mainClean+subGain*subPolarity*subDelay.process(sub[i]);
            if(gateOn) mixed=postGate.processKeyed(mixed,detector[i]);
            mainOut[i]=mixed;
        }
        const int channels=juce::jmin(2,processedMain.getNumChannels());
        for(int ch=1;ch<channels;++ch) processedMain.copyFrom(ch,startSample,processedMain,0,startSample,numSamples);
    }

    void copySubStemTo(juce::AudioBuffer<float>& dest,int channel,int startSample,int numSamples,float levelDb=0.0f)const
    {
        if(channel<0||channel>=dest.getNumChannels()||numSamples<=0)return;
        const float g=dbToGain(levelDb); const auto*s=subWork.getReadPointer(0); auto*d=dest.getWritePointer(channel,startSample);
        for(int i=0;i<numSamples;++i)d[i]=s[i]*g;
    }

    void copyMainCleanStemTo(juce::AudioBuffer<float>& dest,int channel,int startSample,int numSamples,float levelDb=0.0f)const
    {
        if(channel<0||channel>=dest.getNumChannels()||numSamples<=0)return;
        const float g=dbToGain(levelDb); const auto*s=mainCleanWork.getReadPointer(0); auto*d=dest.getWritePointer(channel,startSample);
        for(int i=0;i<numSamples;++i)d[i]=s[i]*g;
    }

private:
    class IntegerDelay
    {
    public:
        void prepare(double sampleRate,float maxMs){fs=sampleRate;const int wanted=juce::jmax(8,(int)std::ceil(.001*maxMs*fs)+8);data.assign((size_t)wanted,0.0f);write=0;}
        void reset()noexcept{std::fill(data.begin(),data.end(),0.0f);write=0;}
        void setDelayMs(float ms)noexcept{if(data.empty()){delaySamples=0;return;}delaySamples=juce::jlimit(0,(int)data.size()-1,(int)std::lround(.001*juce::jlimit(0.0f,90.0f,ms)*fs));}
        float process(float x)noexcept{if(data.empty())return x;data[(size_t)write]=x;int read=write-delaySamples;while(read<0)read+=(int)data.size();const float y=data[(size_t)read];if(++write>=(int)data.size())write=0;return y;}
    private:std::vector<float>data;double fs=48000;int write=0,delaySamples=0;
    };

    static float triode(float x,float drive,float bias,float asym,float headroom) noexcept
    {
        const float h=juce::jmax(0.12f,headroom);
        return h*asymSat((x*drive)/h,bias,asym);
    }

    void updateFilters(const ParallelRigControl& c)
    {
        cleanHp.setHz(juce::jlimit(40.0f,5000.0f,c.cleanHpHz.load()));
        cleanLp.setHz(juce::jlimit(2500.0f,20000.0f,c.cleanLpHz.load())); cleanDc.setHz(18.0f);
        cleanCoupling1.setHz(72.0f); cleanCoupling2.setHz(46.0f); cleanInterstageLp.setHz(12800.0f); cleanRecoveryLp.setHz(11800.0f); cleanTransformerLp.setHz(9800.0f); cleanNfbLow.setHz(180.0f);
        cleanBass.setPeak(fs,110.0f,.72f,juce::jlimit(-12.0f,12.0f,c.cleanBassDb.load()));
        cleanMid.setPeak(fs,720.0f,.82f,juce::jlimit(-12.0f,12.0f,c.cleanMidDb.load()));
        cleanTreble.setPeak(fs,3300.0f,.74f,juce::jlimit(-12.0f,12.0f,c.cleanTrebleDb.load()));
        cleanPresence.setPeak(fs,4700.0f,.72f,juce::jlimit(-12.0f,12.0f,c.cleanPresenceDb.load()));

        subHp.setHz(juce::jlimit(18.0f,180.0f,c.subHpHz.load()));
        subLp.setHz(juce::jlimit(500.0f,8000.0f,c.subLpHz.load())); subDc.setHz(18.0f);
        subCoupling1.setHz(34.0f); subCoupling2.setHz(28.0f); subInterstageLp.setHz(7600.0f); subRecoveryLp.setHz(6500.0f); subTransformerLp.setHz(4700.0f); subNfbLow.setHz(120.0f);
        subBass.setPeak(fs,78.0f,.72f,juce::jlimit(-12.0f,12.0f,c.subBassDb.load()));
        subMid.setPeak(fs,520.0f,.85f,juce::jlimit(-12.0f,12.0f,c.subMidDb.load()));
        subTreble.setPeak(fs,2200.0f,.75f,juce::jlimit(-12.0f,12.0f,c.subTrebleDb.load()));
        subBody.setPeak(fs,105.0f,.80f,juce::jlimit(-12.0f,12.0f,c.subBodyDb.load()));
    }

    void updatePostGate(const ParallelRigControl& c)
    {
        NoiseGate::Params p;
        p.thresholdDb=c.postGateThresholdDb.load(); p.rangeDb=c.postGateRangeDb.load(); p.ratio=c.postGateRatio.load();
        p.attackMs=c.postGateAttackMs.load(); p.holdMs=c.postGateHoldMs.load(); p.releaseMs=c.postGateReleaseMs.load();
        p.hysteresisDb=c.postGateHysteresisDb.load(); p.sidechainHpHz=c.postGateHpHz.load(); p.sidechainLpHz=c.postGateLpHz.load();
        postGate.setParameters(p);
    }

    void processCleanAmp(juce::AudioBuffer<float>& b,int n,const ParallelRigControl& c)
    {
        auto*d=b.getWritePointer(0);
        const float drive=juce::jlimit(0.0f,1.0f,c.cleanDrive.load());
        for(int i=0;i<n;++i)
        {
            float x=cleanLp.process(cleanHp.process(d[i]));

            // V1 input triode: modest gain, dynamic bias memory, broad bandwidth.
            const float biasMem=cleanBiasMemory.process(x);
            const float v1=triode(x,1.18f+1.35f*drive,0.008f+0.012f*biasMem,0.18f,1.45f);
            x=cleanInterstageLp.process(cleanCoupling1.process(v1));

            // Passive-ish tone stack sits between gain stages instead of after a single clipper.
            x=cleanBass.process(x); x=cleanMid.process(x); x=cleanTreble.process(x);

            // Recovery stage restores level and adds a different asymmetry signature.
            const float recovery=triode(x,1.10f+1.65f*drive,-0.006f,0.12f,1.60f);
            x=cleanRecoveryLp.process(cleanCoupling2.process(recovery));

            // Simplified long-tail phase inverter: two opposed nonlinear halves cancel
            // some even-order content while keeping transient asymmetry.
            const float piA=triode(x,1.05f+0.70f*drive, 0.004f,0.10f,1.75f);
            const float piB=triode(-x,1.00f+0.64f*drive,-0.004f,0.10f,1.75f);
            const float pi=0.52f*(piA-piB);

            // Power stage with supply sag and frequency-shaped negative feedback.
            const float lowFb=cleanNfbLow.process(cleanNfbHistory);
            const float feedback=0.11f*cleanNfbHistory+0.05f*lowFb;
            const float powerIn=pi-feedback;
            const float env=cleanSag.process(std::abs(powerIn));
            const float supply=1.0f-0.10f*(0.25f+0.75f*drive)*juce::jlimit(0.0f,1.0f,env*2.6f);
            const float pa=supply*1.22f*asymSat((powerIn*(1.05f+1.30f*drive))/juce::jmax(.42f,supply),.002f,.08f);
            cleanNfbHistory=0.992f*cleanNfbHistory+0.008f*pa;

            x=cleanTransformerLp.process(pa);
            x=cleanPresence.process(x);
            d[i]=cleanDc.process(x);
        }
    }

    void processBassAmp(juce::AudioBuffer<float>& b,int n,const ParallelRigControl& c)
    {
        PitchOctaverHQ::Params p;
        p.octaveUp=0; p.octaveDown=1; p.dry=0;
        p.tracking=juce::jlimit(0.0f,1.0f,c.subTracking.load()); p.tone=juce::jlimit(0.0f,1.0f,c.subTone.load()); p.smooth=juce::jlimit(0.0f,1.0f,c.subSmooth.load());
        octave.setParameters(p); octave.processBlock(b);

        auto*d=b.getWritePointer(0);
        const float drive=juce::jlimit(0.0f,1.0f,c.subDrive.load());
        for(int i=0;i<n;++i)
        {
            float x=subHp.process(d[i]);

            // Bass preamp V1 deliberately keeps more low-frequency headroom.
            const float biasMem=subBiasMemory.process(x);
            const float v1=triode(x,1.20f+1.85f*drive,0.006f+0.010f*biasMem,0.14f,1.70f);
            x=subInterstageLp.process(subCoupling1.process(v1));

            x=subBass.process(x); x=subBody.process(x); x=subMid.process(x);

            // Recovery stage gives the octave voice density without one giant tanh stage.
            const float recovery=triode(x,1.15f+2.05f*drive,-0.004f,0.16f,1.45f);
            x=subRecoveryLp.process(subCoupling2.process(recovery));

            // Phase inverter / driver with wide low-end headroom.
            const float piA=triode(x,1.02f+0.92f*drive, 0.003f,0.09f,1.85f);
            const float piB=triode(-x,0.98f+0.84f*drive,-0.003f,0.09f,1.85f);
            const float pi=0.52f*(piA-piB);

            // Bass power stage: slower sag, more low-frequency feedback and transformer limiting.
            const float lowFb=subNfbLow.process(subNfbHistory);
            const float feedback=0.08f*subNfbHistory+0.10f*lowFb;
            const float powerIn=pi-feedback;
            const float env=subSag.process(std::abs(powerIn));
            const float supply=1.0f-0.15f*(0.20f+0.80f*drive)*juce::jlimit(0.0f,1.0f,env*2.0f);
            const float pa=supply*1.18f*asymSat((powerIn*(1.0f+1.70f*drive))/juce::jmax(.46f,supply),.003f,.11f);
            subNfbHistory=0.994f*subNfbHistory+0.006f*pa;

            x=subTreble.process(pa);
            x=subTransformerLp.process(x);
            x=subLp.process(x);
            d[i]=subDc.process(x);
        }
    }

    double fs=48000; int maxBlock=0;
    juce::AudioBuffer<float>cleanWork,subWork,mainCleanWork;
    PitchOctaverHQ octave;

    OnePoleHP cleanHp,cleanDc,cleanCoupling1,cleanCoupling2,subHp,subDc,subCoupling1,subCoupling2;
    OnePoleLP cleanLp,cleanInterstageLp,cleanRecoveryLp,cleanTransformerLp,cleanNfbLow;
    OnePoleLP subLp,subInterstageLp,subRecoveryLp,subTransformerLp,subNfbLow;
    Biquad cleanBass,cleanMid,cleanTreble,cleanPresence,subBass,subMid,subTreble,subBody;
    Slew cleanSag,subSag,cleanBiasMemory,subBiasMemory;
    float cleanNfbHistory=0.0f, subNfbHistory=0.0f;

    NoiseGate postGate;
    IntegerDelay mainDelay,cleanDelay,subDelay;
};
}
