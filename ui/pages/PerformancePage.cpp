#include "PerformancePage.h"

using namespace guitardsp::hq;

PerformancePage::PerformancePage(AudioEngine& e) : engine(e)
{
    title.setText("PERFORMANCE / MULTI-RIG",juce::dontSendNotification);
    title.setFont(juce::Font(26.0f,juce::Font::bold));
    title.setColour(juce::Label::textColourId,juce::Colours::white);
    addAndMakeVisible(title);

    auto setupTitle=[this](juce::Label& l,const juce::String& text){l.setText(text,juce::dontSendNotification);l.setColour(juce::Label::textColourId,juce::Colour::fromRGB(222,110,58));l.setFont(juce::Font(13.0f,juce::Font::bold));addAndMakeVisible(l);};
    setupTitle(inputTitle,"PICKUP / INPUT LOAD");setupTitle(pitchTitle,"EXPRESSION PITCH / WHAMMY");setupTitle(rigTitle,"MULTI-AMP DETAIL / ALIGNMENT");setupTitle(delayTitle,"DUAL DELAY");setupTitle(sceneTitle,"SCENE / OUTPUT");

    addAndMakeVisible(inputEnabled);
    setupKnob(pickupR,"PICKUP R",1.5,30.0,6.5,0.1," kOhm");
    setupKnob(pickupL,"PICKUP L",0.2,12.0,3.2,0.05," H");
    setupKnob(cableC,"CABLE C",50,3000,470,10," pF");
    setupKnob(inputZ,"INPUT Z",22,10000,1000,10," kOhm");
    setupKnob(inputTrim,"TRIM",-12,12,0,0.1," dB");

    addAndMakeVisible(pitchEnabled);for(auto* b:{&pitchMain,&pitchClean,&pitchSub})addAndMakeVisible(*b);
    setupKnob(pitchSemitones,"SEMITONES",-24,24,12,0.1," st");
    setupKnob(pitchExpression,"EXPR",0,1,0,0.001);
    setupKnob(pitchWet,"WET",0,1,1,0.001);setupKnob(pitchDry,"DRY",0,1,0,0.001);
    setupKnob(pitchTracking,"TRACK",0,1,.55,0.001);setupKnob(pitchTone,"TONE",0,1,.75,0.001);setupKnob(pitchSmooth,"SMOOTH",0,1,.60,0.001);

    addAndMakeVisible(autoLatency);
    setupKnob(cleanBass,"CLEAN BASS",-12,12,0,.1," dB");setupKnob(cleanMid,"CLEAN MID",-12,12,-1,.1," dB");setupKnob(cleanTreble,"CLEAN TREBLE",-12,12,2.5,.1," dB");
    setupKnob(subBass,"SUB BASS",-12,12,2,.1," dB");setupKnob(subMid,"SUB MID",-12,12,.5,.1," dB");setupKnob(subTreble,"SUB TREBLE",-12,12,-2,.1," dB");

    addAndMakeVisible(dualDelayEnabled);
    setupKnob(delayL,"TIME L",1,2000,280,1," ms");setupKnob(delayR,"TIME R",1,2000,420,1," ms");
    setupKnob(feedbackL,"FB L",-.95,.95,.28,.001);setupKnob(feedbackR,"FB R",-.95,.95,.32,.001);setupKnob(crossFeedback,"X-FB",-.9,.9,.12,.001);setupKnob(delayMix,"MIX",0,1,.20,.001);
    setupKnob(delayLowCut,"LOW CUT",20,800,120,1," Hz");setupKnob(delayHighCut,"HIGH CUT",1200,18000,9000,10," Hz");setupKnob(delayModRate,"MOD RATE",.01,8,.35,.01," Hz");setupKnob(delayModDepth,"MOD DEPTH",0,8,.7,.01," ms");

    outputMode.addItem("STEREO MIX",1);outputMode.addItem("OUT1 MAIN+CLEAN / OUT2 SUB",2);addAndMakeVisible(outputMode);
    for(int i=0;i<SceneSwitcherHQ::sceneCount;++i)sceneSelector.addItem("SCENE "+juce::String(i+1),i+1);sceneSelector.setSelectedId(1);addAndMakeVisible(sceneSelector);addAndMakeVisible(sceneCapture);addAndMakeVisible(sceneRecall);

    inputEnabled.onClick=[this]{pushInput();};for(auto* s:{&pickupR,&pickupL,&cableC,&inputZ,&inputTrim})s->onValueChange=[this]{pushInput();};
    pitchEnabled.onClick=[this]{pushPitch();};pitchMain.onClick=[this]{pushPitch();};pitchClean.onClick=[this]{pushPitch();};pitchSub.onClick=[this]{pushPitch();};for(auto* s:{&pitchSemitones,&pitchExpression,&pitchWet,&pitchDry,&pitchTracking,&pitchTone,&pitchSmooth})s->onValueChange=[this]{pushPitch();};
    autoLatency.onClick=[this]{pushRigDetail();};for(auto* s:{&cleanBass,&cleanMid,&cleanTreble,&subBass,&subMid,&subTreble})s->onValueChange=[this]{pushRigDetail();};
    dualDelayEnabled.onClick=[this]{pushDualDelay();};for(auto* s:{&delayL,&delayR,&feedbackL,&feedbackR,&crossFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayModRate,&delayModDepth})s->onValueChange=[this]{pushDualDelay();};
    outputMode.onChange=[this]{if(!updating)engine.setOutputMode(outputMode.getSelectedId()==2?SignalChain::OutputMode::mainLeftSubRight:SignalChain::OutputMode::stereoMix);};
    sceneCapture.onClick=[this]{captureScene();};sceneRecall.onClick=[this]{recallScene();};
    refreshFromEngine();
}

