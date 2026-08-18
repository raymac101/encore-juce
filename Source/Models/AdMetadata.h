/*
  ==============================================================================

    AdMetadata.h

    A venue's lyric-screen ad: one uploaded Storage file (Venues/{venueId}/
    Ads/{docId}) plus its scheduling/rotation metadata (venues/{venueId}/Ads/
    {docId} in Firestore). docId is always the sanitized Storage file name --
    see AdsService::sanitizeAdDocId() -- so the two are always addressable
    from a single id with no separate lookup table.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

struct AdMetadata
{
    juce::String docId;      // Sanitized file name; Storage object AND Firestore doc id.
    juce::String name;       // Display file name (may differ slightly from docId if sanitized).
    juce::String url;        // Firebase Storage download URL.
    juce::String mimeType;   // Storage-reported content type, e.g. "image/png".
    juce::String mediaType;  // "image" | "video" -- authoritative when set.

    int durationSec = 10;    // Images only; ignored for video (plays to natural end).
    int frequency = 1;       // Rotation weight -- higher shows more often. Always >= 1.

    juce::int64 startDateMs = 0; // 0 = no lower bound.
    juce::int64 endDateMs   = 0; // 0 = no upper bound.

    bool hasMetadataDoc = false; // false = Storage-only file, no Firestore doc yet.

    bool isVideo() const noexcept;
    bool isActiveAt (juce::int64 nowMs) const noexcept;
};
