#include "SignalAnalyzerPanel.h"
#include <cmath>

namespace
{
constexpr int fftSize = 2048;
constexpr int fftOrder = 11;

float peakOf(const std::vector<float>& samples)
{
    float peak = 0.0f;
    for (const auto x : samples)
        peak = juce::jmax(peak, std::abs(x));
    return peak;
}

juce::String formatFrequency(float frequency)
{
    if (frequency >= 1000.0f)
    {
        const float khz = frequency / 1000.0f;
        return juce::String(khz, khz < 10.0f && std::fmod(khz, 1.0f) != 0.0f ? 1 : 0) + "k";
    }
    return juce::String((int)std::round(frequency));
}
}

SignalAnalyzerPanel::SignalAnalyzerPanel(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);

    titleLabel.setText("SIGNAL ANALYZER", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(15.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(222, 110, 58));

    modeSelector.addItem("LIVE", 1);
    modeSelector.addItem("SINE", 2);
    modeSelector.addItem("SWEEP", 3);
    modeSelector.setSelectedId(1, juce::dontSendNotification);

    auto fillSource = [](juce::ComboBox& box)
    {
        box.addItem("INPUT", 1);
        box.addItem("POST PEDALS", 2);
        box.addItem("POST AMP", 3);
        box.addItem("POST CAB", 4);
        box.addItem("FINAL OUTPUT", 5);
    };
    fillSource(sourceASelector);
    fillSource(sourceBSelector);
    sourceASelector.setSelectedId(1, juce::dontSendNotification);
    sourceBSelector.setSelectedId(5, juce::dontSendNotification);

    viewSelector.addItem("OSC", 1);
    viewSelector.addItem("FFT", 2);
    viewSelector.setSelectedId(1, juce::dontSendNotification);

    scaleSelector.addItem("LINKED", 1);
    scaleSelector.addItem("AUTO", 2);
    scaleSelector.addItem("FIXED", 3);
    scaleSelector.setSelectedId(1, juce::dontSendNotification);

    timeSelector.addItem("5 ms", 1);
    timeSelector.addItem("10 ms", 2);
    timeSelector.addItem("20 ms", 3);
    timeSelector.addItem("50 ms", 4);
    timeSelector.setSelectedId(3, juce::dontSendNotification);

    ampReferenceLabel.setText("HQ REF", juce::dontSendNotification);
    ampReferenceLabel.setColour(juce::Label::textColourId, juce::Colour::fromRGB(165, 174, 184));
    ampReferenceSelector.addItem("CURRENT", 1);
    ampReferenceSelector.addItem("5F6-A REF", 2);
    ampReferenceSelector.addItem("JVM410H G5 MEASURED", 3);
    ampReferenceSelector.setSelectedId(1, juce::dontSendNotification);
    ampReferenceSelector.setTooltip("Select CURRENT, the circuit-derived 5F6-A reference, or the public-measurement-calibrated JVM410H OD1 Gain-5 reference. Select HQ 20 in the main window to hear/analyse it. Returning to CURRENT restores the captured custom HQ parameters.");
    customAmpSnapshot = audioEngine.getHQAmpEngine().getParameters();
    customAmpSnapshotValid = true;

    frequencySlider.setSliderStyle(juce::Slider::LinearHorizontal);
    frequencySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 22);
    frequencySlider.setRange(20.0, 5000.0, 1.0);
    frequencySlider.setSkewFactorFromMidPoint(440.0);
    frequencySlider.setValue(440.0, juce::dontSendNotification);
    frequencySlider.setTextValueSuffix(" Hz");

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 22);
    levelSlider.setRange(-60.0, 0.0, 0.5);
    levelSlider.setValue(-18.0, juce::dontSendNotification);
    levelSlider.setTextValueSuffix(" dBFS");

    frequencyLabel.setText("FREQ", juce::dontSendNotification);
    levelLabel.setText("LEVEL", juce::dontSendNotification);
    sweepLabel.setText("LOG SWEEP 20 Hz -> 20 kHz", juce::dontSendNotification);
    for (auto* label : { &frequencyLabel, &levelLabel, &sweepLabel })
        label->setColour(juce::Label::textColourId, juce::Colour::fromRGB(165, 174, 184));

    for (auto* component : { static_cast<juce::Component*>(&titleLabel), static_cast<juce::Component*>(&modeSelector),
                             static_cast<juce::Component*>(&sourceASelector), static_cast<juce::Component*>(&sourceBSelector),
                             static_cast<juce::Component*>(&viewSelector), static_cast<juce::Component*>(&scaleSelector),
                             static_cast<juce::Component*>(&timeSelector), static_cast<juce::Component*>(&frequencySlider),
                             static_cast<juce::Component*>(&levelSlider), static_cast<juce::Component*>(&holdButton),
                             static_cast<juce::Component*>(&frequencyLabel), static_cast<juce::Component*>(&levelLabel),
                             static_cast<juce::Component*>(&sweepLabel), static_cast<juce::Component*>(&ampReferenceLabel),
                             static_cast<juce::Component*>(&ampReferenceSelector) })
        addAndMakeVisible(component);

    modeSelector.onChange = [this] { refreshControlVisibility(); updateData(); repaint(); };
    viewSelector.onChange = [this] { repaint(); };
    sourceASelector.onChange = [this] { updateData(); repaint(); };
    sourceBSelector.onChange = [this] { updateData(); repaint(); };
    timeSelector.onChange = [this] { updateData(); repaint(); };
    ampReferenceSelector.onChange = [this] { applyAmpReference(); };
    frequencySlider.onValueChange = [this] { if (modeSelector.getSelectedId() == 2) updateData(); };
    levelSlider.onValueChange = [this] { if (modeSelector.getSelectedId() != 1) updateData(); };

    refreshControlVisibility();
    startTimerHz(15);
}

