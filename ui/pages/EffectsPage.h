#pragma once

#include <JuceHeader.h>

class EffectsPage : public juce::Component
{
public:
    EffectsPage();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title;
    juce::Label info;
};
