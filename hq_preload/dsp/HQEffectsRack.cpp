#include "HQEffectsRack.h"

namespace guitardsp::hq {

void HQEffectsRack::prepare(double sampleRate, int maximumBlockSize) {
    for (int ch = 0; ch < stereoChannels; ++ch) {
        for (auto& pedal : pedals[(size_t)ch]) pedal.prepare(sampleRate, maximumBlockSize);
        noiseGate[(size_t)ch].prepare(sampleRate);
        studioComp[(size_t)ch].prepare(sampleRate);
        guitarComp[(size_t)ch].prepare(sampleRate);
        chorus[(size_t)ch].prepare(sampleRate, maximumBlockSize);
        flanger[(size_t)ch].prepare(sampleRate, maximumBlockSize);
        phaser[(size_t)ch].prepare(sampleRate);
        tremolo[(size_t)ch].prepare(sampleRate);
        vibrato[(size_t)ch].prepare(sampleRate, maximumBlockSize);
        delayFx[(size_t)ch].prepare(sampleRate, maximumBlockSize);
        reverbFx[(size_t)ch].prepare(sampleRate);
    }
    reset();
}

void HQEffectsRack::reset() {
    for (int ch = 0; ch < stereoChannels; ++ch) {
        for (auto& pedal : pedals[(size_t)ch]) pedal.reset();
        noiseGate[(size_t)ch].reset();
        chorus[(size_t)ch].reset();
        flanger[(size_t)ch].reset();
        phaser[(size_t)ch].reset();
        tremolo[(size_t)ch].reset();
        vibrato[(size_t)ch].reset();
    }
}

void HQEffectsRack::updateDynamicParameters() {
    NoiseGate::Params g;
    g.thresholdDb = gateControls.thresholdDb.load();
    g.rangeDb = gateControls.rangeDb.load();
    g.ratio = gateControls.ratio.load();
    g.attackMs = gateControls.attackMs.load();
    g.holdMs = gateControls.holdMs.load();
    g.releaseMs = gateControls.releaseMs.load();
    g.hysteresisDb = gateControls.hysteresisDb.load();
    g.sidechainHpHz = gateControls.sidechainHpHz.load();
    g.sidechainLpHz = gateControls.sidechainLpHz.load();

    StudioCompressor::Params c;
    c.thresholdDb = studioControls.thresholdDb.load();
    c.ratio = studioControls.ratio.load();
    c.attackMs = studioControls.attackMs.load();
    c.releaseMs = studioControls.releaseMs.load();
    c.kneeDb = studioControls.kneeDb.load();
    c.makeupDb = studioControls.makeupDb.load();
    c.mix = studioControls.mix.load();

    GuitarCompressor::Params gc;
    gc.sustain = guitarControls.sustain.load();
    gc.attack = guitarControls.attack.load();
    gc.blend = guitarControls.blend.load();
    gc.levelDb = guitarControls.levelDb.load();

    for (int ch = 0; ch < stereoChannels; ++ch) {
        noiseGate[(size_t)ch].setParameters(g);
        studioComp[(size_t)ch].setParameters(c);
        guitarComp[(size_t)ch].setParameters(gc);
    }
}

void HQEffectsRack::processPreAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    updateDynamicParameters();
    const auto mode = getDynamicsMode();
    const int channels = juce::jmin(stereoChannels, buffer.getNumChannels());

    // Preserve the clean guitar signal as the precision-gate detector key.  The gate
    // itself is applied after all pedal slots, so pedal-generated hiss is attenuated
    // without allowing that hiss to hold the detector open.
    juce::AudioBuffer<float> cleanKey(channels, numSamples);
    for (int ch=0; ch<channels; ++ch)
        cleanKey.copyFrom(ch,0,buffer,ch,startSample,numSamples);

    // Compressors belong before the pedals. The precision gate is intentionally deferred.
    if (mode == DynamicsMode::studioCompressor || mode == DynamicsMode::guitarCompressor)
        applyDynamics(buffer, startSample, numSamples);

