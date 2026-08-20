#include <JuceHeader.h>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <vector>
#include "../engine/LatencyProbe.h"
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;
constexpr int fftOrder = 13;
constexpr int fftSize = 1 << fftOrder;

struct Result
{
    int factor = 1;
    float latencySamples = 0.0f;
    double cpuMs = 0.0;
    double cpuRealtimePercent = 0.0;
    double rms = 0.0;
    double thdDb = 0.0;
    double aliasDb = 0.0;
    double spectralDeltaTo16Db = 0.0;
    std::vector<float> oneKhz;
};

std::vector<float> renderSine(int order, float hz, float amplitude)
{
    guitardsp::hq::AmpEngineHQ amp(order);
    amp.prepare(sampleRate, blockSize);
    amp.setParameters(guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(0.5f,0.5f,0.5f,0.82f,0.5f,0.5f,0.5f));
    juce::AudioBuffer<float> block(1, blockSize);
    std::vector<float> output; output.reserve(fftSize);
    double phase = 0.0; const double inc = juce::MathConstants<double>::twoPi * hz / sampleRate;
    const int totalSamples = fftSize * 2;
    for (int base = 0; base < totalSamples; base += blockSize)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            block.setSample(0, i, amplitude * (float)std::sin(phase)); phase += inc;
            if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;
        }
        amp.process(block);
        if (base >= fftSize) for (int i = 0; i < blockSize; ++i) output.push_back(block.getSample(0, i));
    }
    return output;
}

std::vector<float> spectrum(const std::vector<float>& signal)
{
    juce::dsp::FFT fft(fftOrder);
    std::vector<float> data((size_t)fftSize * 2u, 0.0f);
    const int n = juce::jmin(fftSize, (int)signal.size());
    for (int i = 0; i < n; ++i)
    {
        const float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float)i / (float)(fftSize - 1));
        data[(size_t)i] = signal[(size_t)i] * w;
    }
    fft.performFrequencyOnlyForwardTransform(data.data()); data.resize((size_t)fftSize / 2u); return data;
}

double rms(const std::vector<float>& signal)
{
    double e = 0.0; for (float x : signal) e += (double)x * x;
    return signal.empty() ? 0.0 : std::sqrt(e / (double)signal.size());
}

float binMagnitude(const std::vector<float>& mag, double hz)
{
    const int centre = juce::jlimit(1, (int)mag.size() - 2, (int)std::llround(hz * fftSize / sampleRate));
    float best = 0.0f; for (int b = centre - 1; b <= centre + 1; ++b) best = juce::jmax(best, mag[(size_t)b]); return best;
}

double thdDb(const std::vector<float>& signal, double fundamental)
{
    const auto mag = spectrum(signal); const double fund = juce::jmax(1.0e-12f, binMagnitude(mag, fundamental)); double e = 0.0;
    for (int h = 2; h <= 8; ++h)
    {
        const double hz = fundamental * h; if (hz >= sampleRate * 0.5) break;
        const double m = binMagnitude(mag, hz); e += m * m;
    }
    return 20.0 * std::log10(juce::jmax(1.0e-12, std::sqrt(e) / fund));
}

double foldFrequency(double hz)
{
    hz = std::fmod(std::abs(hz), sampleRate); if (hz > sampleRate * 0.5) hz = sampleRate - hz; return hz;
}

double aliasDb(const std::vector<float>& signal, double fundamental)
{
    const auto mag = spectrum(signal); const double fund = juce::jmax(1.0e-12f, binMagnitude(mag, fundamental)); double e = 0.0;
    for (int h = 3; h <= 8; ++h)
    {
        const double original = fundamental * h; if (original < sampleRate * 0.5) continue;
        const double alias = foldFrequency(original); if (alias < 40.0 || std::abs(alias - fundamental) < 80.0) continue;
        const double m = binMagnitude(mag, alias); e += m * m;
    }
    return 20.0 * std::log10(juce::jmax(1.0e-12, std::sqrt(e) / fund));
}

double spectralDelta(const std::vector<float>& a, const std::vector<float>& reference)
{
    const auto ma = spectrum(a); const auto mr = spectrum(reference); double sum = 0.0; int count = 0;
    for (int bin = 1; bin < (int)ma.size(); ++bin)
    {
        const double hz = sampleRate * bin / fftSize; if (hz < 80.0 || hz > 20000.0) continue;
        const double ref = mr[(size_t)bin]; if (ref < 1.0e-6) continue;
        const double ad = 20.0 * std::log10(juce::jmax(1.0e-12, (double)ma[(size_t)bin]));
        const double rd = 20.0 * std::log10(juce::jmax(1.0e-12, ref)); sum += std::abs(ad - rd); ++count;
    }
    return count > 0 ? sum / count : 0.0;
}

