/*
  ==============================================================================

    VenueService.cpp

  ==============================================================================
*/

#include "VenueService.h"
#include "FirestoreClient.h"
#include <algorithm>

VenueService& VenueService::getInstance()
{
    static VenueService instance;
    return instance;
}

//==============================================================================
namespace
{
    using FC = FirestoreClient;

    // ── Firestore typed-value readers ────────────────────────────────────────
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

    juce::int64 valueAsInt64(const juce::var& v, juce::int64 dflt = 0)
    {
        if (v.hasProperty("integerValue"))
            return v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("doubleValue"))
            return (juce::int64) (double) v.getProperty("doubleValue", 0.0);
        if (v.hasProperty("stringValue"))
            return v.getProperty("stringValue", "").toString().getLargeIntValue();
        return dflt;
    }

    double valueAsDouble(const juce::var& v, double dflt = 0.0)
    {
        if (v.hasProperty("doubleValue"))    return (double) v.getProperty("doubleValue", dflt);
        if (v.hasProperty("integerValue"))   return (double) v.getProperty("integerValue", "").toString().getLargeIntValue();
        if (v.hasProperty("stringValue"))    return v.getProperty("stringValue", "").toString().getDoubleValue();
        return dflt;
    }

    bool valueAsBool(const juce::var& v, bool dflt = false)
    {
        if (v.hasProperty("booleanValue")) return (bool) v.getProperty("booleanValue", dflt);
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

    // Doc-name → relative path under /documents/.
    juce::String relPathFromDocName(const juce::String& docName)
    {
        const auto marker = juce::String("/documents/");
        auto idx = docName.indexOf(marker);
        return idx < 0 ? docName : docName.substring(idx + marker.length());
    }

    // Doc-name → final id segment.
    juce::String docIdFromName(const juce::String& docName)
    {
        return docName.fromLastOccurrenceOf("/", false, false);
    }

    // ── Bulk-delete helper: synchronously DELETE every doc in a collection.
    //   Runs in one thread; not transactional but safe for housekeeping.
    int deleteCollectionContents(const juce::String& collectionPath)
    {
        auto docs = FC::getInstance().listCollection(collectionPath, 500);
        int n = 0;
        for (auto& d : docs)
        {
            auto rel = relPathFromDocName(d.getProperty("name", "").toString());
            if (rel.isNotEmpty() && FC::getInstance().deleteDocument(rel))
                ++n;
        }
        return n;
    }

    // ── Playlist (de)serialisation ───────────────────────────────────────────
    // The Angular Playlist model has: id, imageUrl, songName, artistName.
    // The "new" feed also stores a dateAdded field for ordering.
    Playlist playlistFromDoc(const juce::var& doc)
    {
        auto fields = doc.getProperty("fields", juce::var());
        Playlist p;
        p.id         = valueAsString(fieldByName(fields, "id"))        .toStdString();
        p.imageUrl   = valueAsString(fieldByName(fields, "imageUrl"))  .toStdString();
        p.songName   = valueAsString(fieldByName(fields, "songName"))  .toStdString();
        p.artistName = valueAsString(fieldByName(fields, "artistName")).toStdString();
        return p;
    }

    juce::var playlistFields(const Playlist& p,
                             juce::int64 dateAddedMs = -1 /* skip when negative */)
    {
        std::vector<std::pair<juce::String, juce::var>> entries;
        entries.emplace_back("id",         FC::stringValue(juce::String(p.id)));
        entries.emplace_back("imageUrl",   FC::stringValue(juce::String(p.imageUrl)));
        entries.emplace_back("songName",   FC::stringValue(juce::String(p.songName)));
        entries.emplace_back("artistName", FC::stringValue(juce::String(p.artistName)));
        if (dateAddedMs >= 0)
            entries.emplace_back("dateAdded", FC::integerValue(dateAddedMs));

        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        for (auto& e : entries)
            root->setProperty(e.first, e.second);

        juce::DynamicObject::Ptr wrapper = new juce::DynamicObject();
        wrapper->setProperty("fields", juce::var(root.get()));
        // Caller wants the body to be { "fields": {...} } — return that var.
        // patchDocument and createDocument both expect the bare fields, so we
        // hand back just `juce::var(root.get())` and let the caller wrap.
        return juce::var(root.get());
    }

    // ── Emoji (de)serialisation ──────────────────────────────────────────────
    Emoji emojiFromDoc(const juce::var& doc)
    {
        auto fields = doc.getProperty("fields", juce::var());
        Emoji e;
        e.id        = docIdFromName(doc.getProperty("name", "").toString()).toStdString();
        e.emojiName = valueAsString(fieldByName(fields, "emojiName")).toStdString();
        e.sender    = valueAsString(fieldByName(fields, "sender"))   .toStdString();
        e.source    = valueAsString(fieldByName(fields, "source"))   .toStdString();
        e.xPos      = (float) valueAsDouble(fieldByName(fields, "xPos"));
        e.yPos      = (float) valueAsDouble(fieldByName(fields, "yPos"));
        e.speed     = (float) valueAsDouble(fieldByName(fields, "speed"), 1.0);
        e.alpha     = (float) valueAsDouble(fieldByName(fields, "alpha"), 1.0);
        e.alphaRate = (float) valueAsDouble(fieldByName(fields, "alphaRate"), 0.01);
        e.current   = valueAsInt(fieldByName(fields, "current"));
        return e;
    }

    // ── Playing (de)serialisation ────────────────────────────────────────────
    Playing playingFromDoc(const juce::var& doc)
    {
        auto fields = doc.getProperty("fields", juce::var());
        Playing p;
        p.id           = docIdFromName(doc.getProperty("name", "").toString()).toStdString();
        p.album        = valueAsString(fieldByName(fields, "album"))      .toStdString();
        p.artists      = valueAsString(fieldByName(fields, "artists"))    .toStdString();
        p.durationMS   = valueAsInt   (fieldByName(fields, "durationMS"));
        p.isExplicit   = valueAsBool  (fieldByName(fields, "isExplicit"));
        p.imageUrl     = valueAsString(fieldByName(fields, "imageUrl"))   .toStdString();
        p.keySignature = valueAsString(fieldByName(fields, "keySignature")).toStdString();
        p.lyricUrl     = valueAsString(fieldByName(fields, "lyricUrl"))   .toStdString();
        p.songName     = valueAsString(fieldByName(fields, "songName"))   .toStdString();
        p.songId       = valueAsString(fieldByName(fields, "songId"))     .toStdString();
        p.songVersion  = valueAsString(fieldByName(fields, "songVersion")).toStdString();
        p.pitch        = (float) valueAsDouble(fieldByName(fields, "pitch"));
        p.releaseDate  = valueAsString(fieldByName(fields, "releaseDate")).toStdString();
        p.tempo        = valueAsDouble(fieldByName(fields, "tempo"));
        p.type         = valueAsString(fieldByName(fields, "type"))       .toStdString();
        p.singerName   = valueAsString(fieldByName(fields, "singerName")) .toStdString();
        p.avatar       = valueAsString(fieldByName(fields, "avatar"))     .toStdString();
        p.profileId    = valueAsString(fieldByName(fields, "profileId"))  .toStdString();
        p.deviceId     = valueAsString(fieldByName(fields, "deviceId"))   .toStdString();
        p.foxId        = valueAsString(fieldByName(fields, "foxId"))      .toStdString();
        p.kdId         = valueAsString(fieldByName(fields, "kdId"))       .toStdString();
        return p;
    }

    juce::var playingFields(const Playing& p)
    {
        juce::DynamicObject::Ptr root = new juce::DynamicObject();
        auto put = [&](const char* k, juce::var v) { root->setProperty(k, v); };
        put("album",        FC::stringValue(juce::String(p.album)));
        put("artists",      FC::stringValue(juce::String(p.artists)));
        put("durationMS",   FC::integerValue(p.durationMS));
        put("isExplicit",   FC::booleanValue(p.isExplicit));
        put("imageUrl",     FC::stringValue(juce::String(p.imageUrl)));
        put("keySignature", FC::stringValue(juce::String(p.keySignature)));
        put("lyricUrl",     FC::stringValue(juce::String(p.lyricUrl)));
        put("songName",     FC::stringValue(juce::String(p.songName)));
        put("songId",       FC::stringValue(juce::String(p.songId)));
        put("songVersion",  FC::stringValue(juce::String(p.songVersion)));
        put("pitch",        FC::doubleValue((double) p.pitch));
        put("releaseDate",  FC::stringValue(juce::String(p.releaseDate)));
        put("tempo",        FC::doubleValue(p.tempo));
        put("type",         FC::stringValue(juce::String(p.type)));
        put("singerName",   FC::stringValue(juce::String(p.singerName)));
        put("avatar",       FC::stringValue(juce::String(p.avatar)));
        put("profileId",    FC::stringValue(juce::String(p.profileId)));
        put("deviceId",     FC::stringValue(juce::String(p.deviceId)));
        put("foxId",        FC::stringValue(juce::String(p.foxId)));
        put("kdId",         FC::stringValue(juce::String(p.kdId)));
        return juce::var(root.get());
    }

    // Editable VenueItem fields the settings page can change. Listed once so
    // the body payload and the updateMask stay in sync.
    static const juce::StringArray kEditableFields = {
        "name", "address", "city", "country",
        "code", "codePlus",
        "numSongs", "numSingers", "numStrikes",
        "repeatSongs", "autoapprove",
        "background", "encoreBackground", "encodeBackgroundSize",
        "showOnlineSongs", "showOnlineSongsEncore", "showMemoryStats",
        "logoUrl"
    };

    juce::var fieldsFromVenue(const VenueItem& v)
    {
        return FC::makeFields({
            { "name",     FC::stringValue(juce::String(v.name)) },
            { "address",  FC::stringValue(juce::String(v.address)) },
            { "city",     FC::stringValue(juce::String(v.city)) },
            { "country",  FC::stringValue(juce::String(v.country)) },
            { "code",     FC::stringValue(juce::String(v.code)) },
            { "codePlus", FC::stringValue(juce::String(v.codePlus)) },
            { "numSongs",   FC::stringValue(juce::String(v.numSongs)) },
            { "numSingers", FC::stringValue(juce::String(v.numSingers)) },
            { "numStrikes", FC::stringValue(juce::String(v.numStrikes)) },
            { "repeatSongs", FC::booleanValue(v.repeatSongs) },
            { "autoapprove", FC::booleanValue(v.autoapprove) },
            { "background",           FC::stringValue(juce::String(v.background)) },
            { "encoreBackground",     FC::stringValue(juce::String(v.encoreBackground)) },
            { "encodeBackgroundSize", FC::stringValue(juce::String(v.encodeBackgroundSize)) },
            { "showOnlineSongs",       FC::booleanValue(v.showOnlineSongs) },
            { "showOnlineSongsEncore", FC::booleanValue(v.showOnlineSongsEncore) },
            { "showMemoryStats",       FC::booleanValue(v.showMemoryStats) },
            { "logoUrl", FC::stringValue(juce::String(v.logoUrl)) }
        });
    }

    // Fingerprint a Playing list for the watcher.
    juce::String playingFingerprint(const std::vector<Playing>& list)
    {
        juce::String out;
        out.preallocateBytes(256);
        for (auto& p : list)
            out << p.id << "|" << p.songId << "|" << p.singerName << "/";
        return out;
    }
}

