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

    void clear();

private:
    ArtworkCache() = default;

    std::map<juce::String, juce::Image>                         cache_;
    // pending_ maps URL -> list of callbacks waiting for that download
    std::map<juce::String, std::vector<std::function<void()>>> pending_;
    std::mutex                                                  mutex_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ArtworkCache)
};
