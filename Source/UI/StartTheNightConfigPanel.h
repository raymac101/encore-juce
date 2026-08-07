/*
  ==============================================================================

    StartTheNightConfigPanel.h

    Configuration for the Ribbon's "Start the Night" AI-voice intro, shown
    below the Next Singer card's existing controls in
    Source/UI/RibbonMenu.cpp when that panel is expanded to full screen.
    Lets a host enter their ElevenLabs API key, pick a voice, write the
    announcer script, choose a bundled intro music sting, and generate +
    preview the mixed result (Source/Services/IntroVoiceService.h).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>

class StartTheNightConfigPanel : public juce::Component
{
public:
    StartTheNightConfigPanel();
    ~StartTheNightConfigPanel() override;

    void resized() override;

    /** Seeds every field from persisted values + the list of tracks found
        in assets/music/ (the same folder background music draws from --
        may be empty if that folder's somehow been cleared out, in which
        case the music combo shows a clear "no intro tracks available yet"
        placeholder rather than looking broken). */
    void setInitialState (const juce::String& apiKey,
                          const juce::String& script,
                          const juce::String& voiceId,
                          const juce::String& selectedMusicFilename,
                          const std::vector<juce::File>& availableMusicFiles);

    /** Fired when "Generate & Preview" is clicked. MainComponent does the
        {venue}/{host} substitution, persists all four values, calls
        IntroVoiceService::generateAndCache(), and reports back via
        reportGenerationResult() below. */
    std::function<void (juce::String apiKey, juce::String script, juce::String voiceId, juce::File musicFile)> onGenerateRequested;

    /** Fired as soon as the API key is worth persisting -- on focus lost
        and right after a successful "Fetch Voices" -- so it survives an
        app restart without the host having to click all the way through
        to "Generate & Preview" first. */
    std::function<void (juce::String apiKey)> onApiKeyChanged;

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

    juce::TextButton generateButton_ { "generate" };
    juce::Label statusLabel_;

    std::vector<juce::File> availableMusicFiles_;
    std::vector<juce::String> voiceIds_; // parallel to voiceCombo_'s items, indexed by (comboId - 1)

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StartTheNightConfigPanel)
};
