#include <JuceHeader.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"

namespace
{
constexpr int fftOrder = 12;
constexpr int fftSize = 1 << fftOrder;
constexpr int scoreSamples = 4096;
constexpr int prerollSamples = 4096;
constexpr int fineAlignmentLag = 384;
constexpr int envelopeHop = 128;
constexpr int parameterCount = 12;

struct Clip
{
    bool fit = false;
    std::string name;
    float bass = 0.5f, middle = 0.5f, treble = 0.5f;
    double sampleRate = 44100.0;
    std::vector<float> input, target;
    int baseLag = 0;
    int segmentStart = 0;
    int scoreStart = 0;
};

struct RenderedClip
{
    std::vector<float> model, target;
};

struct Metrics
{
    double objective = std::numeric_limits<double>::infinity();
    double nmseDb = 0.0;
    double spectralMaeDb = 0.0;
    double correlation = 0.0;
    double gain = 1.0;
};

using Candidate = std::array<float, parameterCount>;

const std::array<const char*, parameterCount> parameterNames {
    "driveScale", "biasShift", "lowPassScale", "asymmetryScale", "memoryScale",
    "recoveryGain", "bassTaper", "middleTaper", "trebleTaper",
    "r1Scale", "c1Scale", "c23Scale"
};

const Candidate initialCandidate { 1.0f, 0.0f, 1.0f, 1.0f, 1.0f,
                                   4.0f, 1.0f, 1.0f, 1.0f,
                                   1.0f, 1.0f, 1.0f };
const Candidate lowerBounds { 0.65f, -0.05f, 0.75f, 0.60f, 0.50f,
                              0.5f, 0.35f, 0.35f, 0.35f,
                              0.80f, 0.80f, 0.80f };
const Candidate upperBounds { 1.50f,  0.05f, 1.25f, 1.50f, 1.80f,
                              8.0f, 3.00f, 3.00f, 3.00f,
                              1.20f, 1.20f, 1.20f };
const Candidate initialSteps { 0.15f, 0.015f, 0.10f, 0.15f, 0.20f,
                               1.50f, 0.45f, 0.45f, 0.45f,
                               0.08f, 0.08f, 0.08f };

bool loadMono(juce::AudioFormatManager& formats, const juce::File& file,
              std::vector<float>& samples, double& sampleRate)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->lengthInSamples <= 0 || reader->numChannels == 0)
        return false;
    if (reader->lengthInSamples > 44100LL * 120LL)
        return false;

    const int n = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), n);
    if (!reader->read(&buffer, 0, n, 0, true, true))
        return false;

    samples.resize(static_cast<size_t>(n));
    const auto* left = buffer.getReadPointer(0);
    if (buffer.getNumChannels() == 1)
        std::copy(left, left + n, samples.begin());
    else
    {
        const auto* right = buffer.getReadPointer(1);
        for (int i = 0; i < n; ++i)
            samples[(size_t)i] = 0.5f * (left[i] + right[i]);
    }
    sampleRate = reader->sampleRate;
    return true;
}

std::vector<float> makeEnvelope(const std::vector<float>& samples)
{
    const int frames = static_cast<int>(samples.size()) / envelopeHop;
    std::vector<float> envelope((size_t)juce::jmax(0, frames), 0.0f);
    for (int frame = 0; frame < frames; ++frame)
    {
        double energy = 0.0;
        const int start = frame * envelopeHop;
        for (int i = 0; i < envelopeHop; ++i)
        {
            const double x = samples[(size_t)(start + i)];
            energy += x * x;
        }
        envelope[(size_t)frame] = (float)std::sqrt(energy / envelopeHop);
    }
    return envelope;
}

