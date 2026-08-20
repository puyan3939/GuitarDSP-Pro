#include "EffectsPage.h"

EffectsPage::EffectsPage(guitardsp::hq::HQEffectsRack& r) : rack(r)
{
    title.setText("FX / ROUTING / HQ POST", juce::dontSendNotification);
    title.setFont(juce::Font(26.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    auto setupLabel = [this](juce::Label& l, const juce::String& text, bool accent)
    {
        l.setText(text, juce::dontSendNotification);
        l.setColour(juce::Label::textColourId, accent ? juce::Colour::fromRGB(222,110,58) : juce::Colour::fromRGB(190,198,208));
        l.setFont(juce::Font(accent ? 15.0f : 12.0f, accent ? juce::Font::bold : juce::Font::plain));
        addAndMakeVisible(l);
    };

    setupLabel(routingTitle, "PARALLEL RIG", true); setupLabel(modTitle, "MODULATION", true); setupLabel(delayTitle, "DELAY", true); setupLabel(reverbTitle, "REVERB", true);
    setupLabel(routingHint, "MAIN + CLEAN ATTACK + OCTAVE-DOWN BASS bus. Split is pre-pedal / pre-amp.", false);
    setupLabel(modHint, "", false); setupLabel(delayHint, "", false); setupLabel(reverbHint, "", false);

    for(auto* b:{&parallelEnabled,&cleanEnabled,&subEnabled,&cleanInvert,&subInvert}) addAndMakeVisible(*b);

    setupKnob(mainLevel,-30,12,0,0.1," dB"); setupKnob(mainDelay,0,25,0,0.01," ms");
    setupKnob(cleanLevel,-40,12,-12,0.1," dB"); setupKnob(cleanHp,60,5000,650,1," Hz"); setupKnob(cleanLp,2500,20000,14500,10," Hz"); setupKnob(cleanPresence,-12,12,4,0.1," dB"); setupKnob(cleanDrive,0,1,0.08,0.001); setupKnob(cleanDelay,0,25,0,0.01," ms");
    setupKnob(subLevel,-40,12,-8,0.1," dB"); setupKnob(subHp,18,180,32,1," Hz"); setupKnob(subLp,500,8000,3200,10," Hz"); setupKnob(subBody,-12,12,3,0.1," dB"); setupKnob(subDrive,0,1,0.34,0.001); setupKnob(subTracking,0,1,0.72,0.001); setupKnob(subTone,0,1,0.62,0.001); setupKnob(subSmooth,0,1,0.68,0.001); setupKnob(subDelay,0,25,0,0.01," ms");

    const char* routingNames[]={"MAIN LVL","MAIN DLY","CLEAN LVL","CLEAN HP","CLEAN LP","PRESENCE","CLEAN DRV","CLEAN DLY","SUB LVL","SUB HP","SUB LP","SUB BODY","SUB DRV","TRACK","SUB TONE","SMOOTH","SUB DLY"};
    juce::Slider* routingSliders[]={&mainLevel,&mainDelay,&cleanLevel,&cleanHp,&cleanLp,&cleanPresence,&cleanDrive,&cleanDelay,&subLevel,&subHp,&subLp,&subBody,&subDrive,&subTracking,&subTone,&subSmooth,&subDelay};
    for(int i=0;i<17;++i){routingSliders[i]->setName(routingNames[i]);addAndMakeVisible(*routingSliders[i]);}

    modType.addItem("OFF",1); modType.addItem("CHORUS",2); modType.addItem("FLANGER",3); modType.addItem("PHASER",4); modType.addItem("TREMOLO",5); modType.addItem("VIBRATO",6);
    modType.setSelectedId(1); addAndMakeVisible(modType);
    delayType.addItem("DIGITAL",1); delayType.addItem("ANALOG",2); delayType.addItem("TAPE",3); delayType.setSelectedId(1); addAndMakeVisible(delayType); addAndMakeVisible(delayEnabled);
    reverbType.addItem("ROOM",1); reverbType.addItem("PLATE",2); reverbType.addItem("HALL",3); reverbType.addItem("SPRING",4); reverbType.setSelectedId(1); addAndMakeVisible(reverbType); addAndMakeVisible(reverbEnabled);

    setupKnob(modRate,0.05,12.0,0.7,0.01," Hz"); setupKnob(modDepth,0,1,0.5,0.001); setupKnob(modMix,0,1,0.5,0.001); setupKnob(modFeedback,-0.9,0.9,0,0.001); setupKnob(modManual,0,1,0.5,0.001); setupKnob(modShape,0,1,0.5,0.001);
    setupKnob(delayTime,1,2000,380,1," ms"); setupKnob(delayFeedback,-0.95,0.95,0.35,0.001); setupKnob(delayMix,0,1,0.28,0.001); setupKnob(delayLowCut,20,800,80,1," Hz"); setupKnob(delayHighCut,1200,18000,6500,10," Hz"); setupKnob(delayDrive,0,1,0.1,0.001); setupKnob(delayWow,0,1,0.15,0.001); setupKnob(delayFlutter,0,1,0.08,0.001); setupKnob(delayAge,0,1,0.2,0.001);
    setupKnob(reverbSize,0,1,0.55,0.001); setupKnob(reverbDecay,0,1,0.55,0.001); setupKnob(reverbDamping,0,1,0.5,0.001); setupKnob(reverbPreDelay,0,200,18,1," ms"); setupKnob(reverbMix,0,1,0.22,0.001); setupKnob(reverbMod,0,1,0.15,0.001); setupKnob(reverbDrip,0,1,0.35,0.001);

    const char* modNames[]={"RATE","DEPTH","MIX","FEEDBACK","MANUAL","SHAPE"}; juce::Slider* modSliders[]={&modRate,&modDepth,&modMix,&modFeedback,&modManual,&modShape}; for(int i=0;i<6;++i)modSliders[i]->setName(modNames[i]);
    const char* delayNames[]={"TIME","FEEDBACK","MIX","LOW CUT","HIGH CUT","DRIVE","WOW","FLUTTER","AGE"}; juce::Slider* delaySliders[]={&delayTime,&delayFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayDrive,&delayWow,&delayFlutter,&delayAge}; for(int i=0;i<9;++i)delaySliders[i]->setName(delayNames[i]);
    const char* reverbNames[]={"SIZE","DECAY","DAMPING","PRE DELAY","MIX","MOD","DRIP"}; juce::Slider* reverbSliders[]={&reverbSize,&reverbDecay,&reverbDamping,&reverbPreDelay,&reverbMix,&reverbMod,&reverbDrip}; for(int i=0;i<7;++i)reverbSliders[i]->setName(reverbNames[i]);

    for (auto* s : {&modRate,&modDepth,&modMix,&modFeedback,&modManual,&modShape,&delayTime,&delayFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayDrive,&delayWow,&delayFlutter,&delayAge,&reverbSize,&reverbDecay,&reverbDamping,&reverbPreDelay,&reverbMix,&reverbMod,&reverbDrip}) addAndMakeVisible(*s);

    parallelEnabled.onClick=[this]{pushParallel();}; cleanEnabled.onClick=[this]{pushParallel();}; subEnabled.onClick=[this]{pushParallel();}; cleanInvert.onClick=[this]{pushParallel();}; subInvert.onClick=[this]{pushParallel();};
    for(auto* s:routingSliders) s->onValueChange=[this]{pushParallel();};

    modType.onChange=[this]{pushMod();updateModeHints();}; for(auto* s:{&modRate,&modDepth,&modMix,&modFeedback,&modManual,&modShape}) s->onValueChange=[this]{pushMod();};
    delayEnabled.onClick=[this]{rack.setDelayEnabled(delayEnabled.getToggleState());}; delayType.onChange=[this]{pushDelay();updateModeHints();}; for(auto* s:{&delayTime,&delayFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayDrive,&delayWow,&delayFlutter,&delayAge}) s->onValueChange=[this]{pushDelay();};
    reverbEnabled.onClick=[this]{rack.setReverbEnabled(reverbEnabled.getToggleState());}; reverbType.onChange=[this]{pushReverb();updateModeHints();}; for(auto* s:{&reverbSize,&reverbDecay,&reverbDamping,&reverbPreDelay,&reverbMix,&reverbMod,&reverbDrip}) s->onValueChange=[this]{pushReverb();};

    parallelEnabled.setToggleState(false,juce::dontSendNotification); cleanEnabled.setToggleState(true,juce::dontSendNotification); subEnabled.setToggleState(true,juce::dontSendNotification);
    delayEnabled.setToggleState(false,juce::dontSendNotification); reverbEnabled.setToggleState(false,juce::dontSendNotification);
    pushParallel(); pushMod(); pushDelay(); pushReverb(); rack.setDelayEnabled(false); rack.setReverbEnabled(false); updateModeHints();
}

void EffectsPage::setupKnob(juce::Slider& s,double lo,double hi,double value,double step,const juce::String& suffix)
{
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20); s.setRange(lo,hi,step); s.setValue(value,juce::dontSendNotification); s.setTextValueSuffix(suffix);
}

void EffectsPage::pushParallel()
{
    auto& c=rack.parallelRigControl();
    c.enabled.store(parallelEnabled.getToggleState()); c.cleanEnabled.store(cleanEnabled.getToggleState()); c.subEnabled.store(subEnabled.getToggleState());
    c.cleanInvert.store(cleanInvert.getToggleState()); c.subInvert.store(subInvert.getToggleState());
    c.mainLevelDb.store((float)mainLevel.getValue()); c.mainDelayMs.store((float)mainDelay.getValue());
    c.cleanLevelDb.store((float)cleanLevel.getValue()); c.cleanHpHz.store((float)cleanHp.getValue()); c.cleanLpHz.store((float)cleanLp.getValue()); c.cleanPresenceDb.store((float)cleanPresence.getValue()); c.cleanDrive.store((float)cleanDrive.getValue()); c.cleanDelayMs.store((float)cleanDelay.getValue());
    c.subLevelDb.store((float)subLevel.getValue()); c.subHpHz.store((float)subHp.getValue()); c.subLpHz.store((float)subLp.getValue()); c.subBodyDb.store((float)subBody.getValue()); c.subDrive.store((float)subDrive.getValue()); c.subTracking.store((float)subTracking.getValue()); c.subTone.store((float)subTone.getValue()); c.subSmooth.store((float)subSmooth.getValue()); c.subDelayMs.store((float)subDelay.getValue());
}

void EffectsPage::pushMod(){rack.setModulationMode((guitardsp::hq::HQEffectsRack::ModulationMode)juce::jlimit(0,5,modType.getSelectedId()-1));auto& c=rack.modulationControl();c.rateHz.store((float)modRate.getValue());c.depth.store((float)modDepth.getValue());c.mix.store((float)modMix.getValue());c.feedback.store((float)modFeedback.getValue());c.manual.store((float)modManual.getValue());c.shape.store((float)modShape.getValue());}
void EffectsPage::pushDelay(){auto& c=rack.delayControl();c.flavor.store(juce::jlimit(0,2,delayType.getSelectedId()-1));c.timeMs.store((float)delayTime.getValue());c.feedback.store((float)delayFeedback.getValue());c.mix.store((float)delayMix.getValue());c.lowCutHz.store((float)delayLowCut.getValue());c.highCutHz.store((float)delayHighCut.getValue());c.drive.store((float)delayDrive.getValue());c.wow.store((float)delayWow.getValue());c.flutter.store((float)delayFlutter.getValue());c.age.store((float)delayAge.getValue());}
void EffectsPage::pushReverb(){auto& c=rack.reverbControl();c.flavor.store(juce::jlimit(0,3,reverbType.getSelectedId()-1));c.size.store((float)reverbSize.getValue());c.decay.store((float)reverbDecay.getValue());c.damping.store((float)reverbDamping.getValue());c.preDelayMs.store((float)reverbPreDelay.getValue());c.mix.store((float)reverbMix.getValue());c.mod.store((float)reverbMod.getValue());c.drip.store((float)reverbDrip.getValue());}

void EffectsPage::updateModeHints()
{
    static const char* modHints[]={"OFF","CHORUS: rate / depth / mix / feedback / centre","FLANGER: short delay + feedback for comb sweep","PHASER: all-pass sweep; feedback changes intensity","TREMOLO: amplitude modulation; SHAPE changes chop character","VIBRATO: true variable-delay pitch modulation; MANUAL sets centre"};
    modHint.setText(modHints[juce::jlimit(0,5,modType.getSelectedId()-1)],juce::dontSendNotification);
    const int d=juce::jlimit(0,2,delayType.getSelectedId()-1);delayHint.setText(d==0?"DIGITAL: clean repeats":d==1?"ANALOG: darker saturated feedback; DRIVE matters":"TAPE: DRIVE + WOW + FLUTTER + AGE shape repeats",juce::dontSendNotification);
    const int rv=juce::jlimit(0,3,reverbType.getSelectedId()-1);reverbHint.setText(rv==0?"ROOM: compact natural ambience":rv==1?"PLATE: dense smooth reflections":rv==2?"HALL: long/open ambience":"SPRING: DRIP is the key character control",juce::dontSendNotification);
}

void EffectsPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12,16,21));g.setColour(juce::Colour::fromRGB(28,34,42));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f),12.0f,1.0f);
    g.setColour(juce::Colour::fromRGB(38,45,54));auto r=getLocalBounds().reduced(22);r.removeFromTop(46);auto h=(r.getHeight()-30)/4;for(int i=0;i<4;++i){g.fillRoundedRectangle(r.removeFromTop(h).toFloat(),8.0f);if(i<3)r.removeFromTop(10);}
    g.setColour(juce::Colour::fromRGB(158,167,178));g.setFont(10.0f);
    auto drawNames=[&g](std::initializer_list<juce::Slider*> sliders){for(auto* s:sliders)g.drawText(s->getName(),s->getBounds().withHeight(14),juce::Justification::centredTop);};
    drawNames({&mainLevel,&mainDelay,&cleanLevel,&cleanHp,&cleanLp,&cleanPresence,&cleanDrive,&cleanDelay,&subLevel,&subHp,&subLp,&subBody,&subDrive,&subTracking,&subTone,&subSmooth,&subDelay});
    drawNames({&modRate,&modDepth,&modMix,&modFeedback,&modManual,&modShape});drawNames({&delayTime,&delayFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayDrive,&delayWow,&delayFlutter,&delayAge});drawNames({&reverbSize,&reverbDecay,&reverbDamping,&reverbPreDelay,&reverbMix,&reverbMod,&reverbDrip});
}

