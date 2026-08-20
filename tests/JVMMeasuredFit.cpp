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
constexpr int maxAlignmentLag = 192;
constexpr int parameterCount = 11;

struct Clip
{
    bool fit = false;
    std::string name;
    float bass = 0.5f, middle = 0.5f, treble = 0.5f;
    double sampleRate = 44100.0;
    std::vector<float> input;
    std::vector<float> target;
    int segmentStart = 0;
    int scoreStart = 0;
};

struct RenderedClip
{
    std::vector<float> model;
    std::vector<float> target;
    int lag = 0;
    float correlation = 0.0f;
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
    "bassOffsetDb", "midOffsetDb", "trebleOffsetDb",
    "bassSlopeDb", "midSlopeDb", "trebleSlopeDb"
};

const Candidate initialCandidate { 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
const Candidate lowerBounds { 0.55f, -0.08f, 0.65f, 0.55f, 0.45f, -7.0f, -7.0f, -7.0f, -10.0f, -10.0f, -10.0f };
const Candidate upperBounds { 1.65f,  0.08f, 1.35f, 1.65f, 1.90f,  7.0f,  7.0f,  7.0f,  10.0f,  10.0f,  10.0f };
const Candidate initialSteps { 0.20f, 0.025f, 0.12f, 0.20f, 0.28f, 1.8f, 1.8f, 1.8f, 2.8f, 2.8f, 2.8f };

bool loadMono(juce::AudioFormatManager& formats,
              const juce::File& file,
              std::vector<float>& samples,
              double& sampleRate)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
    if (!reader || reader->lengthInSamples <= 0 || reader->numChannels == 0)
    {
        std::cerr << "Could not open " << file.getFullPathName() << '\n';
        return false;
    }

    if (reader->lengthInSamples > 44100LL * 120LL)
    {
        std::cerr << "Refusing unexpectedly long listening clip " << file.getFileName() << '\n';
        return false;
    }

    const int n = static_cast<int>(reader->lengthInSamples);
    juce::AudioBuffer<float> buffer(static_cast<int>(reader->numChannels), n);
    if (!reader->read(&buffer, 0, n, 0, true, true))
        return false;

    samples.resize(static_cast<size_t>(n));
    const auto* left = buffer.getReadPointer(0);
    if (buffer.getNumChannels() == 1)
    {
        std::copy(left, left + n, samples.begin());
    }
    else
    {
        const auto* right = buffer.getReadPointer(1);
        for (int i = 0; i < n; ++i)
            samples[static_cast<size_t>(i)] = 0.5f * (left[i] + right[i]);
    }

    sampleRate = reader->sampleRate;
    return true;
}

int chooseScoreStart(const std::vector<float>& input, int nAvailable)
{
    const int n = juce::jmin(static_cast<int>(input.size()), nAvailable);
    if (n <= scoreSamples)
        return 0;

    const int first = juce::jmin(prerollSamples, juce::jmax(0, n - scoreSamples));
    int bestStart = first;
    double bestEnergy = -1.0;

    for (int start = first; start + scoreSamples <= n; start += 1024)
    {
        double energy = 0.0;
        for (int i = 0; i < scoreSamples; ++i)
        {
            const double x = input[static_cast<size_t>(start + i)];
            energy += x * x;
        }
        if (energy > bestEnergy)
        {
            bestEnergy = energy;
            bestStart = start;
        }
    }
    return bestStart;
}

