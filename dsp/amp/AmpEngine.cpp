#include "AmpEngine.h"

AmpEngine::AmpEngine()
{
    stageParams = {{
        //  HPF    LPF    Drive Bias    PostLPF Output Nonlin Shape
        { 18.0f, 19000.0f, 1.00f, 0.000f, 19000.0f, 1.00f, 0.00f, 0.00f }, // 01 Input DC / Rumble
        { 35.0f, 15000.0f, 1.00f, 0.000f, 15000.0f, 1.00f, 0.00f, 0.00f }, // 02 Input Bandwidth
        { 70.0f, 12500.0f, 1.00f, 0.000f, 12500.0f, 1.00f, 0.00f, 0.00f }, // 03 Grid / Bright Entry
        { 55.0f, 12500.0f, 2.40f, 0.025f,  8200.0f, 0.92f, 0.80f, 0.60f }, // 04 V1A Triode
        { 90.0f, 11500.0f, 1.00f, 0.000f, 11000.0f, 1.00f, 0.00f, 0.00f }, // 05 V1A Coupling
        { 75.0f, 10500.0f, 3.30f, 0.040f,  6800.0f, 0.72f, 0.82f, 0.55f }, // 06 V1B Triode
        {120.0f,  9800.0f, 1.00f, 0.000f,  9500.0f, 1.00f, 0.00f, 0.00f }, // 07 Interstage Low Cut
        {110.0f,  9000.0f, 4.00f, 0.080f,  5900.0f, 0.60f, 1.00f, 0.90f }, // 08 Cold Clipper
        { 55.0f, 14500.0f, 1.18f, 0.012f, 11000.0f, 0.98f, 0.25f, 0.15f }, // 09 Cathode Follower
        { 45.0f, 16000.0f, 1.00f, 0.000f, 15000.0f, 1.00f, 0.00f, 0.00f }, // 10 Tone Stack Entry
        { 30.0f, 18000.0f, 1.00f, 0.000f, 18000.0f, 1.00f, 0.00f, 0.00f }, // 11 Tone Stack Core (placeholder)
        { 55.0f, 12000.0f, 1.55f, 0.018f,  9000.0f, 0.90f, 0.42f, 0.30f }, // 12 Recovery Triode
        { 30.0f, 18000.0f, 1.00f, 0.000f, 18000.0f, 0.72f, 0.00f, 0.00f }, // 13 Master / Level
        { 70.0f, 12500.0f, 1.00f, 0.000f, 12000.0f, 1.00f, 0.00f, 0.00f }, // 14 PI Input Shaping
        { 65.0f, 12000.0f, 1.55f, 0.015f,  9000.0f, 0.88f, 0.45f, 0.38f }, // 15 Phase Inverter
        { 20.0f, 18000.0f, 1.00f, 0.000f, 18000.0f, 1.00f, 0.00f, 0.00f }, // 16 Supply Sag (placeholder)
        { 85.0f, 11000.0f, 1.00f, 0.000f, 10500.0f, 1.00f, 0.00f, 0.00f }, // 17 Power Grid / Presence Feed
        { 55.0f, 10500.0f, 2.15f, 0.032f,  6500.0f, 0.82f, 0.72f, 0.52f }, // 18 EL34 Power Tubes
        { 20.0f, 18000.0f, 1.00f, 0.000f, 18000.0f, 1.00f, 0.00f, 0.00f }, // 19 NFB / Damping (placeholder)
        { 35.0f,  9000.0f, 1.15f, 0.005f,  7000.0f, 0.92f, 0.18f, 0.15f }  // 20 Output Transformer
    }};
}

void AmpEngine::prepare(double sampleRate, int maximumBlockSize)
{
    juce::ignoreUnused(maximumBlockSize);
    for (auto& stage : stages)
        stage.prepare(sampleRate);
    setParameters(stageParams);
}

void AmpEngine::reset()
{
    for (auto& stage : stages)
        stage.reset();
}

void AmpEngine::setParameters(const std::array<AmpStageParameters, numStages>& params)
{
    stageParams = params;
    for (int i = 0; i < numStages; ++i)
        stages[(size_t) i].setParameters(stageParams[(size_t) i]);
}

void AmpEngine::setStageParameters(int index, const AmpStageParameters& params)
{
    if (index < 0 || index >= numStages)
        return;
    stageParams[(size_t) index] = params;
    stages[(size_t) index].setParameters(params);
}

