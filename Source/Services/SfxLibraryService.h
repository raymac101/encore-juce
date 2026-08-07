/*
  ==============================================================================

    SfxLibraryService.h

    Enumerates every sound effect available to assign into one of the
    Ribbon's 8 configurable slots (Source/UI/RibbonMenu.h). Scans
    assets/sounds/*.wav once and caches the result; each sound is matched
    to an icon in assets/sound-icons/ (*.svg or *.png) via a hand-curated
    table in the .cpp, falling back to best-effort name matching for
    anything not in that table -- callers fall back to a generic sprite
    icon, e.g. "icon-volume-high", when Entry::iconFile is invalid.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class SfxLibraryService
{
public:
    static SfxLibraryService& getInstance();

    struct Entry
    {
        juce::String name;      // display name == the .wav's basename, no extension
        juce::File soundFile;
        juce::File iconFile;    // may be invalid -- no matching icon was found
    };

    /** Every sound in assets/sounds/, sorted alphabetically by name.
        Scanned once on first call and cached. */
    const std::vector<Entry>& getAllSounds();

    /** Case-insensitive lookup by name, e.g. to resolve a slot's persisted
        assignment back to its sound/icon files. Returns nullptr if no such
        sound exists (the underlying file may have been removed since the
        assignment was saved). */
    const Entry* findByName (const juce::String& name);

private:
    SfxLibraryService() = default;
    ~SfxLibraryService() = default;

    void ensureScanned();

    std::vector<Entry> sounds_;
    bool scanned_ = false;

    JUCE_DECLARE_NON_COPYABLE (SfxLibraryService)
};