bool loadManifest(const juce::File& manifest, std::vector<Clip>& clips)
{
    std::ifstream in(manifest.getFullPathName().toStdString());
    if (!in)
    {
        std::cerr << "Could not open manifest: " << manifest.getFullPathName() << '\n';
        return false;
    }

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    const auto baseDir = manifest.getParentDirectory();
    std::string line;
    while (std::getline(in, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        std::istringstream row(line);
        std::string split, inputPath, targetPath;
        Clip clip;
        float bass10 = 5.0f, middle10 = 5.0f, treble10 = 5.0f;
        if (!(row >> split >> clip.name >> bass10 >> middle10 >> treble10 >> inputPath >> targetPath))
        {
            std::cerr << "Malformed manifest line: " << line << '\n';
            return false;
        }

        clip.fit = split == "fit";
        if (!clip.fit && split != "holdout")
        {
            std::cerr << "Manifest split must be fit or holdout: " << split << '\n';
            return false;
        }
        clip.bass = juce::jlimit(0.0f, 1.0f, bass10 / 10.0f);
        clip.middle = juce::jlimit(0.0f, 1.0f, middle10 / 10.0f);
        clip.treble = juce::jlimit(0.0f, 1.0f, treble10 / 10.0f);

        double inRate = 0.0, targetRate = 0.0;
        if (!loadMono(formats, baseDir.getChildFile(inputPath), clip.input, inRate)
            || !loadMono(formats, baseDir.getChildFile(targetPath), clip.target, targetRate))
            return false;

        if (std::abs(inRate - targetRate) > 0.5)
        {
            std::cerr << "Input/reference sample-rate mismatch in " << clip.name << '\n';
            return false;
        }

        clip.sampleRate = inRate;
        const int available = juce::jmin(static_cast<int>(clip.input.size()), static_cast<int>(clip.target.size()));
        if (available < 2048)
        {
            std::cerr << "Clip too short: " << clip.name << '\n';
            return false;
        }
        clip.scoreStart = chooseScoreStart(clip.input, available);
        clip.segmentStart = juce::jmax(0, clip.scoreStart - prerollSamples);
        clips.push_back(std::move(clip));
    }

    const auto fitCount = std::count_if(clips.begin(), clips.end(), [](const Clip& c) { return c.fit; });
    const auto holdoutCount = static_cast<int>(clips.size()) - static_cast<int>(fitCount);
    std::cout << "Loaded " << fitCount << " fit clips and " << holdoutCount << " holdout clips\n";
    return fitCount > 0 && holdoutCount > 0;
}

guitardsp::hq::AmpHQParams parametersFor(const Clip& clip, const Candidate& c)
{
    auto p = guitardsp::hq::AmpEngineHQ::makeJVM410HOD1Reference(clip.bass, clip.middle, clip.treble);
    static const std::array<int, 5> preampStages { 0, 2, 4, 6, 8 };

    for (const int index : preampStages)
    {
        auto& s = p.stage[static_cast<size_t>(index)];
        s.drive = juce::jlimit(0.15f, 12.0f, s.drive * c[0]);
        s.bias = juce::jlimit(-0.30f, 0.30f, s.bias + c[1]);
        s.preLpHz = juce::jlimit(1800.0f, 20000.0f, s.preLpHz * c[2]);
        s.postLpHz = juce::jlimit(1500.0f, 20000.0f, s.postLpHz * c[2]);
        s.asymmetry = juce::jlimit(0.0f, 1.0f, s.asymmetry * c[3]);
        s.memory = juce::jlimit(0.0f, 1.0f, s.memory * c[4]);
    }

    p.bassDb += c[5] + c[8] * (clip.bass - 0.5f);
    p.midDb += c[6] + c[9] * (clip.middle - 0.5f);
    p.trebleDb += c[7] + c[10] * (clip.treble - 0.5f);
    return p;
}

RenderedClip renderClip(guitardsp::hq::AmpEngineHQ& amp, const Clip& clip, const Candidate& c)
{
    const int targetEnd = juce::jmin(static_cast<int>(clip.input.size()), static_cast<int>(clip.target.size()));
    const int desiredScore = juce::jmin(scoreSamples, targetEnd - clip.scoreStart);
    const int segmentEnd = juce::jmin(targetEnd, clip.scoreStart + desiredScore);
    const int segmentLength = segmentEnd - clip.segmentStart;
    const int scoreOffset = clip.scoreStart - clip.segmentStart;

    juce::AudioBuffer<float> buffer(1, segmentLength);
    for (int i = 0; i < segmentLength; ++i)
        buffer.setSample(0, i, clip.input[static_cast<size_t>(clip.segmentStart + i)]);

    amp.setParameters(parametersFor(clip, c));
    amp.reset();
    amp.process(buffer);

    const int n = juce::jmin(desiredScore, segmentLength - scoreOffset);
    RenderedClip rendered;
    rendered.model.resize(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i)
        rendered.model[static_cast<size_t>(i)] = buffer.getSample(0, scoreOffset + i);

    float bestAbsCorrelation = -1.0f;
    int bestLag = 0;
    float bestCorrelation = 0.0f;
    for (int lag = -maxAlignmentLag; lag <= maxAlignmentLag; ++lag)
    {
        const int targetStart = clip.scoreStart + lag;
        if (targetStart < 0 || targetStart + n > static_cast<int>(clip.target.size()))
            continue;

        double dot = 0.0, modelEnergy = 0.0, targetEnergy = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double x = rendered.model[static_cast<size_t>(i)];
            const double y = clip.target[static_cast<size_t>(targetStart + i)];
            dot += x * y;
            modelEnergy += x * x;
            targetEnergy += y * y;
        }
        const float correlation = static_cast<float>(dot / std::sqrt(juce::jmax(1.0e-24, modelEnergy * targetEnergy)));
        if (std::abs(correlation) > bestAbsCorrelation)
        {
            bestAbsCorrelation = std::abs(correlation);
            bestCorrelation = correlation;
            bestLag = lag;
        }
    }

    rendered.lag = bestLag;
    rendered.correlation = bestCorrelation;
    rendered.target.resize(static_cast<size_t>(n));
    const int targetStart = clip.scoreStart + bestLag;
    for (int i = 0; i < n; ++i)
        rendered.target[static_cast<size_t>(i)] = clip.target[static_cast<size_t>(targetStart + i)];
    return rendered;
}

