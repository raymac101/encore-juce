/*
  ==============================================================================

    RoomCorrectionEq.cpp

  ==============================================================================
*/

#include "RoomCorrectionEq.h"

void RoomCorrectionEq::setBands (const std::vector<RoomEqBand>& bands)
{
    const juce::ScopedLock sl (lock_);
    pendingBands_ = bands;
    if ((int) pendingBands_.size() > kMaxBands)
        pendingBands_.resize ((size_t) kMaxBands);
    bandsDirty_.store (true);
}

void RoomCorrectionEq::rebuildIfNeeded (double sampleRate)
{
    const bool sampleRateChanged = std::abs (sampleRate - lastSampleRate_) > 0.5;
    if (! bandsDirty_.exchange (false) && ! sampleRateChanged)
        return;

    std::vector<RoomEqBand> bands;
    {
        const juce::ScopedLock sl (lock_);
        bands = pendingBands_;
    }

    lastSampleRate_ = sampleRate;
    activeBandCount_ = juce::jmin ((int) bands.size(), kMaxBands);

    for (int i = 0; i < activeBandCount_; ++i)
    {
        auto& band = bandStates_[(size_t) i];
        band.frequencyHz = bands[(size_t) i].frequencyHz;
        band.gainDb       = bands[(size_t) i].gainDb;
        band.q             = bands[(size_t) i].q;

        const auto gainAsRatio = juce::Decibels::decibelsToGain (band.gainDb);
        const auto coeffs = juce::IIRCoefficients::makePeakFilter (
            sampleRate, juce::jlimit (20.0, sampleRate * 0.45, band.frequencyHz),
            juce::jmax (0.1, band.q), gainAsRatio);

        for (auto& f : band.filters)
            f.setCoefficients (coeffs);
    }
}

void RoomCorrectionEq::process (juce::AudioBuffer<float>& buffer, double sampleRate)
{
    rebuildIfNeeded (sampleRate);

    if (! enabled_.load() || activeBandCount_ <= 0)
        return;

    const int channels = juce::jmin (2, buffer.getNumChannels());
    const int n = buffer.getNumSamples();

    for (int band = 0; band < activeBandCount_; ++band)
    {
        auto& state = bandStates_[(size_t) band];
        for (int ch = 0; ch < channels; ++ch)
            state.filters[(size_t) ch].processSamples (buffer.getWritePointer (ch), n);
    }
}
