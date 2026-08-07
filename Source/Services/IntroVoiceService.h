/*
  ==============================================================================

    IntroVoiceService.h

    Generates the "Start the Night" AI-voice intro: synthesizes a host's
    script via ElevenLabs' text-to-speech API, mixes the result with a
    chosen track from assets/music/ (the same folder background music
    draws from) into a single audio file, and caches it on disk. The music
    is trimmed to end a few seconds after the voice-over finishes rather
    than playing the full song. Generation happens once, when the host
    saves their configuration in the Ribbon's Next Singer full-screen view
    (Source/UI/StartTheNightConfigPanel.h) -- "Start the Night" itself just
    plays the cached result, no network call at showtime.

    All network/file work runs on a background juce::Thread; callbacks are
    always marshalled back to the message thread, matching every other
    *Service in this codebase.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

class IntroVoiceService
{
public:
    static IntroVoiceService& getInstance();

    struct VoiceInfo
    {
        juce::String id;
        juce::String name;
    };

    /** Calls ElevenLabs' voice-list endpoint with `apiKey` so a config UI
        can show the host's own actually-available voices rather than
        hardcoded IDs that could be renamed/removed. onDone is always
        called, on the message thread. */
    void fetchAvailableVoices (const juce::String& apiKey,
                              std::function<void (bool ok, std::vector<VoiceInfo> voices, juce::String error)> onDone);

    /** Synthesizes `scriptText` via ElevenLabs (voice `voiceId`, using
        `apiKey`), mixes the result with `introMusicFile` (music at full
        level before the voice starts and after it ends, ducked while the
        voice is speaking), and writes the combined file to
        getCachedIntroFile(), overwriting any previous one. onDone is
        always called, on the message thread. */
    void generateAndCache (const juce::String& apiKey,
                          const juce::String& scriptText,
                          const juce::String& voiceId,
                          const juce::File& introMusicFile,
                          std::function<void (bool ok, juce::String error)> onDone);

    /** The cached mixed intro from the last successful generateAndCache()
        call, or an invalid (non-existent) juce::File if none has ever
        succeeded. */
    static juce::File getCachedIntroFile();

private:
    IntroVoiceService() = default;
    ~IntroVoiceService() = default;

    static juce::File getGeneratedDirectory();

    JUCE_DECLARE_NON_COPYABLE (IntroVoiceService)
};
