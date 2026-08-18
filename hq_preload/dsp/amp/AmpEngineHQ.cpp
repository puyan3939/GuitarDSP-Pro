#include "AmpEngineHQ.h"
#include <memory>

namespace guitardsp::hq
{
struct AmpEngineHQ::TubeStage
{
    OnePoleHP hp; OnePoleLP preLp,postLp; Slew memory;
    AmpStageParams p;
    void prepare(double fs){hp.prepare(fs);preLp.prepare(fs);postLp.prepare(fs);memory.prepare(fs,4.0f);}
    void set(const AmpStageParams& x){p=x;hp.setHz(x.preHpHz);preLp.setHz(x.preLpHz);postLp.setHz(x.postLpHz);}
    float process(float x)
    {
        x=preLp.process(hp.process(x)); const float mem=memory.process(x); const float dynamicBias=p.bias+p.memory*0.12f*mem;
        x=asymSat(x*p.drive,dynamicBias,p.asymmetry); return postLp.process(x)*p.output;
    }
};

struct AmpEngineHQ::Channel
{
    std::array<TubeStage,20> st;
    Biquad bass,mid,treble;
    Slew sagEnv,biasEnv;
    float nfbHistory=0.0f;
};

static AmpHQParams defaults()
{
    AmpHQParams p;
    for(auto& s:p.stage)s={20,18000,1,0,0,0,18000,1};
    p.stage[0]={18,19000,1,0,0,0,19000,1};
    p.stage[1]={35,15000,1,0,0,0,15000,1};
    p.stage[2]={70,12500,1,0,0,0,12500,1};
    p.stage[3]={55,12500,2.4f,0.025f,0.30f,0.12f,8200,0.92f};
    p.stage[4]={90,11500,1,0,0,0,11000,1};
    p.stage[5]={75,10500,3.3f,0.040f,0.20f,0.18f,6800,0.72f};
    p.stage[6]={120,9800,1,0,0,0,9500,1};
    p.stage[7]={110,9000,4.0f,0.080f,0.75f,0.20f,5900,0.60f};
    p.stage[8]={55,14500,1.18f,0.012f,0.12f,0.08f,11000,0.98f};
    p.stage[9]={45,16000,1,0,0,0,15000,1};
    p.stage[10]={30,18000,1,0,0,0,18000,1};
    p.stage[11]={55,12000,1.55f,0.018f,0.22f,0.14f,9000,0.90f};
    p.stage[12]={30,18000,1,0,0,0,18000,0.72f};
    p.stage[13]={70,12500,1,0,0,0,12000,1};
    p.stage[14]={65,12000,1.55f,0.015f,0.35f,0.18f,9000,0.88f};
    p.stage[15]={20,18000,1,0,0,0,18000,1};
    p.stage[16]={85,11000,1,0,0,0,10500,1};
    p.stage[17]={55,10500,2.15f,0.032f,0.28f,0.22f,6500,0.82f};
    p.stage[18]={20,18000,1,0,0,0,18000,1};
    p.stage[19]={35,9000,1.15f,0.005f,0.10f,0.04f,7000,0.92f};
    return p;
}

AmpEngineHQ::AmpEngineHQ():params(defaults()),oversampling(2){}
AmpEngineHQ::~AmpEngineHQ() = default;

void AmpEngineHQ::prepare(double sampleRate,int maxBlockSize)
{
    fs=sampleRate;maxBlock=maxBlockSize;oversampling.prepare(fs,maxBlock);work.setSize(1,maxBlock);
    for(auto& cp:channels)
    {
        cp=std::make_unique<Channel>();
        for(auto& s:cp->st)s.prepare(fs*4.0);
        cp->sagEnv.prepare(fs*4.0,params.sagRecoveryMs);cp->biasEnv.prepare(fs*4.0,25.0f);
    }
    updateFilters();setParameters(params);reset();
}

void AmpEngineHQ::reset()
{
    oversampling.reset();
    for(auto& cp:channels)if(cp){for(auto&s:cp->st){s.hp.reset();s.preLp.reset();s.postLp.reset();s.memory.reset();}cp->bass.reset();cp->mid.reset();cp->treble.reset();cp->sagEnv.reset();cp->biasEnv.reset();cp->nfbHistory=0;}
}

void AmpEngineHQ::setParameters(const AmpHQParams& p)
{
    params=p;for(auto& cp:channels)if(cp)for(size_t i=0;i<20;++i)cp->st[i].set(params.stage[i]);updateFilters();
}

void AmpEngineHQ::updateFilters()
{
    for(auto& cp:channels)if(cp)
    {
        cp->bass.setPeak(fs*4.0,110.0f,0.70f,params.bassDb);
        cp->mid.setPeak(fs*4.0,720.0f,0.85f,params.midDb);
        cp->treble.setPeak(fs*4.0,3200.0f,0.70f,params.trebleDb);
    }
}

float AmpEngineHQ::processChannel(Channel& c,float x)
{
    for(int i=0;i<=9;++i)x=c.st[(size_t)i].process(x);
    x=c.treble.process(c.mid.process(c.bass.process(x)));
    x=c.st[10].process(x);
    x=c.st[11].process(x);
    x=c.st[12].process(x);
    x=c.st[13].process(x);
    x=c.st[14].process(x);
    const float demand=c.sagEnv.process(std::abs(x));
    const float sagGain=juce::jlimit(0.42f,1.0f,1.0f-params.sag*0.50f*demand);
    x*=sagGain;
    x=c.st[16].process(x);
    const float nfb=juce::jlimit(0.0f,0.82f,params.damping*0.62f);
    const float presenceTilt=lerp(0.70f,1.18f,params.presence);
    x-=c.nfbHistory*nfb*presenceTilt;
    const float be=c.biasEnv.process(std::abs(x));
    auto pp=params.stage[17];pp.bias+=params.biasExcursion*0.10f*be;c.st[17].set(pp);
    x=c.st[17].process(x);
    auto tp=params.stage[19];tp.drive*=1.0f+params.transformerSaturation*1.6f;c.st[19].set(tp);
    x=c.st[19].process(x);
    c.nfbHistory=x;
    return x*dbToGain(params.outputDb);
}

void AmpEngineHQ::process(juce::AudioBuffer<float>& mono)
{
    const int n=mono.getNumSamples();if(n<=0)return;work.setSize(1,n,false,false,true);work.copyFrom(0,0,mono,0,0,n);
    oversampling.process(work,[this](float x){return processChannel(*channels[0],x);});mono.copyFrom(0,0,work,0,0,n);
}
}
