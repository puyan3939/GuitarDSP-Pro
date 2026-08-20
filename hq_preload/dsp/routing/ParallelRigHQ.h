#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../common/HQDSP.h"
#include "../pedals/PitchOctaverHQ.h"

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
};

class ParallelRigHQ
{
public:
    void prepare(double sampleRate, int maximumBlockSize)
    {
        fs=sampleRate;maxBlock=maximumBlockSize;cleanWork.setSize(1,maximumBlockSize,false,false,true);subWork.setSize(1,maximumBlockSize,false,false,true);octave.prepare(sampleRate,maximumBlockSize);
        cleanHp.prepare(fs);cleanLp.prepare(fs);cleanDc.prepare(fs);cleanBass.reset();cleanMid.reset();cleanTreble.reset();cleanPresence.reset();cleanSag.prepare(fs,28.0f);
        subHp.prepare(fs);subLp.prepare(fs);subDc.prepare(fs);subBass.reset();subMid.reset();subTreble.reset();subBody.reset();subSag.prepare(fs,42.0f);
        mainDelay.prepare(fs,90.0f);cleanDelay.prepare(fs,90.0f);subDelay.prepare(fs,90.0f);reset();
    }
    void reset()
    {
        octave.reset();cleanHp.reset();cleanLp.reset();cleanDc.reset();cleanBass.reset();cleanMid.reset();cleanTreble.reset();cleanPresence.reset();cleanSag.reset();
        subHp.reset();subLp.reset();subDc.reset();subBass.reset();subMid.reset();subTreble.reset();subBody.reset();subSag.reset();mainDelay.reset();cleanDelay.reset();subDelay.reset();cleanWork.clear();subWork.clear();
    }
    void process(const juce::AudioBuffer<float>& dryInput,juce::AudioBuffer<float>& processedMain,int startSample,int numSamples,const ParallelRigControl& c)
    {
        if(!c.enabled.load(std::memory_order_relaxed)||numSamples<=0)return;jassert(numSamples<=maxBlock);if(dryInput.getNumChannels()<=0||processedMain.getNumChannels()<=0)return;updateFilters(c);
        cleanWork.copyFrom(0,0,dryInput,0,0,numSamples);subWork.copyFrom(0,0,dryInput,0,0,numSamples);
        if(c.cleanEnabled.load())processCleanAmp(cleanWork,numSamples,c);else cleanWork.clear();if(c.subEnabled.load())processBassAmp(subWork,numSamples,c);else subWork.clear();
        const float mainGain=dbToGain(juce::jlimit(-60.0f,12.0f,c.mainLevelDb.load())),cleanGain=dbToGain(juce::jlimit(-60.0f,12.0f,c.cleanLevelDb.load())),subGain=dbToGain(juce::jlimit(-60.0f,12.0f,c.subLevelDb.load()));
        const float cleanPolarity=c.cleanInvert.load()?-1.0f:1.0f,subPolarity=c.subInvert.load()?-1.0f:1.0f;
        float mainMs=c.mainDelayMs.load(),cleanMs=c.cleanDelayMs.load(),subMs=c.subDelayMs.load();
        if(c.autoLatencyComp.load()&&c.subEnabled.load())
        {
            const float subLatencyMs=1000.0f*octave.getEstimatedLatencySamples(c.subTracking.load())/(float)fs;mainMs+=subLatencyMs;cleanMs+=subLatencyMs;
        }
        mainDelay.setDelayMs(mainMs);cleanDelay.setDelayMs(cleanMs);subDelay.setDelayMs(subMs);
        const auto* clean=cleanWork.getReadPointer(0);const auto* sub=subWork.getReadPointer(0);auto* mainOut=processedMain.getWritePointer(0,startSample);
        for(int i=0;i<numSamples;++i)mainOut[i]=mainGain*mainDelay.process(mainOut[i])+cleanGain*cleanPolarity*cleanDelay.process(clean[i])+subGain*subPolarity*subDelay.process(sub[i]);
        const int channels=juce::jmin(2,processedMain.getNumChannels());for(int ch=1;ch<channels;++ch)processedMain.copyFrom(ch,startSample,processedMain,0,startSample,numSamples);
    }
    void copySubStemTo(juce::AudioBuffer<float>& dest,int channel,int startSample,int numSamples,float levelDb=0.0f)const
    {
        if(channel<0||channel>=dest.getNumChannels()||numSamples<=0)return;const float g=dbToGain(levelDb);const auto*s=subWork.getReadPointer(0);auto*d=dest.getWritePointer(channel,startSample);for(int i=0;i<numSamples;++i)d[i]=s[i]*g;
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
    void updateFilters(const ParallelRigControl& c)
    {
        cleanHp.setHz(juce::jlimit(40.0f,5000.0f,c.cleanHpHz.load()));cleanLp.setHz(juce::jlimit(2500.0f,20000.0f,c.cleanLpHz.load()));cleanDc.setHz(18.0f);
        cleanBass.setPeak(fs,110.0f,.72f,juce::jlimit(-12.0f,12.0f,c.cleanBassDb.load()));cleanMid.setPeak(fs,720.0f,.82f,juce::jlimit(-12.0f,12.0f,c.cleanMidDb.load()));cleanTreble.setPeak(fs,3300.0f,.74f,juce::jlimit(-12.0f,12.0f,c.cleanTrebleDb.load()));cleanPresence.setPeak(fs,4700.0f,.72f,juce::jlimit(-12.0f,12.0f,c.cleanPresenceDb.load()));
        subHp.setHz(juce::jlimit(18.0f,180.0f,c.subHpHz.load()));subLp.setHz(juce::jlimit(500.0f,8000.0f,c.subLpHz.load()));subDc.setHz(18.0f);
        subBass.setPeak(fs,78.0f,.72f,juce::jlimit(-12.0f,12.0f,c.subBassDb.load()));subMid.setPeak(fs,520.0f,.85f,juce::jlimit(-12.0f,12.0f,c.subMidDb.load()));subTreble.setPeak(fs,2200.0f,.75f,juce::jlimit(-12.0f,12.0f,c.subTrebleDb.load()));subBody.setPeak(fs,105.0f,.80f,juce::jlimit(-12.0f,12.0f,c.subBodyDb.load()));
    }
    void processCleanAmp(juce::AudioBuffer<float>& b,int n,const ParallelRigControl& c)
    {
        auto*d=b.getWritePointer(0);const float drive=juce::jlimit(0.0f,1.0f,c.cleanDrive.load());for(int i=0;i<n;++i){float x=cleanLp.process(cleanHp.process(d[i]));x=cleanBass.process(x);x=cleanMid.process(x);x=cleanTreble.process(x);const float env=cleanSag.process(std::abs(x));const float supply=1.0f-.08f*drive*juce::jlimit(0.0f,1.0f,env*3.0f);const float sat=supply*1.25f*asymSat((x*(1.0f+2.4f*drive))/juce::jmax(.35f,supply),.006f,.12f);x=lerp(x,sat,.12f+.42f*drive);x=cleanPresence.process(x);d[i]=cleanDc.process(x);}
    }
    void processBassAmp(juce::AudioBuffer<float>& b,int n,const ParallelRigControl& c)
    {
        PitchOctaverHQ::Params p;p.octaveUp=0;p.octaveDown=1;p.dry=0;p.tracking=juce::jlimit(0.0f,1.0f,c.subTracking.load());p.tone=juce::jlimit(0.0f,1.0f,c.subTone.load());p.smooth=juce::jlimit(0.0f,1.0f,c.subSmooth.load());octave.setParameters(p);octave.processBlock(b);
        auto*d=b.getWritePointer(0);const float drive=juce::jlimit(0.0f,1.0f,c.subDrive.load());for(int i=0;i<n;++i){float x=subHp.process(d[i]);x=subBass.process(x);x=subBody.process(x);x=subMid.process(x);const float env=subSag.process(std::abs(x));const float supply=1.0f-.16f*drive*juce::jlimit(0.0f,1.0f,env*2.2f);const float pre=x*(1.0f+4.6f*drive),sat=supply*1.15f*asymSat(pre/juce::jmax(.38f,supply),.008f,.18f);x=lerp(x,sat,.22f+.58f*drive);x=subTreble.process(x);x=subLp.process(x);d[i]=subDc.process(x);}
    }
    double fs=48000;int maxBlock=0;juce::AudioBuffer<float>cleanWork,subWork;PitchOctaverHQ octave;OnePoleHP cleanHp,cleanDc,subHp,subDc;OnePoleLP cleanLp,subLp;Biquad cleanBass,cleanMid,cleanTreble,cleanPresence,subBass,subMid,subTreble,subBody;Slew cleanSag,subSag;IntegerDelay mainDelay,cleanDelay,subDelay;
};
}
