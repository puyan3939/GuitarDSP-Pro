#include "CabPage.h"

CabPage::CabPage(guitardsp::hq::CabMicEngineHQ& engine) : cab(engine)
{
    title.setText("CAB / MIC HQ", juce::dontSendNotification);
    title.setFont(juce::Font(28.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    info.setColour(juce::Label::textColourId, juce::Colour::fromRGB(170, 178, 188));
    addAndMakeVisible(info);
    addAndMakeVisible(enabled);

    irEngine.addItem("CLASSIC IR",1); irEngine.addItem("ADVANCED IR",2); irEngine.setSelectedId(1);
    cabType.addItem("1x12 Open",1); cabType.addItem("2x12 Vintage",2); cabType.addItem("4x12 Vintage",3); cabType.addItem("4x12 Modern",4); cabType.setSelectedId(3);
    micType.addItem("Dynamic 57",1); micType.addItem("Ribbon 121",2); micType.addItem("Condenser 67",3); micType.setSelectedId(1);
    addAndMakeVisible(irEngine); addAndMakeVisible(cabType); addAndMakeVisible(micType);

    auto setup=[](juce::Slider& s,double lo,double hi,double v,const juce::String& suffix)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,76,20);s.setRange(lo,hi,0.01);s.setValue(v);s.setTextValueSuffix(suffix);
    };
    setup(position,0,1,0.42,""); setup(distance,0,1,0.18,""); setup(resonance,0,1,0.55,"");
    setup(lowCut,25,350,70," Hz"); setup(highCut,2500,18000,9000," Hz"); setup(mix,0,1,1.0,""); setup(lowVolumeFeel,0,1,0.0,"");
    position.setName("POSITION"); distance.setName("DISTANCE"); resonance.setName("RESONANCE"); lowCut.setName("LOW CUT"); highCut.setName("HIGH CUT"); mix.setName("MIX"); lowVolumeFeel.setName("LOW VOL FEEL");
    lowVolumeFeel.setTooltip("Adds low-volume body, gentle density compression and mild saturation after the cab without changing the IR itself.");
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel}) addAndMakeVisible(*s);

    enabled.onClick=[this]{cab.setEnabled(enabled.getToggleState());};
    irEngine.onChange=[this]{push();updateInfo();};
    cabType.onChange=[this]{push();}; micType.onChange=[this]{push();};
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel}) s->onDragEnd=[this]{push();};
    lowVolumeFeel.onValueChange=[this]{push();};
    updateInfo(); push(); cab.setEnabled(false); enabled.setToggleState(false,juce::dontSendNotification);
}

void CabPage::updateInfo()
{
    if(irEngine.getSelectedId()==2)
        info.setText("ADVANCED: multi-resonance + baffle/cone breakup + phase-aware early reflections. LOW VOL FEEL adds post-cab body/density for quiet playback.",juce::dontSendNotification);
    else
        info.setText("CLASSIC: original generated cabinet IR. LOW VOL FEEL is a separate post-cab compensation stage, so IR A/B remains valid.",juce::dontSendNotification);
}

void CabPage::push()
{
    auto p=cab.getParameters();
    p.irEngine=(guitardsp::hq::CabIrEngine)juce::jlimit(0,1,irEngine.getSelectedId()-1);
    p.cab=(guitardsp::hq::CabType)juce::jlimit(0,3,cabType.getSelectedId()-1);
    p.mic=(guitardsp::hq::MicType)juce::jlimit(0,2,micType.getSelectedId()-1);
    p.position=(float)position.getValue();p.distance=(float)distance.getValue();p.resonance=(float)resonance.getValue();
    p.lowCutHz=(float)lowCut.getValue();p.highCutHz=(float)highCut.getValue();p.mix=(float)mix.getValue();p.lowVolumeFeel=(float)lowVolumeFeel.getValue();cab.setParameters(p);
}

void CabPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12,16,21));g.setColour(juce::Colour::fromRGB(28,34,42));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f),12.0f,1.0f);
    g.setColour(juce::Colour::fromRGB(158,167,178)); g.setFont(10.5f);
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel})
        g.drawText(s->getName(),s->getBounds().withHeight(15),juce::Justification::centredTop);
}

void CabPage::resized()
{
    auto r=getLocalBounds().reduced(24);title.setBounds(r.removeFromTop(44));info.setBounds(r.removeFromTop(42));
    auto top=r.removeFromTop(44);enabled.setBounds(top.removeFromLeft(135));irEngine.setBounds(top.removeFromLeft(150).reduced(4));cabType.setBounds(top.removeFromLeft(175).reduced(4));micType.setBounds(top.removeFromLeft(170).reduced(4));
    auto knobs=r.removeFromTop(190);const int w=juce::jmax(90,knobs.getWidth()/7);
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel}) s->setBounds(knobs.removeFromLeft(w).reduced(3,8));
}
