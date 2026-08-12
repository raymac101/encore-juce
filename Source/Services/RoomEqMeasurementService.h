/*
  ==============================================================================

    RoomEqMeasurementService.h

    Pure DSP for the Room EQ Wizard (Source/UI/RoomEqWizard.h) -- no UI, no
    AudioEngine dependency beyond the recorded buffers it's handed. Two jobs:

    1. generateSweep() -- synthesizes the logarithmic sine sweep played by
       AudioEngine::playAndRecordSweep(). Deterministic, so the sweep's
       instantaneous frequency at any sample index is recoverable from that
       index alone -- no separate "what frequency was playing when" signal
       needs to travel alongside the recording.

    2. computeCorrectionBands() -- turns one or more recorded mic captures
       (one per position the host measured) into a set of cut-only
       parametric bands: per-window FFT magnitude at each window's known
       instantaneous frequency (simpler and more robust than full impulse-
       response deconvolution, at some accuracy cost -- acceptable for
       "flatten the obvious room modes," not audiophile-grade correction),
       generic mic-type compensation, multi-position averaging in the dB
       domain, 1/3-octave smoothing, then peak-only band derivation. Dips
       are deliberately left uncorrected: a null is usually phase
       cancellation, not something a source boost fixes, so leaving them
       alone keeps the auto-applied result safe even if a measurement is a
       little noisy.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include "../Audio/RoomCorrectionEq.h"
#include "UserPreferences.h"

// Deliberately not nested inside RoomEqMeasurementService: a default
// argument on a member function (e.g. `const SweepSpec& = SweepSpec()`)
// needs SweepSpec's default member initializers usable before the
// enclosing class is complete, which a nested definition can't provide.
struct RoomEqSweepSpec
{
    double startHz         = 20.0;
    double endHz           = 20000.0;
    double durationSeconds = 6.0;
};

class RoomEqMeasurementService
{
public:
    enum class MicType { Dynamic, Condenser, Flat };
    using SweepSpec = RoomEqSweepSpec;

    /** Mono logarithmic sine sweep at the given sample rate, `amplitude`
        peak (0-1). A ~3ms edge fade is applied purely to avoid a hard
        click at the very start/end of the buffer -- not a gradual
        level ramp over the sweep's run (the wizard intentionally plays
        the whole sweep at one fixed level). */
    static juce::AudioBuffer<float> generateSweep (double sampleRate,
                                                     const SweepSpec& spec = SweepSpec(),
                                                     float amplitude = 0.6f);

    /** recordedPositions are the raw mono mic captures returned by
        AudioEngine::playAndRecordSweep for each position the host
        measured (minimum 2 is enforced by the wizard UI, not here -- this
        will happily process just one if it's ever called with fewer).
        Returns cut-only bands ready for AudioEngine::setRoomEqBands(),
        already sorted by frequency and capped at RoomCorrectionEq::kMaxBands. */
    static std::vector<RoomEqBand> computeCorrectionBands (
        const std::vector<juce::AudioBuffer<float>>& recordedPositions,
        double sampleRate, MicType micType, const SweepSpec& spec = SweepSpec());

    /** RoomEqBand (Audio/RoomCorrectionEq.h, what the DSP and this service
        deal in) and UserPreferences::RoomEqBandPref (what gets persisted)
        are identical in shape but kept as separate types so Source/Audio
        doesn't need to know about Source/Services. RoomEqWizard/MixerPage/
        MainComponent all move between the two via these. */
    static std::vector<UserPreferences::RoomEqBandPref> toPrefBands (const std::vector<RoomEqBand>& bands);
    static std::vector<RoomEqBand> fromPrefBands (const std::vector<UserPreferences::RoomEqBandPref>& bands);

private:
    static std::vector<double> analysisFrequencies (const SweepSpec& spec);
    static std::vector<float>  extractResponseCurve (const juce::AudioBuffer<float>& recordedMic,
                                                       double sampleRate, const SweepSpec& spec,
                                                       const std::vector<double>& freqs);
    static float micColorationDb (double freqHz, MicType micType);
    static std::vector<float>  smoothOneThirdOctave (const std::vector<float>& curveDb,
                                                       const std::vector<double>& freqs);
    static std::vector<RoomEqBand> deriveBands (const std::vector<float>& smoothedDb,
                                                 const std::vector<double>& freqs);
};
