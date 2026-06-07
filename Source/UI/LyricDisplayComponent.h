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
#include <vector>
#include "../CDG/CDGDecoder.h"

class AudioEngine;

class LyricDisplayComponent  : public juce::Component,
                               private juce::Timer
{
public:
    struct QueuePreviewEntry
    {
        juce::String singerName;
        juce::String songName;
        juce::String artistName;
    };

    struct AdEntry
    {
        juce::String name;
        juce::String url;
        juce::String mimeType;
        int durationSec = 10;

        bool isVideo() const noexcept
        {
            const auto type = mimeType.trim().toLowerCase();
            if (type.startsWith ("video/"))
                return true;

            // Some metadata sources store loose hints like "mp4" or
            // "application/octet-stream" for video ads.
            if (type.contains ("mp4") || type.contains ("m4v") || type.contains ("mov")
                || type.contains ("webm") || type.contains ("quicktime"))
                return true;

            const auto lowerName = name.toLowerCase();
            const auto lowerUrl  = url.toLowerCase();
            return lowerName.endsWith (".mp4") || lowerName.endsWith (".m4v")
                || lowerName.endsWith (".mov") || lowerName.endsWith (".webm")
                || lowerUrl.contains (".mp4") || lowerUrl.contains (".m4v")
                || lowerUrl.contains (".mov") || lowerUrl.contains (".webm");
        }
    };

    LyricDisplayComponent();
    ~LyricDisplayComponent() override;

    /** The display polls this engine for the current playback position. */
    void setAudioEngine (AudioEngine* engine);

    /** Load a .cdg file for synchronised rendering. Pass an invalid juce::File
        to clear back to the idle screen. */
    void loadCDG (const juce::File& cdgFile);

    /** Load an MP4/M4V/MOV video for rendering on this component. When
        autoPlay is false, the media is preloaded and remains paused until
        playVideo() is called. Pass an invalid juce::File (or call
        stopVideo()) to return to the CDG/idle view. */
    bool loadVideo (const juce::File& videoFile, bool autoPlay = true);

    /** Stops video playback (if any) and releases the underlying decoder. */
    void stopVideo();

    /** Start/resume loaded video playback. */
    void playVideo();

    /** Pause loaded video playback, preserving current position. */
    void pauseVideo();

    /** Seek loaded video to an absolute position in seconds. */
    void seekVideo (double positionSeconds);

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

    /** Venue context used by the idle split-screen panel and ad source path. */
    void setVenueContext (const juce::String& venueId, const juce::String& venueName);

    /** Push the next singers shown in the idle split-screen panel. */
    void setQueuePreview (const std::vector<QueuePreviewEntry>& entries);

    /** Forces idle rendering even when a song is preloaded. The flag clears
        automatically when playback resumes. */
    void setForceIdleScreen (bool shouldForce);

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
    void paintAdPanel (juce::Graphics& g, juce::Rectangle<int> area, bool addFrame);
    void paintOverlay (juce::Graphics& g, juce::Rectangle<int> area);
    void layoutVideoBounds();
    void layoutIdleAdVideoBounds (juce::Rectangle<int> area);
    void updateAdPanelAnimation (bool idleMode);

    double getPlaybackPositionSeconds() const;
    double getPlaybackDurationSeconds() const;
    int getAdPanelWidth (juce::Rectangle<int> area, bool idleMode) const;
    int getVenueCodeBarHeight (juce::Rectangle<int> area) const;
    juce::Rectangle<int> getContentRenderArea (juce::Rectangle<int> area) const;
    juce::Rectangle<int> getPrimaryRenderArea (juce::Rectangle<int> area, bool idleMode) const;
    juce::Rectangle<int> getAdRenderArea (juce::Rectangle<int> area, bool idleMode) const;

    void refreshAdsAsync (bool force = false);
    void applyAds (std::vector<AdEntry> ads);
    void advanceIdleAd (bool force = false);
    void stopIdleAdVideo();
    void showIdleAdVideo (const AdEntry& ad);

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
    juce::String venueId_;
    juce::String venueName_;

    juce::Image  logoImage_;

    std::vector<QueuePreviewEntry> queuePreview_;

    std::vector<AdEntry> ads_;
    int currentAdIndex_ = -1;
    int adRemainingMs_ = 0;
    int adRefreshCountdownFrames_ = 0;
    float adPanelVisibility_ = 1.0f;
    float adPanelTarget_ = 1.0f;
    juce::Image currentAdImage_;

    std::unique_ptr<juce::VideoComponent> idleAdVideoComponent_;
    bool forceIdleScreen_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyricDisplayComponent)
};
