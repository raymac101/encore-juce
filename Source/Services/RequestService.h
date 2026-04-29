/*
  ==============================================================================

    RequestService.h

    Polls `venues/{venueId}/requested` for incoming song requests from the
    TAGG mobile app and routes them through callbacks so the desktop client
    can run its auto-approve checks. Mirrors the four
    Angular listeners on VenueService:

        listenForNewRequests       (status == "new")
        listenForApprovedRequests  (status == "approved" && action != "delete")
        listenForRejectedRequests  (status == "rejected")
        listenForDeleteRequests    (action == "delete")

    Firestore real-time listen channels are not implemented in our REST
    client, so we poll on a 3-second timer. Each unique (docId + status)
    pair is dispatched exactly once.

    All callbacks are invoked on the message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/QueueItem.h"
#include <unordered_map>

class RequestService : private juce::Timer
{
public:
    static RequestService& getInstance();

    using Callback = std::function<void(const QueueItem& item)>;

    /** Begin polling /requested for the given venue. Calling start with a
        new venueId implicitly stops any previous polling and clears the
        seen-doc cache. Safe to call from the message thread. */
    void start(const juce::String& venueId);

    /** Stop polling and clear the seen-doc cache. */
    void stop();

    bool isRunning() const noexcept { return running_; }

    //==============================================================================
    // Event sinks. Set once at startup; replaced rather than added.
    Callback onNewRequest;
    Callback onApprovedRequest;
    Callback onRejectedRequest;
    Callback onDeleteRequest;

    //==============================================================================
    // Helpers — write back to /requested. All run synchronously on the
    // message thread (network I/O is wrapped in juce::Thread::launch so they
    // are safe to call from UI handlers, but they return immediately).

    /** Patch a /requested doc's status (and optional reason) field. */
    void patchStatus(const juce::String& venueId,
                     const juce::String& itemId,
                     const juce::String& status,
                     const juce::String& reason = {});

    /** Hard-delete a /requested doc. */
    void deleteRequested(const juce::String& venueId,
                         const juce::String& itemId);

    /** Configure the polling cadence. Default = 3000 ms. */
    void setPollIntervalMs(int ms) noexcept { pollIntervalMs_ = juce::jmax(500, ms); }

private:
    RequestService();
    ~RequestService() override;

    void timerCallback() override;
    void poll();
    void dispatch(const juce::Array<juce::var>& docs);

    juce::String venueId_;
    bool running_ = false;
    bool pollInFlight_ = false;
    int  pollIntervalMs_ = 3000;

    // Tracks the last-seen status for each /requested doc id so we only
    // dispatch on actual transitions.
    std::unordered_map<std::string, std::string> seenStatus_;

    JUCE_DECLARE_NON_COPYABLE(RequestService)
};
