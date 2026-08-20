#include <JuceHeader.h>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include "../hq_preload/dsp/amp/AmpEngineHQ.h"
#include "../hq_preload/dsp/cab/CabMicEngineHQ.h"
#include "../hq_preload/dsp/cab/FactoryIrCatalog.h"

namespace
{
constexpr double sampleRate = 48000.0;
constexpr int blockSize = 256;

bool require(bool condition, const char* name)
{
    std::cout << (condition ? "PASS " : "FAIL ") << name << '\n';
    return condition;
}

float rms(const juce::AudioBuffer<float>& b)
{
    double sum = 0.0;
    int count = 0;
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const auto* d = b.getReadPointer(ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            sum += (double)d[i] * (double)d[i];
            ++count;
        }
    }
    return count > 0 ? (float)std::sqrt(sum / (double)count) : 0.0f;
}

bool sane(const juce::AudioBuffer<float>& b, float maxAbs = 20.0f)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const auto* d = b.getReadPointer(ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (!std::isfinite(d[i]) || std::abs(d[i]) > maxAbs) return false;
    }
    return true;
}

void fillContinuousSine(juce::AudioBuffer<float>& b, float amplitude, float hz, int blockIndex)
{
    const double start = (double)blockIndex * (double)b.getNumSamples();
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        auto* d = b.getWritePointer(ch);
        for (int i = 0; i < b.getNumSamples(); ++i)
            d[i] = amplitude * (float)std::sin(juce::MathConstants<double>::twoPi * hz * (start + i) / sampleRate);
    }
}

float measureAmpGain(guitardsp::hq::AmpEngineHQ& amp, float amplitude, float hz)
{
    amp.reset();
    juce::AudioBuffer<float> b(1, blockSize);
    float inputRms = amplitude / std::sqrt(2.0f);
    for (int block = 0; block < 48; ++block)
    {
        fillContinuousSine(b, amplitude, hz, block);
        amp.process(b);
    }
    return rms(b) / juce::jmax(1.0e-9f, inputRms);
}

void writeLe16(std::ofstream& out, std::uint16_t value)
{
    const char bytes[2] = { (char)(value & 0xffu), (char)((value >> 8) & 0xffu) };
    out.write(bytes, 2);
}

void writeLe32(std::ofstream& out, std::uint32_t value)
{
    const char bytes[4] = { (char)(value & 0xffu), (char)((value >> 8) & 0xffu),
                            (char)((value >> 16) & 0xffu), (char)((value >> 24) & 0xffu) };
    out.write(bytes, 4);
}

bool writeImpulseWav(const juce::File& file)
{
    constexpr std::uint32_t frames = 4096;
    constexpr std::uint16_t channels = 1;
    constexpr std::uint16_t bits = 16;
    constexpr std::uint32_t rate = 48000;
    constexpr std::uint32_t dataBytes = frames * channels * (bits / 8);

    std::ofstream out(file.getFullPathName().toStdString(), std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write("RIFF", 4); writeLe32(out, 36u + dataBytes); out.write("WAVE", 4);
    out.write("fmt ", 4); writeLe32(out, 16); writeLe16(out, 1); writeLe16(out, channels);
    writeLe32(out, rate); writeLe32(out, rate * channels * (bits / 8));
    writeLe16(out, channels * (bits / 8)); writeLe16(out, bits);
    out.write("data", 4); writeLe32(out, dataBytes);
    for (std::uint32_t i = 0; i < frames; ++i)
    {
        const std::int16_t sample = i == 0 ? (std::int16_t)32767 : (i == 1 ? (std::int16_t)-4096 : (std::int16_t)0);
        writeLe16(out, (std::uint16_t)sample);
    }
    return (bool)out;
}

bool validateFactoryCatalog()
{
    bool ok = true;
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    ok &= require(guitardsp::hq::FactoryIrCatalog::count == 15, "Factory IR catalog exposes 15 measured captures");
    for (int i = 0; i < guitardsp::hq::FactoryIrCatalog::count; ++i)
    {
        const auto file = guitardsp::hq::FactoryIrCatalog::fileForIndex(i);
        if (!file.existsAsFile())
        {
            std::cout << "FAIL missing Factory IR " << i << " path=" << file.getFullPathName() << '\n';
            ok = false;
            continue;
        }
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(file));
        const bool valid = reader != nullptr
                        && std::abs(reader->sampleRate - 48000.0) < 0.5
                        && reader->numChannels == 1
                        && reader->bitsPerSample == 24
                        && reader->lengthInSamples > 2048;
        if (!valid)
        {
            std::cout << "FAIL invalid Factory IR " << file.getFileName() << '\n';
            ok = false;
        }
    }
    ok &= require(ok, "All Factory IR WAVs are 48 kHz mono 24-bit measured captures");
    return ok;
}
}