double benchmarkCpu(int order)
{
    guitardsp::hq::AmpEngineHQ amp(order); amp.prepare(sampleRate, blockSize);
    amp.setParameters(guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(0.5f,0.5f,0.5f,0.82f,0.5f,0.5f,0.5f));
    juce::AudioBuffer<float> block(1, blockSize); std::uint32_t state = 0x12345678u;
    constexpr int blocks = 500; const auto start = std::chrono::steady_clock::now();
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < blockSize; ++i)
        {
            state = state * 1664525u + 1013904223u;
            const float noise = ((float)((state >> 8) & 0xffffu) / 32768.0f - 1.0f) * 0.04f;
            const double t = (double)(b * blockSize + i) / sampleRate;
            block.setSample(0, i, 0.22f * (float)std::sin(juce::MathConstants<double>::twoPi * 110.0 * t)
                                + 0.12f * (float)std::sin(juce::MathConstants<double>::twoPi * 220.0 * t) + noise);
        }
        amp.process(block);
    }
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

bool latencyEstimatorSmoke()
{
    const auto probe = guitardsp::LatencyProbe::makeSequence(511, 0.08f); constexpr int expectedStart = 3417;
    std::vector<float> capture(10000, 0.0f); for (size_t i = 0; i < probe.size(); ++i) capture[(size_t)expectedStart + i] = probe[i];
    float corr = 0.0f; const int detected = guitardsp::LatencyProbe::estimateDelaySamples(probe, capture.data(), (int)capture.size(), corr);
    std::cout << "LATENCY_ESTIMATOR detected=" << detected << " expected=" << expectedStart << " correlation=" << corr << '\n';
    return detected == expectedStart && std::abs(corr) > 0.99f;
}

void writeJson(const juce::File& file, const std::array<Result,5>& results)
{
    std::ofstream out(file.getFullPathName().toStdString(), std::ios::trunc);
    out << std::fixed << std::setprecision(6) << "{\n  \"sampleRate\": 48000,\n  \"blockSize\": 256,\n  \"results\": [\n";
    for (size_t i = 0; i < results.size(); ++i)
    {
        const auto& r = results[i];
        out << "    {\"factor\": " << r.factor << ", \"latencySamples\": " << r.latencySamples
            << ", \"latencyMs\": " << (1000.0 * r.latencySamples / sampleRate)
            << ", \"cpuMs\": " << r.cpuMs << ", \"cpuRealtimePercent\": " << r.cpuRealtimePercent
            << ", \"rms\": " << r.rms << ", \"thdDb\": " << r.thdDb << ", \"aliasDb\": " << r.aliasDb
            << ", \"spectralDeltaTo16Db\": " << r.spectralDeltaTo16Db << "}" << (i + 1 == results.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
}
}

int main(int argc, char** argv)
{
    bool ok = latencyEstimatorSmoke(); std::array<Result,5> results; std::array<std::vector<float>,5> oneKhz;
    for (int order = 0; order <= 4; ++order)
    {
        guitardsp::hq::AmpEngineHQ amp(order); amp.prepare(sampleRate, blockSize);
        auto& r = results[(size_t)order]; r.factor = amp.getOversamplingFactor(); r.latencySamples = amp.getOversamplingLatencySamples();
        oneKhz[(size_t)order] = renderSine(order, 1000.0f, 0.34f); const auto high = renderSine(order, 9000.0f, 0.42f);
        r.oneKhz = oneKhz[(size_t)order]; r.rms = rms(r.oneKhz); r.thdDb = thdDb(r.oneKhz, 1000.0); r.aliasDb = aliasDb(high, 9000.0); r.cpuMs = benchmarkCpu(order);
        const double audioMs = 1000.0 * (500.0 * blockSize) / sampleRate; r.cpuRealtimePercent = 100.0 * r.cpuMs / audioMs;
        const int expectedFactor = 1 << order;
        if (r.factor != expectedFactor || !std::isfinite(r.latencySamples) || !std::isfinite(r.thdDb) || !std::isfinite(r.aliasDb)) ok = false;
    }
    for (int order = 0; order <= 4; ++order) results[(size_t)order].spectralDeltaTo16Db = spectralDelta(oneKhz[(size_t)order], oneKhz[4]);
    for (const auto& r : results)
        std::cout << "OS " << r.factor << "x latency=" << r.latencySamples << " smp cpu=" << r.cpuRealtimePercent << "% THD=" << r.thdDb << " dB alias=" << r.aliasDb << " dB spectrumDelta16=" << r.spectralDeltaTo16Db << " dB\n";
    const juce::File report = argc > 1 ? juce::File(argv[1]) : juce::File::getCurrentWorkingDirectory().getChildFile("oversampling-report.json");
    writeJson(report, results); std::cout << "REPORT " << report.getFullPathName() << '\n'; return ok ? 0 : 1;
}