int estimateBaseLag(const std::vector<float>& input,
                    const std::vector<float>& target,
                    float& bestCorrelation)
{
    const auto inputEnv = makeEnvelope(input);
    const auto targetEnv = makeEnvelope(target);
    const int inFrames = static_cast<int>(inputEnv.size());
    const int outFrames = static_cast<int>(targetEnv.size());
    const int shorter = juce::jmin(inFrames, outFrames);
    if (shorter < 32) { bestCorrelation = 0.0f; return 0; }

    const int maxLagFrames = juce::jmin(1024, shorter - 16);
    const int minimumOverlap = juce::jmax(32, shorter * 2 / 5);
    float bestAbs = -1.0f;
    int bestLagFrames = 0;
    bestCorrelation = 0.0f;

    for (int lag = -maxLagFrames; lag <= maxLagFrames; ++lag)
    {
        const int inStart = juce::jmax(0, -lag);
        const int outStart = juce::jmax(0, lag);
        const int overlap = juce::jmin(inFrames - inStart, outFrames - outStart);
        if (overlap < minimumOverlap) continue;

        double meanIn = 0.0, meanOut = 0.0;
        for (int i = 0; i < overlap; ++i)
        {
            meanIn += inputEnv[(size_t)(inStart + i)];
            meanOut += targetEnv[(size_t)(outStart + i)];
        }
        meanIn /= overlap; meanOut /= overlap;

        double dot = 0.0, eIn = 0.0, eOut = 0.0;
        for (int i = 0; i < overlap; ++i)
        {
            const double x = inputEnv[(size_t)(inStart + i)] - meanIn;
            const double y = targetEnv[(size_t)(outStart + i)] - meanOut;
            dot += x * y; eIn += x * x; eOut += y * y;
        }
        const float corr = (float)(dot / std::sqrt(juce::jmax(1.0e-24, eIn * eOut)));
        if (std::abs(corr) > bestAbs)
        {
            bestAbs = std::abs(corr); bestCorrelation = corr; bestLagFrames = lag;
        }
    }
    return bestLagFrames * envelopeHop;
}

int chooseScoreStart(const std::vector<float>& input, int targetSize, int baseLag)
{
    const int inputSize = static_cast<int>(input.size());
    if (inputSize <= scoreSamples) return 0;
    const int first = juce::jmax(prerollSamples, -baseLag + fineAlignmentLag);
    const int last = juce::jmin(inputSize - scoreSamples,
                                targetSize - scoreSamples - baseLag - fineAlignmentLag);
    if (last < first)
        return juce::jlimit(0, juce::jmax(0, inputSize - scoreSamples), inputSize / 3);

    int bestStart = first;
    double bestEnergy = -1.0;
    for (int start = first; start <= last; start += 1024)
    {
        double energy = 0.0;
        for (int i = 0; i < scoreSamples; ++i)
        {
            const double x = input[(size_t)(start + i)];
            energy += x * x;
        }
        if (energy > bestEnergy) { bestEnergy = energy; bestStart = start; }
    }
    return bestStart;
}

bool loadManifest(const juce::File& manifest, std::vector<Clip>& clips)
{
    std::ifstream in(manifest.getFullPathName().toStdString());
    if (!in) return false;

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    const auto baseDir = manifest.getParentDirectory();
    std::string line;

    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream row(line);
        std::string split, inputPath, targetPath;
        Clip clip;
        float b10 = 5.0f, m10 = 5.0f, t10 = 5.0f;
        if (!(row >> split >> clip.name >> b10 >> m10 >> t10 >> inputPath >> targetPath))
            return false;
        clip.fit = split == "fit";
        if (!clip.fit && split != "holdout") return false;
        clip.bass = juce::jlimit(0.0f, 1.0f, b10 / 10.0f);
        clip.middle = juce::jlimit(0.0f, 1.0f, m10 / 10.0f);
        clip.treble = juce::jlimit(0.0f, 1.0f, t10 / 10.0f);

        double inRate = 0.0, targetRate = 0.0;
        if (!loadMono(formats, baseDir.getChildFile(inputPath), clip.input, inRate)
            || !loadMono(formats, baseDir.getChildFile(targetPath), clip.target, targetRate)
            return false;
        if (std::abs(inRate - targetRate) > 0.5) return false;
        clip.sampleRate = inRate;

        float envCorr = 0.0f;
        clip.baseLag = estimateBaseLag(clip.input, clip.target, envCorr);
        clip.scoreStart = chooseScoreStart(clip.input, (int)clip.target.size(), clip.baseLag);
        clip.segmentStart = juce::jmax(0, clip.scoreStart - prerollSamples);
        std::cout << "ALIGN " << clip.name << " baseLag=" << clip.baseLag
                  << " envelopeCorr=" << envCorr << " scoreStart=" << clip.scoreStart << '\n';
        clips.push_back(std::move(clip));
    }

    const int fitCount = (int)std::count_if(clips.begin(), clips.end(), [](const Clip& c){ return c.fit; });
    const int holdoutCount = (int)clips.size() - fitCount;
    std::cout << "Loaded " << fitCount << " fit clips and " << holdoutCount << " holdout clips\n";
    return fitCount > 0 && holdoutCount > 0;
}

