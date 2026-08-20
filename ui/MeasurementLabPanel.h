#pragma once
#include <JuceHeader.h>
#include "../engine/AudioEngine.h"

class MeasurementLabPanel : public juce::Component,
                            private juce::Timer
{
public:
    explicit MeasurementLabPanel(AudioEngine& engine);
    ~MeasurementLabPanel() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshDspLatency();
    void refreshLabels();

    AudioEngine& audioEngine;
    juce::Label title;
    juce::Label instructions;
    juce::Label deviceInfo;
    juce::Label dspInfo;
    juce::Label resultInfo;
    juce::Label osInfo;
    juce::TextButton measureButton { "MEASURE LOOPBACK" };
    juce::TextButton restoreButton { "RESTORE AUDIO" };
    juce::TextButton refreshDspButton { "MEASURE DSP IMPULSE" };
    int dspLatencySamples = -1;
};
