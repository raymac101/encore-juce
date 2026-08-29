/*
  ==============================================================================

    AudioAnalysisWorker.cpp

  ==============================================================================
*/

#include "AudioAnalysisWorker.h"
#include "ApiService.h"
#include "../Audio/KeyBpmAnalyzer.h"

#include <cmath>

//==============================================================================
AudioAnalysisWorker::AudioAnalysisWorker()
    : juce::Thread ("Encore Audio Analysis")
{
}

AudioAnalysisWorker::~AudioAnalysisWorker()
{
    stop();
}

//==============================================================================
void AudioAnalysisWorker::start (std::vector<Job> jobs)
{
    stop();

    if (jobs.empty())
        return;

    jobs_ = std::move (jobs);
    total_.store ((int) jobs_.size());
    done_.store (0);
    finished_.store (false);
    paused_.store (false);
    resumeEvent_.reset();

    startThread (juce::Thread::Priority::low);
}

void AudioAnalysisWorker::stop()
{
    // Release the pause wait first, or stopThread just burns its timeout.
    paused_.store (false);
    resumeEvent_.signal();

    stopThread (5000);
    cancelPendingUpdate();

    std::lock_guard<std::mutex> lock (resultsLock_);
    pending_.clear();
    currentSong_.clear();
}

void AudioAnalysisWorker::setPaused (bool shouldPause)
{
    paused_.store (shouldPause);
    if (! shouldPause)
        resumeEvent_.signal();
}

//==============================================================================
void AudioAnalysisWorker::run()
{
    // Built once for the whole pass -- the old per-song version re-registered
    // every audio format (and its associated allocations) 10k times.
    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    juce::uint32 lastPostMs = juce::Time::getMillisecondCounter();

    for (size_t i = 0; i < jobs_.size(); ++i)
    {
        while (paused_.load() && ! threadShouldExit())
            resumeEvent_.wait (kPauseWaitMs);

        if (threadShouldExit())
            break;

        const auto& job = jobs_[i];

        {
            std::lock_guard<std::mutex> lock (resultsLock_);
            currentSong_ = job.artistName + " - " + job.songName;
        }

        Result result;
        result.songIndex = job.songIndex;

        juce::File tempFile;
        const auto audioFile = KeyBpmAnalyzer::resolvePlayableAudioFile (juce::File (job.sourcePath), tempFile);

        if (audioFile.existsAsFile())
        {
            const auto analysis = KeyBpmAnalyzer::analyze (audioFile);
            result.analysisOk   = analysis.ok;
            result.bpm          = analysis.bpm;
            result.keySignature = analysis.keySignature;

            // Real duration from the same resolved file the analysis above
            // already paid the zip-extraction cost for -- karaoke edits
            // routinely trim/extend vs. the commercial track, so this is
            // preferred over Spotify's/the catalog's durationMS.
            std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));
            if (reader != nullptr && reader->sampleRate > 0.0 && reader->lengthInSamples > 0)
                result.durationMS = (int) std::round ((double) reader->lengthInSamples / reader->sampleRate * 1000.0);
        }

        if (tempFile.existsAsFile())
            tempFile.deleteFile();

        if (threadShouldExit())
            break;

        // Share with the Firebase master list -- most venues use the same
        // handful of karaoke vendors, so one venue's real analysis saves
        // every other one from redoing the same work. Synchronous on this
        // thread on purpose (see ApiService::submitLocalAudioAnalysisSync).
        if (result.analysisOk || result.durationMS > 0)
            ApiService::getInstance().submitLocalAudioAnalysisSync (
                job.artistName, job.songName,
                result.analysisOk ? (double) result.bpm : 0.0,
                result.keySignature, result.durationMS);

        size_t pendingCount = 0;
        {
            std::lock_guard<std::mutex> lock (resultsLock_);
            pending_.push_back (std::move (result));
            pendingCount = pending_.size();
        }

        done_.store ((int) i + 1);

        const auto nowMs = juce::Time::getMillisecondCounter();
        if (pendingCount >= (size_t) kMaxBatchSize
            || nowMs - lastPostMs >= (juce::uint32) kMinPostIntervalMs)
        {
            lastPostMs = nowMs;
            triggerAsyncUpdate();
        }
    }

    if (! threadShouldExit())
    {
        finished_.store (true);
        triggerAsyncUpdate();
    }
}

void AudioAnalysisWorker::handleAsyncUpdate()
{
    std::vector<Result> batch;
    juce::String current;

    {
        std::lock_guard<std::mutex> lock (resultsLock_);
        batch.swap (pending_);
        current = currentSong_;
    }

    if (! batch.empty() && onResults != nullptr)
        onResults (batch, done_.load(), total_.load(), current);

    if (finished_.load() && onFinished != nullptr)
        onFinished (done_.load(), total_.load());
}
