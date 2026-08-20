#include <JuceHeader.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <vector>
#include "../engine/LatencyProbe.h"
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"
#include "../hq_preload/dsp/pedals/PedalEngineHQ.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int fftOrder = 13;
constexpr int fftSize = 1 << fftOrder;
constexpr int osCount = 5;
constexpr int frequencyCount = 4;
constexpr int controlCount = 3;
constexpr int caseCount = frequencyCount * controlCount;

// Exact FFT-bin frequencies reduce window leakage so that non-harmonic energy
// is a useful proxy for aliased/spurious content.
constexpr std::array<int, frequencyCount> frequencyBins { 171, 597, 1195, 1536 };
constexpr std::array<float, controlCount> ampGainControls { 0.35f, 0.65f, 0.90f };
constexpr std::array<float, controlCount> pedalDriveControls { 0.50f, 0.75f, 0.95f };
constexpr float testAmplitude = 0.34f;

struct CaseMetric
{
    double frequencyHz = 0.0;
    float control = 0.0f;
    double unexpectedDb = 0.0;
    double nullTo16Db = 0.0;
};

struct OversamplingResult
{
    int factor = 1;
    float ampLatencySamples = 0.0f;
    float pedalLatencySamples = 0.0f;
    double ampCpuRealtimePercent = 0.0;
    double pedalCpuRealtimePercent = 0.0;
    double ampThdDb = 0.0;
    double pedalThdDb = 0.0;
    double ampUnexpectedAggregateDb = 0.0;
    double pedalUnexpectedAggregateDb = 0.0;
    double ampUnexpectedWorstDb = 0.0;
    double pedalUnexpectedWorstDb = 0.0;
    double ampNullAggregateTo16Db = 0.0;
    double pedalNullAggregateTo16Db = 0.0;
    double ampNullWorstTo16Db = 0.0;
    double pedalNullWorstTo16Db = 0.0;
    std::array<CaseMetric, caseCount> ampCases {};
    std::array<CaseMetric, caseCount> pedalCases {};
};

double binFrequency(int bin) noexcept
{
    return sampleRate * static_cast<double>(bin) / static_cast<double>(fftSize);
}

std::vector<float> spectrum(const std::vector<float>& signal)
{
    juce::dsp::FFT fft(fftOrder);
    std::vector<float> data(static_cast<size_t>(fftSize) * 2u, 0.0f);
    const int n = juce::jmin(fftSize, static_cast<int>(signal.size()));
    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi
                                               * static_cast<float>(i) / static_cast<float>(fftSize - 1));
        data[static_cast<size_t>(i)] = signal[static_cast<size_t>(i)] * w;
    }
    fft.performFrequencyOnlyForwardTransform(data.data());
    data.resize(static_cast<size_t>(fftSize) / 2u);
    return data;
}

float binMagnitude(const std::vector<float>& mag, double hz)
{
    const int centre = juce::jlimit(1, static_cast<int>(mag.size()) - 2,
                                    static_cast<int>(std::llround(hz * fftSize / sampleRate)));
    float best = 0.0f;
    for (int b = centre - 1; b <= centre + 1; ++b)
        best = juce::jmax(best, mag[static_cast<size_t>(b)]);
    return best;
}

double thdDb(const std::vector<float>& signal, double fundamental)
{
    const auto mag = spectrum(signal);
    const double fund = juce::jmax(1.0e-12f, binMagnitude(mag, fundamental));
    double e = 0.0;
    for (int h = 2; h <= 12; ++h)
    {
        const double hz = fundamental * static_cast<double>(h);
        if (hz >= sampleRate * 0.5)
            break;
        const double m = binMagnitude(mag, hz);
        e += m * m;
    }
    return 20.0 * std::log10(juce::jmax(1.0e-15, std::sqrt(e) / fund));
}