double spectralMaeDb(const std::vector<RenderedClip>& rendered, double sampleRate, double gain)
{
    juce::dsp::FFT fft(fftOrder);
    double errorSum = 0.0;
    int count = 0;

    for (const auto& clip : rendered)
    {
        if (static_cast<int>(clip.model.size()) < fftSize || static_cast<int>(clip.target.size()) < fftSize)
            continue;

        std::array<float, fftSize * 2> modelData {};
        std::array<float, fftSize * 2> targetData {};
        for (int i = 0; i < fftSize; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::twoPi * static_cast<float>(i)
                                                  / static_cast<float>(fftSize - 1));
            modelData[static_cast<size_t>(i)] = static_cast<float>(gain) * clip.model[static_cast<size_t>(i)] * w;
            targetData[static_cast<size_t>(i)] = clip.target[static_cast<size_t>(i)] * w;
        }

        fft.performFrequencyOnlyForwardTransform(modelData.data());
        fft.performFrequencyOnlyForwardTransform(targetData.data());

        float targetPeak = 1.0e-12f;
        for (int bin = 1; bin < fftSize / 2; ++bin)
            targetPeak = juce::jmax(targetPeak, targetData[static_cast<size_t>(bin)]);

        for (int bin = 1; bin < fftSize / 2; ++bin)
        {
            const double hz = sampleRate * static_cast<double>(bin) / static_cast<double>(fftSize);
            if (hz < 70.0 || hz > 12000.0)
                continue;

            const float targetMag = targetData[static_cast<size_t>(bin)];
            if (targetMag < targetPeak * 0.001f)
                continue;

            const double modelDb = 20.0 * std::log10(juce::jmax(1.0e-12f, modelData[static_cast<size_t>(bin)]));
            const double targetDb = 20.0 * std::log10(juce::jmax(1.0e-12f, targetMag));
            errorSum += std::abs(modelDb - targetDb);
            ++count;
        }
    }
    return count > 0 ? errorSum / static_cast<double>(count) : 99.0;
}

