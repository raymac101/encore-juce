/*
  ==============================================================================

    HostIdentityCache.h

    Thread-safe, asynchronous cache answering "does this uid have a
    Firestore hosts/{uid} document?" -- i.e. is this identity a karaoke
    host/KJ account anywhere in the system, not just at the current venue.
    Used to decide whether a singer's avatar gets the purple "host account"
    glow in QueueBar (see SingerRow::isHostAccount).

    Usage (mirrors ArtworkCache::getOrFetch -- see ImageCache.h):
        bool known = HostIdentityCache::getInstance().isKnownHost(uid, [weakComp]() {
            if (weakComp) weakComp->repaint();
        });
        // known is the cached answer if already resolved, otherwise false
        // (not yet known) while a background Firestore lookup runs.
        // onResolved fires on the message thread once that lookup completes
        // -- callers should re-check isKnownHost() from it to get the real
        // answer.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <map>
#include <vector>
#include <mutex>

//==============================================================================
class HostIdentityCache
{
public:
    static HostIdentityCache& getInstance();

    bool isKnownHost(const juce::String& uid, std::function<void()> onResolved = nullptr);

    void clear();

private:
    HostIdentityCache() = default;

    std::map<juce::String, bool>                                cache_;
    // pending_ maps uid -> list of callbacks waiting for that lookup
    std::map<juce::String, std::vector<std::function<void()>>> pending_;
    std::mutex                                                   mutex_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HostIdentityCache)
};