// Sum all in-band spectral energy that is NOT at the fundamental or an
// in-band integer harmonic. With coherent sine frequencies this captures the
// broad alias/spur floor instead of only checking a few predicted fold bins.
double unexpectedSpectrumDb(const std::vector<float>& signal, double fundamental)
{
    const auto mag = spectrum(signal);
    const int bins = static_cast<int>(mag.size());
    const int fundamentalBin = static_cast<int>(std::llround(fundamental * fftSize / sampleRate));
    const double fund = juce::jmax(1.0e-12f, binMagnitude(mag, fundamental));
    std::vector<bool> expected(static_cast<size_t>(bins), false);

    for (int h = 1; ; ++h)
    {
        const int centre = fundamentalBin * h;
        if (centre >= bins)
            break;
        for (int d = -3; d <= 3; ++d)
        {
            const int b = centre + d;
            if (b > 0 && b < bins)
                expected[static_cast<size_t>(b)] = true;
        }
    }

    double energy = 0.0;
    for (int b = 1; b < bins; ++b)
    {
        const double hz = sampleRate * static_cast<double>(b) / static_cast<double>(fftSize);
        if (hz < 40.0 || expected[static_cast<size_t>(b)])
            continue;
        const double m = mag[static_cast<size_t>(b)];
        energy += m * m;
    }

    return 20.0 * std::log10(juce::jmax(1.0e-15, std::sqrt(energy) / fund));
}

// Time-domain null against 16x after compensating reported oversampling delay
// and a least-squares level trim. This catches aliases that happen to land on
// harmonic bins as well as phase/wave-shape differences missed by the spectrum
// classifier. More-negative is closer to the 16x reference.
double nullToReferenceDb(const std::vector<float>& candidate,
                         float candidateLatency,
                         const std::vector<float>& reference,
                         float referenceLatency)
{
    if (candidate.empty() || reference.empty())
        return 0.0;

    const int offset = static_cast<int>(std::llround(referenceLatency - candidateLatency));
    const int guard = 64;
    int candidateStart = guard;
    int referenceStart = guard + offset;
    if (referenceStart < guard)
    {
        candidateStart += guard - referenceStart;
        referenceStart = guard;
    }

    const int n = juce::jmin(static_cast<int>(candidate.size()) - candidateStart,
                            static_cast<int>(reference.size()) - referenceStart);
    if (n <= 128)
        return 0.0;

    double cc = 0.0;
    double xx = 0.0;
    double rr = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double x = candidate[static_cast<size_t>(candidateStart + i)];
        const double r = reference[static_cast<size_t>(referenceStart + i)];
        cc += x * r;
        xx += x * x;
        rr += r * r;
    }

    const double scale = cc / juce::jmax(1.0e-30, xx);
    double error = 0.0;
    for (int i = 0; i < n; ++i)
    {
        const double x = candidate[static_cast<size_t>(candidateStart + i)] * scale;
        const double r = reference[static_cast<size_t>(referenceStart + i)];
        const double d = x - r;
        error += d * d;
    }

    const double ratio = std::sqrt(error / juce::jmax(1.0e-30, rr));
    return 20.0 * std::log10(juce::jmax(1.0e-15, ratio));
}

double aggregateRatiosDb(const std::array<CaseMetric, caseCount>& cases, bool useNull)
{
    double sumSquares = 0.0;
    for (const auto& c : cases)
    {
        const double db = useNull ? c.nullTo16Db : c.unexpectedDb;
        const double ratio = std::pow(10.0, db / 20.0);
        sumSquares += ratio * ratio;
    }
    return 20.0 * std::log10(juce::jmax(1.0e-15, std::sqrt(sumSquares / static_cast<double>(caseCount))));
}

double worstDb(const std::array<CaseMetric, caseCount>& cases, bool useNull)
{
    double worst = -std::numeric_limits<double>::infinity();
    for (const auto& c : cases)
        worst = juce::jmax(worst, useNull ? c.nullTo16Db : c.unexpectedDb);
    return worst;
}

