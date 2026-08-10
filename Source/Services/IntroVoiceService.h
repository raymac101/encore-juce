/*
  ==============================================================================

    IntroVoiceService.h

    Generates a "Start the Night" AI-voice intro: synthesizes a host's
    script via ElevenLabs' text-to-speech API, mixes the result with a
    chosen track from assets/music/ (the same folder background music
    draws from) into a single audio file, and writes it to disk under a
    unique filename (never overwriting a previous generation). The music
    is trimmed to end a few seconds after the voice-over finishes rather
    than playing the full song.

    Each successful generation is saved as its own entry in
    UserPreferences::getSavedIntros(), so a host can build up a small
    library of intros (different scripts/voices/music) and pick which one
    "Start the Night" plays via UserPreferences::getSelectedIntroId() --
    see Source/UI/StartTheNightConfigPanel.h for the picker UI. This
    service only ever synthesizes + writes files; it doesn't know about
    the saved list itself, that's MainComponent's job (mirrors how every
    other *Service in this app stays UI/preferences-agnostic).

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
        voice is speaking), and writes the combined file under a unique
        name in getGeneratedDirectory() -- never overwrites a previous
        generation. On success, `onDone` receives the resulting File so
        the caller can save it as a new UserPreferences::SavedIntro
        entry; on failure it receives an invalid File. Always called on
        the message thread. */
    void generateAndCache (const juce::String& apiKey,
                          const juce::String& scriptText,
                          const juce::String& voiceId,
                          const juce::File& introMusicFile,
                          std::function<void (bool ok, juce::File generatedFile, juce::String error)> onDone);

    /** Where generated intro files live -- public so callers can resolve a
        UserPreferences::SavedIntro::fileName back into a real File. */
    static juce::File getGeneratedDirectory();

    /** The single fixed-filename intro this service used to write before
        it supported multiple saved intros, if one is still sitting on
        disk from an older build. MainComponent uses this once, at
        startup, to fold a pre-existing intro into the new saved-intros
        list rather than silently losing it. Returns an invalid File once
        nothing's there (including after that one-time migration deletes
        it). */
    static juce::File getLegacyCachedIntroFile();

private:
    IntroVoiceService() = default;
    ~IntroVoiceService() = default;

    JUCE_DECLARE_NON_COPYABLE (IntroVoiceService)
};
