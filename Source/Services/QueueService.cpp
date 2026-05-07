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
        const auto docName = doc.getProperty("name", "").toString();
        const auto docId = docName.fromLastOccurrenceOf("/", false, false);

        Singers s;
        s.id            = valueAsString(fieldByName(fields, "id")).toStdString();
        if (docId.isNotEmpty())
            s.id = docId.toStdString();
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

        // Self-heal inconsistent ordering from older writes:
        // keep queue order contiguous and force a single unique tail marker.
        for (size_t i = 0; i < all.size(); ++i)
        {
            all[i].order = (int) i;
            all[i].rotationOrder = (int) i;
        }

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

//==============================================================================
namespace
{
    // Encode a QueueItem back into a Firestore mapValue (the inverse of
    // itemFromMap above). We deliberately only set the fields the desktop
    // / mobile clients actually populate — leaving anything else untouched.
    juce::var queueItemToMapValue(const QueueItem& q)
    {
        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        auto put = [&](const char* key, juce::var v) { fields->setProperty(key, v); };

        put("id",          FirestoreClient::stringValue(juce::String(q.id)));
        put("deviceId",    FirestoreClient::stringValue(juce::String(q.deviceId)));
        put("profileId",   FirestoreClient::stringValue(juce::String(q.profileId)));
        put("foxId",       FirestoreClient::stringValue(juce::String(q.foxId)));
        put("singerName",  FirestoreClient::stringValue(juce::String(q.singerName)));
        put("avatar",      FirestoreClient::stringValue(juce::String(q.singerAvatar)));
        put("songId",      FirestoreClient::stringValue(juce::String(q.songId)));
        put("song",        FirestoreClient::stringValue(juce::String(q.songName)));
        put("artist",      FirestoreClient::stringValue(juce::String(q.songArtist)));
        put("songVersion", FirestoreClient::stringValue(juce::String(q.songVersion)));
        put("status",      FirestoreClient::stringValue(juce::String(q.status)));
        put("time",        FirestoreClient::stringValue(juce::String(q.time)));
        put("duration",    FirestoreClient::integerValue(q.duration));
        put("order",       FirestoreClient::integerValue(q.order));
        put("songOrder",   FirestoreClient::integerValue(q.songOrder));
        put("pitch",       FirestoreClient::doubleValue(q.pitch));

        juce::DynamicObject::Ptr map = new juce::DynamicObject();
        map->setProperty("fields", juce::var(fields.get()));

        juce::DynamicObject::Ptr wrapper = new juce::DynamicObject();
        wrapper->setProperty("mapValue", juce::var(map.get()));
        return juce::var(wrapper.get());
    }

    // Wrap an array of mapValue vars into a Firestore arrayValue.
    juce::var songsArrayValue(const std::vector<QueueItem>& songs)
    {
        juce::Array<juce::var> values;
        values.ensureStorageAllocated((int) songs.size());
        for (auto& s : songs)
            values.add(queueItemToMapValue(s));

        juce::DynamicObject::Ptr arr = new juce::DynamicObject();
        arr->setProperty("values", juce::var(values));

        juce::DynamicObject::Ptr wrapper = new juce::DynamicObject();
        wrapper->setProperty("arrayValue", juce::var(arr.get()));
        return juce::var(wrapper.get());
    }

    // Read all queue docs and pick the singer matching `singerName`
    // case-insensitively. Returns (docName, songs) or empty docName if none.
    struct FoundSinger
    {
        juce::String docName;             // "projects/.../venues/X/queue/<id>"
        Singers      singer;
    };

