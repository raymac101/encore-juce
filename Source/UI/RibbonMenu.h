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

class AudioEngine;

class RibbonMenu : public juce::Component
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
    void setSfxVolume (float volume01);
    void setLyricWindowVisible (bool visible);
    void setLyricWindowFullScreen (bool fullScreen);
    void setNextSinger (const juce::String& singerName, const juce::String& songName);

    std::function<void()> onLayoutChanged;

    std::function<void(bool shouldPlay)> onBackgroundPlayPause;
    std::function<void(float volume01)> onBackgroundVolumeChanged;
    std::function<void(double positionSeconds)> onBackgroundSeekRequested;
    std::function<void()> onBackgroundNextTrack;
    std::function<void()> onBackgroundPrevTrack;

    std::function<void()> onLyricToggleWindow;
    std::function<void()> onLyricToggleFullscreen;

    std::function<void()> onPlayNextSinger;

    std::function<void(float volume01)> onSfxVolumeChanged;
    std::function<void(const juce::String& effectName)> onTriggerSfx;

private:
    void updateCardTexts();
    void updateControlState();
  bool isInDragHandle (juce::Point<int> p) const noexcept;
  int getMaxCollapsedHeight() const noexcept;

    void toggleHidden();
    void togglePanel (PanelId id);

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
    juce::File lyricPreviewFile_;
    AudioEngine* audioEngine_ = nullptr;

    juce::TextButton backgroundCard_ { "backgroundCard" };
    juce::TextButton lyricCard_ { "lyricCard" };
    juce::TextButton nextSingerCard_ { "nextSingerCard" };
    juce::TextButton sfxCard_ { "sfxCard" };

    juce::TextButton collapsePanelButton_ { "collapsePanel" };

    juce::Label panelTitleLabel_;

    juce::TextButton bgPrevButton_    { "bgPrev" };
    juce::TextButton bgPlayPauseButton_ { "bgPlayPause" };
    juce::TextButton bgNextButton_    { "bgNext" };
    juce::Slider bgVolumeSlider_;
    juce::Label bgVolumeLabel_;
    juce::Slider bgProgressSlider_;
    juce::Label bgTrackLabel_;
    juce::Label bgTimeLabel_;

    juce::TextButton lyricToggleWindowButton_ { "lyricToggleWindow" };
    juce::TextButton lyricFullscreenButton_ { "lyricFullscreen" };
    juce::Label lyricStateLabel_;
    std::unique_ptr<LyricDisplayComponent> lyricPreview_;

    juce::Label nextSingerNameLabel_;
    juce::Label nextSingerSongLabel_;
    juce::TextButton nextSingerPlayButton_ { "nextSingerPlay" };

    juce::DrawableButton sfxAreYouReadyButton_ { "sfxAreYouReady", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxChickenButton_ { "sfxChicken", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxBurpButton_ { "sfxBurp", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxBruhButton_ { "sfxBruh", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxBuzzerButton_ { "sfxBuzzer", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxDrumFillButton_ { "sfxDrumFill", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxDrumRollButton_ { "sfxDrumRoll", juce::DrawableButton::ImageFitted };
    juce::DrawableButton sfxWooHooButton_ { "sfxWooHoo", juce::DrawableButton::ImageFitted };
    juce::Slider sfxVolumeSlider_;
    juce::Label sfxVolumeLabel_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RibbonMenu)
};
