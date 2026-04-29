/*
  ==============================================================================

    ArchiveService.cpp

  ==============================================================================
*/

#include "ArchiveService.h"
#include "FirestoreClient.h"
#include "UserPreferences.h"

//==============================================================================
ArchiveService& ArchiveService::getInstance()
{
    static ArchiveService instance;
    return instance;
}

//==============================================================================
juce::String ArchiveService::relPathFromDocName(const juce::String& docName)
{
    const auto marker = juce::String("/documents/");
    auto idx = docName.indexOf(marker);
    if (idx < 0) return docName;
    return docName.substring(idx + marker.length());
}

//==============================================================================
int ArchiveService::copyCollectionAddingSession(const juce::String& srcCollection,
                                                const juce::String& dstCollection,
                                                const juce::String& sessionId,
                                                const juce::String& archiveDate)
{
    auto& fc = FirestoreClient::getInstance();
    auto docs = fc.listCollection(srcCollection, 200);
    int count = 0;

    for (auto& d : docs)
    {
        // Pull the existing fields object verbatim, then overlay the
        // archive-specific stamps before writing it into archives/.
        auto fields = d.getProperty("fields", juce::var());
        auto* fieldsObj = fields.getDynamicObject();
        if (fieldsObj == nullptr)
        {
            // Empty doc — still archive the row but with just the stamps.
            juce::DynamicObject::Ptr fresh = new juce::DynamicObject();
            fields = juce::var(fresh.get());
            fieldsObj = fields.getDynamicObject();
        }

        fieldsObj->setProperty("sessionId",   FirestoreClient::stringValue(sessionId));
        fieldsObj->setProperty("archiveDate", FirestoreClient::stringValue(archiveDate));

        // Preserve original document id when possible — easier to correlate
        // the archived row back to the live one for debugging.
        const auto srcDocName = d.getProperty("name", "").toString();
        const auto srcDocId   = srcDocName.fromLastOccurrenceOf("/", false, false);

        fc.createDocument(dstCollection, fields, srcDocId);
        ++count;
    }
    return count;
}

//==============================================================================
void ArchiveService::clearCollection(const juce::String& collectionPath)
{
    auto& fc = FirestoreClient::getInstance();

    // Page through the collection and delete in batches. listCollection has
    // a fixed page size of 200, so loop until the list comes back empty.
    while (true)
    {
        auto docs = fc.listCollection(collectionPath, 200);
        if (docs.isEmpty())
            break;

        for (auto& d : docs)
        {
            const auto rel = relPathFromDocName(d.getProperty("name", "").toString());
            if (rel.isNotEmpty())
                fc.deleteDocument(rel);
        }

        // If we got fewer than the page size, we're definitely done.
        if (docs.size() < 200)
            break;
    }
}