void EffectsPage::resized()
{
    auto r=getLocalBounds().reduced(24);title.setBounds(r.removeFromTop(40));const int sectionH=(r.getHeight()-24)/4;
    auto route=r.removeFromTop(sectionH).reduced(10,5);r.removeFromTop(8);auto mod=r.removeFromTop(sectionH).reduced(10,5);r.removeFromTop(8);auto del=r.removeFromTop(sectionH).reduced(10,5);r.removeFromTop(8);auto rev=r.reduced(10,5);

    auto routeHead=route.removeFromTop(28);routingTitle.setBounds(routeHead.removeFromLeft(120));parallelEnabled.setBounds(routeHead.removeFromLeft(130));cleanEnabled.setBounds(routeHead.removeFromLeft(78));subEnabled.setBounds(routeHead.removeFromLeft(65));cleanInvert.setBounds(routeHead.removeFromLeft(88));subInvert.setBounds(routeHead.removeFromLeft(75));routingHint.setBounds(routeHead);
    const int rowH=juce::jmax(42,route.getHeight()/2);auto routeA=route.removeFromTop(rowH);auto routeB=route;
    juce::Slider* ra[]={&mainLevel,&mainDelay,&cleanLevel,&cleanHp,&cleanLp,&cleanPresence,&cleanDrive,&cleanDelay};int raw=juce::jmax(1,routeA.getWidth()/8);for(auto* s:ra)s->setBounds(routeA.removeFromLeft(raw).reduced(2,6));
    juce::Slider* rb[]={&subLevel,&subHp,&subLp,&subBody,&subDrive,&subTracking,&subTone,&subSmooth,&subDelay};int rbw=juce::jmax(1,routeB.getWidth()/9);for(auto* s:rb)s->setBounds(routeB.removeFromLeft(rbw).reduced(2,6));

    auto modHead=mod.removeFromTop(30);modTitle.setBounds(modHead.removeFromLeft(120));modType.setBounds(modHead.removeFromLeft(150));modHint.setBounds(modHead);juce::Slider* ms[]={&modRate,&modDepth,&modMix,&modFeedback,&modManual,&modShape};int mw=mod.getWidth()/6;for(auto* s:ms)s->setBounds(mod.removeFromLeft(mw).reduced(4,8));
    auto dHead=del.removeFromTop(30);delayTitle.setBounds(dHead.removeFromLeft(85));delayEnabled.setBounds(dHead.removeFromLeft(110));delayType.setBounds(dHead.removeFromLeft(130));delayHint.setBounds(dHead);juce::Slider* ds[]={&delayTime,&delayFeedback,&delayMix,&delayLowCut,&delayHighCut,&delayDrive,&delayWow,&delayFlutter,&delayAge};int dw=del.getWidth()/9;for(auto* s:ds)s->setBounds(del.removeFromLeft(dw).reduced(2,8));
    auto rHead=rev.removeFromTop(30);reverbTitle.setBounds(rHead.removeFromLeft(95));reverbEnabled.setBounds(rHead.removeFromLeft(120));reverbType.setBounds(rHead.removeFromLeft(130));reverbHint.setBounds(rHead);juce::Slider* rs[]={&reverbSize,&reverbDecay,&reverbDamping,&reverbPreDelay,&reverbMix,&reverbMod,&reverbDrip};int rw=rev.getWidth()/7;for(auto* s:rs)s->setBounds(rev.removeFromLeft(rw).reduced(3,8));
}
