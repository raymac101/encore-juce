/*
  ==============================================================================

    StartTheNightConfigPanel.cpp

  ==============================================================================
*/

#include "StartTheNightConfigPanel.h"
#include "../Services/IntroVoiceService.h"
#include "../Localization/LocalizationManager.h"

namespace
{
    const auto kText = juce::Colours::white;
}

//==============================================================================
StartTheNightConfigPanel::StartTheNightConfigPanel()
{
    auto& lm = LocalizationManager::getInstance();

    auto initLabel = [this] (juce::Label& l, const juce::String& text)
    {
        addAndMakeVisible (l);
        l.setText (text, juce::dontSendNotification);
        l.setColour (juce::Label::textColourId, kText);
        l.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    };

    initLabel (apiKeyLabel_, lm.getText ("ribbon.intro.api_key_label"));
    addAndMakeVisible (apiKeyEditor_);
    apiKeyEditor_.setPasswordCharacter ((juce::juce_wchar) 0x2022);
    apiKeyEditor_.setTextToShowWhenEmpty (lm.getText ("ribbon.intro.api_key_placeholder"), kText.withAlpha (0.5f));
    apiKeyEditor_.onFocusLost = [this]
    {
        if (onApiKeyChanged)
            onApiKeyChanged (apiKeyEditor_.getText().trim());
    };

    addAndMakeVisible (fetchVoicesButton_);
    fetchVoicesButton_.setButtonText (lm.getText ("ribbon.intro.fetch_voices"));
    fetchVoicesButton_.onClick = [this] { fetchVoices(); };

    initLabel (voiceLabel_, lm.getText ("ribbon.intro.voice_label"));
    addAndMakeVisible (voiceCombo_);
    voiceCombo_.onChange = [this] { updateGenerateButtonEnabled(); };

    initLabel (scriptLabel_, lm.getText ("ribbon.intro.script_label"));
    addAndMakeVisible (scriptEditor_);
    scriptEditor_.setMultiLine (true, true);
    scriptEditor_.setReturnKeyStartsNewLine (true);
    scriptEditor_.onTextChange = [this] { updateGenerateButtonEnabled(); };

    initLabel (scriptHintLabel_, lm.getText ("ribbon.intro.script_hint"));
    scriptHintLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
    scriptHintLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.6f));

    initLabel (musicLabel_, lm.getText ("ribbon.intro.music_label"));
    addAndMakeVisible (musicCombo_);
    musicCombo_.onChange = [this] { updateGenerateButtonEnabled(); };

    addAndMakeVisible (generateButton_);
    generateButton_.setButtonText (lm.getText ("ribbon.intro.generate"));
    generateButton_.onClick = [this]
    {
        if (! onGenerateRequested)
            return;

        const int musicId = musicCombo_.getSelectedId();
        if (musicId <= 0 || musicId > (int) availableMusicFiles_.size())
            return;

        statusLabel_.setText (LocalizationManager::getInstance().getText ("ribbon.intro.generating"),
                              juce::dontSendNotification);

        onGenerateRequested (apiKeyEditor_.getText(),
                            scriptEditor_.getText(),
                            voiceCombo_.getSelectedId() > 0 ? voiceIds_[(size_t) (voiceCombo_.getSelectedId() - 1)] : juce::String(),
                            availableMusicFiles_[(size_t) (musicId - 1)]);
    };

    addAndMakeVisible (statusLabel_);
    statusLabel_.setColour (juce::Label::textColourId, kText.withAlpha (0.85f));
    statusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));

    updateGenerateButtonEnabled();
}

StartTheNightConfigPanel::~StartTheNightConfigPanel() = default;

void StartTheNightConfigPanel::setInitialState (const juce::String& apiKey,
                                                const juce::String& script,
                                                const juce::String& voiceId,
                                                const juce::String& selectedMusicFilename,
                                                const std::vector<juce::File>& availableMusicFiles)
{
    apiKeyEditor_.setText (apiKey, juce::dontSendNotification);
    scriptEditor_.setText (script, juce::dontSendNotification);

    availableMusicFiles_ = availableMusicFiles;
    musicCombo_.clear (juce::dontSendNotification);

    auto& lm = LocalizationManager::getInstance();
    if (availableMusicFiles_.empty())
    {
        musicCombo_.addItem (lm.getText ("ribbon.intro.no_music_available"), 1);
        musicCombo_.setItemEnabled (1, false);
        musicCombo_.setSelectedId (1, juce::dontSendNotification);
    }
    else
    {
        int selectedId = 1;
        for (int i = 0; i < (int) availableMusicFiles_.size(); ++i)
        {
            const auto name = availableMusicFiles_[(size_t) i].getFileNameWithoutExtension();
            musicCombo_.addItem (name, i + 1);
            if (availableMusicFiles_[(size_t) i].getFileName() == selectedMusicFilename)
                selectedId = i + 1;
        }
        musicCombo_.setSelectedId (selectedId, juce::dontSendNotification);
    }

    // If a voice id was already saved, seed a single-entry combo with it
    // until "Fetch Voices" is pressed (needs the API key/network to get
    // the full list with display names).
    voiceIds_.clear();
    voiceCombo_.clear (juce::dontSendNotification);
    if (voiceId.isNotEmpty())
    {
        voiceIds_.push_back (voiceId);
        voiceCombo_.addItem (voiceId, 1);
        voiceCombo_.setSelectedId (1, juce::dontSendNotification);
    }

    statusLabel_.setText ({}, juce::dontSendNotification);
    updateGenerateButtonEnabled();
}

