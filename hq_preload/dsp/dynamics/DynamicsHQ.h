#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
class EnvelopeFollower
{
public:
    void prepare(double fs) { sampleRate = fs; }
    float process(float x, float attackMs, float releaseMs)
    {
        const float target = std::abs(x);
        const float ms = target > env ? attackMs : releaseMs;
        const float a = std::exp(-1.0f / (0.001f * juce::jmax(0.05f, ms) * (float) sampleRate));
        env = a * env + (1.0f-a) * target;
        return env;
    }
private: double sampleRate=48000.0; float env=0.0f;
};

class StudioCompressor
{
public:
    struct Params { float thresholdDb=-18.0f, ratio=4.0f, attackMs=15.0f, releaseMs=120.0f, kneeDb=6.0f, makeupDb=0.0f, mix=1.0f; };
    void prepare(double fs) { follower.prepare(fs); }
    void setParameters(const Params& p) { params=p; }
    float process(float x)
    {
        const float e = juce::jmax(1.0e-8f, follower.process(x, params.attackMs, params.releaseMs));
        const float inDb = 20.0f*std::log10(e);
        const float over = inDb-params.thresholdDb;
        float gr=0.0f;
        if (over > -0.5f*params.kneeDb)
        {
            const float slope=1.0f-1.0f/juce::jmax(1.0f,params.ratio);
            if (over >= 0.5f*params.kneeDb) gr=-over*slope;
            else
            {
                const float z=over+0.5f*params.kneeDb;
                gr=-slope*z*z/(2.0f*juce::jmax(0.001f,params.kneeDb));
            }
        }
        const float wet=x*dbToGain(gr+params.makeupDb);
        return lerp(x,wet,juce::jlimit(0.0f,1.0f,params.mix));
    }
private: Params params; EnvelopeFollower follower;
};

class GuitarCompressor
{
public:
    struct Params { float sustain=0.55f, attack=0.35f, blend=0.8f, levelDb=0.0f; };
    void prepare(double fs) { comp.prepare(fs); }
    void setParameters(const Params& p)
    {
        params=p;
        StudioCompressor::Params q;
        q.thresholdDb=lerp(-10.0f,-36.0f,p.sustain); q.ratio=lerp(2.0f,8.0f,p.sustain);
        q.attackMs=lerp(1.0f,45.0f,p.attack); q.releaseMs=120.0f; q.kneeDb=9.0f; q.makeupDb=lerp(1.0f,10.0f,p.sustain); q.mix=p.blend;
        comp.setParameters(q);
    }
    float process(float x) { return comp.process(x)*dbToGain(params.levelDb); }
private: Params params; StudioCompressor comp;
};

class NoiseGate
{
public:
    struct Params { float thresholdDb=-55.0f, attackMs=2.0f, holdMs=20.0f, releaseMs=80.0f; };
    void prepare(double fs) { sampleRate=fs; follower.prepare(fs); }
    void setParameters(const Params& p) { params=p; }
    float process(float x)
    {
        const float env=follower.process(x, params.attackMs, params.releaseMs);
        const bool above=20.0f*std::log10(juce::jmax(1.0e-8f,env)) >= params.thresholdDb;
        if (above) { hold=(int)(0.001*params.holdMs*sampleRate); target=1.0f; }
        else if (hold>0) --hold; else target=0.0f;
        const float ms=target>gain?params.attackMs:params.releaseMs;
        const float a=std::exp(-1.0f/(0.001f*juce::jmax(0.1f,ms)*(float)sampleRate));
        gain=a*gain+(1.0f-a)*target;
        return x*gain;
    }
private: Params params; EnvelopeFollower follower; double sampleRate=48000; int hold=0; float gain=1,target=1;
};
}
