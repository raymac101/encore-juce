/*
  ==============================================================================

    AudioEngine.cpp

  ==============================================================================
*/

#include "AudioEngine.h"
#include <cmath>

namespace
{
double estimateAudibleEndSeconds (juce::AudioFormatReader& reader, float silenceThresholdDb)
{
    const auto totalSamples = reader.lengthInSamples;
    if (totalSamples <= 0 || reader.sampleRate <= 0.0)
        return 0.0;

    constexpr int kBlock = 16384;
    const float silenceThreshold = juce::Decibels::decibelsToGain (
        juce::jlimit (-80.0f, -20.0f, silenceThresholdDb));

    const int channels = juce::jlimit (1, 2, (int) reader.numChannels);
    juce::AudioBuffer<float> buffer (channels, kBlock);

    juce::int64 endExclusive = totalSamples;

    while (endExclusive > 0)
    {
        const juce::int64 start = juce::jmax<juce::int64> (0, endExclusive - kBlock);
        const int count = (int) (endExclusive - start);

        buffer.clear();
        if (! reader.read (&buffer, 0, count, start, true, channels > 1))
            break;

        for (int i = count - 1; i >= 0; --i)
        {
            float peak = 0.0f;
            for (int ch = 0; ch < channels; ++ch)
                peak = juce::jmax (peak, std::abs (buffer.getSample (ch, i)));

            if (peak > silenceThreshold)
            {
                const juce::int64 lastAudibleSample = start + i;
                const double seconds = (double) (lastAudibleSample + 1) / reader.sampleRate;
                return juce::jlimit (0.0,
                                     (double) totalSamples / reader.sampleRate,
                                     seconds);
            }
        }

        endExclusive = start;
    }

    return 0.0;
}
}

//==============================================================================
AudioEngine::AudioEngine()
{
    formatManager.registerBasicFormats();
    frequencyData.resize(512, 0.0f);
}

AudioEngine::~AudioEngine()
{
    shutdown();
}

//==============================================================================
// Lifecycle
//==============================================================================

void AudioEngine::initialize()
{
    std::lock_guard<std::mutex> lock(lifecycleMutex);

    if (initialized)
        return;

    if (! setupAudioDevice())
        return;

    // Only actually register mic capture if the feature is enabled AND the
    // device that ended up open genuinely exposes at least one input
    // channel — a mic-input request can silently succeed with fewer
    // channels than asked for, so check the real result rather than trust
    // the request (Risk #1/#3: never assume, always verify capability).
    const bool wantMicInput = UserPreferences::getInstance().getLiveVocalInputEnabled();
    bool deviceHasInput = false;
    if (auto* device = deviceManager.getCurrentAudioDevice())
        deviceHasInput = device->getActiveInputChannels().countNumberOfSetBits() > 0;

    if (wantMicInput && deviceHasInput)
    {
        micCapture.micChannelIndex[0] = UserPreferences::getInstance().getMicInputChannel(1);
        micCapture.micChannelIndex[1] = UserPreferences::getInstance().getMicInputChannel(2);
        vocal1Gain = UserPreferences::getInstance().getMicGain(1);
        vocal2Gain = UserPreferences::getInstance().getMicGain(2);

        // Registered *before* audioSourcePlayer so it captures first within
        // the same audio-thread device callback invocation.
        deviceManager.addAudioCallback(&micCapture);
        micCaptureRegistered = true;
        DBG("[AudioStartup] Live vocal input enabled and device has input channels — mic capture registered.");
    }
    else
    {
        micCaptureRegistered = false;
        if (wantMicInput)
            DBG("[AudioStartup] Live vocal input enabled but the current device has no input channels — mic capture disabled for this session.");
    }

    audioSourcePlayer = std::make_unique<juce::AudioSourcePlayer>();
    deviceManager.addAudioCallback(audioSourcePlayer.get());
    audioSourcePlayer->setSource(this);

    initialized = true;
    persistActiveAudioDevice();
}

void AudioEngine::shutdown()
{
    std::lock_guard<std::mutex> lock(lifecycleMutex);

    if (!initialized)
        return;

    stop();

    if (audioSourcePlayer != nullptr)
    {
        audioSourcePlayer->setSource(nullptr);
        deviceManager.removeAudioCallback(audioSourcePlayer.get());
        audioSourcePlayer.reset();
    }

    if (micCaptureRegistered.exchange(false))
        deviceManager.removeAudioCallback(&micCapture);

    deviceManager.closeAudioDevice();

    resamplingSource.reset();

    if (transportSource != nullptr)
    {
        transportSource->setSource(nullptr);
        transportSource.reset();
    }

    readerSource.reset();
    initialized = false;
}

