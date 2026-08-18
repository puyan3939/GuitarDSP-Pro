#pragma once
#include <JuceHeader.h>
#include "../../hq_preload/dsp/cab/CabMicEngineHQ.h"

class CabPage : public juce::Component
{
public:
    explicit CabPage(guitardsp::hq::CabMicEngineHQ& engine);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void push();
    guitardsp::hq::CabMicEngineHQ& cab;
    juce::Label title, info;
    juce::ToggleButton enabled { "CAB / MIC ON" };
    juce::ComboBox cabType, micType;
    juce::Slider position, distance, resonance, lowCut, highCut, mix;
};
