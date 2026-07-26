/*
  ==============================================================================

    ImageCache.h

    Thread-safe, asynchronous album-art image cache.

    Usage:
        auto img = ArtworkCache::getInstance().getOrFetch(url, [weakComp]() {
            if (weakComp) weakComp->repaint();
        });
        // img is valid if already cached, otherwise invalid (loading in bg).
        // When the download completes, onLoaded is called on the message thread.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <functional>
#include <map>
#include <vector>
#include <mutex>

//==============================================================================
/**
    Thread-safe, asynchronous album-art image cache.
    Named ArtworkCache to avoid conflict with juce::ImageCache.

    Make ONE call to getOrFetch() per component: it returns the image immediately
    if cached, or queues the callback and starts a single background download.
    Multiple callers for the same URL all receive a callback.
*/
class ArtworkCache
{
public:
    static ArtworkCache& getInstance();

    juce::Image getOrFetch(const juce::String& url,
                           std::function<void()> onLoaded = nullptr);

    /** Resolves a singer/host avatar field to a displayable image, handling
        every format TAGG has ever stored there:
          - "preset:<id>"                      -> assets/icon/<id>.png (new TAGG)
          - "assets/icon/<file>.png"            -> that file (legacy TAGG)
          - "assets/images/UnknownAvatar.png"   -> that file (no avatar selected)
          - "https://firebasestorage..../..."   -> fetched via getOrFetch
                                                    (user-uploaded custom photo)
        Local-file cases resolve synchronously. The URL case follows
        getOrFetch's usual contract: returns an invalid image immediately if
        not yet cached, and invokes onLoaded on the message thread once the
        download completes -- callers should re-call resolveAvatar() from
        that callback to pick up the now-cached image. */
    juce::Image resolveAvatar(const juce::String& avatarField,
                              std::function<void()> onLoaded = nullptr);

    void clear();

private:
    ArtworkCache() = default;

    std::map<juce::String, juce::Image>                         cache_;
    // pending_ maps URL -> list of callbacks waiting for that download
    std::map<juce::String, std::vector<std::function<void()>>> pending_;
    std::mutex                                                  mutex_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtworkCache)
};
