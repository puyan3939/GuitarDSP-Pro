#pragma once

#include <JuceHeader.h>
#include <memory>

class SettingsPage : public juce::Component
{
public:
    explicit SettingsPage(juce::AudioDeviceManager& manager);
    ~SettingsPage() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::AudioDeviceManager& deviceManager;
    juce::Label title;
    juce::Label requestedSettings;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> deviceSelector;
};
