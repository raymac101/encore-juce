/*
  ==============================================================================

    AiSongNameCleanupService.h

    Pre-cleans an artist/song pair with Claude before it's used as a Spotify
    search query -- run by the Viracicom Admin bulk metadata tool right
    before each ApiService::searchArtistAndSong() call, to raise the
    first-try success rate on songs whose local (filename-derived) text is
    slightly off: misspellings, swapped artist/song fields, stray symbols
    from filename parsing, or informal contractions that don't match how
    Spotify actually lists the title.

    Requires UserPreferences::getAnthropicApiKey() to be set; calls back
    with ok=false when it's empty so callers can just skip the pre-clean
    step (still attempting the Spotify lookup with the original text).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class AiSongNameCleanupService
{
public:
    static AiSongNameCleanupService& getInstance();

    struct Result
    {
        bool         ok = false;
        bool         changed = false;      // true if artistName/songName differ from the input
        juce::String artistName;
        juce::String songName;
        juce::String errorMessage;         // set when ok == false
    };

    using Callback = std::function<void(Result)>;

    /** Runs on a background thread; callback fires on the message thread.
        Never invents a different song -- only cleans up spelling/formatting
        of the one given, and may swap the two fields if they look
        backwards. */
    void cleanup (const juce::String& artistName, const juce::String& songName, Callback onDone);

private:
    AiSongNameCleanupService() = default;
    ~AiSongNameCleanupService() = default;

    JUCE_DECLARE_NON_COPYABLE (AiSongNameCleanupService)
};
