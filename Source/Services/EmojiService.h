/*
  ==============================================================================

    EmojiService.h

    Polls `venues/{venueId}/emojis` for cheer reactions sent by the TAGG
    mobile app while a singer is performing. Mirrors the Angular
    VenueService.getNewEmoji() real-time listener, but since our REST
    Firestore client has no listen channel, we poll on a short timer instead.

    Each document is delivered through onNewEmoji exactly once (tracked by
    doc id) — it is up to the caller (LyricDisplayComponent) to animate it
    and delete the underlying doc once its on-screen life ends.

    All callbacks are invoked on the message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Emoji.h"
#include <unordered_set>
#include <vector>

class EmojiService : private juce::Timer
{
public:
    static EmojiService& getInstance();

    using Callback = std::function<void(const std::vector<Emoji>& newEmojis)>;

    /** Begin polling /emojis for the given venue. Calling start with a new
        venueId implicitly stops any previous polling and clears the
        seen-doc cache. Safe to call from the message thread. */
    void start(const juce::String& venueId);

    /** Stop polling and clear the seen-doc cache. */
    void stop();

    bool isRunning() const noexcept { return running_; }

    //==============================================================================
    // Event sink. Set once at startup; replaced rather than added. Fired
    // only with documents not previously delivered this session.
    Callback onNewEmoji;

    /** Configure the polling cadence. Default = 1000 ms. */
    void setPollIntervalMs(int ms) noexcept { pollIntervalMs_ = juce::jmax(250, ms); }

private:
    EmojiService() = default;
    ~EmojiService() override;

    void timerCallback() override;
    void poll();

    juce::String venueId_;
    bool running_ = false;
    bool pollInFlight_ = false;
    int  pollIntervalMs_ = 1000;

    // Doc ids already handed to onNewEmoji this session, so repeated polls
    // (the doc typically lives on until it's consumed/animated) don't
    // re-dispatch the same reaction.
    std::unordered_set<std::string> seenIds_;

    JUCE_DECLARE_NON_COPYABLE(EmojiService)
};