//==============================================================================
// loadVenue / getCurrent / getCurrentVenueId / saveVenue / clear (existing)

void VenueService::loadVenue(const juce::String& venueId, LoadCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone)
            juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        int status = 0;
        auto doc = FC::getInstance().getDocument("venues/" + venueId, &status);

        if (status != 200 || ! doc.hasProperty("fields"))
        {
            if (onDone)
            {
                juce::MessageManager::callAsync([onDone, status]()
                {
                    onDone(false, {}, "Venue not found (HTTP " + juce::String(status) + ")");
                });
            }
            return;
        }

        auto unwrapped = FC::unwrapFields(doc);
        auto json = juce::JSON::toString(unwrapped);
        auto venue = VenueItem::fromJson(json);
        venue.id = venueId.toStdString();

        {
            VenueService& self = VenueService::getInstance();
            const juce::ScopedLock sl(self.lock_);
            self.current_   = venue;
            self.currentId_ = venueId;
        }

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, venue]() mutable
            {
                onDone(true, std::move(venue), {});
            });
        }
    });
}

VenueItem VenueService::getCurrent() const
{
    const juce::ScopedLock sl(lock_);
    return current_;
}

juce::String VenueService::getCurrentVenueId() const
{
    const juce::ScopedLock sl(lock_);
    return currentId_;
}

