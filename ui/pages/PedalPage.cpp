#include "PedalPage.h"

PedalPage::PedalPage()
{
    title.setText("PEDAL", juce::dontSendNotification);
    title.setFont(juce::Font(28.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(title);

    info.setText("Phase 0 placeholder\nPedal DSP will be added after the amp core is stable.",
                 juce::dontSendNotification);
    info.setColour(juce::Label::textColourId, juce::Colour::fromRGB(170, 178, 188));
    info.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(info);
}

void PedalPage::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(12, 16, 21));
    g.setColour(juce::Colour::fromRGB(28, 34, 42));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(10.0f), 12.0f, 1.0f);
}

void PedalPage::resized()
{
    auto r = getLocalBounds().reduced(24);
    title.setBounds(r.removeFromTop(50));
    info.setBounds(r.reduced(20));
}
