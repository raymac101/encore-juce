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

    playlist_.clear();

    for (auto it = juce::RangedDirectoryIterator (directory, false, "*", juce::File::findFiles); it != juce::RangedDirectoryIterator(); ++it)
    {
        const auto ext = it->getFile().getFileExtension().toLowerCase();
        if (kAudioExtensions.contains (ext))
            playlist_.push_back (it->getFile());
    }

    // Sort alphabetically so the order is deterministic across platforms.
    std::sort (playlist_.begin(), playlist_.end(), [] (const juce::File& a, const juce::File& b) {
        return a.getFileName().compareNatural (b.getFileName()) < 0;
    });

    DBG ("[BGMusic] Playlist built: " + juce::String ((int) playlist_.size()) + " tracks");

    if (! playlist_.empty())
        loadTrack (0);
}

juce::String BackgroundMusicPlayer::getCurrentTrackName() const
{
    const int idx = currentIndex_.load();
    if (idx < 0 || idx >= (int) playlist_.size())
        return {};
    return playlist_[(size_t) idx].getFileNameWithoutExtension();
}

//==============================================================================
void BackgroundMusicPlayer::loadTrack (int index)
{
    if (index < 0 || index >= (int) playlist_.size())
        return;

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

    auto* reader = formatManager_.createReaderFor (playlist_[(size_t) index]);
    if (reader == nullptr)
    {
        DBG ("[BGMusic] Cannot read: " + playlist_[(size_t) index].getFullPathName());
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

    currentIndex_ = index;
    trackChangedFlag_ = true;

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
    loadTrack (next);

    if (playing_.load())
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_) transportSource_->start();
    }
}

void BackgroundMusicPlayer::skipToPrev()
{
    const int count = juce::jmax (1, (int) playlist_.size());
    const int prev  = (currentIndex_.load() + count - 1) % count;
    loadTrack (prev);

    if (playing_.load())
    {
        std::lock_guard<std::mutex> lock (chainMutex_);
        if (transportSource_) transportSource_->start();
    }
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
                // Pause (but keep position) when fully faded out.
                transportSource_->stop();
                playing_ = false;
                playStateChangedFlag_ = true;
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

    // Update position.
    if (transportSource_)
    {
        currentPosition_ = transportSource_->getCurrentPosition();

        // Auto-advance to next track when this one finishes.
        if (transportSource_->hasStreamFinished())
            advanceToNext();
    }
}

void BackgroundMusicPlayer::advanceToNext()
{
    // Called from the audio thread — just update the index and set a flag;
    // the actual loadTrack is triggered from the timer callback on the message
    // thread to avoid allocations on the audio thread.
    const int next = (currentIndex_.load() + 1) % juce::jmax (1, (int) playlist_.size());
    currentIndex_ = next;
    trackChangedFlag_ = true;
    playing_ = false;  // will be restarted in timerCallback
}

void BackgroundMusicPlayer::timerCallback()
{
    // Fire state-change callbacks on the message thread.
    if (playStateChangedFlag_.exchange (false))
        if (onPlayStateChanged) onPlayStateChanged();

    if (trackChangedFlag_.exchange (false))
    {
        // Load the new track (may have been set by advanceToNext on the audio thread).
        const int idx = currentIndex_.load();
        if (idx >= 0 && idx < (int) playlist_.size())
        {
            const bool wasPlaying = playing_.load();
            loadTrack (idx);
            if (wasPlaying || (fadingIn_.load()))
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
