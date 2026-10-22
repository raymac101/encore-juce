/*
  ==============================================================================

    QueueRotation.cpp

  ==============================================================================
*/

#include "QueueRotation.h"
#include <algorithm>

namespace QueueRotation
{
    namespace
    {
        // Real singers always have a non-empty Firestore doc id in practice;
        // the "name:" form only ever applies to a synthetic host placeholder
        // used before a real host doc exists (see MainComponent::composeQueueWithHost).
        juce::String identityKey(const Singers& s)
        {
            const auto id = juce::String(s.id).trim();
            if (id.isNotEmpty())
                return id;
            return "name:" + juce::String(s.name).trim().toLowerCase();
        }

        int findIndexByIdentity(const std::vector<Singers>& singers, const juce::String& key)
        {
            for (int i = 0; i < (int) singers.size(); ++i)
                if (identityKey(singers[(size_t) i]) == key)
                    return i;
            return -1;
        }
    }

    int findHostIndex(const std::vector<Singers>& singers)
    {
        for (int i = 0; i < (int) singers.size(); ++i)
            if (singers[(size_t) i].isHost)
                return i;
        return -1;
    }

    std::vector<Singers> sortByStableOrder(std::vector<Singers> singers)
    {
        std::stable_sort(singers.begin(), singers.end(),
            [](const Singers& a, const Singers& b)
            {
                if (a.order != b.order)
                    return a.order < b.order;
                return a.isHost && ! b.isHost;
            });
        return singers;
    }

    void renumberStableOrder(std::vector<Singers>& rr)
    {
        for (int i = 0; i < (int) rr.size(); ++i)
            rr[(size_t) i].order = i;
    }

    juce::String findAnchorId(const std::vector<Singers>& singers)
    {
        int bestIndex = -1;
        for (int i = 0; i < (int) singers.size(); ++i)
        {
            const auto& s = singers[(size_t) i];
            if (s.rotationOrder != 0)
                continue;
            if (bestIndex < 0 || s.order < singers[(size_t) bestIndex].order)
                bestIndex = i;
        }

        if (bestIndex >= 0)
            return identityKey(singers[(size_t) bestIndex]);

        // Nobody holds rank 0 (e.g. the host doc's unranked -1, or the host
        // has no doc yet) -- the round start is up next. Picking the smallest
        // positive rank here instead would let a newcomer jump the host.
        if (! singers.empty())
            return identityKey(sortByStableOrder(singers).front());
        return {};
    }

    std::vector<Singers> deriveDisplayQueue(const std::vector<Singers>& rr, const juce::String& anchorId)
    {
        if (rr.empty())
            return {};

        int anchorIndex = findIndexByIdentity(rr, anchorId);
        if (anchorIndex < 0)
            anchorIndex = 0;

        std::vector<Singers> out;
        out.reserve(rr.size());
        for (size_t step = 0; step < rr.size(); ++step)
            out.push_back(rr[(size_t) ((anchorIndex + (int) step) % (int) rr.size())]);
        return out;
    }

    void stampDerivedRanks(std::vector<Singers>& rr, const juce::String& anchorId)
    {
        if (rr.empty())
            return;

        int anchorIndex = findIndexByIdentity(rr, anchorId);
        if (anchorIndex < 0)
            anchorIndex = 0;

        const int n = (int) rr.size();
        for (int i = 0; i < n; ++i)
            rr[(size_t) i].rotationOrder = ((i - anchorIndex) + n) % n;
    }

    juce::String advanceAnchor(const std::vector<Singers>& rr, int departedOrder)
    {
        if (rr.size() < 2)
            return {};

        int nextIndex = -1;
        for (int i = 0; i < (int) rr.size(); ++i)
        {
            if (rr[(size_t) i].order <= departedOrder)
                continue;
            if (nextIndex < 0 || rr[(size_t) i].order < rr[(size_t) nextIndex].order)
                nextIndex = i;
        }

        if (nextIndex >= 0)
            return identityKey(rr[(size_t) nextIndex]);

        // Nothing with a higher order -- wrap to the lowest existing order
        // (rr is sorted per sortByStableOrder, so that's rr[0]).
        return identityKey(rr.front());
    }

    juce::String retreatAnchor(const std::vector<Singers>& rr, int currentOrder)
    {
        if (rr.size() < 2)
            return {};

        int prevIndex = -1;
        for (int i = 0; i < (int) rr.size(); ++i)
        {
            if (rr[(size_t) i].order >= currentOrder)
                continue;
            if (prevIndex < 0 || rr[(size_t) i].order > rr[(size_t) prevIndex].order)
                prevIndex = i;
        }

        if (prevIndex >= 0)
            return identityKey(rr[(size_t) prevIndex]);

        // Nothing with a lower order -- wrap to the highest existing order
        // (rr is sorted per sortByStableOrder, so that's rr.back()).
        return identityKey(rr.back());
    }

    DragRemapResult remapFromDisplayDrag(const std::vector<Singers>& displayedQueue,
                                         int fromIndex, int toIndex,
                                         bool takeRoundStartSlot)
    {
        DragRemapResult result;

        const int n = (int) displayedQueue.size();
        if (fromIndex < 0 || fromIndex >= n || toIndex < 0 || toIndex >= n)
        {
            result.newRR = displayedQueue;
            result.newAnchorId = displayedQueue.empty() ? juce::String() : identityKey(displayedQueue.front());
            return result;
        }

        // The round start stays put: if the singer being dragged IS the
        // round start, whoever followed them in the round takes over that
        // slot; otherwise the round start is unchanged.
        auto roundStartKey = identityKey(sortByStableOrder(displayedQueue).front());
        if (identityKey(displayedQueue[(size_t) fromIndex]) == roundStartKey && n > 1)
            roundStartKey = identityKey(displayedQueue[(size_t) ((fromIndex + 1) % n)]);

        std::vector<Singers> newQueueOrder = displayedQueue;
        auto moved = newQueueOrder[(size_t) fromIndex];
        newQueueOrder.erase(newQueueOrder.begin() + fromIndex);
        newQueueOrder.insert(newQueueOrder.begin() + toIndex, moved);

        int startPos = findIndexByIdentity(newQueueOrder, roundStartKey);
        if (startPos < 0)
            startPos = 0;

        // Dropped directly above the round start: the dropped singer claims
        // the round start slot (and the old start becomes second).
        if (takeRoundStartSlot && startPos > 0 && toIndex == startPos - 1)
        {
            roundStartKey = identityKey(moved);
            startPos = toIndex;
        }

        std::vector<Singers> newRR;
        newRR.reserve(newQueueOrder.size());
        for (size_t step = 0; step < newQueueOrder.size(); ++step)
            newRR.push_back(newQueueOrder[(size_t) ((startPos + (int) step) % n)]);

        renumberStableOrder(newRR);

        result.newRR = std::move(newRR);
        result.newAnchorId = identityKey(newQueueOrder.front());
        return result;
    }
}
