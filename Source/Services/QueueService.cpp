/*
  ==============================================================================

    QueueService.cpp

    Each Firestore doc under venues/{venueId}/queue is a Singers record
    (matches src/app/models/singers.model.ts) with top-level fields
    `id, profileId, deviceId, foxId, name, avatar, status, order,
    rotationOrder, time, reason, songs[], strikes, songsPerformed`.
    The `songs` field is an arrayValue of mapValues — each map is a
    QueueItem (matches src/app/models/queueItem.model.ts).

  ==============================================================================
*/

#include "QueueService.h"
#include "FirestoreClient.h"
#include <algorithm>

QueueService& QueueService::getInstance()
{
    static QueueService instance;
    return instance;
}

namespace
{
    //--- Firestore typed-value readers (work on a `valueObj` which is the
    //    inner { stringValue: ..., integerValue: ..., ... } wrapper). ----

    juce::String valueAsString(const juce::var& v)
    {
        if (v.hasProperty("stringValue"))    return v.getProperty("stringValue", "").toString();
        if (v.hasProperty("integerValue"))   return v.getProperty("integerValue", "").toString();
        if (v.hasProperty("doubleValue"))    return juce::String((double) v.getProperty("doubleValue", 0.0));
        if (v.hasProperty("booleanValue"))   return ((bool) v.getProperty("booleanValue", false)) ? "true" : "false";
        if (v.hasProperty("timestampValue")) return v.getProperty("timestampValue", "").toString();
        return {};
    }