guitardsp::hq::AmpHQParams parametersFor(const Clip& clip, const Candidate& c)
{
    auto p = guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(clip.bass, clip.middle, clip.treble);
    static const std::array<int, 5> preampStages { 0, 2, 4, 6, 8 };
    for (const int index : preampStages)
    {
        auto& s = p.stage[(size_t)index];
        s.drive = juce::jlimit(0.15f, 12.0f, s.drive * c[0]);
        s.bias = juce::jlimit(-0.30f, 0.30f, s.bias + c[1]);
        s.preLpHz = juce::jlimit(1800.0f, 20000.0f, s.preLpHz * c[2]);
        s.postLpHz = juce::jlimit(1500.0f, 20000.0f, s.postLpHz * c[2]);
        s.asymmetry = juce::jlimit(0.0f, 1.0f, s.asymmetry * c[3]);
        s.memory = juce::jlimit(0.0f, 1.0f, s.memory * c[4]);
    }

    p.stage[10].output = juce::jlimit(0.1f, 30.0f, p.stage[10].output * c[5]);
    p.jvmToneStack.bassTaper = c[6];
    p.jvmToneStack.middleTaper = c[7];
    p.jvmToneStack.trebleTaper = c[8];
    p.jvmToneStack.r1Scale = c[9];
    p.jvmToneStack.c1Scale = c[10];
    p.jvmToneStack.c23Scale = c[11];
    return p;
}

RenderedClip renderClip(guitardsp::hq::AmpEngineHQ& amp, const Clip& clip, const Candidate& c)
{
    const int inputEnd = (int)clip.input.size();
    const int desiredScore = juce::jmin(scoreSamples, inputEnd - clip.scoreStart);
    const int segmentEnd = juce::jmin(inputEnd, clip.scoreStart + desiredScore);
    const int segmentLength = segmentEnd - clip.segmentStart;
    const int scoreOffset = clip.scoreStart - clip.segmentStart;

    juce::AudioBuffer<float> buffer(1, segmentLength);
    for (int i = 0; i < segmentLength; ++i)
        buffer.setSample(0, i, clip.input[(size_t)(clip.segmentStart + i)]);
    amp.setParameters(parametersFor(clip, c));
    amp.reset();
    amp.process(buffer);

    const int n = juce::jmin(desiredScore, segmentLength - scoreOffset);
    RenderedClip result;
    result.model.resize((size_t)n);
    for (int i = 0; i < n; ++i)
        result.model[(size_t)i] = buffer.getSample(0, scoreOffset + i);

    float bestAbs = -1.0f;
    int bestLag = clip.baseLag;
    for (int fine = -fineAlignmentLag; fine <= fineAlignmentLag; ++fine)
    {
        const int lag = clip.baseLag + fine;
        const int targetStart = clip.scoreStart + lag;
        if (targetStart < 0 || targetStart + n > (int)clip.target.size()) continue;
        double dot = 0.0, em = 0.0, et = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double x = result.model[(size_t)i];
            const double y = clip.target[(size_t)(targetStart + i)];
            dot += x * y; em += x * x; et += y * y;
        }
        const float corr = (float)(dot / std::sqrt(juce::jmax(1.0e-24, em * et)));
        if (std::abs(corr) > bestAbs) { bestAbs = std::abs(corr); bestLag = lag; }
    }

    const int targetStart = clip.scoreStart + bestLag;
    if (targetStart < 0 || targetStart + n > (int)clip.target.size())
    {
        result.model.clear();
        return result;
    }
    result.target.resize((size_t)n);
    for (int i = 0; i < n; ++i)
        result.target[(size_t)i] = clip.target[(size_t)(targetStart + i)];
    return result;
}

