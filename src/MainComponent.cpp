#include "MainComponent.h"

MainComponent::MainComponent()
{
    audioEngine.initialise();
    setOpaque(true);
    setSize(1280, 720);
    addAndMakeVisible(mainView);
    addAndMakeVisible(analyzerButton);
    addChildComponent(analyzerPanel);

    analyzerButton.setColour(juce::TextButton::buttonColourId, juce::Colour::fromRGB(26, 31, 38));
    analyzerButton.setColour(juce::TextButton::textColourOffId, juce::Colour::fromRGB(222, 110, 58));
    analyzerButton.setTooltip("Open dual oscilloscope / FFT analyzer");
    analyzerButton.onClick = [this]
    {
        const bool shouldShow = !analyzerPanel.isVisible();
        analyzerPanel.setVisible(shouldShow);
        analyzerButton.setButtonText(shouldShow ? "CLOSE ANALYZER" : "ANALYZER");
        analyzerPanel.toFront(false);
        analyzerButton.toFront(false);
        resized();
    };
}

MainComponent::~MainComponent()
{
    audioEngine.shutdown();
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(9, 12, 16));
}

void MainComponent::resized()
{
    mainView.setBounds(getLocalBounds());

    constexpr int panelHeight = 360;
    constexpr int buttonWidth = 126;
    constexpr int buttonHeight = 28;
    constexpr int margin = 10;

    if (analyzerPanel.isVisible())
    {
        analyzerPanel.setBounds(margin,
                                juce::jmax(margin, getHeight() - panelHeight - margin),
                                juce::jmax(200, getWidth() - margin * 2),
                                juce::jmin(panelHeight, getHeight() - margin * 2));
        analyzerButton.setBounds(getWidth() - buttonWidth - margin,
                                 juce::jmax(margin, analyzerPanel.getY() - buttonHeight - 5),
                                 buttonWidth,
                                 buttonHeight);
    }
    else
    {
        analyzerButton.setBounds(getWidth() - buttonWidth - margin,
                                 getHeight() - buttonHeight - margin,
                                 buttonWidth,
                                 buttonHeight);
    }

    analyzerPanel.toFront(false);
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
