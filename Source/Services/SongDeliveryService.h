/*
  ==============================================================================

    SongDeliveryService.h

    Lets a company admin push individual song files from their local drive
    out to one or more company venues via Firebase Storage, and lets each
    venue discover and download whatever's waiting for it.

    Storage: Venues/{venueId}/pendingSongs/{songId}.<ext> -- one object per
    song per venue. A raw .cdg pick is auto-paired with its same-basename
    audio companion and zipped before upload (matching LibraryScanner's own
    CDG+audio pairing convention, LibraryScanner.cpp); .zip/.mp4/.m4a picks
    upload as-is.

    Firestore: venues/{venueId}/pendingSongs/{autoId} -- songName,
    artistName, storagePath, uploadedAt, uploadedBy. Deliberately separate
    from the existing "new songs" playlist (VenueService::addSongToNewSongs)
    -- that collection has no file reference and assumes the song is already
    present in the venue's local library, neither of which holds here.

    Local landing spot for a download: a dedicated "CloudSongs" folder under
    this PC's app-data directory, NOT the user's actual scan-indexed library
    folder. downloadAndInstall() places the file there and adds a "new
    songs" entry so it's visible immediately, but does not itself insert it
    into SongDatabase/the searchable library -- that step needs the KJ to
    run "Add Songs" once pointed at the CloudSongs folder (LibraryPage
    already supports an append-only scan that merges into the existing
    library without wiping it). Wiring a fully silent auto-ingest would mean
    writing into SongDatabase from a background service with no live
    two-machine test coverage; staging locally + a manual one-time "Add
    Songs" pass is the safer boundary for now.

    All methods dispatch network/file I/O onto a background juce::Thread and
    invoke callbacks on the JUCE message thread, matching every other
    service in this codebase.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <vector>
#include <functional>

class SongDeliveryService
{
public:
    static SongDeliveryService& getInstance();

    /** Uploads `localFile` to every venue in `venueIds`. `onDone` fires once
        after all uploads finish (or fail): `ok` is true only if every
        upload succeeded; `succeeded`/`total` let the caller report partial
        progress even on failure. */
    using UploadCallback = std::function<void(bool ok, int succeeded, int total, juce::String error)>;
    void uploadSongToVenues(const juce::File& localFile,
                            const std::vector<juce::String>& venueIds,
                            UploadCallback onDone);

    struct PendingSong
    {
        juce::String id;
        juce::String songName;
        juce::String artistName;
        juce::String storagePath;
    };
    using PendingSongsCallback = std::function<void(bool ok, std::vector<PendingSong> songs, juce::String error)>;

    /** Lists undelivered songs waiting for `venueId`. Call once per venue
        load (see MainComponent::setVenueId, alongside the existing
        SongbookStorageService::ensureSongbookExists check). */
    void getPendingSongs(const juce::String& venueId, PendingSongsCallback onDone);

    using WriteCallback = std::function<void(bool ok, juce::String error)>;

    /** Removes a pending-song entry without downloading it (e.g. the KJ
        dismisses the popup). */
    void dismissPendingSong(const juce::String& venueId, const juce::String& songId, WriteCallback onDone = nullptr);

    /** Downloads `song`'s file into this PC's local CloudSongs staging
        folder, registers it in the venue's "new songs" list (existing
        VenueService::addSongToNewSongs) so it's visible immediately, then
        deletes the pending-song doc so the popup doesn't re-offer it. Does
        NOT insert into SongDatabase -- see the class-level comment above. */
    void downloadAndInstall(const juce::String& venueId, const PendingSong& song, WriteCallback onDone);

    /** Where downloaded songs land locally, so UI code can point the KJ at
        it (e.g. "run Add Songs on this folder"). */
    static juce::File getCloudSongsFolder();

private:
    SongDeliveryService() = default;
    JUCE_DECLARE_NON_COPYABLE(SongDeliveryService)
};
