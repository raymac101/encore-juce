/*
  ==============================================================================

    AudioEngine.cpp

  ==============================================================================
*/

#include "AudioEngine.h"

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
    if (initialized)
        return;

    setupAudioDevice();

    audioSourcePlayer = std::make_unique<juce::AudioSourcePlayer>();
    deviceManager.addAudioCallback(audioSourcePlayer.get());
    audioSourcePlayer->setSource(this);

    initialized = true;
}

void AudioEngine::shutdown()
{
    if (!initialized)
        return;

    stop();

    if (audioSourcePlayer != nullptr)
    {
        audioSourcePlayer->setSource(nullptr);
        deviceManager.removeAudioCallback(audioSourcePlayer.get());
        audioSourcePlayer.reset();
    }

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

void AudioEngine::setupAudioDevice()
{
    auto error = deviceManager.initialise(0, 2, nullptr, true);

    if (error.isNotEmpty())
    {
        handleAudioDeviceError("Failed to initialise audio device: " + error);
        return;
    }

    deviceManager.addChangeListener(this);
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
    currentPosition = 0.0;

    cdgLoaded = cdgFile.exists();

    // Reset echo buffer write position so residual feedback from the previous
    // song does not bleed into the new one.
    echoWritePos = 0;
    echoBuffer.clear();

    if (audioSourcePlayer != nullptr)
        audioSourcePlayer->setSource(this);

    DBG("loadSong: loaded " + audioFile.getFileName()
        + "  (" + juce::String(totalLength.load(), 1) + " s)");
    return true;
}

void AudioEngine::play()
{
    if (transportSource == nullptr)
        return;

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
}

void AudioEngine::seekToPosition(double positionInSeconds)
{
    if (transportSource == nullptr)
        return;

    positionInSeconds = juce::jlimit(0.0, totalLength.load(), positionInSeconds);
    transportSource->setPosition(positionInSeconds);
    currentPosition = positionInSeconds;

    // Flush RubberBand so we don't hear the pre-seek audio at the new position.
    pitchShifter.reset();
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
}

void AudioEngine::releaseResources()
{
    if (resamplingSource != nullptr)
        resamplingSource->releaseResources();
}

void AudioEngine::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (resamplingSource == nullptr || !playing)
        return;

    // 1. Pull from the source chain.
    resamplingSource->getNextAudioBlock(bufferToFill);

    juce::AudioBuffer<float>& buf = *bufferToFill.buffer;

    // 2. Pitch shift + time stretch via RubberBand.
    pitchShifter.process(buf);

    // 3. Apply master + music volume.
    const float gain = masterVolume.load() * musicVolume.load();
    buf.applyGain(gain);

    // 4. Reverb.
    applyReverb(buf);

    // 5. Echo.
    applyEcho(buf);

    // 6. Update position and VU level.
    updatePlaybackPosition();

    if (frequencyAnalysisEnabled)
        performFrequencyAnalysis(buf);

    currentAudioLevel = buf.getRMSLevel(0, bufferToFill.startSample, bufferToFill.numSamples);

    // 7. CDG sync callback.
    if (cdgLoaded && cdgSyncCallback)
        cdgSyncCallback(currentPosition.load(), {});
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

void AudioEngine::updatePlaybackPosition()
{
    if (transportSource == nullptr)
        return;

    currentPosition = transportSource->getCurrentPosition();

    if (currentPosition >= totalLength.load() && playing.load())
    {
        stop();
        if (onSongFinished)
            juce::MessageManager::callAsync([cb = onSongFinished]() { cb(); });
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
        }
    }
}
