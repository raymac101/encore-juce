/*
  ==============================================================================

    ImageCache.cpp

  ==============================================================================
*/

#include "ImageCache.h"

//==============================================================================
ArtworkCache& ArtworkCache::getInstance()
{
    static ArtworkCache inst;
    return inst;
}

//==============================================================================
juce::Image ArtworkCache::getOrFetch(const juce::String& url,
                                    std::function<void()> onLoaded)
{
    if (url.isEmpty())
        return {};

    std::unique_lock<std::mutex> lock(mutex_);

    // Already cached — return immediately
    auto it = cache_.find(url);
    if (it != cache_.end())
        return it->second;

    // Already downloading — queue the callback and bail
    auto pit = pending_.find(url);
    if (pit != pending_.end())
    {
        if (onLoaded) pit->second.push_back(std::move(onLoaded));
        return {};
    }

    // First request for this URL — create the queue entry and start download
    pending_[url] = {};
    if (onLoaded) pending_[url].push_back(std::move(onLoaded));
    lock.unlock();

    juce::String urlCopy = url;
    juce::Thread::launch([this, urlCopy]()
    {
        juce::Image img;

        try
        {
            juce::URL juceUrl(urlCopy);
            auto stream = juceUrl.createInputStream(
                juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                    .withConnectionTimeoutMs(10000)
                    .withNumRedirectsToFollow(5));

            if (stream != nullptr)
            {
                // Buffer the entire download — URL streams don't support seeking
                // but JPEG/PNG decoders need it.
                juce::MemoryBlock mb;
                stream->readIntoMemoryBlock(mb);

                if (mb.getSize() > 0)
                {
                    juce::MemoryInputStream mis(mb, false);
                    img = juce::ImageFileFormat::loadFrom(mis);
                }
            }
        }
        catch (...) {}

        // Store the result and grab the callback list atomically
        std::vector<std::function<void()>> cbs;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            cache_[urlCopy] = img;
            auto pit2 = pending_.find(urlCopy);
            if (pit2 != pending_.end())
            {
                cbs = std::move(pit2->second);
                pending_.erase(pit2);
            }
        }

        // Fire all waiting callbacks on the message thread
        if (! cbs.empty())
            juce::MessageManager::callAsync([cbs = std::move(cbs)]() {
                for (auto& cb : cbs) cb();
            });
    });

    return {};
}

//==============================================================================
void ArtworkCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}
