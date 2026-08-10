/*
  ==============================================================================

    StartTheNightConfigPanel.h

    Configuration for the Ribbon's "Start the Night" AI-voice intro, shown
    below the Next Singer card's existing controls in
    Source/UI/RibbonMenu.cpp when that panel is expanded to full screen.
    Lets a host enter their ElevenLabs API key, pick a voice, write the
    announcer script, choose a track from assets/music/, and generate a
    new saved intro (Source/Services/IntroVoiceService.h). Every successful
    generation is kept -- see UserPreferences::SavedIntro -- so a host can
    build up a small library of intros and pick which one "Start the
    Night" actually plays, rather than each generation overwriting the
    last.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../Services/UserPreferences.h"

class StartTheNightConfigPanel : public juce::Component
{
public:
    StartTheNightConfigPanel();
    ~StartTheNightConfigPanel() override;

    void resized() override;

    /** Seeds every field from persisted values, the list of tracks found
        in assets/music/ (may be empty if that folder's somehow been
        cleared out, in which case the music combo shows a clear "no intro
        tracks available yet" placeholder rather than looking broken), and
        the saved-intros list + which one is currently selected. */
    void setInitialState (const juce::String& apiKey,
                          const juce::String& script,
                          const juce::String& voiceId,
                          const juce::String& selectedMusicFilename,
                          const std::vector<juce::File>& availableMusicFiles,
                          const std::vector<UserPreferences::SavedIntro>& savedIntros,
                          const juce::String& selectedIntroId);

    /** Fired when "Generate" is clicked. MainComponent does the
        {venue}/{host} substitution, persists the draft fields, calls
        IntroVoiceService::generateAndCache(), saves the result as a new
        UserPreferences::SavedIntro (using `introName`, or a timestamp-based
        default if left blank), and reports back via
        reportGenerationResult() below. */
    std::function<void (juce::String apiKey, juce::String script, juce::String voiceId,
                        juce::File musicFile, juce::String introName)> onGenerateRequested;

    /** Fired as soon as the API key is worth persisting -- on focus lost
        and right after a successful "Fetch Voices" -- so it survives an
        app restart without the host having to click all the way through
        to "Generate" first. */
    std::function<void (juce::String apiKey)> onApiKeyChanged;

    /** Fired when the host picks a different entry in the Saved Intros
        list -- that becomes the one "Start the Night" plays. */
    std::function<void (juce::String id)> onSavedIntroSelected;

    /** Fired when the host clicks the saved-intros "Preview" button. */
    std::function<void (juce::String id)> onSavedIntroPreviewRequested;

    /** Fired when the host clicks the saved-intros "Delete" button. */
    std::function<void (juce::String id)> onSavedIntroDeleteRequested;

    /** MainComponent calls this once generation finishes (success or
        failure) so the panel can show a status message. */
    void reportGenerationResult (bool ok, const juce::String& error);

private:
    void fetchVoices();
    void updateGenerateButtonEnabled();

    juce::Label apiKeyLabel_;
    juce::TextEditor apiKeyEditor_;
    juce::TextButton fetchVoicesButton_ { "fetchVoices" };

    juce::Label voiceLabel_;
    juce::ComboBox voiceCombo_;

    juce::Label scriptLabel_;
    juce::TextEditor scriptEditor_;
    juce::Label scriptHintLabel_;

    juce::Label musicLabel_;
    juce::ComboBox musicCombo_;

    juce::Label introNameLabel_;
    juce::TextEditor introNameEditor_;

    juce::TextButton generateButton_ { "generate" };
    juce::Label statusLabel_;

    juce::Label savedIntrosLabel_;
    juce::ComboBox savedIntrosCombo_;
    juce::TextButton previewSavedButton_ { "previewSaved" };
    juce::TextButton deleteSavedButton_ { "deleteSaved" };

    std::vector<juce::File> availableMusicFiles_;
    std::vector<juce::String> voiceIds_;        // parallel to voiceCombo_'s items, indexed by (comboId - 1)
    std::vector<juce::String> savedIntroIds_;   // parallel to savedIntrosCombo_'s items, indexed by (comboId - 1)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StartTheNightConfigPanel)
};
