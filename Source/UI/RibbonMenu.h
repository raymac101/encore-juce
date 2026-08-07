/*
  ==============================================================================

    RibbonMenu.h

    Quick-access ribbon with four expandable boxes:
    1) Background Music
    2) Lyric Display
    3) Next Singer
    4) Sound Effects

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "LyricDisplayComponent.h"
#include "SfxLibraryListPanel.h"
#include "BackgroundMusicLibraryPanel.h"
#include "StartTheNightConfigPanel.h"

class AudioEngine;

class RibbonMenu : public juce::Component,
                   public juce::DragAndDropTarget
{
public:
    enum class PanelId
    {
        none = -1,
        backgroundMusic = 0,
        lyricDisplay = 1,
        nextSinger = 2,
        soundEffects = 3
    };

    RibbonMenu();
    ~RibbonMenu() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseDoubleClick (const juce::MouseEvent& event) override;

    // juce::DragAndDropTarget -- accepts a sound tile dragged from
    // sfxLibraryPanel_ and dropped onto one of the 8 slot buttons below.
    bool isInterestedInDragSource (const SourceDetails& details) override;
    void itemDropped (const SourceDetails& details) override;

    bool isHidden() const noexcept { return hidden_; }
    bool isPanelExpanded() const noexcept { return expandedPanel_ != PanelId::none; }
    PanelId getExpandedPanel() const noexcept { return expandedPanel_; }

    int getCollapsedHeight() const noexcept { return collapsedHeight_; }
    int getHiddenHeight() const noexcept { return hiddenHeight_; }

    void updateAllText();
    void setAudioEngine (AudioEngine* engine);
    void setLyricPreviewFile (const juce::File& cdgFile);
    void setBackgroundState (bool playing, float volume01);
    void setBackgroundTrackInfo (const juce::String& songName,
                   double positionSeconds,
                   double totalSeconds);

    /** Pushes the current folder path (empty == using the bundled default)
        into the full-screen library panel's path label. */
    void setBackgroundFolderPath (const juce::String& path);

    /** Rebuilds the full-screen library panel's checklist. Call after any
        change to the folder or the persisted selection. */
    void setBackgroundAvailableTracks (const std::vector<juce::File>& tracks,
                                       const juce::StringArray& selectedFilenames);
    void setSfxVolume (float volume01);
    void setLyricWindowVisible (bool visible);
    void setLyricWindowFullScreen (bool fullScreen);
    void setNextSinger (const juce::String& singerName, const juce::String& songName);

    /** Whether "Start the Night" has already been used this session --
        true flips the Next Singer button/action back to its normal "Play
        Next Singer" behaviour. MainComponent resets this to false on every
        fresh venue load. */
    void setNightStarted (bool started);

    /** Seeds the intro config panel from persisted values + whatever's in
        assets/music/ (the same folder background music draws from). */
    void setIntroConfigInitialState (const juce::String& apiKey,
                                     const juce::String& script,
                                     const juce::String& voiceId,
                                     const juce::String& selectedMusicFilename,
                                     const std::vector<juce::File>& availableMusicFiles);

    /** Forwarded from MainComponent once IntroVoiceService::generateAndCache
        finishes. */
    void reportIntroGenerationResult (bool ok, const juce::String& error);

    std::function<void()> onLayoutChanged;

    std::function<void(bool shouldPlay)> onBackgroundPlayPause;
    std::function<void(float volume01)> onBackgroundVolumeChanged;
    std::function<void(double positionSeconds)> onBackgroundSeekRequested;
    std::function<void()> onBackgroundNextTrack;
    std::function<void()> onBackgroundPrevTrack;

    std::function<void (juce::File folder)> onBackgroundFolderChanged;
    std::function<void (juce::StringArray selectedFilenames)> onBackgroundSelectionChanged;
    std::function<void (juce::File file)> onBackgroundPreviewRequested;
    std::function<void()> onBackgroundUseDefaultRequested;

    std::function<void()> onLyricToggleWindow;
    std::function<void()> onLyricToggleFullscreen;

    std::function<void()> onPlayNextSinger;

    /** Fired instead of onPlayNextSinger when the Next Singer button is
        clicked while ! nightStarted_ -- MainComponent plays the cached
        intro then itself calls the same code path onPlayNextSinger would
        have. */
    std::function<void()> onStartTheNightRequested;

    std::function<void (juce::String apiKey, juce::String script, juce::String voiceId, juce::File musicFile)> onIntroGenerateRequested;

    /** Fired as soon as the ElevenLabs API key is worth persisting -- see
        StartTheNightConfigPanel::onApiKeyChanged. */
    std::function<void (juce::String apiKey)> onIntroApiKeyChanged;

    std::function<void(float volume01)> onSfxVolumeChanged;
    std::function<void(const juce::String& effectName)> onTriggerSfx;