SignalAnalyzerPanel::~SignalAnalyzerPanel()
{
    stopTimer();
}

void SignalAnalyzerPanel::applyAmpReference()
{
    auto& amp = audioEngine.getHQAmpEngine();
    const int selectedReference = ampReferenceSelector.getSelectedId();
    if (selectedReference == 2 || selectedReference == 3)
    {
        if (!referenceApplied)
        {
            customAmpSnapshot = amp.getParameters();
            customAmpSnapshotValid = true;
        }

        if (selectedReference == 2)
            amp.setParameters(guitardsp::hq::AmpEngineHQ::makeBassman5F6AReference());
        else
            amp.setParameters(guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(0.5f, 0.5f, 0.5f));
        referenceApplied = true;
    }
    else if (referenceApplied && customAmpSnapshotValid)
    {
        amp.setParameters(customAmpSnapshot);
        referenceApplied = false;
    }
    updateData();
    repaint();
}

void SignalAnalyzerPanel::refreshControlVisibility()
{
    const int mode = modeSelector.getSelectedId();
    const bool isSine = mode == 2;
    const bool isTest = mode != 1;
    frequencyLabel.setVisible(isSine);
    frequencySlider.setVisible(isSine);
    levelLabel.setVisible(isTest);
    levelSlider.setVisible(isTest);
    sweepLabel.setVisible(mode == 3);
}

SignalTapBuffer::TapPoint SignalAnalyzerPanel::tapForSelectorId(int id) noexcept
{
    switch (id)
    {
        case 2: return SignalTapBuffer::TapPoint::postPedals;
        case 3: return SignalTapBuffer::TapPoint::postAmp;
        case 4: return SignalTapBuffer::TapPoint::postCab;
        case 5: return SignalTapBuffer::TapPoint::output;
        default: return SignalTapBuffer::TapPoint::input;
    }
}

juce::String SignalAnalyzerPanel::tapName(int id)
{
    switch (id)
    {
        case 2: return "POST PEDALS";
        case 3: return "POST AMP";
        case 4: return "POST CAB";
        case 5: return "FINAL OUTPUT";
        default: return "INPUT";
    }
}

void SignalAnalyzerPanel::timerCallback()
{
    if (holdButton.getToggleState())
        return;
    updateData();
    repaint();
}

void SignalAnalyzerPanel::updateData()
{
    const double fs = juce::jmax(8000.0, audioEngine.getCurrentSampleRate());
    int sampleCount = fftSize;
    if (viewSelector.getSelectedId() == 1)
    {
        static const std::array<double, 4> timesMs { 5.0, 10.0, 20.0, 50.0 };
        const int index = juce::jlimit(0, 3, timeSelector.getSelectedId() - 1);
        sampleCount = juce::jlimit(128, 4096, (int)std::round(fs * timesMs[(size_t)index] / 1000.0));
    }

    const bool live = modeSelector.getSelectedId() == 1;
    if (!live)
    {
        const auto testMode = modeSelector.getSelectedId() == 3 ? AudioEngine::AnalyzerTestMode::sweep
                                                                : AudioEngine::AnalyzerTestMode::sine;
        audioEngine.renderAnalyzerTest(testMode,
                                       (float)frequencySlider.getValue(),
                                       (float)levelSlider.getValue(),
                                       20.0f,
                                       20000.0f);
    }

    const auto tapA = tapForSelectorId(sourceASelector.getSelectedId());
    const auto tapB = tapForSelectorId(sourceBSelector.getSelectedId());
    if (live)
    {
        audioEngine.copyLiveAnalyzerTap(tapA, samplesA, sampleCount);
        audioEngine.copyLiveAnalyzerTap(tapB, samplesB, sampleCount);
    }
    else
    {
        audioEngine.copyTestAnalyzerTap(tapA, samplesA, sampleCount);
        audioEngine.copyTestAnalyzerTap(tapB, samplesB, sampleCount);
    }
}

void SignalAnalyzerPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(10, 13, 18));
    g.setColour(juce::Colour::fromRGB(34, 41, 50));
    g.drawRect(getLocalBounds(), 1);

    float scaleA = 1.0f;
    float scaleB = 1.0f;
    if (viewSelector.getSelectedId() == 1)
    {
        const float peakA = peakOf(samplesA);
        const float peakB = peakOf(samplesB);
        switch (scaleSelector.getSelectedId())
        {
            case 1:
            {
                const float linked = juce::jmax(0.02f, juce::jmax(peakA, peakB) * 1.12f);
                scaleA = scaleB = linked;
                break;
            }
            case 2:
                scaleA = juce::jmax(0.02f, peakA * 1.12f);
                scaleB = juce::jmax(0.02f, peakB * 1.12f);
                break;
            default:
                scaleA = scaleB = 1.0f;
                break;
        }
    }

    const bool showThd = modeSelector.getSelectedId() == 2;
    drawMonitor(g, monitorABounds, "A  " + tapName(sourceASelector.getSelectedId()), samplesA, scaleA, showThd);
    drawMonitor(g, monitorBBounds, "B  " + tapName(sourceBSelector.getSelectedId()), samplesB, scaleB, showThd);
}

void SignalAnalyzerPanel::drawMonitor(juce::Graphics& g,
                                      juce::Rectangle<float> bounds,
                                      const juce::String& name,
                                      const std::vector<float>& samples,
                                      float verticalScale,
                                      bool showThd)
{
    g.setColour(juce::Colour::fromRGB(14, 19, 25));
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(juce::Colour::fromRGB(49, 59, 70));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    auto header = bounds.removeFromTop(24.0f);
    g.setColour(juce::Colour::fromRGB(222, 110, 58));
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawText(name, header.removeFromLeft(180.0f), juce::Justification::centredLeft);

    g.setColour(juce::Colour::fromRGB(165, 174, 184));
    g.setFont(juce::Font(11.0f));
    g.drawText(makeStatsText(samples, showThd), header, juce::Justification::centredRight);

    bounds.reduce(8.0f, 5.0f);
    if (viewSelector.getSelectedId() == 2)
        drawSpectrum(g, bounds, samples);
    else
        drawOscilloscope(g, bounds, samples, verticalScale);
}

void SignalAnalyzerPanel::drawOscilloscope(juce::Graphics& g,
                                            juce::Rectangle<float> bounds,
                                            const std::vector<float>& samples,
                                            float verticalScale) const
{
    g.setColour(juce::Colour::fromRGB(31, 38, 47));
    for (int i = 1; i < 4; ++i)
    {
        const float y = bounds.getY() + bounds.getHeight() * (float)i / 4.0f;
        g.drawHorizontalLine((int)y, bounds.getX(), bounds.getRight());
    }
    g.setColour(juce::Colour::fromRGB(74, 86, 99));
    g.drawHorizontalLine((int)bounds.getCentreY(), bounds.getX(), bounds.getRight());

    if (samples.size() < 2)
        return;

    juce::Path path;
    const float denom = juce::jmax(0.0001f, verticalScale);
    for (size_t i = 0; i < samples.size(); ++i)
    {
        const float x = bounds.getX() + bounds.getWidth() * (float)i / (float)(samples.size() - 1);
        const float normalized = juce::jlimit(-1.0f, 1.0f, samples[i] / denom);
        const float y = bounds.getCentreY() - normalized * bounds.getHeight() * 0.46f;
        if (i == 0) path.startNewSubPath(x, y); else path.lineTo(x, y);
    }

    g.setColour(juce::Colour::fromRGB(98, 213, 167));
    g.strokePath(path, juce::PathStrokeType(1.35f));
}