bool AudioEngine::setupAudioDevice()
{
    const auto startMs = juce::Time::getMillisecondCounterHiRes();
    const auto preferredDevice = UserPreferences::getInstance().getPreferredAudioOutputDevice().trim();

    juce::AudioDeviceManager::AudioDeviceSetup preferredSetup;
    const juce::AudioDeviceManager::AudioDeviceSetup* preferredSetupPtr = nullptr;

    if (preferredDevice.isNotEmpty())
    {
        preferredSetup.outputDeviceName = preferredDevice;
        preferredSetup.inputDeviceName.clear();
        preferredSetupPtr = &preferredSetup;
        DBG("[AudioStartup] Attempting preferred output device: " + preferredDevice);
    }
    else
    {
        DBG("[AudioStartup] No saved output device. Falling back to Windows default device.");
    }

    // Only request input channels when live vocal input is enabled — with
    // the feature flag off, this is byte-for-byte the original 0-input call
    // (Risk #1: existing users see zero behaviour change).
    const bool wantMicInput = UserPreferences::getInstance().getLiveVocalInputEnabled();
    const int numInputChannelsRequested = wantMicInput ? 2 : 0;

    auto error = deviceManager.initialise(numInputChannelsRequested, 2, nullptr, true, {}, preferredSetupPtr);

    // A mic-input request failing must never take down playback itself —
    // fall back to the plain output-only setup automatically.
    if (error.isNotEmpty() && numInputChannelsRequested > 0)
    {
        DBG("[AudioStartup] Input-channel init failed (" + error + "); retrying output-only.");
        error = deviceManager.initialise(0, 2, nullptr, true, {}, preferredSetupPtr);
    }

    const auto initMs = juce::Time::getMillisecondCounterHiRes() - startMs;

    if (error.isNotEmpty())
    {
        handleAudioDeviceError("Failed to initialise audio device: " + error);
        DBG("[AudioStartup] deviceManager.initialise failed after " + juce::String(initMs, 1) + " ms");
        return false;
    }

    deviceManager.addChangeListener(this);

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        DBG("[AudioStartup] Opened output device '" + device->getName()
            + "' @ " + juce::String(device->getCurrentSampleRate(), 0)
            + " Hz, buffer " + juce::String(device->getCurrentBufferSizeSamples())
            + " in " + juce::String(initMs, 1) + " ms");
    }
    else
    {
        DBG("[AudioStartup] No active audio device after initialise; completed in "
            + juce::String(initMs, 1) + " ms");
    }

    return true;
}

void AudioEngine::persistActiveAudioDevice() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto name = device->getName().trim();
        if (name.isNotEmpty())
            UserPreferences::getInstance().setPreferredAudioOutputDevice(name);
    }
}

void AudioEngine::handleAudioDeviceError(const juce::String& message)
{
    DBG("AudioEngine error: " + message);
}

//==============================================================================
// Playback control
//==============================================================================

bool AudioEngine::loadSong(const juce::File& audioFile, const juce::File& cdgFile)
{
    if (!initialized)
        return false;

    if (!audioFile.exists())
    {
        DBG("loadSong: file not found: " + audioFile.getFullPathName());
        return false;
    }

    stop();

    // Detach source player before tearing down the chain so the audio thread
    // cannot dereference dangling pointers.
    if (audioSourcePlayer != nullptr)
        audioSourcePlayer->setSource(nullptr);

    resamplingSource.reset();

    if (transportSource != nullptr)
    {
        transportSource->setSource(nullptr);
        transportSource.reset();
    }

    readerSource.reset();

    auto* reader = formatManager.createReaderFor(audioFile);
    if (reader == nullptr)
    {
        DBG("loadSong: no reader for " + audioFile.getFullPathName());
        if (audioSourcePlayer != nullptr)
            audioSourcePlayer->setSource(this);
        return false;
    }

    readerSource    = std::make_unique<juce::AudioFormatReaderSource>(reader, true);
    transportSource = std::make_unique<juce::AudioTransportSource>();
    transportSource->setSource(readerSource.get(), 0, nullptr, reader->sampleRate);

    double deviceSR = 44100.0;
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        deviceSR = dev->getCurrentSampleRate();

    resamplingSource = std::make_unique<juce::ResamplingAudioSource>(
        transportSource.get(), false, 2);
    resamplingSource->setResamplingRatio(deviceSR / reader->sampleRate);

    // Prepare the new chain with the device's current parameters.
    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        const int blockSize = dev->getCurrentBufferSizeSamples();
        resamplingSource->prepareToPlay(blockSize, deviceSR);
        pitchShifter.prepare(deviceSR, blockSize, 2);
    }

    pitchShifter.reset();

    totalLength     = reader->lengthInSamples / reader->sampleRate;
    audibleEndPosition = estimateAudibleEndSeconds (
        *reader,
        UserPreferences::getInstance().getTrailingSilenceThresholdDb());
    if (audibleEndPosition.load() <= 0.0)
        audibleEndPosition = totalLength.load();
    currentPosition = 0.0;
    audibleEndNotified = false;

    cdgLoaded = cdgFile.exists();

    // Reset echo buffer write position so residual feedback from the previous
    // song does not bleed into the new one.
    echoWritePos = 0;
    echoBuffer.clear();

    if (audioSourcePlayer != nullptr)
        audioSourcePlayer->setSource(this);

    DBG("loadSong: loaded " + audioFile.getFileName()
        + "  (" + juce::String(totalLength.load(), 1) + " s, audible end "
        + juce::String(audibleEndPosition.load(), 1) + " s)");
    return true;
}

void AudioEngine::unloadSong()
{
    stop();

    if (audioSourcePlayer != nullptr)
        audioSourcePlayer->setSource(nullptr);

    resamplingSource.reset();

    if (transportSource != nullptr)
    {
        transportSource->setSource(nullptr);
        transportSource.reset();
    }

    readerSource.reset();

    totalLength = 0.0;
    currentPosition = 0.0;
    audibleEndPosition = 0.0;
    cdgLoaded = false;

    if (audioSourcePlayer != nullptr)
        audioSourcePlayer->setSource(this);
}

void AudioEngine::play()
{
    if (transportSource == nullptr)
        return;

    const auto audibleEnd = audibleEndPosition.load();
    if (audibleEnd > 0.0 && currentPosition.load() < audibleEnd)
        audibleEndNotified = false;

    if (!playing)
    {
        transportSource->start();
        playing = true;
        paused  = false;
    }
}

