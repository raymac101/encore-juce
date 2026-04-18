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

class BottomBar : public juce::Component,
                  private juce::Timer
{
public:
    BottomBar();
    ~BottomBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;

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

    static constexpr int kBarHeight = 120;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomBar)
};
