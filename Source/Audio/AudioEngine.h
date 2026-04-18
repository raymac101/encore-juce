/*
  ==============================================================================

    AudioEngine.h
    Created: 15 Apr 2026 7:10:32pm
    Author:  GitHub Copilot

    Professional audio engine for karaoke system with real-time processing

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/QueueItem.h"

//==============================================================================
/** 
    Professional audio engine handling multi-track karaoke playback.
    Supports real-time pitch shifting, tempo adjustment, and CDG synchronization.
*/
class AudioEngine : public juce::AudioSource,
                   public juce::ChangeListener
{
public:
    //==============================================================================
    AudioEngine();
    ~AudioEngine() override;
    
    //==============================================================================
    // Audio Device Management
    void initialize();
    void shutdown();
    bool isInitialized() const { return initialized; }
    
    //==============================================================================
    // Playback Control
    bool loadSong(const juce::File& audioFile, const juce::File& cdgFile = juce::File{});
    void play();
    void pause();
    void stop();
    void seekToPosition(double positionInSeconds);
    
    bool isPlaying() const { return playing; }
    bool isPaused() const { return paused; }
    double getCurrentPosition() const { return currentPosition; }
    double getTotalLength() const { return totalLength; }
    
    //==============================================================================
    // Real-time Audio Processing
    void setPitchShift(float semitones); // -12 to +12 semitones
    void setTempoAdjustment(float ratio); // 0.5 to 2.0 (50% to 200% speed)
    void setKeyChange(int semitones); // Musical key adjustment
    
    float getPitchShift() const { return pitchShiftSemitones; }
    float getTempoRatio() const { return tempoRatio; }
    int getKeyChange() const { return keyChangeSemitones; }
    
    //==============================================================================
    // Audio Levels and Mixing
    void setMasterVolume(float volume); // 0.0 to 1.0
    void setMusicVolume(float volume);  // Background music level
    void setVocalVolume(float volume);  // Microphone level
    void setVocalEffectsLevel(float level); // Reverb, echo, etc.
    
    float getMasterVolume() const { return masterVolume; }
    float getMusicVolume() const { return musicVolume; }
    float getVocalVolume() const { return vocalVolume; }
    float getVocalEffectsLevel() const { return vocalEffectsLevel; }
    
    //==============================================================================
    // Vocal Effects
    void setReverbEnabled(bool enabled);
    void setReverbLevel(float level); // 0.0 to 1.0
    void setEchoEnabled(bool enabled);
    void setEchoLevel(float level); // 0.0 to 1.0
    void setEchoDelay(float delayMs); // Echo delay in milliseconds
    
    //==============================================================================
    // Audio Analysis (for visualizations)
    void enableFrequencyAnalysis(bool enabled);
    const std::vector<float>& getFrequencySpectrum() const { return frequencyData; }
    float getCurrentLevel() const { return currentAudioLevel; }
    
    //==============================================================================
    // CDG/Lyric Synchronization
    void setCDGSyncCallback(std::function<void(double, const juce::String&)> callback);
    bool hasCDGData() const { return cdgLoaded; }
    
    //==============================================================================
    // Audio Device Selection
    void setAudioDevice(const juce::String& deviceName);
    juce::StringArray getAvailableAudioDevices() const;
    juce::String getCurrentAudioDevice() const;
    
    //==============================================================================
    // Performance Monitoring
    double getCpuUsage() const;
    int getCurrentBufferSize() const;
    double getCurrentSampleRate() const;
    
    //==============================================================================
    // AudioSource Interface
    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    
    //==============================================================================
    // ChangeListener Interface (for device changes)
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;

private:
    //==============================================================================
    // Audio Device and Setup
    juce::AudioDeviceManager deviceManager;
    std::unique_ptr<juce::AudioSourcePlayer> audioSourcePlayer;
    bool initialized = false;
    
    //==============================================================================
    // Audio Sources
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<juce::AudioTransportSource> transportSource;
    std::unique_ptr<juce::ResamplingAudioSource> resamplingSource;
    
    // Audio format manager for loading files
    juce::AudioFormatManager formatManager;
    
    //==============================================================================
    // Real-time Processing Chain (Commented out for initial build)
    // std::unique_ptr<juce::dsp::ProcessorChain<
    //     juce::dsp::Gain<float>,          // Master gain
    //     juce::dsp::Reverb,              // Reverb effect
    //     juce::dsp::DelayLine<float>     // Echo effect
    // >> processingChain;
    
    //==============================================================================
    // Audio Parameters
    std::atomic<float> masterVolume { 0.8f };
    std::atomic<float> musicVolume { 0.7f };
    std::atomic<float> vocalVolume { 0.8f };
    std::atomic<float> vocalEffectsLevel { 0.3f };
    
    std::atomic<float> pitchShiftSemitones { 0.0f };
    std::atomic<float> tempoRatio { 1.0f };
    std::atomic<int> keyChangeSemitones { 0 };
    
    //==============================================================================
    // Effect Parameters
    std::atomic<bool> reverbEnabled { false };
    std::atomic<float> reverbLevel { 0.3f };
    std::atomic<bool> echoEnabled { false };
    std::atomic<float> echoLevel { 0.2f };
    std::atomic<float> echoDelayMs { 250.0f };
    
    //==============================================================================
    // Playback State
    std::atomic<bool> playing { false };
    std::atomic<bool> paused { false };
    std::atomic<double> currentPosition { 0.0 };
    std::atomic<double> totalLength { 0.0 };
    
    //==============================================================================
    // Analysis and Monitoring
    bool frequencyAnalysisEnabled = false;
    std::vector<float> frequencyData;
    std::atomic<float> currentAudioLevel { 0.0f };
    // juce::dsp::FFT fftProcessor { 10 }; // 1024 point FFT - Commented out for initial build
    
    //==============================================================================
    // CDG Data
    bool cdgLoaded = false;
    std::function<void(double, const juce::String&)> cdgSyncCallback;
    
    //==============================================================================
    // Performance Monitoring
    mutable std::atomic<double> cpuUsagePercent { 0.0 };
    
    //==============================================================================
    // Internal Methods
    void updatePlaybackPosition();
    void processAudioEffects(juce::AudioBuffer<float>& buffer);
    void performFrequencyAnalysis(const juce::AudioBuffer<float>& buffer);
    void updateCDGSync();
    void calculateCpuUsage();
    
    // Audio device setup
    void setupAudioDevice();
    void handleAudioDeviceError(const juce::String& errorMessage);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AudioEngine)
};