double spectralMaeDb(const std::vector<RenderedClip>& rendered, double sampleRate, double gain)
{
    juce::dsp::FFT fft(fftOrder);
    double errorSum = 0.0;
    int count = 0;
    for (const auto& clip : rendered)
    {
        if ((int)clip.model.size() < fftSize || (int)clip.target.size() < fftSize) continue;
        std::array<float, fftSize * 2> modelData {}, targetData {};
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * (float)i / (float)(fftSize - 1));
            modelData[(size_t)i] = (float)gain * clip.model[(size_t)i] * w;
            targetData[(size_t)i] = clip.target[(size_t)i] * w;
        }
        fft.performFrequencyOnlyForwardTransform(modelData.data());
        fft.performFrequencyOnlyForwardTransform(targetData.data());
        float peak = 1.0e-12f;
        for (int bin = 1; bin < fftSize / 2; ++bin) peak = juce::jmax(peak, targetData[(size_t)bin]);
        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const double hz = sampleRate * bin / (double)fftSize;
            if (hz < 70.0 || hz > 12000.0 || targetData[(size_t)bin] < peak * 0.001f) continue;
            const double md = 20.0 * std::log10(juce::jmax(1.0e-12f, modelData[(size_t)bin]));
            const double td = 20.0 * std::log10(juce::jmax(1.0e-12f, targetData[(size_t)bin]));
            errorSum += std::abs(md - td); ++count;
        }
    }
    return count > 0 ? errorSum / count : 99.0;
}

Metrics evaluate(const std::vector<Clip>& clips, bool fitSplit, const Candidate& c,
                 guitardsp::hq::AmpEngineHQ& amp)
{
    std::vector<RenderedClip> rendered;
    double sampleRate = 44100.0;
    for (const auto& clip : clips)
    {
        if (clip.fit != fitSplit) continue;
        sampleRate = clip.sampleRate;
        auto r = renderClip(amp, clip, c);
        if (!r.model.empty() && r.model.size() == r.target.size()) rendered.push_back(std::move(r));
    }

    double dot = 0.0, modelEnergy = 0.0;
    for (const auto& clip : rendered)
        for (size_t i = 0; i < clip.model.size(); ++i)
        {
            dot += (double)clip.model[i] * clip.target[i];
            modelEnergy += (double)clip.model[i] * clip.model[i];
        }
    double gain = modelEnergy > 1.0e-20 ? dot / modelEnergy : 1.0;
    gain = juce::jlimit(-100.0, 100.0, gain);
    if (std::abs(gain) < 0.01) gain = gain < 0.0 ? -0.01 : 0.01;

    double errorEnergy = 0.0, targetEnergy = 0.0, corrSum = 0.0;
    int corrCount = 0;
    for (const auto& clip : rendered)
    {
        double cdot = 0.0, cm = 0.0, ct = 0.0;
        for (size_t i = 0; i < clip.model.size(); ++i)
        {
            const double m = gain * clip.model[i];
            const double t = clip.target[i];
            const double e = m - t;
            errorEnergy += e * e; targetEnergy += t * t;
            cdot += m * t; cm += m * m; ct += t * t;
        }
        corrSum += cdot / std::sqrt(juce::jmax(1.0e-24, cm * ct)); ++corrCount;
    }

    Metrics m;
    const double nmse = errorEnergy / juce::jmax(1.0e-20, targetEnergy);
    m.nmseDb = 10.0 * std::log10(juce::jmax(1.0e-12, nmse));
    m.spectralMaeDb = spectralMaeDb(rendered, sampleRate, gain);
    m.correlation = corrCount > 0 ? corrSum / corrCount : 0.0;
    m.gain = gain;
    m.objective = std::sqrt(nmse) + 0.025 * m.spectralMaeDb;
    return m;
}

