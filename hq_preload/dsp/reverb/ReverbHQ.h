#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>
#include "../common/HQDSP.h"

namespace guitardsp::hq
{
enum class ReverbType { room, plate, hall, spring };
class ReverbHQ
{
public:
    struct Params { ReverbType type=ReverbType::room; float size=0.5f, decay=0.5f, damping=0.45f, preDelayMs=12.0f, mod=0.12f, dwell=0.45f, drip=0.35f, mix=0.22f; };
    void prepare(double fs)
    {
        sampleRate=fs; const std::array<float,8> ms{29.7f,37.1f,41.1f,43.7f,53.1f,61.7f,71.9f,79.3f};
        for(size_t i=0;i<lines.size();++i){lines[i].assign((size_t)(0.001f*ms[i]*(float)fs*1.8f)+8,0);idx[i]=0;damp[i]=0;}
        pre.assign((size_t)(0.25*fs)+8,0);preIdx=0;
    }
    void setParameters(const Params&p){params=p;}
    float process(float x)
    {
        const int pd=juce::jlimit(0,(int)pre.size()-1,(int)(0.001f*params.preDelayMs*(float)sampleRate)); const size_t read=(preIdx+pre.size()-(size_t)pd)%pre.size(); const float input=pre[read]; pre[preIdx]=x;preIdx=(preIdx+1)%pre.size();
        std::array<float,8> y{}; float sum=0;
        for(size_t i=0;i<8;++i){y[i]=lines[i][idx[i]];sum+=y[i];}
        const float mean=sum/8.0f; float out=0;
        const float baseFb=lerp(0.58f,0.91f,params.decay);
        for(size_t i=0;i<8;++i)
        {
            float v=input*0.32f+(2.0f*mean-y[i])*baseFb;
            damp[i]=lerp(v,damp[i],lerp(0.05f,0.82f,params.damping)); v=damp[i];
            if(params.type==ReverbType::plate)v=softSat(v*1.08f);
            if(params.type==ReverbType::spring){const float drip=std::sin((float)(phase*(1.0+i*0.071)))*params.drip*0.018f;v+=drip*std::abs(input)*6.0f;v=asymSat(v,0.01f,0.3f);}
            lines[i][idx[i]]=v;idx[i]=(idx[i]+1)%lines[i].size();out+=y[i]*(i&1?-1.0f:1.0f);
        }
        phase+=0.019f+params.mod*0.003f;if(phase>100000)phase=0;out*=0.18f;return lerp(x,out,params.mix);
    }
private:
    Params params;double sampleRate=48000;std::array<std::vector<float>,8>lines;std::array<size_t,8>idx{};std::array<float,8>damp{};std::vector<float>pre;size_t preIdx=0;double phase=0;
};
}