void SignalAnalyzerPanel::drawSpectrum(juce::Graphics& g,
                                        juce::Rectangle<float> bounds,
                                        const std::vector<float>& samples) const
{
    if (samples.empty())
        return;

    std::array<float, fftSize * 2> fftData {};
    const int copyCount = juce::jmin((int)samples.size(), fftSize);
    const int sourceOffset = (int)samples.size() - copyCount;
    for (int i = 0; i < copyCount; ++i)
        fftData[(size_t)i] = samples[(size_t)(sourceOffset + i)];

    fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const double fs = juce::jmax(8000.0, audioEngine.getCurrentSampleRate());
    const float minFreq = 20.0f;
    const float maxFreq = (float)(fs * 0.5);
    const float logMin = std::log10(minFreq);
    const float logMax = std::log10(maxFreq);

    auto plot = bounds;
    plot.removeFromLeft(38.0f);
    plot.removeFromBottom(17.0f);

    g.setFont(juce::Font(9.5f));
    static const std::array<int, 6> dbTicks { -100, -80, -60, -40, -20, 0 };
    for (const int db : dbTicks)
    {
        const float y = juce::jmap((float)db, -100.0f, 0.0f, plot.getBottom(), plot.getY());
        g.setColour(juce::Colour::fromRGB(db == 0 ? 58 : 36, db == 0 ? 68 : 44, db == 0 ? 78 : 53));
        g.drawHorizontalLine((int)y, plot.getX(), plot.getRight());
        g.setColour(juce::Colour::fromRGB(126, 137, 149));
        g.drawText(juce::String(db) + " dB",
                   juce::Rectangle<float>(bounds.getX(), y - 7.0f, 35.0f, 14.0f),
                   juce::Justification::centredRight);
    }

    static const std::array<float, 10> frequencyTicks { 20.0f, 50.0f, 100.0f, 200.0f, 500.0f,
                                                        1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f };
    for (const float frequency : frequencyTicks)
    {
        if (frequency < minFreq || frequency > maxFreq)
            continue;
        const float xNorm = (std::log10(frequency) - logMin) / (logMax - logMin);
        const float x = plot.getX() + xNorm * plot.getWidth();
        g.setColour(juce::Colour::fromRGB(34, 42, 51));
        g.drawVerticalLine((int)x, plot.getY(), plot.getBottom());
        g.setColour(juce::Colour::fromRGB(126, 137, 149));
        g.drawText(formatFrequency(frequency),
                   juce::Rectangle<float>(x - 18.0f, plot.getBottom() + 2.0f, 36.0f, 13.0f),
                   juce::Justification::centred);
    }

    juce::Path path;
    bool started = false;
    float peakDb = -100.0f;
    float peakFrequency = 0.0f;
    float peakX = plot.getX();
    float peakY = plot.getBottom();

    for (int bin = 1; bin < fftSize / 2; ++bin)
    {
        const float frequency = (float)((double)bin * fs / (double)fftSize);
        if (frequency < minFreq || frequency > maxFreq)
            continue;

        const float xNorm = (std::log10(frequency) - logMin) / (logMax - logMin);
        const float magnitude = fftData[(size_t)bin] / (float)(fftSize / 2);
        const float db = juce::Decibels::gainToDecibels(magnitude, -100.0f);
        const float clippedDb = juce::jlimit(-100.0f, 0.0f, db);
        const float x = plot.getX() + xNorm * plot.getWidth();
        const float y = juce::jmap(clippedDb, -100.0f, 0.0f, plot.getBottom(), plot.getY());
        if (!started) { path.startNewSubPath(x, y); started = true; } else path.lineTo(x, y);

        if (db > peakDb)
        {
            peakDb = db;
            peakFrequency = frequency;
            peakX = x;
            peakY = y;
        }
    }

    g.setColour(juce::Colour::fromRGB(98, 213, 167));
    g.strokePath(path, juce::PathStrokeType(1.35f));

    if (peakFrequency > 0.0f && peakDb > -96.0f)
    {
        g.setColour(juce::Colour::fromRGB(222, 110, 58));
        g.drawVerticalLine((int)peakX, plot.getY(), plot.getBottom());
        g.fillEllipse(peakX - 2.5f, peakY - 2.5f, 5.0f, 5.0f);

        const juce::String peakText = "PEAK  " + formatFrequency(peakFrequency) + "Hz  " + juce::String(peakDb, 1) + " dB";
        g.setFont(juce::Font(10.5f, juce::Font::bold));
        auto badge = juce::Rectangle<float>(plot.getRight() - 148.0f, plot.getY() + 5.0f, 143.0f, 18.0f);
        g.setColour(juce::Colour::fromRGB(26, 31, 38).withAlpha(0.92f));
        g.fillRoundedRectangle(badge, 3.0f);
        g.setColour(juce::Colour::fromRGB(222, 110, 58));
        g.drawText(peakText, badge.reduced(4.0f, 0.0f), juce::Justification::centredRight);
    }
}

