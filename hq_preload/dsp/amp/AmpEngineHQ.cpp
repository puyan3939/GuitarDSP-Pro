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
    Slew sagEnv,biasEnv,transformerFlux;
    OnePoleLP nfbLow;
    float nfbHistory=0.0f;
    float piMemory=0.0f;
};

static AmpHQParams defaults()
{
    AmpHQParams p;
    for(auto& s:p.stage)s={20,18000,1,0,0,0,18000,1};
    p.stage[0]={18,19000,1,0,0,0,19000,1}; p.stage[1]={35,15000,1,0,0,0,15000,1}; p.stage[2]={70,12500,1,0,0,0,12500,1};
    p.stage[3]={55,12500,2.4f,0.025f,0.30f,0.12f,8200,0.92f}; p.stage[4]={90,11500,1,0,0,0,11000,1};
    p.stage[5]={75,10500,3.3f,0.040f,0.20f,0.18f,6800,0.72f}; p.stage[6]={120,9800,1,0,0,0,9500,1};
    p.stage[7]={110,9000,4.0f,0.080f,0.75f,0.20f,5900,0.60f}; p.stage[8]={55,14500,1.18f,0.012f,0.12f,0.08f,11000,0.98f};
    p.stage[9]={45,16000,1,0,0,0,15000,1}; p.stage[10]={30,18000,1,0,0,0,18000,1}; p.stage[11]={55,12000,1.55f,0.018f,0.22f,0.14f,9000,0.90f};
    p.stage[12]={30,18000,1,0,0,0,18000,0.72f}; p.stage[13]={70,12500,1,0,0,0,12000,1}; p.stage[14]={65,12000,1.55f,0.015f,0.35f,0.18f,9000,0.88f};
    p.stage[15]={20,18000,1,0,0,0,18000,1}; p.stage[16]={85,11000,1,0,0,0,10500,1}; p.stage[17]={55,10500,2.15f,0.032f,0.28f,0.22f,6500,0.82f};
    p.stage[18]={20,18000,1,0,0,0,18000,1}; p.stage[19]={35,9000,1.15f,0.005f,0.10f,0.04f,7000,0.92f};
    return p;
}

AmpHQParams AmpEngineHQ::makeBassman5F6AReference()
{
    AmpHQParams p = defaults();
    for (auto& s : p.stage)
        s = { 20.0f, 18000.0f, 0.25f, 0.0f, 0.0f, 0.0f, 18000.0f, 4.0f };

    p.stage[0] = { 32.0f, 14500.0f, 0.82f, 0.006f, 0.12f, 0.045f, 11200.0f, 1.70f };
    p.stage[1] = { 48.0f, 16000.0f, 0.25f, 0.0f, 0.0f, 0.0f, 15500.0f, 4.0f };
    p.stage[2] = { 42.0f, 13200.0f, 1.32f, 0.010f, 0.18f, 0.070f, 9800.0f, 1.08f };
    p.stage[3] = { 24.0f, 17000.0f, 0.30f, 0.002f, 0.04f, 0.025f, 16000.0f, 3.22f };

    p.bassDb = 3.4f;
    p.midDb = -8.6f;
    p.trebleDb = 4.2f;
    p.stage[10] = { 28.0f, 17500.0f, 0.25f, 0.0f, 0.0f, 0.0f, 17000.0f, 3.35f };

    p.stage[11] = { 45.0f, 14500.0f, 0.78f, 0.006f, 0.10f, 0.045f, 11800.0f, 1.45f };
    p.stage[12] = { 28.0f, 17500.0f, 0.25f, 0.0f, 0.0f, 0.0f, 17000.0f, 4.0f };
    p.stage[13] = { 32.0f, 16500.0f, 0.25f, 0.0f, 0.0f, 0.0f, 16000.0f, 4.0f };
    p.stage[14] = { 34.0f, 14000.0f, 1.10f, 0.008f, 0.16f, 0.080f, 10800.0f, 1.02f };

    p.stage[16] = { 38.0f, 14500.0f, 0.42f, 0.0f, 0.03f, 0.020f, 13200.0f, 2.35f };
    p.stage[17] = { 48.0f, 11800.0f, 1.78f, 0.018f, 0.22f, 0.16f, 7800.0f, 0.92f };
    p.stage[18] = { 24.0f, 17500.0f, 0.25f, 0.0f, 0.0f, 0.0f, 17000.0f, 4.0f };
    p.stage[19] = { 30.0f, 8200.0f, 0.42f, 0.002f, 0.05f, 0.025f, 7600.0f, 2.20f };

    p.sag = 0.36f;
    p.sagRecoveryMs = 125.0f;
    p.damping = 0.52f;
    p.presence = 0.34f;
    p.biasExcursion = 0.18f;
    p.transformerSaturation = 0.20f;
    p.outputDb = -10.5f;
    return p;
}

