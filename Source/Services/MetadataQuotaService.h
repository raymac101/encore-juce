/*
  ==============================================================================

    MetadataQuotaService.h

    Thin client for the shared daily Spotify-call quota tracked server-side
    in Firestore (metadataQuota/daily, enforced inside the
    /searchArtistAndSong Cloud Function -- see firebase/functions/index.js).
    Used by the Viracicom Admin bulk metadata tool to show "used X of 1000
    today" and to clamp its batch-size picker to what's actually left.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class MetadataQuotaService
{
public:
    static MetadataQuotaService& getInstance();

    struct Status
    {
        bool         ok = false;
        int          usedCalls = 0;
        int          cap = 1000;
        int          remaining = 0;
        juce::String errorMessage;
    };

    using Callback = std::function<void (Status)>;

    /** Calls the getMetadataQuotaStatus Cloud Function. Runs on a background
        thread; callback fires on the message thread. */
    void getStatus (Callback onDone);

private:
    MetadataQuotaService() = default;
    ~MetadataQuotaService() = default;

    JUCE_DECLARE_NON_COPYABLE (MetadataQuotaService)
};
