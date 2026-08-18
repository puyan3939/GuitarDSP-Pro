#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
class FractionalModDelay
{
public:
    FractionalModDelay(): delay(8192) {}
    void prepare(double fs,int maxBlock)
    {
        sampleRate=fs; juce::dsp::ProcessSpec s{fs,(juce::uint32)maxBlock,1}; delay.prepare(s); delay.reset();
    }
    void reset(){delay.reset();}
    float process(float x,float delayMs,float feedback)
    {
        delay.setDelay(juce::jlimit(0.5f,8000.0f,delayMs*0.001f*(float)sampleRate));
        const float y=delay.popSample(0); delay.pushSample(0,x+y*feedback); return y;
    }
private: double sampleRate=48000.0; juce::dsp::DelayLine<float,juce::dsp::DelayLineInterpolationTypes::Lagrange3rd> delay;
};

class ChorusHQ
{
public:
    struct Params { float rateHz=0.8f, depthMs=4.5f, centreMs=11.0f, feedback=0.05f, mix=0.35f; };
    void prepare(double fs,int maxBlock) { sampleRate=fs; d.prepare(fs,maxBlock); reset(); }
    void reset(){phase=0.0f;d.reset();}
    void setParameters(const Params&p){params=p;}
    float process(float x)
    {
        const float lfo=0.5f+0.5f*std::sin(phase); phase+=2*juce::MathConstants<float>::pi*params.rateHz/(float)sampleRate; if(phase>6.283185f)phase-=6.283185f;
        const float wet=d.process(x,params.centreMs+(lfo-0.5f)*2.0f*params.depthMs,params.feedback);
        return lerp(x,wet,params.mix);
    }
private: Params params; double sampleRate=48000; float phase=0; FractionalModDelay d;
};

class FlangerHQ
{
public:
    struct Params { float rateHz=0.25f, depthMs=1.8f, manualMs=2.2f, feedback=0.55f, mix=0.5f; };
    void prepare(double fs,int maxBlock){sampleRate=fs;d.prepare(fs,maxBlock);reset();}
    void reset(){phase=0.0f;d.reset();}
    void setParameters(const Params&p){params=p;}
    float process(float x)
    {
        const float l=std::sin(phase); phase+=2*juce::MathConstants<float>::pi*params.rateHz/(float)sampleRate;if(phase>6.283185f)phase-=6.283185f;
        return lerp(x,d.process(x,params.manualMs+params.depthMs*l,juce::jlimit(-0.92f,0.92f,params.feedback)),params.mix);
    }
private: Params params;double sampleRate=48000;float phase=0;FractionalModDelay d;
};

class PhaserHQ
{
public:
    struct Params { float rateHz=0.45f, depth=0.8f, feedback=0.25f, mix=0.5f; int stages=6; };
    void prepare(double fs){sampleRate=fs;reset();} void setParameters(const Params&p){params=p;}
    float process(float x)
    {
        const float l=0.5f+0.5f*std::sin(phase);phase+=2*juce::MathConstants<float>::pi*params.rateHz/(float)sampleRate;if(phase>6.283185f)phase-=6.283185f;
        const float hz=lerp(180.0f,2100.0f,l*params.depth); const float g=std::tan(juce::MathConstants<float>::pi*hz/(float)sampleRate); const float a=(1.0f-g)/(1.0f+g);
        float y=x+feedbackState*params.feedback; const int n=juce::jlimit(2,8,params.stages);
        for(int i=0;i<n;++i){const float o=-a*y+z[(size_t)i];z[(size_t)i]=y+a*o;y=o;}
        feedbackState=y; return lerp(x,y,params.mix);
    }
    void reset(){phase=0.0f;z.fill(0);feedbackState=0;}
private: Params params;double sampleRate=48000;float phase=0,feedbackState=0;std::array<float,8>z{};
};

class TremoloHQ
{
public:
    struct Params { float rateHz=4.0f, depth=0.55f, mix=1.0f, shape=0.0f; };
    void prepare(double fs){sampleRate=fs;reset();}
    void reset(){phase=0.0f;}
    void setParameters(const Params&p){params=p;}
    float process(float x)
    {
        const float sine=std::sin(phase);
        const float drive=1.0f+10.0f*juce::jlimit(0.0f,1.0f,params.shape);
        const float shaped=std::tanh(sine*drive)/std::tanh(drive);
        const float depth=juce::jlimit(0.0f,1.0f,params.depth);
        const float gain=1.0f-depth*(0.5f+0.5f*shaped);
        advance();
        return lerp(x,x*gain,juce::jlimit(0.0f,1.0f,params.mix));
    }
private:
    void advance(){phase+=2.0f*juce::MathConstants<float>::pi*juce::jlimit(0.05f,20.0f,params.rateHz)/(float)sampleRate;if(phase>2.0f*juce::MathConstants<float>::pi)phase-=2.0f*juce::MathConstants<float>::pi;}
    Params params;double sampleRate=48000.0;float phase=0.0f;
};

class VibratoHQ
{
public:
    struct Params { float rateHz=5.0f, depthMs=2.2f, centreMs=5.0f, mix=1.0f; };
    void prepare(double fs,int maxBlock){sampleRate=fs;d.prepare(fs,maxBlock);reset();}
    void reset(){phase=0.0f;d.reset();}
    void setParameters(const Params&p){params=p;}
    float process(float x)
    {
        const float lfo=std::sin(phase);
        const float depth=juce::jlimit(0.0f,juce::jmax(0.0f,params.centreMs-0.55f),params.depthMs);
        const float wet=d.process(x,params.centreMs+depth*lfo,0.0f);
        phase+=2.0f*juce::MathConstants<float>::pi*juce::jlimit(0.05f,15.0f,params.rateHz)/(float)sampleRate;
        if(phase>2.0f*juce::MathConstants<float>::pi)phase-=2.0f*juce::MathConstants<float>::pi;
        return lerp(x,wet,juce::jlimit(0.0f,1.0f,params.mix));
    }
private: Params params;double sampleRate=48000.0;float phase=0.0f;FractionalModDelay d;
};
}