void PerformancePage::setupKnob(juce::Slider& s,const juce::String& name,double lo,double hi,double value,double step,const juce::String& suffix)
{
    s.setName(name);s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20);s.setRange(lo,hi,step);s.setValue(value,juce::dontSendNotification);s.setTextValueSuffix(suffix);addAndMakeVisible(s);
}

void PerformancePage::refreshFromEngine()
{
    updating=true;
    auto& in=engine.getInputLoadingControl();inputEnabled.setToggleState(in.enabled.load(),juce::dontSendNotification);pickupR.setValue(in.pickupResistanceOhm.load()/1000.0f,juce::dontSendNotification);pickupL.setValue(in.pickupInductanceH.load(),juce::dontSendNotification);cableC.setValue(in.cableCapacitancePf.load(),juce::dontSendNotification);inputZ.setValue(in.inputImpedanceOhm.load()/1000.0f,juce::dontSendNotification);inputTrim.setValue(in.trimDb.load(),juce::dontSendNotification);
    auto& p=engine.getExpressionPitchControl();pitchEnabled.setToggleState(p.enabled.load(),juce::dontSendNotification);const int pm=p.routeMask.load();pitchMain.setToggleState((pm&HQEffectsRack::routeMain)!=0,juce::dontSendNotification);pitchClean.setToggleState((pm&HQEffectsRack::routeClean)!=0,juce::dontSendNotification);pitchSub.setToggleState((pm&HQEffectsRack::routeSub)!=0,juce::dontSendNotification);pitchSemitones.setValue(p.semitones.load(),juce::dontSendNotification);pitchExpression.setValue(p.expression.load(),juce::dontSendNotification);pitchWet.setValue(p.wet.load(),juce::dontSendNotification);pitchDry.setValue(p.dry.load(),juce::dontSendNotification);pitchTracking.setValue(p.tracking.load(),juce::dontSendNotification);pitchTone.setValue(p.tone.load(),juce::dontSendNotification);pitchSmooth.setValue(p.smooth.load(),juce::dontSendNotification);
    auto& r=engine.getHQEffectsRack().parallelRigControl();autoLatency.setToggleState(r.autoLatencyComp.load(),juce::dontSendNotification);cleanBass.setValue(r.cleanBassDb.load(),juce::dontSendNotification);cleanMid.setValue(r.cleanMidDb.load(),juce::dontSendNotification);cleanTreble.setValue(r.cleanTrebleDb.load(),juce::dontSendNotification);subBass.setValue(r.subBassDb.load(),juce::dontSendNotification);subMid.setValue(r.subMidDb.load(),juce::dontSendNotification);subTreble.setValue(r.subTrebleDb.load(),juce::dontSendNotification);
    auto& d=engine.getDualDelayControl();dualDelayEnabled.setToggleState(d.enabled.load(),juce::dontSendNotification);delayL.setValue(d.timeLms.load(),juce::dontSendNotification);delayR.setValue(d.timeRms.load(),juce::dontSendNotification);feedbackL.setValue(d.feedbackL.load(),juce::dontSendNotification);feedbackR.setValue(d.feedbackR.load(),juce::dontSendNotification);crossFeedback.setValue(d.crossFeedback.load(),juce::dontSendNotification);delayMix.setValue(d.mix.load(),juce::dontSendNotification);delayLowCut.setValue(d.lowCutHz.load(),juce::dontSendNotification);delayHighCut.setValue(d.highCutHz.load(),juce::dontSendNotification);delayModRate.setValue(d.modRateHz.load(),juce::dontSendNotification);delayModDepth.setValue(d.modDepthMs.load(),juce::dontSendNotification);
    outputMode.setSelectedId(engine.getOutputMode()==SignalChain::OutputMode::mainLeftSubRight?2:1,juce::dontSendNotification);
    updating=false;
}

