/*
  ==============================================================================

    QueueItem.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Queue item data model implementation

  ==============================================================================
*/

#include "QueueItem.h"

//==============================================================================
juce::String QueueItem::toJson() const
{
    juce::DynamicObject::Ptr jsonObject = new juce::DynamicObject();
    
    jsonObject->setProperty("id", juce::String(id));
    jsonObject->setProperty("deviceId", juce::String(deviceId));
    jsonObject->setProperty("singerName", juce::String(singerName));
    jsonObject->setProperty("singerAvatar", juce::String(singerAvatar));
    jsonObject->setProperty("songId", juce::String(songId));
    jsonObject->setProperty("songName", juce::String(songName));
    jsonObject->setProperty("songArtist", juce::String(songArtist));
    jsonObject->setProperty("songVersion", juce::String(songVersion));
    jsonObject->setProperty("duration", duration);
    jsonObject->setProperty("order", order);
    jsonObject->setProperty("songOrder", songOrder);
    jsonObject->setProperty("pitch", pitch);
    jsonObject->setProperty("status", juce::String(status));
    jsonObject->setProperty("time", juce::String(time));
    jsonObject->setProperty("alerts", alerts ? "true" : "false");
    jsonObject->setProperty("dateAdded", (int64)dateAdded);
    jsonObject->setProperty("key", juce::String(key));
    jsonObject->setProperty("volume", volume);
    
    juce::var jsonVar(jsonObject.get());
    return juce::JSON::toString(jsonVar);
}

//==============================================================================
QueueItem QueueItem::fromJson(const juce::String& json)
{
    QueueItem item;
    
    juce::var parsedJson = juce::JSON::parse(json);
    
    if (parsedJson.isObject())
    {
        juce::DynamicObject* obj = parsedJson.getDynamicObject();
        
        if (obj != nullptr)
        {
            item.id = obj->getProperty("id").toString().toStdString();
            item.deviceId = obj->getProperty("deviceId").toString().toStdString();
            item.singerName = obj->getProperty("singerName").toString().toStdString();
            item.singerAvatar = obj->getProperty("singerAvatar").toString().toStdString();
            item.songId = obj->getProperty("songId").toString().toStdString();
            item.songName = obj->getProperty("songName").toString().toStdString();
            item.songArtist = obj->getProperty("songArtist").toString().toStdString();
            item.songVersion = obj->getProperty("songVersion").toString().toStdString();
            item.duration = (int)obj->getProperty("duration");
            item.order = (int)obj->getProperty("order");
            item.songOrder = (int)obj->getProperty("songOrder");
            item.pitch = (float)obj->getProperty("pitch");
            item.status = obj->getProperty("status").toString().toStdString();
            item.time = obj->getProperty("time").toString().toStdString();
            item.alerts = (bool)obj->getProperty("alerts");
            item.dateAdded = (int64)obj->getProperty("dateAdded");
            item.key = (int)obj->getProperty("key");
            item.volume = (float)obj->getProperty("volume");
        }
    }
    
    return item;
}

//==============================================================================
bool QueueItem::isValid() const
{
    return !id.empty() && !singerName.empty() && !songId.empty() && !songName.empty();
}

//==============================================================================
juce::String QueueItem::getFormattedDuration() const
{
    int minutes = duration / 60;
    int seconds = duration % 60;
    
    return juce::String::formatted("%d:%02d", minutes, seconds);
}

//==============================================================================
juce::String QueueItem::getLocalizedStatus() const
{
    // TODO: Implement proper localization when LocalizationManager is ready
    if (status == "queued") return "Queued";
    if (status == "playing") return "Now Playing";
    if (status == "completed") return "Completed";
    if (status == "skipped") return "Skipped";
    
    return juce::String(status);
}

//==============================================================================
juce::String QueueItem::getEstimatedWaitTime(int averageSongLength) const
{
    if (order <= 0) return "Up Next";
    
    int totalWaitSeconds = order * averageSongLength;
    int hours = totalWaitSeconds / 3600;
    int minutes = (totalWaitSeconds % 3600) / 60;
    
    if (hours > 0)
        return juce::String::formatted("%d:%02d hrs", hours, minutes);
    else
        return juce::String::formatted("%d mins", minutes);
}