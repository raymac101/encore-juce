/*
  ==============================================================================

    BackgroundMusicPlayer.h

    Standalone background-music playback service.

    Plays a looping playlist of audio files independently of the main karaoke
    AudioEngine.  Used to fill silence between karaoke singers.

    Features:
    - Manages its own AudioDeviceManager / AudioSourcePlayer so it never
      conflicts with karaoke playback.
    - Builds its playlist automatically from assets/music/*.
    - Supports play / pause / stop / next / prev.
    - Smooth fade-in and fade-out via an atomic gain ramp applied each audio
      callback.  Call fadeOut() when a karaoke song starts and fadeIn() when
      it ends.
    - Thread-safe state queries.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Audio/ChannelPluginChain.h"
#include <atomic>
#include <vector>
#include <functional>
#include <mutex>

class BackgroundMusicPlayer : public juce::AudioSource,
                              private juce::Timer
{
public:
    BackgroundMusicPlayer();
    ~BackgroundMusicPlayer() override;

    //==========================================================================
    // Lifecycle
    void initialize();
    void shutdown();
    bool isInitialized() const noexcept { return initialized_.load(); }

    //==========================================================================
    // Playlist
    /** Scans a directory for supported audio files, sorted naturally by
        name. Rebuilds BOTH getAvailableTracks() (everything found) and the
        actually-played list to match (i.e. "play everything" is the
        default until setTrackSelection() narrows it down). Must be called
        from the message thread. */
    void setPlaylistDirectory (const juce::File& directory);

    /** Re-points the playlist at the bundled assets/music directory (the
        same default initialize() starts with) and clears the track
        selection back to "everything." For a "Use Default" button in a
        folder-picker UI. */
    void resetToDefaultFolder();

    /** Narrows the actually-played list down to the given filenames (name +
        extension, no path) out of whatever setPlaylistDirectory() last
        found. An empty array means "play everything" -- the same default
        setPlaylistDirectory() already starts with. Must be called from the
        message thread. */
    void setTrackSelection (const juce::StringArray& selectedFilenames);

    /** Every track found by the last setPlaylistDirectory() call, regardless
        of the current selection -- for populating a track-picker UI. */
    std::vector<juce::File> getAvailableTracks() const;

    /** Loads and plays `file` immediately (must be one of
        getAvailableTracks()), independently of the current selection/
        rotation -- for auditioning a track from a picker UI. Does not
        change getCurrentTrackIndex() or disturb the regular playlist
        rotation for subsequent skipToNext()/skipToPrev() calls. */
    void playSpecificTrack (const juce::File& file);

    /** Plays `file` (any file, not just one of getAvailableTracks() --
        typically a generated/cached intro) once, then automatically
        resumes the regular playlist rotation from wherever it was and
        calls onFinished on the message thread. Forces full volume
        immediately (no fade-in ramp) so the intro is audible even if
        background music was previously faded to 0. For the Ribbon's
        "Start the Night" action. */
    void playOneShotIntro (const juce::File& file, std::function<void (bool completedNaturally)> onFinished);

    int getTrackCount() const noexcept { return (int) playlist_.size(); }
    int getCurrentTrackIndex() const noexcept { return currentIndex_.load(); }

    /** Returns the display name of the currently loaded track. */
    juce::String getCurrentTrackName() const;

    //==========================================================================
    // Transport
    void play();
    void pause();
    void stop();
    void skipToNext();
    void skipToPrev();
    void seekToPosition (double seconds);

    bool isPlaying() const noexcept { return playing_.load(); }
    double getCurrentPosition() const noexcept { return currentPosition_.load(); }
    double getTotalLength() const noexcept { return totalLength_.load(); }

    //==========================================================================
    // Volume  (0.0 – 1.0, independent of karaoke main volume)
    void setVolume (float volume01);
    float getVolume() const noexcept { return targetVolume_.load(); }

    //==========================================================================
    // Fade control (called by MainComponent when karaoke starts/ends)
    void fadeOut (float durationSeconds = 1.5f);
    void fadeIn  (float durationSeconds = 1.5f);

    //==========================================================================
    // Master on/off (Ribbon's enable/disable button). When disabled, play()
    // and fadeIn() both no-op -- nothing can start this player back up,
    // regardless of which of the many call sites tries (song pause/stop,
    // song finishing naturally, or the user's own Play button). Disabling
    // while already audible fades it out immediately rather than cutting
    // it off. Does not affect playSpecificTrack()/playOneShotIntro(),
    // which are deliberate one-shot previews/announcements, not the
    // "fill silence between singers" behaviour this gate is for.
    void setEnabled (bool enabled);
    bool isEnabled() const noexcept { return enabled_.load(); }

    //==========================================================================
    // Phase B: this player's own VST3 plugin chain (the Mixer page's
    // "Background Music" strip). Message thread only for load/unload/editor
    // access; this class calls process() on it internally, from its own
    // audio thread.
    ChannelPluginChain& getPluginChain() noexcept { return pluginChain_; }

    //==========================================================================
    // Callbacks (called on the message thread)
    std::function<void()> onTrackChanged;
    std::function<void()> onPlayStateChanged;

    //==========================================================================
    // juce::AudioSource
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    void timerCallback() override;   // used to fire callbacks on the message thread
    void loadTrack (int index);

    // Shared by loadTrack(int) (looks up playlist_[index] then delegates
    // here) and playSpecificTrack() (calls this directly with indexToSet=-1
    // so previewing a track never disturbs the real rotation's
    // currentIndex_).
    void loadTrackFile (const juce::File& trackFile, int indexToSet);
    void advanceToNext();

    std::atomic<bool> initialized_ { false };
    std::atomic<bool> playing_ { false };
    std::atomic<bool> enabled_ { true };
    std::atomic<double> currentPosition_ { 0.0 };
    std::atomic<double> totalLength_ { 0.0 };

    // Volume & fade state (manipulated from both threads; atomic)
    std::atomic<float> targetVolume_ { 0.5f };
    std::atomic<float> currentGain_ { 0.5f };
    std::atomic<float> fadeRatePerSample_ { 0.0f };
    std::atomic<bool> fadingOut_ { false };
    std::atomic<bool> fadingIn_  { false };

    std::atomic<int> currentIndex_ { 0 };
    std::atomic<bool> trackChangedFlag_ { false };
    std::atomic<bool> playStateChangedFlag_ { false };

    // playOneShotIntro() state -- set on the message thread when starting,
    // checked/cleared on the audio thread (getNextAudioBlock) when the
    // one-shot file finishes, consumed on the message thread (timerCallback)
    // to resume the regular rotation and fire the completion callback.
    // Mirrors the existing trackChangedFlag_ pattern.
    std::atomic<bool> oneShotIntroActive_ { false };
    std::atomic<bool> oneShotIntroFinishedFlag_ { false };
    std::function<void (bool)> oneShotIntroFinishedCallback_;  // message-thread only

    // Devices — BackgroundMusicPlayer uses a secondary AudioDeviceManager so
    // it runs concurrently with the main karaoke engine.
    juce::AudioDeviceManager deviceManager_;
    std::unique_ptr<juce::AudioSourcePlayer> sourcePlayer_;
    juce::AudioFormatManager formatManager_;

    // Source chain
    mutable std::mutex chainMutex_;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource_;
    std::unique_ptr<juce::AudioTransportSource>    transportSource_;
    std::unique_ptr<juce::ResamplingAudioSource>   resamplingSource_;

    // Playlist — rescanned on the message thread (setPlaylistDirectory) but
    // its size/contents are also read from the audio thread (advanceToNext),
    // so all access goes through playlistMutex_. Kept separate from
    // chainMutex_ so the two never nest.
    //
    // availableTracks_ is everything setPlaylistDirectory() found;
    // playlist_ is the subset actually played/rotated (== availableTracks_
    // until setTrackSelection() narrows it down). Every existing bit of
    // rotation logic (loadTrack/skipToNext/skipToPrev/advanceToNext) only
    // ever looks at playlist_, so none of it needed to change.
    mutable std::mutex playlistMutex_;
    std::vector<juce::File> availableTracks_;
    std::vector<juce::File> playlist_;
    double deviceSampleRate_ { 44100.0 };

    ChannelPluginChain pluginChain_;
    juce::MidiBuffer pluginMidi_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackgroundMusicPlayer)
};
