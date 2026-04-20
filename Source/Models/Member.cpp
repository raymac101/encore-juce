/*
  ==============================================================================

    Member.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Member and MemberHistory model implementation

  ==============================================================================
*/

#include "Member.h"

//==============================================================================
juce::String MemberHistory::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("artist", juce::String(artist));
    obj->setProperty("song", juce::String(song));
    obj->setProperty("songId", juce::String(songId));
    obj->setProperty("songVersion", juce::String(songVersion));
    obj->setProperty("duration", duration);
    obj->setProperty("pitch", (double)pitch);
    obj->setProperty("kjId", juce::String(kjId));
    obj->setProperty("dateTime", (juce::int64)dateTime);
    return juce::JSON::toString(juce::var(obj.get()));
}

MemberHistory MemberHistory::fromJsonObject(juce::DynamicObject* obj)
{
    MemberHistory h;
    if (obj == nullptr) return h;
    h.artist      = obj->getProperty("artist").toString().toStdString();
    h.song        = obj->getProperty("song").toString().toStdString();
    h.songId      = obj->getProperty("songId").toString().toStdString();
    h.songVersion = obj->getProperty("songVersion").toString().toStdString();
    h.duration    = (int)obj->getProperty("duration");
    h.pitch       = (float)(double)obj->getProperty("pitch");
    h.kjId        = obj->getProperty("kjId").toString().toStdString();
    h.dateTime    = (int64_t)obj->getProperty("dateTime");
    return h;
}

//==============================================================================
juce::String Member::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", juce::String(id));
    obj->setProperty("singerName", juce::String(singerName));
    obj->setProperty("profileId", juce::String(profileId));
    obj->setProperty("firstDate", (juce::int64)firstDate);
    obj->setProperty("lastDate", (juce::int64)lastDate);

    juce::Array<juce::var> histArr;
    for (auto& h : memberHistory)
    {
        juce::var parsed = juce::JSON::parse(h.toJson());
        histArr.add(parsed);
    }
    obj->setProperty("memberHistory", juce::var(histArr));

    return juce::JSON::toString(juce::var(obj.get()));
}

Member Member::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Member Member::fromJsonObject(juce::DynamicObject* obj)
{
    Member m;
    if (obj == nullptr) return m;

    m.id         = obj->getProperty("id").toString().toStdString();
    m.singerName = obj->getProperty("singerName").toString().toStdString();
    m.profileId  = obj->getProperty("profileId").toString().toStdString();
    m.firstDate  = (int64_t)obj->getProperty("firstDate");
    m.lastDate   = (int64_t)obj->getProperty("lastDate");

    auto histVar = obj->getProperty("memberHistory");
    if (histVar.isArray())
        for (int i = 0; i < histVar.size(); ++i)
            if (auto* histObj = histVar[i].getDynamicObject())
                m.memberHistory.push_back(MemberHistory::fromJsonObject(histObj));

    return m;
}

bool Member::isValid() const
{
    return !id.empty() && !singerName.empty();
}
