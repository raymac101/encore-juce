/*
  ==============================================================================

    Playlist.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Playlist model implementation

  ==============================================================================
*/

#include "Playlist.h"

//==============================================================================
juce::String Playlist::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", juce::String(id));
    obj->setProperty("imageUrl", juce::String(imageUrl));
    obj->setProperty("songName", juce::String(songName));
    obj->setProperty("artistName", juce::String(artistName));
    return juce::JSON::toString(juce::var(obj.get()));
}

Playlist Playlist::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Playlist Playlist::fromJsonObject(juce::DynamicObject* obj)
{
    Playlist p;
    if (obj == nullptr) return p;
    p.id         = obj->getProperty("id").toString().toStdString();
    p.imageUrl   = obj->getProperty("imageUrl").toString().toStdString();
    p.songName   = obj->getProperty("songName").toString().toStdString();
    p.artistName = obj->getProperty("artistName").toString().toStdString();
    return p;
}

bool Playlist::isValid() const
{
    return !id.empty() && !songName.empty();
}
