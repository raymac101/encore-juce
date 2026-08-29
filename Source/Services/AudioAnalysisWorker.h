/*
  ==============================================================================

    AudioAnalysisWorker.h

    One persistent low-priority thread that runs LibraryPage's offline
    tempo/key/duration sweep over a whole song library.

    Replaces the previous design, which launched a fresh juce::Thread AND a
    fresh MessageManager::callAsync per song (plus one more thread per song
    for the shared-analysis upload). Over a 10k-song library that was ~30k
    thread creations and 10k message-thread round trips, which starved the
    UI -- clicks and keystrokes queued behind the analysis traffic -- and
    kept the heap churning.

    Here the thread is created once, results are accumulated on the worker
    side and delivered to the message thread in coalesced batches (see
    kMaxBatchSize / kMinPostIntervalMs), and the shared-analysis upload runs
    inline on this same thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

class AudioAnalysisWorker : private juce::Thread,
                            private juce::AsyncUpdater
{
public:
    /** Deliberately holds only what the worker needs rather than a whole
        CdgSong -- copying 10k full records (7 string vectors each) just to
        hand them to a thread was a large chunk of the old memory spike. */
    struct Job
    {
        size_t       songIndex = 0;   // index into LibraryPage::songs_
        juce::String sourcePath;      // KeyBpmAnalyzer::versionSourcePath, resolved on the message thread
        juce::String artistName;
        juce::String songName;
    };

    struct Result
    {
        size_t       songIndex = 0;
        bool         analysisOk = false;   // false when the file was missing/undecodable
        int          bpm = 0;
        juce::String keySignature;
        int          durationMS = 0;       // 0 when it couldn't be measured
    };

    AudioAnalysisWorker();
    ~AudioAnalysisWorker() override;

    /** Starts (or restarts) a pass. Message thread only. */
    void start (std::vector<Job> jobs);

    /** Stops the pass and joins the thread. Message thread only. */
    void stop();

    void setPaused (bool shouldPause);
    bool isPaused() const noexcept          { return paused_.load(); }
    bool isBusy() const                     { return isThreadRunning(); }

    /** Called on the message thread with every result produced since the
        last call. Never fires with an empty batch. */
    std::function<void (const std::vector<Result>&, int done, int total,
                        const juce::String& currentSong)> onResults;

    /** Called on the message thread once the pass runs out of work. Not
        called when the pass is cancelled via stop(). */
    std::function<void (int done, int total)> onFinished;

private:
    void run() override;
    void handleAsyncUpdate() override;

    static constexpr int kMaxBatchSize      = 25;
    static constexpr int kMinPostIntervalMs = 400;
    static constexpr int kPauseWaitMs       = 200;

    std::vector<Job> jobs_;   // worker-thread only once the thread is running

    std::atomic<bool> paused_ { false };
    juce::WaitableEvent resumeEvent_ { false };

    mutable std::mutex  resultsLock_;
    std::vector<Result> pending_;
    juce::String        currentSong_;

    std::atomic<int>  done_ { 0 };
    std::atomic<int>  total_ { 0 };
    std::atomic<bool> finished_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioAnalysisWorker)
};
