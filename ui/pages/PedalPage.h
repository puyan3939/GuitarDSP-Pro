#pragma once

#include <JuceHeader.h>
#include "../../hq_preload/dsp/HQEffectsRack.h"

class PedalPage : public juce::Component
{
public:
    explicit PedalPage(guitardsp::hq::HQEffectsRack& rack);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromEngine()
    {
        auto& gc=effectsRack.gateControl();
        gateThreshold.setValue(gc.thresholdDb.load(),juce::dontSendNotification);
        gateRange.setValue(gc.rangeDb.load(),juce::dontSendNotification);
        gateRatio.setValue(gc.ratio.load(),juce::dontSendNotification);
        gateAttack.setValue(gc.attackMs.load(),juce::dontSendNotification);
        gateHold.setValue(gc.holdMs.load(),juce::dontSendNotification);
        gateRelease.setValue(gc.releaseMs.load(),juce::dontSendNotification);
        gateHysteresis.setValue(gc.hysteresisDb.load(),juce::dontSendNotification);
        gateSidechainHp.setValue(gc.sidechainHpHz.load(),juce::dontSendNotification);
        gateSidechainLp.setValue(gc.sidechainLpHz.load(),juce::dontSendNotification);
        gateEnabled.setToggleState(effectsRack.getDynamicsMode()==guitardsp::hq::HQEffectsRack::DynamicsMode::gate,juce::dontSendNotification);
        loadSlot();
    }

private:
    void loadSlot();
    void pushControls();
    void pushGateControls();
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

    juce::ToggleButton gateEnabled { "PRECISION GATE" };
    juce::Slider gateThreshold, gateRange, gateRatio, gateAttack, gateHold,
                 gateRelease, gateHysteresis, gateSidechainHp, gateSidechainLp;

    juce::Slider drive;
    juce::Slider tone;
    juce::Slider level;
    juce::Slider blend;
    juce::Slider aux1;
    juce::Slider aux2;
    juce::Slider aux3;
};