void AudioEngine::pause()
{
    if (transportSource == nullptr)
        return;

    if (playing)
    {
        transportSource->stop();
        playing = false;
        paused  = true;
        masterCompOutputMeter = 0.0f;
        masterLimiterReductionMeter = 0.0f;
    }
    else if (paused)
    {
        transportSource->start();
        playing = true;
        paused  = false;
    }
}

void AudioEngine::stop()
{
    if (transportSource == nullptr)
        return;

    transportSource->stop();
    transportSource->setPosition(0.0);
    playing         = false;
    paused          = false;
    currentPosition = 0.0;
    audibleEndNotified = false;
    masterCompOutputMeter = 0.0f;
    masterLimiterReductionMeter = 0.0f;
}

void AudioEngine::seekToPosition(double positionInSeconds)
{
    if (transportSource == nullptr)
        return;

    // Detach the audio callback before touching the pitch shifter's internal
    // buffers/stretcher — pitchShifter.reset() reallocates them with no
    // locking, and the audio thread may be mid-process() on the same
    // objects. Mirrors the detach/reattach already done in loadSong()/
    // unloadSong() for the same reason.
    if (audioSourcePlayer != nullptr)
        audioSourcePlayer->setSource(nullptr);

    positionInSeconds = juce::jlimit(0.0, totalLength.load(), positionInSeconds);
    transportSource->setPosition(positionInSeconds);
    currentPosition = positionInSeconds;

    const auto audibleEnd = audibleEndPosition.load();
    if (audibleEnd > 0.0 && positionInSeconds < audibleEnd)
        audibleEndNotified = false;

    // Flush RubberBand so we don't hear the pre-seek audio at the new position.
    pitchShifter.reset();

    if (audioSourcePlayer != nullptr)
        audioSourcePlayer->setSource(this);
}

//==============================================================================
// Pitch and tempo
//==============================================================================

void AudioEngine::setPitchShift(float semitones)
{
    pitchShifter.setPitchSemitones(semitones);
}

void AudioEngine::setTempoAdjustment(float ratio)
{
    pitchShifter.setTimeRatio(ratio);
}

void AudioEngine::setKeyChange(int semitones)
{
    keyChangeSemitones = juce::jlimit(-12, 12, semitones);
    pitchShifter.setPitchSemitones(static_cast<float>(keyChangeSemitones.load()));
}

//==============================================================================
// Volume
//==============================================================================

void AudioEngine::setMasterVolume(float v) { masterVolume = juce::jlimit(0.0f, 1.0f, v); }
void AudioEngine::setMusicVolume(float v)  { musicVolume  = juce::jlimit(0.0f, 1.0f, v); }
void AudioEngine::setVocalVolume(float v)  { vocalVolume  = juce::jlimit(0.0f, 1.0f, v); }
void AudioEngine::setVocalEffectsLevel(float l) { vocalEffectsLevel = juce::jlimit(0.0f, 1.0f, l); }
void AudioEngine::setSfxVolume(float v)    { sfxVolume    = juce::jlimit(0.0f, 1.0f, v); }

void AudioEngine::setVocal1Gain(float gain)
{
    vocal1Gain = juce::jlimit(0.0f, 1.0f, gain);
    UserPreferences::getInstance().setMicGain(1, vocal1Gain.load());
}

void AudioEngine::setVocal2Gain(float gain)
{
    vocal2Gain = juce::jlimit(0.0f, 1.0f, gain);
    UserPreferences::getInstance().setMicGain(2, vocal2Gain.load());
}

void AudioEngine::setMicInputChannel(int micIndex, int deviceChannelIndex)
{
    if (micIndex != 1 && micIndex != 2)
        return;

    micCapture.micChannelIndex[micIndex - 1] = deviceChannelIndex;
    UserPreferences::getInstance().setMicInputChannel(micIndex, deviceChannelIndex);
}

int AudioEngine::getMicInputChannel(int micIndex) const
{
    if (micIndex != 1 && micIndex != 2)
        return -1;

    return micCapture.micChannelIndex[micIndex - 1].load();
}

juce::StringArray AudioEngine::getAvailableInputChannelNames() const
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getInputChannelNames();

    return {};
}

bool AudioEngine::triggerOneShotSfx(const juce::File& audioFile)
{
    if (! initialized || ! audioFile.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(audioFile));
    if (reader == nullptr || reader->lengthInSamples <= 0)
        return false;

    constexpr double kMaxSfxSeconds = 20.0;
    const auto maxSamples = (juce::int64) (reader->sampleRate * kMaxSfxSeconds);
    const int numSamples = (int) juce::jlimit<juce::int64>(1, maxSamples, reader->lengthInSamples);
    const int channels = juce::jlimit(1, 2, (int) reader->numChannels);

    juce::AudioBuffer<float> decoded(channels, numSamples);
    if (! reader->read(&decoded, 0, numSamples, 0, true, channels > 1))
        return false;

    {
        const std::lock_guard<std::mutex> lock(sfxMutex);
        oneShotSfxBuffer = std::move(decoded);
        oneShotSfxReadPos = 0;
        oneShotSfxActive = true;
    }

    return true;
}

void AudioEngine::setMasterEqLow(float db)
{
    masterEqLowDb = juce::jlimit(-18.0f, 18.0f, db);
    masterEqState.dirty = true;
}

void AudioEngine::setMasterEqMid(float db)
{
    masterEqMidDb = juce::jlimit(-18.0f, 18.0f, db);
    masterEqState.dirty = true;
}

