#include "MainView.h"

MainView::MainView(AudioEngine& engine)
    : audioEngine(engine),
      ampPage(engine.getAmpEngine()),
      pedalPage(engine.getHQEffectsRack()),
      cabPage(engine.getCabMicEngine()),
      effectsPage(engine.getHQEffectsRack()),
      settingsPage(engine.getDeviceManager())
{
    addAndMakeVisible(navigation);
    addAndMakeVisible(ampPage);
    addChildComponent(pedalPage);
    addChildComponent(cabPage);
    addChildComponent(effectsPage);
    addChildComponent(settingsPage);

    navigation.onPageSelected = [this](NavigationBar::Page page){ showPage(page); };

    auto configureGainSlider = [](juce::Slider& slider, double min, double max, double initial)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 64, 24);
        slider.setRange(min, max, 0.1);
        slider.setValue(initial, juce::dontSendNotification);
        slider.setNumDecimalPlacesToDisplay(1);
        slider.setTextValueSuffix(" dB");
    };

    configureGainSlider(inputGainSlider, -36.0, 18.0, -6.0);
    configureGainSlider(outputGainSlider, -60.0, 6.0, -18.0);

    ampModeSelector.addItem("LEGACY 20", 1);
    ampModeSelector.addItem("HQ 20", 2);
    ampModeSelector.setSelectedId(1, juce::dontSendNotification);
    ampModeSelector.setTooltip("A/B the editable 20-stage amp and the higher-detail HQ model");

    inputLabel.setText("INPUT", juce::dontSendNotification);
    outputLabel.setText("OUTPUT", juce::dontSendNotification);
    inputMeterLabel.setText("-100 dB", juce::dontSendNotification);
    outputMeterLabel.setText("-100 dB", juce::dontSendNotification);
    inputLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(165,174,184));
    outputLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(165,174,184));
    inputMeterLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(98,213,167));
    outputMeterLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(98,213,167));

    addAndMakeVisible(inputGainSlider);addAndMakeVisible(outputGainSlider);addAndMakeVisible(ampModeSelector);addAndMakeVisible(bypassButton);
    addAndMakeVisible(inputLabel);addAndMakeVisible(outputLabel);addAndMakeVisible(inputMeterLabel);addAndMakeVisible(outputMeterLabel);

    inputGainSlider.onValueChange=[this]{audioEngine.setInputGainDb((float)inputGainSlider.getValue());};
    outputGainSlider.onValueChange=[this]{audioEngine.setOutputGainDb((float)outputGainSlider.getValue());};
    ampModeSelector.onChange=[this]{audioEngine.setAmpMode(ampModeSelector.getSelectedId()==2?SignalChain::AmpMode::hq:SignalChain::AmpMode::legacy);};
    bypassButton.onClick=[this]{audioEngine.setBypass(bypassButton.getToggleState());};

    audioEngine.setInputGainDb((float)inputGainSlider.getValue());audioEngine.setOutputGainDb((float)outputGainSlider.getValue());audioEngine.setAmpMode(SignalChain::AmpMode::legacy);
    showPage(NavigationBar::Page::amp);startTimerHz(15);
}

MainView::~MainView(){stopTimer();}
void MainView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(9,12,16));auto r=getLocalBounds();auto header=r.removeFromTop(76);g.setColour(juce::Colour::fromRGB(14,18,24));g.fillRect(header);
    g.setColour(juce::Colours::white);g.setFont(juce::Font(24.0f,juce::Font::bold));g.drawText("GuitarDSP-Pro",header.removeFromLeft(250).reduced(20,0),juce::Justification::centredLeft);
    g.setColour(juce::Colour::fromRGB(222,110,58));g.setFont(juce::Font(13.0f));g.drawText("AMP LAB / SAFE OUTPUT",header.removeFromLeft(190),juce::Justification::centredLeft);
}
void MainView::resized()
{
    auto r=getLocalBounds();auto header=r.removeFromTop(76);auto controls=header.removeFromRight(845).reduced(8,8);
    inputLabel.setBounds(controls.removeFromLeft(48));inputGainSlider.setBounds(controls.removeFromLeft(170).reduced(3,10));inputMeterLabel.setBounds(controls.removeFromLeft(70));controls.removeFromLeft(5);
    outputLabel.setBounds(controls.removeFromLeft(58));outputGainSlider.setBounds(controls.removeFromLeft(170).reduced(3,10));outputMeterLabel.setBounds(controls.removeFromLeft(70));ampModeSelector.setBounds(controls.removeFromLeft(135).reduced(4,10));bypassButton.setBounds(controls.removeFromLeft(90).reduced(5,10));
    navigation.setBounds(r.removeFromTop(54));if(visiblePage!=nullptr)visiblePage->setBounds(r.reduced(14));
}
void MainView::showPage(NavigationBar::Page page)
{
    juce::Component* next=nullptr;switch(page){case NavigationBar::Page::amp:next=&ampPage;break;case NavigationBar::Page::pedal:next=&pedalPage;break;case NavigationBar::Page::cab:next=&cabPage;break;case NavigationBar::Page::effects:next=&effectsPage;break;case NavigationBar::Page::settings:next=&settingsPage;break;}
    if(visiblePage!=nullptr)visiblePage->setVisible(false);visiblePage=next;if(visiblePage!=nullptr){visiblePage->setVisible(true);visiblePage->toFront(false);}resized();
}
void MainView::timerCallback()
{
    const float inDb=juce::jmax(audioEngine.getInputPeak(0),audioEngine.getInputPeak(1));const float outDb=juce::jmax(audioEngine.getOutputPeak(0),audioEngine.getOutputPeak(1));
    inputMeterLabel.setText(juce::String(inDb,1)+" dB",juce::dontSendNotification);outputMeterLabel.setText(juce::String(outDb,1)+" dB",juce::dontSendNotification);
}