std::vector<float> renderAmpSine(int order, float hz, float amplitude, float gainControl)
{
    guitardsp::hq::AmpEngineHQ amp(order);
    amp.prepare(sampleRate, blockSize);
    amp.setParameters(guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(
        0.5f, 0.5f, 0.5f, gainControl, 0.5f, 0.5f, 0.5f));

    juce::AudioBuffer<float> block(1, blockSize);
    std::vector<float> output;
    output.reserve(fftSize);
    double phase = 0.0;
    const double inc = juce::MathConstants<double>::twoPi * hz / sampleRate;

    for (int base = 0; base < fftSize * 2; base += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            block.setSample(0, i, amplitude * static_cast<float>(std::sin(phase)));
            phase += inc;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }
        amp.process(block);
        if (base >= fftSize)
            for (int i = 0; i < blockSize; ++i)
                output.push_back(block.getSample(0, i));
    }
    return output;
}

std::vector<float> renderPedalSine(int order, float hz, float amplitude, float driveControl)
{
    guitardsp::hq::PedalEngineHQ pedal(order);
    pedal.prepare(sampleRate, blockSize);
    pedal.setType(guitardsp::hq::PedalType::hardDistortion);
    guitardsp::hq::PedalParams p;
    p.drive = driveControl;
    p.tone = 0.68f;
    p.lowCutHz = 72.0f;
    p.focusHz = 1050.0f;
    p.midDb = 2.0f;
    pedal.setParameters(p);

    juce::AudioBuffer<float> block(1, blockSize);
    std::vector<float> output;
    output.reserve(fftSize);
    double phase = 0.0;
    const double inc = juce::MathConstants<double>::twoPi * hz / sampleRate;

    for (int base = 0; base < fftSize * 2; base += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            block.setSample(0, i, amplitude * static_cast<float>(std::sin(phase)));
            phase += inc;
            if (phase >= juce::MathConstants<double>::twoPi)
                phase -= juce::MathConstants<double>::twoPi;
        }
        pedal.process(block);
        if (base >= fftSize)
            for (int i = 0; i < blockSize; ++i)
                output.push_back(block.getSample(0, i));
    }
    return output;
}

template <typename Processor>
double benchmarkCpu(Processor& processor)
{
    juce::AudioBuffer<float> block(1, blockSize);
    std::uint32_t state = 0x12345678u;
    constexpr int blocks = 500;
    const auto start = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            state = state * 1664525u + 1013904223u;
            const float noise = (static_cast<float>((state >> 8) & 0xffffu) / 32768.0f - 1.0f) * 0.04f;
            const double t = static_cast<double>(b * blockSize + i) / sampleRate;
            block.setSample(0, i,
                            0.22f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 110.0 * t))
                            + 0.12f * static_cast<float>(std::sin(juce::MathConstants<double>::twoPi * 220.0 * t))
                            + noise);
        }
        processor.process(block);
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

double ampCpuPercent(int order)
{
    guitardsp::hq::AmpEngineHQ amp(order);
    amp.prepare(sampleRate, blockSize);
    amp.setParameters(guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(
        0.5f, 0.5f, 0.5f, 0.65f, 0.5f, 0.5f, 0.5f));
    const double elapsed = benchmarkCpu(amp);
    const double audioMs = 1000.0 * (500.0 * blockSize) / sampleRate;
    return 100.0 * elapsed / audioMs;
}

double pedalCpuPercent(int order)
{
    guitardsp::hq::PedalEngineHQ pedal(order);
    pedal.prepare(sampleRate, blockSize);
    pedal.setType(guitardsp::hq::PedalType::hardDistortion);
    guitardsp::hq::PedalParams p;
    p.drive = 0.75f;
    p.tone = 0.68f;
    p.lowCutHz = 72.0f;
    p.focusHz = 1050.0f;
    p.midDb = 2.0f;
    pedal.setParameters(p);
    const double elapsed = benchmarkCpu(pedal);
    const double audioMs = 1000.0 * (500.0 * blockSize) / sampleRate;
    return 100.0 * elapsed / audioMs;
}

