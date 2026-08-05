/*
  ==============================================================================

    SongbookStorageService.h

    Keeps a per-venue copy of songbook.json in Firebase Storage (path
    "venues/{venueId}/songbook.json") up to date. The TAGG mobile app reads
    that file directly to know what a venue can sing -- if it's missing
    (e.g. a brand-new venue that was never scanned from this PC), TAGG shows
    an error when a customer connects.

    All methods dispatch their network work onto a background juce::Thread
    and always invoke callbacks back on the JUCE message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class SongbookStorageService
{
public:
    static SongbookStorageService& getInstance();

    /** Checks Storage for "venues/{venueId}/songbook.json". If it's already
        there, calls onDone(true, false, {}) immediately (well, after the
        network round trip). If it's missing and this PC has a locally
        scanned songbook.json (from any prior library scan), uploads it and
        calls onDone(true, true, {}). If it's missing AND there is no local
        songbook to fall back on, calls onDone(false, false, {}) so the
        caller can kick off an initial library scan instead. */
    void ensureSongbookExists(const juce::String& venueId,
                               std::function<void(bool exists, bool justUploaded, juce::String error)> onDone);

    /** Uploads the current local songbook.json (from LibraryScanner's
        default app-data location) to "venues/{venueId}/songbook.json" in
        Storage, overwriting whatever's there. Called after every completed
        library scan so Storage stays current for the active venue, and as
        the fallback path inside ensureSongbookExists(). */
    void uploadLocalSongbook(const juce::String& venueId,
                              std::function<void(bool ok, juce::String error)> onDone);

    /** Seeds "venues/{venueId}/songbook.json" with an empty song list ("[]")
        right after a venue is created, so its Storage folder/object exists
        from the very first moment -- even before anyone has scanned a
        library for it. Naturally overwritten the first time a real scan
        completes (LibraryPage uploads its result after every scan). */
    void seedEmptySongbook(const juce::String& venueId,
                            std::function<void(bool ok, juce::String error)> onDone = nullptr);

    /** Compares this PC's local songbook.json (LibraryScanner's default
        app-data location) byte-for-byte against "venues/{venueId}/
        songbook.json" in Storage. TAGG reads the Storage copy directly, so
        any drift (e.g. a library scan done on a different PC, or a scan
        that failed to upload) means TAGG is offering songs this PC's queue
        doesn't actually have, or vice versa.

        Calls onDone(true, {}) if they match OR if there's nothing local to
        compare (nothing to flag) OR if the check itself couldn't complete
        (network down, etc. -- fails safe, same philosophy as the rest of
        this service: never surface a false "out of sync" from a connectivity
        hiccup). Calls onDone(false, {}) only when both files were
        successfully read and their contents differ. */
    void checkSongbookInSync(const juce::String& venueId,
                              std::function<void(bool inSync, juce::String error)> onDone);

private:
    SongbookStorageService() = default;
    ~SongbookStorageService() = default;

    static juce::String storageObjectPath(const juce::String& venueId);

    JUCE_DECLARE_NON_COPYABLE(SongbookStorageService)
};
