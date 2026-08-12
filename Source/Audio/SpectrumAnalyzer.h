/*
  ==============================================================================

    SpectrumAnalyzer.h

    Real-time FFT-based band analyzer for the TopBar VU meter's "Spectrum
    Band" style (Source/UI/TopBar.cpp) -- continuously running, unlike
    RoomEqMeasurementService's one-shot sweep analysis. Deliberately
    separate from the old `performFrequencyAnalysis` stub in AudioEngine.cpp
    (a fake rms*decay curve, not real per-band data); this one does an
    actual windowed FFT on the master bus.

    Runs entirely on the audio thread (pushSamples(), called once per
    getNextAudioBlock()); reads (getBandLevels()) are safe from any thread
    via a fixed array of atomics, one per band.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>

class SpectrumAnalyzer
{
public:
    static constexpr int kNumBands = 14;
    static constexpr int kFftOrder = 10;              // 1024-sample FFT
    static constexpr int kFftSize  = 1 << kFftOrder;
    static constexpr int kHopSize  = kFftSize / 2;     // 50% overlap between analyses
    static constexpr double kLowHz  = 40.0;
    static constexpr double kHighHz = 16000.0;

    SpectrumAnalyzer();

    /** Audio thread. `data` should already be a mono mix (e.g. average of
        L+R) of whatever signal should drive the display -- the master bus,
        in TopBar's case. */
    void pushSamples (const float* data, int numSamples, double sampleRate);

    /** Any thread. 0..1 per band, already attack/release-smoothed for a
        settled LED-meter look rather than a raw, jittery FFT readout. */
    std::array<float, kNumBands> getBandLevels() const;

    /** Center (geometric mean) frequency of each band, in Hz -- sample-rate
        independent, for UI labeling (TopBar's Spectrum Band style). Kept
        here rather than duplicated in TopBar so the labels can't drift
        from the actual log-spaced band edges used in runAnalysis(). */
    static std::array<double, kNumBands> getBandCenterFrequencies();

private:
    void runAnalysis (double sampleRate);
    void rebuildBandRangesIfNeeded (double sampleRate);

    juce::dsp::FFT fft_;
    std::array<float, kFftSize> hannWindow_ {};
    std::array<float, kFftSize> ringBuffer_ {};
    int ringWritePos_ = 0;
    int samplesSinceAnalysis_ = 0;

    double lastSampleRateForBands_ = 0.0;
    std::array<int, kNumBands + 1> bandBinEdges_ {};

    std::array<std::atomic<float>, kNumBands> smoothedBands_;

    JUCE_DECLARE_NON_COPYABLE (SpectrumAnalyzer)
};
