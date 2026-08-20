#pragma once

#include <JuceHeader.h>
#include "../engine/AudioEngine.h"
#include "NavigationBar.h"
#include "pages/AmpPage.h"
#include "pages/PedalPage.h"
#include "pages/CabPage.h"
#include "pages/EffectsPage.h"
#include "pages/PerformancePage.h"
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

    juce::File getPresetDirectory() const;
    juce::File getPresetFile(const juce::String& name) const;
    void refreshPresetList(const juce::String& preferred = {});
    void saveCurrentPreset();
    void loadSelectedPreset();
    void deleteSelectedPreset();
    juce::var capturePreset() const;
    bool applyPreset(const juce::var& preset);
    void refreshPagesFromEngine();

    AudioEngine& audioEngine;
    NavigationBar navigation;

    AmpPage ampPage;
    PedalPage pedalPage;
    CabPage cabPage;
    EffectsPage effectsPage;
    PerformancePage performancePage;
    SettingsPage settingsPage;

    juce::Slider inputGainSlider;
    juce::Slider outputGainSlider;
    juce::ComboBox ampModeSelector;
    juce::ToggleButton bypassButton { "BYPASS" };
    juce::Label inputLabel;
    juce::Label outputLabel;
    juce::Label inputMeterLabel;
    juce::Label outputMeterLabel;

    juce::Label presetLabel;
    juce::ComboBox presetSelector;
    juce::TextButton presetSaveButton { "SAVE" };
    juce::TextButton presetDeleteButton { "DELETE" };
    bool refreshingPresets = false;

    juce::Component* visiblePage = nullptr;
};
