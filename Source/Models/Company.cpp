/*
  ==============================================================================

    Company.cpp

  ==============================================================================
*/

#include "Company.h"

//==============================================================================
juce::String Company::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty("id",          juce::String(id));
    obj->setProperty("name",        juce::String(name));
    obj->setProperty("status",      juce::String(status));
    obj->setProperty("ownerUserId", juce::String(ownerUserId));
    obj->setProperty("address",     juce::String(address));
    obj->setProperty("city",        juce::String(city));
    obj->setProperty("country",     juce::String(country));
    obj->setProperty("logoUrl",     juce::String(logoUrl));
    return juce::JSON::toString(juce::var(obj.get()));
}

Company Company::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Company Company::fromJsonObject(juce::DynamicObject* obj)
{
    Company c;
    if (obj == nullptr) return c;
    c.id          = obj->getProperty("id").toString().toStdString();
    c.name        = obj->getProperty("name").toString().toStdString();
    c.status      = obj->getProperty("status").toString().toStdString();
    c.ownerUserId = obj->getProperty("ownerUserId").toString().toStdString();
    c.address     = obj->getProperty("address").toString().toStdString();
    c.city        = obj->getProperty("city").toString().toStdString();
    c.country     = obj->getProperty("country").toString().toStdString();
    c.logoUrl     = obj->getProperty("logoUrl").toString().toStdString();
    return c;
}

bool Company::isValid() const
{
    return ! name.empty() && ! ownerUserId.empty();
}
