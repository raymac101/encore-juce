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
#include <array>
#include <unordered_map>
#include <vector>
#include "../CDG/CDGDecoder.h"
#include "../Models/Emoji.h"

class AudioEngine;
class WebVideoView;

// Lyric screen visual theme, configurable in Settings alongside Color/Motion
// Intensity. Stored in UserPreferences as a plain clamped int (0-7) so the
// Services layer doesn't need to include this UI header.
enum class LyricTheme
{
    Classic = 0,
    NeonPulse,
    ConfettiPop,
    DiscoSweep,
    RetroMarquee,
    LaserGrid,
    BubblegumPop,
    VuMeterPulse
};

// Resolved, intensity-scaled colour set for one theme. accent/ink/etc. cover
// every element that was previously a single hardcoded cyan/white/black
// literal in paintIdle()/paintOverlay() -- see resolveLyricThemePalette().
struct LyricThemePalette
{
    juce::Colour accent;
    juce::Colour ink;
    juce::Colour inkDim;
    juce::Colour cardFill;
    juce::Colour cardBorder;
    juce::Colour avatarRing;
    juce::Colour avatarFallback;
};

class LyricDisplayComponent  : public juce::Component,
                               private juce::Timer
{
public:
    struct QueuePreviewEntry
    {
        juce::String singerName;
        juce::String songName;
        juce::String artistName;
        juce::String avatarPath;
    };

    struct AdEntry
    {
        juce::String name;
        juce::String url;
        juce::String mimeType;
        int durationSec = 10;
        int frequency = 1;

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

    /** Lower-third next-up singer shown on the left side of the black bar.
        Pass empty to hide. */
    void setLowerThirdNextUpSinger (const juce::String& singerName);

    /** Idle-screen now-singing summary shown above the Next Up list.
        Pass empty values to hide the section. */
    void setNowSingingInfo (const juce::String& singerName,
                            const juce::String& songName,
                            const juce::String& artistName,
                            const juce::String& avatarPath);

    /** Forces idle rendering even when a song is preloaded. The flag clears
        automatically when playback resumes. */
    void setForceIdleScreen (bool shouldForce);

    /** Override the idle-screen logo with a venue-specific image. Pass an
        invalid image to revert to the bundled Encore logo. */
    void setVenueLogo (const juce::Image& logo);

    /** Spawn a cheer emoji reaction (from the TAGG mobile app) to animate
        rising up the left side of the screen. Silently ignored while the
        idle/ad screen is showing (no active song) or once the concurrent
        on-screen cap is reached. */
    void addEmoji (const Emoji& request);

    //==============================================================================
    void paint  (juce::Graphics&) override;
    void resized() override;

private:

    void timerCallback() override;
    void paintIdle    (juce::Graphics& g, juce::Rectangle<int> area);
    void paintCdg     (juce::Graphics& g, juce::Rectangle<int> area);
    void paintAdPanel (juce::Graphics& g, juce::Rectangle<int> area, bool addFrame);
    void paintOverlay (juce::Graphics& g, juce::Rectangle<int> area);
    void paintThemeBackdrop (juce::Graphics& g, juce::Rectangle<int> area, LyricTheme theme,
                             const LyricThemePalette& palette, double animPhase, float motion01);
    void paintNowCardDecoration (juce::Graphics& g, juce::Rectangle<int> nowCard, LyricTheme theme,
                                 const LyricThemePalette& palette, double animPhase, float motion01);
    void seedThemeAnimations();
    void updateEmojis (double dtSeconds);
    void paintEmojis  (juce::Graphics& g, juce::Rectangle<int> area);
    juce::Image getOrLoadEmojiSheet (const juce::String& emojiName, int& outTotalFrames);
    void layoutVideoBounds();
    void layoutIdleAdVideoBounds (juce::Rectangle<int> area);
    void updateAdPanelAnimation (bool idleMode);
    juce::Image getQueuePreviewAvatar (const juce::String& avatarPath);

    // Resolves avatarPath via ArtworkCache::resolveAvatar(), caching the
    // result in queueAvatarCache_ by the raw path/preset/URL string. If the
    // avatar is a URL still downloading, updates the cache entry and repaints
    // once it arrives.
    juce::Image resolveAndCacheAvatar (const juce::String& avatarPath);

    double getPlaybackPositionSeconds() const;
    double getPlaybackDurationSeconds() const;
    int getAdPanelWidth (juce::Rectangle<int> area, bool idleMode) const;
    int getVenueCodeBarHeight (juce::Rectangle<int> area) const;
    juce::Rectangle<int> getContentRenderArea (juce::Rectangle<int> area) const;
    juce::Rectangle<int> getPrimaryRenderArea (juce::Rectangle<int> area, bool idleMode) const;
    juce::Rectangle<int> getAdRenderArea (juce::Rectangle<int> area, bool idleMode) const;

    void refreshAdsAsync (bool force = false);
    void applyAds (std::vector<AdEntry> ads);
    void buildAdPlaybackOrder();
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
    juce::String nowSingingName_;
    juce::String nowSingingSong_;
    juce::String nowSingingArtist_;
    juce::String nowSingingAvatarPath_;
    juce::String lowerThirdNextUpSinger_;
    juce::String venueCode_;
    juce::String venueId_;
    juce::String venueName_;

    juce::Image  logoImage_;

    std::vector<QueuePreviewEntry> queuePreview_;
    std::unordered_map<std::string, juce::Image> queueAvatarCache_;

    std::vector<AdEntry> ads_;
    int currentAdIndex_ = -1;
    std::vector<int> adPlaybackOrder_;
    size_t adPlaybackPos_ = 0;
    int adRemainingMs_ = 0;
    int adRefreshCountdownFrames_ = 0;
    float adPanelVisibility_ = 1.0f;
    float adPanelTarget_ = 1.0f;
    juce::Image currentAdImage_;

    // macOS renders idle-screen video ads through idleAdVideoComponent_
    // (AVFoundation). On Windows juce::VideoComponent (DirectShow) can't decode
    // MP4/H.264, so idleAdWebVideo_ (WebView2) is used instead and the clip is
    // played from the local AdMediaCache copy. Exactly one is non-null.
    std::unique_ptr<juce::VideoComponent> idleAdVideoComponent_;
    std::unique_ptr<WebVideoView> idleAdWebVideo_;
    bool forceIdleScreen_ = false;

    // Emoji cheer reactions (venues/<id>/emojis, via EmojiService).
    std::vector<Emoji> activeEmojis_;
    std::unordered_map<std::string, juce::Image> emojiSheetCache_;
    std::unordered_map<std::string, int> emojiFrameCountCache_;
    double lastEmojiTickMs_ = 0.0;
    static constexpr int kMaxConcurrentEmojis = 40;
    static constexpr int kEmojiFrameSize = 480;

    // Lyric screen theme animation (Settings: Theme / Color Intensity /
    // Motion Intensity). animPhase_ is the single shared, motion-scaled,
    // monotonically-advancing clock every theme's decoration reads from --
    // see timerCallback() and paintThemeBackdrop(). Seed arrays are
    // constructor-initialised once and never mutated, so switching themes
    // needs no reset: every value is a pure function of animPhase_ plus a
    // fixed per-element seed.
    double animPhase_ = 0.0;
    double lastThemeTickMs_ = 0.0;

    struct ConfettiSeed    { float x0, fallSpeed, size, rotSpeed, rotOffset; int colourIdx; };
    struct BubbleSeed      { float x0, y0, radius, bobSpeed, bobPhase, driftSpeed, driftPhase; };
    struct MarqueeBulbSeed { float x; bool topEdge; float phaseOffset; };
    struct VuBarSeed       { float x; float speedMul; float phaseOffset; };

    std::array<ConfettiSeed, 24>    confettiSeeds_;
    std::array<BubbleSeed, 6>       bubbleSeeds_;
    std::array<MarqueeBulbSeed, 24> marqueeBulbSeeds_;
    std::array<VuBarSeed, 20>       vuBarSeeds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LyricDisplayComponent)
};
