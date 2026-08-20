#pragma once
#include <JuceHeader.h>
#include <array>

namespace guitardsp::hq
{
struct FactoryIrCatalog
{
    static constexpr int count = 15;

    static const std::array<const char*, count>& fileNames() noexcept
    {
        static const std::array<const char*, count> names {
            "1_Cookie_Monster.wav",
            "2_Darth_Genocider.wav",
            "3_Kitten_Slayer.wav",
            "4_Kaiju_Tamer.wav",
            "5_Iceburn_Suicide.wav",
            "6_Vertical_Lip_Stabber.wav",
            "7_Manslaughter_Joe.wav",
            "8_Big_Bubba.wav",
            "9_Devils_Cunnilingus.wav",
            "10_October_32th.wav",
            "11_Wumbo.wav",
            "12_World_Collider.wav",
            "13_Cannibal_Choir.wav",
            "14_Cathode_Ray_Fleshburn.wav",
            "15_Impaler_Jim.wav"
        };
        return names;
    }

    static int indexForFileName(const juce::String& name) noexcept
    {
        for (int i = 0; i < count; ++i)
            if (name.equalsIgnoreCase(fileNames()[(size_t)i]))
                return i;
        return -1;
    }

    static int indexForFile(const juce::File& file) noexcept
    {
        return indexForFileName(file.getFileName());
    }

    static juce::String displayName(int index)
    {
        index = juce::jlimit(0, count - 1, index);
        juce::String name(fileNames()[(size_t)index]);
        name = name.upToLastOccurrenceOf(".wav", false, true);
        const int underscore = name.indexOfChar('_');
        if (underscore >= 0)
            name = name.substring(underscore + 1);
        return juce::String(index + 1).paddedLeft('0', 2) + "  " + name.replaceCharacter('_', ' ');
    }

    static juce::File userDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
            .getChildFile("GuitarDSP-Pro").getChildFile("FactoryIR");
    }

    static juce::File executableDirectory()
    {
        return juce::File::getSpecialLocation(juce::File::currentExecutableFile)
            .getParentDirectory().getChildFile("FactoryIR");
    }

    static juce::File workingDirectory()
    {
        return juce::File::getCurrentWorkingDirectory().getChildFile("FactoryIR");
    }

    static juce::File fileForIndex(int index)
    {
        index = juce::jlimit(0, count - 1, index);
        const juce::String name(fileNames()[(size_t)index]);
        for (const auto& directory : { userDirectory(), executableDirectory(), workingDirectory() })
        {
            const auto candidate = directory.getChildFile(name);
            if (candidate.existsAsFile())
                return candidate;
        }
        return executableDirectory().getChildFile(name);
    }

    static bool allAvailable()
    {
        for (int i = 0; i < count; ++i)
            if (!fileForIndex(i).existsAsFile())
                return false;
        return true;
    }

    static juce::File resolveFile(const juce::File& requested)
    {
        if (requested.existsAsFile())
            return requested;
        const int index = indexForFile(requested);
        return index >= 0 ? fileForIndex(index) : requested;
    }

    static juce::File installStableCopy(const juce::File& source)
    {
        const int index = indexForFile(source);
        if (index < 0 || !source.existsAsFile())
            return source;

        auto directory = userDirectory();
        if (!directory.isDirectory())
            directory.createDirectory();
        const auto destination = directory.getChildFile(fileNames()[(size_t)index]);
        if (destination == source)
            return source;

        if (!destination.existsAsFile() || destination.getSize() != source.getSize())
        {
            destination.deleteFile();
            if (!source.copyFileTo(destination))
                return source;
        }
        return destination.existsAsFile() ? destination : source;
    }
};
}
