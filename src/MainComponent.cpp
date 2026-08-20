#include "MainComponent.h"

MainComponent::MainComponent()
{
    audioEngine.initialise();
    setOpaque(true);
    setSize(1280, 720);
    addAndMakeVisible(mainView);

    analyzerButton.setTooltip("Open DSP Analyzer in a separate window");
    analyzerButton.onClick = [this] { openAnalyzer(); };
    addAndMakeVisible(analyzerButton);
    analyzerButton.toFront(false);
}

MainComponent::~MainComponent()
{
    analyzerWindow.reset();
    audioEngine.shutdown();
}

void MainComponent::openAnalyzer()
{
    if (analyzerWindow == nullptr)
        analyzerWindow = std::make_unique<AnalyzerWindow>(audioEngine);

    analyzerWindow->setVisible(true);
    analyzerWindow->toFront(true);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(9, 12, 16));
}

void MainComponent::resized()
{
    mainView.setBounds(getLocalBounds());

    // Keep the launcher in the preset/header strip so it never covers amp/pedal knobs.
    const int buttonWidth = 110;
    analyzerButton.setBounds(juce::jmax(8, getWidth() - buttonWidth - 16), 82, buttonWidth, 26);
    analyzerButton.toFront(false);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (auto* top = findParentComponentOfClass<juce::DocumentWindow>())
            top->setVisible(false);
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
        return true;
    }

    return juce::Component::keyPressed(key);
}