//==============================================================================
void ArchiveService::archiveAndClearSession(const juce::String& venueId,
                                            const juce::String& venueName,
                                            DoneCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone)
            juce::MessageManager::callAsync(
                [onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch(
        [venueId, venueName, onDone = std::move(onDone)]()
        {
            try
            {
                auto& fc = FirestoreClient::getInstance();

                const auto now         = juce::Time::getCurrentTime();
                const auto startMillis = now.toMilliseconds();
                const auto sessionDate = now.formatted("%Y-%m-%d");

                // 1. Create the archives/{venueId}/sessions/{auto} doc.
                const auto sessionsColl = "archives/" + venueId + "/sessions";
                auto sessionFields = FirestoreClient::makeFields({
                    { "venueId",          FirestoreClient::stringValue(venueId)   },
                    { "venueName",        FirestoreClient::stringValue(venueName) },
                    { "sessionDate",      FirestoreClient::stringValue(sessionDate) },
                    { "startTime",        FirestoreClient::integerValue(startMillis) },
                    { "endTime",          FirestoreClient::integerValue(0) },
                    { "totalSongsPlayed", FirestoreClient::integerValue(0) },
                    { "totalMembers",     FirestoreClient::integerValue(0) },
                    { "createdAt",        FirestoreClient::integerValue(startMillis) }
                });

                auto sessionDoc = fc.createDocument(sessionsColl, sessionFields);
                const auto sessionDocName = sessionDoc.getProperty("name", "").toString();
                if (sessionDocName.isEmpty())
                    throw std::runtime_error("Failed to create archive session doc");

                const auto sessionId = sessionDocName.fromLastOccurrenceOf("/", false, false);

                // 2. Archive audit data (songs played) into archives/{id}/audit.
                const int auditCount = copyCollectionAddingSession(
                    "venues/"   + venueId + "/audit",
                    "archives/" + venueId + "/audit",
                    sessionId, sessionDate);

                // 3. Archive member data into archives/{id}/members.
                const int memberCount = copyCollectionAddingSession(
                    "venues/"   + venueId + "/membersAudit",
                    "archives/" + venueId + "/members",
                    sessionId, sessionDate);

                // 4. Clear all four operational collections, including
                //    /requested (matches Angular's clearOperationalData).
                clearCollection("venues/" + venueId + "/audit");
                clearCollection("venues/" + venueId + "/membersAudit");
                clearCollection("venues/" + venueId + "/queue");
                clearCollection("venues/" + venueId + "/requested");

                // 5. Update the session record with final totals.
                const auto endMillis = juce::Time::getCurrentTime().toMilliseconds();
                const auto sessionRel = relPathFromDocName(sessionDocName);
                const auto maskedPath = sessionRel
                    + "?updateMask.fieldPaths=totalSongsPlayed"
                    + "&updateMask.fieldPaths=totalMembers"
                    + "&updateMask.fieldPaths=endTime";

                auto totals = FirestoreClient::makeFields({
                    { "totalSongsPlayed", FirestoreClient::integerValue(auditCount)  },
                    { "totalMembers",     FirestoreClient::integerValue(memberCount) },
                    { "endTime",          FirestoreClient::integerValue(endMillis)   }
                });
                fc.patchDocument(maskedPath, totals);

                DBG("[Archive] Session " << sessionId
                    << " archived: " << auditCount << " songs, "
                    << memberCount << " members. Operational collections cleared.");

                if (onDone)
                    juce::MessageManager::callAsync(
                        [onDone, sessionId] { onDone(true, sessionId, {}); });
            }
            catch (const std::exception& e)
            {
                const juce::String msg(e.what());
                DBG("[Archive] FAILED: " << msg);
                if (onDone)
                    juce::MessageManager::callAsync(
                        [onDone, msg] { onDone(false, {}, msg); });
            }
            catch (...)
            {
                DBG("[Archive] FAILED: unknown error");
                if (onDone)
                    juce::MessageManager::callAsync(
                        [onDone] { onDone(false, {}, "Unknown error"); });
            }
        });
}

//==============================================================================
void ArchiveService::startNightlyCleanup(const juce::String& venueId,
                                         const juce::String& venueName)
{
    venueId_   = venueId;
    venueName_ = venueName;

    if (venueId.isEmpty())
    {
        stopTimer();
        return;
    }

    // Don't archive the moment the app starts — assume any cleanup that
    // should already have run for "today" was either already done or the
    // PC was off, in which case there's nothing meaningful to archive.
    lastCleanupDate_ = juce::Time::getCurrentTime().formatted("%Y-%m-%d");

    // Tick once a minute. Cheaper than computing exact ms-until-next-hour
    // and naturally robust against the machine sleeping past the target.
    startTimer(60 * 1000);
}

void ArchiveService::stopNightlyCleanup()
{
    stopTimer();
    venueId_.clear();
    venueName_.clear();
    cleanupInFlight_ = false;
}

void ArchiveService::timerCallback()
{
    if (cleanupInFlight_ || venueId_.isEmpty())
        return;

    const auto now      = juce::Time::getCurrentTime();
    const auto today    = now.formatted("%Y-%m-%d");
    const int  hourNow  = now.getHours();
    const int  targetHr = UserPreferences::getInstance().getNightlyCleanupHour();

    if (hourNow != targetHr || today == lastCleanupDate_)
        return;

    cleanupInFlight_  = true;
    lastCleanupDate_  = today;

    DBG("[Archive] Nightly cleanup triggered for venue " << venueId_
        << " at " << hourNow << ":00");

    archiveAndClearSession(venueId_, venueName_,
        [this] (bool ok, juce::String sessionId, juce::String error)
        {
            cleanupInFlight_ = false;
            if (ok)
                DBG("[Archive] Nightly cleanup OK (session " << sessionId << ")");
            else
                DBG("[Archive] Nightly cleanup FAILED: " << error);
        });
}
