#include "MainView.h"

MainView::MainView(AudioEngine& engine)
    : audioEngine(engine),
      ampPage(engine.getAmpEngine()),
      settingsPage(engine.getDeviceManager())
{
    addAndMakeVisible(navigation);
    addAndMakeVisible(ampPage);
    addChildComponent(pedalPage);
    addChildComponent(cabPage);
    addChildComponent(effectsPage);
    addChildComponent(settingsPage);

    navigation.onPageSelected = [this](NavigationBar::Page page)
    {
        showPage(page);
    };

    auto configureGainSlider = [](juce::Slider& slider, double min, double max, double initial)
    {
        slider.setSliderStyle(juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 70, 24);
        slider.setRange(min, max, 0.1);
        slider.setValue(initial, juce::dontSendNotification);
        slider.setNumDecimalPlacesToDisplay(1);
        slider.setTextValueSuffix(" dB");
    };

    configureGainSlider(inputGainSlider, -36.0, 18.0, -6.0);
    configureGainSlider(outputGainSlider, -60.0, 6.0, -18.0);

    inputLabel.setText("INPUT", juce::dontSendNotification);
    outputLabel.setText("OUTPUT", juce::dontSendNotification);
    inputMeterLabel.setText("-100 dB", juce::dontSendNotification);
    outputMeterLabel.setText("-100 dB", juce::dontSendNotification);

    inputLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(165, 174, 184));
    outputLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(165, 174, 184));
    inputMeterLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(98, 213, 167));
    outputMeterLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(98, 213, 167));

    addAndMakeVisible(inputGainSlider);
    addAndMakeVisible(outputGainSlider);
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(inputLabel);
    addAndMakeVisible(outputLabel);
    addAndMakeVisible(inputMeterLabel);
    addAndMakeVisible(outputMeterLabel);

    inputGainSlider.onValueChange = [this]
    {
        audioEngine.setInputGainDb((float) inputGainSlider.getValue());
    };

    outputGainSlider.onValueChange = [this]
    {
        audioEngine.setOutputGainDb((float) outputGainSlider.getValue());
    };

    bypassButton.onClick = [this]
    {
        audioEngine.setBypass(bypassButton.getToggleState());
    };

    audioEngine.setInputGainDb((float) inputGainSlider.getValue());
    audioEngine.setOutputGainDb((float) outputGainSlider.getValue());

    showPage(NavigationBar::Page::amp);
    startTimerHz(15);
}

MainView::~MainView()
{
    stopTimer();
}

void MainView::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(9, 12, 16));

    auto r = getLocalBounds();
    auto header = r.removeFromTop(76);
    g.setColour(juce::Colour::fromRGB(14, 18, 24));
    g.fillRect(header);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(24.0f, juce::Font::bold));
    g.drawText("GuitarDSP-Pro", header.removeFromLeft(250).reduced(20, 0), juce::Justification::centredLeft);

    g.setColour(juce::Colour::fromRGB(222, 110, 58));
    g.setFont(juce::Font(13.0f));
    g.drawText("PHASE 0 / SAFE SHELL", header.removeFromLeft(190), juce::Justification::centredLeft);
}

void MainView::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop(76);

    auto controls = header.removeFromRight(760).reduced(10, 8);
    inputLabel.setBounds(controls.removeFromLeft(55));
    inputGainSlider.setBounds(controls.removeFromLeft(190).reduced(4, 10));
    inputMeterLabel.setBounds(controls.removeFromLeft(75));
    controls.removeFromLeft(8);
    outputLabel.setBounds(controls.removeFromLeft(62));
    outputGainSlider.setBounds(controls.removeFromLeft(190).reduced(4, 10));
    outputMeterLabel.setBounds(controls.removeFromLeft(75));
    bypassButton.setBounds(controls.removeFromLeft(90).reduced(6, 10));

    navigation.setBounds(r.removeFromTop(54));

    if (visiblePage != nullptr)
        visiblePage->setBounds(r.reduced(14));
}

void MainView::showPage(NavigationBar::Page page)
{
    juce::Component* next = nullptr;

    switch (page)
    {
        case NavigationBar::Page::amp:      next = &ampPage; break;
        case NavigationBar::Page::pedal:    next = &pedalPage; break;
        case NavigationBar::Page::cab:      next = &cabPage; break;
        case NavigationBar::Page::effects:  next = &effectsPage; break;
        case NavigationBar::Page::settings: next = &settingsPage; break;
    }

    if (visiblePage != nullptr)
        visiblePage->setVisible(false);

    visiblePage = next;
    if (visiblePage != nullptr)
    {
        visiblePage->setVisible(true);
        visiblePage->toFront(false);
    }
    resized();
}

void MainView::timerCallback()
{
    const float inDb = juce::jmax(audioEngine.getInputPeak(0), audioEngine.getInputPeak(1));
    const float outDb = juce::jmax(audioEngine.getOutputPeak(0), audioEngine.getOutputPeak(1));
    inputMeterLabel.setText(juce::String(inDb, 1) + " dB", juce::dontSendNotification);
    outputMeterLabel.setText(juce::String(outDb, 1) + " dB", juce::dontSendNotification);
}
