/*
  ==============================================================================

    SpectrumAnalyzer.cpp

  ==============================================================================
*/

#include "SpectrumAnalyzer.h"
#include <cmath>

SpectrumAnalyzer::SpectrumAnalyzer() : fft_ (kFftOrder)
{
    for (int i = 0; i < kFftSize; ++i)
        hannWindow_[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (kFftSize - 1));

    for (auto& b : smoothedBands_)
        b.store (0.0f);
}

void SpectrumAnalyzer::rebuildBandRangesIfNeeded (double sampleRate)
{
    if (std::abs (sampleRate - lastSampleRateForBands_) < 0.5)
        return;
    lastSampleRateForBands_ = sampleRate;

    // Log-spaced band edges, 40Hz-16kHz -- covers the musically interesting
    // range for a visual meter without wasting bands on near-silent sub-40Hz
    // rumble or content-free air above 16kHz.
    const double binHz = sampleRate / (double) kFftSize;

    for (int i = 0; i <= kNumBands; ++i)
    {
        const double freq = kLowHz * std::pow (kHighHz / kLowHz, (double) i / (double) kNumBands);
        bandBinEdges_[(size_t) i] = juce::jlimit (0, kFftSize / 2, (int) std::round (freq / binHz));
    }
}

std::array<double, SpectrumAnalyzer::kNumBands> SpectrumAnalyzer::getBandCenterFrequencies()
{
    std::array<double, kNumBands> centers {};
    for (int i = 0; i < kNumBands; ++i)
    {
        const double lo = kLowHz * std::pow (kHighHz / kLowHz, (double) i / (double) kNumBands);
        const double hi = kLowHz * std::pow (kHighHz / kLowHz, (double) (i + 1) / (double) kNumBands);
        centers[(size_t) i] = std::sqrt (lo * hi);
    }
    return centers;
}

void SpectrumAnalyzer::pushSamples (const float* data, int numSamples, double sampleRate)
{
    if (data == nullptr || numSamples <= 0)
        return;

    rebuildBandRangesIfNeeded (sampleRate);

    for (int i = 0; i < numSamples; ++i)
    {
        ringBuffer_[(size_t) ringWritePos_] = data[i];
        ringWritePos_ = (ringWritePos_ + 1) % kFftSize;

        if (++samplesSinceAnalysis_ >= kHopSize)
        {
            samplesSinceAnalysis_ = 0;
            runAnalysis (sampleRate);
        }
    }
}

void SpectrumAnalyzer::runAnalysis (double /*sampleRate*/)
{
    std::array<float, (size_t) kFftSize * 2> fftData {};

    for (int i = 0; i < kFftSize; ++i)
    {
        const int readPos = (ringWritePos_ + i) % kFftSize;
        fftData[(size_t) i] = ringBuffer_[(size_t) readPos] * hannWindow_[(size_t) i];
    }

    fft_.performFrequencyOnlyForwardTransform (fftData.data());

    constexpr float kMinDb = -60.0f;

    for (int band = 0; band < kNumBands; ++band)
    {
        const int lo = bandBinEdges_[(size_t) band];
        const int hi = juce::jmax (lo + 1, bandBinEdges_[(size_t) band + 1]);

        float mag = 0.0f;
        int count = 0;
        for (int b = lo; b < hi; ++b)
        {
            mag += fftData[(size_t) b];
            ++count;
        }
        if (count > 0)
            mag /= (float) count;

        const float ampEstimate = mag / (float) kFftSize;
        const float db = 20.0f * std::log10 (ampEstimate + 1.0e-9f);
        const float normalized = juce::jlimit (0.0f, 1.0f, (db - kMinDb) / (0.0f - kMinDb));

        const float prev = smoothedBands_[(size_t) band].load();
        // Fast attack (LEDs jump up quickly), slower release (settle back
        // down gradually) -- the classic spectrum-analyzer "look", not a
        // raw jittery FFT readout.
        const float coeff = (normalized > prev) ? 0.55f : 0.15f;
        smoothedBands_[(size_t) band].store (prev + (normalized - prev) * coeff);
    }
}

std::array<float, SpectrumAnalyzer::kNumBands> SpectrumAnalyzer::getBandLevels() const
{
    std::array<float, kNumBands> out {};
    for (int i = 0; i < kNumBands; ++i)
        out[(size_t) i] = smoothedBands_[(size_t) i].load();
    return out;
}