bool latencyEstimatorSmoke()
{
    const auto probe = guitardsp::LatencyProbe::makeSequence(511, 0.08f);
    constexpr int expectedStart = 3417;
    std::vector<float> capture(10000, 0.0f);
    for (size_t i = 0; i < probe.size(); ++i)
        capture[static_cast<size_t>(expectedStart) + i] = probe[i];

    float corr = 0.0f;
    const int detected = guitardsp::LatencyProbe::estimateDelaySamples(
        probe, capture.data(), static_cast<int>(capture.size()), corr);
    std::cout << "LATENCY_ESTIMATOR detected=" << detected
              << " expected=" << expectedStart
              << " correlation=" << corr << '\n';
    return detected == expectedStart && std::abs(corr) > 0.99f;
}

void writeCases(std::ofstream& out, const std::array<CaseMetric, caseCount>& cases)
{
    out << "[";
    for (int i = 0; i < caseCount; ++i)
    {
        const auto& c = cases[static_cast<size_t>(i)];
        out << "{\"frequencyHz\":" << c.frequencyHz
            << ",\"control\":" << c.control
            << ",\"unexpectedDb\":" << c.unexpectedDb
            << ",\"nullTo16Db\":" << c.nullTo16Db << "}"
            << (i + 1 == caseCount ? "" : ",");
    }
    out << "]";
}