juce::String SignalAnalyzerPanel::makeStatsText(const std::vector<float>& samples, bool showThd) const
{
    if (samples.empty())
        return "Peak --  RMS --  DC --";

    float peak = 0.0f;
    double sum = 0.0;
    double sumSquares = 0.0;
    for (const auto x : samples)
    {
        peak = juce::jmax(peak, std::abs(x));
        sum += x;
        sumSquares += (double)x * (double)x;
    }

    const float rms = (float)std::sqrt(sumSquares / (double)samples.size());
    const float dc = (float)(sum / (double)samples.size());
    juce::String text = "Peak " + juce::String(juce::Decibels::gainToDecibels(peak, -100.0f), 1)
                      + "  RMS " + juce::String(juce::Decibels::gainToDecibels(rms, -100.0f), 1)
                      + "  DC " + juce::String(dc, 4);
    if (showThd)
    {
        const float thd = calculateThd(samples, (float)frequencySlider.getValue());
        text += "  THD~ " + juce::String(thd * 100.0f, 2) + "%";
    }
    return text;
}

float SignalAnalyzerPanel::calculateThd(const std::vector<float>& samples, float fundamentalHz) const
{
    if ((int)samples.size() < fftSize || fundamentalHz <= 0.0f)
        return 0.0f;

    std::array<float, fftSize * 2> fftData {};
    const int offset = (int)samples.size() - fftSize;
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t)i] = samples[(size_t)(offset + i)];
    fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
    fft.performFrequencyOnlyForwardTransform(fftData.data());

    const double fs = juce::jmax(8000.0, audioEngine.getCurrentSampleRate());
    const int fundamentalBin = juce::jlimit(1, fftSize / 2 - 1, (int)std::round((double)fundamentalHz * fftSize / fs));
    const double fundamental = juce::jmax(1.0e-12, (double)fftData[(size_t)fundamentalBin]);
    double harmonicsSquared = 0.0;
    for (int harmonic = 2; harmonic <= 10; ++harmonic)
    {
        const int bin = fundamentalBin * harmonic;
        if (bin >= fftSize / 2)
            break;
        const double magnitude = fftData[(size_t)bin];
        harmonicsSquared += magnitude * magnitude;
    }
    return (float)(std::sqrt(harmonicsSquared) / fundamental);
}

void SignalAnalyzerPanel::resized()
{
    auto r = getLocalBounds().reduced(10);
    auto top = r.removeFromTop(30);
    titleLabel.setBounds(top.removeFromLeft(130));
    modeSelector.setBounds(top.removeFromLeft(74).reduced(2, 2));
    viewSelector.setBounds(top.removeFromLeft(62).reduced(2, 2));
    scaleSelector.setBounds(top.removeFromLeft(78).reduced(2, 2));
    timeSelector.setBounds(top.removeFromLeft(70).reduced(2, 2));
    holdButton.setBounds(top.removeFromLeft(60).reduced(3, 2));
    ampReferenceLabel.setBounds(top.removeFromLeft(48));
    ampReferenceSelector.setBounds(top.removeFromLeft(185).reduced(2, 2));

    auto generator = top;
    frequencyLabel.setBounds(generator.removeFromLeft(38));
    frequencySlider.setBounds(generator.removeFromLeft(165));
    levelLabel.setBounds(generator.removeFromLeft(40));
    levelSlider.setBounds(generator.removeFromLeft(145));
    sweepLabel.setBounds(generator);

    r.removeFromTop(5);
    auto left = r.removeFromLeft(r.getWidth() / 2).reduced(3);
    auto right = r.reduced(3);

    auto leftHeader = left.removeFromTop(27);
    sourceASelector.setBounds(leftHeader.removeFromLeft(160));
    auto rightHeader = right.removeFromTop(27);
    sourceBSelector.setBounds(rightHeader.removeFromLeft(160));

    monitorABounds = left.toFloat();
    monitorBBounds = right.toFloat();
}
