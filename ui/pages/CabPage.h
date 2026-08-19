#pragma once
#include <JuceHeader.h>
#include "../../hq_preload/dsp/cab/CabMicEngineHQ.h"

class CabPage : public juce::Component
{
public:
    explicit CabPage(guitardsp::hq::CabMicEngineHQ& engine);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromEngine()
    {
        const auto& p=cab.getParameters();
        enabled.setToggleState(cab.isEnabled(),juce::dontSendNotification);
        irEngine.setSelectedId((int)p.irEngine+1,juce::dontSendNotification);
        cabType.setSelectedId((int)p.cab+1,juce::dontSendNotification);
        micType.setSelectedId((int)p.mic+1,juce::dontSendNotification);
        position.setValue(p.position,juce::dontSendNotification);
        distance.setValue(p.distance,juce::dontSendNotification);
        resonance.setValue(p.resonance,juce::dontSendNotification);
        lowCut.setValue(p.lowCutHz,juce::dontSendNotification);
        highCut.setValue(p.highCutHz,juce::dontSendNotification);
        mix.setValue(p.mix,juce::dontSendNotification);
        lowVolumeFeel.setValue(p.lowVolumeFeel,juce::dontSendNotification);
    }

private:
    void push();
    void updateInfo();
    guitardsp::hq::CabMicEngineHQ& cab;
    juce::Label title, info;
    juce::ToggleButton enabled { "CAB / MIC ON" };
    juce::ComboBox irEngine, cabType, micType;
    juce::Slider position, distance, resonance, lowCut, highCut, mix, lowVolumeFeel;
};