void StartTheNightConfigPanel::fetchVoices()
{
    const auto apiKey = apiKeyEditor_.getText().trim();
    if (apiKey.isEmpty())
    {
        statusLabel_.setText (LocalizationManager::getInstance().getText ("ribbon.intro.enter_api_key_first"),
                              juce::dontSendNotification);
        return;
    }

    statusLabel_.setText (LocalizationManager::getInstance().getText ("ribbon.intro.fetching_voices"),
                          juce::dontSendNotification);

    juce::Component::SafePointer<StartTheNightConfigPanel> safe (this);
    IntroVoiceService::getInstance().fetchAvailableVoices (apiKey,
        [safe] (bool ok, std::vector<IntroVoiceService::VoiceInfo> voices, juce::String error)
        {
            if (safe == nullptr)
                return;

            auto& lm = LocalizationManager::getInstance();

            if (! ok)
            {
                safe->statusLabel_.setText (lm.getText ("ribbon.intro.fetch_voices_failed") + " " + error,
                                            juce::dontSendNotification);
                return;
            }

            // A key that just successfully fetched voices is worth saving
            // immediately, without waiting for focus-lost or Generate.
            if (safe->onApiKeyChanged)
                safe->onApiKeyChanged (safe->apiKeyEditor_.getText().trim());

            const auto previouslySelected = safe->voiceIds_.empty() ? juce::String()
                : (safe->voiceCombo_.getSelectedId() > 0
                   ? safe->voiceIds_[(size_t) (safe->voiceCombo_.getSelectedId() - 1)]
                   : juce::String());

            safe->voiceIds_.clear();
            safe->voiceCombo_.clear (juce::dontSendNotification);

            int selectedId = 0;
            for (int i = 0; i < (int) voices.size(); ++i)
            {
                safe->voiceIds_.push_back (voices[(size_t) i].id);
                safe->voiceCombo_.addItem (voices[(size_t) i].name, i + 1);
                if (voices[(size_t) i].id == previouslySelected)
                    selectedId = i + 1;
            }

            if (selectedId == 0 && ! voices.empty())
                selectedId = 1;
            safe->voiceCombo_.setSelectedId (selectedId, juce::dontSendNotification);

            safe->statusLabel_.setText ({}, juce::dontSendNotification);
            safe->updateGenerateButtonEnabled();
        });
}

void StartTheNightConfigPanel::updateGenerateButtonEnabled()
{
    const bool ready = apiKeyEditor_.getText().trim().isNotEmpty()
                     && scriptEditor_.getText().trim().isNotEmpty()
                     && voiceCombo_.getSelectedId() > 0
                     && musicCombo_.getSelectedId() > 0
                     && ! availableMusicFiles_.empty();
    generateButton_.setEnabled (ready);
}

void StartTheNightConfigPanel::reportGenerationResult (bool ok, const juce::String& error)
{
    auto& lm = LocalizationManager::getInstance();
    statusLabel_.setText (ok ? lm.getText ("ribbon.intro.generate_success")
                             : (lm.getText ("ribbon.intro.generate_failed") + " " + error),
                          juce::dontSendNotification);
}

void StartTheNightConfigPanel::resized()
{
    auto area = getLocalBounds().reduced (4);
    constexpr int rowH = 26;
    constexpr int gap = 6;

    auto apiRow = area.removeFromTop (rowH);
    apiKeyLabel_.setBounds (apiRow.removeFromLeft (90));
    fetchVoicesButton_.setBounds (apiRow.removeFromRight (110));
    apiRow.removeFromRight (6);
    apiKeyEditor_.setBounds (apiRow);
    area.removeFromTop (gap);

    auto voiceRow = area.removeFromTop (rowH);
    voiceLabel_.setBounds (voiceRow.removeFromLeft (90));
    voiceCombo_.setBounds (voiceRow);
    area.removeFromTop (gap);

    scriptLabel_.setBounds (area.removeFromTop (20));
    scriptEditor_.setBounds (area.removeFromTop (70));
    scriptHintLabel_.setBounds (area.removeFromTop (16));
    area.removeFromTop (gap);

    auto musicRow = area.removeFromTop (rowH);
    musicLabel_.setBounds (musicRow.removeFromLeft (90));
    musicCombo_.setBounds (musicRow);
    area.removeFromTop (gap);

    auto genRow = area.removeFromTop (rowH);
    generateButton_.setBounds (genRow.removeFromLeft (160));
    genRow.removeFromLeft (10);
    statusLabel_.setBounds (genRow);
}
