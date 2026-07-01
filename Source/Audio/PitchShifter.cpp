/*
  ==============================================================================

    PitchShifter.cpp

  ==============================================================================
*/

#include "PitchShifter.h"
#include <cmath>
#include <cstring>
#include <algorithm>

//==============================================================================
PitchShifter::PitchShifter() {}

//==============================================================================
void PitchShifter::prepare(double sampleRate, int maxBlockSize, int numChannels)
{
    currentSampleRate    = sampleRate;
    currentMaxBlockSize  = maxBlockSize;
    currentNumChannels   = numChannels;
    rebuild();
}

void PitchShifter::rebuild()
{
    using RBS = RubberBand::RubberBandStretcher;

    // OptionWindowShort reduces latency for real-time use at a small
    // quality cost — appropriate for live karaoke playback.
    stretcher = std::make_unique<RBS>(
        static_cast<size_t>(currentSampleRate),
        static_cast<size_t>(currentNumChannels),
        RBS::OptionProcessRealTime  |
        RBS::OptionPitchHighQuality |
        RBS::OptionWindowShort
    );

    stretcher->setPitchScale(std::pow(2.0, static_cast<double>(pitchSemitones.load()) / 12.0));
    stretcher->setTimeRatio(static_cast<double>(timeRatio.load()));

    // Pre-allocate intermediate buffers.  outBuf is 4× block size to absorb
    // any RubberBand output bursts caused by the time-stretch ratio.
    inBuf.assign(currentNumChannels, std::vector<float>(currentMaxBlockSize, 0.0f));
    outBuf.assign(currentNumChannels, std::vector<float>(currentMaxBlockSize * 4, 0.0f));
    inPtrs.resize(currentNumChannels);
    outPtrs.resize(currentNumChannels);

    for (int ch = 0; ch < currentNumChannels; ++ch)
    {
        inPtrs[ch]  = inBuf[ch].data();
        outPtrs[ch] = outBuf[ch].data();
    }
}

void PitchShifter::reset()
{
    if (stretcher != nullptr)
        rebuild();
}

//==============================================================================
void PitchShifter::setPitchSemitones(float semitones)
{
    semitones = juce::jlimit(-12.0f, 12.0f, semitones);
    pitchSemitones.store(semitones);

    if (stretcher != nullptr)
        stretcher->setPitchScale(std::pow(2.0, static_cast<double>(semitones) / 12.0));
}

void PitchShifter::setTimeRatio(float ratio)
{
    ratio = juce::jlimit(0.5f, 2.0f, ratio);
    timeRatio.store(ratio);

    if (stretcher != nullptr)
        stretcher->setTimeRatio(static_cast<double>(ratio));
}

int PitchShifter::getLatency() const noexcept
{
    return stretcher ? static_cast<int>(stretcher->getLatency()) : 0;
}

//==============================================================================
void PitchShifter::process(juce::AudioBuffer<float>& buffer)
{
    if (stretcher == nullptr)
        return;

    const int numSamples = buffer.getNumSamples();
    const int channels   = juce::jmin(buffer.getNumChannels(), currentNumChannels);

    // Process in chunks no larger than currentMaxBlockSize so inBuf/outBuf
    // (sized once in rebuild()) never need to grow on the audio thread —
    // std::vector::resize() here would be a real-time-unsafe allocation.
    // In the normal case (numSamples <= currentMaxBlockSize, i.e. the block
    // size prepare() was called with) this loop runs exactly once and is
    // numerically identical to a single-shot process/retrieve.
    int samplesDone = 0;
    while (samplesDone < numSamples)
    {
        const int chunk = juce::jmin(numSamples - samplesDone, currentMaxBlockSize);

        for (int ch = 0; ch < channels; ++ch)
        {
            std::memcpy(inBuf[ch].data(),
                        buffer.getReadPointer(ch, samplesDone),
                        static_cast<size_t>(chunk) * sizeof(float));
            inPtrs[ch] = inBuf[ch].data();
        }

        stretcher->process(inPtrs.data(), static_cast<size_t>(chunk), false);

        const int available  = stretcher->available();
        const int toRetrieve  = available > 0 ? std::min(available, chunk) : 0;

        if (toRetrieve > 0)
        {
            for (int ch = 0; ch < channels; ++ch)
                outPtrs[ch] = outBuf[ch].data();

            stretcher->retrieve(outPtrs.data(), static_cast<size_t>(toRetrieve));

            for (int ch = 0; ch < channels; ++ch)
            {
                std::memcpy(buffer.getWritePointer(ch, samplesDone),
                            outBuf[ch].data(),
                            static_cast<size_t>(toRetrieve) * sizeof(float));

                // Zero-fill any shortfall (can happen during latency compensation).
                if (toRetrieve < chunk)
                    buffer.clear(ch, samplesDone + toRetrieve, chunk - toRetrieve);
            }
        }
        else
        {
            // Initial latency fill — output silence until RubberBand has
            // enough input to produce its first output frames.
            for (int ch = 0; ch < channels; ++ch)
                buffer.clear(ch, samplesDone, chunk);
        }

        samplesDone += chunk;
    }

    // Device channels beyond what we pitch-shift (e.g. a >2-channel output
    // device) don't carry processed content — silence them rather than
    // leaving the raw pre-shift audio audible on those channels.
    for (int ch = channels; ch < buffer.getNumChannels(); ++ch)
        buffer.clear(ch, 0, numSamples);
}
