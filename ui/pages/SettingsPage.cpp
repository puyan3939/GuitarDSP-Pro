#include "SettingsPage.h"

SettingsPage::SettingsPage(juce::AudioDeviceManager& manager)
    : deviceManager(manager)
{
    title.setText("SETTINGS / AUDIO", juce::dontSendNotification);
    title.setFont(juce::Font(28.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    deviceSelector = std::make_unique<juce::AudioDeviceSelectorComponent>(
        deviceManager,
        1, 1,
        2, 2,
        false,
        false,
        true,
        false);

    addAndMakeVisible(*deviceSelector);

    requestedSettings.setText("Requested target: 48 kHz / 256 samples\n"
                              "If the WAVIO driver does not expose those values, select the closest stable option.",
                              juce::dontSendNotification);
    requestedSettings.setColour(juce::Label::textColourId, juce::Colour::fromRGB(170, 178, 188));
    requestedSettings.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(requestedSettings);
}

SettingsPage::~SettingsPage() = default;

void SettingsPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12, 16, 21));
    g.setColour(juce::Colour::fromRGB(28, 34, 42));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f), 12.0f, 1.0f);
}

void SettingsPage::resized()
{
    auto r = getLocalBounds().reduced(24);
    title.setBounds(r.removeFromTop(46));
    requestedSettings.setBounds(r.removeFromTop(58));
    deviceSelector->setBounds(r.reduced(0, 8));
}
