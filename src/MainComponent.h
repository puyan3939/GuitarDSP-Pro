#pragma once

#include <JuceHeader.h>
#include "../engine/AudioEngine.h"
#include "../ui/MainView.h"

class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;

private:
    class AnalyzerWindow;
    class MeasurementWindow;

    void toggleAnalyzerWindow();
    void setAnalyzerWindowVisible(bool shouldShow);
    void toggleMeasurementWindow();
    void setMeasurementWindowVisible(bool shouldShow);

    AudioEngine audioEngine;
    MainView mainView { audioEngine };
    juce::TextButton analyzerButton { "ANALYZER" };
    juce::TextButton measurementButton { "MEASURE LAB" };
    std::unique_ptr<AnalyzerWindow> analyzerWindow;
    std::unique_ptr<MeasurementWindow> measurementWindow;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};
