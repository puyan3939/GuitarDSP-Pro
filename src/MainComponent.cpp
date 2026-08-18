#include "MainComponent.h"

MainComponent::MainComponent()
{
    setOpaque(true);
    setSize(1280, 720);
    addAndMakeVisible(mainView);
}

MainComponent::~MainComponent() = default;

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(9, 12, 16));
}

void MainComponent::resized()
{
    mainView.setBounds(getLocalBounds());
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
