#include "JvmToneStack.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace guitardsp::hq
{
namespace
{
constexpr int unknownNodes = 7; // circuit nodes 2..8; node 1 is the known input source
constexpr int capacitorStates = 3;

float clampControl(float x) noexcept { return std::clamp(x, 0.0f, 1.0f); }
float mapPot(float x, float exponent) noexcept
{
    x = clampControl(x);
    exponent = std::clamp(exponent, 0.20f, 5.0f);
    if (x <= 0.0f) return 0.0f;
    if (x >= 1.0f) return 1.0f;
    return std::pow(x, exponent);
}
}

void JvmToneStack::prepare(double sampleRate)
{
    fs = std::max(8000.0, sampleRate);
    rebuild();
    reset();
}

void JvmToneStack::reset() noexcept
{
    state.fill(0.0f);
}

void JvmToneStack::setConfig(const JvmToneStackConfig& newConfig)
{
    config = newConfig;
    rebuild();
}

float JvmToneStack::process(float input) noexcept
{
    const auto oldState = state;
    float output = outputD * input;
    for (int i = 0; i < capacitorStates; ++i)
        output += outputC[(size_t)i] * oldState[(size_t)i];

    for (int row = 0; row < capacitorStates; ++row)
    {
        float next = stateB[(size_t)row] * input;
        for (int column = 0; column < capacitorStates; ++column)
            next += stateA[(size_t)row][(size_t)column] * oldState[(size_t)column];
        state[(size_t)row] = next;
    }
    return output;
}

void JvmToneStack::rebuild()
{
    using NodeVector = std::array<double, unknownNodes>;
    using NodeMatrix = std::array<NodeVector, unknownNodes>;

    NodeMatrix matrix {};
    NodeVector inputRhs {};
    std::array<NodeVector, capacitorStates> stateRhs {};
    std::array<NodeVector, capacitorStates> capacitorIncidence {};
    std::array<double, capacitorStates> capacitorInputIncidence {};

    auto makeIncidence = [](int positiveNode, int negativeNode, NodeVector& unknown, double& inputCoefficient)
    {
        unknown.fill(0.0);
        inputCoefficient = 0.0;
        auto addNode = [&](int node, double sign)
        {
            if (node == 1)
                inputCoefficient += sign;
            else if (node >= 2 && node <= 8)
                unknown[(size_t)(node - 2)] += sign;
        };
        addNode(positiveNode, 1.0);
        addNode(negativeNode, -1.0);
    };

    auto stampConductance = [&](int positiveNode, int negativeNode, double conductance)
    {
        NodeVector incidence {};
        double inputCoefficient = 0.0;
        makeIncidence(positiveNode, negativeNode, incidence, inputCoefficient);
        for (int row = 0; row < unknownNodes; ++row)
        {
            inputRhs[(size_t)row] -= conductance * incidence[(size_t)row] * inputCoefficient;
            for (int column = 0; column < unknownNodes; ++column)
                matrix[(size_t)row][(size_t)column] += conductance * incidence[(size_t)row] * incidence[(size_t)column];
        }
    };

    auto stampResistor = [&](int positiveNode, int negativeNode, double resistance)
    {
        stampConductance(positiveNode, negativeNode, 1.0 / std::max(0.1, resistance));
    };

    auto stampCapacitor = [&](int stateIndex, int positiveNode, int negativeNode, double capacitance)
    {
        const double conductance = std::max(1.0e-15, capacitance) * fs;
        auto& incidence = capacitorIncidence[(size_t)stateIndex];
        double inputCoefficient = 0.0;
        makeIncidence(positiveNode, negativeNode, incidence, inputCoefficient);
        capacitorInputIncidence[(size_t)stateIndex] = inputCoefficient;

        for (int row = 0; row < unknownNodes; ++row)
        {
            inputRhs[(size_t)row] -= conductance * incidence[(size_t)row] * inputCoefficient;
            stateRhs[(size_t)stateIndex][(size_t)row] += conductance * incidence[(size_t)row];
            for (int column = 0; column < unknownNodes; ++column)
                matrix[(size_t)row][(size_t)column] += conductance * incidence[(size_t)row] * incidence[(size_t)column];
        }
    };

    const double bass = mapPot(config.bass, config.bassTaper);
    const double middle = mapPot(config.middle, config.middleTaper);
    const double treble = mapPot(config.treble, config.trebleTaper);
    constexpr double minimumPotFraction = 1.0e-5;

    const double r1 = 33000.0 * std::clamp((double)config.r1Scale, 0.5, 1.5);
    const double r2 = 39000.0 * std::clamp((double)config.r2Scale, 0.5, 1.5);
    constexpr double treblePot = 200000.0;
    constexpr double bassPot = 1000000.0;
    constexpr double middlePot = 20000.0;
    constexpr double load = 1000000.0;
    const double c1 = 470.0e-12 * std::clamp((double)config.c1Scale, 0.5, 1.5);
    const double c2 = 22.0e-9 * std::clamp((double)config.c23Scale, 0.5, 1.5);
    const double c3 = c2;

    // Published JVM410H tone-stack topology/component values:
    // R1 33k, R2 39k, Treble 200k, Bass 1M, Middle 20k, load 1M,
    // C1 470pF, C2/C3 22nF. Nodes follow the passive FMV network.
    stampResistor(1, 2, r1);
    stampResistor(3, 4, treblePot * std::max(minimumPotFraction, 1.0 - treble));
    stampResistor(4, 5, treblePot * std::max(minimumPotFraction, treble));
    stampResistor(5, 6, bassPot * std::max(minimumPotFraction, bass));
    stampResistor(6, 7, middlePot * std::max(minimumPotFraction, 1.0 - middle));
    stampResistor(7, 0, middlePot * std::max(minimumPotFraction, middle));
    stampResistor(4, 8, r2);
    stampResistor(8, 0, load);

    stampCapacitor(0, 1, 3, c1);
    stampCapacitor(1, 2, 5, c2);
    stampCapacitor(2, 2, 7, c3);

    // Solve the seven nodal equations for four right-hand sides at once:
    // current input plus each of the three previous capacitor voltages.
    std::array<std::array<double, unknownNodes + 1 + capacitorStates>, unknownNodes> augmented {};
    for (int row = 0; row < unknownNodes; ++row)
    {
        for (int column = 0; column < unknownNodes; ++column)
            augmented[(size_t)row][(size_t)column] = matrix[(size_t)row][(size_t)column];
        augmented[(size_t)row][unknownNodes] = inputRhs[(size_t)row];
        for (int s = 0; s < capacitorStates; ++s)
            augmented[(size_t)row][unknownNodes + 1 + s] = stateRhs[(size_t)s][(size_t)row];
    }

    bool valid = true;
    for (int column = 0; column < unknownNodes; ++column)
    {
        int pivot = column;
        for (int row = column + 1; row < unknownNodes; ++row)
            if (std::abs(augmented[(size_t)row][(size_t)column]) > std::abs(augmented[(size_t)pivot][(size_t)column]))
                pivot = row;

        if (std::abs(augmented[(size_t)pivot][(size_t)column]) < 1.0e-18)
        {
            valid = false;
            break;
        }
        if (pivot != column)
            std::swap(augmented[(size_t)pivot], augmented[(size_t)column]);

        const double divisor = augmented[(size_t)column][(size_t)column];
        for (int entry = column; entry < unknownNodes + 1 + capacitorStates; ++entry)
            augmented[(size_t)column][(size_t)entry] /= divisor;

        for (int row = 0; row < unknownNodes; ++row)
        {
            if (row == column) continue;
            const double factor = augmented[(size_t)row][(size_t)column];
            for (int entry = column; entry < unknownNodes + 1 + capacitorStates; ++entry)
                augmented[(size_t)row][(size_t)entry] -= factor * augmented[(size_t)column][(size_t)entry];
        }
    }

    if (!valid)
    {
        stateA = {};
        stateB = {};
        outputC = {};
        outputD = 1.0f;
        return;
    }

    NodeVector nodeFromInput {};
    std::array<NodeVector, capacitorStates> nodeFromState {};
    for (int node = 0; node < unknownNodes; ++node)
    {
        nodeFromInput[(size_t)node] = augmented[(size_t)node][unknownNodes];
        for (int s = 0; s < capacitorStates; ++s)
            nodeFromState[(size_t)s][(size_t)node] = augmented[(size_t)node][unknownNodes + 1 + s];
    }

    for (int stateRow = 0; stateRow < capacitorStates; ++stateRow)
    {
        double b = capacitorInputIncidence[(size_t)stateRow];
        for (int node = 0; node < unknownNodes; ++node)
            b += capacitorIncidence[(size_t)stateRow][(size_t)node] * nodeFromInput[(size_t)node];
        stateB[(size_t)stateRow] = (float)b;

        for (int stateColumn = 0; stateColumn < capacitorStates; ++stateColumn)
        {
            double a = 0.0;
            for (int node = 0; node < unknownNodes; ++node)
                a += capacitorIncidence[(size_t)stateRow][(size_t)node]
                   * nodeFromState[(size_t)stateColumn][(size_t)node];
            stateA[(size_t)stateRow][(size_t)stateColumn] = (float)a;
        }
    }

    constexpr int outputNodeIndex = 8 - 2;
    outputD = (float)nodeFromInput[(size_t)outputNodeIndex];
    for (int s = 0; s < capacitorStates; ++s)
        outputC[(size_t)s] = (float)nodeFromState[(size_t)s][(size_t)outputNodeIndex];
}
}
