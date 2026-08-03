/*
  ==============================================================================

    Singers.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Singer data model implementation

  ==============================================================================
*/

#include "Singers.h"

//==============================================================================
juce::String Singers::toJson() const
{
    juce::DynamicObject::Ptr jsonObject = new juce::DynamicObject();
    
    jsonObject->setProperty("id", juce::String(id));
    jsonObject->setProperty("name", juce::String(name));
    jsonObject->setProperty("avatar", juce::String(avatar));
    jsonObject->setProperty("deviceId", juce::String(deviceId));
    jsonObject->setProperty("order", order);
    jsonObject->setProperty("rotationOrder", rotationOrder);
    jsonObject->setProperty("strikes", strikes);
    jsonObject->setProperty("songsPerformed", songsPerformed);
    jsonObject->setProperty("currentlyUp", currentlyUp);
    jsonObject->setProperty("lastSeen", (int64)lastSeen);
    jsonObject->setProperty("preferredLanguage", juce::String(preferredLanguage));
    jsonObject->setProperty("timezone", juce::String(timezone));
    
    // Convert songs array to JSON array
    juce::Array<juce::var> songsArray;
    for (const auto& song : songs)
    {
        juce::var songJson = juce::JSON::parse(song.toJson());
        songsArray.add(songJson);
    }
    jsonObject->setProperty("songs", songsArray);
    
    juce::var jsonVar(jsonObject.get());
    return juce::JSON::toString(jsonVar);
}

//==============================================================================
Singers Singers::fromJson(const juce::String& json)
{
    Singers singer;
    
    juce::var parsedJson = juce::JSON::parse(json);
    
    if (parsedJson.isObject())
    {
        juce::DynamicObject* obj = parsedJson.getDynamicObject();
        
        if (obj != nullptr)
        {
            singer.id = obj->getProperty("id").toString().toStdString();
            singer.name = obj->getProperty("name").toString().toStdString();
            singer.avatar = obj->getProperty("avatar").toString().toStdString();
            singer.deviceId = obj->getProperty("deviceId").toString().toStdString();
            singer.order = (int)obj->getProperty("order");
            singer.rotationOrder = (int)obj->getProperty("rotationOrder");
            singer.strikes = (int)obj->getProperty("strikes");
            singer.songsPerformed = (int)obj->getProperty("songsPerformed");
            singer.currentlyUp = (bool)obj->getProperty("currentlyUp");
            singer.lastSeen = (int64)obj->getProperty("lastSeen");
            singer.preferredLanguage = obj->getProperty("preferredLanguage").toString().toStdString();
            singer.timezone = obj->getProperty("timezone").toString().toStdString();
            
            // Parse songs array
            juce::var songsVar = obj->getProperty("songs");
            if (songsVar.isArray())
            {
                juce::Array<juce::var>* songsArray = songsVar.getArray();
                for (int i = 0; i < songsArray->size(); ++i)
                {
                    juce::String songJson = juce::JSON::toString(songsArray->getReference(i));
                    QueueItem song = QueueItem::fromJson(songJson);
                    singer.songs.push_back(song);
                }
            }
        }
    }
    
    return singer;
}

//==============================================================================
bool Singers::isValid() const
{
    return !id.empty() && !name.empty();
}

//==============================================================================
void Singers::addSong(const QueueItem& song)
{
    songs.push_back(song);
    updateLastSeen();
}

//==============================================================================
bool Singers::removeSong(const std::string& songId)
{
    auto it = std::find_if(songs.begin(), songs.end(),
        [&songId](const QueueItem& song) { return song.id == songId; });
    
    if (it != songs.end())
    {
        songs.erase(it);
        return true;
    }
    
    return false;
}

//==============================================================================
QueueItem* Singers::getNextSong()
{
    for (auto& song : songs)
    {
        if (song.status == "queued")
            return &song;
    }
    
    return nullptr;
}

//==============================================================================
int Singers::getTotalQueuedDuration() const
{
    int totalDuration = 0;
    
    for (const auto& song : songs)
    {
        if (song.status == "queued")
            totalDuration += song.duration;
    }
    
    return totalDuration;
}

//==============================================================================
juce::String Singers::getQueueSummary() const
{
    int queuedCount = 0;
    int totalDuration = 0;
    
    for (const auto& song : songs)
    {
        if (song.status == "queued")
        {
            queuedCount++;
            totalDuration += song.duration;
        }
    }
    
    if (queuedCount == 0)
        return "No songs queued";
    
    int minutes = totalDuration / 60;
    
    if (queuedCount == 1)
        return juce::String::formatted("1 song, %d mins", minutes);
    else
        return juce::String::formatted("%d songs, %d mins", queuedCount, minutes);
}

//==============================================================================
bool Singers::shouldBeRemoved(int maxStrikes) const
{
    return strikes >= maxStrikes;
}

//==============================================================================
void Singers::markSongPerformed()
{
    songsPerformed++;
    updateLastSeen();
}

//==============================================================================
void Singers::updateLastSeen()
{
    lastSeen = juce::Time::getCurrentTime().toMilliseconds();
}