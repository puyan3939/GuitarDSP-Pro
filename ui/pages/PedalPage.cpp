#include "PedalPage.h"

using namespace guitardsp::hq;

PedalPage::PedalPage(HQEffectsRack& rack) : effectsRack(rack)
{
    title.setText("PEDAL / HQ BOARD", juce::dontSendNotification);
    title.setFont(juce::Font(28.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    description.setColour(juce::Label::textColourId, juce::Colour::fromRGB(170,178,188));
    description.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(description);

    for (int i=0; i<HQEffectsRack::pedalSlots; ++i) slotSelector.addItem("SLOT " + juce::String(i+1), i+1);
    slotSelector.setSelectedId(1, juce::dontSendNotification); addAndMakeVisible(slotSelector);

    const char* models[] = {"Clean Boost","Treble Boost","Mid Overdrive","Transparent OD","Hard Distortion","Germanium Fuzz","Silicon Fuzz","Octave Fuzz","Velcro Fuzz","HQ Octaver"};
    for (int i=0;i<10;++i) modelSelector.addItem(models[i],i+1);
    addAndMakeVisible(modelSelector); addAndMakeVisible(enabledButton);

    setupKnob(drive,"DRIVE",0.0,1.0,0.001); setupKnob(tone,"TONE",0.0,1.0,0.001);
    setupKnob(level,"LEVEL",-24.0,18.0,0.1); level.setTextValueSuffix(" dB");
    setupKnob(blend,"BLEND",0.0,1.0,0.001); setupKnob(aux1,"AUX 1",0.0,1.0,0.001); setupKnob(aux2,"AUX 2",0.0,1.0,0.001); setupKnob(aux3,"AUX 3",0.0,1.0,0.001);

    dynamicsTitle.setText("DYNAMICS / HQ",juce::dontSendNotification); dynamicsTitle.setColour(juce::Label::textColourId,juce::Colour::fromRGB(222,110,58)); dynamicsTitle.setFont(juce::Font(13.0f,juce::Font::bold)); addAndMakeVisible(dynamicsTitle);
    dynamicsHint.setColour(juce::Label::textColourId,juce::Colour::fromRGB(160,170,180)); addAndMakeVisible(dynamicsHint);
    grLabel.setColour(juce::Label::textColourId,juce::Colour::fromRGB(98,213,167)); grLabel.setFont(juce::Font(12.0f,juce::Font::bold)); addAndMakeVisible(grLabel);
    dynamicsMode.addItem("OFF",1); dynamicsMode.addItem("PRECISION GATE",2); dynamicsMode.addItem("HQ STUDIO COMP",3); dynamicsMode.addItem("HQ GUITAR COMP",4); addAndMakeVisible(dynamicsMode);

    setupKnob(gateThreshold,"THRESHOLD",-90.0,-20.0,0.5); gateThreshold.setTextValueSuffix(" dB");
    setupKnob(gateRange,"RANGE",-90.0,0.0,1.0); gateRange.setTextValueSuffix(" dB"); setupKnob(gateRatio,"RATIO",1.0,12.0,0.1);
    setupKnob(gateAttack,"ATTACK",0.1,25.0,0.1); gateAttack.setTextValueSuffix(" ms"); setupKnob(gateHold,"HOLD",0.0,500.0,1.0); gateHold.setTextValueSuffix(" ms");
    setupKnob(gateRelease,"RELEASE",10.0,1000.0,1.0); gateRelease.setTextValueSuffix(" ms"); setupKnob(gateHysteresis,"HYST",0.0,12.0,0.1); gateHysteresis.setTextValueSuffix(" dB");
    setupKnob(gateSidechainHp,"SC HP",10.0,500.0,1.0); gateSidechainHp.setTextValueSuffix(" Hz"); setupKnob(gateSidechainLp,"SC LP",1000.0,16000.0,10.0); gateSidechainLp.setTextValueSuffix(" Hz");

    setupKnob(compThreshold,"THRESHOLD",-60.0,0.0,0.5); compThreshold.setTextValueSuffix(" dB"); setupKnob(compRatio,"RATIO",1.0,20.0,0.1);
    setupKnob(compAttack,"ATTACK",0.05,100.0,0.05); compAttack.setTextValueSuffix(" ms"); setupKnob(compRelease,"RELEASE",10.0,2000.0,1.0); compRelease.setTextValueSuffix(" ms");
    setupKnob(compKnee,"KNEE",0.0,24.0,0.1); compKnee.setTextValueSuffix(" dB"); setupKnob(compMakeup,"MAKEUP",-12.0,18.0,0.1); compMakeup.setTextValueSuffix(" dB");
    setupKnob(compMix,"MIX",0.0,1.0,0.001); setupKnob(compSidechainHp,"SC HP",20.0,500.0,1.0); compSidechainHp.setTextValueSuffix(" Hz");
    addAndMakeVisible(compRms); addAndMakeVisible(compAutoRelease); addAndMakeVisible(compAutoMakeup);

    setupKnob(guitarSustain,"SUSTAIN",0.0,1.0,0.001); setupKnob(guitarAttack,"ATTACK",0.0,1.0,0.001); setupKnob(guitarBlend,"BLEND",0.0,1.0,0.001); setupKnob(guitarLevel,"LEVEL",-12.0,18.0,0.1); guitarLevel.setTextValueSuffix(" dB");

    slotSelector.onChange=[this]{loadSlot();}; modelSelector.onChange=[this]{updateModelLabels();pushControls();}; enabledButton.onClick=[this]{pushControls();};
    for(auto* s:{&drive,&tone,&level,&blend,&aux1,&aux2,&aux3}) s->onValueChange=[this]{pushControls();};

    dynamicsMode.onChange=[this]{ if(!updating) effectsRack.setDynamicsMode((HQEffectsRack::DynamicsMode)juce::jlimit(0,3,dynamicsMode.getSelectedId()-1)); updateDynamicsVisibility(); };
    for(auto* s:{&gateThreshold,&gateRange,&gateRatio,&gateAttack,&gateHold,&gateRelease,&gateHysteresis,&gateSidechainHp,&gateSidechainLp}) s->onValueChange=[this]{pushGateControls();};
    for(auto* s:{&compThreshold,&compRatio,&compAttack,&compRelease,&compKnee,&compMakeup,&compMix,&compSidechainHp}) s->onValueChange=[this]{pushStudioCompControls();};
    compRms.onClick=[this]{pushStudioCompControls();}; compAutoRelease.onClick=[this]{pushStudioCompControls();}; compAutoMakeup.onClick=[this]{pushStudioCompControls();};
    for(auto* s:{&guitarSustain,&guitarAttack,&guitarBlend,&guitarLevel}) s->onValueChange=[this]{pushGuitarCompControls();};

    refreshFromEngine(); loadSlot(); updateDynamicsVisibility(); startTimerHz(15);
}

void PedalPage::setupKnob(juce::Slider& s,const juce::String& name,double lo,double hi,double step)
{
    s.setName(name); s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,74,20); s.setRange(lo,hi,step); addAndMakeVisible(s);
}

void PedalPage::refreshFromEngine()
{
    updating=true;
    dynamicsMode.setSelectedId((int)effectsRack.getDynamicsMode()+1,juce::dontSendNotification);
    auto& gc=effectsRack.gateControl(); gateThreshold.setValue(gc.thresholdDb.load(),juce::dontSendNotification); gateRange.setValue(gc.rangeDb.load(),juce::dontSendNotification); gateRatio.setValue(gc.ratio.load(),juce::dontSendNotification); gateAttack.setValue(gc.attackMs.load(),juce::dontSendNotification); gateHold.setValue(gc.holdMs.load(),juce::dontSendNotification); gateRelease.setValue(gc.releaseMs.load(),juce::dontSendNotification); gateHysteresis.setValue(gc.hysteresisDb.load(),juce::dontSendNotification); gateSidechainHp.setValue(gc.sidechainHpHz.load(),juce::dontSendNotification); gateSidechainLp.setValue(gc.sidechainLpHz.load(),juce::dontSendNotification);
    auto& sc=effectsRack.studioCompControl(); compThreshold.setValue(sc.thresholdDb.load(),juce::dontSendNotification); compRatio.setValue(sc.ratio.load(),juce::dontSendNotification); compAttack.setValue(sc.attackMs.load(),juce::dontSendNotification); compRelease.setValue(sc.releaseMs.load(),juce::dontSendNotification); compKnee.setValue(sc.kneeDb.load(),juce::dontSendNotification); compMakeup.setValue(sc.makeupDb.load(),juce::dontSendNotification); compMix.setValue(sc.mix.load(),juce::dontSendNotification); compSidechainHp.setValue(sc.sidechainHpHz.load(),juce::dontSendNotification); compRms.setToggleState(sc.rms.load(),juce::dontSendNotification); compAutoRelease.setToggleState(sc.autoRelease.load(),juce::dontSendNotification); compAutoMakeup.setToggleState(sc.autoMakeup.load(),juce::dontSendNotification);
    auto& g=effectsRack.guitarCompControl(); guitarSustain.setValue(g.sustain.load(),juce::dontSendNotification); guitarAttack.setValue(g.attack.load(),juce::dontSendNotification); guitarBlend.setValue(g.blend.load(),juce::dontSendNotification); guitarLevel.setValue(g.levelDb.load(),juce::dontSendNotification);
    updating=false; updateDynamicsVisibility(); loadSlot();
}

void PedalPage::timerCallback()
{
    grLabel.setText("GR " + juce::String(effectsRack.getCompressorGainReductionDb(),1) + " dB",juce::dontSendNotification);
}

void PedalPage::loadSlot()
{
    const int index=juce::jlimit(0,HQEffectsRack::pedalSlots-1,slotSelector.getSelectedId()-1); auto& c=effectsRack.pedalSlot(index); updating=true;
    enabledButton.setToggleState(c.enabled.load(),juce::dontSendNotification); modelSelector.setSelectedId(juce::jlimit(0,9,c.model.load())+1,juce::dontSendNotification);
    drive.setValue(c.drive.load(),juce::dontSendNotification); tone.setValue(c.tone.load(),juce::dontSendNotification); level.setValue(c.levelDb.load(),juce::dontSendNotification); blend.setValue(c.mix.load(),juce::dontSendNotification);
    aux1.setValue(c.aux1.load(),juce::dontSendNotification); aux2.setValue(c.aux2.load(),juce::dontSendNotification); aux3.setValue(c.aux3.load(),juce::dontSendNotification); updating=false; updateModelLabels();
}

void PedalPage::pushControls()
{
    if(updating)return; const int index=juce::jlimit(0,HQEffectsRack::pedalSlots-1,slotSelector.getSelectedId()-1); auto& c=effectsRack.pedalSlot(index);
    c.enabled.store(enabledButton.getToggleState()); c.model.store(juce::jlimit(0,9,modelSelector.getSelectedId()-1)); c.drive.store((float)drive.getValue()); c.tone.store((float)tone.getValue()); c.levelDb.store((float)level.getValue()); c.mix.store((float)blend.getValue()); c.aux1.store((float)aux1.getValue()); c.aux2.store((float)aux2.getValue()); c.aux3.store((float)aux3.getValue());
}

void PedalPage::pushGateControls()
{
    if(updating)return; auto& c=effectsRack.gateControl(); c.thresholdDb.store((float)gateThreshold.getValue()); c.rangeDb.store((float)gateRange.getValue()); c.ratio.store((float)gateRatio.getValue()); c.attackMs.store((float)gateAttack.getValue()); c.holdMs.store((float)gateHold.getValue()); c.releaseMs.store((float)gateRelease.getValue()); c.hysteresisDb.store((float)gateHysteresis.getValue()); c.sidechainHpHz.store((float)gateSidechainHp.getValue()); c.sidechainLpHz.store((float)gateSidechainLp.getValue());
}

void PedalPage::pushStudioCompControls()
{
    if(updating)return; auto& c=effectsRack.studioCompControl(); c.thresholdDb.store((float)compThreshold.getValue()); c.ratio.store((float)compRatio.getValue()); c.attackMs.store((float)compAttack.getValue()); c.releaseMs.store((float)compRelease.getValue()); c.kneeDb.store((float)compKnee.getValue()); c.makeupDb.store((float)compMakeup.getValue()); c.mix.store((float)compMix.getValue()); c.sidechainHpHz.store((float)compSidechainHp.getValue()); c.rms.store(compRms.getToggleState()); c.autoRelease.store(compAutoRelease.getToggleState()); c.autoMakeup.store(compAutoMakeup.getToggleState());
}

void PedalPage::pushGuitarCompControls()
{
    if(updating)return; auto& c=effectsRack.guitarCompControl(); c.sustain.store((float)guitarSustain.getValue()); c.attack.store((float)guitarAttack.getValue()); c.blend.store((float)guitarBlend.getValue()); c.levelDb.store((float)guitarLevel.getValue());
}

void PedalPage::updateDynamicsVisibility()
{
    const int mode=juce::jlimit(0,3,dynamicsMode.getSelectedId()-1); const bool gate=mode==1, studio=mode==2, guitar=mode==3;
    for(auto* s:{&gateThreshold,&gateRange,&gateRatio,&gateAttack,&gateHold,&gateRelease,&gateHysteresis,&gateSidechainHp,&gateSidechainLp}) s->setVisible(gate);
    for(auto* s:{&compThreshold,&compRatio,&compAttack,&compRelease,&compKnee,&compMakeup,&compMix,&compSidechainHp}) s->setVisible(studio);
    compRms.setVisible(studio); compAutoRelease.setVisible(studio); compAutoMakeup.setVisible(studio);
    for(auto* s:{&guitarSustain,&guitarAttack,&guitarBlend,&guitarLevel}) s->setVisible(guitar);
    grLabel.setVisible(studio||guitar);
    dynamicsHint.setText(mode==0?"Dynamics bypassed":gate?"Gate uses the clean input as its detector key":studio?"Transparent soft-knee compressor: Peak/RMS detector, sidechain HPF, auto release/makeup and parallel MIX":"Fast guitar compressor: SUSTAIN / ATTACK / BLEND / LEVEL",juce::dontSendNotification);
    resized(); repaint();
}

void PedalPage::updateModelLabels()
{
    const int m=juce::jlimit(0,9,modelSelector.getSelectedId()-1); blend.setEnabled(m==3 || m==9); drive.setName("DRIVE"); tone.setName("TONE"); level.setName("LEVEL"); blend.setName(m==3?"CLEAN BLEND":m==9?"DRY":"BLEND (N/A)");
    switch((PedalType)m)
    {
        case PedalType::cleanBoost: aux1.setName("LOW CUT"); aux2.setName("FOCUS"); aux3.setName("MID SHAPE"); description.setText("Clean boost: headroom/push with bandwidth shaping before the amp.",juce::dontSendNotification); break;
        case PedalType::trebleBoost: aux1.setName("LOW CUT"); aux2.setName("FOCUS"); aux3.setName("MID BOOST"); description.setText("Treble boost: cuts low end and pushes the upper mids into the amp.",juce::dontSendNotification); break;
        case PedalType::midOD: aux1.setName("LOW CUT"); aux2.setName("MID FREQ"); aux3.setName("MID BOOST"); description.setText("Mid overdrive: focused asymmetric clipping for a pronounced mid push.",juce::dontSendNotification); break;
        case PedalType::transparentOD: aux1.setName("LOW CUT"); aux2.setName("FOCUS"); aux3.setName("MID SHAPE"); description.setText("Transparent OD: softer clipping plus CLEAN BLEND for parallel dry signal.",juce::dontSendNotification); break;
        case PedalType::hardDistortion: aux1.setName("LOW CUT"); aux2.setName("FOCUS"); aux3.setName("MID SHAPE"); description.setText("Hard distortion: abrupt clipping and dense harmonics; watch output level.",juce::dontSendNotification); break;
        case PedalType::germaniumFuzz: aux1.setName("BIAS"); aux2.setName("BODY"); aux3.setName("TRIM"); description.setText("Germanium fuzz: two-stage asymmetric fuzz with bias and supply-memory behaviour.",juce::dontSendNotification); break;
        case PedalType::siliconFuzz: aux1.setName("LOW CUT"); aux2.setName("FOCUS"); aux3.setName("MID SHAPE"); description.setText("Silicon fuzz: tighter, higher-gain fuzz with stronger harmonic density.",juce::dontSendNotification); break;
        case PedalType::octaveFuzz: aux1.setName("OCTAVE"); aux2.setName("BODY"); aux3.setName("TRIM"); description.setText("Octave fuzz: full-wave rectification creates the upper-octave component before re-clipping.",juce::dontSendNotification); break;
        case PedalType::velcroFuzz: aux1.setName("BIAS"); aux2.setName("STARVE"); aux3.setName("GATE"); description.setText("Velcro fuzz: supply starvation plus hysteretic gating for sputter and note cut-off.",juce::dontSendNotification); break;
        case PedalType::hqOctaver: drive.setName("OCT +1"); tone.setName("OCT -1"); aux1.setName("TRACK"); aux2.setName("PITCH TONE"); aux3.setName("SMOOTH"); description.setText("HQ Octaver: clean fixed octaves using dual-head windowed pitch shifting. DRIVE=+1, TONE=-1, DRY blends original.",juce::dontSendNotification); break;
    }
    repaint();
}

void PedalPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12,16,21)); g.setColour(juce::Colour::fromRGB(28,34,42)); g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f),12.0f);
    g.setColour(juce::Colour::fromRGB(150,158,168)); g.setFont(10.5f);
    auto drawNames=[&g](std::initializer_list<juce::Slider*> sliders){for(auto* s:sliders) if(s->isVisible()) g.drawText(s->getName(),s->getBounds().withHeight(15).translated(0,-12),juce::Justification::centred);};
    drawNames({&gateThreshold,&gateRange,&gateRatio,&gateAttack,&gateHold,&gateRelease,&gateHysteresis,&gateSidechainHp,&gateSidechainLp}); drawNames({&compThreshold,&compRatio,&compAttack,&compRelease,&compKnee,&compMakeup,&compMix,&compSidechainHp}); drawNames({&guitarSustain,&guitarAttack,&guitarBlend,&guitarLevel}); drawNames({&drive,&tone,&level,&blend,&aux1,&aux2,&aux3});
}