void VenueService::saveVenue(const juce::String& venueId,
                             const VenueItem& venue,
                             SaveCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone)
            juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, venue, onDone = std::move(onDone)]()
    {
        juce::StringArray maskParts;
        for (auto& f : kEditableFields)
            maskParts.add("updateMask.fieldPaths=" + f);

        const auto path = "venues/" + venueId + "?" + maskParts.joinIntoString("&");
        const auto fields = fieldsFromVenue(venue);

        const bool ok = FC::getInstance().patchDocument(path, fields);

        if (ok)
        {
            VenueService& self = VenueService::getInstance();
            const juce::ScopedLock sl(self.lock_);
            self.current_   = venue;
            self.currentId_ = venueId;
        }

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, ok]()
            {
                onDone(ok, ok ? juce::String() : juce::String("Save failed"));
            });
        }
    });
}

void VenueService::clear()
{
    stopWatchingPlaying();
    const juce::ScopedLock sl(lock_);
    current_   = {};
    currentId_ = {};
}

//==============================================================================
// Code generator (utility)
juce::String VenueService::generateCode(int length)
{
    static const char* charset = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    auto& rng = juce::Random::getSystemRandom();
    juce::String out;
    for (int i = 0; i < length; ++i)
        out += juce::String::charToString(charset[rng.nextInt(26)]);
    return out;
}

//==============================================================================
// Venue CRUD (top-level)
void VenueService::getVenues(ListCallback onDone)
{
    juce::Thread::launch([onDone = std::move(onDone)]()
    {
        auto docs = FC::getInstance().listCollection("venues", 500);

        std::vector<VenueItem> out;
        out.reserve((size_t) docs.size());
        for (auto& d : docs)
        {
            auto unwrapped = FC::unwrapFields(d);
            auto venue = VenueItem::fromJson(juce::JSON::toString(unwrapped));
            venue.id = docIdFromName(d.getProperty("name", "").toString()).toStdString();
            out.push_back(std::move(venue));
        }

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
            {
                onDone(true, std::move(out), {});
            });
        }
    });
}

