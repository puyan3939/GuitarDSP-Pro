#pragma once

#include <JuceHeader.h>
#include "../../hq_preload/dsp/HQEffectsRack.h"

class EffectsPage : public juce::Component
{
public:
    explicit EffectsPage(guitardsp::hq::HQEffectsRack& rack);
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void pushMod();
    void pushDelay();
    void pushReverb();
    void updateModeHints();
    void setupKnob(juce::Slider& s, double lo, double hi, double value, double step, const juce::String& suffix = {});

    guitardsp::hq::HQEffectsRack& rack;

    juce::Label title;
    juce::Label modTitle, delayTitle, reverbTitle;
    juce::Label modHint, delayHint, reverbHint;

    juce::ComboBox modType;
    juce::Slider modRate, modDepth, modMix, modFeedback, modManual, modShape;

    juce::ToggleButton delayEnabled { "DELAY ON" };
    juce::ComboBox delayType;
    juce::Slider delayTime, delayFeedback, delayMix, delayLowCut, delayHighCut, delayDrive, delayWow, delayFlutter, delayAge;

    juce::ToggleButton reverbEnabled { "REVERB ON" };
    juce::ComboBox reverbType;
    juce::Slider reverbSize, reverbDecay, reverbDamping, reverbPreDelay, reverbMix, reverbMod, reverbDrip;
};