AmpHQParams AmpEngineHQ::makeJVM410HOD1Reference(float bass, float middle, float treble)
{
    bass = juce::jlimit(0.0f, 1.0f, bass);
    middle = juce::jlimit(0.0f, 1.0f, middle);
    treble = juce::jlimit(0.0f, 1.0f, treble);

    AmpHQParams p = defaults();
    for (auto& s : p.stage)
        s = { 20.0f, 18000.0f, 0.25f, 0.0f, 0.0f, 0.0f, 18000.0f, 4.0f };

    // OD1/Gain=5 topology-informed starting point. The measured-data fitter
    // estimates compact corrections around these values instead of learning an
    // opaque black-box replacement for the HQ engine.
    p.stage[0] = { 28.0f, 16000.0f, 1.30f, 0.008f, 0.12f, 0.035f, 13500.0f, 1.28f };
    p.stage[2] = { 52.0f, 13200.0f, 2.35f, 0.018f, 0.22f, 0.075f, 10200.0f, 0.94f };
    p.stage[4] = { 78.0f, 11200.0f, 3.10f, 0.032f, 0.30f, 0.110f, 8600.0f, 0.78f };
    p.stage[6] = { 105.0f, 9800.0f, 3.55f, 0.045f, 0.38f, 0.145f, 7200.0f, 0.70f };
    p.stage[8] = { 58.0f, 13200.0f, 1.38f, 0.010f, 0.14f, 0.060f, 10800.0f, 0.94f };

    p.bassDb = -4.0f + 10.5f * bass;
    p.midDb = -7.0f + 11.5f * middle;
    p.trebleDb = -5.0f + 13.0f * treble;

    p.stage[10] = { 28.0f, 16500.0f, 0.30f, 0.0f, 0.02f, 0.020f, 15000.0f, 3.18f };
    p.stage[11] = { 44.0f, 14200.0f, 1.05f, 0.010f, 0.16f, 0.070f, 11200.0f, 1.22f };
    p.stage[14] = { 38.0f, 13200.0f, 1.22f, 0.012f, 0.20f, 0.090f, 10200.0f, 1.02f };

    p.stage[16] = { 36.0f, 14500.0f, 0.42f, 0.0f, 0.04f, 0.025f, 13200.0f, 2.30f };
    p.stage[17] = { 46.0f, 11800.0f, 1.42f, 0.014f, 0.18f, 0.120f, 8300.0f, 0.94f };
    p.stage[19] = { 30.0f, 8800.0f, 0.42f, 0.002f, 0.05f, 0.030f, 8000.0f, 2.18f };

    p.sag = 0.18f;
    p.sagRecoveryMs = 72.0f;
    p.damping = 0.58f;
    p.presence = 0.48f;
    p.biasExcursion = 0.12f;
    p.transformerSaturation = 0.24f;
    p.outputDb = -12.0f;
    return p;
}

// 16x internal processing at 48 kHz = 768 kHz. This is intentionally
// aggressive for a quality A/B test; revert to 8x if the audible benefit
// does not justify the realtime CPU cost.
AmpEngineHQ::AmpEngineHQ():params(defaults()),oversampling(4){}
AmpEngineHQ::~AmpEngineHQ() = default;

void AmpEngineHQ::prepare(double sampleRate,int maxBlockSize)
{
    fs=sampleRate;maxBlock=maxBlockSize;oversampling.prepare(fs,maxBlock);work.setSize(1,maxBlock);
    const double internalFs=oversampling.getInternalSampleRate();
    for(auto& cp:channels)
    {
        cp=std::make_unique<Channel>();
        for(auto& s:cp->st)s.prepare(internalFs);
        cp->sagEnv.prepare(internalFs,params.sagRecoveryMs);
        cp->biasEnv.prepare(internalFs,22.0f);
        cp->transformerFlux.prepare(internalFs,6.0f);
        cp->nfbLow.prepare(internalFs);cp->nfbLow.setHz(1050.0f);
    }
    updateFilters();setParameters(params);reset();
}

