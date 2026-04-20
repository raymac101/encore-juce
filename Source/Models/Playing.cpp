/*
  ==============================================================================

    Playing.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Playing model implementation

  ==============================================================================
*/

#include "Playing.h"

//==============================================================================
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
juce::String Playing::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("album", juce::String(album));
    obj->setProperty("artists", juce::String(artists));
    obj->setProperty("durationMS", durationMS);
    obj->setProperty("explicit", isExplicit);
    obj->setProperty("genres", juce::var(strVecToVar(genres)));
    obj->setProperty("id", juce::String(id));
    obj->setProperty("imageUrl", juce::String(imageUrl));
    obj->setProperty("keySignature", juce::String(keySignature));
    obj->setProperty("lyricUrl", juce::String(lyricUrl));
    obj->setProperty("songName", juce::String(songName));
    obj->setProperty("songId", juce::String(songId));
    obj->setProperty("songVersion", juce::String(songVersion));
    obj->setProperty("pitch", (double)pitch);
    obj->setProperty("releaseDate", juce::String(releaseDate));
    obj->setProperty("tempo", tempo);
    obj->setProperty("type", juce::String(type));
    obj->setProperty("singerName", juce::String(singerName));
    obj->setProperty("avatar", juce::String(avatar));
    obj->setProperty("profileId", juce::String(profileId));
    obj->setProperty("deviceId", juce::String(deviceId));
    obj->setProperty("foxId", juce::String(foxId));
    obj->setProperty("kdId", juce::String(kdId));

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
Playing Playing::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Playing Playing::fromJsonObject(juce::DynamicObject* obj)
{
    Playing p;
    if (obj == nullptr) return p;

    p.album        = obj->getProperty("album").toString().toStdString();
    p.artists      = obj->getProperty("artists").toString().toStdString();
    p.durationMS   = (int)obj->getProperty("durationMS");
    p.isExplicit   = (bool)obj->getProperty("explicit");
    p.genres       = varToStrVec(obj->getProperty("genres"));
    p.id           = obj->getProperty("id").toString().toStdString();
    p.imageUrl     = obj->getProperty("imageUrl").toString().toStdString();
    p.keySignature = obj->getProperty("keySignature").toString().toStdString();
    p.lyricUrl     = obj->getProperty("lyricUrl").toString().toStdString();
    p.songName     = obj->getProperty("songName").toString().toStdString();
    p.songId       = obj->getProperty("songId").toString().toStdString();
    p.songVersion  = obj->getProperty("songVersion").toString().toStdString();
    p.pitch        = (float)(double)obj->getProperty("pitch");
    p.releaseDate  = obj->getProperty("releaseDate").toString().toStdString();
    p.tempo        = (double)obj->getProperty("tempo");
    p.type         = obj->getProperty("type").toString().toStdString();
    p.singerName   = obj->getProperty("singerName").toString().toStdString();
    p.avatar       = obj->getProperty("avatar").toString().toStdString();
    p.profileId    = obj->getProperty("profileId").toString().toStdString();
    p.deviceId     = obj->getProperty("deviceId").toString().toStdString();
    p.foxId        = obj->getProperty("foxId").toString().toStdString();
    p.kdId         = obj->getProperty("kdId").toString().toStdString();

    return p;
}

//==============================================================================
bool Playing::isValid() const
{
    return !songId.empty() && !songName.empty();
}

juce::String Playing::getFormattedDuration() const
{
    int totalSeconds = durationMS / 1000;
    return juce::String::formatted("%d:%02d", totalSeconds / 60, totalSeconds % 60);
}
