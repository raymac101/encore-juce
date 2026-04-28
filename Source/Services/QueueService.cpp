/*
  ==============================================================================

    QueueService.cpp

  ==============================================================================
*/

#include "QueueService.h"
#include "FirestoreClient.h"
#include <algorithm>
#include <unordered_map>

QueueService& QueueService::getInstance()
{
    static QueueService instance;
    return instance;
}

namespace
{
    // Read a typed field directly from a Firestore document var (which still
    // has the { fields: { ... } } wrapper). Mirrors the helpers on
    // FirestoreClient but adds double + helper-by-name conveniences for the
    // queue schema where some numeric fields may have been written as
    // strings.
    juce::String readStr(const juce::var& doc, const juce::String& field)
    {
        return FirestoreClient::readString(doc, field);
    }

    int readInt(const juce::var& doc, const juce::String& field, int dflt = 0)
    {
        auto fields = doc.getProperty("fields", juce::var());
        auto v = fields.getProperty(juce::Identifier(field), juce::var());
        if (v.hasProperty("integerValue"))
            return (int) v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("doubleValue"))
            return (int) (double) v.getProperty("doubleValue", 0.0);
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "").toString().getIntValue();
        return dflt;
    }

    double readDouble(const juce::var& doc, const juce::String& field, double dflt = 0.0)
    {
        auto fields = doc.getProperty("fields", juce::var());
        auto v = fields.getProperty(juce::Identifier(field), juce::var());
        if (v.hasProperty("doubleValue"))
            return (double) v.getProperty("doubleValue", dflt);
        if (v.hasProperty("integerValue"))
            return (double) v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "").toString().getDoubleValue();
        return dflt;
    }

    QueueItem itemFromDoc(const juce::var& doc)
    {
        // Angular schema field names — see src/app/models/queueItem.model.ts.
        QueueItem q;
        q.id           = readStr(doc, "id").toStdString();
        q.deviceId     = readStr(doc, "deviceId").toStdString();
        q.singerName   = readStr(doc, "singerName").toStdString();
        q.singerAvatar = readStr(doc, "avatar").toStdString();
        q.songId       = readStr(doc, "songId").toStdString();
        q.songName     = readStr(doc, "song").toStdString();
        q.songArtist   = readStr(doc, "artist").toStdString();
        q.songVersion  = readStr(doc, "songVersion").toStdString();
        q.duration     = readInt(doc, "duration");
        q.order        = readInt(doc, "order");
        q.songOrder    = readInt(doc, "songOrder");
        q.pitch        = (float) readDouble(doc, "pitch", 1.0);
        q.status       = readStr(doc, "status").toStdString();
        q.time         = readStr(doc, "time").toStdString();
        return q;
    }

    // Group queue items into singer rows. Grouping key is profileId where
    // present (Angular's foxId/profileId), else singerName. Songs inside a
    // singer are ordered by `songOrder`; singers are ordered by
    // `rotationOrder` (falling back to the lowest `order` of their songs).
    QueueService::Snapshot buildSnapshot(const juce::Array<juce::var>& docs)
    {
        QueueService::Snapshot snap;

        // First pass: parse + figure out grouping keys.
        struct Row
        {
            QueueItem    item;
            juce::String key;
            juce::String foxId;
            juce::String profileId;
            int          rotationOrder = 0;
        };

        std::vector<Row> rows;
        rows.reserve((size_t) docs.size());

        for (auto& d : docs)
        {
            Row r;
            r.item          = itemFromDoc(d);
            r.foxId         = readStr(d, "foxId");
            r.profileId     = readStr(d, "profileId");
            r.rotationOrder = readInt(d, "rotationOrder");

            r.key = r.foxId.isNotEmpty() ? r.foxId
                  : r.profileId.isNotEmpty() ? r.profileId
                  : juce::String(r.item.singerName);

            if (r.key.isEmpty())
                continue;

            rows.push_back(std::move(r));
        }

        // Second pass: bucket by key, preserving first-seen order so we can
        // give each bucket a deterministic insertion index for tie-breaks.
        std::unordered_map<std::string, size_t> indexByKey;
        std::vector<Singers> buckets;
        std::vector<int>     bucketRotation;
        std::vector<int>     bucketLowestOrder;

        for (auto& r : rows)
        {
            auto k = r.key.toStdString();
            auto it = indexByKey.find(k);
            if (it == indexByKey.end())
            {
                Singers s;
                s.id            = r.profileId.isNotEmpty() ? r.profileId.toStdString() : k;
                s.name          = r.item.singerName;
                s.avatar        = r.item.singerAvatar;
                s.deviceId      = r.item.deviceId;
                s.rotationOrder = r.rotationOrder;
                s.order         = r.item.order;

                indexByKey[k] = buckets.size();
                buckets.push_back(std::move(s));
                bucketRotation.push_back(r.rotationOrder);
                bucketLowestOrder.push_back(r.item.order);
            }
            else
            {
                bucketLowestOrder[it->second] =
                    juce::jmin(bucketLowestOrder[it->second], r.item.order);
            }

            buckets[indexByKey[k]].songs.push_back(r.item);
        }

        // Sort each singer's songs by songOrder.
        for (auto& s : buckets)
        {
            std::sort(s.songs.begin(), s.songs.end(),
                      [](const QueueItem& a, const QueueItem& b)
                      {
                          return a.songOrder < b.songOrder;
                      });
        }

        // Identify "now playing" — singer with any song whose status is
        // "playing", or fall back to the singer with the lowest order if
        // none is explicitly playing.
        int nowPlayingBucket = -1;
        for (size_t i = 0; i < buckets.size(); ++i)
        {
            for (auto& q : buckets[i].songs)
            {
                if (q.status == "playing")
                {
                    nowPlayingBucket = (int) i;
                    break;
                }
            }
            if (nowPlayingBucket >= 0) break;
        }

        // Build sort indices for the upcoming list, excluding now-playing.
        std::vector<size_t> upcomingIdx;
        upcomingIdx.reserve(buckets.size());
        for (size_t i = 0; i < buckets.size(); ++i)
            if ((int) i != nowPlayingBucket)
                upcomingIdx.push_back(i);

        std::sort(upcomingIdx.begin(), upcomingIdx.end(),
                  [&](size_t a, size_t b)
                  {
                      // Sort by rotationOrder, then by lowest item order.
                      if (bucketRotation[a] != bucketRotation[b])
                          return bucketRotation[a] < bucketRotation[b];
                      return bucketLowestOrder[a] < bucketLowestOrder[b];
                  });

        snap.singers.reserve(upcomingIdx.size());
        int newOrder = 0;
        for (auto idx : upcomingIdx)
        {
            auto s = std::move(buckets[idx]);
            s.order = newOrder++;
            snap.singers.push_back(std::move(s));
        }

        if (nowPlayingBucket >= 0)
        {
            snap.nowPlaying    = std::move(buckets[(size_t) nowPlayingBucket]);
            snap.hasNowPlaying = true;
        }

        return snap;
    }
}

void QueueService::loadQueue(const juce::String& venueId, LoadCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone)
            juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        const auto path = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(path, 200);

        auto snap = buildSnapshot(docs);

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, snap = std::move(snap)]() mutable
            {
                onDone(true, std::move(snap), {});
            });
        }
    });
}