    FoundSinger findSingerByName(const juce::Array<juce::var>& docs,
                                 const juce::String& singerName)
    {
        FoundSinger out;
        if (singerName.isEmpty())
            return out;

        const auto target = singerName.toLowerCase();
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            const auto name = valueAsString(fieldByName(fields, "name")).toLowerCase();
            if (name == target)
            {
                out.docName = d.getProperty("name", "").toString();
                out.singer  = singerFromDoc(d);
                return out;
            }
        }
        return out;
    }

    juce::String relPathFromDocName(const juce::String& docName)
    {
        // "projects/X/databases/(default)/documents/<rel>" -> "<rel>"
        const auto marker = juce::String("/documents/");
        auto idx = docName.indexOf(marker);
        if (idx < 0) return docName;
        return docName.substring(idx + marker.length());
    }
}

void QueueService::appendSong(const juce::String& venueId,
                              const QueueItem& item,
                              WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, item, onDone = std::move(onDone)]()
    {
        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

        auto found = findSingerByName(docs, juce::String(item.singerName));

        if (found.docName.isNotEmpty())
        {
            // Existing singer — append the song and PATCH the songs field.
            auto songs = found.singer.songs;
            QueueItem copy = item;
            copy.songOrder = (int) songs.size();
            copy.status = copy.status.empty() ? "queued" : copy.status;
            songs.push_back(copy);

            const auto rel = relPathFromDocName(found.docName);
            const auto maskedPath = rel + "?updateMask.fieldPaths=songs";

            juce::DynamicObject::Ptr fields = new juce::DynamicObject();
            fields->setProperty("songs", songsArrayValue(songs));

            const bool ok = FirestoreClient::getInstance()
                                .patchDocument(maskedPath, juce::var(fields.get()));

            DBG ("[Queue] appendSong existing singer '" << juce::String(item.singerName)
                 << "' songs=" << (int) songs.size() << " ok=" << (ok ? 1 : 0));

            if (onDone)
                juce::MessageManager::callAsync([onDone, ok]
                    { onDone(ok, ok ? juce::String() : juce::String("PATCH failed")); });
            return;
        }

        // New singer — create a fresh /queue doc with this song.
        // Order placement: end of current queue.
        int maxOrder = -1;
        for (auto& d : docs)
        {
            auto f = d.getProperty("fields", juce::var());
            maxOrder = juce::jmax(maxOrder, valueAsInt(fieldByName(f, "order")));
        }

        QueueItem first = item;
        first.songOrder = 0;
        first.order     = 0;
        first.status    = first.status.empty() ? "queued" : first.status;

        std::vector<QueueItem> initialSongs { first };

        // Determine the document ID to use for this singer's queue doc.
        // Mobile users provide profileId; KJ-added singers may have no
        // profile ID (or "Unknown"). For those, generate a stable unique
        // queue doc ID so reorder/update calls can always target this singer.
        const juce::String profileId = juce::String(item.profileId).trim();
        const bool hasProfileId = profileId.isNotEmpty()
                       && profileId.compareIgnoreCase("unknown") != 0;
        juce::String docId = hasProfileId
                   ? profileId
                   : ("kj-" + juce::Uuid().toString().removeCharacters("{}-"));

        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty("id",             FirestoreClient::stringValue(docId));
        fields->setProperty("name",           FirestoreClient::stringValue(juce::String(item.singerName)));
        fields->setProperty("avatar",         FirestoreClient::stringValue(juce::String(item.singerAvatar)));
        fields->setProperty("deviceId",       FirestoreClient::stringValue(juce::String(item.deviceId)));
        fields->setProperty("profileId",      FirestoreClient::stringValue(hasProfileId ? profileId : juce::String("Unknown")));
        fields->setProperty("foxId",          FirestoreClient::stringValue(juce::String(item.foxId)));
        fields->setProperty("status",         FirestoreClient::stringValue("queued"));
        fields->setProperty("order",          FirestoreClient::integerValue(maxOrder + 1));
        fields->setProperty("rotationOrder",  FirestoreClient::integerValue(maxOrder + 1));
        fields->setProperty("strikes",        FirestoreClient::integerValue(-1));
        fields->setProperty("songsPerformed", FirestoreClient::integerValue(0));
        fields->setProperty("songs",          songsArrayValue(initialSongs));

        auto resp = FirestoreClient::getInstance()
                        .createDocument(collPath, juce::var(fields.get()), docId);
        const bool ok = resp.isObject();

        DBG ("[Queue] appendSong new singer '" << juce::String(item.singerName)
             << "' ok=" << (ok ? 1 : 0));

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]
                { onDone(ok, ok ? juce::String() : juce::String("createDocument failed")); });
    });
}

