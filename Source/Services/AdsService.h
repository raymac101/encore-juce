/*
  ==============================================================================

    AdsService.h

    Single source of truth for a venue's lyric-screen ads: merges the
    Storage file listing (Venues/{venueId}/Ads/) with its Firestore
    scheduling/rotation metadata (venues/{venueId}/Ads/). Used by both
    AdsPage (the management UI) and LyricDisplayComponent (idle-screen
    playback), so the two never diverge on what an "ad" is.

    All methods dispatch their network work onto a background juce::Thread
    and invoke callbacks back on the JUCE message thread, except
    fetchActiveAdsSync(), which is synchronous by design -- the idle screen
    already runs its ad refresh on its own background thread and expects a
    plain return value (see the old fetchVenueAds() this replaces).

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <vector>
#include "../Models/AdMetadata.h"

class AdsService
{
public:
    static AdsService& getInstance();

    using ListCallback  = std::function<void (bool ok, std::vector<AdMetadata> ads)>;
    using WriteCallback = std::function<void (bool ok, juce::String error)>;

    /** Full unfiltered listing (Storage-only files included, expired/future
        ads included) -- used by the management page's grid. */
    void listAllAds (const juce::String& venueId, ListCallback onDone);

    /** Merged listing filtered to ads active right now. Caller MUST already
        be on a background thread -- mirrors the old fetchVenueAds()
        contract that LyricDisplayComponent's refresh timer relies on. */
    static std::vector<AdMetadata> fetchActiveAdsSync (const juce::String& venueId);

    /** Uploads localFile to Venues/{venueId}/Ads/{sanitizeAdDocId(fileName)}
        and writes its metadata doc at venues/{venueId}/Ads/{sameId}. If an
        ad with that id already exists, both the file and the doc are
        overwritten. */
    void uploadAd (const juce::String& venueId, const juce::File& localFile,
                   AdMetadata meta, WriteCallback onDone);

    /** Updates only the Firestore metadata doc for an existing ad. */
    void updateAdMetadata (const juce::String& venueId, const juce::String& docId,
                           AdMetadata meta, WriteCallback onDone);

    /** Removes both the Storage object and the Firestore metadata doc. */
    void deleteAd (const juce::String& venueId, const juce::String& docId, WriteCallback onDone);

    /** Firestore document IDs can't contain "/" and can't be "." or "..";
        this also caps length as a defensive measure. Storage object names
        have no such restriction, but AdsService always uses this same
        sanitized value as BOTH the Storage file name and the Firestore doc
        id, so the two never need a separate lookup. */
    static juce::String sanitizeAdDocId (const juce::String& fileName);

private:
    AdsService() = default;
    ~AdsService() = default;

    /** Synchronous merge of Storage + Firestore for one venue. Caller must
        already be on a background thread. */
    static std::vector<AdMetadata> mergeVenueAds (const juce::String& venueId);

    JUCE_DECLARE_NON_COPYABLE (AdsService)
};