int main()
{
    bool ok = true;

    const auto reference = guitardsp::hq::AmpEngineHQ::makeBassman5F6AReference();
    ok &= require(reference.midDb < reference.bassDb - 6.0f && reference.midDb < reference.trebleDb - 6.0f,
                  "5F6-A reference keeps broad mid scoop");
    ok &= require(reference.sag > 0.25f && reference.sagRecoveryMs >= 100.0f,
                  "5F6-A reference includes rectifier-style sag memory");

    guitardsp::hq::AmpEngineHQ amp;
    amp.prepare(sampleRate, blockSize);
    amp.setParameters(reference);
    const float lowLevelGain = measureAmpGain(amp, 0.008f, 750.0f);
    const float highLevelGain = measureAmpGain(amp, 0.45f, 750.0f);
    std::cout << "INFO 5F6-A low-level gain " << lowLevelGain << " high-level gain " << highLevelGain << '\n';
    ok &= require(std::isfinite(lowLevelGain) && std::isfinite(highLevelGain) && lowLevelGain > 1.0e-4f,
                  "5F6-A reference produces finite output");
    ok &= require(highLevelGain < lowLevelGain * 0.98f,
                  "5F6-A reference compresses as drive rises");

    const float lowBand = measureAmpGain(amp, 0.002f, 110.0f);
    const float midBand = measureAmpGain(amp, 0.002f, 720.0f);
    const float highBand = measureAmpGain(amp, 0.002f, 3200.0f);
    std::cout << "INFO 5F6-A response gains low=" << lowBand << " mid=" << midBand << " high=" << highBand << '\n';
    ok &= require(midBand < juce::jmax(lowBand, highBand) * 0.90f,
                  "5F6-A reference response retains mid recess");

    const auto temp = juce::File::getSpecialLocation(juce::File::tempDirectory)
                        .getNonexistentChildFile("guitardsp-user-ir-smoke", ".wav", false);
    ok &= require(writeImpulseWav(temp), "Create temporary user IR WAV");
    {
        guitardsp::hq::CabMicEngineHQ cab;
        cab.prepare(sampleRate, blockSize);
        auto cp = cab.getParameters();
        cp.irEngine = guitardsp::hq::CabIrEngine::external;
        cp.externalIrSize = guitardsp::hq::ExternalIrSize::samples2048;
        cp.lowCutHz = 25.0f;
        cp.highCutHz = 18000.0f;
        cp.mix = 1.0f;
        cab.setParameters(cp);
        ok &= require(cab.loadExternalImpulse(temp), "User IR WAV accepted");
        cab.setEnabled(true);

        juce::AudioBuffer<float> b(2, blockSize);
        int blocksProcessed = 0;
        for (; blocksProcessed < 160 && cab.getCurrentIrSize() != 2048; ++blocksProcessed)
        {
            fillContinuousSine(b, 0.08f, 440.0f, blocksProcessed);
            cab.process(b, 0, blockSize);
            juce::Thread::sleep(2);
        }
        std::cout << "INFO user IR size " << cab.getCurrentIrSize() << " after " << blocksProcessed << " blocks\n";
        ok &= require(cab.getCurrentIrSize() == 2048, "User IR resampled/truncated to 2048 samples");

        for (int block = 0; block < 24; ++block)
        {
            fillContinuousSine(b, 0.08f, 440.0f, blocksProcessed + block);
            cab.process(b, 0, blockSize);
        }
        ok &= require(sane(b) && rms(b) > 1.0e-7f, "User IR convolution finite and active");
    }
    temp.deleteFile();

    ok &= validateFactoryCatalog();
    {
        guitardsp::hq::CabMicEngineHQ cab;
        cab.prepare(sampleRate, blockSize);
        auto cp = cab.getParameters();
        cp.irEngine = guitardsp::hq::CabIrEngine::external;
        cp.externalIrSize = guitardsp::hq::ExternalIrSize::samples2048;
        cp.lowCutHz = 25.0f;
        cp.highCutHz = 18000.0f;
        cp.mix = 1.0f;
        cab.setParameters(cp);
        const auto factoryFile = guitardsp::hq::FactoryIrCatalog::fileForIndex(0);
        ok &= require(cab.loadExternalImpulse(factoryFile), "Factory measured IR accepted by convolution engine");
        cab.setEnabled(true);

        juce::AudioBuffer<float> b(2, blockSize);
        int blocksProcessed = 0;
        for (; blocksProcessed < 160 && cab.getCurrentIrSize() != 2048; ++blocksProcessed)
        {
            fillContinuousSine(b, 0.08f, 440.0f, blocksProcessed);
            cab.process(b, 0, blockSize);
            juce::Thread::sleep(2);
        }
        std::cout << "INFO factory IR size " << cab.getCurrentIrSize() << " after " << blocksProcessed << " blocks\n";
        ok &= require(cab.getCurrentIrSize() == 2048, "Factory IR uses selected 2048-sample mode");
        for (int block = 0; block < 24; ++block)
        {
            fillContinuousSine(b, 0.08f, 440.0f, blocksProcessed + block);
            cab.process(b, 0, blockSize);
        }
        ok &= require(sane(b) && rms(b) > 1.0e-7f, "Factory IR convolution finite and active");
    }

    return ok ? 0 : 1;
}
