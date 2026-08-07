/*
  ==============================================================================

    BackgroundMusicPlayer.cpp

  ==============================================================================
*/

#include "BackgroundMusicPlayer.h"

namespace
{
static const juce::StringArray kAudioExtensions { ".mp3", ".wav", ".ogg", ".flac", ".aac", ".m4a" };

juce::File resolveAssetDir (const juce::String& relative)
{
    auto exeDir = juce::File::getSpecialLocation (juce::File::currentExecutableFile).getParentDirectory();
    auto cwd    = juce::File::getCurrentWorkingDirectory();

    const juce::Array<juce::File> roots {
        cwd,
        exeDir,
        exeDir.getParentDirectory(),
        exeDir.getParentDirectory().getParentDirectory(),
        exeDir.getParentDirectory().getParentDirectory().getParentDirectory()
    };

    for (const auto& root : roots)
    {
        auto candidate = root.getChildFile (relative);
        if (candidate.isDirectory())
            return candidate;
    }

    return {};
}
}

//==============================================================================
BackgroundMusicPlayer::BackgroundMusicPlayer()
{
    formatManager_.registerBasicFormats();
    startTimerHz (20);  // poll for state-change callbacks
}

BackgroundMusicPlayer::~BackgroundMusicPlayer()
{
    stopTimer();
    shutdown();
}

//==============================================================================
void BackgroundMusicPlayer::initialize()
{
    if (initialized_.load())
        return;

    // Use a separate device manager for background music so it runs in
    // parallel with the main AudioEngine's device manager.
    auto err = deviceManager_.initialise (0, 2, nullptr, true);
    if (err.isNotEmpty())
    {
        DBG ("[BGMusic] Device init failed: " + err);
        return;
    }

    sourcePlayer_ = std::make_unique<juce::AudioSourcePlayer>();
    deviceManager_.addAudioCallback (sourcePlayer_.get());
    sourcePlayer_->setSource (this);

    if (auto* dev = deviceManager_.getCurrentAudioDevice())
        deviceSampleRate_ = dev->getCurrentSampleRate();

    initialized_ = true;

    // Build default playlist from assets/music
    auto musicDir = resolveAssetDir ("assets/music");
    if (musicDir.isDirectory())
        setPlaylistDirectory (musicDir);
}

void BackgroundMusicPlayer::shutdown()
{
    if (! initialized_.load())
        return;

    stop();

    if (sourcePlayer_ != nullptr)
    {
        sourcePlayer_->setSource (nullptr);
        deviceManager_.removeAudioCallback (sourcePlayer_.get());
        sourcePlayer_.reset();
    }

    deviceManager_.closeAudioDevice();

    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        resamplingSource_.reset();
        if (transportSource_) { transportSource_->setSource (nullptr); transportSource_.reset(); }
        readerSource_.reset();
    }

    initialized_ = false;
}

//==============================================================================
void BackgroundMusicPlayer::setPlaylistDirectory (const juce::File& directory)
{
    if (! directory.isDirectory())
        return;

    bool hasTracks = false;
    {
        std::lock_guard<std::mutex> lock (playlistMutex_);
        availableTracks_.clear();

        for (auto it = juce::RangedDirectoryIterator (directory, false, "*", juce::File::findFiles); it != juce::RangedDirectoryIterator(); ++it)
        {
            const auto ext = it->getFile().getFileExtension().toLowerCase();
            if (kAudioExtensions.contains (ext))
                availableTracks_.push_back (it->getFile());
        }

        // Sort alphabetically so the order is deterministic across platforms.
        std::sort (availableTracks_.begin(), availableTracks_.end(), [] (const juce::File& a, const juce::File& b) {
            return a.getFileName().compareNatural (b.getFileName()) < 0;
        });

        // Default: play everything just found, until/unless
        // setTrackSelection() narrows it down.
        playlist_ = availableTracks_;

        DBG ("[BGMusic] Playlist built: " + juce::String ((int) playlist_.size()) + " tracks");
        hasTracks = ! playlist_.empty();
    }

    if (hasTracks)
        loadTrack (0);
}

void BackgroundMusicPlayer::resetToDefaultFolder()
{
    auto musicDir = resolveAssetDir ("assets/music");
    if (musicDir.isDirectory())
        setPlaylistDirectory (musicDir);
    setTrackSelection ({});
}