void printResult(const Candidate& c, const Metrics& m, const char* prefix)
{
    std::cout << std::fixed << std::setprecision(6) << prefix
              << " objective=" << m.objective << " nmseDb=" << m.nmseDb
              << " spectralMaeDb=" << m.spectralMaeDb << " correlation=" << m.correlation
              << " gain=" << m.gain << " gainDb=" << 20.0 * std::log10(juce::jmax(1.0e-12, std::abs(m.gain)));
    for (int i = 0; i < parameterCount; ++i) std::cout << ' ' << parameterNames[(size_t)i] << '=' << c[(size_t)i];
    std::cout << '\n';
}

Candidate fit(const std::vector<Clip>& clips, guitardsp::hq::AmpEngineHQ& amp, Metrics& bestMetrics)
{
    Candidate best = initialCandidate;
    Candidate steps = initialSteps;
    bestMetrics = evaluate(clips, true, best, amp);
    printResult(best, bestMetrics, "PHYS_START");

    for (int pass = 0; pass < 3; ++pass)
    {
        for (int p = 0; p < parameterCount; ++p)
        {
            const Candidate base = best;
            for (const float direction : { -1.0f, 1.0f })
            {
                Candidate trial = base;
                trial[(size_t)p] = juce::jlimit(lowerBounds[(size_t)p], upperBounds[(size_t)p],
                                                base[(size_t)p] + direction * steps[(size_t)p]);
                if (trial[(size_t)p] == base[(size_t)p]) continue;
                const auto metrics = evaluate(clips, true, trial, amp);
                if (metrics.objective < bestMetrics.objective)
                {
                    best = trial; bestMetrics = metrics;
                    printResult(best, bestMetrics, "PHYS_IMPROVED");
                }
            }
        }
        for (auto& step : steps) step *= 0.45f;
    }
    return best;
}

void writeReport(const juce::File& file, const Candidate& c, const Metrics& fitM, const Metrics& holdM)
{
    std::ofstream out(file.getFullPathName().toStdString(), std::ios::trunc);
    out << std::fixed << std::setprecision(8)
        << "{\n  \"target\": \"Marshall JVM410H OD1 Gain 5 - physical tone stack\",\n"
        << "  \"fit\": {\"objective\": " << fitM.objective << ", \"nmseDb\": " << fitM.nmseDb
        << ", \"spectralMaeDb\": " << fitM.spectralMaeDb << ", \"correlation\": " << fitM.correlation
        << ", \"gain\": " << fitM.gain << "},\n"
        << "  \"holdout\": {\"objective\": " << holdM.objective << ", \"nmseDb\": " << holdM.nmseDb
        << ", \"spectralMaeDb\": " << holdM.spectralMaeDb << ", \"correlation\": " << holdM.correlation
        << ", \"gain\": " << holdM.gain << "},\n  \"parameters\": {\n";
    for (int i = 0; i < parameterCount; ++i)
        out << "    \"" << parameterNames[(size_t)i] << "\": " << c[(size_t)i]
            << (i + 1 == parameterCount ? "\n" : ",\n");
    out << "  }\n}\n";
}
}

int main(int argc, char** argv)
{
    if (argc < 2) { std::cerr << "Usage: GuitarDSPJVMPhysicalFit <manifest.tsv> [report.json]\n"; return 2; }
    std::vector<Clip> clips;
    if (!loadManifest(juce::File(argv[1]), clips)) return 3;
    const double rate = clips.front().sampleRate;
    for (const auto& c : clips) if (std::abs(c.sampleRate - rate) > 0.5) return 4;

    guitardsp::hq::AmpEngineHQ amp;
    amp.prepare(rate, prerollSamples + scoreSamples + 512);
    Metrics fitMetrics;
    const auto best = fit(clips, amp, fitMetrics);
    const auto holdoutMetrics = evaluate(clips, false, best, amp);
    printResult(best, fitMetrics, "PHYS_FIT_RESULT");
    printResult(best, holdoutMetrics, "PHYS_HOLDOUT_RESULT");
    if (argc >= 3) writeReport(juce::File(argv[2]), best, fitMetrics, holdoutMetrics);
    return std::isfinite(fitMetrics.objective) && std::isfinite(holdoutMetrics.objective) ? 0 : 5;
}