Metrics evaluate(const std::vector<Clip>& clips,
                 bool fitSplit,
                 const Candidate& c,
                 guitardsp::hq::AmpEngineHQ& amp)
{
    std::vector<RenderedClip> rendered;
    double sampleRate = 44100.0;
    for (const auto& clip : clips)
    {
        if (clip.fit != fitSplit)
            continue;
        sampleRate = clip.sampleRate;
        rendered.push_back(renderClip(amp, clip, c));
    }

    double dot = 0.0, modelEnergy = 0.0;
    for (const auto& clip : rendered)
    {
        for (size_t i = 0; i < clip.model.size(); ++i)
        {
            dot += static_cast<double>(clip.model[i]) * static_cast<double>(clip.target[i]);
            modelEnergy += static_cast<double>(clip.model[i]) * static_cast<double>(clip.model[i]);
        }
    }

    double gain = modelEnergy > 1.0e-20 ? dot / modelEnergy : 1.0;
    gain = juce::jlimit(-100.0, 100.0, gain);
    if (std::abs(gain) < 0.01)
        gain = gain < 0.0 ? -0.01 : 0.01;

    double errorEnergy = 0.0, targetEnergy = 0.0, correlationSum = 0.0;
    int correlationCount = 0;
    for (const auto& clip : rendered)
    {
        double clipDot = 0.0, clipModelEnergy = 0.0, clipTargetEnergy = 0.0;
        for (size_t i = 0; i < clip.model.size(); ++i)
        {
            const double model = gain * static_cast<double>(clip.model[i]);
            const double target = static_cast<double>(clip.target[i]);
            const double error = model - target;
            errorEnergy += error * error;
            targetEnergy += target * target;
            clipDot += model * target;
            clipModelEnergy += model * model;
            clipTargetEnergy += target * target;
        }
        correlationSum += clipDot / std::sqrt(juce::jmax(1.0e-24, clipModelEnergy * clipTargetEnergy));
        ++correlationCount;
    }

    Metrics metrics;
    const double nmse = errorEnergy / juce::jmax(1.0e-20, targetEnergy);
    metrics.nmseDb = 10.0 * std::log10(juce::jmax(1.0e-12, nmse));
    metrics.spectralMaeDb = spectralMaeDb(rendered, sampleRate, gain);
    metrics.correlation = correlationCount > 0 ? correlationSum / static_cast<double>(correlationCount) : 0.0;
    metrics.gain = gain;
    metrics.objective = std::sqrt(nmse) + 0.025 * metrics.spectralMaeDb;
    return metrics;
}

void printCandidate(const Candidate& c, const Metrics& metrics, const char* prefix)
{
    std::cout << std::fixed << std::setprecision(6)
              << prefix
              << " objective=" << metrics.objective
              << " nmseDb=" << metrics.nmseDb
              << " spectralMaeDb=" << metrics.spectralMaeDb
              << " correlation=" << metrics.correlation
              << " gain=" << metrics.gain
              << " gainDb=" << 20.0 * std::log10(juce::jmax(1.0e-12, std::abs(metrics.gain)));
    for (int i = 0; i < parameterCount; ++i)
        std::cout << ' ' << parameterNames[static_cast<size_t>(i)] << '=' << c[static_cast<size_t>(i)];
    std::cout << '\n';
}

