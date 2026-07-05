/*
  ==============================================================================

    AudioEngine.h

    Multi-track karaoke audio engine.

    Signal chain (per audio callback):
        AudioFormatReaderSource
          → AudioTransportSource  (play / pause / seek)
          → ResamplingAudioSource (file SR → device SR)
          → PitchShifter          (RubberBand — pitch + tempo)
          → master gain
          → juce::Reverb          (vocal reverb)
          → echo delay buffer     (vocal echo / slap)
          → output

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PitchShifter.h"
#include "../Services/UserPreferences.h"
#include <array>
#include <atomic>
#include <mutex>

//==============================================================================
class AudioEngine : public juce::AudioSource,
                    public juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    //==========================================================================
    // Lifecycle
    void initialize();
    void shutdown();
    bool isInitialized() const noexcept { return initialized; }

    //==========================================================================
    // Playback
    bool loadSong(const juce::File& audioFile,
                  const juce::File& cdgFile = juce::File{});
    void unloadSong();
    void play();
    void pause();
    void stop();
    void seekToPosition(double positionInSeconds);

    bool   isPlaying()           const noexcept { return playing.load(); }
    bool   isPaused()            const noexcept { return paused.load(); }
    double getCurrentPosition()  const noexcept { return currentPosition.load(); }
    double getTotalLength()      const noexcept { return totalLength.load(); }
    /**
      Estimated end of audible content in seconds. This trims trailing
      silence by scanning backward from file-end at load time.
    */
    double getAudibleEndPosition() const noexcept { return audibleEndPosition.load(); }

    //==========================================================================
    // Pitch and tempo  (thread-safe — may be called from any thread)
    void setPitchShift(float semitones);  // -12 to +12
    void setTempoAdjustment(float ratio); // 0.5 to 2.0
    void setKeyChange(int semitones);     // convenience: calls setPitchShift

    float getPitchShift()  const noexcept { return pitchShifter.getPitchSemitones(); }
    float getTempoRatio()  const noexcept { return pitchShifter.getTimeRatio(); }
    int   getKeyChange()   const noexcept { return keyChangeSemitones.load(); }

    //==========================================================================
    // Volume
    void setMasterVolume(float volume);  // 0.0 – 1.0
    void setMusicVolume(float volume);
    void setVocalVolume(float volume);
    void setVocalEffectsLevel(float level);
    void setSfxVolume(float volume);     // 0.0 – 1.0, live-adjustable (affects a currently-playing SFX too)
    bool triggerOneShotSfx(const juce::File& audioFile);

    float getMasterVolume()      const noexcept { return masterVolume.load(); }
    float getMusicVolume()       const noexcept { return musicVolume.load(); }
    float getVocalVolume()       const noexcept { return vocalVolume.load(); }
    float getVocalEffectsLevel() const noexcept { return vocalEffectsLevel.load(); }
    float getSfxVolume()         const noexcept { return sfxVolume.load(); }

    //==========================================================================
    // Live microphone input (Vocal 1 / Vocal 2) — gated behind
    // UserPreferences::getLiveVocalInputEnabled(). When the feature is
    // disabled, none of this opens input channels or affects the signal
    // chain at all.
    void  setVocal1Gain(float gain);  // 0.0 – 1.0
    void  setVocal2Gain(float gain);
    float getVocal1Gain() const noexcept { return vocal1Gain.load(); }
    float getVocal2Gain() const noexcept { return vocal2Gain.load(); }

    // micIndex is 1 or 2. deviceChannelIndex of -1 means "not mapped" (silent).
    void setMicInputChannel(int micIndex, int deviceChannelIndex);
    int  getMicInputChannel(int micIndex) const;

    /** Names of the current device's available input channels, for a
        settings UI to populate a "Mic 1/2 Input Channel" picker with. Empty
        if no device is open or it has no input channels. */
    juce::StringArray getAvailableInputChannelNames() const;

    /** True once mic capture has actually been registered with the device
        (feature enabled AND the device has at least one input channel). */
    bool isLiveVocalInputActive() const noexcept { return micCaptureRegistered.load(); }

    /** Exposed so a Settings page can embed a juce::AudioDeviceSelectorComponent
        bound to the same device manager AudioEngine itself uses. */
    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }

    //==========================================================================
    // Master EQ (3-band) + insert drive
    void setMasterEqLow(float db);   // -18 to +18 dB
    void setMasterEqMid(float db);   // -18 to +18 dB
    void setMasterEqHigh(float db);  // -18 to +18 dB
    void setMasterInsertDrive(float amount); // 0.0 to 1.0

    float getMasterEqLow() const noexcept { return masterEqLowDb.load(); }
    float getMasterEqMid() const noexcept { return masterEqMidDb.load(); }
    float getMasterEqHigh() const noexcept { return masterEqHighDb.load(); }
    float getMasterInsertDrive() const noexcept { return masterInsertDrive.load(); }

    //==========================================================================
    // Master dynamics (always-on on the master bus)
    void setMasterCompressorThreshold(float db); // -48 to 0 dB
    void setMasterCompressorRatio(float ratio);  // 1 to 20
    void setMasterCompressorAttackMs(float ms);  // 1 to 200 ms
    void setMasterCompressorReleaseMs(float ms); // 10 to 1000 ms
    void setMasterCompressorMakeupDb(float db);  // 0 to 18 dB
    void setMasterCompressorEnabled(bool enabled);
    void setMasterLimiterCeilingDb(float db);    // -12 to -0.1 dB
    void setMasterLimiterReleaseMs(float ms);    // 5 to 500 ms
    void setMasterLimiterEnabled(bool enabled);

    bool  isMasterCompressorEnabled() const noexcept { return masterCompEnabled.load(); }
    bool  isMasterLimiterEnabled() const noexcept { return masterLimiterEnabled.load(); }
    float getMasterCompressorThreshold() const noexcept { return masterCompThresholdDb.load(); }
    float getMasterCompressorRatio() const noexcept { return masterCompRatio.load(); }
    float getMasterCompressorAttackMs() const noexcept { return masterCompAttackMs.load(); }
    float getMasterCompressorReleaseMs() const noexcept { return masterCompReleaseMs.load(); }
    float getMasterCompressorMakeupDb() const noexcept { return masterCompMakeupDb.load(); }
    float getMasterLimiterCeilingDb() const noexcept { return masterLimiterCeilingDb.load(); }
    float getMasterLimiterReleaseMs() const noexcept { return masterLimiterReleaseMs.load(); }

    //==========================================================================
    // Reverb
    void setReverbEnabled(bool enabled);
    void setReverbRoomSize(float size);   // 0.0 – 1.0
    void setReverbLevel(float level);     // 0.0 – 1.0 wet mix

    //==========================================================================
    // Echo / delay
    void setEchoEnabled(bool enabled);
    void setEchoLevel(float level);      // 0.0 – 1.0 feedback level
    void setEchoDelay(float delayMs);    // 50 – 2000 ms

    //==========================================================================
    // Frequency analysis (for waveform/VU meters)
    void enableFrequencyAnalysis(bool enabled);
    const std::vector<float>& getFrequencySpectrum() const { return frequencyData; }
    float getCurrentLevel() const noexcept { return currentAudioLevel.load(); }
    float getMasterCompressorOutputMeter() const noexcept { return masterCompOutputMeter.load(); }
    float getMasterLimiterReductionMeter() const noexcept { return masterLimiterReductionMeter.load(); }

    //==========================================================================
    // Song-end callback — fired on the message thread when playback reaches
    // the end of the file. Wire this in MainComponent to advance the queue
    // (optionally after the countdown delay). Never called on stop()/pause().
    std::function<void()> onSongFinished;

    // Fired once per playback pass when the current position reaches the
    // estimated end of audible content (trimmed trailing silence).
    std::function<void()> onAudibleEndReached;

    //==========================================================================
    // CDG synchronisation callback — fired on the audio thread each block.
    // Signature: void(double positionSeconds, const juce::String& lyricHint)
    void setCDGSyncCallback(std::function<void(double, const juce::String&)> cb);
    bool hasCDGData() const noexcept { return cdgLoaded; }

    //==========================================================================
    // Audio device selection
    void                setAudioDevice(const juce::String& deviceName);
    juce::StringArray   getAvailableAudioDevices() const;
    juce::String        getCurrentAudioDevice()    const;

    //==========================================================================
    // Performance monitoring
    double getCpuUsage()          const noexcept { return cpuUsagePercent.load(); }
    int    getCurrentBufferSize() const;
    double getCurrentSampleRate() const;

    //==========================================================================
    // juce::AudioSource
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;

    // juce::ChangeListener (audio device changes)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    //==========================================================================
    // Live mic capture — a second, capture-only AudioIODeviceCallback
    // registered on the SAME deviceManager as audioSourcePlayer, added
    // *before* it so it captures first within the same audio-thread device
    // callback invocation. It never touches the output buffers at all; it
    // just copies raw input samples into a small buffer that
    // getNextAudioBlock() (still driven entirely by the untouched
    // AudioSourcePlayer path) reads afterwards, on the same thread, within
    // the same callback. This deliberately avoids restructuring
    // AudioSourcePlayer / the existing detach-reattach safety idiom used by
    // seekToPosition/loadSong/unloadSong/initialize/shutdown.
    class MicCaptureCallback : public juce::AudioIODeviceCallback
    {
    public:
        void audioDeviceIOCallbackWithContext(const float* const* inputChannelData,
                                              int numInputChannels,
                                              float* const* /*outputChannelData*/,
                                              int /*numOutputChannels*/,
                                              int numSamples,
                                              const juce::AudioIODeviceCallbackContext&) override
        {
            const int capacity = (int) channelBuffers[0].size();
            const int toCopy = juce::jlimit(0, capacity, numSamples);

            for (int mic = 0; mic < 2; ++mic)
            {
                const int ch = micChannelIndex[mic].load();
                auto* dst = channelBuffers[(size_t) mic].data();

                if (ch >= 0 && ch < numInputChannels && inputChannelData[ch] != nullptr)
                    std::memcpy(dst, inputChannelData[ch], (size_t) toCopy * sizeof(float));
                else
                    std::fill(dst, dst + toCopy, 0.0f);
            }

            capturedSamples = toCopy;
        }

        void audioDeviceAboutToStart(juce::AudioIODevice* device) override
        {
            const int maxBlockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
            for (auto& buf : channelBuffers)
                buf.assign((size_t) juce::jmax(64, maxBlockSize), 0.0f);
            capturedSamples = 0;
        }

        void audioDeviceStopped() override { capturedSamples = 0; }

        // Read from getNextAudioBlock(), same audio-thread invocation.
        const float* getChannelData(int micIndex) const noexcept { return channelBuffers[(size_t) micIndex].data(); }
        int getCapturedSamples() const noexcept { return capturedSamples; }

        // Mapped from the message thread (setMicInputChannel), read on the
        // audio thread — genuinely cross-thread, hence atomic.
        std::atomic<int> micChannelIndex[2] { { -1 }, { -1 } };

    private:
        std::array<std::vector<float>, 2> channelBuffers;
        int capturedSamples = 0;
    };

    MicCaptureCallback micCapture;
    std::atomic<bool>  micCaptureRegistered { false };
    std::atomic<float> vocal1Gain { 0.8f };
    std::atomic<float> vocal2Gain { 0.8f };

    //==========================================================================
    // Audio device
    juce::AudioDeviceManager              deviceManager;
    std::unique_ptr<juce::AudioSourcePlayer> audioSourcePlayer;
    juce::AudioFormatManager              formatManager;
    std::atomic<bool> initialized { false };
    mutable std::mutex lifecycleMutex;

    //==========================================================================
    // Source chain
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::AudioTransportSource>    transportSource;
    std::unique_ptr<juce::ResamplingAudioSource>   resamplingSource;

    //==========================================================================
    // DSP — pitch / tempo
    PitchShifter pitchShifter;

    //==========================================================================
    // DSP — reverb (juce::Reverb lives in juce_audio_basics, no juce_dsp needed)
    juce::Reverb reverb;

    struct ReverbDirtyFlag
    {
        std::atomic<bool> dirty { true };
        std::atomic<float> roomSize  { 0.5f };
        std::atomic<float> wetLevel  { 0.0f };
    } reverbState;

    std::atomic<bool>  reverbEnabled { false };

    //==========================================================================
    // DSP — echo / delay (circular buffer, max 2 s)
    juce::AudioBuffer<float> echoBuffer;
    int   echoWritePos       = 0;
    int   maxEchoDelaySamples = 0;
    double echoSampleRate    = 44100.0;

    std::atomic<bool>  echoEnabled  { false };
    std::atomic<float> echoLevel    { 0.25f };
    std::atomic<float> echoDelayMs  { 250.0f };

    //==========================================================================
    // Playback parameters
    std::atomic<float>  masterVolume       { 0.8f };
    std::atomic<float>  musicVolume        { 0.7f };
    std::atomic<float>  vocalVolume        { 0.8f };
    std::atomic<float>  vocalEffectsLevel  { 0.3f };
    std::atomic<float>  sfxVolume          { 0.85f };
    std::atomic<int>    keyChangeSemitones { 0 };
    std::atomic<float>  masterEqLowDb      { 0.0f };
    std::atomic<float>  masterEqMidDb      { 0.0f };
    std::atomic<float>  masterEqHighDb     { 0.0f };
    std::atomic<float>  masterInsertDrive  { 0.0f };
    std::atomic<float>  masterCompThresholdDb { -18.0f };
    std::atomic<float>  masterCompRatio       { 3.0f };
    std::atomic<float>  masterCompAttackMs    { 18.0f };
    std::atomic<float>  masterCompReleaseMs   { 220.0f };
    std::atomic<float>  masterCompMakeupDb    { 3.0f };
    std::atomic<bool>   masterCompEnabled     { true };
    std::atomic<float>  masterLimiterCeilingDb { -1.0f };
    std::atomic<float>  masterLimiterReleaseMs { 75.0f };
    std::atomic<bool>   masterLimiterEnabled   { true };

    struct MasterEqState
    {
      std::atomic<bool> dirty { true };
      std::array<juce::IIRFilter, 2> lowShelf;
      std::array<juce::IIRFilter, 2> midPeak;
      std::array<juce::IIRFilter, 2> highShelf;
    } masterEqState;

    struct MasterDynamicsState
    {
        std::atomic<bool> dirty { true };
        double sampleRate = 44100.0;
        float compAttackCoeff = 0.0f;
        float compReleaseCoeff = 0.0f;
        float limiterReleaseCoeff = 0.0f;
        float compGainDb = 0.0f;
        float limiterGain = 1.0f;
    } masterDynamicsState;

    //==========================================================================
    // Playback state
    std::atomic<bool>   playing         { false };
    std::atomic<bool>   paused          { false };
    std::atomic<double> currentPosition { 0.0 };
    std::atomic<double> totalLength     { 0.0 };
    std::atomic<double> audibleEndPosition { 0.0 };
    std::atomic<bool> audibleEndNotified { false };

    // At time ratios other than 1.0, the pitch shifter can still be holding
    // buffered output once the input file position reaches totalLength —
    // draining_/drainDeadlineMs_ (audio-thread only) keep playback running
    // until that backlog empties instead of truncating the song early.
    bool   draining_ = false;
    double drainDeadlineMs_ = 0.0;

    //==========================================================================
    // Analysis
    bool               frequencyAnalysisEnabled = false;
    std::vector<float> frequencyData;
    std::atomic<float> currentAudioLevel { 0.0f };
    std::atomic<float> masterCompOutputMeter { 0.0f };
    std::atomic<float> masterLimiterReductionMeter { 0.0f };

    //==========================================================================
    // One-shot SFX overlay (ribbon sound pad)
    std::mutex sfxMutex;
    juce::AudioBuffer<float> oneShotSfxBuffer;
    int oneShotSfxReadPos = 0;
    std::atomic<bool> oneShotSfxActive { false };

    //==========================================================================
    // CDG
    bool cdgLoaded = false;
    std::function<void(double, const juce::String&)> cdgSyncCallback;

    //==========================================================================
    // Performance
    mutable std::atomic<double> cpuUsagePercent { 0.0 };

    //==========================================================================
    // Internal
    bool setupAudioDevice();
    void persistActiveAudioDevice() const;
    void handleAudioDeviceError(const juce::String& message);
    bool mixOneShotSfx(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    bool mixMicInput(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    void applyReverb(juce::AudioBuffer<float>& buffer);
    void applyEcho(juce::AudioBuffer<float>& buffer);
    void applyMasterEq(juce::AudioBuffer<float>& buffer);
    void applyMasterInsert(juce::AudioBuffer<float>& buffer);
    void applyMasterDynamics(juce::AudioBuffer<float>& buffer);
    void updateMasterDynamicsCoefficients();
    void updatePlaybackPosition();
    void performFrequencyAnalysis(const juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
