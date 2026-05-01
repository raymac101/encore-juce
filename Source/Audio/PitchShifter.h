/*
  ==============================================================================

    PitchShifter.h

    Real-time pitch shifting and time stretching using the Rubber Band Library.
    Wraps RubberBandStretcher for use inside AudioEngine::getNextAudioBlock.

    Dependency: Rubber Band Library (https://breakfastquay.com/rubberband/)
    Install:  brew install rubberband   (macOS)
              vcpkg install rubberband  (Windows)

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <rubberband/RubberBandStretcher.h>
#include <vector>
#include <memory>
#include <atomic>

//==============================================================================
/**
    Wraps RubberBandStretcher as an in-place AudioBuffer processor.

    Usage:
        // On the message thread (device setup)
        pitchShifter.prepare(sampleRate, maxBlockSize, numChannels);

        // On the audio thread
        pitchShifter.process(audioBuffer);

    Parameter changes (setPitchSemitones, setTimeRatio) are thread-safe —
    they may be called from any thread and take effect immediately.
*/
class PitchShifter
{
public:
    PitchShifter();
    ~PitchShifter() = default;

    //==========================================================================
    // Call once before the audio callback starts, and again if the device
    // setup changes (sample rate / buffer size).
    void prepare(double sampleRate, int maxBlockSize, int numChannels);

    // Reset internal RubberBand state (flushes latency buffer).
    void reset();

    //==========================================================================
    // Process a buffer in-place.  The buffer is modified to hold the
    // pitch/tempo-shifted output.  During the initial latency fill period
    // the buffer is filled with silence.
    void process(juce::AudioBuffer<float>& buffer);

    //==========================================================================
    // Pitch shift in semitones [-12, +12].  Thread-safe.
    void  setPitchSemitones(float semitones);
    float getPitchSemitones() const noexcept { return pitchSemitones.load(); }

    // Time-stretch ratio [0.5, 2.0]  (1.0 = no change).  Thread-safe.
    void  setTimeRatio(float ratio);
    float getTimeRatio() const noexcept { return timeRatio.load(); }

    // Latency in samples introduced by RubberBand (updates after prepare()).
    int  getLatency() const noexcept;
    bool isReady()    const noexcept { return stretcher != nullptr; }

private:
    //==========================================================================
    std::unique_ptr<RubberBand::RubberBandStretcher> stretcher;

    double currentSampleRate = 44100.0;
    int    currentNumChannels = 2;
    int    currentMaxBlockSize = 512;

    std::atomic<float> pitchSemitones { 0.0f };
    std::atomic<float> timeRatio      { 1.0f };

    // Intermediate buffers — RubberBand requires non-const input pointers
    // and we need stable storage across the callback.
    std::vector<std::vector<float>> inBuf;
    std::vector<std::vector<float>> outBuf;
    std::vector<const float*>       inPtrs;
    std::vector<float*>             outPtrs;

    void rebuild();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PitchShifter)
};
