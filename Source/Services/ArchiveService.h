/*
  ==============================================================================

    ArchiveService.h

    End-of-night session archive + cleanup. Mirrors the Angular
    ArchiveService (src/app/services/archive.service.ts) so behaviour is
    consistent between the legacy Electron app and the JUCE rewrite:

    1. Copy /venues/{id}/audit/*       to /archives/{id}/audit/*
       (each archived doc gets the new sessionId + archiveDate stamped on)
    2. Copy /venues/{id}/membersAudit/* to /archives/{id}/members/*
    3. Create /archives/{id}/sessions/{sessionId} record with totals.
    4. Delete every doc in:
         /venues/{id}/audit
         /venues/{id}/membersAudit
         /venues/{id}/queue
         /venues/{id}/requested

    The archive step preserves audit data so it remains queryable; the
    delete step empties the operational collections so the next karaoke
    night starts with a clean queue / requested list.

    All public entry points are safe to call from the message thread; the
    actual HTTP work runs on a background juce::Thread::launch task.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class ArchiveService : public juce::Timer
{
public:
    static ArchiveService& getInstance();

    /** Archive + clear the current session for the given venue. Runs in the
        background; `onDone` is invoked on the message thread when finished.
        On success `sessionId` is the id of the newly-created sessions doc;
        on failure `error` carries a human-readable message. */
    using DoneCallback = std::function<void(bool ok,
                                            juce::String sessionId,
                                            juce::String error)>;
    void archiveAndClearSession(const juce::String& venueId,
                                const juce::String& venueName,
                                DoneCallback onDone = {});

    /** Begin (or restart) the nightly cleanup schedule. The configured
        cleanup hour comes from UserPreferences::getNightlyCleanupHour()
        and is re-read each minute, so changes in the Settings page take
        effect on the next tick. Calling this with an empty venueId stops
        the timer. */
    void startNightlyCleanup(const juce::String& venueId,
                             const juce::String& venueName);

    void stopNightlyCleanup();

private:
    ArchiveService() = default;
    ~ArchiveService() override = default;

    void timerCallback() override;

    // Strip the "projects/.../databases/(default)/documents/" prefix from a
    // Firestore document `name` so we can pass it back into FirestoreClient
    // (which prepends that prefix itself).
    static juce::String relPathFromDocName(const juce::String& docName);

    // Copy every doc in srcCollection to dstCollection, stamping each with
    // sessionId + archiveDate. Returns the number of docs copied.
    static int copyCollectionAddingSession(const juce::String& srcCollection,
                                           const juce::String& dstCollection,
                                           const juce::String& sessionId,
                                           const juce::String& archiveDate);

    // Delete every doc in the given collection.
    static void clearCollection(const juce::String& collectionPath);

    // Cleanup-scheduling state (only touched on the message thread).
    juce::String venueId_;
    juce::String venueName_;
    juce::String lastCleanupDate_;       // YYYY-MM-DD; prevents repeat runs
    bool         cleanupInFlight_ = false;

    JUCE_DECLARE_NON_COPYABLE(ArchiveService)
};
