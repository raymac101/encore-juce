/*
  ==============================================================================

    ImageCache.cpp

  ==============================================================================
*/

#include "ImageCache.h"
#include "GlobalProgressService.h"

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
        const int progressTaskId = GlobalProgressService::getInstance().beginTask("Downloading artwork...");
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

        GlobalProgressService::getInstance().endTask(progressTaskId);
    });

    return {};
}

//==============================================================================
juce::Image ArtworkCache::resolveAvatar(const juce::String& avatarField,
                                        std::function<void()> onLoaded)
{
    const auto field = avatarField.trim();
    if (field.isEmpty())
        return {};

    // User-uploaded custom photo: a full Firebase Storage download URL.
    if (field.startsWithIgnoreCase("http://") || field.startsWithIgnoreCase("https://"))
        return getOrFetch(field, std::move(onLoaded));

    auto appDir = juce::File::getSpecialLocation(juce::File::currentExecutableFile)
                      .getParentDirectory();
    auto iconDir = appDir.getChildFile("assets/icon");

    // New TAGG preset format: "preset:<id>" -> assets/icon/<id>[.ext].
    if (field.startsWithIgnoreCase("preset:"))
    {
        const auto id = field.substring(7).trim();
        if (id.isEmpty())
            return {};

        auto direct = iconDir.getChildFile(id);
        if (direct.existsAsFile())
            return juce::ImageFileFormat::loadFrom(direct);

        static const char* const extensions[] = { ".png", ".jpg", ".jpeg", ".gif" };
        for (auto* ext : extensions)
        {
            auto candidate = iconDir.getChildFile(id + ext);
            if (candidate.existsAsFile())
                return juce::ImageFileFormat::loadFrom(candidate);
        }
        return {};
    }

    // Legacy TAGG relative path ("assets/icon/1082581.png") or the "no
    // avatar selected" placeholder ("assets/images/UnknownAvatar.png").
    auto baseName = field.fromLastOccurrenceOf("/", false, false);
    if (baseName.isEmpty())
        baseName = field;

    auto candidate1 = iconDir.getChildFile(baseName);
    if (candidate1.existsAsFile())
        return juce::ImageFileFormat::loadFrom(candidate1);

    auto candidate2 = appDir.getChildFile(field);
    if (candidate2.existsAsFile())
        return juce::ImageFileFormat::loadFrom(candidate2);

    auto candidate3 = appDir.getChildFile("assets/" + field);
    if (candidate3.existsAsFile())
        return juce::ImageFileFormat::loadFrom(candidate3);

    return {};
}

//==============================================================================
void ArtworkCache::clear()
{
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.clear();
}