    juce::AudioBuffer<float> mono(1, numSamples);
    for (int slot = 0; slot < pedalSlots; ++slot) {
        auto& control = pedalControls[(size_t)slot];
        if (!control.enabled.load(std::memory_order_relaxed)) continue;

        PedalParams p;
        p.drive = control.drive.load();
        p.tone = control.tone.load();
        p.levelDb = control.levelDb.load();
        p.cleanMix = juce::jlimit(0.0f, 1.0f, control.mix.load());
        p.lowCutHz = 40.0f + 260.0f * juce::jlimit(0.0f, 1.0f, control.aux1.load());
        p.focusHz = 300.0f + 2200.0f * juce::jlimit(0.0f, 1.0f, control.aux2.load());
        p.midDb = -12.0f + 24.0f * juce::jlimit(0.0f, 1.0f, control.aux3.load());
        p.presenceDb = p.midDb;
        p.bias = -0.2f + 0.4f * juce::jlimit(0.0f, 1.0f, control.aux1.load());
        p.scoop = juce::jlimit(0.0f, 1.0f, control.aux2.load());
        p.octave = juce::jlimit(0.0f, 1.0f, control.aux1.load());
        p.starve = juce::jlimit(0.0f, 1.0f, control.aux2.load());
        p.gate = juce::jlimit(0.0f, 1.0f, control.aux3.load());

        const auto type = static_cast<PedalType>(juce::jlimit(0, 8, control.model.load()));
        for (int ch = 0; ch < channels; ++ch) {
            auto& pedal = pedals[(size_t)ch][(size_t)slot];
            pedal.setType(type);
            pedal.setParameters(p);
            mono.copyFrom(0, 0, buffer, ch, startSample, numSamples);
            pedal.process(mono);
            buffer.copyFrom(ch, startSample, mono, 0, 0, numSamples);
        }
    }

    if (mode == DynamicsMode::gate) {
        for (int ch=0; ch<channels; ++ch) {
            auto* audio=buffer.getWritePointer(ch,startSample);
            const auto* key=cleanKey.getReadPointer(ch);
            for (int i=0;i<numSamples;++i)
                audio[i]=noiseGate[(size_t)ch].processKeyed(audio[i],key[i]);
        }
    }
}

void HQEffectsRack::applyDynamics(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    const auto mode = getDynamicsMode();
    if (mode == DynamicsMode::off) return;
    const int channels = juce::jmin(stereoChannels, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch) {
        auto* d = buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i) {
            switch (mode) {
                case DynamicsMode::gate: d[i] = noiseGate[(size_t)ch].process(d[i]); break;
                case DynamicsMode::studioCompressor: d[i] = studioComp[(size_t)ch].process(d[i]); break;
                case DynamicsMode::guitarCompressor: d[i] = guitarComp[(size_t)ch].process(d[i]); break;
                default: break;
            }
        }
    }
}

