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
    void play();
    void pause();
    void stop();
    void seekToPosition(double positionInSeconds);

    bool   isPlaying()           const noexcept { return playing.load(); }
    bool   isPaused()            const noexcept { return paused.load(); }
    double getCurrentPosition()  const noexcept { return currentPosition.load(); }
    double getTotalLength()      const noexcept { return totalLength.load(); }

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

    float getMasterVolume()      const noexcept { return masterVolume.load(); }
    float getMusicVolume()       const noexcept { return musicVolume.load(); }
    float getVocalVolume()       const noexcept { return vocalVolume.load(); }
    float getVocalEffectsLevel() const noexcept { return vocalEffectsLevel.load(); }

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
    // Audio device
    juce::AudioDeviceManager              deviceManager;
    std::unique_ptr<juce::AudioSourcePlayer> audioSourcePlayer;
    juce::AudioFormatManager              formatManager;
    bool initialized = false;

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

    //==========================================================================
    // Analysis
    bool               frequencyAnalysisEnabled = false;
    std::vector<float> frequencyData;
    std::atomic<float> currentAudioLevel { 0.0f };
    std::atomic<float> masterCompOutputMeter { 0.0f };
    std::atomic<float> masterLimiterReductionMeter { 0.0f };

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
