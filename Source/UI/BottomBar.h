/*
  ==============================================================================

    BottomBar.h
    Created: 16 Apr 2026
    Author:  GitHub Copilot

    Bottom music control bar component.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "DesignTokens.h"
#include "../Localization/LocalizationManager.h"
#include "../Services/GlobalProgressService.h"

class BottomBar : public juce::Component,
                  private juce::Timer,
                  private GlobalProgressService::Listener
{
public:
    //==============================================================================
    // Public constants
    static constexpr int MIN_BAR_HEIGHT = 80;
    static constexpr int MAX_BAR_HEIGHT = 300;
    static constexpr int DEFAULT_BAR_HEIGHT = 120;

    // Current height (adjustable)
    int currentBarHeight = DEFAULT_BAR_HEIGHT;

    // Callback for height changes
    std::function<void(int newHeight)> onHeightChanged;

    //==============================================================================
    BottomBar();
    ~BottomBar() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Mouse handling for resizing and waveform seeking
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

    // Height management
    void setBarHeight(int newHeight);
    int getBarHeight() const { return currentBarHeight; }

    void setPlaying(bool playing);
    void setProgress(float progress01);
    void setDurationSeconds(double seconds);
    void setPitch(int semitones);
    void setVolume(int volumeStep);
    void setPlayEnabled(bool enabled);

    /** Replace the procedural waveform with real peaks. Values should be in
      [0.0, 1.0]. Pass an empty vector to clear waveform rendering and show
      the status message instead. */
    void setWaveformSamples(const std::vector<float>& samples);

    /** Set the centered message shown in the waveform area while no real
      waveform peaks are available. */
    void setWaveformStatusMessage(const juce::String& message);

    /** When true, the internal 30Hz timer stops auto-advancing progress so that
        an external source (e.g. the audio engine) drives it via setProgress(). */
    void setExternalProgressControl(bool externalControl) { externalProgressControl_ = externalControl; }

    /** Mirrors the nav bar / queue bar's current widths so the transport
        controls sit under the nav rail, the sliders sit under the queue
        rail, and the waveform in between lines up with the main content
        area above -- call whenever either bar's width changes (see
        MainComponent::resized()). 0 falls back to the old fixed-percentage
        sizing (used before the first real widths are known). */
    void setSideRailWidths(int leftWidth, int rightWidth);

    std::function<void()> onReturnToZero;
    std::function<void()> onStopAndReturnToZero;
    std::function<void(bool isNowPlaying)> onPlayPause;
    std::function<void()> onJumpToEnd;
    std::function<void(float progress01)> onSeek;
    std::function<void(int semitones)> onPitchChanged;
    std::function<void(int volumeStep)> onVolumeChanged;

    /** Fired when the "expand main screen" button is clicked. MainComponent
        has no direct handle on the app's top-level window, so this is a
        request -- the actual toggle happens further up the call chain. */
    std::function<void()> onExpandMainScreenClicked;

    /** Fired when the "expand lyric screen" button is clicked. */
    std::function<void()> onExpandLyricScreenClicked;

    /** Fired when the "Phone Lyrics" button is clicked -- start/reopen the
        local web stream of the lyric screen for phones/tablets on the venue
        WiFi (see LyricStreamServer). */
    std::function<void()> onPhoneLyricsClicked;

    /** Update the main-screen button's icon to reflect the window's actual
        fullscreen state. Called after any change, whether triggered by this
        button or elsewhere (e.g. the Window menu). Also called on every
        ~50ms UI-refresh tick so the icon self-heals regardless of trigger
        source -- cheap ONLY because the implementation no-ops when
        `expanded` matches the current state; do not remove that guard, an
        earlier unconditional version was measurably responsible for
        whole-app sluggishness (repeated juce::ImageButton::setImages() at
        20Hz). */
    void setMainScreenExpanded (bool expanded);

    /** Same, for the lyric (singer-facing) display window. */
    void setLyricScreenExpanded (bool expanded);

    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

