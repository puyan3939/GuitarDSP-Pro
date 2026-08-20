#pragma once
#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <cmath>
#include <algorithm>
#include "../common/HQDSP.h"
#include "../pedals/PitchOctaverHQ.h"

namespace guitardsp::hq
{
// Controls for a multi-bus rig. The feature is opt-in so legacy presets remain
// unchanged until PARALLEL RIG is enabled.
struct ParallelRigControl
{
    std::atomic<bool> enabled{false};

    std::atomic<float> mainLevelDb{0.0f};
    std::atomic<float> mainDelayMs{0.0f};

    std::atomic<bool> cleanEnabled{true};
    std::atomic<float> cleanLevelDb{-12.0f};
    std::atomic<float> cleanHpHz{650.0f};
    std::atomic<float> cleanLpHz{14500.0f};
    std::atomic<float> cleanPresenceDb{4.0f};
    std::atomic<float> cleanDrive{0.08f};
    std::atomic<float> cleanDelayMs{0.0f};
    std::atomic<bool> cleanInvert{false};

    std::atomic<bool> subEnabled{true};
    std::atomic<float> subLevelDb{-8.0f};
    std::atomic<float> subHpHz{32.0f};
    std::atomic<float> subLpHz{3200.0f};
    std::atomic<float> subBodyDb{3.0f};
    std::atomic<float> subDrive{0.34f};
    std::atomic<float> subTracking{0.72f};
    std::atomic<float> subTone{0.62f};
    std::atomic<float> subSmooth{0.68f};
    std::atomic<float> subDelayMs{0.0f};
    std::atomic<bool> subInvert{false};
};

// Three-bus processor for rigs where one guitar behaves like several
// instruments at once:
//   MAIN  : normal pedal -> amp -> cab path (processed outside this class)
//   CLEAN : un-clipped attack / upper-mid detail path
//   SUB   : dedicated octave-down -> bass-head / bass-cab path
//
// The clean and sub buses deliberately use independent filtering, saturation,
// level and delay compensation. This is structurally different from simply
// mixing octave voices into the front of one guitar amp.
class ParallelRigHQ
{
public:
    void prepare(double sampleRate, int maximumBlockSize)
    {
        fs = sampleRate;
        maxBlock = maximumBlockSize;
        cleanWork.setSize(1, maximumBlockSize, false, false, true);
        subWork.setSize(1, maximumBlockSize, false, false, true);

        octave.prepare(sampleRate, maximumBlockSize);

        cleanHp.prepare(fs); cleanLp.prepare(fs); cleanDc.prepare(fs);
        subHp.prepare(fs); subLp.prepare(fs); subDc.prepare(fs);
        cleanPresence.reset(); subBody.reset();

        mainDelay.prepare(fs, 25.0f);
        cleanDelay.prepare(fs, 25.0f);
        subDelay.prepare(fs, 25.0f);

        reset();
    }

    void reset()
    {
        octave.reset();
        cleanHp.reset(); cleanLp.reset(); cleanDc.reset(); cleanPresence.reset();
        subHp.reset(); subLp.reset(); subDc.reset(); subBody.reset();
        mainDelay.reset(); cleanDelay.reset(); subDelay.reset();
        cleanWork.clear(); subWork.clear();
    }

    void process(const juce::AudioBuffer<float>& dryInput,
                 juce::AudioBuffer<float>& processedMain,
                 int startSample,
                 int numSamples,
                 const ParallelRigControl& c)
    {
        if (!c.enabled.load(std::memory_order_relaxed) || numSamples <= 0)
            return;

        jassert(numSamples <= maxBlock);
        if (dryInput.getNumChannels() <= 0 || processedMain.getNumChannels() <= 0)
            return;

        updateFilters(c);

        cleanWork.copyFrom(0, 0, dryInput, 0, 0, numSamples);
        subWork.copyFrom(0, 0, dryInput, 0, 0, numSamples);

        if (c.cleanEnabled.load(std::memory_order_relaxed))
            processClean(cleanWork, numSamples, c);
        else
            cleanWork.clear();

        if (c.subEnabled.load(std::memory_order_relaxed))
            processSub(subWork, numSamples, c);
        else
            subWork.clear();

        const float mainGain = dbToGain(juce::jlimit(-60.0f, 12.0f, c.mainLevelDb.load()));
        const float cleanGain = dbToGain(juce::jlimit(-60.0f, 12.0f, c.cleanLevelDb.load()));
        const float subGain = dbToGain(juce::jlimit(-60.0f, 12.0f, c.subLevelDb.load()));
        const float cleanPolarity = c.cleanInvert.load() ? -1.0f : 1.0f;
        const float subPolarity = c.subInvert.load() ? -1.0f : 1.0f;

        mainDelay.setDelayMs(c.mainDelayMs.load());
        cleanDelay.setDelayMs(c.cleanDelayMs.load());
        subDelay.setDelayMs(c.subDelayMs.load());

        const auto* clean = cleanWork.getReadPointer(0);
        const auto* sub = subWork.getReadPointer(0);
        auto* mainOut = processedMain.getWritePointer(0, startSample);

        // The amp path is already mono by this point. Mix the buses once, then
        // duplicate the exact result to channel 2 so delay states do not advance
        // twice and phase compensation remains deterministic.
        for (int i = 0; i < numSamples; ++i)
        {
            const float main = mainDelay.process(mainOut[i]);
            const float attack = cleanDelay.process(clean[i]);
            const float bass = subDelay.process(sub[i]);
            mainOut[i] = mainGain * main + cleanGain * cleanPolarity * attack + subGain * subPolarity * bass;
        }

        const int channels = juce::jmin(2, processedMain.getNumChannels());
        for (int ch = 1; ch < channels; ++ch)
            processedMain.copyFrom(ch, startSample, processedMain, 0, startSample, numSamples);
    }

private:
    class IntegerDelay
    {
    public:
        void prepare(double sampleRate, float maxMs)
        {
            fs = sampleRate;
            const int wanted = juce::jmax(8, (int)std::ceil(0.001 * maxMs * fs) + 8);
            data.assign((size_t)wanted, 0.0f);
            write = 0;
        }
        void reset() noexcept { std::fill(data.begin(), data.end(), 0.0f); write = 0; }
        void setDelayMs(float ms) noexcept
        {
            if (data.empty()) { delaySamples = 0; return; }
            delaySamples = juce::jlimit(0, (int)data.size() - 1,
                (int)std::lround(0.001 * juce::jlimit(0.0f, 25.0f, ms) * fs));
        }
        float process(float x) noexcept
        {
            if (data.empty()) return x;
            data[(size_t)write] = x;
            int read = write - delaySamples;
            while (read < 0) read += (int)data.size();
            const float y = data[(size_t)read];
            if (++write >= (int)data.size()) write = 0;
            return y;
        }
    private:
        std::vector<float> data;
        double fs = 48000.0;
        int write = 0, delaySamples = 0;
    };