void VenueService::addVenue(const VenueItem& venue, AddVenueCallback onDone)
{
    VenueItem v = venue;
    if (v.code.empty())     v.code     = generateCode(6).toStdString();
    if (v.codePlus.empty()) v.codePlus = generateCode(6).toStdString();
    const auto now = juce::Time::getCurrentTime().toMilliseconds();
    v.dateTime    = now;
    v.updateSongs = now;

    juce::Thread::launch([v, onDone = std::move(onDone)]()
    {
        auto fields = fieldsFromVenue(v);
        auto resp = FC::getInstance().createDocument("venues", fields);
        const bool ok = resp.isObject();
        const auto newId = ok ? docIdFromName(resp.getProperty("name", "").toString())
                              : juce::String();
        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, ok, newId]()
            {
                onDone(ok, newId, ok ? juce::String() : juce::String("createDocument failed"));
            });
        }
    });
}

void VenueService::deleteVenue(const juce::String& venueId, WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        // Same subcollections the Angular service deletes — note the
        // playlists/venueLists/<list> nested path needs to be split.
        const juce::StringArray subPaths = {
            "queue", "requested", "playing", "emojis",
            "playlists/venueLists/new",
            "playlists/venueLists/Popular",
            "playlists/venueLists/Recommended"
        };
        for (auto& sub : subPaths)
            deleteCollectionContents("venues/" + venueId + "/" + sub);

        const bool ok = FC::getInstance().deleteDocument("venues/" + venueId);

        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, ok]()
            {
                onDone(ok, ok ? juce::String() : juce::String("delete failed"));
            });
        }
    });
}

void VenueService::updateVenueCode(const juce::String& venueId,
                                   const juce::String& newCode,
                                   WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, newCode, onDone = std::move(onDone)]()
    {
        auto fields = FC::makeFields({ { "code", FC::stringValue(newCode) } });
        const auto path = "venues/" + venueId + "?updateMask.fieldPaths=code";
        const bool ok = FC::getInstance().patchDocument(path, fields);
        if (onDone)
        {
            juce::MessageManager::callAsync([onDone, ok]()
            {
                onDone(ok, ok ? juce::String() : juce::String("patch failed"));
            });
        }
    });
}

void VenueService::addCode(const juce::String& venueId, WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }

    const auto code     = generateCode(6);
    const auto codePlus = generateCode(6);
    const auto now      = juce::Time::getCurrentTime().toMilliseconds();

    juce::Thread::launch([venueId, code, codePlus, now, onDone = std::move(onDone)]()
    {
        auto fields = FC::makeFields({
            { "code",     FC::stringValue(code) },
            { "codePlus", FC::stringValue(codePlus) },
            { "dateTime", FC::integerValue(now) }
        });
        const auto path = "venues/" + venueId
                        + "?updateMask.fieldPaths=code"
                        + "&updateMask.fieldPaths=codePlus"
                        + "&updateMask.fieldPaths=dateTime";
        const bool ok = FC::getInstance().patchDocument(path, fields);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("patch failed")); });
    });
}

void VenueService::setLastSongUpdateDate(WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No active venue"); });
        return;
    }

    const auto now = juce::Time::getCurrentTime().toMilliseconds();
    juce::Thread::launch([venueId, now, onDone = std::move(onDone)]()
    {
        auto fields = FC::makeFields({ { "updateSongs", FC::integerValue(now) } });
        const auto path = "venues/" + venueId + "?updateMask.fieldPaths=updateSongs";
        const bool ok = FC::getInstance().patchDocument(path, fields);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("patch failed")); });
    });
}

void VenueService::updateDate(WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No active venue"); });
        return;
    }

    const auto now = juce::Time::getCurrentTime().toMilliseconds();
    juce::Thread::launch([venueId, now, onDone = std::move(onDone)]()
    {
        auto fields = FC::makeFields({ { "dateTime", FC::integerValue(now) } });
        const auto path = "venues/" + venueId + "?updateMask.fieldPaths=dateTime";
        const bool ok = FC::getInstance().patchDocument(path, fields);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("patch failed")); });
    });
}

//==============================================================================
// Now-playing
void VenueService::addCurrentSongPlaying(const Playing& playing, WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No active venue"); });
        return;
    }

    Playing p = playing;
    if (p.singerName.empty())
        p.singerName = "Unknown Singer";

    juce::Thread::launch([venueId, p, onDone = std::move(onDone)]()
    {
        // Best-effort emoji clear (Angular: clearEmojisCollection).
        deleteCollectionContents("venues/" + venueId + "/emojis");

        // Wrap fields → { fields: { ... } } (createDocument expects fields obj).
        auto fields = playingFields(p);
        juce::DynamicObject::Ptr w = new juce::DynamicObject();
        w->setProperty("fields", fields);
        // createDocument's signature takes the fields directly (sets up
        // "fields" wrapper itself), so pass the inner var.
        auto resp = FC::getInstance().createDocument("venues/" + venueId + "/playing", fields);
        const bool ok = resp.isObject();
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("createDocument failed")); });
    });
}

