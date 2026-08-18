#pragma once

#include <JuceHeader.h>
#include "../../dsp/amp/AmpEngine.h"

class AmpPage : public juce::Component
{
public:
    explicit AmpPage(AmpEngine& engine);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void loadSelectedStage();
    void pushSelectedStage();

    AmpEngine& ampEngine;
    juce::Label title;
    juce::ComboBox stageSelector;
    juce::Label role;
    juce::Label listenFor;

    juce::Slider preHp, preLp, drive, bias, postLp, output, nonlinear, clipShape;
    bool updating = false;
};
