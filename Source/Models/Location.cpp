/*
  ==============================================================================

    Location.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Location model implementation

  ==============================================================================
*/

#include "Location.h"

//==============================================================================
juce::String PlaceLocation::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("lat", lat);
    obj->setProperty("lng", lng);
    obj->setProperty("address", juce::String(address));
    obj->setProperty("staticMapImageUrl", juce::String(staticMapImageUrl));
    return juce::JSON::toString(juce::var(obj.get()));
}

PlaceLocation PlaceLocation::fromJsonObject(juce::DynamicObject* obj)
{
    PlaceLocation l;
    if (obj == nullptr) return l;
    l.lat               = (double)obj->getProperty("lat");
    l.lng               = (double)obj->getProperty("lng");
    l.address           = obj->getProperty("address").toString().toStdString();
    l.staticMapImageUrl = obj->getProperty("staticMapImageUrl").toString().toStdString();
    return l;
}
