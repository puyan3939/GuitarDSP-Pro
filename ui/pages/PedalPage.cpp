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

    addAndMakeVisible(gateEnabled);
    setupKnob(gateThreshold,"THRESHOLD",-90.0,-20.0,0.5); gateThreshold.setTextValueSuffix(" dB");
    setupKnob(gateRange,"RANGE",-90.0,0.0,1.0); gateRange.setTextValueSuffix(" dB");
    setupKnob(gateRatio,"RATIO",1.0,12.0,0.1);
    setupKnob(gateAttack,"ATTACK",0.1,25.0,0.1); gateAttack.setTextValueSuffix(" ms");
    setupKnob(gateHold,"HOLD",0.0,500.0,1.0); gateHold.setTextValueSuffix(" ms");
    setupKnob(gateRelease,"RELEASE",10.0,1000.0,1.0); gateRelease.setTextValueSuffix(" ms");
    setupKnob(gateHysteresis,"HYST",0.0,12.0,0.1); gateHysteresis.setTextValueSuffix(" dB");
    setupKnob(gateSidechainHp,"SC HP",10.0,500.0,1.0); gateSidechainHp.setTextValueSuffix(" Hz");
    setupKnob(gateSidechainLp,"SC LP",1000.0,16000.0,10.0); gateSidechainLp.setTextValueSuffix(" Hz");

    auto& gc=effectsRack.gateControl();
    gateThreshold.setValue(gc.thresholdDb.load(),juce::dontSendNotification); gateRange.setValue(gc.rangeDb.load(),juce::dontSendNotification);
    gateRatio.setValue(gc.ratio.load(),juce::dontSendNotification); gateAttack.setValue(gc.attackMs.load(),juce::dontSendNotification);
    gateHold.setValue(gc.holdMs.load(),juce::dontSendNotification); gateRelease.setValue(gc.releaseMs.load(),juce::dontSendNotification);
    gateHysteresis.setValue(gc.hysteresisDb.load(),juce::dontSendNotification); gateSidechainHp.setValue(gc.sidechainHpHz.load(),juce::dontSendNotification); gateSidechainLp.setValue(gc.sidechainLpHz.load(),juce::dontSendNotification);
    gateEnabled.setToggleState(effectsRack.getDynamicsMode()==HQEffectsRack::DynamicsMode::gate,juce::dontSendNotification);

    slotSelector.onChange=[this]{loadSlot();}; modelSelector.onChange=[this]{updateModelLabels();pushControls();}; enabledButton.onClick=[this]{pushControls();};
    for(auto* s:{&drive,&tone,&level,&blend,&aux1,&aux2,&aux3}) s->onValueChange=[this]{pushControls();};
    gateEnabled.onClick=[this]{effectsRack.setDynamicsMode(gateEnabled.getToggleState()?HQEffectsRack::DynamicsMode::gate:HQEffectsRack::DynamicsMode::off);};
    for(auto* s:{&gateThreshold,&gateRange,&gateRatio,&gateAttack,&gateHold,&gateRelease,&gateHysteresis,&gateSidechainHp,&gateSidechainLp}) s->onValueChange=[this]{pushGateControls();};

    loadSlot();
}

void PedalPage::setupKnob(juce::Slider& s,const juce::String& name,double lo,double hi,double step)
{
    s.setName(name); s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,78,20); s.setRange(lo,hi,step); addAndMakeVisible(s);
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
    auto& c=effectsRack.gateControl();
    c.thresholdDb.store((float)gateThreshold.getValue()); c.rangeDb.store((float)gateRange.getValue()); c.ratio.store((float)gateRatio.getValue());
    c.attackMs.store((float)gateAttack.getValue()); c.holdMs.store((float)gateHold.getValue()); c.releaseMs.store((float)gateRelease.getValue());
    c.hysteresisDb.store((float)gateHysteresis.getValue()); c.sidechainHpHz.store((float)gateSidechainHp.getValue()); c.sidechainLpHz.store((float)gateSidechainLp.getValue());
}

void PedalPage::updateModelLabels()
{
    const int m=juce::jlimit(0,9,modelSelector.getSelectedId()-1);
    blend.setEnabled(m==3 || m==9);
    drive.setName("DRIVE"); tone.setName("TONE"); level.setName("LEVEL");
    blend.setName(m==3?"CLEAN BLEND":m==9?"DRY":"BLEND (N/A)");
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
        case PedalType::hqOctaver:
            drive.setName("OCT +1"); tone.setName("OCT -1"); aux1.setName("TRACK"); aux2.setName("PITCH TONE"); aux3.setName("SMOOTH");
            description.setText("HQ Octaver: clean Whammy/POG-style fixed octaves using dual-head windowed pitch shifting. DRIVE=+1 octave, TONE=-1 octave, DRY blends the original.",juce::dontSendNotification); break;
    }
    repaint();
}

void PedalPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12,16,21)); g.setColour(juce::Colour::fromRGB(28,34,42)); g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f),12.0f);
    g.setColour(juce::Colour::fromRGB(222,110,58)); g.setFont(13.0f); g.drawText("PRECISION INPUT GATE",getLocalBounds().reduced(28).withTrimmedTop(130).withHeight(18),juce::Justification::centredLeft);
    g.setColour(juce::Colour::fromRGB(150,158,168)); g.setFont(11.0f);
    const juce::Slider* gateSliders[]={&gateThreshold,&gateRange,&gateRatio,&gateAttack,&gateHold,&gateRelease,&gateHysteresis,&gateSidechainHp,&gateSidechainLp};
    const juce::Slider* pedalSliders[]={&drive,&tone,&level,&blend,&aux1,&aux2,&aux3};
    for(auto* s:gateSliders) g.drawText(s->getName(),s->getBounds().withHeight(16).translated(0,-13),juce::Justification::centred);
    for(auto* s:pedalSliders) g.drawText(s->getName(),s->getBounds().withHeight(18).translated(0,-15),juce::Justification::centred);
}

void PedalPage::resized()
{
    auto r=getLocalBounds().reduced(24); title.setBounds(r.removeFromTop(42));
    auto top=r.removeFromTop(44); slotSelector.setBounds(top.removeFromLeft(140).reduced(4)); modelSelector.setBounds(top.removeFromLeft(230).reduced(4)); enabledButton.setBounds(top.removeFromLeft(90).reduced(8)); description.setBounds(r.removeFromTop(42));

    auto gateHeader=r.removeFromTop(28); gateEnabled.setBounds(gateHeader.removeFromLeft(150).reduced(4));
    auto gateRow=r.removeFromTop(130); const int gw=juce::jmax(82,gateRow.getWidth()/9);
    for(auto* s:{&gateThreshold,&gateRange,&gateRatio,&gateAttack,&gateHold,&gateRelease,&gateHysteresis,&gateSidechainHp,&gateSidechainLp}) s->setBounds(gateRow.removeFromLeft(gw).reduced(3,10));

    r.removeFromTop(10); auto knobs=r.removeFromTop(195); const int w=juce::jmax(100,knobs.getWidth()/7);
    for(auto* s:{&drive,&tone,&level,&blend,&aux1,&aux2,&aux3}) s->setBounds(knobs.removeFromLeft(w).reduced(5,12));
}