void AudioEngine::setMasterEqHigh(float db)
{
    masterEqHighDb = juce::jlimit(-18.0f, 18.0f, db);
    masterEqState.dirty = true;
}

void AudioEngine::setMasterInsertDrive(float amount)
{
    masterInsertDrive = juce::jlimit(0.0f, 1.0f, amount);
}

void AudioEngine::setMasterCompressorThreshold(float db)
{
    masterCompThresholdDb = juce::jlimit(-48.0f, 0.0f, db);
    masterDynamicsState.dirty = true;
}

void AudioEngine::setMasterCompressorRatio(float ratio)
{
    masterCompRatio = juce::jlimit(1.0f, 20.0f, ratio);
}

void AudioEngine::setMasterCompressorAttackMs(float ms)
{
    masterCompAttackMs = juce::jlimit(1.0f, 200.0f, ms);
    masterDynamicsState.dirty = true;
}

void AudioEngine::setMasterCompressorReleaseMs(float ms)
{
    masterCompReleaseMs = juce::jlimit(10.0f, 1000.0f, ms);
    masterDynamicsState.dirty = true;
}

void AudioEngine::setMasterCompressorMakeupDb(float db)
{
    masterCompMakeupDb = juce::jlimit(0.0f, 18.0f, db);
}

void AudioEngine::setMasterCompressorEnabled(bool enabled)
{
    masterCompEnabled = enabled;
}

void AudioEngine::setMasterLimiterCeilingDb(float db)
{
    masterLimiterCeilingDb = juce::jlimit(-12.0f, -0.1f, db);
}

void AudioEngine::setMasterLimiterReleaseMs(float ms)
{
    masterLimiterReleaseMs = juce::jlimit(5.0f, 500.0f, ms);
    masterDynamicsState.dirty = true;
}

void AudioEngine::setMasterLimiterEnabled(bool enabled)
{
    masterLimiterEnabled = enabled;
}

//==============================================================================
// Reverb
//==============================================================================

void AudioEngine::setReverbEnabled(bool enabled)
{
    reverbEnabled = enabled;
    if (!enabled)
        reverb.reset();
}

void AudioEngine::setReverbRoomSize(float size)
{
    reverbState.roomSize = juce::jlimit(0.0f, 1.0f, size);
    reverbState.dirty    = true;
}

void AudioEngine::setReverbLevel(float level)
{
    reverbState.wetLevel = juce::jlimit(0.0f, 1.0f, level);
    reverbState.dirty    = true;
}

//==============================================================================
// Echo
//==============================================================================

void AudioEngine::setEchoEnabled(bool enabled)  { echoEnabled = enabled; }
void AudioEngine::setEchoLevel(float level)      { echoLevel   = juce::jlimit(0.0f, 0.95f, level); }
void AudioEngine::setEchoDelay(float delayMs)    { echoDelayMs = juce::jlimit(50.0f, 2000.0f, delayMs); }

//==============================================================================
// Analysis
//==============================================================================

void AudioEngine::enableFrequencyAnalysis(bool enabled)
{
    frequencyAnalysisEnabled = enabled;
}

//==============================================================================
// CDG
//==============================================================================

void AudioEngine::setCDGSyncCallback(std::function<void(double, const juce::String&)> cb)
{
    cdgSyncCallback = std::move(cb);
}

//==============================================================================
// AudioSource interface
//==============================================================================

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    echoSampleRate = sampleRate;

    if (resamplingSource != nullptr)
        resamplingSource->prepareToPlay(samplesPerBlockExpected, sampleRate);

    pitchShifter.prepare(sampleRate, samplesPerBlockExpected, 2);

    reverb.setSampleRate(sampleRate);

    // Allocate echo delay buffer: 2 seconds maximum.
    maxEchoDelaySamples = static_cast<int>(sampleRate * 2.0);
    echoBuffer.setSize(2, maxEchoDelaySamples, false, true, false);
    echoWritePos = 0;

    masterEqState.dirty = true;
    masterDynamicsState.sampleRate = sampleRate;
    masterDynamicsState.dirty = true;

    // Per-channel plugin-chain scratch buffers — sized once here so
    // getNextAudioBlock() never allocates on the audio thread.
    musicBuf_.setSize(2, samplesPerBlockExpected, false, false, true);
    vocal1Buf_.setSize(2, samplesPerBlockExpected, false, false, true);
    vocal2Buf_.setSize(2, samplesPerBlockExpected, false, false, true);
    sfxBuf_.setSize(2, samplesPerBlockExpected, false, false, true);

    musicPluginChain_.prepare(sampleRate, samplesPerBlockExpected, 2);
    vocal1PluginChain_.prepare(sampleRate, samplesPerBlockExpected, 2);
    vocal2PluginChain_.prepare(sampleRate, samplesPerBlockExpected, 2);
    fxPluginChain_.prepare(sampleRate, samplesPerBlockExpected, 2);
    masterPluginChain_.prepare(sampleRate, samplesPerBlockExpected, 2);
}

