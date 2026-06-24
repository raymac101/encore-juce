/*
  ==============================================================================

    AuditService.h

    Migration of src/app/services/audit.service.ts.

    Writes audit records when a song passes the 30-second mark (or finishes
    naturally).  Matches the Angular behaviour exactly — same Firestore
    collection paths, same field names.

    Collections written:
      audit                                    — global audit log
      venues/{venueId}/audit                   — venue-scoped recently played
      users/{profileId}/userAudit              — per-user detailed audit
                                                 (only when profileId is set)
      venues/{venueId}/membersAudit            — singer history (upserted by
                                                 profileId — or singerName
                                                 when no profileId)

    Threading
    ─────────
    addAudit() dispatches all Firestore writes on a background thread and is
    safe to call from the JUCE message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Audit.h"
#include "../Models/Member.h"
#include "../Models/CdgSong.h"
#include "../Models/Singers.h"
#include "../Models/QueueItem.h"

class AuditService
{
public:
    static AuditService& getInstance();

    //==========================================================================
    /**
        Fire-and-forget: build all four audit documents from the current song
        and singer state, then write them to Firestore in the background.

        @param song        The CdgSong that was just played (provides metadata).
        @param singer      The Singers record for the performing singer.
        @param item        The QueueItem being performed (pitch, version,
                           profileId, deviceId, foxId).
        @param venueId     Active venue Firestore document ID.
        @param venueName   Display name of the venue (stored in audit docs).
        @param kjId        Firestore UID of the signed-in KJ / host.
    */
    void addAudit(const CdgSong&    song,
                  const Singers&    singer,
                  const QueueItem&  item,
                  const juce::String& venueId,
                  const juce::String& venueName,
                  const juce::String& kjId);

private:
    AuditService()  = default;
    ~AuditService() = default;

    /** Mirrors the Angular determineSource() logic. */
    static juce::String determineSource(const juce::String& deviceId);

    /** Build the Firestore-typed-value fields map for an Audit document. */
    static juce::var auditFields(const Audit& a);

    /** Build the Firestore-typed-value fields map for a UserAudit document. */
    static juce::var userAuditFields(const UserAudit& u);

    /** Build the Firestore-typed-value fields map for a Member document
        (used when creating a new membersAudit doc). */
    static juce::var memberFields(const Member& m);

    /** Build a single MemberHistory entry as a Firestore mapValue. */
    static juce::var memberHistoryMapValue(const MemberHistory& h);

    JUCE_DECLARE_NON_COPYABLE(AuditService)
};
