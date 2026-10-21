/*
  ==============================================================================

    HostIdentityCache.cpp

  ==============================================================================
*/

#include "HostIdentityCache.h"
#include "FirestoreClient.h"

//==============================================================================
HostIdentityCache& HostIdentityCache::getInstance()
{
    static HostIdentityCache inst;
    return inst;
}

bool HostIdentityCache::isKnownHost(const juce::String& uid, std::function<void()> onResolved)
{
    if (uid.isEmpty())
        return false;

    std::unique_lock<std::mutex> lock(mutex_);

    // Already resolved — return immediately.
    auto it = cache_.find(uid);
    if (it != cache_.end())
        return it->second;

    // Already checking — queue the callback and bail.
    auto pit = pending_.find(uid);
    if (pit != pending_.end())
    {
        if (onResolved) pit->second.push_back(std::move(onResolved));
        return false;
    }

    // First request for this uid — create the queue entry and start the check.
    pending_[uid] = {};
    if (onResolved) pending_[uid].push_back(std::move(onResolved));
    lock.unlock();

    juce::String uidCopy = uid;
    juce::Thread::launch([this, uidCopy]()
    {
        // getDocument() returns a void var specifically on a 404, which is
        // exactly how "no hosts/{uid} doc" looks -- see FirestoreClient.cpp.
        const auto doc = FirestoreClient::getInstance().getDocument("hosts/" + uidCopy);
        const bool isHost = ! doc.isVoid();

        std::vector<std::function<void()>> cbs;
        {
            std::lock_guard<std::mutex> lk(mutex_);
            cache_[uidCopy] = isHost;
            auto pit2 = pending_.find(uidCopy);
            if (pit2 != pending_.end())
            {
                cbs = std::move(pit2->second);
                pending_.erase(pit2);
            }
        }

        if (! cbs.empty())
            juce::MessageManager::callAsync([cbs = std::move(cbs)]() {
                for (auto& cb : cbs) cb();
            });
    });

    return false;
}

void HostIdentityCache::clear()
{
    std::lock_guard<std::mutex> lk(mutex_);
    cache_.clear();
    pending_.clear();
}