void AmpEngineHQ::reset()
{
    oversampling.reset();
    for(auto& cp:channels)if(cp)
    {
        for(auto&s:cp->st){s.hp.reset();s.preLp.reset();s.postLp.reset();s.memory.reset();}
        cp->bass.reset();cp->mid.reset();cp->treble.reset();cp->sagEnv.reset();cp->biasEnv.reset();cp->transformerFlux.reset();cp->nfbLow.reset();
        cp->nfbHistory=0;cp->piMemory=0;
    }
}

void AmpEngineHQ::setParameters(const AmpHQParams& p)
{
    params=p;for(auto& cp:channels)if(cp){for(size_t i=0;i<20;++i)cp->st[i].set(params.stage[i]);cp->sagEnv.prepare(oversampling.getInternalSampleRate(),params.sagRecoveryMs);}updateFilters();
}

void AmpEngineHQ::updateFilters()
{
    const double internalFs=oversampling.getInternalSampleRate();
    for(auto& cp:channels)if(cp)
    {
        cp->bass.setPeak(internalFs,110.0f,0.70f,params.bassDb);
        cp->mid.setPeak(internalFs,720.0f,0.85f,params.midDb);
        cp->treble.setPeak(internalFs,3200.0f,0.70f,params.trebleDb);
    }
}

float AmpEngineHQ::processChannel(Channel& c,float x)
{
    for(int i=0;i<=9;++i)x=c.st[(size_t)i].process(x);
    x=c.treble.process(c.mid.process(c.bass.process(x)));
    x=c.st[10].process(x);x=c.st[11].process(x);x=c.st[12].process(x);x=c.st[13].process(x);

    const float piIn=c.st[14].process(x);
    c.piMemory=0.9965f*c.piMemory+0.0035f*piIn;
    const float piPos=asymSat((piIn+0.055f*c.piMemory)*1.30f,0.010f,0.28f);
    const float piNeg=asymSat((-piIn+0.035f*c.piMemory)*1.18f,-0.014f,0.22f);
    x=0.52f*(piPos-piNeg);

    const float demand=c.sagEnv.process(x*x);
    const float sagDepth=juce::jlimit(0.0f,0.70f,params.sag*0.62f*demand);
    const float supply=1.0f-sagDepth;

    const float lowFeedback=c.nfbLow.process(c.nfbHistory);
    const float highFeedback=c.nfbHistory-lowFeedback;
    const float presenceAmount=juce::jlimit(0.0f,1.0f,params.presence);
    const float feedbackSignal=lowFeedback+(1.0f-0.78f*presenceAmount)*highFeedback;
    const float nfb=juce::jlimit(0.0f,0.82f,params.damping*0.68f);
    x-=feedbackSignal*nfb;

    x=c.st[16].process(x);
    const float biasDemand=c.biasEnv.process(std::abs(x));
    const float excursion=params.biasExcursion*0.16f*biasDemand;
    const float pwrPos=asymSat((x-excursion)*params.stage[17].drive/supply,params.stage[17].bias,0.34f);
    const float pwrNeg=asymSat((-x-excursion)*params.stage[17].drive/(supply*0.97f),-params.stage[17].bias,0.30f);
    x=0.5f*(pwrPos-pwrNeg)*supply*params.stage[17].output;

    const float flux=c.transformerFlux.process(x);
    const float sat=juce::jlimit(0.0f,1.0f,params.transformerSaturation);
    const float magnetic=std::tanh(x*(1.0f+1.9f*sat)+flux*(0.08f+0.22f*sat));
    x=c.st[19].postLp.process(magnetic)*params.stage[19].output;
    c.nfbHistory=x;
    return x*dbToGain(params.outputDb);
}

void AmpEngineHQ::process(juce::AudioBuffer<float>& mono)
{
    const int n=mono.getNumSamples();if(n<=0||!channels[0])return;work.setSize(1,n,false,false,true);work.copyFrom(0,0,mono,0,0,n);
    oversampling.process(work,[this](float x){return processChannel(*channels[0],x);});mono.copyFrom(0,0,work,0,0,n);
}
}
