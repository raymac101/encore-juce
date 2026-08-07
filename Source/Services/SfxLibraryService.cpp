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
    // an icon literally named "Boing.svg"). Only used as a fallback for
    // sounds that aren't in kIconMap below.
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

    // Same idea, but keeps digits -- trailing-digit-stripping would collide
    // "Fail2"/"Fail3"/"Fail4" (each hand-assigned a *different* icon) onto
    // the same key as "Fail". Used only for matching against kIconMap.
    juce::String normalizeKeepDigits (const juce::String& s)
    {
        juce::String out;
        for (auto c : s)
            if (juce::CharacterFunctions::isLetterOrDigit (c))
                out += juce::String::charToString (juce::CharacterFunctions::toLowerCase (c));
        return out;
    }

    struct IconMapEntry { const char* soundName; const char* iconFileName; };

    // Hand-curated sound -> icon assignments (icons8 line icons + a few
    // custom SVGs, see assets/sound-icons/). Checked before the fuzzy
    // matcher below, which only ever catches a handful of the ~113 sounds
    // by coincidence of naming.
    const IconMapEntry kIconMap[] = {
    { "Aaaaah", "scary-tree.png" },
    { "Aaaaahhh", "love.png" },
    { "Air Horn", "air-horn.png" },
    { "Amen1", "pray-1.png" },
    { "Amen2", "pray.png" },
    { "Amen3", "prayer.png" },
    { "Amen4", "pray.png" },
    { "Applause_Kids", "hand.png" },
    { "Applause", "applause.png" },
    { "Applause2", "hand.png" },
    { "Applause3", "thumbs-up.png" },
    { "Are You Ready", "ARE YOU READY!.svg" },
    { "Are You Ready2", "ARE YOU READY!.svg" },
    { "Baby Cry", "crying-baby-1.png" },
    { "Boing", "coil.png" },
    { "Boing2", "coil.png" },
    { "Boing3", "coil.png" },
    { "Boing4", "coil.png" },
    { "Bomb", "bomb.png" },
    { "Bong_birds", "Bong-Birds.svg" },
    { "Boo", "boo.png" },
    { "Bruh", "BRUH.svg" },
    { "Burp", "Burp.svg" },
    { "Buzzer", "buzzer.png" },
    { "Cat", "cat.png" },
    { "Cha Ching", "coins.png" },
    { "Cha Ching2", "dollar-bag.png" },
    { "Chicken", "Chicken.svg" },
    { "Crickets", "Cricket.svg" },
    { "Denied", "denied.png" },
    { "DJ Horn", "air-horn.png" },
    { "Drum Fill", "Drum-Fill.svg" },
    { "Drum Roll", "drum-roll.svg" },
    { "Dun Dun Dunnn", "who.png" },
    { "Easy", "easy.png" },
    { "Explosion", "explosion.png" },
    { "Fail", "fail-1.png" },
    { "Fail2", "fail-2.png" },
    { "Fail3", "fail-3.png" },
    { "Fail4", "fail.png" },
    { "Fart", "bending.png" },
    { "Fart2", "fart.png" },
    { "Fart3", "bending.png" },
    { "Fart4", "fart.png" },
    { "Fatality", "victim.png" },
    { "Goat", "year-of-goat.png" },
    { "Gong", "gong.png" },
    { "Gun Load", "Gun-Load.svg" },
    { "Gun Shoot", "firing-gun.png" },
    { "Hawk", "falcon.png" },
    { "Heart Beat", "heart-beat-1.png" },
    { "Heart Stop", "skull-heart.png" },
    { "Hey Thats Pretty Good", "cool-emoji.png" },
    { "Horn Big", "siren.png" },
    { "Horn Old Car", "horn.png" },
    { "Horn Raid", "french-horn.png" },
    { "Just Do It", "Just Do It.svg" },
    { "KungFu", "Kung Fu.svg" },
    { "Laser", "laser.png" },
    { "Laugh", "hah.png" },
    { "Laugh2", "laugh-1.png" },
    { "Laugh3", "laugh-2.png" },
    { "Laugh4", "laugh-3.png" },
    { "Laugh5", "laugh.png" },
    { "Laugh6 SpongeBob", "sponge.png" },
    { "Laugh7", "rolling-on-the-floor-laughing.png" },
    { "Laugh8 JJ", "laugh-2.png" },
    { "Laugh9 Crowd", "crowd.png" },
    { "LOL", "laugh-1.png" },
    { "Love", "love.png" },
    { "MicDrop", "mic-drop.png" },
    { "Missile Whistle", "atomic-bomb.png" },
    { "Mission Failed", "fail.png" },
    { "Nice", "Nice.svg" },
    { "NO (The Office)", "NO.svg" },
    { "OMG", "omg-icon.svg" },
    { "Party Horn", "horn.png" },
    { "Piano Riff", "piano-riff.svg" },
    { "Police Siren", "siren.png" },
    { "Quack", "quack.svg" },
    { "Record Scratch", "music-record-1.png" },
    { "Record Scratch2", "music-record.png" },
    { "Record Stop", "music-record-1.png" },
    { "Record Stop2", "music-record.png" },
    { "Riser", "Rising.svg" },
    { "Rock and Roll", "rock-and-roll.png" },
    { "Samauri", "Samauri.svg" },
    { "Say What", "say-whhat.svg" },
    { "Scary Music", "Scary.svg" },
    { "Scary Music2", "Scream.svg" },
    { "Scary Music3", "bat.png" },
    { "Scary Music4", "scream.png" },
    { "Shots Fired", "Shots Fired.svg" },
    { "Star", "star.png" },
    { "That was legit", "we-heart-it.png" },
    { "This is Awesome", "shocked.png" },
    { "Tick Tock", "clock.png" },
    { "Weeeee", "Weee.svg" },
    { "What!", "what.png" },
    { "Whistle Down", "Whistle.svg" },
    { "Whistle2", "whistle.png" },
    { "Whistle3", "whistle.png" },
    { "Who Are You", "man-shrugging.png" },
    { "Wilhelm Scream", "shocked-1.png" },
    { "Wolf Whistle", "wolf-1.png" },
    { "WooHoo", "WooHoo.svg" },
    { "XFiles", "grey.png" },
    { "Yeeaahhh", "Yeeaahh!.svg" },
    { "YeeHaw", "YeeHaw.svg" },
    { "Yikes", "thinking-face.png" },
    };

    juce::File findMappedIcon (const juce::String& soundName, const juce::File& iconsDir)
    {
        const auto key = normalizeKeepDigits (soundName);
        for (auto& entry : kIconMap)
            if (normalizeKeepDigits (entry.soundName) == key)
                return iconsDir.getChildFile (juce::CharPointer_UTF8 (entry.iconFileName));
        return {};
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
        iconFiles = iconsDir.findChildFiles (juce::File::findFiles, false, "*.svg;*.png");

    const auto soundFiles = soundsDir.findChildFiles (juce::File::findFiles, false, "*.wav");

    sounds_.reserve ((size_t) soundFiles.size());
    for (auto& soundFile : soundFiles)
    {
        Entry e;
        e.name = soundFile.getFileNameWithoutExtension();
        e.soundFile = soundFile;

        // Hand-curated assignment first -- covers ~110 of the ~113 sounds.
        if (iconsDir.isDirectory())
        {
            auto mapped = findMappedIcon (e.name, iconsDir);
            if (mapped.existsAsFile())
                e.iconFile = mapped;
        }

        // Fuzzy name-match fallback for anything not in the table above.
        if (! e.iconFile.existsAsFile())
        {
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
