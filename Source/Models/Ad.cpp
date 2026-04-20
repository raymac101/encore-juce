/*
  ==============================================================================

    Ad.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Ad model implementation

  ==============================================================================
*/

#include "Ad.h"

//==============================================================================
juce::String Ad::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id", juce::String(id));
    obj->setProperty("mediaUrl", juce::String(mediaUrl));
    obj->setProperty("mediaType", juce::String(mediaType));
    obj->setProperty("createdDate", (juce::int64)createdDate);
    obj->setProperty("createdBy", juce::String(createdBy));
    obj->setProperty("startDate", (juce::int64)startDate);
    obj->setProperty("endDate", (juce::int64)endDate);
    obj->setProperty("plays", plays);
    obj->setProperty("title", juce::String(title));
    obj->setProperty("description", juce::String(description));
    obj->setProperty("duration", duration);
    return juce::JSON::toString(juce::var(obj.get()));
}

Ad Ad::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Ad Ad::fromJsonObject(juce::DynamicObject* obj)
{
    Ad a;
    if (obj == nullptr) return a;
    a.id          = obj->getProperty("id").toString().toStdString();
    a.mediaUrl    = obj->getProperty("mediaUrl").toString().toStdString();
    a.mediaType   = obj->getProperty("mediaType").toString().toStdString();
    a.createdDate = (int64_t)obj->getProperty("createdDate");
    a.createdBy   = obj->getProperty("createdBy").toString().toStdString();
    a.startDate   = (int64_t)obj->getProperty("startDate");
    a.endDate     = (int64_t)obj->getProperty("endDate");
    a.plays       = (int)obj->getProperty("plays");
    a.title       = obj->getProperty("title").toString().toStdString();
    a.description = obj->getProperty("description").toString().toStdString();
    a.duration    = (int)obj->getProperty("duration");
    return a;
}

bool Ad::isValid() const
{
    return !id.empty() && !mediaUrl.empty();
}