void writeJson(const juce::File& file, const std::array<OversamplingResult, osCount>& results)
{
    std::ofstream out(file.getFullPathName().toStdString(), std::ios::trunc);
    out << std::fixed << std::setprecision(6)
        << "{\n  \"sampleRate\":48000,\n  \"blockSize\":256,\n"
        << "  \"frequencyBins\":[171,597,1195,1536],\n"
        << "  \"ampGainControls\":[0.35,0.65,0.90],\n"
        << "  \"pedalDriveControls\":[0.50,0.75,0.95],\n"
        << "  \"results\":[\n";

    for (size_t i = 0; i < results.size(); ++i)
    {
        const auto& r = results[i];
        out << "    {\"factor\":" << r.factor
            << ",\"ampLatencySamples\":" << r.ampLatencySamples
            << ",\"pedalLatencySamples\":" << r.pedalLatencySamples
            << ",\"ampCpuRealtimePercent\":" << r.ampCpuRealtimePercent
            << ",\"pedalCpuRealtimePercent\":" << r.pedalCpuRealtimePercent
            << ",\"ampThdDb\":" << r.ampThdDb
            << ",\"pedalThdDb\":" << r.pedalThdDb
            << ",\"ampUnexpectedAggregateDb\":" << r.ampUnexpectedAggregateDb
            << ",\"pedalUnexpectedAggregateDb\":" << r.pedalUnexpectedAggregateDb
            << ",\"ampUnexpectedWorstDb\":" << r.ampUnexpectedWorstDb
            << ",\"pedalUnexpectedWorstDb\":" << r.pedalUnexpectedWorstDb
            << ",\"ampNullAggregateTo16Db\":" << r.ampNullAggregateTo16Db
            << ",\"pedalNullAggregateTo16Db\":" << r.pedalNullAggregateTo16Db
            << ",\"ampNullWorstTo16Db\":" << r.ampNullWorstTo16Db
            << ",\"pedalNullWorstTo16Db\":" << r.pedalNullWorstTo16Db
            << ",\"ampCases\":";
        writeCases(out, r.ampCases);
        out << ",\"pedalCases\":";
        writeCases(out, r.pedalCases);
        out << "}" << (i + 1 == results.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
}
}

int main(int argc, char** argv)
{
    bool ok = latencyEstimatorSmoke();
    std::array<OversamplingResult, osCount> results;
    std::array<std::array<std::vector<float>, caseCount>, osCount> ampSignals;
    std::array<std::array<std::vector<float>, caseCount>, osCount> pedalSignals;

    for (int order = 0; order < osCount; ++order)
    {
        auto& r = results[static_cast<size_t>(order)];
        guitardsp::hq::AmpEngineHQ amp(order);
        amp.prepare(sampleRate, blockSize);
        guitardsp::hq::PedalEngineHQ pedal(order);
        pedal.prepare(sampleRate, blockSize);

        r.factor = amp.getOversamplingFactor();
        r.ampLatencySamples = amp.getOversamplingLatencySamples();
        r.pedalLatencySamples = pedal.getOversamplingLatencySamples();
        r.ampCpuRealtimePercent = ampCpuPercent(order);
        r.pedalCpuRealtimePercent = pedalCpuPercent(order);

        int caseIndex = 0;
        for (int f = 0; f < frequencyCount; ++f)
        {
            const double hz = binFrequency(frequencyBins[static_cast<size_t>(f)]);
            for (int c = 0; c < controlCount; ++c, ++caseIndex)
            {
                ampSignals[static_cast<size_t>(order)][static_cast<size_t>(caseIndex)] =
                    renderAmpSine(order, static_cast<float>(hz), testAmplitude,
                                  ampGainControls[static_cast<size_t>(c)]);
                pedalSignals[static_cast<size_t>(order)][static_cast<size_t>(caseIndex)] =
                    renderPedalSine(order, static_cast<float>(hz), testAmplitude,
                                    pedalDriveControls[static_cast<size_t>(c)]);
            }
        }

        // Representative THD reading: ~1 kHz, middle measured control value.
        const int baselineCase = 1;
        const double baselineHz = binFrequency(frequencyBins[0]);
        r.ampThdDb = thdDb(ampSignals[static_cast<size_t>(order)][static_cast<size_t>(baselineCase)], baselineHz);
        r.pedalThdDb = thdDb(pedalSignals[static_cast<size_t>(order)][static_cast<size_t>(baselineCase)], baselineHz);

        const int expectedFactor = 1 << order;
        if (r.factor != expectedFactor || pedal.getOversamplingFactor() != expectedFactor
            || !std::isfinite(r.ampThdDb) || !std::isfinite(r.pedalThdDb))
            ok = false;
    }

    const auto ampReferenceLatency = results[4].ampLatencySamples;
    const auto pedalReferenceLatency = results[4].pedalLatencySamples;

    for (int order = 0; order < osCount; ++order)
    {
        auto& r = results[static_cast<size_t>(order)];
        int caseIndex = 0;
        for (int f = 0; f < frequencyCount; ++f)
        {
            const double hz = binFrequency(frequencyBins[static_cast<size_t>(f)]);
            for (int c = 0; c < controlCount; ++c, ++caseIndex)
            {
                auto& ampCase = r.ampCases[static_cast<size_t>(caseIndex)];
                ampCase.frequencyHz = hz;
                ampCase.control = ampGainControls[static_cast<size_t>(c)];
                ampCase.unexpectedDb = unexpectedSpectrumDb(
                    ampSignals[static_cast<size_t>(order)][static_cast<size_t>(caseIndex)], hz);
                ampCase.nullTo16Db = order == 4 ? -300.0 : nullToReferenceDb(
                    ampSignals[static_cast<size_t>(order)][static_cast<size_t>(caseIndex)],
                    r.ampLatencySamples,
                    ampSignals[4][static_cast<size_t>(caseIndex)],
                    ampReferenceLatency);

                auto& pedalCase = r.pedalCases[static_cast<size_t>(caseIndex)];
                pedalCase.frequencyHz = hz;
                pedalCase.control = pedalDriveControls[static_cast<size_t>(c)];
                pedalCase.unexpectedDb = unexpectedSpectrumDb(
                    pedalSignals[static_cast<size_t>(order)][static_cast<size_t>(caseIndex)], hz);
                pedalCase.nullTo16Db = order == 4 ? -300.0 : nullToReferenceDb(
                    pedalSignals[static_cast<size_t>(order)][static_cast<size_t>(caseIndex)],
                    r.pedalLatencySamples,
                    pedalSignals[4][static_cast<size_t>(caseIndex)],
                    pedalReferenceLatency);

                if (!std::isfinite(ampCase.unexpectedDb) || !std::isfinite(ampCase.nullTo16Db)
                    || !std::isfinite(pedalCase.unexpectedDb) || !std::isfinite(pedalCase.nullTo16Db))
                    ok = false;
            }
        }

        r.ampUnexpectedAggregateDb = aggregateRatiosDb(r.ampCases, false);
        r.pedalUnexpectedAggregateDb = aggregateRatiosDb(r.pedalCases, false);
        r.ampUnexpectedWorstDb = worstDb(r.ampCases, false);
        r.pedalUnexpectedWorstDb = worstDb(r.pedalCases, false);
        r.ampNullAggregateTo16Db = aggregateRatiosDb(r.ampCases, true);
        r.pedalNullAggregateTo16Db = aggregateRatiosDb(r.pedalCases, true);
        r.ampNullWorstTo16Db = worstDb(r.ampCases, true);
        r.pedalNullWorstTo16Db = worstDb(r.pedalCases, true);
    }

    for (const auto& r : results)
    {
        std::cout << "OS " << r.factor << "x"
                  << " amp[lat=" << r.ampLatencySamples
                  << " cpu=" << r.ampCpuRealtimePercent << "%"
                  << " THD=" << r.ampThdDb
                  << " unexpectedAgg=" << r.ampUnexpectedAggregateDb
                  << " unexpectedWorst=" << r.ampUnexpectedWorstDb
                  << " nullAgg16=" << r.ampNullAggregateTo16Db
                  << " nullWorst16=" << r.ampNullWorstTo16Db << "]"
                  << " pedal[lat=" << r.pedalLatencySamples
                  << " cpu=" << r.pedalCpuRealtimePercent << "%"
                  << " THD=" << r.pedalThdDb
                  << " unexpectedAgg=" << r.pedalUnexpectedAggregateDb
                  << " unexpectedWorst=" << r.pedalUnexpectedWorstDb
                  << " nullAgg16=" << r.pedalNullAggregateTo16Db
                  << " nullWorst16=" << r.pedalNullWorstTo16Db << "]\n";
    }

    const auto& amp8 = results[3];
    const auto& amp16 = results[4];
    const auto& pedal8 = results[3];
    const auto& pedal16 = results[4];
    std::cout << "AMP_8X_VS_16X cpuRatio=" << amp8.ampCpuRealtimePercent / juce::jmax(1.0e-9, amp16.ampCpuRealtimePercent)
              << " unexpectedDeltaDb=" << amp8.ampUnexpectedAggregateDb - amp16.ampUnexpectedAggregateDb
              << " worstDeltaDb=" << amp8.ampUnexpectedWorstDb - amp16.ampUnexpectedWorstDb
              << " nullAggDb=" << amp8.ampNullAggregateTo16Db
              << " nullWorstDb=" << amp8.ampNullWorstTo16Db << '\n';
    std::cout << "PEDAL_8X_VS_16X cpuRatio=" << pedal8.pedalCpuRealtimePercent / juce::jmax(1.0e-9, pedal16.pedalCpuRealtimePercent)
              << " unexpectedDeltaDb=" << pedal8.pedalUnexpectedAggregateDb - pedal16.pedalUnexpectedAggregateDb
              << " worstDeltaDb=" << pedal8.pedalUnexpectedWorstDb - pedal16.pedalUnexpectedWorstDb
              << " nullAggDb=" << pedal8.pedalNullAggregateTo16Db
              << " nullWorstDb=" << pedal8.pedalNullWorstTo16Db << '\n';

    const juce::File report = argc > 1
        ? juce::File(argv[1])
        : juce::File::getCurrentWorkingDirectory().getChildFile("oversampling-report.json");
    writeJson(report, results);
    std::cout << "REPORT " << report.getFullPathName() << '\n';
    return ok ? 0 : 1;
}
