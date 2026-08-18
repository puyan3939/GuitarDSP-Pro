#pragma once

#include <JuceHeader.h>

class PedalPage : public juce::Component
{
public:
    PedalPage();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title;
    juce::Label info;
};
