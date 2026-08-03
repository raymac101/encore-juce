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
#include <unordered_map>

QueueService& QueueService::getInstance()
{
    static QueueService instance;
    return instance;
}

namespace
{
    juce::String normalizedSingerKey(const juce::String& singerName)
    {
        auto key = singerName.trim().toLowerCase();
        key = key.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789");
        return key.isNotEmpty() ? key : juce::String("unknown");
    }

    juce::String makeManualSingerDocId(const QueueItem& item)
    {
        const auto singerKey = normalizedSingerKey(juce::String(item.singerName));
        const auto deviceKey = juce::String(item.deviceId).trim().toLowerCase();
        const auto hashSeed = singerKey + "|" + deviceKey;

        const auto hashValue = (juce::int64) std::hash<std::string>{}(hashSeed.toStdString());
        const auto hashHex = juce::String::toHexString(hashValue < 0 ? -hashValue : hashValue);

        return "manual-" + singerKey.substring(0, 24) + "-" + hashHex;
    }

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

        // isHost is stored as a booleanValue field on the host's queue doc.
        auto isHostField = fieldByName(fields, "isHost");
        if (isHostField.hasProperty("booleanValue"))
            s.isHost = (bool) isHostField.getProperty("booleanValue", false);

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

        // Sort by stable RR order (host always first, regardless of its
        // stored order value -- heals any historical data where that wasn't
        // enforced), with deterministic tie-breakers so duplicate/missing
        // order values do not reshuffle singers on restart. `order` is the
        // persisted Round Robin position and must NOT be rewritten here --
        // only join/removal/manual-reorder are allowed to change it (see
        // QueueRotation.h). Callers derive the rotated display queue (and
        // restamp `rotationOrder`) themselves via QueueRotation.
        std::sort(all.begin(), all.end(),
                  [](const Singers& a, const Singers& b)
                  {
                      if (a.isHost != b.isHost) return a.isHost;
                      if (a.order != b.order) return a.order < b.order;
                      if (a.rotationOrder != b.rotationOrder) return a.rotationOrder < b.rotationOrder;
                      return juce::String(a.name).toLowerCase() < juce::String(b.name).toLowerCase();
                  });

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

    // Find a queue doc by its Firestore document ID (last path segment).
    // Used for auth singers whose doc ID == their profileId (auth UID).
    FoundSinger findSingerByDocId(const juce::Array<juce::var>& docs,
                                  const juce::String& docId)
    {
        FoundSinger out;
        if (docId.isEmpty())
            return out;

        const auto target = docId.trim().toLowerCase();
        for (auto& d : docs)
        {
            const auto fullName = d.getProperty("name", "").toString();
            const auto id = fullName.fromLastOccurrenceOf("/", false, false).trim().toLowerCase();
            if (id == target)
            {
                out.docName = fullName;
                out.singer  = singerFromDoc(d);
                return out;
            }
        }
        return out;
    }

