#pragma once

#include <JuceHeader.h>
#include <functional>

class NavigationBar : public juce::Component
{
public:
    enum class Page
    {
        amp = 0,
        pedal,
        cab,
        effects,
        settings
    };

    NavigationBar();
    void resized() override;
    void setCurrentPage(Page page);

    std::function<void(Page)> onPageSelected;

private:
    void configureButton(juce::TextButton& button, const juce::String& text, Page page);
    void refreshColours();

    juce::TextButton ampButton, pedalButton, cabButton, effectsButton, settingsButton;
    Page currentPage = Page::amp;
};