void VenueService::getCurrentSinger(PlayingListCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No active venue"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        auto docs = FC::getInstance().listCollection("venues/" + venueId + "/playing", 50);
        std::vector<Playing> out;
        out.reserve((size_t) docs.size());
        for (auto& d : docs)
            out.push_back(playingFromDoc(d));
        if (onDone)
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
                { onDone(true, std::move(out), {}); });
    });
}

void VenueService::startWatchingPlaying(PlayingChangeCallback onChange)
{
    if (getCurrentVenueId().isEmpty()) return;

    {
        const juce::ScopedLock sl(lock_);
        onPlayingChange_ = std::move(onChange);
        watchingPlaying_ = true;
        playingFingerprint_.clear();
    }
    if (! isTimerRunning())
        startTimer(watchIntervalMs_);
    pollPlayingWatcher();
}

void VenueService::stopWatchingPlaying()
{
    {
        const juce::ScopedLock sl(lock_);
        watchingPlaying_ = false;
        onPlayingChange_ = {};
        playingFingerprint_.clear();
    }
    stopTimer();
}

void VenueService::timerCallback()
{
    pollPlayingWatcher();
}

void VenueService::pollPlayingWatcher()
{
    bool active;
    juce::String venueId;
    {
        const juce::ScopedLock sl(lock_);
        active  = watchingPlaying_ && ! playingInFlight_;
        venueId = currentId_;
    }
    if (! active || venueId.isEmpty())
        return;

    {
        const juce::ScopedLock sl(lock_);
        playingInFlight_ = true;
    }

    juce::Thread::launch([venueId]()
    {
        auto docs = FC::getInstance().listCollection("venues/" + venueId + "/playing", 50);
        std::vector<Playing> list;
        list.reserve((size_t) docs.size());
        for (auto& d : docs) list.push_back(playingFromDoc(d));

        juce::MessageManager::callAsync([venueId, list = std::move(list)]() mutable
        {
            VenueService& self = VenueService::getInstance();
            PlayingChangeCallback cb;
            bool changed = false;
            {
                const juce::ScopedLock sl(self.lock_);
                self.playingInFlight_ = false;
                if (! self.watchingPlaying_ || venueId != self.currentId_)
                    return;
                auto fp = playingFingerprint(list);
                if (fp != self.playingFingerprint_)
                {
                    self.playingFingerprint_ = fp;
                    cb = self.onPlayingChange_;
                    changed = true;
                }
            }
            if (changed && cb)
                cb(std::move(list));
        });
    });
}

void VenueService::removeCurrentSongPlaying(const juce::String& playingDocId,
                                            WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty() || playingDocId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing args"); });
        return;
    }

    juce::Thread::launch([venueId, playingDocId, onDone = std::move(onDone)]()
    {
        const bool ok = FC::getInstance()
                            .deleteDocument("venues/" + venueId + "/playing/" + playingDocId);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("delete failed")); });
    });
}

void VenueService::removeAllCurrentSongPlaying(WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No active venue"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        deleteCollectionContents("venues/" + venueId + "/playing");
        if (onDone)
            juce::MessageManager::callAsync([onDone]() { onDone(true, {}); });
    });
}

//==============================================================================
// Emojis
void VenueService::getNewEmoji(const juce::String& venueId, EmojiListCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        auto docs = FC::getInstance().listCollection("venues/" + venueId + "/emojis", 200);
        std::vector<Emoji> out;
        out.reserve((size_t) docs.size());
        for (auto& d : docs) out.push_back(emojiFromDoc(d));
        if (onDone)
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
                { onDone(true, std::move(out), {}); });
    });
}

void VenueService::deleteEmoji(const juce::String& venueId,
                               const juce::String& emojiId,
                               WriteCallback onDone)
{
    if (venueId.isEmpty() || emojiId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing args"); });
        return;
    }

    juce::Thread::launch([venueId, emojiId, onDone = std::move(onDone)]()
    {
        const bool ok = FC::getInstance().deleteDocument("venues/" + venueId + "/emojis/" + emojiId);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("delete failed")); });
    });
}

//==============================================================================
// Playlists
namespace
{
    juce::String playlistsRoot(const juce::String& venueId, const juce::String& listName)
    {
        return "venues/" + venueId + "/playlists/venueLists/" + listName;
    }
}

void VenueService::addSongToLists(const CdgSong& song,
                                  const juce::StringArray& listNames,
                                  WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty() || listNames.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing args"); });
        return;
    }

    juce::Thread::launch([venueId, song, listNames, onDone = std::move(onDone)]()
    {
        Playlist p;
        p.id         = song.id;
        p.imageUrl   = song.imageUrl;
        p.songName   = song.songName;
        p.artistName = song.artistName;

        bool allOk = true;
        for (auto& list : listNames)
        {
            auto fields = playlistFields(p);
            auto resp = FC::getInstance().createDocument(playlistsRoot(venueId, list), fields);
            if (! resp.isObject()) allOk = false;
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, allOk]()
                { onDone(allOk, allOk ? juce::String() : juce::String("at least one createDocument failed")); });
    });
}