private:
    void updateCardTexts();
    void updateControlState();
  bool isInDragHandle (juce::Point<int> p) const noexcept;
  int getMaxCollapsedHeight() const noexcept;

    void toggleHidden();
    void togglePanel (PanelId id);

    // Applies UserPreferences::getSfxSlotAssignments() to the 8 buttons
    // below (icon + onClick effectName) -- called once at construction and
    // again after any slot is changed via drag-drop or the clear button.
    void refreshSfxSlots();

    // The 8 slot buttons in fixed order, matching the persisted assignment
    // array's index order. Used by refreshSfxSlots(), both grid layouts,
    // and itemDropped()'s hit-testing, so the list only ever lives in one
    // place.
    std::array<juce::DrawableButton*, 8> sfxSlotButtons();
    std::array<juce::TextButton*, 8> sfxSlotClearButtons();

    // Cached by refreshSfxSlots() so resized() can decide per-slot clear-
    // button visibility without re-reading UserPreferences every layout
    // pass.
    juce::StringArray sfxSlotAssignmentsCache_;

    PanelId expandedPanel_ = PanelId::none;
    bool hidden_ = false;
    int collapsedHeight_ = 176;
    int lastOpenHeight_ = 176;
    bool draggingHandle_ = false;
    int dragStartHeight_ = 176;
    int dragStartScreenY_ = 0;

    static constexpr int dragHandleHeight_ = 20;
    static constexpr int hiddenHeight_ = 20;
    static constexpr int minCollapsedHeight_ = 96;
    static constexpr int maxCollapsedHeightCap_ = 420;

    bool backgroundPlaying_ = false;
    float backgroundVolume01_ = 0.5f;
    juce::String backgroundSongName_;
    double backgroundPositionSeconds_ = 0.0;
    double backgroundTotalSeconds_ = 0.0;
    float sfxVolume01_ = 0.85f;
    bool lyricWindowVisible_ = true;
    bool lyricWindowFullScreen_ = false;
    juce::String nextSingerName_;
    juce::String nextSingerSong_;
    bool nightStarted_ = false;
    juce::File lyricPreviewFile_;
    AudioEngine* audioEngine_ = nullptr;

    juce::TextButton backgroundCard_ { "backgroundCard" };
    juce::TextButton lyricCard_ { "lyricCard" };
    juce::TextButton nextSingerCard_ { "nextSingerCard" };
    juce::TextButton sfxCard_ { "sfxCard" };

    // Gear icon in each compact card's top-right corner -- an explicit
    // affordance for the same "expand to full screen" action the card
    // itself already performs on click (see togglePanel()).
    juce::DrawableButton backgroundCardGear_ { "backgroundCardGear", juce::DrawableButton::ImageFitted };
    juce::DrawableButton lyricCardGear_      { "lyricCardGear",      juce::DrawableButton::ImageFitted };
    juce::DrawableButton nextSingerCardGear_ { "nextSingerCardGear", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxCardGear_        { "sfxCardGear",        juce::DrawableButton::ImageFitted };

    juce::TextButton collapsePanelButton_ { "collapsePanel" };

    juce::Label panelTitleLabel_;

    juce::DrawableButton bgPrevButton_       { "bgPrev", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton bgPlayPauseButton_  { "bgPlayPause", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton bgNextButton_       { "bgNext", juce::DrawableButton::ImageOnButtonBackground };
    juce::Slider bgVolumeSlider_;
    juce::Label bgVolumeLabel_;
    juce::Slider bgProgressSlider_;
    juce::Label bgTrackLabel_;
    juce::Label bgTimeLabel_;

    // Folder picker + track checklist, shown below the transport controls
    // only when expandedPanel_ == PanelId::backgroundMusic.
    std::unique_ptr<BackgroundMusicLibraryPanel> bgLibraryPanel_;

    juce::TextButton lyricToggleWindowButton_ { "lyricToggleWindow" };
    juce::TextButton lyricFullscreenButton_ { "lyricFullscreen" };
    juce::Label lyricStateLabel_;
    std::unique_ptr<LyricDisplayComponent> lyricPreview_;

    juce::Label nextSingerNameLabel_;
    juce::Label nextSingerSongLabel_;
    juce::TextButton nextSingerPlayButton_ { "nextSingerPlay" };

    // "Start the Night" intro configuration, shown below the Next Singer
    // card's existing controls only when expandedPanel_ == PanelId::nextSinger.
    std::unique_ptr<StartTheNightConfigPanel> introConfigPanel_;

    juce::DrawableButton sfxAreYouReadyButton_ { "sfxAreYouReady", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton sfxChickenButton_ { "sfxChicken", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton sfxBurpButton_ { "sfxBurp", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton sfxBruhButton_ { "sfxBruh", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton sfxBuzzerButton_ { "sfxBuzzer", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton sfxDrumFillButton_ { "sfxDrumFill", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton sfxDrumRollButton_ { "sfxDrumRoll", juce::DrawableButton::ImageOnButtonBackground };
    juce::DrawableButton sfxWooHooButton_ { "sfxWooHoo", juce::DrawableButton::ImageOnButtonBackground };
    juce::Slider sfxVolumeSlider_;
    juce::Label sfxVolumeLabel_;

    // Small "x" overlay per slot (expanded view only, visible only when
    // that slot is non-empty) to clear its assignment. Same toFront()-per-
    // layout-pass fix as the card gear icons -- see positionGear() in
    // resized().
    std::array<juce::TextButton, 8> sfxSlotClearButtons_ {
        juce::TextButton ("sfxClear0"), juce::TextButton ("sfxClear1"),
        juce::TextButton ("sfxClear2"), juce::TextButton ("sfxClear3"),
        juce::TextButton ("sfxClear4"), juce::TextButton ("sfxClear5"),
        juce::TextButton ("sfxClear6"), juce::TextButton ("sfxClear7")
    };

    // Scrollable sound library + filter box, shown below the 8-slot grid
    // only when expandedPanel_ == PanelId::soundEffects.
    std::unique_ptr<SfxLibraryListPanel> sfxLibraryPanel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RibbonMenu)
};
