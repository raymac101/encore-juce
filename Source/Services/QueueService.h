/*
  ==============================================================================

    QueueService.h

    Loads the live queue for a venue from Firestore at
    `venues/<venueId>/queue` and groups the QueueItems into Singers ready for
    the QueueBar. Mirrors the Angular QueueItem schema (singerName, song,
    artist, avatar, profileId, status, order, songOrder, rotationOrder, ...).

    All network calls run on a background thread; callbacks fire on the
    message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/QueueItem.h"
#include "../Models/Singers.h"
#include <vector>

class QueueService : private juce::Timer
{
public:
    static QueueService& getInstance();

    struct Snapshot
    {
        std::vector<Singers> singers;     // upcoming rotation
        Singers              nowPlaying;  // valid only if hasNowPlaying
        bool                 hasNowPlaying = false;
    };

    using LoadCallback = std::function<void(bool ok, Snapshot snap, juce::String error)>;

    /** Asynchronously load `venues/<venueId>/queue`. Callback runs on the
        message thread. */
    void loadQueue(const juce::String& venueId, LoadCallback onDone);

    using WriteCallback = std::function<void(bool ok, juce::String error)>;

    /** Append `item` to the end of the matching singer's `songs` array under
        `venues/<venueId>/queue`, finding the singer by case-insensitive
        `singerName`. If no singer matches, a brand-new singer document is
        created using canonical queue-doc IDs:
        - auth singers use `profileId` (Firebase auth UID)
        - manual singers use deterministic namespaced `manual-*` IDs.
        Network I/O runs on a background thread; callback fires on the
        message thread. */
    void appendSong(const juce::String& venueId,
                    const QueueItem& item,
                    WriteCallback onDone = nullptr);

    /** Remove a song from the matching singer's `songs` array. Match
        precedence: songId, then (song + artist + singerName). If the singer
        ends up with no songs left, the singer doc is left in place (matches
        the Angular behaviour). */
    void removeSong(const juce::String& venueId,
                    const QueueItem& item,
                    WriteCallback onDone = nullptr);

    /** Delete the singer document under `venues/<venueId>/queue` matching
        `singerNameOrDocId` -- tries an exact Firestore doc ID match first
        (auth singers, e.g. `MainComponent::onRemoveSinger`), then falls
        back to a case-insensitive display-name match (e.g. the strikes-out
        path in `queueAndLoadNextSingerSong`). */
    void deleteSinger(const juce::String& venueId,
                      const juce::String& singerNameOrDocId,
                      WriteCallback onDone = nullptr);

    /** PATCH a singer's `songs` array on `venues/<venueId>/queue` to exactly
        `newSongs` (matched by case-insensitive `singerName`). The order of
        the supplied songs is the order written to Firestore — songOrder
        values are renumbered 0..N-1 before write. Used by the
        edit-singer-modal dialog to commit reorder / pitch / delete edits. */
    void patchSingerSongs(const juce::String& venueId,
                          const juce::String& singerName,
                          const std::vector<QueueItem>& newSongs,
                          WriteCallback onDone = nullptr);

    /** Persist the provided singer sequence as canonical queue order for
        `venues/<venueId>/queue`. Writes both `order` and `rotationOrder`
        for each matched singer doc so restarts/mobile clients restore the
        exact same round-robin state. */
    void persistSingerOrder(const juce::String& venueId,
                            const std::vector<Singers>& orderedSingers,
                            WriteCallback onDone = nullptr);

    /** Ensure the host has a permanent Firestore doc at
        `venues/<venueId>/queue/<authUid>`.  Creates it with `order = 0`
        and an empty songs array if the doc doesn't already exist; no-ops
        silently if it does.  Should be called once after the queue loads
        so the host slot is always backed by a real document.
        Network I/O runs on a background thread; callback fires on the
        message thread. */
    void ensureHostQueueDoc(const juce::String& venueId,
                            const juce::String& authUid,
                            const juce::String& stageName,
                            const juce::String& avatarUrl,
                            WriteCallback onDone = nullptr);

    //==============================================================================
    // Live watcher — polls `venues/<venueId>/queue` on a timer and fires
    // onChange (with the freshly-parsed Snapshot) whenever the contents
    // differ from the previous poll. Used to pick up reorders / status
    // changes pushed by the TAGG mobile app or other desktop clients.
    using ChangeCallback = std::function<void(Snapshot snap)>;

    /** Start polling /queue for the given venue. Calling start with a new
        venueId implicitly stops any previous polling. */
    void startWatching(const juce::String& venueId, ChangeCallback onChange);

    /** Stop polling. */
    void stopWatching();

    /** Configure the watcher cadence (default 3000 ms). */
    void setWatchIntervalMs(int ms) noexcept { watchIntervalMs_ = juce::jmax(500, ms); }

    /** Fired on the message thread when the watcher transitions between
        healthy (poll succeeding) and unhealthy (kUnhealthyFailureThreshold
        consecutive failed polls -- a real outage, not a single blip). Set
        once at startup; replaced rather than added. */
    std::function<void(bool healthy)> onConnectionHealthChanged;

    /** Immediately clear in-flight/failure state and restart the watcher
        for the current venue, even though startWatching() would otherwise
        just swap the callback because we're already "watching" it. Used by
        the UI's manual "Reconnect Now" action so the user isn't stuck
        waiting out the request watchdog in FirestoreClient. Safe to call
        from the message thread; no-ops if nothing is being watched yet. */
    void forceReconnect();

private:
    QueueService() = default;

    void timerCallback() override;
    void pollWatcher();

    // Message-thread-only in-flight write counter. A write's background
    // thread can take several seconds (persistSingerOrder in particular
    // PATCHes one singer doc at a time), and the watcher polls on a fixed
    // 3s timer regardless -- without this, a poll can land mid-write and
    // read a half-old/half-new queue, which momentarily undoes whatever
    // optimistic local update the write was made to confirm (e.g. Rotate
    // visibly reverting for a moment before "correcting" itself). Both
    // beginWrite() (called synchronously from each public write method)
    // and endWrite() (called via callAsync from the write's background
    // thread once it finishes) only ever run on the message thread, same
    // as pollWatcher() itself, so this needs no atomics/locking.
    void beginWrite() noexcept { ++pendingWrites_; }
    void endWrite();
    int               pendingWrites_ = 0;

    // RAII scope guard for the above: construct at the top of a write's
    // background-thread lambda, and its destructor marks the write done
    // regardless of which of the lambda's several return points is taken.
    struct WriteGuard
    {
        explicit WriteGuard (QueueService* s) : svc (s) {}
        ~WriteGuard() { juce::MessageManager::callAsync ([s = svc] { s->endWrite(); }); }
        QueueService* svc;
    };

    juce::String      watchVenueId_;
    bool              watching_ = false;
    bool              watchInFlight_ = false;
    int               watchIntervalMs_ = 3000;
    juce::String      lastFingerprint_;
    ChangeCallback    onChange_;

    static constexpr int kUnhealthyFailureThreshold = 3;
    int               consecutiveFailures_ = 0;
    bool              reportedUnhealthy_   = false;

    // Serializes read-modify-write sequences (list -> mutate -> PATCH/POST)
    // across appendSong/removeSong/patchSingerSongs/persistSingerOrder/
    // ensureHostQueueDoc, each of which runs on its own background thread.
    // Without this, two near-simultaneous writes can both read the same
    // stale songs[] snapshot and the second PATCH silently clobbers the
    // first (lost update).
    juce::CriticalSection writeLock_;

    JUCE_DECLARE_NON_COPYABLE(QueueService)
};