const char* AmpEngine::getStageName(int index) noexcept
{
    static constexpr const char* names[numStages] = {
        "Input DC / Rumble", "Input Bandwidth", "Grid / Bright Entry", "V1A Triode",
        "V1A Coupling", "V1B Triode", "Interstage Low Cut", "Cold Clipper",
        "Cathode Follower", "Tone Stack Entry", "Tone Stack Core", "Recovery Triode",
        "Master / Level", "PI Input Shaping", "Phase Inverter", "Supply Sag",
        "Power Grid / Presence Feed", "EL34 Power Tubes", "NFB / Damping", "Output Transformer"
    };
    return (index >= 0 && index < numStages) ? names[index] : "Unknown";
}

const char* AmpEngine::getStageRole(int index) noexcept
{
    static constexpr const char* roles[numStages] = {
        "Removes DC/subsonic energy before the amp. Main tool: HPF; normally no distortion.",
        "Defines the overall input bandwidth before gain stages. Use HPF/LPF to set vintage vs open input response.",
        "Represents grid/bright-entry shaping before V1A. Changes attack and what frequencies hit the first triode.",
        "First real voltage-gain/nonlinear stage. Drive controls early breakup; Bias changes asymmetry/even harmonics.",
        "Interstage coupling after V1A. Mostly a low-frequency shaping point; use HPF to tighten later distortion.",
        "Second triode gain stage. Adds denser preamp saturation after V1A has already changed the waveform.",
        "Controls low-frequency energy entering the cold clipper. Important for palm-mute tightness and blocking-like behaviour.",
        "Low-headroom asymmetric clipping stage. One of the main aggressive preamp character controls.",
        "Buffer/driver behaviour before the tone network. Keep Drive subtle; listen for compression and attack changes.",
        "Shapes the signal entering the tone stack. Bandwidth and level matter more than distortion here.",
        "Dedicated tone-stack location. Bass/Mid/Treble belong here; generic Drive should not be the main control.",
        "Post-tone-stack recovery gain. Can add saturation after EQ, which sounds different from V1A/V1B clipping.",
        "Master level feeding the PI. Use Output rather than Drive to choose how hard the power section is hit.",
        "Bandwidth/coupling before the phase inverter. Controls low-end stress and top-end openness of PI distortion.",
        "Phase-inverter nonlinear stage. Adds later, more open crunch before the power tubes.",
        "Dynamic power-supply sag location. This will become a dedicated envelope/supply model, not another Drive stage.",
        "Power-grid and presence-feed shaping. Controls what frequencies and transients reach the power tubes.",
        "Main power-tube saturation stage. Broad, thick clipping distinct from the sharper cold clipper.",
        "Negative-feedback/damping location. This will become a closed-loop power-stage control, not a simple level stage.",
        "Final transformer bandwidth/saturation. Usually subtle; shapes the last high-frequency roll-off and density."
    };
    return (index >= 0 && index < numStages) ? roles[index] : "";
}

const char* AmpEngine::getStageListenFor(int index) noexcept
{
    static constexpr const char* listen[numStages] = {
        "Rumble and handling noise", "Open vs restricted bandwidth", "Pick attack before V1A", "Touch sensitivity / edge of breakup",
        "Loose vs tight bass into V1B", "Dense preamp saturation", "Palm-mute tightness", "Aggressive asymmetric crunch",
        "Compression without obvious fuzz", "How hard the EQ network is driven", "Broad Bass/Mid/Treble balance", "Post-EQ saturation",
        "Preamp vs power-section balance", "Low-end stress before PI", "Open late-stage crunch", "Attack compression and recovery",
        "Power-stage tightness/presence", "Broad power saturation", "Tight vs loose damping", "Final smoothing / density"
    };
    return (index >= 0 && index < numStages) ? listen[index] : "";
}

void AmpEngine::process(juce::AudioBuffer<float>& monoBuffer)
{
    auto* x = monoBuffer.getWritePointer(0);
    const int n = monoBuffer.getNumSamples();
    for (int i = 0; i < n; ++i)
    {
        float y = x[i];
        for (auto& stage : stages)
            y = stage.processSample(y);
        x[i] = y * 0.24f;
    }
}
