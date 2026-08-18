#pragma once

#include <JuceHeader.h>
#include "../../hq_preload/dsp/HQEffectsRack.h"

class PedalPage : public juce::Component
{
public:
    explicit PedalPage(guitardsp::hq::HQEffectsRack& rack);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void loadSlot();
    void pushControls();
    void updateModelLabels();
    void setupKnob(juce::Slider& slider, const juce::String& name,
                   double min, double max, double step);

    guitardsp::hq::HQEffectsRack& effectsRack;
    bool updating = false;

    juce::Label title;
    juce::Label description;
    juce::ComboBox slotSelector;
    juce::ComboBox modelSelector;
    juce::ToggleButton enabledButton { "ON" };

    juce::Slider drive;
    juce::Slider tone;
    juce::Slider level;
    juce::Slider blend;
    juce::Slider aux1;
    juce::Slider aux2;
    juce::Slider aux3;
};
