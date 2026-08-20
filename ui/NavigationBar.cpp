#include "NavigationBar.h"

NavigationBar::NavigationBar()
{
    configureButton(ampButton, "AMP", Page::amp);
    configureButton(pedalButton, "PEDAL", Page::pedal);
    configureButton(cabButton, "CAB", Page::cab);
    configureButton(effectsButton, "FX", Page::effects);
    configureButton(performanceButton, "RIG", Page::performance);
    configureButton(settingsButton, "SETTINGS", Page::settings);
    refreshColours();
}

void NavigationBar::configureButton(juce::TextButton& button, const juce::String& text, Page page)
{
    button.setButtonText(text);
    button.setClickingTogglesState(false);
    button.onClick = [this, page]
    {
        setCurrentPage(page);
        if (onPageSelected)
            onPageSelected(page);
    };
    addAndMakeVisible(button);
}

void NavigationBar::setCurrentPage(Page page)
{
    currentPage = page;
    refreshColours();
}

void NavigationBar::refreshColours()
{
    auto apply = [this](juce::TextButton& button, Page page)
    {
        const bool selected = page == currentPage;
        button.setColour(juce::TextButton::buttonColourId,
                         selected ? juce::Colour::fromRGB(222, 110, 58)
                                  : juce::Colour::fromRGB(26, 31, 38));
        button.setColour(juce::TextButton::textColourOffId,
                         selected ? juce::Colours::white
                                  : juce::Colour::fromRGB(165, 174, 184));
    };
    apply(ampButton, Page::amp); apply(pedalButton, Page::pedal); apply(cabButton, Page::cab);
    apply(effectsButton, Page::effects); apply(performanceButton, Page::performance); apply(settingsButton, Page::settings);
}

void NavigationBar::resized()
{
    auto r = getLocalBounds().reduced(8, 5);
    const int w = r.getWidth() / 6;
    ampButton.setBounds(r.removeFromLeft(w).reduced(4, 0));
    pedalButton.setBounds(r.removeFromLeft(w).reduced(4, 0));
    cabButton.setBounds(r.removeFromLeft(w).reduced(4, 0));
    effectsButton.setBounds(r.removeFromLeft(w).reduced(4, 0));
    performanceButton.setBounds(r.removeFromLeft(w).reduced(4, 0));
    settingsButton.setBounds(r.reduced(4, 0));
}
