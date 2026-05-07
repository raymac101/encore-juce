/*
  ==============================================================================

    AudioEngine.cpp

  ==============================================================================
*/

#include "AudioEngine.h"
#include <cmath>

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

    if (! setupAudioDevice())
        return;

    audioSourcePlayer = std::make_unique<juce::AudioSourcePlayer>();
    deviceManager.addAudioCallback(audioSourcePlayer.get());
    audioSourcePlayer->setSource(this);

    initialized = true;
    persistActiveAudioDevice();
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

    auto error = deviceManager.initialise(0, 2, nullptr, true, {}, preferredSetupPtr);
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
    masterCompOutputMeter = 0.0f;
    masterLimiterReductionMeter = 0.0f;
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

    // 3. Apply per-channel style gains (current engine has one program source,
    // so we treat vocal volume as a trim influencing FX return feel).
    const float gain = masterVolume.load() * musicVolume.load();
    buf.applyGain(gain);

    // 4. Master EQ.
    applyMasterEq(buf);

    // 5. Reverb.
    applyReverb(buf);

    // 6. Echo.
    applyEcho(buf);

    // 7. Insert saturation on the post-FX bus.
    applyMasterInsert(buf);

    // 8. Always-on master compressor + limiter for song-to-song consistency.
    applyMasterDynamics(buf);

    // 9. Update position and VU level.
    updatePlaybackPosition();

    if (frequencyAnalysisEnabled)
        performFrequencyAnalysis(buf);

    currentAudioLevel = buf.getRMSLevel(0, bufferToFill.startSample, bufferToFill.numSamples);

    // 10. CDG sync callback.
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
        }
    }
}