void AudioEngine::releaseResources()
{
    if (resamplingSource != nullptr)
        resamplingSource->releaseResources();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    const bool programActive = (resamplingSource != nullptr && playing.load());
    const bool micActive     = micCaptureRegistered.load();
    juce::AudioBuffer<float>& buf = *bufferToFill.buffer;
    const int numSamples  = bufferToFill.numSamples;
    const int numChannels = juce::jmin(buf.getNumChannels(), 2);

    // Each real channel — Music, Vocal 1, Vocal 2, Effects/SFX — is
    // rendered into its own scratch buffer and run through its own plugin
    // chain before being summed into the master buffer below. This is what
    // makes per-channel plugins actually isolated (e.g. a Vocal-only reverb
    // never touches the backing track), rather than all channels sharing
    // one buffer. Capacity was fixed at prepare-time; setSize here with
    // avoidReallocating=true only narrows the logical size, no allocation.
    musicBuf_.setSize(numChannels, numSamples, false, false, true);
    musicBuf_.clear();

    if (programActive)
    {
        // 1. Pull from the source chain straight into Music's scratch buffer.
        juce::AudioSourceChannelInfo musicInfo(&musicBuf_, 0, numSamples);
        resamplingSource->getNextAudioBlock(musicInfo);

        // 2. Pitch shift + time stretch via RubberBand.
        pitchShifter.process(musicBuf_);

        // 3. Music's own volume (Master's contribution is applied once,
        // uniformly, when all channels are summed below).
        musicBuf_.applyGain(musicVolume.load());

        // 4. Music's plugin chain.
        musicMidi_.clear();
        musicPluginChain_.process(musicBuf_, musicMidi_);
    }

    // Vocal 1 / Vocal 2 — raw mic samples + each mic's own gain, no
    // pitch-shift, each through its own plugin chain.
    vocal1Buf_.setSize(numChannels, numSamples, false, false, true);
    vocal1Buf_.clear();
    vocal2Buf_.setSize(numChannels, numSamples, false, false, true);
    vocal2Buf_.clear();

    if (micActive)
    {
        fillMicChannelBuffer(vocal1Buf_, 1, numSamples);
        fillMicChannelBuffer(vocal2Buf_, 2, numSamples);
    }

    vocal1Midi_.clear();
    vocal1PluginChain_.process(vocal1Buf_, vocal1Midi_);
    vocal2Midi_.clear();
    vocal2PluginChain_.process(vocal2Buf_, vocal2Midi_);

    // Effects / SFX — decoded one-shot sound into its own scratch buffer
    // (previously mixed directly into the shared program buffer).
    sfxBuf_.setSize(numChannels, numSamples, false, false, true);
    sfxBuf_.clear();
    const bool sfxActive = mixOneShotSfx(sfxBuf_, 0, numSamples);
    sfxMidi_.clear();
    fxPluginChain_.process(sfxBuf_, sfxMidi_);

    // Live mic monitoring must keep running through the master stages below
    // whenever mic capture is registered, even with no song loaded and no
    // SFX playing — a KJ needs to be able to test/use mics between songs,
    // not only while a song is playing.
    if (! programActive && ! sfxActive && ! micActive)
    {
        currentAudioLevel = 0.0f;
        masterCompOutputMeter = 0.0f;
        masterLimiterReductionMeter = 0.0f;
        return;
    }

    // Sum all four channels into the master buffer. Master's own volume is
    // applied uniformly here — this is what makes Master "the overall
    // volume of everything combined."
    const float master = masterVolume.load();
    buf.clear();
    for (int ch = 0; ch < numChannels; ++ch)
    {
        if (programActive)
            buf.addFrom(ch, bufferToFill.startSample, musicBuf_, ch, 0, numSamples, master);
        buf.addFrom(ch, bufferToFill.startSample, vocal1Buf_, ch, 0, numSamples, master);
        buf.addFrom(ch, bufferToFill.startSample, vocal2Buf_, ch, 0, numSamples, master);
        buf.addFrom(ch, bufferToFill.startSample, sfxBuf_,    ch, 0, numSamples, master);
    }

    // Master EQ / reverb / echo / insert saturation — existing, unchanged
    // master-bus processing, now applied to the combined signal.
    applyMasterEq(buf);
    applyReverb(buf);
    applyEcho(buf);
    applyMasterInsert(buf);

    // Master's own optional plugin chain (Phase B) — deliberately placed
    // *before* the always-on compressor/limiter below, so the safety
    // limiter always has final say over the output regardless of what
    // third-party plugin is loaded here.
    masterMidi_.clear();
    masterPluginChain_.process(buf, masterMidi_);

    // Always-on master compressor + limiter for song-to-song consistency.
    applyMasterDynamics(buf);

    // Update position and VU level.
    if (programActive)
        updatePlaybackPosition();

    if (frequencyAnalysisEnabled)
        performFrequencyAnalysis(buf);

    currentAudioLevel = buf.getRMSLevel(0, bufferToFill.startSample, bufferToFill.numSamples);

    // CDG sync callback.
    if (programActive && cdgLoaded && cdgSyncCallback)
        cdgSyncCallback(currentPosition.load(), {});
}

bool AudioEngine::mixOneShotSfx(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! oneShotSfxActive.load() || numSamples <= 0)
        return false;

    const std::lock_guard<std::mutex> lock(sfxMutex);
    if (! oneShotSfxActive.load() || oneShotSfxBuffer.getNumSamples() <= 0)
        return false;

    const int available = oneShotSfxBuffer.getNumSamples() - oneShotSfxReadPos;
    if (available <= 0)
    {
        oneShotSfxActive = false;
        return false;
    }

    const int toMix = juce::jmin(numSamples, available);
    const int srcChannels = juce::jmax(1, oneShotSfxBuffer.getNumChannels());
    const int dstChannels = juce::jmin(buffer.getNumChannels(), 2);
    // Read live so the Mixer page's Effects fader/mute/solo affects a sound
    // that's already playing, not just future triggers. Master's own
    // contribution is applied once, uniformly, when channels are summed in
    // getNextAudioBlock — this is the Effects channel's own level only.
    const float gain = sfxVolume.load();

    for (int ch = 0; ch < dstChannels; ++ch)
    {
        const int srcCh = juce::jmin(ch, srcChannels - 1);
        auto* dst = buffer.getWritePointer(ch, startSample);
        const auto* src = oneShotSfxBuffer.getReadPointer(srcCh, oneShotSfxReadPos);

        for (int i = 0; i < toMix; ++i)
            dst[i] += src[i] * gain;
    }

    oneShotSfxReadPos += toMix;
    if (oneShotSfxReadPos >= oneShotSfxBuffer.getNumSamples())
    {
        oneShotSfxActive = false;
        oneShotSfxReadPos = 0;
        oneShotSfxBuffer.setSize(0, 0);
    }

    return true;
}

