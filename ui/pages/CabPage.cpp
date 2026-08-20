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

    irEngine.addItem("CLASSIC IR", 1);
    irEngine.addItem("ADVANCED IR", 2);
    irEngine.addItem("FACTORY IR", 3);
    irEngine.addItem("USER IR", 4);
    irEngine.setSelectedId(1);

    irSize.addItem("1024", 1); irSize.addItem("2048", 2); irSize.addItem("FULL", 3); irSize.setSelectedId(2);
    cabType.addItem("1x12 Open", 1); cabType.addItem("2x12 Vintage", 2); cabType.addItem("4x12 Vintage", 3); cabType.addItem("4x12 Modern", 4); cabType.setSelectedId(3);
    micType.addItem("Dynamic 57", 1); micType.addItem("Ribbon 121", 2); micType.addItem("Condenser 67", 3); micType.setSelectedId(1);
    for (int i = 0; i < guitardsp::hq::FactoryIrCatalog::count; ++i)
        factoryIr.addItem(guitardsp::hq::FactoryIrCatalog::displayName(i), i + 1);
    factoryIr.setSelectedId(1, juce::dontSendNotification);

    addAndMakeVisible(irEngine); addAndMakeVisible(irSize); addAndMakeVisible(cabType); addAndMakeVisible(micType);
    addAndMakeVisible(factoryIr); addAndMakeVisible(loadIrButton); addAndMakeVisible(polarityInvert);

    irName.setColour(juce::Label::textColourId, juce::Colour::fromRGB(122, 205, 180));
    irName.setFont(juce::Font(11.5f));
    addAndMakeVisible(irName);

    auto setup=[](juce::Slider& s,double lo,double hi,double v,const juce::String& suffix)
    {
        s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);s.setTextBoxStyle(juce::Slider::TextBoxBelow,false,76,20);s.setRange(lo,hi,0.01);s.setValue(v);s.setTextValueSuffix(suffix);
    };
    setup(position,0,1,0.42,""); setup(distance,0,1,0.18,""); setup(resonance,0,1,0.55,"");
    setup(lowCut,25,350,70," Hz"); setup(highCut,2500,18000,9000," Hz"); setup(mix,0,1,1.0,""); setup(lowVolumeFeel,0,1,0.0,""); setup(irLevelDb,-24,12,0.0," dB");
    position.setName("POSITION"); distance.setName("DISTANCE"); resonance.setName("RESONANCE"); lowCut.setName("LOW CUT"); highCut.setName("HIGH CUT"); mix.setName("MIX"); lowVolumeFeel.setName("LOW VOL FEEL"); irLevelDb.setName("IR LEVEL");
    lowVolumeFeel.setTooltip("Adds low-volume body, gentle density compression and mild saturation after the cab without changing the IR itself.");
    irLevelDb.setTooltip("Gain applied to the convolved cab signal before dry/wet mix.");
    polarityInvert.setTooltip("Invert IR polarity for phase alignment when comparing or blending captures.");
    irSize.setTooltip("Measured IR target length. 1024 saves CPU, 2048 matches the common compact guitar-cab length, FULL keeps the complete capture including room tail.");
    factoryIr.setTooltip("15 measured 48 kHz / 24-bit mono cabinet captures from the Jester Dyne Brutal IR Pack.");
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel,&irLevelDb}) addAndMakeVisible(*s);

    enabled.onClick=[this]{cab.setEnabled(enabled.getToggleState());};
    irEngine.onChange=[this]
    {
        if (irEngine.getSelectedId() == 4)
            cab.clearExternalImpulse();
        push(); updateExternalControls(); updateInfo();
    };
    irSize.onChange=[this]{push();updateInfo();};
    cabType.onChange=[this]{push();}; micType.onChange=[this]{push();};
    factoryIr.onChange=[this]{if(irEngine.getSelectedId()==3){loadSelectedFactoryIr();push();updateInfo();}};
    polarityInvert.onClick=[this]{push();};
    loadIrButton.onClick=[this]{chooseExternalIr();};
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel,&irLevelDb}) s->onDragEnd=[this]{push();};
    lowVolumeFeel.onValueChange=[this]{push();};
    irLevelDb.onValueChange=[this]{push();};
    updateExternalControls(); updateInfo(); push(); cab.setEnabled(false); enabled.setToggleState(false,juce::dontSendNotification);
}

void CabPage::loadSelectedFactoryIr()
{
    const int index = juce::jlimit(0, guitardsp::hq::FactoryIrCatalog::count - 1, factoryIr.getSelectedId() - 1);
    const auto file = guitardsp::hq::FactoryIrCatalog::fileForIndex(index);
    if (file.existsAsFile())
        cab.loadExternalImpulse(file);
}

void CabPage::chooseExternalIr()
{
    irChooser = std::make_unique<juce::FileChooser>("Load cabinet impulse response", juce::File{}, "*.wav;*.aif;*.aiff");
    irChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
        [this](const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (file.existsAsFile() && cab.loadExternalImpulse(file))
            {
                irEngine.setSelectedId(4, juce::dontSendNotification);
                updateExternalControls();
                updateInfo();
                repaint();
            }
            irChooser.reset();
        });
}

