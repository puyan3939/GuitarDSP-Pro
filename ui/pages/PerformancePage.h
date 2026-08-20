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
    void setupKnob(juce::Slider&,const juce::String&,double,double,double,double,const juce::String& ={});
    void pushInput(); void pushPitch(); void pushRigDetail(); void pushDualDelay(); void captureScene(); void recallScene();
    AudioEngine& engine; bool updating=false;
    juce::Label title,inputTitle,pitchTitle,rigTitle,delayTitle,sceneTitle;
    juce::ToggleButton inputEnabled{"INPUT LOAD"}; juce::Slider pickupR,pickupL,cableC,inputZ,inputTrim;
    juce::ToggleButton pitchEnabled{"WHAMMY"},pitchMain{"MAIN"},pitchClean{"CLEAN"},pitchSub{"SUB"};
    juce::Slider pitchSemitones,pitchExpression,pitchWet,pitchDry,pitchTracking,pitchTone,pitchSmooth;
    juce::ToggleButton rigEnabled{"MULTI-RIG"},cleanEnabled{"CLEAN"},subEnabled{"SUB"},autoLatency{"AUTO LATENCY"},cleanInvert{"CLEAN INV"},subInvert{"SUB INV"};
    juce::Slider mainLevel,mainDelay,cleanLevel,cleanHp,cleanLp,cleanPresence,cleanDrive,cleanBass,cleanMid,cleanTreble,cleanDelay;
    juce::Slider subLevel,subHp,subLp,subBody,subDrive,subBass,subMid,subTreble,subTracking,subTone,subSmooth,subDelay;
    juce::ToggleButton dualDelayEnabled{"DUAL DELAY"}; juce::Slider delayL,delayR,feedbackL,feedbackR,crossFeedback,delayMix,delayLowCut,delayHighCut,delayModRate,delayModDepth;
    juce::ComboBox outputMode,sceneSelector; juce::TextButton sceneCapture{"CAPTURE"},sceneRecall{"RECALL"};
};
