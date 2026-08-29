/*
  ==============================================================================

    QueueWriteOutbox.h

    Disk-backed retry queue for live queue writes (add song / remove song).

    Without this, a write that fails while the venue link is down is simply
    lost: QueueService reports ok=false to its callback and nothing else
    happens, so a singer added during an outage silently never makes it into
    the rotation. Modelled directly on PerformanceEventOutbox -- SQLite with
    synchronous=FULL so the write survives an app crash, exponential backoff
    with jitter, and replay driven off a timer.

    Only appendSong/removeSong are handled on purpose. Those are incremental
    read-modify-write operations, so replaying one later re-reads whatever
    the queue looks like at that point and stays correct. The other writes
    (patchSingerSongs, persistSingerOrder, deleteSinger) are "set this doc to
    exactly this snapshot" operations -- replaying one minutes later would
    clobber every change made in the meantime, which is worse than losing it.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <sqlite3.h>

#include "../Models/QueueItem.h"

class QueueWriteOutbox : private juce::Timer
{
public:
    enum class Op
    {
        appendSong,
        removeSong
    };

    static QueueWriteOutbox& getInstance();

    void start();
    void stop();

    /** Persists a queue write that just failed, so it can be replayed once
        the link returns. Returns false if it could not be persisted (in
        which case the write really is lost and the caller should say so). */
    bool enqueue (Op op, const juce::String& venueId, const QueueItem& item);

    int getPendingCount() const;

    /** Fired on the message thread whenever the pending count changes, so
        the UI can show "N queue changes waiting to sync". */
    std::function<void (int pending)> onPendingCountChanged;

private:
    QueueWriteOutbox() = default;
    ~QueueWriteOutbox() override;

    void timerCallback() override;
    bool openIfNeeded();
    void close();
    void flushAsync();
    void deliverNext();
    void notifyPendingCount();

    struct PendingWrite
    {
        juce::String entryId;
        Op           op = Op::appendSong;
        juce::String venueId;
        QueueItem    item;
        int          attempts = 0;
    };

    bool insertEntry (const juce::String& entryId, Op op,
                      const juce::String& venueId, const juce::String& payloadJson);
    bool readNextPending (PendingWrite& write);
    void markDelivered (const juce::String& entryId);
    void markFailed (const PendingWrite& write, const juce::String& error);
    void purgeExpired();

    mutable juce::CriticalSection databaseLock_;
    sqlite3* database_ = nullptr;
    std::atomic<bool> deliveryInFlight_ { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QueueWriteOutbox)
};