void AudioEngine::fillMicChannelBuffer(juce::AudioBuffer<float>& dest, int micIndex, int numSamples)
{
    const int available = micCapture.getCapturedSamples();
    if (available <= 0)
        return;

    const float gain = (micIndex == 1 ? vocal1Gain.load() : vocal2Gain.load());
    if (gain <= 0.0f)
        return;

    const int toMix = juce::jmin(numSamples, available);
    const int dstChannels = juce::jmin(dest.getNumChannels(), 2);
    const float* src = micCapture.getChannelData(micIndex - 1);

    for (int ch = 0; ch < dstChannels; ++ch)
    {
        auto* d = dest.getWritePointer(ch);
        for (int i = 0; i < toMix; ++i)
            d[i] += src[i] * gain;
    }
}

//==============================================================================
// Private DSP helpers
//==============================================================================

void AudioEngine::applyReverb(juce::AudioBuffer<float>& buffer)
{
    if (!reverbEnabled.load())
        return;

    // Update reverb parameters on the audio thread only when dirty.
    if (reverbState.dirty.exchange(false))
    {
        juce::Reverb::Parameters p;
        p.roomSize  = reverbState.roomSize.load();
        p.damping   = 0.5f;
        p.width     = 1.0f;
        p.wetLevel  = reverbState.wetLevel.load() * vocalEffectsLevel.load();
        p.dryLevel  = 1.0f - p.wetLevel;
        p.freezeMode = 0.0f;
        reverb.setParameters(p);
    }

    if (buffer.getNumChannels() >= 2)
        reverb.processStereo(buffer.getWritePointer(0),
                             buffer.getWritePointer(1),
                             buffer.getNumSamples());
    else
        reverb.processMono(buffer.getWritePointer(0), buffer.getNumSamples());
}

void AudioEngine::applyEcho(juce::AudioBuffer<float>& buffer)
{
    if (!echoEnabled.load() || maxEchoDelaySamples == 0)
        return;

    const float feedback = echoLevel.load();
    const int delaySamples = juce::jlimit(
        1,
        maxEchoDelaySamples - 1,
        static_cast<int>(echoDelayMs.load() / 1000.0f * static_cast<float>(echoSampleRate)));

    const int numSamples = buffer.getNumSamples();
    const int channels   = juce::jmin(buffer.getNumChannels(), echoBuffer.getNumChannels());

    for (int i = 0; i < numSamples; ++i)
    {
        const int readPos = (echoWritePos + maxEchoDelaySamples - delaySamples)
                            % maxEchoDelaySamples;

        for (int ch = 0; ch < channels; ++ch)
        {
            const float dry    = buffer.getSample(ch, i);
            const float delayed = echoBuffer.getSample(ch, readPos);
            const float out    = dry + delayed * feedback;
            buffer.setSample(ch, i, out);
            echoBuffer.setSample(ch, echoWritePos, out);
        }

        echoWritePos = (echoWritePos + 1) % maxEchoDelaySamples;
    }
}

void AudioEngine::applyMasterEq(juce::AudioBuffer<float>& buffer)
{
    if (masterEqState.dirty.exchange(false))
    {
        const auto sr = juce::jmax(22050.0, getCurrentSampleRate());
        const auto low = juce::Decibels::decibelsToGain(masterEqLowDb.load());
        const auto mid = juce::Decibels::decibelsToGain(masterEqMidDb.load());
        const auto high = juce::Decibels::decibelsToGain(masterEqHighDb.load());

        auto lowCoeffs = juce::IIRCoefficients::makeLowShelf(sr, 120.0, 0.7071, low);
        auto midCoeffs = juce::IIRCoefficients::makePeakFilter(sr, 1200.0, 0.8, mid);
        auto highCoeffs = juce::IIRCoefficients::makeHighShelf(sr, 8000.0, 0.7071, high);

        for (int ch = 0; ch < 2; ++ch)
        {
            masterEqState.lowShelf[(size_t) ch].setCoefficients(lowCoeffs);
            masterEqState.midPeak[(size_t) ch].setCoefficients(midCoeffs);
            masterEqState.highShelf[(size_t) ch].setCoefficients(highCoeffs);
        }
    }

    const int channels = juce::jmin(2, buffer.getNumChannels());
    for (int ch = 0; ch < channels; ++ch)
    {
        auto* s = buffer.getWritePointer(ch);
        const int n = buffer.getNumSamples();
        masterEqState.lowShelf[(size_t) ch].processSamples(s, n);
        masterEqState.midPeak[(size_t) ch].processSamples(s, n);
        masterEqState.highShelf[(size_t) ch].processSamples(s, n);
    }
}