void CabPage::updateExternalControls()
{
    const int mode = irEngine.getSelectedId();
    const bool factory = mode == 3;
    const bool user = mode == 4;
    const bool measured = factory || user;
    loadIrButton.setVisible(user);
    loadIrButton.setEnabled(user);
    factoryIr.setVisible(factory);
    factoryIr.setEnabled(factory);
    irSize.setEnabled(measured);
    cabType.setEnabled(!measured);
    micType.setEnabled(!measured);
    position.setEnabled(!measured);
    distance.setEnabled(!measured);
    resonance.setEnabled(!measured);
    irName.setVisible(measured);
}

void CabPage::updateInfo()
{
    const int mode = irEngine.getSelectedId();
    if (mode == 3)
    {
        const int index = juce::jlimit(0, guitardsp::hq::FactoryIrCatalog::count - 1, factoryIr.getSelectedId() - 1);
        const auto file = guitardsp::hq::FactoryIrCatalog::fileForIndex(index);
        const juce::String sizeText = irSize.getSelectedId()==1 ? "1024" : (irSize.getSelectedId()==2 ? "2048" : "FULL");
        info.setText("FACTORY: measured Jester Dyne Brutal Pack capture, 48 kHz mono / 24-bit. Length=" + sizeText + ".", juce::dontSendNotification);
        irName.setText("IR: " + guitardsp::hq::FactoryIrCatalog::displayName(index) + (file.existsAsFile() ? "  [READY]" : "  [MISSING - rebuild with Factory IR fetch enabled]"), juce::dontSendNotification);
    }
    else if(mode == 4)
    {
        const juce::String sizeText = irSize.getSelectedId()==1 ? "1024" : (irSize.getSelectedId()==2 ? "2048" : "FULL");
        info.setText(juce::String("USER IR: WAV/AIFF is decoded and resampled by JUCE convolution. Mono path uses the file's first channel; length=") + sizeText + ".",juce::dontSendNotification);
        irName.setText("IR: " + cab.getExternalIrName(),juce::dontSendNotification);
    }
    else if(mode == 2)
    {
        info.setText("ADVANCED: multi-resonance + baffle/cone breakup + phase-aware early reflections. LOW VOL FEEL adds post-cab body/density for quiet playback.",juce::dontSendNotification);
        irName.setText({},juce::dontSendNotification);
    }
    else
    {
        info.setText("CLASSIC: original generated cabinet IR. LOW VOL FEEL is a separate post-cab compensation stage, so IR A/B remains valid.",juce::dontSendNotification);
        irName.setText({},juce::dontSendNotification);
    }
}

void CabPage::push()
{
    auto p=cab.getParameters();
    const int mode = irEngine.getSelectedId();
    p.irEngine = mode == 1 ? guitardsp::hq::CabIrEngine::classic
               : mode == 2 ? guitardsp::hq::CabIrEngine::advanced
                           : guitardsp::hq::CabIrEngine::external;
    p.externalIrSize=(guitardsp::hq::ExternalIrSize)juce::jlimit(0,2,irSize.getSelectedId()-1);
    p.cab=(guitardsp::hq::CabType)juce::jlimit(0,3,cabType.getSelectedId()-1);
    p.mic=(guitardsp::hq::MicType)juce::jlimit(0,2,micType.getSelectedId()-1);
    p.position=(float)position.getValue();p.distance=(float)distance.getValue();p.resonance=(float)resonance.getValue();
    p.lowCutHz=(float)lowCut.getValue();p.highCutHz=(float)highCut.getValue();p.mix=(float)mix.getValue();p.lowVolumeFeel=(float)lowVolumeFeel.getValue();
    p.irLevelDb=(float)irLevelDb.getValue();p.polarityInvert=polarityInvert.getToggleState();cab.setParameters(p);
    if (mode == 3)
        loadSelectedFactoryIr();
}

void CabPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12,16,21));g.setColour(juce::Colour::fromRGB(28,34,42));g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f),12.0f,1.0f);
    g.setColour(juce::Colour::fromRGB(158,167,178)); g.setFont(10.5f);
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel,&irLevelDb})
        g.drawText(s->getName(),s->getBounds().withHeight(15),juce::Justification::centredTop);
}

void CabPage::resized()
{
    auto r=getLocalBounds().reduced(24);title.setBounds(r.removeFromTop(44));info.setBounds(r.removeFromTop(42));
    auto top=r.removeFromTop(44);enabled.setBounds(top.removeFromLeft(135));irEngine.setBounds(top.removeFromLeft(145).reduced(4));cabType.setBounds(top.removeFromLeft(165).reduced(4));micType.setBounds(top.removeFromLeft(160).reduced(4));irSize.setBounds(top.removeFromLeft(105).reduced(4));polarityInvert.setBounds(top.removeFromLeft(95).reduced(3));
    auto irRow=r.removeFromTop(34);factoryIr.setBounds(irRow.removeFromLeft(275).reduced(3,3));loadIrButton.setBounds(irRow.removeFromLeft(105).reduced(3,3));irName.setBounds(irRow.reduced(6,0));
    auto knobs=r.removeFromTop(190);const int w=juce::jmax(82,knobs.getWidth()/8);
    for(auto* s:{&position,&distance,&resonance,&lowCut,&highCut,&mix,&lowVolumeFeel,&irLevelDb}) s->setBounds(knobs.removeFromLeft(w).reduced(3,8));
}