void BackgroundMusicPlayer::setTrackSelection (const juce::StringArray& selectedFilenames)
{
    bool hasTracks = false;
    {
        std::lock_guard<std::mutex> lock (playlistMutex_);

        if (selectedFilenames.isEmpty())
        {
            playlist_ = availableTracks_;
        }
        else
        {
            playlist_.clear();
            for (auto& file : availableTracks_)
                if (selectedFilenames.contains (file.getFileName()))
                    playlist_.push_back (file);
        }

        DBG ("[BGMusic] Track selection applied: " + juce::String ((int) playlist_.size())
             + " / " + juce::String ((int) availableTracks_.size()) + " tracks");
        hasTracks = ! playlist_.empty();
    }

    if (hasTracks)
        loadTrack (0);
}

std::vector<juce::File> BackgroundMusicPlayer::getAvailableTracks() const
{
    std::lock_guard<std::mutex> lock (playlistMutex_);
    return availableTracks_;
}

void BackgroundMusicPlayer::playSpecificTrack (const juce::File& file)
{
    bool found = false;
    {
        std::lock_guard<std::mutex> lock (playlistMutex_);
        found = std::find (availableTracks_.begin(), availableTracks_.end(), file) != availableTracks_.end();
    }

    if (! found)
        return;

    loadTrackFile (file, -1);
    play();
}

void BackgroundMusicPlayer::playOneShotIntro (const juce::File& file, std::function<void (bool)> onFinished)
{
    if (! initialized_.load() || ! file.existsAsFile())
    {
        if (onFinished)
            juce::MessageManager::callAsync ([onFinished] { onFinished (false); });
        return;
    }

    oneShotIntroFinishedCallback_ = std::move (onFinished);
    oneShotIntroActive_ = true;

    // Force full volume immediately -- currentGain_ may currently be at (or
    // ramping toward) 0 if background music was faded out before the show
    // started, and the intro must be audible regardless.
    currentGain_ = targetVolume_.load();

    loadTrackFile (file, -1);
    play();
}

juce::String BackgroundMusicPlayer::getCurrentTrackName() const
{
    const int idx = currentIndex_.load();
    std::lock_guard<std::mutex> lock (playlistMutex_);
    if (idx < 0 || idx >= (int) playlist_.size())
        return {};
    return playlist_[(size_t) idx].getFileNameWithoutExtension();
}

//==============================================================================
void BackgroundMusicPlayer::loadTrack (int index)
{
    juce::File trackFile;
    {
        std::lock_guard<std::mutex> lock (playlistMutex_);
        if (index < 0 || index >= (int) playlist_.size())
            return;
        trackFile = playlist_[(size_t) index];
    }

    loadTrackFile (trackFile, index);
}

void BackgroundMusicPlayer::loadTrackFile (const juce::File& trackFile, int indexToSet)
{
    const bool wasPlaying = playing_.load();

    // Stop player while we rebuild the chain.
    if (sourcePlayer_ != nullptr)
        sourcePlayer_->setSource (nullptr);

    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        resamplingSource_.reset();
        if (transportSource_) { transportSource_->setSource (nullptr); transportSource_.reset(); }
        readerSource_.reset();
    }

    auto* reader = formatManager_.createReaderFor (trackFile);
    if (reader == nullptr)
    {
        DBG ("[BGMusic] Cannot read: " + trackFile.getFullPathName());
        if (sourcePlayer_ != nullptr)
            sourcePlayer_->setSource (this);
        return;
    }

    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        readerSource_    = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        transportSource_ = std::make_unique<juce::AudioTransportSource>();
        transportSource_->setSource (readerSource_.get(), 0, nullptr, reader->sampleRate);

        resamplingSource_ = std::make_unique<juce::ResamplingAudioSource> (transportSource_.get(), false, 2);
        resamplingSource_->setResamplingRatio (deviceSampleRate_ / reader->sampleRate);

        if (auto* dev = deviceManager_.getCurrentAudioDevice())
        {
            const int blockSize = dev->getCurrentBufferSizeSamples();
            resamplingSource_->prepareToPlay (blockSize, deviceSampleRate_);
        }

        totalLength_     = reader->lengthInSamples / reader->sampleRate;
        currentPosition_ = 0.0;
    }

    // indexToSet < 0 means "don't touch the rotation position" -- used by
    // playSpecificTrack() so previewing a track (which may not even be in
    // the current selection) never corrupts currentIndex_ for the next
    // real skipToNext()/skipToPrev().
    if (indexToSet >= 0)
        currentIndex_ = indexToSet;
    // NOTE: do NOT set trackChangedFlag_ here.  That flag is exclusively for
    // advanceToNext() which signals the message thread that a reload is needed.
    // loadTrack() IS the reload — setting the flag here would create an
    // infinite timer loop (timer sees flag → calls loadTrack → sets flag → …).

    if (sourcePlayer_ != nullptr)
        sourcePlayer_->setSource (this);

    if (wasPlaying)
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_) transportSource_->start();
        playing_ = true;
        playStateChangedFlag_ = true;
    }
}

