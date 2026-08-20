#include "MainComponent.h"
#include "../ui/SignalAnalyzerPanel.h"
#include "../ui/MeasurementLabPanel.h"

class MainComponent::AnalyzerWindow : public juce::DocumentWindow
{
public:
    AnalyzerWindow(AudioEngine& engine, std::function<void()> onClosed)
        : juce::DocumentWindow("DSP Analyzer", juce::Colour::fromRGB(10, 13, 18), juce::DocumentWindow::allButtons),
          onClosedCallback(std::move(onClosed))
    {
        setUsingNativeTitleBar(true); setResizable(true, true); setResizeLimits(760, 480, 2400, 1600);
        analyzerPanel = new SignalAnalyzerPanel(engine); analyzerPanel->setAnalyzerActive(false); setContentOwned(analyzerPanel, true);
        centreWithSize(1180, 700); setVisible(false);
    }

    ~AnalyzerWindow() override { if (analyzerPanel != nullptr) analyzerPanel->setAnalyzerActive(false); }
    void showAnalyzer() { if (analyzerPanel != nullptr) analyzerPanel->setAnalyzerActive(true); setVisible(true); toFront(true); }
    void hideAnalyzer() { if (analyzerPanel != nullptr) analyzerPanel->setAnalyzerActive(false); setVisible(false); if (onClosedCallback) onClosedCallback(); }
    void closeButtonPressed() override { hideAnalyzer(); }

private:
    SignalAnalyzerPanel* analyzerPanel = nullptr;
    std::function<void()> onClosedCallback;
};

class MainComponent::MeasurementWindow : public juce::DocumentWindow
{
public:
    MeasurementWindow(AudioEngine& engine, std::function<void()> onClosed)
        : juce::DocumentWindow("Measurement Lab", juce::Colour::fromRGB(10, 13, 18), juce::DocumentWindow::allButtons),
          onClosedCallback(std::move(onClosed))
    {
        setUsingNativeTitleBar(true); setResizable(true, true); setResizeLimits(700, 440, 1400, 1000);
        setContentOwned(new MeasurementLabPanel(engine), true); centreWithSize(900, 540); setVisible(false);
    }

    void showLab() { setVisible(true); toFront(true); }
    void hideLab() { setVisible(false); if (onClosedCallback) onClosedCallback(); }
    void closeButtonPressed() override { hideLab(); }

private:
    std::function<void()> onClosedCallback;
};

MainComponent::MainComponent()
{
    audioEngine.initialise(); setOpaque(true); setSize(1280, 720); addAndMakeVisible(mainView); addAndMakeVisible(analyzerButton); addAndMakeVisible(measurementButton);
    for (auto* button : { &analyzerButton, &measurementButton })
    {
        button->setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(26, 31, 38));
        button->setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(222, 110, 58));
    }
    analyzerButton.setTooltip("Open dual oscilloscope / FFT analyzer in a separate window");
    measurementButton.setTooltip("Open hardware latency and oversampling measurement tools");
    analyzerButton.onClick = [this] { toggleAnalyzerWindow(); };
    measurementButton.onClick = [this] { toggleMeasurementWindow(); };
    audioEngine.setLiveAnalyzerEnabled(false);
}

MainComponent::~MainComponent()
{
    measurementWindow.reset(); analyzerWindow.reset(); audioEngine.shutdown();
}

void MainComponent::toggleAnalyzerWindow()
{
    const bool shouldShow = analyzerWindow == nullptr || !analyzerWindow->isVisible(); setAnalyzerWindowVisible(shouldShow);
}

void MainComponent::setAnalyzerWindowVisible(bool shouldShow)
{
    if (shouldShow)
    {
        if (analyzerWindow == nullptr)
        {
            analyzerWindow = std::make_unique<AnalyzerWindow>(audioEngine, [this]
            {
                audioEngine.setLiveAnalyzerEnabled(false); analyzerButton.setButtonText("ANALYZER");
            });
        }
        audioEngine.setLiveAnalyzerEnabled(true); analyzerWindow->showAnalyzer(); analyzerButton.setButtonText("CLOSE ANALYZER");
    }
    else if (analyzerWindow != nullptr) analyzerWindow->hideAnalyzer();
}

void MainComponent::toggleMeasurementWindow()
{
    const bool shouldShow = measurementWindow == nullptr || !measurementWindow->isVisible(); setMeasurementWindowVisible(shouldShow);
}

void MainComponent::setMeasurementWindowVisible(bool shouldShow)
{
    if (shouldShow)
    {
        if (measurementWindow == nullptr)
        {
            measurementWindow = std::make_unique<MeasurementWindow>(audioEngine, [this] { measurementButton.setButtonText("MEASURE LAB"); });
        }
        measurementWindow->showLab(); measurementButton.setButtonText("CLOSE LAB");
    }
    else if (measurementWindow != nullptr) measurementWindow->hideLab();
}

void MainComponent::paint(juce::Graphics& g) { g.fillAll(juce::Colour::fromRGB(9, 12, 16)); }

void MainComponent::resized()
{
    mainView.setBounds(getLocalBounds());
    constexpr int analyzerWidth = 126, measureWidth = 126, buttonHeight = 28, margin = 10;
    analyzerButton.setBounds(getWidth() - analyzerWidth - margin, getHeight() - buttonHeight - margin, analyzerWidth, buttonHeight);
    measurementButton.setBounds(getWidth() - analyzerWidth - measureWidth - margin * 2, getHeight() - buttonHeight - margin, measureWidth, buttonHeight);
    analyzerButton.toFront(false); measurementButton.toFront(false);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (auto* top = findParentComponentOfClass<juce::DocumentWindow>()) top->setVisible(false);
        juce::JUCEApplication::getInstance()->systemRequestedQuit(); return true;
    }
    return juce::Component::keyPressed(key);
}