void VenueService::getPlaylists(const juce::String& venueId,
                                const juce::String& listName,
                                PlaylistListCallback onDone)
{
    if (venueId.isEmpty() || listName.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, "Missing args"); });
        return;
    }

    juce::Thread::launch([venueId, listName, onDone = std::move(onDone)]()
    {
        auto docs = FC::getInstance().listCollection(playlistsRoot(venueId, listName), 500);
        std::vector<Playlist> out;
        out.reserve((size_t) docs.size());
        for (auto& d : docs) out.push_back(playlistFromDoc(d));
        if (onDone)
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
                { onDone(true, std::move(out), {}); });
    });
}

void VenueService::getNewSongs(const juce::String& venueId, PlaylistListCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        // Sort docs by dateAdded desc, take top 20.
        auto docs = FC::getInstance().listCollection(playlistsRoot(venueId, "new"), 200);

        struct Sortable { Playlist p; juce::int64 ts; };
        std::vector<Sortable> all;
        all.reserve((size_t) docs.size());
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            Sortable s;
            s.p  = playlistFromDoc(d);
            s.ts = valueAsInt64(fieldByName(fields, "dateAdded"));
            all.push_back(std::move(s));
        }
        std::sort(all.begin(), all.end(),
                  [](const Sortable& a, const Sortable& b) { return a.ts > b.ts; });

        std::vector<Playlist> out;
        out.reserve(20);
        for (size_t i = 0; i < all.size() && out.size() < 20; ++i)
            out.push_back(std::move(all[i].p));

        if (onDone)
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
                { onDone(true, std::move(out), {}); });
    });
}

void VenueService::addSongToNewSongs(const CdgSong& song, WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No active venue"); });
        return;
    }

    juce::Thread::launch([venueId, song, onDone = std::move(onDone)]()
    {
        auto root = playlistsRoot(venueId, "new");
        auto docs = FC::getInstance().listCollection(root, 200);

        // Look for existing entry with the same `id` field.
        juce::String existingDocPath;
        juce::int64  existingDate = 0;
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            auto idStr = valueAsString(fieldByName(fields, "id"));
            if (idStr == juce::String(song.id))
            {
                existingDocPath = relPathFromDocName(d.getProperty("name", "").toString());
                existingDate    = valueAsInt64(fieldByName(fields, "dateAdded"));
                break;
            }
        }

        Playlist p;
        p.id = song.id; p.imageUrl = song.imageUrl;
        p.songName = song.songName; p.artistName = song.artistName;

        const auto when = (existingDate > 0)
                            ? existingDate
                            : juce::Time::getCurrentTime().toMilliseconds();

        auto fields = playlistFields(p, when);

        bool ok;
        if (existingDocPath.isNotEmpty())
        {
            // PATCH all fields (update existing).
            const auto path = existingDocPath
                              + "?updateMask.fieldPaths=id"
                              + "&updateMask.fieldPaths=imageUrl"
                              + "&updateMask.fieldPaths=songName"
                              + "&updateMask.fieldPaths=artistName"
                              + "&updateMask.fieldPaths=dateAdded";
            ok = FC::getInstance().patchDocument(path, fields);
        }
        else
        {
            auto resp = FC::getInstance().createDocument(root, fields);
            ok = resp.isObject();
        }

        // Cap the list at 20 entries (oldest first).
        if (ok)
        {
            auto refreshed = FC::getInstance().listCollection(root, 200);
            if (refreshed.size() > 20)
            {
                struct Item { juce::String relPath; juce::int64 ts; };
                std::vector<Item> all;
                all.reserve((size_t) refreshed.size());
                for (auto& d : refreshed)
                {
                    Item it;
                    it.relPath = relPathFromDocName(d.getProperty("name", "").toString());
                    it.ts = valueAsInt64(fieldByName(d.getProperty("fields", juce::var()), "dateAdded"));
                    all.push_back(std::move(it));
                }
                // newest first
                std::sort(all.begin(), all.end(),
                          [](const Item& a, const Item& b) { return a.ts > b.ts; });
                for (size_t i = 20; i < all.size(); ++i)
                    FC::getInstance().deleteDocument(all[i].relPath);
            }
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("write failed")); });
    });
}

void VenueService::updateSongInNewSongs(const CdgSong& song, WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No active venue"); });
        return;
    }

    juce::Thread::launch([venueId, song, onDone = std::move(onDone)]()
    {
        auto root = playlistsRoot(venueId, "new");
        auto docs = FC::getInstance().listCollection(root, 200);

        juce::String existingDocPath;
        juce::int64  existingDate = 0;
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            if (valueAsString(fieldByName(fields, "id")) == juce::String(song.id))
            {
                existingDocPath = relPathFromDocName(d.getProperty("name", "").toString());
                existingDate    = valueAsInt64(fieldByName(fields, "dateAdded"));
                break;
            }
        }

        if (existingDocPath.isEmpty())
        {
            if (onDone)
                juce::MessageManager::callAsync([onDone]() { onDone(true, {}); });  // not in list = no-op
            return;
        }

        Playlist p;
        p.id = song.id; p.imageUrl = song.imageUrl;
        p.songName = song.songName; p.artistName = song.artistName;

        auto fields = playlistFields(p, existingDate);
        const auto path = existingDocPath
                          + "?updateMask.fieldPaths=id"
                          + "&updateMask.fieldPaths=imageUrl"
                          + "&updateMask.fieldPaths=songName"
                          + "&updateMask.fieldPaths=artistName";
        const bool ok = FC::getInstance().patchDocument(path, fields);
        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("patch failed")); });
    });
}