    void updateFilters(const ParallelRigControl& c)
    {
        cleanHp.setHz(juce::jlimit(60.0f, 5000.0f, c.cleanHpHz.load()));
        cleanLp.setHz(juce::jlimit(2500.0f, 20000.0f, c.cleanLpHz.load()));
        cleanDc.setHz(18.0f);
        cleanPresence.setPeak(fs, 3200.0f, 0.75f,
                              juce::jlimit(-12.0f, 12.0f, c.cleanPresenceDb.load()));

        subHp.setHz(juce::jlimit(18.0f, 180.0f, c.subHpHz.load()));
        subLp.setHz(juce::jlimit(500.0f, 8000.0f, c.subLpHz.load()));
        subDc.setHz(18.0f);
        subBody.setPeak(fs, 105.0f, 0.80f,
                        juce::jlimit(-12.0f, 12.0f, c.subBodyDb.load()));
    }

    void processClean(juce::AudioBuffer<float>& b, int n, const ParallelRigControl& c)
    {
        auto* d = b.getWritePointer(0);
        const float drive = juce::jlimit(0.0f, 1.0f, c.cleanDrive.load());
        for (int i = 0; i < n; ++i)
        {
            float x = cleanLp.process(cleanHp.process(d[i]));
            x = cleanPresence.process(x);
            if (drive > 0.0001f)
            {
                const float saturated = 1.35f * asymSat(x * (1.0f + 2.2f * drive), 0.006f, 0.10f);
                x = lerp(x, saturated, 0.20f + 0.45f * drive);
            }
            d[i] = cleanDc.process(x);
        }
    }

    void processSub(juce::AudioBuffer<float>& b, int n, const ParallelRigControl& c)
    {
        PitchOctaverHQ::Params p;
        p.octaveUp = 0.0f;
        p.octaveDown = 1.0f;
        p.dry = 0.0f;
        p.tracking = juce::jlimit(0.0f, 1.0f, c.subTracking.load());
        p.tone = juce::jlimit(0.0f, 1.0f, c.subTone.load());
        p.smooth = juce::jlimit(0.0f, 1.0f, c.subSmooth.load());
        octave.setParameters(p);
        octave.processBlock(b);

        auto* d = b.getWritePointer(0);
        const float drive = juce::jlimit(0.0f, 1.0f, c.subDrive.load());
        for (int i = 0; i < n; ++i)
        {
            float x = subHp.process(d[i]);
            x = subBody.process(x);

            // A deliberately simple, high-headroom bass head: enough compression
            // and harmonic support to make the octave voice physical without
            // turning it into another guitar fuzz path.
            const float pre = x * (1.0f + 3.8f * drive);
            const float sat = 1.10f * asymSat(pre, 0.008f, 0.16f);
            x = lerp(x, sat, 0.28f + 0.52f * drive);
            x = subLp.process(x);
            d[i] = subDc.process(x);
        }
    }

    double fs = 48000.0;
    int maxBlock = 0;
    juce::AudioBuffer<float> cleanWork, subWork;
    PitchOctaverHQ octave;
    OnePoleHP cleanHp, cleanDc, subHp, subDc;
    OnePoleLP cleanLp, subLp;
    Biquad cleanPresence, subBody;
    IntegerDelay mainDelay, cleanDelay, subDelay;
};
}
