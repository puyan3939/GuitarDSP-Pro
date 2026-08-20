#pragma once

#include <JuceHeader.h>
#include "../../engine/AudioEngine.h"

class PerformancePage : public juce::Component
{
public:
    explicit PerformancePage(AudioEngine& engine);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromEngine();

private:
    void setupKnob(juce::Slider&, const juce::String& name, double lo, double hi, double value, double step, const juce::String& suffix = {});
    void pushInput();
    void pushPitch();
    void pushRigDetail();
    void pushDualDelay();
    void captureScene();
    void recallScene();

    AudioEngine& engine;
    bool updating = false;

    juce::Label title;
    juce::Label inputTitle, pitchTitle, rigTitle, delayTitle, sceneTitle;

    juce::ToggleButton inputEnabled { "INPUT LOAD" };
    juce::Slider pickupR, pickupL, cableC, inputZ, inputTrim;

    juce::ToggleButton pitchEnabled { "WHAMMY" };
    juce::ToggleButton pitchMain { "MAIN" }, pitchClean { "CLEAN" }, pitchSub { "SUB" };
    juce::Slider pitchSemitones, pitchExpression, pitchWet, pitchDry, pitchTracking, pitchTone, pitchSmooth;

    juce::ToggleButton rigEnabled { "MULTI-RIG" };
    juce::ToggleButton cleanEnabled { "CLEAN" }, subEnabled { "SUB" }, autoLatency { "AUTO LATENCY" };
    juce::Slider mainLevel, cleanLevel, subLevel, cleanBass, cleanMid, cleanTreble, subBass, subMid, subTreble;

    juce::ToggleButton dualDelayEnabled { "DUAL DELAY" };
    juce::Slider delayL, delayR, feedbackL, feedbackR, crossFeedback, delayMix, delayLowCut, delayHighCut, delayModRate, delayModDepth;

    juce::ComboBox outputMode;
    juce::ComboBox sceneSelector;
    juce::TextButton sceneCapture { "CAPTURE" };
    juce::TextButton sceneRecall { "RECALL" };
};
