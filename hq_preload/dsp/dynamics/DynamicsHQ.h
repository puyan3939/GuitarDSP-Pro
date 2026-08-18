#pragma once
#include <JuceHeader.h>
#include <cmath>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
class EnvelopeFollower
{
public:
    void prepare(double fs) { sampleRate = fs; reset(); }
    void reset() noexcept { env=0.0f; }
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

// Guitar-oriented downward expander / precision gate.
// The detector can be keyed from the clean input while attenuation is applied to a
// later signal (for example after high-gain pedals). This prevents pedal-generated
// hiss from holding the gate open.
class NoiseGate
{
public:
    struct Params
    {
        float thresholdDb=-55.0f;
        float rangeDb=-60.0f;
        float ratio=4.0f;
        float attackMs=1.0f;
        float holdMs=35.0f;
        float releaseMs=180.0f;
        float hysteresisDb=4.0f;
        float sidechainHpHz=55.0f;
        float sidechainLpHz=6500.0f;
    };

    void prepare(double fs)
    {
        sampleRate=fs;
        follower.prepare(fs);
        detectorHP.prepare(fs);
        detectorLP.prepare(fs);
        updateFilters();
        reset();
    }

    void reset() noexcept
    {
        follower.reset(); detectorHP.reset(); detectorLP.reset();
        holdSamples=0; gainDb=0.0f; gateOpen=true;
    }

    void setParameters(const Params& p)
    {
        params=p;
        params.thresholdDb=juce::jlimit(-100.0f,-5.0f,params.thresholdDb);
        params.rangeDb=juce::jlimit(-90.0f,0.0f,params.rangeDb);
        params.ratio=juce::jlimit(1.0f,20.0f,params.ratio);
        params.attackMs=juce::jlimit(0.1f,50.0f,params.attackMs);
        params.holdMs=juce::jlimit(0.0f,1000.0f,params.holdMs);
        params.releaseMs=juce::jlimit(5.0f,2000.0f,params.releaseMs);
        params.hysteresisDb=juce::jlimit(0.0f,18.0f,params.hysteresisDb);
        params.sidechainHpHz=juce::jlimit(10.0f,1000.0f,params.sidechainHpHz);
        params.sidechainLpHz=juce::jlimit(800.0f,18000.0f,params.sidechainLpHz);
        if(params.sidechainLpHz < params.sidechainHpHz*1.5f)
            params.sidechainLpHz=params.sidechainHpHz*1.5f;
        updateFilters();
    }

    float process(float x) { return processKeyed(x, x); }

    float processKeyed(float audio, float detectorInput)
    {
        float detector=detectorHP.process(detectorInput);
        detector=detectorLP.process(detector);
        const float env=follower.process(detector, juce::jmax(0.2f,params.attackMs*0.6f), juce::jmax(10.0f,params.releaseMs*0.55f));
        const float envDb=20.0f*std::log10(juce::jmax(1.0e-9f,env));

        const float openThreshold=params.thresholdDb+0.5f*params.hysteresisDb;
        const float closeThreshold=params.thresholdDb-0.5f*params.hysteresisDb;

        if(!gateOpen && envDb>=openThreshold)
        {
            gateOpen=true;
            holdSamples=(int)std::round(0.001*params.holdMs*sampleRate);
        }
        else if(gateOpen)
        {
            if(envDb>=closeThreshold)
                holdSamples=(int)std::round(0.001*params.holdMs*sampleRate);
            else if(holdSamples>0)
                --holdSamples;
            else
                gateOpen=false;
        }

        float targetDb=0.0f;
        if(!gateOpen)
        {
            const float below=juce::jmin(0.0f,envDb-params.thresholdDb);
            targetDb=juce::jmax(params.rangeDb, below*(params.ratio-1.0f));
        }

        const float timeMs=targetDb>gainDb ? params.attackMs : params.releaseMs;
        const float a=std::exp(-1.0f/(0.001f*juce::jmax(0.1f,timeMs)*(float)sampleRate));
        gainDb=a*gainDb+(1.0f-a)*targetDb;
        return audio*dbToGain(gainDb);
    }

    float getGainReductionDb() const noexcept { return gainDb; }
    bool isOpen() const noexcept { return gateOpen; }

private:
    void updateFilters()
    {
        detectorHP.setHz(params.sidechainHpHz);
        detectorLP.setHz(params.sidechainLpHz);
    }

    Params params;
    EnvelopeFollower follower;
    OnePoleHP detectorHP;
    OnePoleLP detectorLP;
    double sampleRate=48000.0;
    int holdSamples=0;
    float gainDb=0.0f;
    bool gateOpen=true;
};
}