void HQEffectsRack::updatePostParameters() {
    const float rate = modControls.rateHz.load();
    const float depth = juce::jlimit(0.0f, 1.0f, modControls.depth.load());
    const float mix = juce::jlimit(0.0f, 1.0f, modControls.mix.load());
    const float manual = juce::jlimit(0.0f, 1.0f, modControls.manual.load());

    ChorusHQ::Params chorusParams;
    chorusParams.rateHz = rate;
    chorusParams.depthMs = 1.0f + 7.0f * depth;
    chorusParams.centreMs = 7.0f + 9.0f * manual;
    chorusParams.feedback = juce::jlimit(-0.85f, 0.85f, modControls.feedback.load());
    chorusParams.mix = mix;

    FlangerHQ::Params flangerParams;
    flangerParams.rateHz = rate;
    flangerParams.depthMs = 0.2f + 3.0f * depth;
    flangerParams.manualMs = 0.6f + 5.0f * manual;
    flangerParams.feedback = juce::jlimit(-0.92f, 0.92f, modControls.feedback.load());
    flangerParams.mix = mix;

    PhaserHQ::Params phaserParams;
    phaserParams.rateHz = rate;
    phaserParams.depth = depth;
    phaserParams.feedback = juce::jlimit(-0.85f, 0.85f, modControls.feedback.load());
    phaserParams.mix = mix;
    phaserParams.stages = 6;

    TremoloHQ::Params tremoloParams;
    tremoloParams.rateHz = rate;
    tremoloParams.depth = depth;
    tremoloParams.mix = mix;
    tremoloParams.shape = juce::jlimit(0.0f,1.0f,modControls.shape.load());

    VibratoHQ::Params vibratoParams;
    vibratoParams.rateHz = rate;
    vibratoParams.centreMs = 3.0f + 7.0f * manual;
    vibratoParams.depthMs = juce::jmin(vibratoParams.centreMs - 0.55f, 0.15f + 4.5f * depth);
    vibratoParams.mix = mix;

    DelayHQ::Params d;
    d.type = static_cast<DelayType>(juce::jlimit(0, 2, delayControls.flavor.load()));
    d.timeMs = delayControls.timeMs.load(); d.feedback = delayControls.feedback.load(); d.mix = delayControls.mix.load();
    d.lowCutHz = delayControls.lowCutHz.load(); d.highCutHz = delayControls.highCutHz.load(); d.drive = delayControls.drive.load();
    d.wow = delayControls.wow.load(); d.flutter = delayControls.flutter.load(); d.age = delayControls.age.load();

    ReverbHQ::Params r;
    r.type = static_cast<ReverbType>(juce::jlimit(0, 3, reverbControls.flavor.load()));
    r.size = reverbControls.size.load(); r.decay = reverbControls.decay.load(); r.damping = reverbControls.damping.load();
    r.preDelayMs = reverbControls.preDelayMs.load(); r.mix = reverbControls.mix.load(); r.mod = reverbControls.mod.load(); r.drip = reverbControls.drip.load();

    for (int ch = 0; ch < stereoChannels; ++ch) {
        chorus[(size_t)ch].setParameters(chorusParams);
        flanger[(size_t)ch].setParameters(flangerParams);
        phaser[(size_t)ch].setParameters(phaserParams);
        tremolo[(size_t)ch].setParameters(tremoloParams);
        vibrato[(size_t)ch].setParameters(vibratoParams);
        delayFx[(size_t)ch].setParameters(d);
        reverbFx[(size_t)ch].setParameters(r);
    }
}

void HQEffectsRack::processPostAmp(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    updatePostParameters();
    applyModulation(buffer, startSample, numSamples);
    const int channels = juce::jmin(stereoChannels, buffer.getNumChannels());
    const bool useDelay = isDelayEnabled(), useReverb = isReverbEnabled();
    if (!useDelay && !useReverb) return;
    for (int ch = 0; ch < channels; ++ch) {
        auto* data = buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i) {
            float x = data[i];
            if (useDelay) x = delayFx[(size_t)ch].process(x);
            if (useReverb) x = reverbFx[(size_t)ch].process(x);
            data[i] = x;
        }
    }
}

void HQEffectsRack::applyModulation(juce::AudioBuffer<float>& buffer, int startSample, int numSamples) {
    const auto mode = getModulationMode();
    if (mode == ModulationMode::off) return;
    const int channels = juce::jmin(stereoChannels, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch) {
        auto* data = buffer.getWritePointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i) {
            switch (mode) {
                case ModulationMode::chorus: data[i] = chorus[(size_t)ch].process(data[i]); break;
                case ModulationMode::flanger: data[i] = flanger[(size_t)ch].process(data[i]); break;
                case ModulationMode::phaser: data[i] = phaser[(size_t)ch].process(data[i]); break;
                case ModulationMode::tremolo: data[i] = tremolo[(size_t)ch].process(data[i]); break;
                case ModulationMode::vibrato: data[i] = vibrato[(size_t)ch].process(data[i]); break;
                default: break;
            }
        }
    }
}

}