void PerformancePage::pushInput(){if(updating)return;auto& c=engine.getInputLoadingControl();c.enabled.store(inputEnabled.getToggleState());c.pickupResistanceOhm.store((float)pickupR.getValue()*1000.0f);c.pickupInductanceH.store((float)pickupL.getValue());c.cableCapacitancePf.store((float)cableC.getValue());c.inputImpedanceOhm.store((float)inputZ.getValue()*1000.0f);c.trimDb.store((float)inputTrim.getValue());}
void PerformancePage::pushPitch(){if(updating)return;auto& c=engine.getExpressionPitchControl();int mask=0;if(pitchMain.getToggleState())mask|=HQEffectsRack::routeMain;if(pitchClean.getToggleState())mask|=HQEffectsRack::routeClean;if(pitchSub.getToggleState())mask|=HQEffectsRack::routeSub;c.enabled.store(pitchEnabled.getToggleState());c.routeMask.store(mask);c.semitones.store((float)pitchSemitones.getValue());c.expression.store((float)pitchExpression.getValue());c.wet.store((float)pitchWet.getValue());c.dry.store((float)pitchDry.getValue());c.tracking.store((float)pitchTracking.getValue());c.tone.store((float)pitchTone.getValue());c.smooth.store((float)pitchSmooth.getValue());}
void PerformancePage::pushRigDetail(){if(updating)return;auto& c=engine.getHQEffectsRack().parallelRigControl();c.autoLatencyComp.store(autoLatency.getToggleState());c.cleanBassDb.store((float)cleanBass.getValue());c.cleanMidDb.store((float)cleanMid.getValue());c.cleanTrebleDb.store((float)cleanTreble.getValue());c.subBassDb.store((float)subBass.getValue());c.subMidDb.store((float)subMid.getValue());c.subTrebleDb.store((float)subTreble.getValue());}
void PerformancePage::pushDualDelay(){if(updating)return;auto& c=engine.getDualDelayControl();c.enabled.store(dualDelayEnabled.getToggleState());c.timeLms.store((float)delayL.getValue());c.timeRms.store((float)delayR.getValue());c.feedbackL.store((float)feedbackL.getValue());c.feedbackR.store((float)feedbackR.getValue());c.crossFeedback.store((float)crossFeedback.getValue());c.mix.store((float)delayMix.getValue());c.lowCutHz.store((float)delayLowCut.getValue());c.highCutHz.store((float)delayHighCut.getValue());c.modRateHz.store((float)delayModRate.getValue());c.modDepthMs.store((float)delayModDepth.getValue());}

