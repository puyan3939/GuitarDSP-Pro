#include <JuceHeader.h>
#include "MainComponent.h"

class GuitarDSPApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return "GuitarDSP-Pro"; }
    const juce::String getApplicationVersion() override { return "0.1.0"; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise(const juce::String&) override
    {
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        explicit MainWindow(const juce::String& name)
            : juce::DocumentWindow(name,
                                   juce::Colour::fromRGB(8, 10, 14),
                                   juce::DocumentWindow::closeButton)
        {
            setUsingNativeTitleBar(true);
            setResizable(true, false);
            setContentOwned(new MainComponent(), true);
            centreWithSize(1280, 720);
            setVisible(true);
        }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION(GuitarDSPApplication)
