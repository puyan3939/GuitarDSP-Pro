#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "../engine/AudioEngine.h"

class SignalAnalyzerPanel : public juce::Component,
                            private juce::Timer
{
public:
    explicit SignalAnalyzerPanel(AudioEngine& engine);
    ~SignalAnalyzerPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void setAnalyzerActive(bool active)
    {
        if (active)
        {
            startTimerHz(15);
            updateData();
            repaint();
        }
        else
        {
            stopTimer();
        }
    }

private:
    void timerCallback() override;
    void updateData();
    void refreshControlVisibility();
    void applyAmpReference();
    void drawMonitor(juce::Graphics& g,
                     juce::Rectangle<float> bounds,
                     const juce::String& name,
                     const std::vector<float>& samples,
                     float verticalScale,
                     bool showThd);
    void drawOscilloscope(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          const std::vector<float>& samples,
                          float verticalScale) const;
    void drawSpectrum(juce::Graphics& g,
                      juce::Rectangle<float> bounds,
                      const std::vector<float>& samples) const;
    juce::String makeStatsText(const std::vector<float>& samples, bool showThd) const;
    float calculateThd(const std::vector<float>& samples, float fundamentalHz) const;
    static SignalTapBuffer::TapPoint tapForSelectorId(int id) noexcept;
    static juce::String tapName(int id);

    AudioEngine& audioEngine;
    juce::ComboBox modeSelector, sourceASelector, sourceBSelector, viewSelector, scaleSelector, timeSelector, ampReferenceSelector;
    juce::Slider frequencySlider, levelSlider;
    juce::ToggleButton holdButton { "HOLD" };
    juce::Label titleLabel, frequencyLabel, levelLabel, sweepLabel, ampReferenceLabel;
    std::vector<float> samplesA, samplesB;
    juce::Rectangle<float> monitorABounds, monitorBBounds;
    mutable juce::dsp::FFT fft { 11 };
    mutable juce::dsp::WindowingFunction<float> fftWindow { 2048, juce::dsp::WindowingFunction<float>::hann };
    guitardsp::hq::AmpHQParams customAmpSnapshot;
    bool customAmpSnapshotValid = false;
    bool referenceApplied = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SignalAnalyzerPanel)
};