    int valueAsInt(const juce::var& v, int dflt = 0)
    {
        if (v.hasProperty("integerValue"))
            return (int) v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("doubleValue"))
            return (int) (double) v.getProperty("doubleValue", 0.0);
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "").toString().getIntValue();
        return dflt;
    }

    double valueAsDouble(const juce::var& v, double dflt = 0.0)
    {
        if (v.hasProperty("doubleValue"))
            return (double) v.getProperty("doubleValue", dflt);
        if (v.hasProperty("integerValue"))
            return (double) v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "").toString().getDoubleValue();
        return dflt;
    }

    bool valueAsBool(const juce::var& v, bool dflt = false)
    {
        if (v.hasProperty("booleanValue"))
            return (bool) v.getProperty("booleanValue", dflt);
        if (v.hasProperty("stringValue"))
        {
            auto s = v.getProperty("stringValue", "").toString().toLowerCase();
            return s == "true" || s == "1" || s == "yes";
        }
        return dflt;
    }

    juce::var fieldByName(const juce::var& fields, const juce::String& name)
    {
        return fields.getProperty(juce::Identifier(name), juce::var());
    }

    // Convert a mapValue (which has its own .fields wrapper) into a QueueItem.
    QueueItem itemFromMap(const juce::var& mapValue)
    {
        auto fields = mapValue.getProperty("fields", juce::var());

        QueueItem q;
        q.id           = valueAsString(fieldByName(fields, "id")).toStdString();
        q.deviceId     = valueAsString(fieldByName(fields, "deviceId")).toStdString();
        q.singerName   = valueAsString(fieldByName(fields, "singerName")).toStdString();
        q.singerAvatar = valueAsString(fieldByName(fields, "avatar")).toStdString();
        q.songId       = valueAsString(fieldByName(fields, "songId")).toStdString();
        q.songName     = valueAsString(fieldByName(fields, "song")).toStdString();
        q.songArtist   = valueAsString(fieldByName(fields, "artist")).toStdString();
        q.songVersion  = valueAsString(fieldByName(fields, "songVersion")).toStdString();
        q.duration     = valueAsInt   (fieldByName(fields, "duration"));
        q.order        = valueAsInt   (fieldByName(fields, "order"));
        q.songOrder    = valueAsInt   (fieldByName(fields, "songOrder"));
        q.pitch        = (float) valueAsDouble(fieldByName(fields, "pitch"), 1.0);
        q.status       = valueAsString(fieldByName(fields, "status")).toStdString();
        q.time         = valueAsString(fieldByName(fields, "time")).toStdString();
        q.alerts       = valueAsBool  (fieldByName(fields, "addedAlert"))
                       || valueAsBool (fieldByName(fields, "singingAlert"))
                       || valueAsBool (fieldByName(fields, "nextAlert"));
        return q;
    }

    // Convert one Firestore queue document into a Singers.
    Singers singerFromDoc(const juce::var& doc)
    {
        auto fields = doc.getProperty("fields", juce::var());

        Singers s;
        s.id            = valueAsString(fieldByName(fields, "id")).toStdString();
        s.name          = valueAsString(fieldByName(fields, "name")).toStdString();
        s.avatar        = valueAsString(fieldByName(fields, "avatar")).toStdString();
        s.deviceId      = valueAsString(fieldByName(fields, "deviceId")).toStdString();
        s.order         = valueAsInt   (fieldByName(fields, "order"));
        s.rotationOrder = valueAsInt   (fieldByName(fields, "rotationOrder"));
        s.strikes       = valueAsInt   (fieldByName(fields, "strikes"));
        s.songsPerformed= valueAsInt   (fieldByName(fields, "songsPerformed"));

        // songs: arrayValue → values[] → each is a mapValue.
        auto songsField = fieldByName(fields, "songs");
        auto arr = songsField.getProperty("arrayValue", juce::var())
                             .getProperty("values", juce::var());
        if (auto* a = arr.getArray())
        {
            s.songs.reserve((size_t) a->size());
            for (auto& v : *a)
            {
                if (v.hasProperty("mapValue"))
                    s.songs.push_back(itemFromMap(v.getProperty("mapValue", juce::var())));
            }
        }

        // Sort the singer's songs by songOrder.
        std::sort(s.songs.begin(), s.songs.end(),
                  [](const QueueItem& a, const QueueItem& b) {
                      return a.songOrder < b.songOrder;
                  });

        // Detect now-playing — either status on the singer doc itself or on
        // any of its songs.
        const auto singerStatus = valueAsString(fieldByName(fields, "status")).toLowerCase();
        s.currentlyUp = (singerStatus == "playing");
        if (! s.currentlyUp)
        {
            for (auto& q : s.songs)
            {
                if (juce::String(q.status).toLowerCase() == "playing")
                {
                    s.currentlyUp = true;
                    break;
                }
            }
        }

        return s;
    }

    QueueService::Snapshot buildSnapshot(const juce::Array<juce::var>& docs)
    {
        QueueService::Snapshot snap;

        std::vector<Singers> all;
        all.reserve((size_t) docs.size());
        for (auto& d : docs)
        {
            auto s = singerFromDoc(d);
            // Skip empty skeleton docs.
            if (s.name.empty() && s.songs.empty())
                continue;
            all.push_back(std::move(s));
        }

        // Sort by `order` (Angular's sortQueueByOrder).
        std::sort(all.begin(), all.end(),
                  [](const Singers& a, const Singers& b) { return a.order < b.order; });

        int nowIdx = -1;
        for (size_t i = 0; i < all.size(); ++i)
        {
            if (all[i].currentlyUp) { nowIdx = (int) i; break; }
        }

        snap.singers.reserve(all.size());
        for (size_t i = 0; i < all.size(); ++i)
        {
            if ((int) i == nowIdx)
            {
                snap.nowPlaying    = all[i];
                snap.hasNowPlaying = true;
            }
            else
            {
                snap.singers.push_back(std::move(all[i]));
            }
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

        DBG ("[Queue] loaded " << docs.size() << " docs from " << path);

        auto snap = buildSnapshot(docs);

        DBG ("[Queue] parsed singers=" << (int) snap.singers.size()
             << " nowPlaying=" << (snap.hasNowPlaying ? "yes" : "no"));

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, snap = std::move(snap)]() mutable
            {
                onDone(true, std::move(snap), {});
            });
        }
    });
}