void QueueService::removeSong(const juce::String& venueId,
                              const QueueItem& item,
                              WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, item, onDone = std::move(onDone)]()
    {
        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

        auto found = findSingerByName(docs, juce::String(item.singerName));
        if (found.docName.isEmpty())
        {
            DBG ("[Queue] removeSong: singer '" << juce::String(item.singerName)
                 << "' not found");
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(false, "singer not found"); });
            return;
        }

        std::vector<QueueItem> kept;
        kept.reserve(found.singer.songs.size());

        const auto wantId     = juce::String(item.songId);
        const auto wantSong   = juce::String(item.songName).toLowerCase();
        const auto wantArtist = juce::String(item.songArtist).toLowerCase();

        bool removed = false;
        for (auto& s : found.singer.songs)
        {
            const bool matchById   = wantId.isNotEmpty() && juce::String(s.songId) == wantId;
            const bool matchByText = ! matchById
                                  && juce::String(s.songName).toLowerCase()   == wantSong
                                  && juce::String(s.songArtist).toLowerCase() == wantArtist;
            if (! removed && (matchById || matchByText))
            {
                removed = true;
                continue;
            }
            kept.push_back(s);
        }

        // Re-number songOrder so positions stay sequential.
        for (size_t i = 0; i < kept.size(); ++i)
            kept[i].songOrder = (int) i;

        const auto rel = relPathFromDocName(found.docName);
        const auto maskedPath = rel + "?updateMask.fieldPaths=songs";

        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty("songs", songsArrayValue(kept));

        const bool ok = FirestoreClient::getInstance()
                            .patchDocument(maskedPath, juce::var(fields.get()));

        DBG ("[Queue] removeSong singer='" << juce::String(item.singerName)
             << "' song='" << juce::String(item.songName)
             << "' removed=" << (removed ? 1 : 0)
             << " songsLeft=" << (int) kept.size()
             << " ok=" << (ok ? 1 : 0));

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]
                { onDone(ok, ok ? juce::String() : juce::String("PATCH failed")); });
    });
}

void QueueService::deleteSinger(const juce::String& venueId,
                                const juce::String& singerName,
                                WriteCallback onDone)
{
    if (venueId.isEmpty() || singerName.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "missing arg"); });
        return;
    }

    juce::Thread::launch([venueId, singerName, onDone = std::move(onDone)]()
    {
        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

        auto found = findSingerByName(docs, singerName);
        if (found.docName.isEmpty())
        {
            DBG ("[Queue] deleteSinger: singer '" << singerName << "' not found");
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(false, "singer not found"); });
            return;
        }

        const auto rel = relPathFromDocName(found.docName);
        const bool ok = FirestoreClient::getInstance().deleteDocument(rel);

        DBG ("[Queue] deleteSinger singer='" << singerName
             << "' ok=" << (ok ? 1 : 0));

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]
                { onDone(ok, ok ? juce::String() : juce::String("DELETE failed")); });
    });
}