void AudioEngine::applyMasterInsert(juce::AudioBuffer<float>& buffer)
{
    const float drive = masterInsertDrive.load();
    if (drive <= 0.0001f)
        return;

    const float inGain = 1.0f + drive * 6.0f;
    const float outGain = 1.0f / (1.0f + drive * 1.8f);
    const int channels = buffer.getNumChannels();
    const int n = buffer.getNumSamples();

    for (int ch = 0; ch < channels; ++ch)
    {
        auto* s = buffer.getWritePointer(ch);
        for (int i = 0; i < n; ++i)
        {
            const float x = s[i] * inGain;
            s[i] = std::tanh(x) * outGain;
        }
    }
}

void AudioEngine::updateMasterDynamicsCoefficients()
{
    const auto sr = juce::jmax(22050.0, masterDynamicsState.sampleRate);

    const auto attackSeconds = juce::jmax(0.001f, masterCompAttackMs.load() * 0.001f);
    const auto releaseSeconds = juce::jmax(0.005f, masterCompReleaseMs.load() * 0.001f);
    const auto limiterReleaseSeconds = juce::jmax(0.003f, masterLimiterReleaseMs.load() * 0.001f);

    masterDynamicsState.compAttackCoeff = std::exp(-1.0f / (float) (attackSeconds * (float) sr));
    masterDynamicsState.compReleaseCoeff = std::exp(-1.0f / (float) (releaseSeconds * (float) sr));
    masterDynamicsState.limiterReleaseCoeff = std::exp(-1.0f / (float) (limiterReleaseSeconds * (float) sr));
}

void AudioEngine::applyMasterDynamics(juce::AudioBuffer<float>& buffer)
{
    if (masterDynamicsState.dirty.exchange(false))
        updateMasterDynamicsCoefficients();

    const int channels = juce::jmin(2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();
    if (channels <= 0 || n <= 0)
        return;

    auto* left = buffer.getWritePointer(0);
    auto* right = channels > 1 ? buffer.getWritePointer(1) : nullptr;

    const bool compEnabled = masterCompEnabled.load();
    const bool limiterEnabled = masterLimiterEnabled.load();
    const float thresholdDb = masterCompThresholdDb.load();
    const float ratio = juce::jmax(1.0f, masterCompRatio.load());
    const float makeupDb = masterCompMakeupDb.load();
    const float compAtk = masterDynamicsState.compAttackCoeff;
    const float compRel = masterDynamicsState.compReleaseCoeff;

    const float limiterCeilingGain = juce::Decibels::decibelsToGain(masterLimiterCeilingDb.load());
    const float limiterRelease = masterDynamicsState.limiterReleaseCoeff;

    float compGainDb = masterDynamicsState.compGainDb;
    float limiterGain = masterDynamicsState.limiterGain;
    float compPostPeak = 0.0f;
    float limiterReductionPeak = 0.0f;

    constexpr float kMinLinear = 1.0e-9f;

    for (int i = 0; i < n; ++i)
    {
        const float inL = left[i];
        const float inR = right != nullptr ? right[i] : inL;
        float compGain = 1.0f;
        if (compEnabled)
        {
            const float detector = juce::jmax(std::abs(inL), std::abs(inR));
            const float levelDb = juce::Decibels::gainToDecibels(juce::jmax(detector, kMinLinear), -160.0f);
            const float overDb = levelDb - thresholdDb;
            const float targetCompGainDb = overDb > 0.0f ? -(overDb - (overDb / ratio)) : 0.0f;

            const float compCoeff = targetCompGainDb < compGainDb ? compAtk : compRel;
            compGainDb = compCoeff * compGainDb + (1.0f - compCoeff) * targetCompGainDb;
            compGain = juce::Decibels::decibelsToGain(compGainDb + makeupDb);
        }
        else
        {
            compGainDb = 0.0f;
            compGain = 1.0f;
        }

        float yL = inL * compGain;
        float yR = inR * compGain;

        compPostPeak = juce::jmax(compPostPeak, juce::jmax(std::abs(yL), std::abs(yR)));

        if (limiterEnabled)
        {
            const float postDetector = juce::jmax(std::abs(yL), std::abs(yR));
            const float targetLimiterGain = postDetector > limiterCeilingGain
                                                ? (limiterCeilingGain / juce::jmax(postDetector, kMinLinear))
                                                : 1.0f;

            if (targetLimiterGain < limiterGain)
                limiterGain = targetLimiterGain;
            else
                limiterGain = limiterRelease * limiterGain + (1.0f - limiterRelease) * targetLimiterGain;
        }
        else
        {
            limiterGain = 1.0f;
        }

        left[i] = yL * limiterGain;
        if (right != nullptr)
            right[i] = yR * limiterGain;

        limiterReductionPeak = juce::jmax(limiterReductionPeak, 1.0f - limiterGain);
    }

    masterDynamicsState.compGainDb = compGainDb;
    masterDynamicsState.limiterGain = limiterGain;

    const float compPostDb = juce::Decibels::gainToDecibels(juce::jmax(compPostPeak, 1.0e-9f), -160.0f);
    const float compNorm = juce::jlimit(0.0f, 1.0f, (compPostDb + 54.0f) / 54.0f);

    const float prevComp = masterCompOutputMeter.load();
    const float compSmoothed = juce::jmax(compNorm, prevComp * 0.88f + compNorm * 0.12f);
    masterCompOutputMeter = compSmoothed;

    const float prevLimiter = masterLimiterReductionMeter.load();
    const float limiterSmoothed = limiterReductionPeak > prevLimiter
                                      ? limiterReductionPeak
                                      : (prevLimiter * 0.92f + limiterReductionPeak * 0.08f);
    masterLimiterReductionMeter = juce::jlimit(0.0f, 1.0f, limiterSmoothed);
}

void AudioEngine::updatePlaybackPosition()
{
    if (transportSource == nullptr)
        return;

    // Clamp for display/comparison purposes only — the transport itself
    // keeps advancing (returning silence) past end-of-file while we drain
    // the pitch shifter's backlog below.
    currentPosition = juce::jmin(transportSource->getCurrentPosition(), totalLength.load());

    const bool backlogPending = pitchShifter.getPendingOutputFrames() > 0;

    const auto audibleEnd = audibleEndPosition.load();
    if (playing.load()
        && audibleEnd > 0.0
        && !audibleEndNotified.load()
        && currentPosition.load() >= audibleEnd
        && !backlogPending)
    {
        audibleEndNotified = true;
        if (onAudibleEndReached)
            juce::MessageManager::callAsync([cb = onAudibleEndReached]() { cb(); });
    }

    if (currentPosition >= totalLength.load() && playing.load())
    {
        if (! draining_)
        {
            draining_ = true;
            // At a slow time ratio, RubberBand can still be holding several
            // seconds of unretrieved audio once the input file is fully
            // consumed. Keep the transport running (it emits silence past
            // its own end, which continues to drain the stretcher) until
            // the backlog empties — capped so a numerical edge case in the
            // stretcher can never hang playback indefinitely.
            drainDeadlineMs_ = juce::Time::getMillisecondCounterHiRes() + 8000.0;
        }

        const bool deadlinePassed = juce::Time::getMillisecondCounterHiRes() >= drainDeadlineMs_;
        if (! backlogPending || deadlinePassed)
        {
            draining_ = false;
            stop();
            if (onSongFinished)
                juce::MessageManager::callAsync([cb = onSongFinished]() { cb(); });
        }
    }
    else
    {
        draining_ = false;
    }
}

void AudioEngine::performFrequencyAnalysis(const juce::AudioBuffer<float>& buffer)
{
    // Simple magnitude envelope across frequency bins using a sliding RMS.
    // Replace with juce::dsp::FFT for a proper spectrum analyser.
    const float rms = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
    const int   n   = static_cast<int>(frequencyData.size());

    for (int i = 0; i < n; ++i)
    {
        // Rough approximation: high frequencies decay faster.
        const float freq = static_cast<float>(i) / static_cast<float>(n);
        frequencyData[i] = rms * std::exp(-freq * 3.0f);
    }
}

//==============================================================================
// Device management
//==============================================================================

void AudioEngine::setAudioDevice(const juce::String& deviceName)
{
    auto setup = deviceManager.getAudioDeviceSetup();
    setup.outputDeviceName = deviceName;
    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
        handleAudioDeviceError("setAudioDevice: " + error);
    else
        persistActiveAudioDevice();
}

juce::StringArray AudioEngine::getAvailableAudioDevices() const
{
    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
        return type->getDeviceNames(false);
    return {};
}

juce::String AudioEngine::getCurrentAudioDevice() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getName();
    return {};
}