void PerformancePage::captureScene()
{
    const int idx=juce::jlimit(0,SceneSwitcherHQ::sceneCount-1,sceneSelector.getSelectedId()-1);auto& s=engine.getSceneSwitcher().scene(idx);auto& r=engine.getHQEffectsRack().parallelRigControl();auto& p=engine.getExpressionPitchControl();auto& d=engine.getDualDelayControl();s.parallelEnabled=r.enabled.load();s.mainDb=r.mainLevelDb.load();s.cleanDb=r.cleanLevelDb.load();s.subDb=r.subLevelDb.load();s.expressionPitchEnabled=p.enabled.load();s.expression=p.expression.load();s.pitchSemitones=p.semitones.load();s.dualDelayEnabled=d.enabled.load();for(int i=0;i<HQEffectsRack::pedalSlots;++i){auto& slot=engine.getHQEffectsRack().pedalSlot(i);s.pedalEnabled[(size_t)i]=slot.enabled.load();s.pedalRouteMask[(size_t)i]=slot.routeMask.load();}
}
void PerformancePage::recallScene(){const int idx=juce::jlimit(0,SceneSwitcherHQ::sceneCount-1,sceneSelector.getSelectedId()-1);engine.getSceneSwitcher().request(idx);}

void PerformancePage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12,16,21));g.setColour(juce::Colour::fromRGB(28,34,42));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f),12.0f,1.0f);
    g.setColour(juce::Colour::fromRGB(150,158,168));g.setFont(10.5f);
    for(auto* s:{&pickupR,&pickupL,&cableC,&inputZ,&inputTrim,&pitchSemitones,&pitchExpression,&pitchWet,&pitchDry,&pitchTracking,&pitchTone,&pitchSmooth,&cleanBass,&cleanMid,&cleanTreble,&subBass,&subMid,&subTreble,&delayL,&delayR,&feedbackL,&feedbackR,&crossFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayModRate,&delayModDepth})g.drawText(s->getName(),s->getBounds().withHeight(16).translated(0,-12),juce::Justification::centred);
}

void PerformancePage::resized()
{
    auto r=getLocalBounds().reduced(22);title.setBounds(r.removeFromTop(38));
    auto layoutSection=[&](juce::Label& label,int h){auto s=r.removeFromTop(h);label.setBounds(s.removeFromTop(22));return s;};
    auto in=layoutSection(inputTitle,112);auto ih=in.removeFromTop(26);inputEnabled.setBounds(ih.removeFromLeft(130));int iw=juce::jmax(86,in.getWidth()/5);for(auto* s:{&pickupR,&pickupL,&cableC,&inputZ,&inputTrim})s->setBounds(in.removeFromLeft(iw).reduced(3,8));r.removeFromTop(5);
    auto pi=layoutSection(pitchTitle,126);auto ph=pi.removeFromTop(26);pitchEnabled.setBounds(ph.removeFromLeft(110));pitchMain.setBounds(ph.removeFromLeft(80));pitchClean.setBounds(ph.removeFromLeft(85));pitchSub.setBounds(ph.removeFromLeft(70));int pw=juce::jmax(82,pi.getWidth()/7);for(auto* s:{&pitchSemitones,&pitchExpression,&pitchWet,&pitchDry,&pitchTracking,&pitchTone,&pitchSmooth})s->setBounds(pi.removeFromLeft(pw).reduced(3,8));r.removeFromTop(5);
    auto rig=layoutSection(rigTitle,112);auto rh=rig.removeFromTop(26);autoLatency.setBounds(rh.removeFromLeft(150));int rw=juce::jmax(84,rig.getWidth()/6);for(auto* s:{&cleanBass,&cleanMid,&cleanTreble,&subBass,&subMid,&subTreble})s->setBounds(rig.removeFromLeft(rw).reduced(3,8));r.removeFromTop(5);
    auto de=layoutSection(delayTitle,120);auto dh=de.removeFromTop(26);dualDelayEnabled.setBounds(dh.removeFromLeft(130));int dw=juce::jmax(76,de.getWidth()/10);for(auto* s:{&delayL,&delayR,&feedbackL,&feedbackR,&crossFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayModRate,&delayModDepth})s->setBounds(de.removeFromLeft(dw).reduced(2,8));r.removeFromTop(5);
    auto sc=layoutSection(sceneTitle,54);sceneSelector.setBounds(sc.removeFromLeft(120).reduced(3));sceneCapture.setBounds(sc.removeFromLeft(90).reduced(3));sceneRecall.setBounds(sc.removeFromLeft(90).reduced(3));sc.removeFromLeft(20);outputMode.setBounds(sc.removeFromLeft(260).reduced(3));
}