//==============================================================================
void BackgroundMusicPlayer::play()
{
    if (! initialized_.load() || playlist_.empty())
        return;

    // If no track is loaded yet, load the first one.
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_ == nullptr)
        {
            // Unlock before calling loadTrack (which also locks).
        }
        else
        {
            transportSource_->start();
            playing_ = true;
            playStateChangedFlag_ = true;
            return;
        }
    }

    loadTrack (currentIndex_.load());

    std::lock_guard<std::mutex> lock (chainMutex_);
    if (transportSource_)
    {
        transportSource_->start();
        playing_ = true;
        playStateChangedFlag_ = true;
    }
}

void BackgroundMusicPlayer::pause()
{
    std::lock_guard<std::mutex> lock (chainMutex_);
    if (transportSource_)
        transportSource_->stop();
    playing_ = false;
    playStateChangedFlag_ = true;
}

void BackgroundMusicPlayer::stop()
{
    std::lock_guard<std::mutex> lock (chainMutex_);
    if (transportSource_)
    {
        transportSource_->stop();
        transportSource_->setPosition (0.0);
    }
    playing_         = false;
    currentPosition_ = 0.0;
    playStateChangedFlag_ = true;
}

void BackgroundMusicPlayer::skipToNext()
{
    const int next = (currentIndex_.load() + 1) % juce::jmax (1, (int) playlist_.size());
    const bool wasPlaying = playing_.load();
    loadTrack (next);

    if (wasPlaying)
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_) transportSource_->start();
        playing_ = true;
    }

    if (onTrackChanged) onTrackChanged();
}

void BackgroundMusicPlayer::skipToPrev()
{
    const int count = juce::jmax (1, (int) playlist_.size());
    const int prev  = (currentIndex_.load() + count - 1) % count;
    const bool wasPlaying = playing_.load();
    loadTrack (prev);

    if (wasPlaying)
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_) transportSource_->start();
        playing_ = true;
    }

    if (onTrackChanged) onTrackChanged();
}

void BackgroundMusicPlayer::seekToPosition (double seconds)
{
    std::lock_guard<std::mutex> lock (chainMutex_);
    if (transportSource_)
        transportSource_->setPosition (juce::jlimit (0.0, totalLength_.load(), seconds));
}

//==============================================================================
void BackgroundMusicPlayer::setVolume (float v)
{
    targetVolume_ = juce::jlimit (0.0f, 1.0f, v);
    // If we're not in a fade, snap immediately.
    if (! fadingOut_.load() && ! fadingIn_.load())
        currentGain_ = targetVolume_.load();
}

void BackgroundMusicPlayer::fadeOut (float durationSeconds)
{
    const float duration = juce::jmax (0.05f, durationSeconds);
    const float samplesForFade = duration * (float) deviceSampleRate_;
    // Rate = change in gain per sample.  From current gain down to 0.
    fadeRatePerSample_ = currentGain_.load() / juce::jmax (1.0f, samplesForFade);
    fadingIn_  = false;
    fadingOut_ = true;
}

void BackgroundMusicPlayer::fadeIn (float durationSeconds)
{
    if (! initialized_.load() || playlist_.empty())
        return;

    // Ensure playback is running before fading in.
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_ != nullptr && ! transportSource_->isPlaying())
        {
            transportSource_->start();
            playing_ = true;
            playStateChangedFlag_ = true;
        }
    }

    if (playlist_.empty())
        return;

    const float duration = juce::jmax (0.05f, durationSeconds);
    const float samplesForFade = duration * (float) deviceSampleRate_;
    const float target = targetVolume_.load();
    fadeRatePerSample_ = (target - currentGain_.load()) / juce::jmax (1.0f, samplesForFade);
    fadingOut_ = false;
    fadingIn_  = true;
}

//==============================================================================
void BackgroundMusicPlayer::prepareToPlay (int samplesPerBlockExpected, double sampleRate)
{
    deviceSampleRate_ = sampleRate;

    std::lock_guard<std::mutex> lock (chainMutex_);
    if (resamplingSource_)
        resamplingSource_->prepareToPlay (samplesPerBlockExpected, sampleRate);

    pluginChain_.prepare (sampleRate, samplesPerBlockExpected, 2);
}

void BackgroundMusicPlayer::releaseResources()
{
    std::lock_guard<std::mutex> lock (chainMutex_);
    if (resamplingSource_)
        resamplingSource_->releaseResources();
}

