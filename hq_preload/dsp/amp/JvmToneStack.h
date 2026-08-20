#pragma once

#include <array>

namespace guitardsp::hq
{
struct JvmToneStackConfig
{
    float bass = 0.5f;
    float middle = 0.5f;
    float treble = 0.5f;
    float bassTaper = 1.0f;
    float middleTaper = 1.0f;
    float trebleTaper = 1.0f;
    float r1Scale = 1.0f;
    float r2Scale = 1.0f;
    float c1Scale = 1.0f;
    float c23Scale = 1.0f;
};

// Passive Marshall/FMV tone-stack model used by the measured JVM410H reference.
// The network is discretised with backward-Euler capacitor companions, but the
// 7-node linear solve is performed only when controls/components change. The
// resulting three-state system costs only a handful of multiplies per sample.
class JvmToneStack
{
public:
    void prepare(double sampleRate);
    void reset() noexcept;
    void setConfig(const JvmToneStackConfig& newConfig);
    float process(float input) noexcept;

private:
    void rebuild();

    double fs = 48000.0;
    JvmToneStackConfig config;
    std::array<std::array<float, 3>, 3> stateA {};
    std::array<float, 3> stateB {};
    std::array<float, 3> outputC {};
    float outputD = 1.0f;
    std::array<float, 3> state {};
};
}