Candidate fitParameters(const std::vector<Clip>& clips,
                        guitardsp::hq::AmpEngineHQ& amp,
                        Metrics& bestMetrics)
{
    Candidate best = initialCandidate;
    Candidate steps = initialSteps;
    bestMetrics = evaluate(clips, true, best, amp);
    printCandidate(best, bestMetrics, "FIT_START");

    for (int pass = 0; pass < 2; ++pass)
    {
        for (int parameter = 0; parameter < parameterCount; ++parameter)
        {
            const auto baseline = best;
            const auto baselineMetrics = bestMetrics;
            for (const float direction : { -1.0f, 1.0f })
            {
                Candidate trial = baseline;
                trial[static_cast<size_t>(parameter)] =
                    juce::jlimit(lowerBounds[static_cast<size_t>(parameter)],
                                 upperBounds[static_cast<size_t>(parameter)],
                                 baseline[static_cast<size_t>(parameter)]
                                     + direction * steps[static_cast<size_t>(parameter)]);

                if (trial[static_cast<size_t>(parameter)] == baseline[static_cast<size_t>(parameter)])
                    continue;

                const auto trialMetrics = evaluate(clips, true, trial, amp);
                if (trialMetrics.objective < bestMetrics.objective)
                {
                    best = trial;
                    bestMetrics = trialMetrics;
                    printCandidate(best, bestMetrics, "FIT_IMPROVED");
                }
            }

            if (bestMetrics.objective > baselineMetrics.objective)
            {
                best = baseline;
                bestMetrics = baselineMetrics;
            }
        }
        for (auto& step : steps)
            step *= 0.45f;
    }
    return best;
}

void writeJsonReport(const juce::File& file,
                     const Candidate& best,
                     const Metrics& fit,
                     const Metrics& holdout)
{
    std::ofstream out(file.getFullPathName().toStdString(), std::ios::trunc);
    out << std::fixed << std::setprecision(8)
        << "{\n"
        << "  \"target\": \"Marshall JVM410H OD1 Gain 5\",\n"
        << "  \"fit\": {\"objective\": " << fit.objective
        << ", \"nmseDb\": " << fit.nmseDb
        << ", \"spectralMaeDb\": " << fit.spectralMaeDb
        << ", \"correlation\": " << fit.correlation
        << ", \"gain\": " << fit.gain << "},\n"
        << "  \"holdout\": {\"objective\": " << holdout.objective
        << ", \"nmseDb\": " << holdout.nmseDb
        << ", \"spectralMaeDb\": " << holdout.spectralMaeDb
        << ", \"correlation\": " << holdout.correlation
        << ", \"gain\": " << holdout.gain << "},\n"
        << "  \"parameters\": {\n";
    for (int i = 0; i < parameterCount; ++i)
    {
        out << "    \"" << parameterNames[static_cast<size_t>(i)] << "\": "
            << best[static_cast<size_t>(i)] << (i + 1 == parameterCount ? "\n" : ",\n");
    }
    out << "  }\n}\n";
}
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        std::cerr << "Usage: GuitarDSPJVMFit <manifest.tsv> [report.json]\n";
        return 2;
    }

    std::vector<Clip> clips;
    const juce::File manifest(argv[1]);
    if (!loadManifest(manifest, clips))
        return 3;

    double rate = clips.front().sampleRate;
    for (const auto& clip : clips)
    {
        if (std::abs(clip.sampleRate - rate) > 0.5)
        {
            std::cerr << "All clips must use the same sample rate\n";
            return 4;
        }
    }

    guitardsp::hq::AmpEngineHQ amp;
    amp.prepare(rate, prerollSamples + scoreSamples + 512);

    Metrics fitMetrics;
    const Candidate best = fitParameters(clips, amp, fitMetrics);
    const Metrics holdoutMetrics = evaluate(clips, false, best, amp);

    printCandidate(best, fitMetrics, "FIT_RESULT");
    printCandidate(best, holdoutMetrics, "HOLDOUT_RESULT");

    if (argc >= 3)
        writeJsonReport(juce::File(argv[2]), best, fitMetrics, holdoutMetrics);

    if (!std::isfinite(fitMetrics.objective) || !std::isfinite(holdoutMetrics.objective))
        return 5;
    return 0;
}