void BackgroundMusicPlayer::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();

    std::lock_guard<std::mutex> lock (chainMutex_);

    if (resamplingSource_ == nullptr || ! playing_.load())
        return;

    resamplingSource_->getNextAudioBlock (info);

    // Apply fading / volume gain sample-by-sample (simplified: per-block ramp).
    const int numSamples = info.numSamples;
    if (numSamples <= 0)
        return;

    float gain = currentGain_.load();
    const float rate = fadeRatePerSample_.load();

    auto& buf = *info.buffer;
    const int channels = juce::jmin (2, buf.getNumChannels());

    for (int i = 0; i < numSamples; ++i)
    {
        if (fadingOut_.load())
        {
            gain -= rate;
            if (gain <= 0.0f)
            {
                gain = 0.0f;
                fadingOut_ = false;
                // Request a pause from the message thread; don't touch
                // the transport source from inside the audio callback.
                playStateChangedFlag_ = true;
                pauseAfterFadeFlag_   = true;
            }
        }
        else if (fadingIn_.load())
        {
            gain += rate;
            if (gain >= targetVolume_.load())
            {
                gain = targetVolume_.load();
                fadingIn_ = false;
            }
        }

        for (int ch = 0; ch < channels; ++ch)
            buf.setSample (ch, info.startSample + i, buf.getSample (ch, info.startSample + i) * gain);
    }

    currentGain_ = gain;

    // Phase B: this player's own plugin chain, operating on exactly the
    // active sub-region of the shared device buffer (a non-owning view —
    // no allocation).
    juce::AudioBuffer<float> activeView (buf.getArrayOfWritePointers(), channels, info.startSample, numSamples);
    pluginMidi_.clear();
    pluginChain_.process (activeView, pluginMidi_);

    // Update position.
    if (transportSource_)
    {
        currentPosition_ = transportSource_->getCurrentPosition();

        // Auto-advance to next track when this one finishes -- unless a
        // one-shot intro is playing, in which case the message thread
        // needs to resume the regular rotation instead (see
        // playOneShotIntro()/timerCallback()).
        if (transportSource_->hasStreamFinished())
        {
            if (oneShotIntroActive_.load())
            {
                oneShotIntroActive_ = false;
                playing_ = false;
                oneShotIntroFinishedFlag_ = true;
            }
            else
            {
                advanceToNext();
            }
        }
    }
}

void BackgroundMusicPlayer::advanceToNext()
{
    // Called from the audio thread — just update the index and set a flag;
    // the actual loadTrack is triggered from the timer callback on the message
    // thread to avoid allocations on the audio thread.
    int playlistSize = 1;
    {
        std::lock_guard<std::mutex> lock (playlistMutex_);
        playlistSize = (int) playlist_.size();
    }
    const int next = (currentIndex_.load() + 1) % juce::jmax (1, playlistSize);
    currentIndex_ = next;
    trackChangedFlag_ = true;
    playing_ = false;  // will be restarted in timerCallback
}

void BackgroundMusicPlayer::timerCallback()
{
    // Handle deferred pause (set by fade-out completing on the audio thread).
    if (pauseAfterFadeFlag_.exchange (false))
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_) transportSource_->stop();
        playing_ = false;
    }

    // A one-shot intro (playOneShotIntro) just finished on the audio thread
    // -- resume the regular rotation from wherever it was, then tell the
    // caller. Must happen before the trackChangedFlag_ handling below,
    // which is unrelated (that's for the normal playlist auto-advance).
    if (oneShotIntroFinishedFlag_.exchange (false))
    {
        auto callback = std::move (oneShotIntroFinishedCallback_);
        oneShotIntroFinishedCallback_ = nullptr;

        loadTrack (currentIndex_.load());

        if (callback)
            callback (true);
    }

    // Fire state-change callbacks on the message thread.
    if (playStateChangedFlag_.exchange (false))
        if (onPlayStateChanged) onPlayStateChanged();

    if (trackChangedFlag_.exchange (false))
    {
        // Load the new track (set by advanceToNext on the audio thread).
        const int idx = currentIndex_.load();
        int playlistSize = 0;
        {
            std::lock_guard<std::mutex> lock (playlistMutex_);
            playlistSize = (int) playlist_.size();
        }
        if (idx >= 0 && idx < playlistSize)
        {
            loadTrack (idx);
            // Auto-advance always starts the next track playing.
            {
                std::lock_guard<std::mutex> lock (chainMutex_);
                if (transportSource_)
                {
                    transportSource_->start();
                    playing_ = true;
                }
            }
        }

        if (onTrackChanged) onTrackChanged();
    }
}