    // Find a singer by profileId field (Firebase auth UID).
    FoundSinger findSingerByProfileId(const juce::Array<juce::var>& docs,
                                      const juce::String& profileId)
    {
        FoundSinger out;
        if (profileId.isEmpty())
            return out;

        const auto target = profileId.trim().toLowerCase();
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            const auto profile = valueAsString(fieldByName(fields, "profileId")).trim().toLowerCase();
            if (profile == target)
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

void QueueService::ensureHostQueueDoc(const juce::String& venueId,
                                      const juce::String& authUid,
                                      const juce::String& stageName,
                                      const juce::String& avatarUrl,
                                      WriteCallback onDone)
{
    if (venueId.isEmpty() || authUid.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing venueId or authUid"); });
        return;
    }

    juce::Thread::launch([this, venueId, authUid, stageName, avatarUrl, onDone = std::move(onDone)]()
    {
        const juce::ScopedLock lock(writeLock_);

        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

        // If a doc already exists for this auth UID, nothing to do.
        auto existing = findSingerByDocId(docs, authUid);
        if (existing.docName.isEmpty())
            existing = findSingerByProfileId(docs, authUid);
        if (existing.docName.isNotEmpty())
        {
            DBG ("[Queue] ensureHostQueueDoc: host doc already exists for '" << authUid << "'");
            if (onDone) juce::MessageManager::callAsync([onDone] { onDone(true, {}); });
            return;
        }

        // Create a permanent host singer doc with no songs, pinned at order 0.
        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty("id",             FirestoreClient::stringValue(authUid));
        fields->setProperty("name",           FirestoreClient::stringValue(stageName));
        fields->setProperty("avatar",         FirestoreClient::stringValue(avatarUrl));
        fields->setProperty("deviceId",       FirestoreClient::stringValue(juce::String("host")));
        fields->setProperty("profileId",      FirestoreClient::stringValue(authUid));
        fields->setProperty("foxId",          FirestoreClient::stringValue(juce::String()));
        fields->setProperty("status",         FirestoreClient::stringValue("active"));
        fields->setProperty("isHost",         FirestoreClient::booleanValue(true));
        fields->setProperty("order",          FirestoreClient::integerValue(0));
        fields->setProperty("rotationOrder",  FirestoreClient::integerValue(-1));
        fields->setProperty("strikes",        FirestoreClient::integerValue(0));
        fields->setProperty("songsPerformed", FirestoreClient::integerValue(0));

        // Empty songs array
        juce::DynamicObject::Ptr arrInner = new juce::DynamicObject();
        arrInner->setProperty("values", juce::var(juce::Array<juce::var>{}));
        juce::DynamicObject::Ptr arrWrapper = new juce::DynamicObject();
        arrWrapper->setProperty("arrayValue", juce::var(arrInner.get()));
        fields->setProperty("songs", juce::var(arrWrapper.get()));

        bool ok = false;
        FirestoreClient::getInstance()
            .createDocument(collPath, juce::var(fields.get()), authUid, &ok);

        DBG ("[Queue] ensureHostQueueDoc created host doc '" << authUid << "' ok=" << (ok ? 1 : 0));

        if (onDone) juce::MessageManager::callAsync([onDone, ok]
            { onDone(ok, ok ? juce::String() : juce::String("createDocument failed")); });
    });
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

    juce::Thread::launch([this, venueId, item, onDone = std::move(onDone)]()
    {
        const juce::ScopedLock lock(writeLock_);

        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

        // For auth singers (profileId == auth UID), look up by doc ID first
        // so we can find a host slot that exists in Firestore even when its
        // display name hasn't been resolved yet.
        const juce::String profileId = juce::String(item.profileId).trim();
        auto found = profileId.isNotEmpty()
                   ? findSingerByDocId(docs, profileId)
                   : FoundSinger{};
        if (found.docName.isEmpty() && profileId.isNotEmpty())
            found = findSingerByProfileId(docs, profileId);
        if (found.docName.isEmpty())
            found = findSingerByName(docs, juce::String(item.singerName));

        if (found.docName.isNotEmpty())
        {
            // Existing singer — append the song and PATCH the songs field.
            auto songs = found.singer.songs;
            QueueItem copy = item;
            copy.songOrder = (int) songs.size();
            copy.status = copy.status.empty() ? "queued" : copy.status;
            songs.push_back(copy);

            const auto rel = relPathFromDocName(found.docName);
            const auto maskedPath = rel + "?updateMask.fieldPaths=songs&updateMask.fieldPaths=strikes";

            juce::DynamicObject::Ptr fields = new juce::DynamicObject();
            fields->setProperty("songs", songsArrayValue(songs));
            // Singer added a song again — clear accumulated skip strikes.
            fields->setProperty("strikes", FirestoreClient::integerValue(0));

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

        // Queue singer docs use one canonical ID policy:
        // - Auth singers: their Firebase auth UID (`profileId`)
        // - Manual singers: deterministic namespaced ID (`manual-*`)
        const bool hasProfileId = profileId.isNotEmpty()
                       && profileId.compareIgnoreCase("unknown") != 0;

        const juce::String docId = hasProfileId
                                 ? profileId
                                 : makeManualSingerDocId(item);
        const juce::String storedProfileId = hasProfileId ? profileId : docId;

        juce::DynamicObject::Ptr fields = new juce::DynamicObject();
        fields->setProperty("id",             FirestoreClient::stringValue(docId));
        fields->setProperty("name",           FirestoreClient::stringValue(juce::String(item.singerName)));
        fields->setProperty("avatar",         FirestoreClient::stringValue(juce::String(item.singerAvatar)));
        fields->setProperty("deviceId",       FirestoreClient::stringValue(juce::String(item.deviceId)));
        fields->setProperty("profileId",      FirestoreClient::stringValue(storedProfileId));
        fields->setProperty("foxId",          FirestoreClient::stringValue(juce::String(item.foxId)));
        fields->setProperty("status",         FirestoreClient::stringValue("queued"));
        // `order` is the stable RR position -- appended to the bottom.
        // `rotationOrder` is a derived display-rank cache (restamped by
        // QueueRotation::stampDerivedRanks() the next time anything
        // reloads/reorders); it must NOT be hardcoded to the new RR
        // position here, or a new singer would appear to BE the anchor.
        fields->setProperty("order",          FirestoreClient::integerValue(maxOrder + 1));
        fields->setProperty("strikes",        FirestoreClient::integerValue(0));
        fields->setProperty("songsPerformed", FirestoreClient::integerValue(0));
        fields->setProperty("songs",          songsArrayValue(initialSongs));

        bool ok = false;
        FirestoreClient::getInstance()
            .createDocument(collPath, juce::var(fields.get()), docId, &ok);

        DBG ("[Queue] appendSong new singer '" << juce::String(item.singerName)
               << "' docId='" << docId << "'"
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

    juce::Thread::launch([this, venueId, item, onDone = std::move(onDone)]()
    {
        const juce::ScopedLock lock(writeLock_);

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
                                const juce::String& singerNameOrDocId,
                                WriteCallback onDone)
{
    if (venueId.isEmpty() || singerNameOrDocId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "missing arg"); });
        return;
    }

    juce::Thread::launch([this, venueId, singerNameOrDocId, onDone = std::move(onDone)]()
    {
        const juce::ScopedLock lock(writeLock_);

        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 200);

        // Callers pass either the singer's Firestore doc ID (auth singers --
        // e.g. MainComponent::onRemoveSinger) or their display name (e.g.
        // the strike-out removal in queueAndLoadNextSingerSong). Try doc ID
        // first since a name can legitimately collide with another field,
        // but a doc ID match is unambiguous.
        auto found = findSingerByDocId(docs, singerNameOrDocId);
        if (found.docName.isEmpty())
            found = findSingerByName(docs, singerNameOrDocId);
        if (found.docName.isEmpty())
        {
            DBG ("[Queue] deleteSinger: singer '" << singerNameOrDocId << "' not found");
            if (onDone)
                juce::MessageManager::callAsync([onDone] { onDone(false, "singer not found"); });
            return;
        }

        const auto rel = relPathFromDocName(found.docName);
        const bool ok = FirestoreClient::getInstance().deleteDocument(rel);

        DBG ("[Queue] deleteSinger singer='" << singerNameOrDocId
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

    juce::Thread::launch([this, venueId, singerName, newSongs, onDone = std::move(onDone)]() mutable
    {
        const juce::ScopedLock lock(writeLock_);

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

void QueueService::persistSingerOrder(const juce::String& venueId,
                                      const std::vector<Singers>& orderedSingers,
                                      WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([this, venueId, orderedSingers, onDone = std::move(onDone)]()
    {
        const juce::ScopedLock lock(writeLock_);

        const auto collPath = "venues/" + venueId + "/queue";
        auto docs = FirestoreClient::getInstance().listCollection(collPath, 300);

        std::unordered_map<std::string, juce::String> relPathByDocId;
        std::unordered_map<std::string, juce::String> relPathByName;
        relPathByDocId.reserve((size_t) docs.size());
        relPathByName.reserve((size_t) docs.size());

        for (auto& d : docs)
        {
            const auto fullName = d.getProperty("name", juce::var()).toString();
            const auto relPath  = relPathFromDocName(fullName);
            const auto docId    = fullName.fromLastOccurrenceOf("/", false, false).trim();
            const auto fields   = d.getProperty("fields", juce::var());
            const auto name     = valueAsString(fieldByName(fields, "name")).trim().toLowerCase();

            if (docId.isNotEmpty() && relPath.isNotEmpty())
                relPathByDocId[docId.toStdString()] = relPath;
            if (name.isNotEmpty() && relPath.isNotEmpty())
                relPathByName[name.toStdString()] = relPath;
        }

        bool allOk = true;
        int writeOrder = 0;
        int patched = 0;

        for (const auto& singer : orderedSingers)
        {

            juce::String relPath;

            const auto docId = juce::String(singer.id).trim();
            if (docId.isNotEmpty())
            {
                auto byId = relPathByDocId.find(docId.toStdString());
                if (byId != relPathByDocId.end())
                    relPath = byId->second;
            }

            if (relPath.isEmpty())
            {
                const auto key = juce::String(singer.name).trim().toLowerCase();
                auto byName = relPathByName.find(key.toStdString());
                if (byName != relPathByName.end())
                    relPath = byName->second;
            }

            if (relPath.isNotEmpty())
            {
                const int rotationToWrite = singer.rotationOrder >= 0
                                          ? singer.rotationOrder
                                          : writeOrder;

                auto fields = FirestoreClient::makeFields({
                    { "order", FirestoreClient::integerValue(writeOrder) },
                    { "rotationOrder", FirestoreClient::integerValue(rotationToWrite) },
                    { "strikes", FirestoreClient::integerValue(juce::jmax(0, singer.strikes)) }
                });

                const auto patchPath = relPath
                    + "?updateMask.fieldPaths=order&updateMask.fieldPaths=rotationOrder&updateMask.fieldPaths=strikes"
                    + "&currentDocument.exists=true";

                const bool ok = FirestoreClient::getInstance().patchDocument(patchPath, fields);
                allOk = allOk && ok;
                ++patched;
            }
            else
            {
                allOk = false;
            }

            ++writeOrder;
        }

        DBG ("[Queue] persistSingerOrder singers=" << (int) orderedSingers.size()
             << " patched=" << patched
             << " ok=" << (allOk ? 1 : 0));

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, allOk]
            {
                onDone(allOk, allOk ? juce::String() : juce::String("persistSingerOrder partial failure"));
            });
        }
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
