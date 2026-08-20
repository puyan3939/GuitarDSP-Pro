#pragma once

#include <JuceHeader.h>
#include "../../hq_preload/dsp/HQEffectsRack.h"

class PedalPage : public juce::Component,
                  private juce::Timer
{
public:
    explicit PedalPage(guitardsp::hq::HQEffectsRack& rack);
    ~PedalPage() override { stopTimer(); }
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromEngine();

private:
    void timerCallback() override;
    void loadSlot();
    void pushControls();
    void pushGateControls();
    void pushStudioCompControls();
    void pushGuitarCompControls();
    void updateModelLabels();
    void updateDynamicsVisibility();
    void setupKnob(juce::Slider& slider, const juce::String& name,
                   double min, double max, double step);

    guitardsp::hq::HQEffectsRack& effectsRack;
    bool updating = false;

    juce::Label title;
    juce::Label description;
    juce::ComboBox slotSelector;
    juce::ComboBox modelSelector;
    juce::ToggleButton enabledButton { "ON" };
    juce::ToggleButton routeMain { "MAIN" }, routeClean { "CLEAN" }, routeSub { "SUB" };

    juce::Label dynamicsTitle, dynamicsHint, grLabel;
    juce::ComboBox dynamicsMode;
    juce::Slider gateThreshold, gateRange, gateRatio, gateAttack, gateHold,
                 gateRelease, gateHysteresis, gateSidechainHp, gateSidechainLp;

    juce::Slider compThreshold, compRatio, compAttack, compRelease,
                 compKnee, compMakeup, compMix, compSidechainHp;
    juce::ToggleButton compRms { "RMS" };
    juce::ToggleButton compAutoRelease { "AUTO REL" };
    juce::ToggleButton compAutoMakeup { "AUTO MAKEUP" };

    juce::Slider guitarSustain, guitarAttack, guitarBlend, guitarLevel;

    juce::Slider drive;
    juce::Slider tone;
    juce::Slider level;
    juce::Slider blend;
    juce::Slider aux1;
    juce::Slider aux2;
    juce::Slider aux3;
};
