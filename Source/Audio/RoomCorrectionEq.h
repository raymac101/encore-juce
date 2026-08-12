/*
  ==============================================================================

    RoomCorrectionEq.h

    Up to 10 parametric (peak) filter bands applied to the true final master
    bus (see AudioEngine::getNextAudioBlock() -- inserted after Music/Vocal1/
    Vocal2/SFX are summed, before the master plugin chain), so a correction
    derived by the Room EQ Wizard (Source/UI/RoomEqWizard.h) affects
    everything hitting the PA -- backing tracks and live mic together --
    not just the music channel. That's a deliberate distinction from the
    existing 3-band "Master EQ" (AudioEngine::applyMasterEq), which is
    scoped to the music channel only.

    Same dirty-flag-rebuild-on-the-audio-thread convention as
    AudioEngine::MasterEqState: setBands()/setEnabled() are called from the
    message thread, process() runs on the audio thread and only rebuilds
    filter coefficients when something actually changed.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

/** frequencyHz/q describe the peak filter's center and width; gainDb is
    negative for the cuts the wizard derives (a positive value would boost,
    which nothing in this codebase currently generates automatically -- see
    RoomEqMeasurementService's "cuts only" rationale). */
struct RoomEqBand
{
    double frequencyHz = 1000.0;
    float  gainDb       = 0.0f;
    double q             = 1.4;
};

class RoomCorrectionEq
{
public:
    static constexpr int kMaxBands = 10;

    RoomCorrectionEq() = default;

    /** Message thread. Silently truncates to kMaxBands if more are passed. */
    void setBands (const std::vector<RoomEqBand>& bands);

    /** Message thread. */
    void setEnabled (bool enabled) noexcept { enabled_.store (enabled); }
    bool isEnabled() const noexcept { return enabled_.load(); }

    /** Audio thread. No-op (buffer untouched) when disabled or when there
        are no bands -- callers don't need to check isEnabled() themselves
        first. */
    void process (juce::AudioBuffer<float>& buffer, double sampleRate);

private:
    struct BandState
    {
        std::array<juce::IIRFilter, 2> filters; // index by channel, max 2 (L/R)
        double frequencyHz = 1000.0;
        float  gainDb       = 0.0f;
        double q             = 1.4;
    };

    void rebuildIfNeeded (double sampleRate);

    juce::CriticalSection lock_;
    std::vector<RoomEqBand> pendingBands_;
    std::atomic<bool> bandsDirty_ { false };
    std::atomic<bool> enabled_ { false };

    std::array<BandState, kMaxBands> bandStates_;
    int activeBandCount_ = 0;
    double lastSampleRate_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE (RoomCorrectionEq)
};