void PedalPage::resized()
{
    auto r=getLocalBounds().reduced(24); title.setBounds(r.removeFromTop(38));
    auto top=r.removeFromTop(38); slotSelector.setBounds(top.removeFromLeft(125).reduced(3)); modelSelector.setBounds(top.removeFromLeft(215).reduced(3)); enabledButton.setBounds(top.removeFromLeft(75).reduced(5)); description.setBounds(top);
    auto dynHead=r.removeFromTop(32); dynamicsTitle.setBounds(dynHead.removeFromLeft(110)); dynamicsMode.setBounds(dynHead.removeFromLeft(170).reduced(2)); grLabel.setBounds(dynHead.removeFromRight(90)); dynamicsHint.setBounds(dynHead);
    auto dynRow=r.removeFromTop(112);
    std::vector<juce::Slider*> visible;
    for(auto* s:{&gateThreshold,&gateRange,&gateRatio,&gateAttack,&gateHold,&gateRelease,&gateHysteresis,&gateSidechainHp,&gateSidechainLp,&compThreshold,&compRatio,&compAttack,&compRelease,&compKnee,&compMakeup,&compMix,&compSidechainHp,&guitarSustain,&guitarAttack,&guitarBlend,&guitarLevel}) if(s->isVisible()) visible.push_back(s);
    if(!visible.empty()){const int w=juce::jmax(72,dynRow.getWidth()/(int)visible.size());for(auto* s:visible)s->setBounds(dynRow.removeFromLeft(w).reduced(2,8));}
    auto toggles=r.removeFromTop(25); compRms.setBounds(toggles.removeFromLeft(70)); compAutoRelease.setBounds(toggles.removeFromLeft(100)); compAutoMakeup.setBounds(toggles.removeFromLeft(125));
    r.removeFromTop(5); auto knobs=r.removeFromTop(150); const int w=juce::jmax(90,knobs.getWidth()/7); for(auto* s:{&drive,&tone,&level,&blend,&aux1,&aux2,&aux3}) s->setBounds(knobs.removeFromLeft(w).reduced(4,10));
}
