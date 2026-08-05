/*
  ==============================================================================

    VenueSessionService.h

    Lightweight presence heartbeat so the app can tell a host "this venue
    appears to be live on another computer right now" before they open it
    (login) or before they overwrite its cloud songbook (the sync-check
    prompt). This is a soft, best-effort signal, not a lock: it fails open
    on any error and cannot fully rule out two devices opening within the
    same few seconds of each other (see the plan doc for why a hard lock was
    deliberately rejected -- a crashed/disconnected show PC must never be
    able to lock its own host out of restarting their own show).

    Firestore path: venues/{venueId}/sessions/{deviceId}, fields:
      deviceId          -- this install's UserPreferences::getDeviceId()
      deviceLabel       -- juce::SystemStats::getComputerName(), for display
      lastHeartbeatAt   -- epoch ms, refreshed every heartbeat tick

    A device's own session doc is a plain, full-replace PATCH each tick
    (Firestore's PATCH upserts, so this covers doc-creation too) -- no
    createDocument step needed. Read/write access is already covered by the
    existing venues/{venueId}/{document=**} wildcard rule in
    firebase/firestore.rules; no rules changes needed for this new
    subcollection.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>

class VenueSessionService : private juce::Timer
{
public:
    static VenueSessionService& getInstance();

    /** Starts (or restarts, if already running for a different venue)
        writing this device's heartbeat for `venueId` every 60s, starting
        immediately. Safe to call repeatedly / from any venue-load path --
        internally stops any previous heartbeat first. */
    void startHeartbeat(const juce::String& venueId);

    /** Stops the heartbeat timer and best-effort deletes this device's own
        session doc (tidiness only -- checkForOtherActiveSessions' staleness
        filtering means a missed delete, e.g. from a crash, self-heals after
        a few minutes regardless). No-op if not currently running. */
    void stopHeartbeat();

    /** Checks whether any OTHER device has a fresh (< 5 minutes old)
        heartbeat for `venueId`. Calls onDone on the message thread with
        otherActive=true + that device's label if so. Fails open
        (otherActive=false) on any read error -- a connectivity hiccup or a
        misconfigured security rule must never block a host from opening
        their own venue; see the DBG log this emits when the result is
        empty, since that case is ambiguous between "genuinely nobody else"
        and "the read itself failed" (FirestoreClient::listCollection
        doesn't currently distinguish the two to its callers). */
    using CheckCallback = std::function<void (bool otherActive, juce::String otherDeviceLabel, juce::String error)>;
    void checkForOtherActiveSessions(const juce::String& venueId, CheckCallback onDone);

private:
    VenueSessionService() = default;
    ~VenueSessionService() override = default;

    void timerCallback() override;
    void writeHeartbeatNow();

    juce::String activeVenueId_;

    JUCE_DECLARE_NON_COPYABLE (VenueSessionService)
};
