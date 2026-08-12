/*
  ==============================================================================

    RoomEqMeasurementService.cpp

  ==============================================================================
*/

#include "RoomEqMeasurementService.h"
#include <algorithm>
#include <cmath>
#include <map>

//==============================================================================
juce::AudioBuffer<float> RoomEqMeasurementService::generateSweep (double sampleRate,
                                                                     const SweepSpec& spec,
                                                                     float amplitude)
{
    const int numSamples = juce::jmax (1, (int) std::round (spec.durationSeconds * sampleRate));
    juce::AudioBuffer<float> out (1, numSamples);
    auto* data = out.getWritePointer (0);

    // Exponential ("log") sweep: instantaneous frequency f(t) = f0 * (f1/f0)^(t/T).
    // Integrating gives the closed-form phase below (Farina's sweep) -- this
    // is exact, so analysis can recover "what frequency was playing at
    // sample n" purely from n, with no separate reference signal.
    const double f0 = spec.startHz;
    const double f1 = spec.endHz;
    const double T  = spec.durationSeconds;
    const double R  = std::log (f1 / f0);

    const int fadeSamples = juce::jmax (1, (int) std::round (0.003 * sampleRate));

    for (int n = 0; n < numSamples; ++n)
    {
        const double t   = (double) n / sampleRate;
        const double phi = (2.0 * juce::MathConstants<double>::pi * f0 * T / R) * (std::exp (R * t / T) - 1.0);
        float sample = (float) std::sin (phi);

        if (n < fadeSamples)
            sample *= (float) n / (float) fadeSamples;
        else if (n >= numSamples - fadeSamples)
            sample *= (float) (numSamples - 1 - n) / (float) fadeSamples;

        data[n] = sample * amplitude;
    }

    return out;
}

//==============================================================================
std::vector<double> RoomEqMeasurementService::analysisFrequencies (const SweepSpec& spec)
{
    // ~12 points per octave -- enough resolution for 1/3-octave smoothing
    // downstream to have real neighbors to average, without so many points
    // that the per-point FFT analysis becomes the dominant cost.
    constexpr int pointsPerOctave = 12;
    const double octaves = std::log2 (spec.endHz / spec.startHz);
    const int numPoints = juce::jmax (2, (int) std::round (octaves * pointsPerOctave));

    std::vector<double> freqs;
    freqs.reserve ((size_t) numPoints + 1);
    for (int i = 0; i <= numPoints; ++i)
        freqs.push_back (spec.startHz * std::pow (spec.endHz / spec.startHz, (double) i / numPoints));
    return freqs;
}

std::vector<float> RoomEqMeasurementService::extractResponseCurve (const juce::AudioBuffer<float>& recordedMic,
                                                                      double sampleRate, const SweepSpec& spec,
                                                                      const std::vector<double>& freqs)
{
    const int numSamples = recordedMic.getNumSamples();
    const auto* src = recordedMic.getReadPointer (0);

    const double f0 = spec.startHz;
    const double f1 = spec.endHz;
    const double T  = spec.durationSeconds;
    const double R  = std::log (f1 / f0);

    // FFT objects are expensive-ish to construct; window sizes are
    // quantized to powers of two so the same order gets reused across many
    // neighboring analysis frequencies.
    std::map<int, std::unique_ptr<juce::dsp::FFT>> fftByOrder;
    std::vector<float> curveDb;
    curveDb.reserve (freqs.size());

    for (double f : freqs)
    {
        // Where in the recording this frequency was playing, from the
        // sweep's own deterministic phase function (inverse of f(t) above).
        const double t = T * std::log (f / f0) / R;
        const int centerSample = (int) std::round (t * sampleRate);

        // A handful of cycles at this frequency, clamped to a sane window
        // range -- enough cycles for a clean FFT peak without the window
        // spanning so much time that the sweep's frequency drifts
        // significantly within it.
        constexpr double cyclesTarget = 6.0;
        int windowSamplesRaw = (int) std::round (cyclesTarget * sampleRate / f);
        windowSamplesRaw = juce::jlimit (256, 8192, windowSamplesRaw);
        int order = (int) std::ceil (std::log2 ((double) windowSamplesRaw));
        order = juce::jlimit (8, 13, order);
        const int windowSize = 1 << order;

        int startSample = centerSample - windowSize / 2;
        startSample = juce::jlimit (0, juce::jmax (0, numSamples - windowSize), startSample);

        if (numSamples < windowSize)
        {
            // Recording shorter than the window this frequency wants --
            // shouldn't happen given the tail padding, but stay safe rather
            // than reading out of bounds.
            curveDb.push_back (-120.0f);
            continue;
        }

        auto& fft = fftByOrder[order];
        if (fft == nullptr)
            fft = std::make_unique<juce::dsp::FFT> (order);

        std::vector<float> fftData ((size_t) windowSize * 2, 0.0f);
        for (int i = 0; i < windowSize; ++i)
        {
            // Hann window to keep the FFT peak clean.
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (windowSize - 1));
            fftData[(size_t) i] = src[startSample + i] * w;
        }

        fft->performFrequencyOnlyForwardTransform (fftData.data());

        const double binHz = sampleRate / (double) windowSize;
        const int targetBin = juce::jlimit (0, windowSize / 2, (int) std::round (f / binHz));

        // Sum a couple of neighboring bins too, in case the target
        // frequency falls between bin centers.
        float mag = 0.0f;
        for (int b = juce::jmax (0, targetBin - 1); b <= juce::jmin (windowSize / 2, targetBin + 1); ++b)
            mag += fftData[(size_t) b];

        // Normalizing by windowSize turns the FFT peak into an
        // amplitude-like quantity that's comparable across the differently
        // sized windows used at different frequencies -- only relative
        // values across the curve matter here, not absolute SPL.
        const float ampEstimate = mag / (float) windowSize;
        curveDb.push_back (20.0f * std::log10 (ampEstimate + 1.0e-9f));
    }

    return curveDb;
}

