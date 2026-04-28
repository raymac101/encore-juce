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

class QueueService
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

private:
    QueueService() = default;

    JUCE_DECLARE_NON_COPYABLE(QueueService)
};
