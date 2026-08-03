/*
  ==============================================================================

    WaveformGenerator.h

    Generates a small, normalized peak-per-bucket waveform (values in [0,1])
    from an audio file on a background thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>

class WaveformGenerator
{
public:
    /** Asynchronously read the given audio file and produce `bucketCount` peak
        values normalized to [0, 1]. The callback fires on the JUCE message
        thread. An empty vector is passed if the file couldn't be read. */
    static void generateAsync(const juce::File& audioFile,
                              int bucketCount,
                              std::function<void(std::vector<float>)> onReady);
};