//==============================================================================
float RoomEqMeasurementService::micColorationDb (double freqHz, MicType micType)
{
    // Rough, generic approximations of typical vocal-mic voicing -- not
    // measured per-model, just enough to stop an obvious mic-shape peak
    // (e.g. a handheld dynamic's presence bump) from being mistaken for a
    // room mode. A log-frequency Gaussian bump models the peak; a simple
    // one-pole-shaped curve models the low-end rolloff.
    auto bumpDb = [] (double f, double centerHz, float gainDb, double octaveWidth)
    {
        const double x = std::log2 (f / centerHz) / (octaveWidth * 0.5);
        return gainDb * (float) std::exp (-0.5 * x * x);
    };
    auto lowRolloffDb = [] (double f, double freqHz_, float gainDb)
    {
        return gainDb * (float) (1.0 / (1.0 + (f / freqHz_) * (f / freqHz_)));
    };

    switch (micType)
    {
        case MicType::Dynamic:
            return bumpDb (freqHz, 4500.0, 4.0f, 1.5) + lowRolloffDb (freqHz, 110.0, -3.0f);
        case MicType::Condenser:
            return bumpDb (freqHz, 6000.0, 2.0f, 1.2) + lowRolloffDb (freqHz, 80.0, -1.5f);
        case MicType::Flat:
        default:
            return 0.0f;
    }
}

//==============================================================================
std::vector<float> RoomEqMeasurementService::smoothOneThirdOctave (const std::vector<float>& curveDb,
                                                                      const std::vector<double>& freqs)
{
    std::vector<float> out (curveDb.size());
    constexpr double halfWidthOctaves = (1.0 / 3.0) * 0.5;

    for (size_t i = 0; i < freqs.size(); ++i)
    {
        double sum = 0.0;
        int count = 0;
        for (size_t j = 0; j < freqs.size(); ++j)
        {
            if (std::abs (std::log2 (freqs[j] / freqs[i])) <= halfWidthOctaves)
            {
                sum += curveDb[j];
                ++count;
            }
        }
        out[i] = (float) (count > 0 ? sum / count : curveDb[i]);
    }
    return out;
}

