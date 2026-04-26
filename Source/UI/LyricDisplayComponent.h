/*
  ==============================================================================

    LyricDisplayComponent.h

    Singer-facing lyric display. This is the JUCE equivalent of the Angular
    lyric-display.component — it renders the currently playing CDG graphics
    in time with the audio, and shows an idle screen when nothing is loaded.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../CDG/CDGDecoder.h"

class AudioEngine;

class LyricDisplayComponent  : public juce::Component,
                               private juce::Timer
{
public:
    LyricDisplayComponent();
    ~LyricDisplayComponent() override;

    /** The display polls this engine for the current playback position. */
    void setAudioEngine (AudioEngine* engine);

    /** Load a .cdg file for synchronised rendering. Pass an invalid juce::File
        to clear back to the idle screen. */
    void loadCDG (const juce::File& cdgFile);

    /** Load and start playing an MP4/M4V/MOV video. The video is rendered
        full-screen on this component and provides its own audio. Pass an
        invalid juce::File (or call stopVideo()) to return to the CDG/idle
        view. */
    bool loadVideo (const juce::File& videoFile);

    /** Stops video playback (if any) and releases the underlying decoder. */
    void stopVideo();

    /** True while a video is loaded and playing. */
    bool isVideoActive() const;

    /** Current play position of the loaded video in seconds, or 0.0. */
    double getVideoPosition() const;

    /** Total duration of the loaded video in seconds, or 0.0. */
    double getVideoDuration() const;

    /** Update the "next singer / next song" overlay. Pass empty strings to
        hide it. */
    void setNextSinger (const juce::String& singerName,
                        const juce::String& songName,
                        const juce::String& artist);

    /** Optional venue code to display bottom-third, matching the Angular UI. */
    void setVenueCode (const juce::String& code) { venueCode_ = code; repaint(); }

    /** Override the idle-screen logo with a venue-specific image. Pass an
        invalid image to revert to the bundled Encore logo. */
    void setVenueLogo (const juce::Image& logo);

    //==============================================================================
    void paint  (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void paintIdle    (juce::Graphics& g, juce::Rectangle<int> area);
    void paintCdg     (juce::Graphics& g, juce::Rectangle<int> area);
    void paintOverlay (juce::Graphics& g, juce::Rectangle<int> area);
    void layoutVideoBounds();

    AudioEngine* audioEngine_ = nullptr;

    CDGDecoder   decoder_;
    juce::File   loadedFile_;

    // MP4 video playback (used when an MP4 song is selected). Lazily created
    // the first time loadVideo() is called.
    std::unique_ptr<juce::VideoComponent> videoComponent_;

    juce::String nextSinger_;
    juce::String nextSong_;
    juce::String nextArtist_;
    juce::String venueCode_;

    juce::Image  logoImage_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyricDisplayComponent)
};