int AudioEngine::getCurrentBufferSize() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getCurrentBufferSizeSamples();
    return 0;
}

double AudioEngine::getCurrentSampleRate() const
{
    if (auto* dev = deviceManager.getCurrentAudioDevice())
        return dev->getCurrentSampleRate();
    return 0.0;
}

//==============================================================================
void AudioEngine::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
    {
        persistActiveAudioDevice();

        // Re-prepare RubberBand and echo buffer for the new device parameters.
        if (auto* dev = deviceManager.getCurrentAudioDevice())
        {
            const double sr        = dev->getCurrentSampleRate();
            const int    blockSize = dev->getCurrentBufferSizeSamples();

            pitchShifter.prepare(sr, blockSize, 2);
            reverb.setSampleRate(sr);

            echoSampleRate       = sr;
            maxEchoDelaySamples  = static_cast<int>(sr * 2.0);
            echoBuffer.setSize(2, maxEchoDelaySamples, false, true, false);
            echoWritePos = 0;

            masterDynamicsState.sampleRate = sr;
            masterDynamicsState.dirty = true;

            musicBuf_.setSize(2, blockSize, false, false, true);
            vocal1Buf_.setSize(2, blockSize, false, false, true);
            vocal2Buf_.setSize(2, blockSize, false, false, true);
            sfxBuf_.setSize(2, blockSize, false, false, true);

            musicPluginChain_.prepare(sr, blockSize, 2);
            vocal1PluginChain_.prepare(sr, blockSize, 2);
            vocal2PluginChain_.prepare(sr, blockSize, 2);
            fxPluginChain_.prepare(sr, blockSize, 2);
            masterPluginChain_.prepare(sr, blockSize, 2);

            // Re-check mic capture capability — the device may have changed
            // (e.g. via a Settings device picker) to one with or without
            // input channels since AudioEngine last checked.
            const bool wantMicInput = UserPreferences::getInstance().getLiveVocalInputEnabled();
            const bool deviceHasInput = dev->getActiveInputChannels().countNumberOfSetBits() > 0;

            if (wantMicInput && deviceHasInput && ! micCaptureRegistered.load())
            {
                deviceManager.addAudioCallback(&micCapture);
                micCaptureRegistered = true;
                DBG("[AudioEngine] Device change gave mic capture an input-capable device — registered.");
            }
            else if (micCaptureRegistered.load() && (! wantMicInput || ! deviceHasInput))
            {
                deviceManager.removeAudioCallback(&micCapture);
                micCaptureRegistered = false;
                DBG("[AudioEngine] Device change removed input capability — mic capture unregistered.");
            }
        }
    }
}