//==============================================================================
std::vector<RoomEqBand> RoomEqMeasurementService::deriveBands (const std::vector<float>& smoothedDb,
                                                                  const std::vector<double>& freqs)
{
    const size_t n = smoothedDb.size();
    if (n == 0)
        return {};

    // A broad (roughly 1-octave) rolling average as the "what's normal
    // here" baseline -- peaks are measured as deviation above this, not
    // above the curve's overall mean, so a PA that's simply tilted bright
    // or dark overall doesn't itself register as one giant "peak."
    std::vector<float> baseline (n);
    constexpr double baselineHalfWidthOctaves = 0.5;
    for (size_t i = 0; i < n; ++i)
    {
        double sum = 0.0;
        int count = 0;
        for (size_t j = 0; j < n; ++j)
        {
            if (std::abs (std::log2 (freqs[j] / freqs[i])) <= baselineHalfWidthOctaves)
            {
                sum += smoothedDb[j];
                ++count;
            }
        }
        baseline[i] = (float) (count > 0 ? sum / count : smoothedDb[i]);
    }

    std::vector<float> deviation (n);
    for (size_t i = 0; i < n; ++i)
        deviation[i] = smoothedDb[i] - baseline[i];

    constexpr float peakThresholdDb = 3.0f;
    constexpr float maxCutDb = -9.0f;
    constexpr double minPeakSeparationOctaves = 0.5;

    struct Candidate { size_t index; float deviationDb; };
    std::vector<Candidate> candidates;

    for (size_t i = 0; i < n; ++i)
    {
        if (deviation[i] < peakThresholdDb)
            continue;
        const bool leftOk  = (i == 0)     || deviation[i] >= deviation[i - 1];
        const bool rightOk = (i == n - 1) || deviation[i] >= deviation[i + 1];
        if (leftOk && rightOk)
            candidates.push_back ({ i, deviation[i] });
    }

    // Strongest first, so when two candidates are too close together the
    // stronger one wins the slot.
    std::sort (candidates.begin(), candidates.end(), [] (const Candidate& a, const Candidate& b)
    {
        return a.deviationDb > b.deviationDb;
    });

    std::vector<RoomEqBand> bands;
    for (auto& c : candidates)
    {
        if ((int) bands.size() >= RoomCorrectionEq::kMaxBands)
            break;

        const double freq = freqs[c.index];
        bool tooClose = false;
        for (auto& b : bands)
            if (std::abs (std::log2 (freq / b.frequencyHz)) < minPeakSeparationOctaves)
            {
                tooClose = true;
                break;
            }
        if (tooClose)
            continue;

        // Width: walk outward in log-frequency until the deviation drops to
        // half the peak's height (a standard -3dB-style bandwidth
        // definition, in the deviation-above-baseline domain rather than
        // raw dB), then convert that octave bandwidth to a filter Q.
        const float halfHeight = c.deviationDb * 0.5f;
        size_t lo = c.index, hi = c.index;
        while (lo > 0 && deviation[lo - 1] >= halfHeight) --lo;
        while (hi < n - 1 && deviation[hi + 1] >= halfHeight) ++hi;
        double bandwidthOctaves = std::log2 (freqs[hi] / freqs[lo]);
        bandwidthOctaves = juce::jlimit (0.1, 4.0, bandwidthOctaves);

        // Standard octave-bandwidth-to-Q relationship for a peak filter.
        const double bw = std::pow (2.0, bandwidthOctaves);
        double q = std::sqrt (bw) / (bw - 1.0);
        q = juce::jlimit (0.5, 8.0, q);

        RoomEqBand band;
        band.frequencyHz = freq;
        band.gainDb       = juce::jmax (maxCutDb, -c.deviationDb);
        band.q             = q;
        bands.push_back (band);
    }

    std::sort (bands.begin(), bands.end(), [] (const RoomEqBand& a, const RoomEqBand& b)
    {
        return a.frequencyHz < b.frequencyHz;
    });

    return bands;
}

//==============================================================================
std::vector<UserPreferences::RoomEqBandPref> RoomEqMeasurementService::toPrefBands (const std::vector<RoomEqBand>& bands)
{
    std::vector<UserPreferences::RoomEqBandPref> out;
    out.reserve (bands.size());
    for (auto& b : bands)
        out.push_back ({ b.frequencyHz, b.gainDb, b.q });
    return out;
}

std::vector<RoomEqBand> RoomEqMeasurementService::fromPrefBands (const std::vector<UserPreferences::RoomEqBandPref>& bands)
{
    std::vector<RoomEqBand> out;
    out.reserve (bands.size());
    for (auto& b : bands)
        out.push_back ({ b.frequencyHz, b.gainDb, b.q });
    return out;
}

//==============================================================================
std::vector<RoomEqBand> RoomEqMeasurementService::computeCorrectionBands (
    const std::vector<juce::AudioBuffer<float>>& recordedPositions,
    double sampleRate, MicType micType, const SweepSpec& spec)
{
    if (recordedPositions.empty())
        return {};

    const auto freqs = analysisFrequencies (spec);

    std::vector<float> averagedDb (freqs.size(), 0.0f);
    int validPositions = 0;

    for (auto& recording : recordedPositions)
    {
        if (recording.getNumSamples() <= 0 || recording.getNumChannels() <= 0)
            continue;

        auto curveDb = extractResponseCurve (recording, sampleRate, spec, freqs);
        for (size_t i = 0; i < freqs.size(); ++i)
            curveDb[i] -= micColorationDb (freqs[i], micType);

        for (size_t i = 0; i < freqs.size(); ++i)
            averagedDb[i] += curveDb[i];
        ++validPositions;
    }

    if (validPositions == 0)
        return {};

    for (auto& v : averagedDb)
        v /= (float) validPositions;

    const auto smoothed = smoothOneThirdOctave (averagedDb, freqs);
    return deriveBands (smoothed, freqs);
}
