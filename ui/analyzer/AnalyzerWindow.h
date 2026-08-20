#pragma once

#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include "../../engine/AudioEngine.h"

class AnalyzerWindow : public juce::DocumentWindow
{
private:
    class Content : public juce::Component, private juce::Timer
    {
    public:
        explicit Content(AudioEngine& engine)
            : audioEngine(engine), inputHistory(historySamples, 0.0f), outputHistory(historySamples, 0.0f)
        {
            setOpaque(true);
            startTimerHz(30);
        }

        ~Content() override
        {
            stopTimer();
        }

        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour::fromRGB(9, 12, 16));

            auto r = getLocalBounds().toFloat().reduced(16.0f);
            auto header = r.removeFromTop(44.0f);
            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(21.0f, juce::Font::bold));
            g.drawText("DSP ANALYZER", header, juce::Justification::centredLeft);

            g.setColour(juce::Colour::fromRGB(122, 132, 143));
            g.setFont(juce::Font(12.0f));
            g.drawText("Independent monitor - edit the main window while watching input/output",
                       header.removeFromRight(600.0f), juce::Justification::centredRight);

            r.removeFromTop(8.0f);
            constexpr float gap = 12.0f;
            const float scopeHeight = (r.getHeight() - gap) * 0.5f;
            auto inputBounds = r.removeFromTop(scopeHeight);
            r.removeFromTop(gap);

            drawScope(g, inputBounds, inputHistory, "INPUT", juce::Colour::fromRGB(98, 213, 167));
            drawScope(g, r, outputHistory, "OUTPUT", juce::Colour::fromRGB(222, 110, 58));
        }

        void resized() override {}

    private:
        static constexpr int historySamples = 2048;

        void timerCallback() override
        {
            if (!isShowing())
                return;

            auto& tap = audioEngine.getAnalyzerTap();
            const int inCount = tap.readInput(inputScratch.data(), static_cast<int>(inputScratch.size()));
            const int outCount = tap.readOutput(outputScratch.data(), static_cast<int>(outputScratch.size()));
            appendSamples(inputHistory, inputScratch.data(), inCount);
            appendSamples(outputHistory, outputScratch.data(), outCount);
            repaint();
        }

        static void appendSamples(std::vector<float>& history, const float* samples, int count)
        {
            if (samples == nullptr || count <= 0)
                return;

            if (count >= static_cast<int>(history.size()))
            {
                std::copy(samples + count - static_cast<int>(history.size()), samples + count, history.begin());
                return;
            }

            const auto keep = history.size() - static_cast<size_t>(count);
            std::memmove(history.data(), history.data() + count, keep * sizeof(float));
            std::copy(samples, samples + count, history.begin() + static_cast<std::ptrdiff_t>(keep));
        }

        static void drawScope(juce::Graphics& g,
                              juce::Rectangle<float> bounds,
                              const std::vector<float>& samples,
                              const juce::String& title,
                              juce::Colour traceColour)
        {
            g.setColour(juce::Colour::fromRGB(15, 20, 27));
            g.fillRoundedRectangle(bounds, 8.0f);
            g.setColour(juce::Colour::fromRGB(43, 51, 61));
            g.drawRoundedRectangle(bounds, 8.0f, 1.0f);

            auto plot = bounds.reduced(14.0f, 30.0f);
            g.setColour(juce::Colour::fromRGB(115, 127, 140));
            g.setFont(juce::Font(13.0f, juce::Font::bold));
            g.drawText(title, bounds.withHeight(26.0f).reduced(14.0f, 0.0f), juce::Justification::centredLeft);

            g.setColour(juce::Colour::fromRGB(34, 42, 51));
            g.drawHorizontalLine(static_cast<int>(plot.getCentreY()), plot.getX(), plot.getRight());

            if (samples.size() < 2 || plot.getWidth() <= 1.0f)
                return;

            juce::Path path;
            const float centreY = plot.getCentreY();
            const float halfHeight = plot.getHeight() * 0.46f;
            const float xScale = plot.getWidth() / static_cast<float>(samples.size() - 1);

            for (size_t i = 0; i < samples.size(); ++i)
            {
                const float x = plot.getX() + static_cast<float>(i) * xScale;
                const float y = centreY - juce::jlimit(-1.0f, 1.0f, samples[i]) * halfHeight;
                if (i == 0)
                    path.startNewSubPath(x, y);
                else
                    path.lineTo(x, y);
            }

            g.setColour(traceColour);
            g.strokePath(path, juce::PathStrokeType(1.35f));
        }

        AudioEngine& audioEngine;
        std::vector<float> inputHistory;
        std::vector<float> outputHistory;
        std::array<float, 4096> inputScratch {};
        std::array<float, 4096> outputScratch {};
    };

public:
    explicit AnalyzerWindow(AudioEngine& engine)
        : juce::DocumentWindow("DSP Analyzer",
                               juce::Colour::fromRGB(9, 12, 16),
                               juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton,
                               true)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setResizeLimits(720, 460, 2200, 1400);
        setContentOwned(new Content(engine), false);
        centreWithSize(1000, 680);
        setVisible(false);
    }

    ~AnalyzerWindow() override = default;

    void closeButtonPressed() override
    {
        setVisible(false);
    }
};
