/*
  ==============================================================================

    License.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    License model implementation

  ==============================================================================
*/

#include "License.h"

//==============================================================================
juce::String License::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("venueId", juce::String(venueId));
    obj->setProperty("venueName", juce::String(venueName));
    obj->setProperty("licenseKey", juce::String(licenseKey));
    obj->setProperty("expiryDate", (juce::int64)expiryDate);
    obj->setProperty("isValid", isValid_);
    return juce::JSON::toString(juce::var(obj.get()));
}

License License::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

License License::fromJsonObject(juce::DynamicObject* obj)
{
    License l;
    if (obj == nullptr) return l;
    l.venueId    = obj->getProperty("venueId").toString().toStdString();
    l.venueName  = obj->getProperty("venueName").toString().toStdString();
    l.licenseKey = obj->getProperty("licenseKey").toString().toStdString();
    l.expiryDate = (int64_t)obj->getProperty("expiryDate");
    l.isValid_   = (bool)obj->getProperty("isValid");
    return l;
}
