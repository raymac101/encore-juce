/*
  ==============================================================================

    SongSelectionDialog.h

    Modal "Song Selection" dialog shown when a user clicks a song on the
    Home page or in the Search results. Mirrors the features of the Angular
    search-modal-component:
      - Album artwork
      - Song name + artist
      - Singer name field
      - Song-version dropdown (one entry per CdgSong version/file)
      - Pitch (+/-) in semitones
      - 4 action buttons: Cancel, Play Next, Play Now, Add to Queue

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/CdgSong.h"

//==============================================================================
/** Result passed back to the caller when an action button is pressed. */
struct SongSelectionResult
{
    enum class Action { AddToQueue, PlayNext, PlayNow, Cancelled };

    Action      action     = Action::Cancelled;
    CdgSong     song;
    juce::String singerName;
    int         versionIndex = 0;          // index into song.version/fullPath/etc.
    int         pitchSemitones = 0;        // -12..+12
};

//==============================================================================
class SongSelectionDialog : public juce::Component
{
public:
    explicit SongSelectionDialog(const CdgSong& song);
    ~SongSelectionDialog() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Called when the user clicks one of the action buttons (or Cancel / X).
        After this fires the dialog closes itself. */
    std::function<void(const SongSelectionResult&)> onResult;

    /** Launch the dialog as a modal window centred on the parent. Ownership
        is managed internally; the callback is invoked with the chosen action. */
    static void launch(juce::Component* parent,
                       const CdgSong& song,
                       std::function<void(const SongSelectionResult&)> onResult);

private:
    void closeWithResult(SongSelectionResult::Action action);
    void updatePitchLabel();

    CdgSong song_;

    juce::Image artwork_;

    juce::Label         titleLabel_;
    juce::TextButton    closeButton_   { "X" };

    juce::Label         songFieldLabel_;
    juce::Label         songValueLabel_;
    juce::Label         artistFieldLabel_;
    juce::Label         artistValueLabel_;

    juce::Label         singerFieldLabel_;
    juce::TextEditor    singerEditor_;

    juce::Label         versionFieldLabel_;
    juce::ComboBox      versionBox_;

    juce::Label         pitchFieldLabel_;
    juce::TextButton    pitchMinusButton_ { "-" };
    juce::Label         pitchValueLabel_;
    juce::TextButton    pitchPlusButton_  { "+" };
    juce::Label         pitchUnitLabel_;

    juce::TextButton    cancelButton_;
    juce::TextButton    playNextButton_;
    juce::TextButton    playNowButton_;
    juce::TextButton    addToQueueButton_;

    int pitchSemitones_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongSelectionDialog)
};
