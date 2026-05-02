/*
  ==============================================================================

    VenueService.h

    Migration of src/app/services/venue.service.ts.

    Sub-collections under venues/{venueId} that this service touches:
      • playing           — Playing record(s) for the song that's up next
      • emojis            — real-time emoji reactions from the mobile app
      • playlists/venueLists/<listName>
            ├── new          (curated "new songs" list, capped at 20)
            ├── Popular
            └── Recommended

    Queue and requested writes live in QueueService and RequestService.
    Venue-user / member admin lives in HostService / a dedicated venue-user
    service (not migrated here).

    Threading
    ─────────
    All public methods that perform network I/O run on a background thread
    via juce::Thread::launch and dispatch their callbacks on the JUCE
    message thread.  Pure utility helpers (generateCode, sortQueueBy*) are
    synchronous.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/VenueItem.h"
#include "../Models/Playing.h"
#include "../Models/Playlist.h"
#include "../Models/Emoji.h"
#include "../Models/Singers.h"
#include "../Models/CdgSong.h"

class VenueService : private juce::Timer
{
public:
    static VenueService& getInstance();

    //==========================================================================
    // Cached "active venue" (the one the host is currently signed in to).
    using LoadCallback = std::function<void(bool ok, VenueItem venue, juce::String error)>;
    using SaveCallback = std::function<void(bool ok, juce::String error)>;
    using WriteCallback = std::function<void(bool ok, juce::String error)>;
    using IntCallback   = std::function<void(int count, juce::String error)>;

    /** Load `venues/<venueId>`. Caches on success. */
    void loadVenue(const juce::String& venueId, LoadCallback onDone);

    VenueItem    getCurrent()        const;
    juce::String getCurrentVenueId() const;

    /** PATCH the editable fields of `venues/<venueId>`. */
    void saveVenue(const juce::String& venueId,
                   const VenueItem& venue,
                   SaveCallback onDone);

    void clear();

    //==========================================================================
    // Venue CRUD (top-level `venues` collection)

    using ListCallback = std::function<void(bool ok, std::vector<VenueItem> list, juce::String error)>;

    /** GET /venues. Returns every venue document. */
    void getVenues(ListCallback onDone);

    /** Create a new venue document. Generates code/codePlus, sets dateTime
        and updateSongs to "now", uses Firestore auto-ID for the new doc.
        Returns the new venueId in the callback. */
    using AddVenueCallback = std::function<void(bool ok, juce::String venueId, juce::String error)>;
    void addVenue(const VenueItem& venue, AddVenueCallback onDone);

    /** Hard-delete `venues/<venueId>` plus the standard subcollections
        (queue, requested, playing, emojis, playlists/.../new/Popular/Recommended). */
    void deleteVenue(const juce::String& venueId, WriteCallback onDone);

    /** Update only the `code` field on a venue. */
    void updateVenueCode(const juce::String& venueId,
                         const juce::String& newCode,
                         WriteCallback onDone);

    /** Re-generate code + codePlus on a venue and persist them. Mirrors the
        Angular addCode() helper. */
    void addCode(const juce::String& venueId, WriteCallback onDone);

    /** Bump the `updateSongs` timestamp on the active venue (signal to
        clients that the songbook has been refreshed). */
    void setLastSongUpdateDate(WriteCallback onDone = nullptr);

    /** Bump the `dateTime` field on the active venue. */
    void updateDate(WriteCallback onDone = nullptr);

    //==========================================================================
    // Code generator (utility — no I/O)
    static juce::String generateCode(int length = 6);

    //==========================================================================
    // Now-playing collection (venues/<id>/playing)

    /** Append a Playing record. If the supplied record has an empty
        singerName the value is replaced with "Unknown Singer".
        Also clears `venues/<id>/emojis` (best effort, errors are ignored). */
    void addCurrentSongPlaying(const Playing& playing, WriteCallback onDone = nullptr);

    using PlayingListCallback =
        std::function<void(bool ok, std::vector<Playing> list, juce::String error)>;

    /** One-shot read of all Playing docs for the active venue. */
    void getCurrentSinger(PlayingListCallback onDone);

    /** Live watcher (3 s polling) — onChange fires whenever the list
        contents differ from the previous poll. Replaces any previous
        watcher. */
    using PlayingChangeCallback = std::function<void(std::vector<Playing>)>;
    void startWatchingPlaying(PlayingChangeCallback onChange);
    void stopWatchingPlaying();

    /** Delete a single playing doc by id. Uses the active venue. */
    void removeCurrentSongPlaying(const juce::String& playingDocId,
                                  WriteCallback onDone = nullptr);

    /** Delete every doc under venues/<active>/playing. */
    void removeAllCurrentSongPlaying(WriteCallback onDone = nullptr);

    //==========================================================================
    // Emojis (venues/<id>/emojis)

    using EmojiListCallback =
        std::function<void(bool ok, std::vector<Emoji> list, juce::String error)>;

    /** One-shot read of all emoji docs for a venue. */
    void getNewEmoji(const juce::String& venueId, EmojiListCallback onDone);

    /** Delete a specific emoji by id. */
    void deleteEmoji(const juce::String& venueId,
                     const juce::String& emojiId,
                     WriteCallback onDone = nullptr);

    //==========================================================================
    // Playlists (venues/<id>/playlists/venueLists/<listName>)
    //
    // Reserved listName "new" is the curated "new songs" feed (capped at 20
    // entries, ordered by dateAdded desc). All other listNames (e.g.
    // "Popular", "Recommended") are unbounded.

    /** Add a song to one or more playlists. Each list write creates a
        new auto-ID Playlist document. */
    void addSongToLists(const CdgSong& song,
                        const juce::StringArray& listNames,
                        WriteCallback onDone = nullptr);

    using PlaylistListCallback =
        std::function<void(bool ok, std::vector<Playlist> list, juce::String error)>;

    /** Read a playlist by name. */
    void getPlaylists(const juce::String& venueId,
                      const juce::String& listName,
                      PlaylistListCallback onDone);

    /** Read the curated "new songs" feed — up to 20 entries, newest first. */
    void getNewSongs(const juce::String& venueId, PlaylistListCallback onDone);

    /** Add a song to the "new songs" list (or refresh its metadata if it
        already exists). Caps the list at 20 entries (oldest dropped). */
    void addSongToNewSongs(const CdgSong& song, WriteCallback onDone = nullptr);

    /** Update the metadata of an existing song in the "new songs" list,
        preserving its dateAdded. No-op if the song isn't there. */
    void updateSongInNewSongs(const CdgSong& song, WriteCallback onDone = nullptr);

    /** Remove a single song id from the "new songs" list. */
    void deleteSongFromNewSongs(const juce::String& songId, WriteCallback onDone = nullptr);

    /** Remove a single song id from any named playlist. */
    void deleteSongFromPlaylist(const juce::String& listName,
                                const juce::String& songId,
                                WriteCallback onDone = nullptr);

    /** Remove duplicates and entries with empty ids from the "new" list.
        Returns the number of docs deleted via onDone. */
    void cleanupOrphanedNewSongs(IntCallback onDone = nullptr);

    /** Same dedupe + empty-id sweep, applied to a named playlist. */
    void cleanupPlaylistSongs(const juce::String& listName,
                              IntCallback onDone = nullptr);

    /** Run cleanup on every standard playlist (new / Popular / Recommended). */
    void cleanupAllPlaylists(IntCallback onDone = nullptr);

    //==========================================================================
    // Queue / requested cleanup (these subcollections are also written by
    // QueueService and RequestService — placed here to mirror the Angular
    // service surface).

    /** Hard-delete every doc under venues/<id>/queue. */
    void deleteAllSingersFromQueue(const juce::String& venueId,
                                   WriteCallback onDone = nullptr);

    /** Hard-delete every doc under venues/<id>/requested. */
    void deleteAllSongsFromRequested(const juce::String& venueId,
                                     WriteCallback onDone = nullptr);

    //==========================================================================
    // Existing-session check — counts queue and requested docs so the host
    // can decide whether to keep, clear, or archive on app start.

    struct SessionCounts
    {
        int queueCount     = 0;
        int requestedCount = 0;
    };
    using SessionCountsCallback =
        std::function<void(bool ok, SessionCounts counts, juce::String error)>;

    void checkExistingSessionData(const juce::String& venueId,
                                  SessionCountsCallback onDone);

    //==========================================================================
    // Play history (venues/<id>/playHistory — last 20 songs played > 30 s)

    struct PlayHistoryEntry
    {
        std::string songId;
        std::string songName;
        std::string artistName;
        std::string imageUrl;
        std::string singerName;
    };

    /** Write one play-history entry for the active venue (best-effort, no
        retry). Trims the collection to 20 docs after each write, keeping the
        most recent ones. */
    void addPlayHistory(const PlayHistoryEntry& entry, WriteCallback onDone = nullptr);

    /** Read up to 20 most-recent play-history docs for `venueId`, sorted
        newest-first, and return them as Playlist objects so they can be fed
        directly into HomePage::setRecentlyPlayedFromHistory(). */
    void getRecentlyPlayed(const juce::String& venueId, PlaylistListCallback onDone);

    //==========================================================================
    // Sort helpers (utility — no I/O). Operate in-place on the supplied vector.
    static void sortQueueByName (std::vector<Singers>& q);
    static void sortQueueByOrder(std::vector<Singers>& q);

private:
    VenueService() = default;

    // juce::Timer — drives the playing watcher.
    void timerCallback() override;
    void pollPlayingWatcher();

    mutable juce::CriticalSection lock_;
    VenueItem    current_;
    juce::String currentId_;

    // Playing watcher state.
    bool                  watchingPlaying_  = false;
    bool                  playingInFlight_  = false;
    juce::String          playingFingerprint_;
    PlayingChangeCallback onPlayingChange_;
    int                   watchIntervalMs_  = 3000;

    JUCE_DECLARE_NON_COPYABLE(VenueService)
};
