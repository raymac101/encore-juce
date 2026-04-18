/*
  ==============================================================================

    VenueItem.cpp
    Created: 15 Apr 2026 7:04:12pm
    Author:  GitHub Copilot

    Venue data model implementation

  ==============================================================================
*/

#include "VenueItem.h"

//==============================================================================
juce::String VenueItem::toJson() const
{
    juce::DynamicObject::Ptr jsonObject = new juce::DynamicObject();
    
    jsonObject->setProperty("id", juce::String(id));
    jsonObject->setProperty("name", juce::String(name));
    jsonObject->setProperty("address", juce::String(address));
    jsonObject->setProperty("city", juce::String(city));
    jsonObject->setProperty("country", juce::String(country));
    jsonObject->setProperty("adminEmail", juce::String(adminEmail));
    jsonObject->setProperty("password", juce::String(password));
    jsonObject->setProperty("code", juce::String(code));
    jsonObject->setProperty("codePlus", juce::String(codePlus));
    jsonObject->setProperty("updateSongs", (int64)updateSongs);
    jsonObject->setProperty("dateTime", (int64)dateTime);
    jsonObject->setProperty("songBookUrl", juce::String(songBookUrl));
    jsonObject->setProperty("logoUrl", juce::String(logoUrl));
    jsonObject->setProperty("background", background);
    jsonObject->setProperty("showOnlineSongs", showOnlineSongs);
    jsonObject->setProperty("numSongs", numSongs);
    jsonObject->setProperty("numStrikes", numStrikes);
    jsonObject->setProperty("repeatSongs", repeatSongs);
    jsonObject->setProperty("autoapprove", autoapprove);
    jsonObject->setProperty("language", juce::String(language));
    jsonObject->setProperty("timezone", juce::String(timezone));
    
    juce::var jsonVar(jsonObject.get());
    return juce::JSON::toString(jsonVar);
}

//==============================================================================
VenueItem VenueItem::fromJson(const juce::String& json)
{
    VenueItem venue;
    
    juce::var parsedJson = juce::JSON::parse(json);
    
    if (parsedJson.isObject())
    {
        juce::DynamicObject* obj = parsedJson.getDynamicObject();
        
        if (obj != nullptr)
        {
            venue.id = obj->getProperty("id").toString().toStdString();
            venue.name = obj->getProperty("name").toString().toStdString();
            venue.address = obj->getProperty("address").toString().toStdString();
            venue.city = obj->getProperty("city").toString().toStdString();
            venue.country = obj->getProperty("country").toString().toStdString();
            venue.adminEmail = obj->getProperty("adminEmail").toString().toStdString();
            venue.password = obj->getProperty("password").toString().toStdString();
            venue.code = obj->getProperty("code").toString().toStdString();
            venue.codePlus = obj->getProperty("codePlus").toString().toStdString();
            venue.updateSongs = (int64)obj->getProperty("updateSongs");
            venue.dateTime = (int64)obj->getProperty("dateTime");
            venue.songBookUrl = obj->getProperty("songBookUrl").toString().toStdString();
            venue.logoUrl = obj->getProperty("logoUrl").toString().toStdString();
            venue.background = (int)obj->getProperty("background");
            venue.showOnlineSongs = (bool)obj->getProperty("showOnlineSongs");
            venue.numSongs = (int)obj->getProperty("numSongs");
            venue.numStrikes = (int)obj->getProperty("numStrikes");
            venue.repeatSongs = (bool)obj->getProperty("repeatSongs");
            venue.autoapprove = (bool)obj->getProperty("autoapprove");
            venue.language = obj->getProperty("language").toString().toStdString();
            venue.timezone = obj->getProperty("timezone").toString().toStdString();
        }
    }
    
    return venue;
}

//==============================================================================
bool VenueItem::isValid() const
{
    return !id.empty() && !name.empty() && !code.empty();
}

//==============================================================================
juce::String VenueItem::getLocalizedName() const
{
    // For now, return the standard name
    // TODO: Implement localization lookup when LocalizationManager is ready
    return juce::String(name);
}