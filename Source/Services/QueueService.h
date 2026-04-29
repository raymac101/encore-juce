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
        created using the item's singerName, avatar and profileId. Network
        I/O runs on a background thread; callback fires on the message
        thread. */
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

    /** PATCH a singer's `songs` array on `venues/<venueId>/queue` to exactly
        `newSongs` (matched by case-insensitive `singerName`). The order of
        the supplied songs is the order written to Firestore — songOrder
        values are renumbered 0..N-1 before write. Used by the
        edit-singer-modal dialog to commit reorder / pitch / delete edits. */
    void patchSingerSongs(const juce::String& venueId,
                          const juce::String& singerName,
                          const std::vector<QueueItem>& newSongs,
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

private:
    QueueService() = default;

    void timerCallback() override;
    void pollWatcher();

    juce::String      watchVenueId_;
    bool              watching_ = false;
    bool              watchInFlight_ = false;
    int               watchIntervalMs_ = 3000;
    juce::String      lastFingerprint_;
    ChangeCallback    onChange_;

    JUCE_DECLARE_NON_COPYABLE(QueueService)
};
