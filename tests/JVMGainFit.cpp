#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"

namespace
{
constexpr int scoreSamples=4096, prerollSamples=2048, fineLag=384;
struct Clip { bool fit=false; std::string name; float bass=.5f,middle=.5f,treble=.5f,gain=.5f; double sampleRate=48000.0; std::vector<float> input,target; int scoreStart=0; };
struct Candidate { float gainSpanDb=24.0f; float gainCurve=1.25f; };
struct Metrics { double objective=std::numeric_limits<double>::infinity(), nmseDb=0.0, correlation=0.0, globalGain=1.0; };

bool loadMono(juce::AudioFormatManager& formats,const juce::File& file,std::vector<float>& samples,double& sampleRate)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if(!reader||reader->lengthInSamples<=0||reader->numChannels==0)return false;
    const auto cappedLength=std::min<juce::int64>(reader->lengthInSamples,48000LL*120LL);
    if(cappedLength<scoreSamples+prerollSamples+fineLag)return false;
    const int n=(int)cappedLength; juce::AudioBuffer<float> b((int)reader->numChannels,n);
    if(!reader->read(&b,0,n,0,true,true))return false;
    samples.resize((size_t)n);
    if(b.getNumChannels()==1)std::copy(b.getReadPointer(0),b.getReadPointer(0)+n,samples.begin());
    else for(int i=0;i<n;++i)samples[(size_t)i]=0.5f*(b.getSample(0,i)+b.getSample(1,i));
    sampleRate=reader->sampleRate;return true;
}

int chooseScoreStart(const std::vector<float>& input,const std::vector<float>& target)
{
    const int maxStart=juce::jmin((int)input.size(),(int)target.size())-scoreSamples-fineLag;
    if(maxStart<=prerollSamples)return juce::jmax(0,maxStart/2);
    double best=-1.0;int bestStart=prerollSamples;
    for(int start=prerollSamples;start<=maxStart;start+=1024)
    {
        double e=0.0;for(int i=0;i<scoreSamples;++i){const double x=input[(size_t)(start+i)];e+=x*x;}
        if(e>best){best=e;bestStart=start;}
    }
    return bestStart;
}

bool loadManifest(const juce::File& manifest,std::vector<Clip>& clips)
{
    std::ifstream in(manifest.getFullPathName().toStdString());if(!in)return false;
    juce::AudioFormatManager formats;formats.registerBasicFormats();const auto base=manifest.getParentDirectory();std::string line;
    while(std::getline(in,line))
    {
        if(line.empty()||line[0]=='#')continue;std::istringstream row(line);std::string split,inPath,outPath;float b10=5,m10=5,t10=5,g10=5;Clip c;
        if(!(row>>split>>c.name>>b10>>m10>>t10>>g10>>inPath>>outPath))return false;
        c.fit=split=="fit";if(!c.fit&&split!="holdout")return false;
        c.bass=juce::jlimit(0.0f,1.0f,b10/10.0f);c.middle=juce::jlimit(0.0f,1.0f,m10/10.0f);c.treble=juce::jlimit(0.0f,1.0f,t10/10.0f);c.gain=juce::jlimit(0.0f,1.0f,g10/10.0f);
        double ir=0,tr=0;if(!loadMono(formats,base.getChildFile(inPath),c.input,ir)||!loadMono(formats,base.getChildFile(outPath),c.target,tr)||std::abs(ir-tr)>0.5)return false;
        c.sampleRate=ir;c.scoreStart=chooseScoreStart(c.input,c.target);clips.push_back(std::move(c));
    }
    const int fitCount=(int)std::count_if(clips.begin(),clips.end(),[](const Clip& c){return c.fit;});const int holdCount=(int)clips.size()-fitCount;
    std::cout<<"Loaded "<<fitCount<<" fit clips and "<<holdCount<<" holdout clips\n";return fitCount>=3&&holdCount>=1;
}

guitardsp::hq::AmpHQParams paramsFor(const Clip& c,const Candidate& k)
{
    auto p=guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(c.bass,c.middle,c.treble,0.5f);
    const float x=std::pow(c.gain,k.gainCurve),x5=std::pow(0.5f,k.gainCurve),deltaDb=k.gainSpanDb*(x-x5);
    p.stage[2].drive=juce::jlimit(0.15f,12.0f,p.stage[2].drive*guitardsp::hq::dbToGain(deltaDb*0.68f));
    p.stage[4].drive=juce::jlimit(0.15f,12.0f,p.stage[4].drive*guitardsp::hq::dbToGain(deltaDb*0.32f));return p;
}

struct Rendered{std::vector<float> model,target;};
Rendered render(guitardsp::hq::AmpEngineHQ& amp,const Clip& c,const Candidate& k)
{
    const int segmentStart=juce::jmax(0,c.scoreStart-prerollSamples),scoreOffset=c.scoreStart-segmentStart;
    const int available=juce::jmin((int)c.input.size(),(int)c.target.size())-segmentStart;
    const int segmentLength=juce::jmin(available,prerollSamples+scoreSamples+fineLag);
    if(segmentLength<=scoreOffset+scoreSamples)return {};
    juce::AudioBuffer<float> b(1,segmentLength);for(int i=0;i<segmentLength;++i)b.setSample(0,i,c.input[(size_t)(segmentStart+i)]);
    amp.setParameters(paramsFor(c,k));amp.reset();amp.process(b);
    int bestLag=0;float bestAbs=-1.0f;
    for(int lag=-fineLag;lag<=fineLag;++lag)
    {
        const int targetStart=c.scoreStart+lag;if(targetStart<0||targetStart+scoreSamples>(int)c.target.size())continue;
        double dot=0,em=0,et=0;for(int i=0;i<scoreSamples;++i){const double m=b.getSample(0,scoreOffset+i),t=c.target[(size_t)(targetStart+i)];dot+=m*t;em+=m*m;et+=t*t;}
        const float corr=(float)(dot/std::sqrt(juce::jmax(1.0e-24,em*et)));if(std::abs(corr)>bestAbs){bestAbs=std::abs(corr);bestLag=lag;}
    }
    Rendered r;r.model.resize(scoreSamples);r.target.resize(scoreSamples);const int targetStart=c.scoreStart+bestLag;
    for(int i=0;i<scoreSamples;++i){r.model[(size_t)i]=b.getSample(0,scoreOffset+i);r.target[(size_t)i]=c.target[(size_t)(targetStart+i)];}return r;
}