void QueueService::patchSingerSongs(const juce::String& venueId,
                                    const juce::String& singerName,
                                    const std::vector<QueueItem>& newSongs,
                                    WriteCallback onDone)
{
    if (venueId.isEmpty() || singerName.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "missing arg"); });
        return;
    }

    juce::Thread::launch([venueId, singerName, newSongs, onDone = std::move(onDone)]() mutable
    {
        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

        auto found = findSingerByName(docs, singerName);
        if (found.docName.isEmpty())
        {
            DBG ("[Queue] patchSingerSongs: singer '" << singerName << "' not found");
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(false, "singer not found"); });
            return;
        }

        // Make a local mutable copy so we can renumber songOrder regardless
        // of how `juce::Thread::launch` invokes the lambda body.
        std::vector<QueueItem> outSongs = newSongs;
        for (size_t i = 0; i < outSongs.size(); ++i)
            outSongs[i].songOrder = (int) i;

        const auto rel = relPathFromDocName(found.docName);
        const auto maskedPath = rel + "?updateMask.fieldPaths=songs";

        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty("songs", songsArrayValue(outSongs));

        const bool ok = FirestoreClient::getInstance()
                            .patchDocument(maskedPath, juce::var(fields.get()));

        DBG ("[Queue] patchSingerSongs singer='" << singerName
             << "' songs=" << (int) outSongs.size()
             << " ok=" << (ok ? 1 : 0));

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]
                { onDone(ok, ok ? juce::String() : juce::String("PATCH failed")); });
    });
}

//==============================================================================
namespace
{
    // Build a string fingerprint that captures every queue field we care
    // about — singer ordering, song ordering inside each singer, status
    // markers, etc. Any change in this string means we should reload.
    juce::String fingerprintFromSnapshot(const QueueService::Snapshot& snap)
    {
        juce::String out;
        out.preallocateBytes(1024);

        auto appendSinger = [&](const Singers& s)
        {
            out << "[" << juce::String(s.id)
                << "|" << juce::String(s.name)
                << "|o=" << s.order
                << "|r=" << s.rotationOrder
                << "|st=" << juce::String(s.currentlyUp ? "playing" : "")
                << "|songs:";
            for (auto& q : s.songs)
                out << "(" << juce::String(q.songId)
                    << ":" << juce::String(q.songName)
                    << ":" << juce::String(q.songArtist)
                    << ":so=" << q.songOrder
                    << ":st=" << juce::String(q.status)
                    << ")";
            out << "]";
        };

        if (snap.hasNowPlaying)
        {
            out << "NOW=";
            appendSinger(snap.nowPlaying);
        }
        for (auto& s : snap.singers)
            appendSinger(s);

        return out;
    }
}

void QueueService::startWatching(const juce::String& venueId, ChangeCallback onChange)
{
    if (venueId.isEmpty())
        return;

    if (watching_ && venueId == watchVenueId_)
    {
        onChange_ = std::move(onChange);
        return;
    }

    stopWatching();

    watchVenueId_   = venueId;
    onChange_       = std::move(onChange);
    watching_       = true;
    lastFingerprint_.clear();

    DBG ("[Queue] watcher start for venues/" << watchVenueId_ << "/queue every "
         << watchIntervalMs_ << "ms");

    juce::Timer::startTimer(watchIntervalMs_);
    pollWatcher();
}

void QueueService::stopWatching()
{
    if (! watching_) return;
    watching_ = false;
    juce::Timer::stopTimer();
    onChange_ = {};
    lastFingerprint_.clear();
    watchVenueId_.clear();
    DBG ("[Queue] watcher stop");
}

void QueueService::timerCallback()
{
    pollWatcher();
}

void QueueService::pollWatcher()
{
    if (! watching_ || watchInFlight_ || watchVenueId_.isEmpty())
        return;

    watchInFlight_ = true;
    const auto venueId = watchVenueId_;

    juce::Thread::launch([this, venueId]
    {
        const auto path = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(path, 200);
        auto snap = buildSnapshot(docs);

        juce::MessageManager::callAsync([this, venueId, snap = std::move(snap)]() mutable
        {
            watchInFlight_ = false;

            if (! watching_ || venueId != watchVenueId_)
                return;

            const auto fp = fingerprintFromSnapshot(snap);
            if (fp == lastFingerprint_)
                return;

            DBG ("[Queue] watcher detected change");
            lastFingerprint_ = fp;
            if (onChange_)
                onChange_(std::move(snap));
        });
    });
}
