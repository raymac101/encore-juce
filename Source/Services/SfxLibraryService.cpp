/*
  ==============================================================================

    SfxLibraryService.cpp

  ==============================================================================
*/

#include "SfxLibraryService.h"
#include "../UI/SpriteIcon.h"
#include <algorithm>

namespace
{
    // Lowercases and strips everything but letters/digits, then drops
    // trailing digits (handles "Boing2" -> "boing", so it can still match
    // an icon literally named "Boing.svg"). Not a perfect matcher -- with
    // ~113 sounds and ~30 icons most sounds simply won't match anything,
    // which is expected; this only needs to catch the easy cases.
    juce::String normalize (const juce::String& s)
    {
        juce::String out;
        for (auto c : s)
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                out += juce::String::charToString (juce::CharacterFunctions::toLowerCase (c));

        while (out.isNotEmpty() && juce::CharacterFunctions::isDigit (out.getLastCharacter()))
            out = out.dropLastCharacters (1);

        return out;
    }
}

SfxLibraryService& SfxLibraryService::getInstance()
{
    static SfxLibraryService instance;
    return instance;
}

void SfxLibraryService::ensureScanned()
{
    if (scanned_)
        return;
    scanned_ = true;

    const auto soundsDir = SpriteIcon::resolveAssetDirectory ("assets/sounds");
    if (! soundsDir.isDirectory())
        return;

    const auto iconsDir = SpriteIcon::resolveAssetDirectory ("assets/sound-icons");
    juce::Array<juce::File> iconFiles;
    if (iconsDir.isDirectory())
        iconFiles = iconsDir.findChildFiles (juce::File::findFiles, false, "*.svg");

    const auto soundFiles = soundsDir.findChildFiles (juce::File::findFiles, false, "*.wav");

    sounds_.reserve ((size_t) soundFiles.size());
    for (auto& soundFile : soundFiles)
    {
        Entry e;
        e.name = soundFile.getFileNameWithoutExtension();
        e.soundFile = soundFile;

        const auto normSound = normalize (e.name);
        if (normSound.isNotEmpty())
        {
            for (auto& iconFile : iconFiles)
            {
                const auto normIcon = normalize (iconFile.getFileNameWithoutExtension());
                if (normIcon.isEmpty())
                    continue;

                if (normIcon == normSound
                    || normSound.startsWith (normIcon)
                    || normIcon.startsWith (normSound))
                {
                    e.iconFile = iconFile;
                    break;
                }
            }
        }

        sounds_.push_back (std::move (e));
    }

    std::sort (sounds_.begin(), sounds_.end(), [] (const Entry& a, const Entry& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });
}

const std::vector<SfxLibraryService::Entry>& SfxLibraryService::getAllSounds()
{
    ensureScanned();
    return sounds_;
}

const SfxLibraryService::Entry* SfxLibraryService::findByName (const juce::String& name)
{
    ensureScanned();
    for (auto& e : sounds_)
        if (e.name.equalsIgnoreCase (name))
            return &e;
    return nullptr;
}