Metrics evaluate(const std::vector<Clip>& clips,bool fitSplit,const Candidate& k,guitardsp::hq::AmpEngineHQ& amp)
{
    std::vector<Rendered> rs;for(const auto& c:clips)if(c.fit==fitSplit){auto r=render(amp,c,k);if(!r.model.empty())rs.push_back(std::move(r));}
    if(rs.empty())return {};
    double dot=0,em=0;for(const auto& r:rs)for(size_t i=0;i<r.model.size();++i){dot+=(double)r.model[i]*r.target[i];em+=(double)r.model[i]*r.model[i];}
    double globalGain=em>1e-20?dot/em:1.0;globalGain=juce::jlimit(-100.0,100.0,globalGain);
    double err=0,et=0,corr=0;int nc=0;
    for(const auto& r:rs)
    {
        double cd=0,cm=0,ct=0;for(size_t i=0;i<r.model.size();++i){const double m=globalGain*r.model[i],t=r.target[i],e=m-t;err+=e*e;et+=t*t;cd+=m*t;cm+=m*m;ct+=t*t;}
        corr+=cd/std::sqrt(juce::jmax(1.0e-24,cm*ct));++nc;
    }
    Metrics m;const double nmse=err/juce::jmax(1.0e-20,et);m.nmseDb=10.0*std::log10(juce::jmax(1e-12,nmse));m.correlation=nc?corr/nc:0.0;m.globalGain=globalGain;m.objective=std::sqrt(nmse)+0.20*(1.0-m.correlation);return m;
}

void print(const char* tag,const Candidate& c,const Metrics& m)
{
    std::cout<<std::fixed<<std::setprecision(6)<<tag<<" objective="<<m.objective<<" nmseDb="<<m.nmseDb<<" correlation="<<m.correlation<<" globalGain="<<m.globalGain<<" gainSpanDb="<<c.gainSpanDb<<" gainCurve="<<c.gainCurve<<'\n';
}

Candidate fit(const std::vector<Clip>& clips,guitardsp::hq::AmpEngineHQ& amp,Metrics& bestM)
{
    Candidate best;bestM=evaluate(clips,true,best,amp);print("GAIN_START",best,bestM);float spanStep=8.0f,curveStep=0.45f;
    for(int pass=0;pass<4;++pass)
    {
        for(int p=0;p<2;++p)
        {
            const Candidate base=best;for(float dir:{-1.0f,1.0f})
            {
                Candidate trial=base;if(p==0)trial.gainSpanDb=juce::jlimit(6.0f,48.0f,base.gainSpanDb+dir*spanStep);else trial.gainCurve=juce::jlimit(0.35f,3.0f,base.gainCurve+dir*curveStep);
                const auto m=evaluate(clips,true,trial,amp);if(m.objective<bestM.objective){best=trial;bestM=m;print("GAIN_IMPROVED",best,bestM);}
            }
        }
        spanStep*=0.45f;curveStep*=0.45f;
    }
    return best;
}

void report(const juce::File& f,const Candidate& c,const Metrics& fitM,const Metrics& holdM)
{
    std::ofstream out(f.getFullPathName().toStdString(),std::ios::trunc);
    out<<std::fixed<<std::setprecision(8)<<"{\n  \"target\": \"Marshall JVM410H OD1 measured Gain axis\",\n"
       <<"  \"measuredAxes\": [\"bass\", \"middle\", \"treble\", \"gain\"],\n"
       <<"  \"fixedInPublicData\": [\"channel=OD1\", \"channelVolume=10\", \"master=5\", \"presence=5\", \"resonance=5\"],\n"
       <<"  \"gainSpanDb\": "<<c.gainSpanDb<<",\n  \"gainCurve\": "<<c.gainCurve<<",\n"
       <<"  \"fit\": {\"objective\": "<<fitM.objective<<", \"nmseDb\": "<<fitM.nmseDb<<", \"correlation\": "<<fitM.correlation<<"},\n"
       <<"  \"holdout\": {\"objective\": "<<holdM.objective<<", \"nmseDb\": "<<holdM.nmseDb<<", \"correlation\": "<<holdM.correlation<<"}\n}\n";
}
}

int main(int argc,char** argv)
{
    if(argc<3){std::cerr<<"usage: GuitarDSPJVMGainFit manifest.tsv report.json\n";return 2;}
    std::vector<Clip> clips;if(!loadManifest(juce::File(argv[1]),clips)){std::cerr<<"Could not load gain manifest\n";return 3;}
    guitardsp::hq::AmpEngineHQ amp(4);amp.prepare(clips.front().sampleRate,8192);Metrics fitM;const auto best=fit(clips,amp,fitM);const auto holdM=evaluate(clips,false,best,amp);
    print("GAIN_FINAL_FIT",best,fitM);print("GAIN_HOLDOUT",best,holdM);report(juce::File(argv[2]),best,fitM,holdM);
    if(!std::isfinite(fitM.objective)||!std::isfinite(holdM.objective)||holdM.correlation<0.05)return 4;return 0;
}