void VenueService::deleteSongFromNewSongs(const juce::String& songId, WriteCallback onDone)
{
    deleteSongFromPlaylist("new", songId, std::move(onDone));
}

void VenueService::deleteSongFromPlaylist(const juce::String& listName,
                                          const juce::String& songId,
                                          WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty() || listName.isEmpty() || songId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "Missing args"); });
        return;
    }

    juce::Thread::launch([venueId, listName, songId, onDone = std::move(onDone)]()
    {
        auto root = playlistsRoot(venueId, listName);
        auto docs = FC::getInstance().listCollection(root, 500);

        bool removed = false;
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            if (valueAsString(fieldByName(fields, "id")) == songId)
            {
                auto rel = relPathFromDocName(d.getProperty("name", "").toString());
                if (FC::getInstance().deleteDocument(rel))
                    removed = true;
            }
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, removed]()
                { onDone(removed, removed ? juce::String() : juce::String("not found")); });
    });
}

namespace
{
    // Shared cleanup logic — drops empty-id docs and duplicate ids
    // (keeps newest by dateAdded). Returns the number of deletions.
    int cleanupListContents(const juce::String& root)
    {
        auto docs = FC::getInstance().listCollection(root, 500);

        struct Item { juce::String relPath; juce::String id; juce::int64 ts; };
        std::vector<Item> items;
        items.reserve((size_t) docs.size());
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            Item it;
            it.relPath = relPathFromDocName(d.getProperty("name", "").toString());
            it.id      = valueAsString(fieldByName(fields, "id")).trim();
            it.ts      = valueAsInt64(fieldByName(fields, "dateAdded"));
            items.push_back(std::move(it));
        }

        // First pass: delete empty-id docs.
        int n = 0;
        std::vector<Item> kept;
        for (auto& it : items)
        {
            if (it.id.isEmpty())
            {
                if (FC::getInstance().deleteDocument(it.relPath)) ++n;
            }
            else
            {
                kept.push_back(it);
            }
        }

        // Second pass: dedupe by id (keep newest dateAdded).
        std::sort(kept.begin(), kept.end(),
                  [](const Item& a, const Item& b) { return a.ts > b.ts; });
        juce::StringArray seen;
        for (auto& it : kept)
        {
            if (seen.contains(it.id))
            {
                if (FC::getInstance().deleteDocument(it.relPath)) ++n;
            }
            else
            {
                seen.add(it.id);
            }
        }
        return n;
    }
}

void VenueService::cleanupOrphanedNewSongs(IntCallback onDone)
{
    cleanupPlaylistSongs("new", std::move(onDone));
}

void VenueService::cleanupPlaylistSongs(const juce::String& listName, IntCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty() || listName.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(0, "Missing args"); });
        return;
    }

    juce::Thread::launch([venueId, listName, onDone = std::move(onDone)]()
    {
        int n = cleanupListContents(playlistsRoot(venueId, listName));
        if (onDone)
            juce::MessageManager::callAsync([onDone, n]() { onDone(n, {}); });
    });
}

void VenueService::cleanupAllPlaylists(IntCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(0, "No active venue"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        const juce::StringArray lists = { "new", "Popular", "Recommended" };
        int total = 0;
        for (auto& name : lists)
            total += cleanupListContents(playlistsRoot(venueId, name));

        if (onDone)
            juce::MessageManager::callAsync([onDone, total]() { onDone(total, {}); });
    });
}

//==============================================================================
// Queue / requested cleanup
void VenueService::deleteAllSingersFromQueue(const juce::String& venueId, WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }
    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        deleteCollectionContents("venues/" + venueId + "/queue");
        if (onDone)
            juce::MessageManager::callAsync([onDone]() { onDone(true, {}); });
    });
}

void VenueService::deleteAllSongsFromRequested(const juce::String& venueId, WriteCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No venueId"); });
        return;
    }
    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        deleteCollectionContents("venues/" + venueId + "/requested");
        if (onDone)
            juce::MessageManager::callAsync([onDone]() { onDone(true, {}); });
    });
}

//==============================================================================
// Session check
void VenueService::checkExistingSessionData(const juce::String& venueId,
                                            SessionCountsCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        SessionCounts c;
        c.queueCount     = FC::getInstance().listCollection("venues/" + venueId + "/queue", 500).size();
        c.requestedCount = FC::getInstance().listCollection("venues/" + venueId + "/requested", 500).size();
        if (onDone)
            juce::MessageManager::callAsync([onDone, c]() { onDone(true, c, {}); });
    });
}

