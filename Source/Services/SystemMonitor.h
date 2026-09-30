/*
  ==============================================================================

    SystemMonitor.h

    Lightweight, poll-on-demand process CPU% and memory sampler -- backs the
    optional performance-stats readout in TopBar (see UserPreferences::
    getShowPerformanceStats()), added to help diagnose an Encore instance
    that's gone sluggish or needs periodic restarts on underpowered venue
    hardware, without the KJ having to alt-tab to Activity Monitor/Task
    Manager mid-show.

    Deliberately NOT a juce::Thread or Timer of its own: every sample() call
    is just a couple of cheap OS syscalls (no I/O, no allocation of note), so
    it's safe to call directly from TopBar's existing 60fps timer at
    whatever throttled rate TopBar chooses (it samples at ~1Hz -- there's no
    value in CPU%/memory refreshing faster than a human can read it, and
    this deliberately doesn't add a second always-running timer to a bar
    that's now specifically about *not* wasting cycles).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class SystemMonitor
{
public:
    static SystemMonitor& getInstance();

    struct Sample
    {
        /** This process's CPU usage since the previous sample() call,
            normalised to 0-100% of total machine capacity (i.e. all cores
            pegged == 100, not numCores*100) -- matches the scale a KJ
            skimming the top bar expects, rather than Activity Monitor's
            per-core-additive convention. -1 if unavailable on this
            platform/first call. */
        float cpuPercent = -1.0f;

        /** This process's current resident memory in megabytes, or -1 if
            unavailable. */
        double memoryMB = -1.0;
    };

    /** Cheap: a couple of OS syscalls, no allocation. Safe to call from the
        message thread. The first call after construction (or after a long
        gap) returns cpuPercent = -1 since there's no previous sample to
        diff against -- callers should treat that as "still warming up"
        rather than "0% busy". */
    Sample sample();

private:
    SystemMonitor() = default;

    // Previous sample's process CPU time (user+system, seconds) and the
    // wall-clock time it was taken at, so the next call can compute
    // (deltaCpuTime / deltaWallTime) as a percentage. 0 until the first
    // real sample.
    double   lastProcessCpuSeconds_ = 0.0;
    juce::int64 lastWallTimeMs_ = 0;
    bool     haveLastSample_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SystemMonitor)
};
