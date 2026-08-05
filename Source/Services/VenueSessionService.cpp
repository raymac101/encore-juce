/*
  ==============================================================================

    VenueSessionService.cpp

  ==============================================================================
*/

#include "VenueSessionService.h"
#include "FirestoreClient.h"
#include "UserPreferences.h"

namespace
{
    constexpr int kHeartbeatIntervalMs = 60 * 1000;
    constexpr juce::int64 kStalenessMs = 5 * 60 * 1000; // 5 minutes

    juce::String sessionsCollectionPath(const juce::String& venueId)
    {
        return "venues/" + venueId + "/sessions";
    }
}

//==============================================================================
VenueSessionService& VenueSessionService::getInstance()
{
    static VenueSessionService instance;
    return instance;
}

void VenueSessionService::startHeartbeat(const juce::String& venueId)
{
    stopHeartbeat();
    if (venueId.isEmpty())
        return;

    activeVenueId_ = venueId;
    writeHeartbeatNow();
    startTimer(kHeartbeatIntervalMs);
}

void VenueSessionService::stopHeartbeat()
{
    stopTimer();
    if (activeVenueId_.isEmpty())
        return;

    const auto venueId = activeVenueId_;
    const auto deviceId = UserPreferences::getInstance().getDeviceId();
    activeVenueId_.clear();

    // Best-effort tidy-up only -- checkForOtherActiveSessions' staleness
    // filtering means a missed delete (e.g. this PC crashed instead of
    // signing out cleanly) self-heals after a few minutes regardless.
    juce::Thread::launch([venueId, deviceId]
    {
        FirestoreClient::getInstance().deleteDocument(sessionsCollectionPath(venueId) + "/" + deviceId);
    });
}

void VenueSessionService::timerCallback()
{
    writeHeartbeatNow();
}

void VenueSessionService::writeHeartbeatNow()
{
    if (activeVenueId_.isEmpty())
        return;

    const auto venueId = activeVenueId_;
    const auto deviceId = UserPreferences::getInstance().getDeviceId();
    const auto deviceLabel = juce::SystemStats::getComputerName();
    const auto nowMs = juce::Time::currentTimeMillis();

    juce::Thread::launch([venueId, deviceId, deviceLabel, nowMs]
    {
        // A plain PATCH with no updateMask fully replaces the doc's fields
        // (and upserts if it doesn't exist yet), which is exactly what we
        // want here -- every field this doc has is rewritten every tick
        // anyway, so there's nothing a partial updateMask would protect.
        auto fields = FirestoreClient::makeFields({
            { "deviceId",        FirestoreClient::stringValue(deviceId) },
            { "deviceLabel",     FirestoreClient::stringValue(deviceLabel) },
            { "lastHeartbeatAt", FirestoreClient::integerValue(nowMs) }
        });

        FirestoreClient::getInstance().patchDocument(sessionsCollectionPath(venueId) + "/" + deviceId, fields);
    });
}

void VenueSessionService::checkForOtherActiveSessions(const juce::String& venueId, CheckCallback onDone)
{
    const auto deviceId = UserPreferences::getInstance().getDeviceId();

    juce::Thread::launch([venueId, deviceId, onDone]
    {
        auto docs = FirestoreClient::getInstance().listCollection(sessionsCollectionPath(venueId), 50);

        // Empty is ambiguous between "genuinely nobody else is active" and
        // "the read itself failed" (network hiccup, or a misconfigured
        // Firestore rule) -- FirestoreClient::listCollection doesn't
        // currently surface that distinction. This fails open either way
        // (never a false "someone else is active"), but logs distinctly so
        // a systematically-empty result is at least visible during
        // rollout/QA rather than silently indistinguishable from success.
        if (docs.isEmpty())
            DBG ("[VenueSession] no session docs for venue " << venueId
                 << " -- either nobody else is active, or the read failed (listCollection doesn't distinguish)");

        const auto nowMs = juce::Time::currentTimeMillis();
        bool otherActive = false;
        juce::String otherLabel;

        for (auto& doc : docs)
        {
            const auto id = FirestoreClient::readString (doc, "deviceId");
            if (id.isEmpty() || id == deviceId)
                continue;

            const auto lastBeat = FirestoreClient::readInt (doc, "lastHeartbeatAt", 0);
            if (lastBeat <= 0 || (nowMs - lastBeat) > kStalenessMs)
                continue; // stale -- treat as gone, never lock out a crashed host

            otherActive = true;
            otherLabel = FirestoreClient::readString (doc, "deviceLabel");
            break;
        }

        if (onDone)
            juce::MessageManager::callAsync ([onDone, otherActive, otherLabel]
            {
                onDone (otherActive, otherLabel, {});
            });
    });
}
