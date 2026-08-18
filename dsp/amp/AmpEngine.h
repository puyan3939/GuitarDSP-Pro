#pragma once

#include <JuceHeader.h>
#include <array>
#include "AmpStage.h"

class AmpEngine
{
public:
    static constexpr int numStages = 20;

    AmpEngine();
    void prepare(double sampleRate, int maximumBlockSize);
    void reset();
    void process(juce::AudioBuffer<float>& monoBuffer);

    const std::array<AmpStageParameters, numStages>& getParameters() const noexcept { return stageParams; }
    void setParameters(const std::array<AmpStageParameters, numStages>& params);
    void setStageParameters(int index, const AmpStageParameters& params);

    static const char* getStageName(int index) noexcept;
    static const char* getStageRole(int index) noexcept;
    static const char* getStageListenFor(int index) noexcept;

private:
    std::array<AmpStage, numStages> stages;
    std::array<AmpStageParameters, numStages> stageParams;
};
