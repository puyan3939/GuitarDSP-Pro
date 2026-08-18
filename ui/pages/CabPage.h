#pragma once

#include <JuceHeader.h>

class CabPage : public juce::Component
{
public:
    CabPage();
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::Label title;
    juce::Label info;
};
