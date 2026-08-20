#include "MeasurementLabPanel.h"

MeasurementLabPanel::MeasurementLabPanel(AudioEngine& engine)
    : audioEngine(engine)
{
    setOpaque(true);
    title.setText("MEASUREMENT LAB", juce::dontSendNotification);
    title.setFont(juce::Font(20.0f, juce::Font::bold));
    title.setColour(juce::Label::textColourId, juce::Colour::fromRGB(222, 110, 58));

    instructions.setText("LATENCY: connect physical OUTPUT 1 -> INPUT 1. The app mutes normal audio and sends a low-level probe for about 0.35 s.\n"
                         "DSP IMPULSE measures the current in-app chain separately. Estimated end-to-end = hardware loopback + DSP impulse delay.",
                         juce::dontSendNotification);
    instructions.setColour(juce::Label::textColourId, juce::Colour::fromRGB(180, 188, 198));
    instructions.setJustificationType(juce::Justification::topLeft);

    for (auto* label : { &deviceInfo, &dspInfo, &resultInfo, &osInfo })
    {
        label->setColour(juce::Label::textColourId, juce::Colour::fromRGB(214, 219, 225));
        label->setJustificationType(juce::Justification::topLeft);
    }

    osInfo.setText("Oversampling A/B is measured offline in CI at 1x / 2x / 4x / 8x / 16x (latency, CPU, THD, alias energy, spectrum delta).",
                   juce::dontSendNotification);

    addAndMakeVisible(title); addAndMakeVisible(instructions); addAndMakeVisible(deviceInfo); addAndMakeVisible(dspInfo);
    addAndMakeVisible(resultInfo); addAndMakeVisible(osInfo); addAndMakeVisible(measureButton); addAndMakeVisible(refreshDspButton);

    measureButton.onClick = [this]
    {
        if (audioEngine.startRoundTripLatencyMeasurement())
        {
            resultInfo.setText("MEASURING... normal audio temporarily muted", juce::dontSendNotification);
            measureButton.setEnabled(false);
        }
    };
    refreshDspButton.onClick = [this] { refreshDspLatency(); };

    refreshLabels(); startTimerHz(10);
}

MeasurementLabPanel::~MeasurementLabPanel() { stopTimer(); }

void MeasurementLabPanel::refreshDspLatency()
{
    dspLatencySamples = audioEngine.measureCurrentDspLatencySamples(); refreshLabels();
}

void MeasurementLabPanel::timerCallback()
{
    if (audioEngine.isRoundTripLatencyCaptureReady()) audioEngine.finaliseRoundTripLatencyMeasurement();
    measureButton.setEnabled(!audioEngine.isRoundTripLatencyMeasurementActive()); refreshLabels();
}

void MeasurementLabPanel::refreshLabels()
{
    const double fs = juce::jmax(1.0, audioEngine.getCurrentSampleRate());
    const int inLat = audioEngine.getReportedInputLatencySamples();
    const int outLat = audioEngine.getReportedOutputLatencySamples();
    const int driverSum = inLat + outLat;
    deviceInfo.setText("DEVICE REPORTED  input " + juce::String(inLat) + " smp  +  output " + juce::String(outLat)
                       + " smp  =  " + juce::String(driverSum) + " smp / " + juce::String(1000.0 * driverSum / fs, 2) + " ms",
                       juce::dontSendNotification);

    if (dspLatencySamples >= 0)
        dspInfo.setText("DSP IMPULSE PEAK  " + juce::String(dspLatencySamples) + " smp / " + juce::String(1000.0 * dspLatencySamples / fs, 3) + " ms", juce::dontSendNotification);
    else
        dspInfo.setText("DSP IMPULSE PEAK  not measured", juce::dontSendNotification);

    if (audioEngine.hasRoundTripLatencyResult())
    {
        const int roundTrip = audioEngine.getMeasuredRoundTripLatencySamples();
        const float corr = audioEngine.getLatencyMeasurementCorrelation();
        if (roundTrip >= 0)
        {
            juce::String text = "MEASURED HARDWARE ROUND TRIP  " + juce::String(roundTrip) + " smp / "
                              + juce::String(1000.0 * roundTrip / fs, 3) + " ms"
                              + "   correlation " + juce::String(corr, 3);
            if (dspLatencySamples >= 0)
            {
                const int total = roundTrip + dspLatencySamples;
                text += "\nEST. INPUT -> DSP -> OUTPUT  " + juce::String(total) + " smp / " + juce::String(1000.0 * total / fs, 3) + " ms";
            }
            resultInfo.setText(text, juce::dontSendNotification);
        }
        else
        {
            resultInfo.setText("No reliable loopback detected (correlation " + juce::String(corr, 3)
                               + "). Check OUTPUT 1 -> INPUT 1 cable and interface routing.", juce::dontSendNotification);
        }
    }
}

void MeasurementLabPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour::fromRGB(10, 13, 18));
    g.setColour(juce::Colour::fromRGB(31, 38, 47));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(14.0f), 10.0f, 1.0f);
}

void MeasurementLabPanel::resized()
{
    auto r = getLocalBounds().reduced(28);
    title.setBounds(r.removeFromTop(38)); instructions.setBounds(r.removeFromTop(72)); r.removeFromTop(10);
    deviceInfo.setBounds(r.removeFromTop(34)); dspInfo.setBounds(r.removeFromTop(34)); resultInfo.setBounds(r.removeFromTop(68));
    r.removeFromTop(8); auto buttons = r.removeFromTop(34); measureButton.setBounds(buttons.removeFromLeft(190)); buttons.removeFromLeft(10); refreshDspButton.setBounds(buttons.removeFromLeft(190));
    r.removeFromTop(22); osInfo.setBounds(r.removeFromTop(54));
}
