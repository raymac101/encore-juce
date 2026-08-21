#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <sqlite3.h>

#include "../Models/CdgSong.h"
#include "../Models/QueueItem.h"
#include "../Models/Singers.h"

class PerformanceEventOutbox : private juce::Timer
{
public:
    static PerformanceEventOutbox& getInstance();

    void start();
    void stop();

    /** Persists the complete event before any network delivery is attempted. */
    bool enqueuePerformance(const CdgSong& song,
                            const Singers& singer,
                            const QueueItem& item,
                            const juce::String& venueId,
                            juce::int64 startedAtMs,
                            juce::int64 endedAtMs,
                            bool naturalEnd);

    int getPendingCount() const;

private:
    PerformanceEventOutbox() = default;
    ~PerformanceEventOutbox() override;

    void timerCallback() override;
    bool openIfNeeded();
    void close();
    bool insertEvent(const juce::String& eventId, const juce::String& payloadJson);
    void flushAsync();
    void deliverNext();

    struct PendingEvent
    {
        juce::String eventId;
        juce::String payloadJson;
        int attempts = 0;
    };

    bool readNextPending(PendingEvent& event);
    void markDelivered(const juce::String& eventId);
    void markFailed(const PendingEvent& event, const juce::String& error);

    mutable juce::CriticalSection databaseLock_;
    sqlite3* database_ = nullptr;
    std::atomic<bool> deliveryInFlight_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PerformanceEventOutbox)
};