//==============================================================================
// Play history
namespace
{
    juce::String playHistoryRoot(const juce::String& venueId)
    {
        return "venues/" + venueId + "/playHistory";
    }

    juce::var playHistoryFields(const VenueService::PlayHistoryEntry& e, juce::int64 playedAtMs)
    {
        auto root = new juce::DynamicObject();
        root->setProperty("id",          FC::stringValue(juce::String(e.songId)));
        root->setProperty("songName",    FC::stringValue(juce::String(e.songName)));
        root->setProperty("artistName",  FC::stringValue(juce::String(e.artistName)));
        root->setProperty("imageUrl",    FC::stringValue(juce::String(e.imageUrl)));
        root->setProperty("singerName",  FC::stringValue(juce::String(e.singerName)));
        root->setProperty("playedAt",    FC::integerValue(playedAtMs));
        return juce::var(root);
    }

    Playlist playlistFromHistoryDoc(const juce::var& doc)
    {
        auto fields = doc.getProperty("fields", juce::var());
        Playlist p;
        p.id         = valueAsString(fieldByName(fields, "id"))        .toStdString();
        p.imageUrl   = valueAsString(fieldByName(fields, "imageUrl"))  .toStdString();
        p.songName   = valueAsString(fieldByName(fields, "songName"))  .toStdString();
        p.artistName = valueAsString(fieldByName(fields, "artistName")).toStdString();
        return p;
    }
}

void VenueService::addPlayHistory(const PlayHistoryEntry& entry, WriteCallback onDone)
{
    const auto venueId = getCurrentVenueId();
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, "No active venue"); });
        return;
    }

    juce::Thread::launch([venueId, entry, onDone = std::move(onDone)]()
    {
        const auto root    = playHistoryRoot(venueId);
        const auto when    = juce::Time::getCurrentTime().toMilliseconds();
        auto       fields  = playHistoryFields(entry, when);

        auto resp = FC::getInstance().createDocument(root, fields);
        const bool ok = resp.isObject();

        // Keep at most 20 docs — delete the oldest ones.
        if (ok)
        {
            auto docs = FC::getInstance().listCollection(root, 200);

            struct Sortable { juce::String relPath; juce::int64 ts; };
            std::vector<Sortable> all;
            all.reserve((size_t) docs.size());
            for (auto& d : docs)
            {
                auto f = d.getProperty("fields", juce::var());
                Sortable s;
                s.relPath = relPathFromDocName(d.getProperty("name", "").toString());
                s.ts      = valueAsInt64(fieldByName(f, "playedAt"));
                all.push_back(std::move(s));
            }
            // Sort newest first; delete everything beyond the 20th entry.
            std::sort(all.begin(), all.end(),
                      [](const Sortable& a, const Sortable& b) { return a.ts > b.ts; });
            for (size_t i = 20; i < all.size(); ++i)
                FC::getInstance().deleteDocument(all[i].relPath);
        }

        if (onDone)
            juce::MessageManager::callAsync([onDone, ok]()
                { onDone(ok, ok ? juce::String() : juce::String("createDocument failed")); });
    });
}

void VenueService::getRecentlyPlayed(const juce::String& venueId, PlaylistListCallback onDone)
{
    if (venueId.isEmpty())
    {
        if (onDone) juce::MessageManager::callAsync([onDone] { onDone(false, {}, "No venueId"); });
        return;
    }

    juce::Thread::launch([venueId, onDone = std::move(onDone)]()
    {
        auto docs = FC::getInstance().listCollection(playHistoryRoot(venueId), 200);

        struct Sortable { Playlist p; juce::int64 ts; };
        std::vector<Sortable> all;
        all.reserve((size_t) docs.size());
        for (auto& d : docs)
        {
            auto fields = d.getProperty("fields", juce::var());
            Sortable s;
            s.p  = playlistFromHistoryDoc(d);
            s.ts = valueAsInt64(fieldByName(fields, "playedAt"));
            all.push_back(std::move(s));
        }
        std::sort(all.begin(), all.end(),
                  [](const Sortable& a, const Sortable& b) { return a.ts > b.ts; });

        std::vector<Playlist> out;
        out.reserve(20);
        for (size_t i = 0; i < all.size() && out.size() < 20; ++i)
            out.push_back(std::move(all[i].p));

        if (onDone)
            juce::MessageManager::callAsync([onDone, out = std::move(out)]() mutable
                { onDone(true, std::move(out), {}); });
    });
}

//==============================================================================
// Sort helpers
void VenueService::sortQueueByName(std::vector<Singers>& q)
{
    std::sort(q.begin(), q.end(), [](const Singers& a, const Singers& b)
    {
        auto la = juce::String(a.name).toLowerCase().trim();
        auto lb = juce::String(b.name).toLowerCase().trim();
        return la < lb;
    });
}

void VenueService::sortQueueByOrder(std::vector<Singers>& q)
{
    std::sort(q.begin(), q.end(),
              [](const Singers& a, const Singers& b) { return a.order < b.order; });
}