private:
    // Forwards hover state and "clicked without dragging" from volumeSlider.
    // A plain juce::MouseListener (not a Component override) so it can't
    // collide with BottomBar's own mouseEnter/mouseExit, which handle the
    // resize handle for the bar itself.
    class VolumeHoverListener : public juce::MouseListener
    {
    public:
        std::function<void(bool)> onHoverChanged;
        std::function<void()> onPlainClick;

        void mouseEnter(const juce::MouseEvent&) override { if (onHoverChanged) onHoverChanged(true); }
        void mouseExit(const juce::MouseEvent&) override  { if (onHoverChanged) onHoverChanged(false); }
        void mouseDown(const juce::MouseEvent& e) override { downPos = e.getPosition(); moved = false; }
        void mouseDrag(const juce::MouseEvent& e) override { if (e.getPosition().getDistanceFrom(downPos) > 3) moved = true; }
        void mouseUp(const juce::MouseEvent&) override    { if (! moved && onPlainClick) onPlainClick(); }

    private:
        juce::Point<int> downPos;
        bool moved = false;
    };
    VolumeHoverListener volumeHoverListener_;
    void toggleVolumeMute();
    bool volumeMuted_ = false;
    int  volumeBeforeMute_ = 5;

    void timerCallback() override;
  void globalProgressChanged(bool isBusy, const juce::String& message) override;
    void setupUI();
  void loadTransportIcons();
  void updateTransportButtonIcons();
    void updateTimeLabels();
    void configureTransportButton(juce::ShapeButton& button, bool isPrimary);
    juce::String formatTime(double seconds) const;
  std::unique_ptr<juce::Drawable> createSpriteIcon(const juce::String& symbolId,
                           const juce::Colour& colour) const;

    juce::Rectangle<int> getTransportArea() const;
    juce::Rectangle<int> getWaveformArea() const;
    juce::Rectangle<int> getWaveformDrawArea() const;
    juce::Rectangle<int> getSlidersArea() const;

  juce::ShapeButton returnToZeroButton { "toStart", juce::Colours::white, juce::Colours::white, juce::Colours::white };
  juce::ShapeButton stopButton { "stop", DesignTokens::colour(DesignTokens::kDanger), DesignTokens::colour(DesignTokens::kDanger), DesignTokens::colour(DesignTokens::kDanger) };
  juce::ShapeButton playPauseButton { "playPause", juce::Colours::white, juce::Colours::white, juce::Colours::white };
  juce::ShapeButton jumpToEndButton { "toEnd", juce::Colours::white, juce::Colours::white, juce::Colours::white };

    juce::Slider pitchSlider;
    juce::Slider volumeSlider;

    // Stacked "expand to fullscreen" buttons, to the right of volumeSlider.
    juce::ImageButton expandMainScreenButton  { "expandMainScreenButton" };
    juce::ImageButton expandLyricScreenButton { "expandLyricScreenButton" };
    juce::TextButton  phoneLyricsButton_;
    juce::Image screen1ExpandImage, screen1CollapseImage;
    juce::Image screen2ExpandImage, screen2CollapseImage;
    bool mainScreenExpanded_ = false;
    bool lyricScreenExpanded_ = false;
    static void setButtonImage (juce::ImageButton& button, const juce::Image& image);
    juce::Label currentTimeLabel;
    juce::Label durationLabel;
    juce::Label pitchLabel;
    juce::Label volumeLabel;

    bool isPlaying = false;
    bool hasPlaybackStarted = false;
    float progress = 0.0f;
    double durationSeconds = 0.0;
    std::vector<float> waveform;
    juce::String waveformStatusMessage_;
    bool externalProgressControl_ = false;
    int  leftRailWidth_ = 0;
    int  rightRailWidth_ = 0;
    bool globalBusy_ = false;
    juce::String globalBusyMessage_;
    float spinnerPhase_ = 0.0f;

    //==============================================================================
    // Resize handle - Visual Studio style (at the TOP edge for bottom bar)
    static constexpr int RESIZE_HANDLE_INACTIVE_HEIGHT = 1;
    static constexpr int RESIZE_HANDLE_ACTIVE_HEIGHT = 6;
    bool isResizing = false;
    bool isOverResizeHandle = false;
    int resizeStartHeight = 0;
    juce::Point<int> resizeStartPosition;
    int getCurrentResizeHandleHeight() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomBar)
};
