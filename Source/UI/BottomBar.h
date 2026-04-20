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
#include "../Localization/LocalizationManager.h"

class BottomBar : public juce::Component,
                  private juce::Timer
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
    ~BottomBar() override = default;

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

    std::function<void()> onReturnToZero;
    std::function<void()> onStopAndReturnToZero;
    std::function<void(bool isNowPlaying)> onPlayPause;
    std::function<void()> onJumpToEnd;
    std::function<void(float progress01)> onSeek;
    std::function<void(int semitones)> onPitchChanged;
    std::function<void(int volumeStep)> onVolumeChanged;

    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

private:
    void timerCallback() override;
    void setupUI();
  void loadTransportIcons();
  void updateTransportButtonIcons();
    void updateTimeLabels();
    void configureTransportButton(juce::ShapeButton& button, bool isPrimary);
    void generateWaveform();
    juce::String formatTime(double seconds) const;
  std::unique_ptr<juce::Drawable> createSpriteIcon(const juce::String& symbolId,
                           const juce::Colour& colour) const;

    juce::Rectangle<int> getTransportArea() const;
    juce::Rectangle<int> getWaveformArea() const;
    juce::Rectangle<int> getWaveformDrawArea() const;
    juce::Rectangle<int> getSlidersArea() const;

  juce::ShapeButton returnToZeroButton { "toStart", juce::Colours::white, juce::Colours::white, juce::Colours::white };
  juce::ShapeButton stopButton { "stop", juce::Colours::red, juce::Colours::red, juce::Colours::red };
  juce::ShapeButton playPauseButton { "playPause", juce::Colours::white, juce::Colours::white, juce::Colours::white };
  juce::ShapeButton jumpToEndButton { "toEnd", juce::Colours::white, juce::Colours::white, juce::Colours::white };

    juce::Slider pitchSlider;
    juce::Slider volumeSlider;
    juce::Label currentTimeLabel;
    juce::Label durationLabel;
    juce::Label pitchLabel;
    juce::Label volumeLabel;

    bool isPlaying = false;
    bool hasPlaybackStarted = false;
    float progress = 0.0f;
    double durationSeconds = 0.0;
    std::vector<float> waveform;

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
