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
    /** Scans a directory for supported audio files and rebuilds the playlist.
        Must be called from the message thread. */
    void setPlaylistDirectory (const juce::File& directory);

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
    void advanceToNext();

    std::atomic<bool> initialized_ { false };
    std::atomic<bool> playing_ { false };
    std::atomic<double> currentPosition_ { 0.0 };
    std::atomic<double> totalLength_ { 0.0 };

    // Volume & fade state (manipulated from both threads; atomic)
    std::atomic<float> targetVolume_ { 0.5f };
    std::atomic<float> currentGain_ { 0.5f };
    std::atomic<float> fadeRatePerSample_ { 0.0f };
    std::atomic<bool> fadingOut_ { false };
    std::atomic<bool> fadingIn_  { false };
    std::atomic<bool> pauseAfterFadeFlag_ { false };  // set by audio thread, handled on msg thread

    std::atomic<int> currentIndex_ { 0 };
    std::atomic<bool> trackChangedFlag_ { false };
    std::atomic<bool> playStateChangedFlag_ { false };

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
    mutable std::mutex playlistMutex_;
    std::vector<juce::File> playlist_;
    double deviceSampleRate_ { 44100.0 };

    ChannelPluginChain pluginChain_;
    juce::MidiBuffer pluginMidi_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BackgroundMusicPlayer)
};
