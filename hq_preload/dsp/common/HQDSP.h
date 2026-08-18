#pragma once
#include <JuceHeader.h>
#include <cmath>
#include <array>

namespace guitardsp::hq
{
inline float dbToGain(float db) noexcept { return std::pow(10.0f, db / 20.0f); }
inline float lerp(float a, float b, float t) noexcept { return a + (b-a)*t; }
inline float softSat(float x) noexcept { return std::tanh(x); }
inline float asymSat(float x, float bias, float asymmetry) noexcept
{
    const float y = x + bias;
    const float pos = std::tanh(y * (1.0f + 0.75f * asymmetry));
    const float neg = std::tanh(y * (1.0f - 0.45f * asymmetry));
    const float s = y >= 0.0f ? pos : neg;
    return s - std::tanh(bias);
}

struct OnePoleHP
{
    void prepare(double fs) { sampleRate = fs; setHz(hz); reset(); }
    void setHz(float f)
    {
        hz = juce::jlimit(1.0f, 0.45f*(float)sampleRate, f);
        const float a = std::exp(-2.0f * juce::MathConstants<float>::pi * hz / (float)sampleRate);
        coeff = a;
    }
    float process(float x) noexcept
    {
        const float y = coeff * (y1 + x - x1);
        x1=x; y1=y; return y;
    }
    void reset() noexcept { x1=y1=0.0f; }
    double sampleRate=48000.0; float hz=20.0f, coeff=0.0f, x1=0.0f, y1=0.0f;
};

struct OnePoleLP
{
    void prepare(double fs) { sampleRate=fs; setHz(hz); reset(); }
    void setHz(float f)
    {
        hz=juce::jlimit(1.0f,0.45f*(float)sampleRate,f);
        coeff=std::exp(-2.0f*juce::MathConstants<float>::pi*hz/(float)sampleRate);
    }
    float process(float x) noexcept { y=(1.0f-coeff)*x+coeff*y; return y; }
    void reset() noexcept { y=0.0f; }
    double sampleRate=48000.0; float hz=20000.0f, coeff=0.0f, y=0.0f;
};

struct Biquad
{
    void reset() noexcept { z1=z2=0.0f; }
    float process(float x) noexcept
    {
        const float y=b0*x+z1;
        z1=b1*x-a1*y+z2;
        z2=b2*x-a2*y;
        return y;
    }
    void setPeak(double fs,float hz,float q,float gainDb)
    {
        const float A=std::pow(10.0f,gainDb/40.0f);
        const float w=2.0f*juce::MathConstants<float>::pi*hz/(float)fs;
        const float alpha=std::sin(w)/(2.0f*q), c=std::cos(w);
        const float aa0=1.0f+alpha/A;
        b0=(1.0f+alpha*A)/aa0; b1=(-2.0f*c)/aa0; b2=(1.0f-alpha*A)/aa0;
        a1=(-2.0f*c)/aa0; a2=(1.0f-alpha/A)/aa0;
    }
    float b0=1,b1=0,b2=0,a1=0,a2=0,z1=0,z2=0;
};

class NonlinearOversampler
{
public:
    explicit NonlinearOversampler(int order=2)
        : factor(order), os(1, order, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false) {}

    void prepare(double fs, int maxBlock)
    {
        sampleRate=fs; os.reset(); os.initProcessing((size_t)maxBlock);
    }
    void reset() { os.reset(); }
    int getFactor() const noexcept { return 1 << factor; }

    template<class Fn>
    void process(juce::AudioBuffer<float>& mono, Fn&& fn)
    {
        juce::dsp::AudioBlock<float> b(mono);
        auto up=os.processSamplesUp(b);
        auto* x=up.getChannelPointer(0);
        for(size_t i=0;i<up.getNumSamples();++i) x[i]=fn(x[i]);
        os.processSamplesDown(b);
    }
private:
    int factor=2; double sampleRate=48000.0; juce::dsp::Oversampling<float> os;
};

class Slew
{
public:
    void prepare(double fs,float ms) { a=std::exp(-1.0f/(0.001f*ms*(float)fs)); }
    float process(float x) noexcept { y=a*y+(1.0f-a)*x; return y; }
    void reset(float v=0) noexcept { y=v; }
private: float a=0.99f,y=0;
};
}
