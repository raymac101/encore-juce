/*
  ==============================================================================

    Audit.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Audit and UserAudit model implementation

  ==============================================================================
*/

#include "Audit.h"

//==============================================================================
// Helpers
static juce::Array<juce::var> strVecToVar(const std::vector<std::string>& v)
{
    juce::Array<juce::var> a;
    for (auto& s : v) a.add(juce::String(s));
    return a;
}

static std::vector<std::string> varToStrVec(const juce::var& v)
{
    std::vector<std::string> r;
    if (v.isArray())
        for (int i = 0; i < v.size(); ++i)
            r.push_back(v[i].toString().toStdString());
    return r;
}

//==============================================================================
// Audit
//==============================================================================
juce::String Audit::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("venueId", juce::String(venueId));
    obj->setProperty("venueName", juce::String(venueName));
    obj->setProperty("songName", juce::String(songName));
    obj->setProperty("songId", juce::String(songId));
    obj->setProperty("date", (juce::int64)date);
    obj->setProperty("artist", juce::String(artist));
    obj->setProperty("singerId", juce::String(singerId));
    obj->setProperty("source", juce::String(source));
    obj->setProperty("deviceId", juce::String(deviceId));
    return juce::JSON::toString(juce::var(obj.get()));
}

Audit Audit::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Audit Audit::fromJsonObject(juce::DynamicObject* obj)
{
    Audit a;
    if (obj == nullptr) return a;
    a.venueId   = obj->getProperty("venueId").toString().toStdString();
    a.venueName = obj->getProperty("venueName").toString().toStdString();
    a.songName  = obj->getProperty("songName").toString().toStdString();
    a.songId    = obj->getProperty("songId").toString().toStdString();
    a.date      = (int64_t)obj->getProperty("date");
    a.artist    = obj->getProperty("artist").toString().toStdString();
    a.singerId  = obj->getProperty("singerId").toString().toStdString();
    a.source    = obj->getProperty("source").toString().toStdString();
    a.deviceId  = obj->getProperty("deviceId").toString().toStdString();
    return a;
}

//==============================================================================
// UserAudit
//==============================================================================
juce::String UserAudit::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("venueId", juce::String(venueId));
    obj->setProperty("venueName", juce::String(venueName));
    obj->setProperty("date", (juce::int64)date);
    obj->setProperty("album", juce::String(album));
    obj->setProperty("artist", juce::String(artist));
    obj->setProperty("durationMS", durationMS);
    obj->setProperty("explicit", isExplicit);
    obj->setProperty("genres", juce::var(strVecToVar(genres)));
    obj->setProperty("id", juce::String(id));
    obj->setProperty("imageUrl", juce::String(imageUrl));
    obj->setProperty("keySignature", juce::String(keySignature));
    obj->setProperty("lyricUrl", juce::String(lyricUrl));
    obj->setProperty("songName", juce::String(songName));
    obj->setProperty("songId", juce::String(songId));
    obj->setProperty("releaseDate", juce::String(releaseDate));
    obj->setProperty("tempo", tempo);
    obj->setProperty("type", juce::String(type));
    obj->setProperty("singerName", juce::String(singerName));
    obj->setProperty("avatar", juce::String(avatar));
    obj->setProperty("profileId", juce::String(profileId));
    obj->setProperty("deviceId", juce::String(deviceId));
    obj->setProperty("foxId", juce::String(foxId));
    obj->setProperty("songVersion", juce::String(songVersion));
    obj->setProperty("pitch", (double)pitch);
    obj->setProperty("kjId", juce::String(kjId));
    return juce::JSON::toString(juce::var(obj.get()));
}

UserAudit UserAudit::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

UserAudit UserAudit::fromJsonObject(juce::DynamicObject* obj)
{
    UserAudit u;
    if (obj == nullptr) return u;
    u.venueId      = obj->getProperty("venueId").toString().toStdString();
    u.venueName    = obj->getProperty("venueName").toString().toStdString();
    u.date         = (int64_t)obj->getProperty("date");
    u.album        = obj->getProperty("album").toString().toStdString();
    u.artist       = obj->getProperty("artist").toString().toStdString();
    u.durationMS   = (int)obj->getProperty("durationMS");
    u.isExplicit   = (bool)obj->getProperty("explicit");
    u.genres       = varToStrVec(obj->getProperty("genres"));
    u.id           = obj->getProperty("id").toString().toStdString();
    u.imageUrl     = obj->getProperty("imageUrl").toString().toStdString();
    u.keySignature = obj->getProperty("keySignature").toString().toStdString();
    u.lyricUrl     = obj->getProperty("lyricUrl").toString().toStdString();
    u.songName     = obj->getProperty("songName").toString().toStdString();
    u.songId       = obj->getProperty("songId").toString().toStdString();
    u.releaseDate  = obj->getProperty("releaseDate").toString().toStdString();
    u.tempo        = (double)obj->getProperty("tempo");
    u.type         = obj->getProperty("type").toString().toStdString();
    u.singerName   = obj->getProperty("singerName").toString().toStdString();
    u.avatar       = obj->getProperty("avatar").toString().toStdString();
    u.profileId    = obj->getProperty("profileId").toString().toStdString();
    u.deviceId     = obj->getProperty("deviceId").toString().toStdString();
    u.foxId        = obj->getProperty("foxId").toString().toStdString();
    u.songVersion  = obj->getProperty("songVersion").toString().toStdString();
    u.pitch        = (float)(double)obj->getProperty("pitch");
    u.kjId         = obj->getProperty("kjId").toString().toStdString();
    return u;
}
