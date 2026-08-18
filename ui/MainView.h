#pragma once

#include <JuceHeader.h>
#include "../engine/AudioEngine.h"
#include "NavigationBar.h"
#include "pages/AmpPage.h"
#include "pages/PedalPage.h"
#include "pages/CabPage.h"
#include "pages/EffectsPage.h"
#include "pages/SettingsPage.h"

class MainView : public juce::Component,
                 private juce::Timer
{
public:
    explicit MainView(AudioEngine& engine);
    ~MainView() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void showPage(NavigationBar::Page page);
    void timerCallback() override;

    AudioEngine& audioEngine;
    NavigationBar navigation;

    AmpPage ampPage;
    PedalPage pedalPage;
    CabPage cabPage;
    EffectsPage effectsPage;
    SettingsPage settingsPage;

    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::ComboBox ampModeSelector;
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;

    juce::Component* visiblePage = nullptr;
};
