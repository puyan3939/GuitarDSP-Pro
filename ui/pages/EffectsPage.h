#pragma once

#include <JuceHeader.h>
#include "../../hq_preload/dsp/HQEffectsRack.h"

class EffectsPage : public juce::Component
{
public:
    explicit EffectsPage(guitardsp::hq::HQEffectsRack& rack);
    void paint(juce::Graphics&) override;
    void resized() override;
    void refreshFromEngine()
    {
        modType.setSelectedId((int)rack.getModulationMode()+1,juce::dontSendNotification);
        auto& m=rack.modulationControl();
        modRate.setValue(m.rateHz.load(),juce::dontSendNotification); modDepth.setValue(m.depth.load(),juce::dontSendNotification);
        modMix.setValue(m.mix.load(),juce::dontSendNotification); modFeedback.setValue(m.feedback.load(),juce::dontSendNotification);
        modManual.setValue(m.manual.load(),juce::dontSendNotification); modShape.setValue(m.shape.load(),juce::dontSendNotification);
        auto& d=rack.delayControl(); delayEnabled.setToggleState(rack.isDelayEnabled(),juce::dontSendNotification);
        delayType.setSelectedId(d.flavor.load()+1,juce::dontSendNotification); delayTime.setValue(d.timeMs.load(),juce::dontSendNotification);
        delayFeedback.setValue(d.feedback.load(),juce::dontSendNotification); delayMix.setValue(d.mix.load(),juce::dontSendNotification);
        delayLowCut.setValue(d.lowCutHz.load(),juce::dontSendNotification); delayHighCut.setValue(d.highCutHz.load(),juce::dontSendNotification);
        delayDrive.setValue(d.drive.load(),juce::dontSendNotification); delayWow.setValue(d.wow.load(),juce::dontSendNotification);
        delayFlutter.setValue(d.flutter.load(),juce::dontSendNotification); delayAge.setValue(d.age.load(),juce::dontSendNotification);
        auto& r=rack.reverbControl(); reverbEnabled.setToggleState(rack.isReverbEnabled(),juce::dontSendNotification);
        reverbType.setSelectedId(r.flavor.load()+1,juce::dontSendNotification); reverbSize.setValue(r.size.load(),juce::dontSendNotification);
        reverbDecay.setValue(r.decay.load(),juce::dontSendNotification); reverbDamping.setValue(r.damping.load(),juce::dontSendNotification);
        reverbPreDelay.setValue(r.preDelayMs.load(),juce::dontSendNotification); reverbMix.setValue(r.mix.load(),juce::dontSendNotification);
        reverbMod.setValue(r.mod.load(),juce::dontSendNotification); reverbDrip.setValue(r.drip.load(),juce::dontSendNotification);

        auto& p=rack.parallelRigControl();
        parallelEnabled.setToggleState(p.enabled.load(),juce::dontSendNotification);
        cleanEnabled.setToggleState(p.cleanEnabled.load(),juce::dontSendNotification);
        subEnabled.setToggleState(p.subEnabled.load(),juce::dontSendNotification);
        cleanInvert.setToggleState(p.cleanInvert.load(),juce::dontSendNotification);
        subInvert.setToggleState(p.subInvert.load(),juce::dontSendNotification);
        mainLevel.setValue(p.mainLevelDb.load(),juce::dontSendNotification); mainDelay.setValue(p.mainDelayMs.load(),juce::dontSendNotification);
        cleanLevel.setValue(p.cleanLevelDb.load(),juce::dontSendNotification); cleanHp.setValue(p.cleanHpHz.load(),juce::dontSendNotification);
        cleanLp.setValue(p.cleanLpHz.load(),juce::dontSendNotification); cleanPresence.setValue(p.cleanPresenceDb.load(),juce::dontSendNotification);
        cleanDrive.setValue(p.cleanDrive.load(),juce::dontSendNotification); cleanDelay.setValue(p.cleanDelayMs.load(),juce::dontSendNotification);
        subLevel.setValue(p.subLevelDb.load(),juce::dontSendNotification); subHp.setValue(p.subHpHz.load(),juce::dontSendNotification);
        subLp.setValue(p.subLpHz.load(),juce::dontSendNotification); subBody.setValue(p.subBodyDb.load(),juce::dontSendNotification);
        subDrive.setValue(p.subDrive.load(),juce::dontSendNotification); subTracking.setValue(p.subTracking.load(),juce::dontSendNotification);
        subTone.setValue(p.subTone.load(),juce::dontSendNotification); subSmooth.setValue(p.subSmooth.load(),juce::dontSendNotification);
        subDelay.setValue(p.subDelayMs.load(),juce::dontSendNotification);
        updateModeHints();
    }

private:
    void pushMod();
    void pushDelay();
    void pushReverb();
    void pushParallel();
    void updateModeHints();
    void setupKnob(juce::Slider& s, double lo, double hi, double value, double step, const juce::String& suffix = {});

    guitardsp::hq::HQEffectsRack& rack;

    juce::Label title;
    juce::Label routingTitle, modTitle, delayTitle, reverbTitle;
    juce::Label routingHint, modHint, delayHint, reverbHint;

    juce::ToggleButton parallelEnabled { "PARALLEL RIG" };
    juce::ToggleButton cleanEnabled { "CLEAN" }, subEnabled { "SUB" };
    juce::ToggleButton cleanInvert { "CLEAN Ø" }, subInvert { "SUB Ø" };
    juce::Slider mainLevel, mainDelay;
    juce::Slider cleanLevel, cleanHp, cleanLp, cleanPresence, cleanDrive, cleanDelay;
    juce::Slider subLevel, subHp, subLp, subBody, subDrive, subTracking, subTone, subSmooth, subDelay;

    juce::ComboBox modType;
    juce::Slider modRate, modDepth, modMix, modFeedback, modManual, modShape;

    juce::ToggleButton delayEnabled { "DELAY ON" };
    juce::ComboBox delayType;
    juce::Slider delayTime, delayFeedback, delayMix, delayLowCut, delayHighCut, delayDrive, delayWow, delayFlutter, delayAge;

    juce::ToggleButton reverbEnabled { "REVERB ON" };
    juce::ComboBox reverbType;
    juce::Slider reverbSize, reverbDecay, reverbDamping, reverbPreDelay, reverbMix, reverbMod, reverbDrip;
};
