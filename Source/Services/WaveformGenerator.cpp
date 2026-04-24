/*
  ==============================================================================

    WaveformGenerator.cpp

  ==============================================================================
*/

#include "WaveformGenerator.h"

namespace
{
    std::vector<float> computePeaks(const juce::File& file, int bucketCount)
    {
        std::vector<float> peaks;
        if (bucketCount <= 0 || ! file.existsAsFile())
            return peaks;

        juce::AudioFormatManager fmt;
        fmt.registerBasicFormats();

        std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(file));
        if (reader == nullptr)
            return peaks;

        const juce::int64 totalSamples = reader->lengthInSamples;
        if (totalSamples <= 0)
            return peaks;

        peaks.assign((size_t) bucketCount, 0.0f);
        const juce::int64 samplesPerBucket = juce::jmax<juce::int64>(1, totalSamples / bucketCount);

        constexpr int kBlock = 16384;
        juce::AudioBuffer<float> buffer((int) reader->numChannels, kBlock);

        float globalMax = 0.0001f;

        for (int b = 0; b < bucketCount; ++b)
        {
            juce::int64 startSample = (juce::int64) b * samplesPerBucket;
            juce::int64 endSample   = juce::jmin(totalSamples, startSample + samplesPerBucket);
            float bucketMax = 0.0f;

            juce::int64 pos = startSample;
            while (pos < endSample)
            {
                int toRead = (int) juce::jmin<juce::int64>(kBlock, endSample - pos);
                if (! reader->read(&buffer, 0, toRead, pos, true, reader->numChannels > 1))
                    break;

                for (int ch = 0; ch < (int) reader->numChannels; ++ch)
                {
                    auto* d = buffer.getReadPointer(ch);
                    for (int i = 0; i < toRead; ++i)
                    {
                        float v = std::abs(d[i]);
                        if (v > bucketMax) bucketMax = v;
                    }
                }
                pos += toRead;
            }

            peaks[(size_t) b] = bucketMax;
            if (bucketMax > globalMax) globalMax = bucketMax;
        }

        // Normalize to [0, 1] with a gentle floor so bars are visible even on
        // quiet songs.
        for (auto& p : peaks)
            p = juce::jlimit(0.05f, 1.0f, p / globalMax);

        return peaks;
    }
}

//==============================================================================
void WaveformGenerator::generateAsync(const juce::File& audioFile,
                                      int bucketCount,
                                      std::function<void(std::vector<float>)> onReady)
{
    juce::Thread::launch([audioFile, bucketCount, onReady = std::move(onReady)]() mutable {
        auto peaks = computePeaks(audioFile, bucketCount);
        juce::MessageManager::callAsync([peaks = std::move(peaks), onReady = std::move(onReady)]() mutable {
            if (onReady) onReady(std::move(peaks));
        });
    });
}
