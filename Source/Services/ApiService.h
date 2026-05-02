/*
  ==============================================================================

    ApiService.h

    Migration of src/app/services/api.service.ts.

    Talks to the TAGG Cloud Functions backend at
        https://us-central1-tagg-9ee2b.cloudfunctions.net/app/
    using a fixed bearer token ("KaraokeWorld") to enrich CdgSong records
    with Spotify-style metadata (album art, duration, tempo, key, genres,
    release date, artist/song-name corrections, popularity).

    Layered behaviour mirrors the Angular service:
       1. searchArtistAndSong() first checks the local shared_metadata.json
          cache (keyed by normalised "artist|song").
       2. On a miss, calls the cloud function over HTTPS, parses the
          Spotify-shaped response, persists the result to shared_metadata.json
          and returns the enriched CdgSong.

    All public calls are asynchronous: HTTP I/O runs on a background thread
    via juce::Thread::launch and the completion callback fires on the JUCE
    message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/CdgSong.h"

class ApiService
{
public:
    static ApiService& getInstance();

    /** Result delivered to searchArtistAndSong's callback. */
    struct Result
    {
        bool         ok = false;
        bool         fromCache = false;        // true when served from shared_metadata.json
        CdgSong      song;                     // updated record (only valid when ok)
        juce::String errorMessage;             // human-readable when !ok
    };

    using Callback = std::function<void(Result)>;

    /** Mirror of Angular searchArtistAndSong(currentSong, artist, song).
        - Trims and normalises both terms (strips vendor codes via fixBrackets,
          drops non-alphanumerics, applies AC/DC special case).
        - Checks the shared metadata cache, then falls back to the cloud
          function. Saves a successful API result back to the cache.
        - Callback fires on the message thread.
        Pass empty `artist` or `song` to short-circuit with ok=false. */
    void searchArtistAndSong(const CdgSong& currentSong,
                             const juce::String& artist,
                             const juce::String& song,
                             Callback onDone);

    //==========================================================================
    // Helpers — exposed for testing and reuse from other code paths.

    /** Strip vendor disc-code suffixes like "(KK)", "(Sc)", "(Sfg)", "(1)" etc.
        Returns the cleaned string (single replacement, mirrors Angular). */
    static juce::String fixBrackets(const juce::String& s);

    /** Convert Spotify-style "<key>:<mode>" (e.g. "9:0") to the human-readable
        key signature used in the songbook (e.g. "A minor"). Empty input
        returns empty. */
    static juce::String getKeySignature(const juce::String& spotifyKeyMode);

    /** Normalise an artist/song pair into the cache key used by
        shared_metadata.json. Lowercase, alphanumerics + spaces only. */
    static juce::String makeCacheKey(const juce::String& artist,
                                     const juce::String& song);

    //==========================================================================
    // Shared metadata cache (shared_metadata.json) — exposed so the rest of
    // the app can prime / read it directly when convenient.

    /** Returns the shared cache file path. Created lazily on first save. */
    juce::File getSharedMetadataFile() const;

    /** Look up a cached metadata blob by normalised key. Returns an empty
        var on miss. Synchronous file I/O — call from a background thread. */
    juce::var getSharedMetadata(const juce::String& artist,
                                const juce::String& song);

    /** Insert / update a cached entry. Synchronous file I/O — call from a
        background thread. Returns true on successful write. */
    bool saveSharedMetadata(const CdgSong& song);

    //==========================================================================
    // Configuration — override in tests if needed.
    void setApiBaseUrl(const juce::String& url)         noexcept { taggApiUrl_ = url; }
    void setBearerToken(const juce::String& token)      noexcept { bearerToken_ = token; }
    void setRequestTimeoutMs(int ms)                    noexcept { timeoutMs_ = juce::jmax(1000, ms); }

private:
    ApiService();
    ~ApiService() = default;

    Result doSpotifyApiCall(const CdgSong& currentSong,
                            const juce::String& artist,
                            const juce::String& song);

    Result tryCachedLookup(const CdgSong& currentSong,
                           const juce::String& artist,
                           const juce::String& song);

    juce::String taggApiUrl_  = "https://us-central1-tagg-9ee2b.cloudfunctions.net/app/";
    juce::String bearerToken_ = "KaraokeWorld";
    int          timeoutMs_   = 15000;

    juce::CriticalSection cacheLock_;

    JUCE_DECLARE_NON_COPYABLE(ApiService)
};
