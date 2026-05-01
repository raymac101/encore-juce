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

    //==========================================================================
    // CDG
    bool cdgLoaded = false;
    std::function<void(double, const juce::String&)> cdgSyncCallback;

    //==========================================================================
    // Performance
    mutable std::atomic<double> cpuUsagePercent { 0.0 };

    //==========================================================================
    // Internal
    void setupAudioDevice();
    void handleAudioDeviceError(const juce::String& message);
    void applyReverb(juce::AudioBuffer<float>& buffer);
    void applyEcho(juce::AudioBuffer<float>& buffer);
    void updatePlaybackPosition();
    void performFrequencyAnalysis(const juce::AudioBuffer<float>& buffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
