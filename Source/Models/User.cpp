/*
  ==============================================================================

    User.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    User authentication model implementation

  ==============================================================================
*/

#include "User.h"

//==============================================================================
juce::String User::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("id", juce::String(id));
    obj->setProperty("email", juce::String(email));
    obj->setProperty("token", juce::String(token));
    obj->setProperty("tokenExpirationDate", (juce::int64)tokenExpirationDate);

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
User User::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

User User::fromJsonObject(juce::DynamicObject* obj)
{
    User u;
    if (obj == nullptr) return u;

    u.id                  = obj->getProperty("id").toString().toStdString();
    u.email               = obj->getProperty("email").toString().toStdString();
    u.token               = obj->getProperty("token").toString().toStdString();
    u.tokenExpirationDate = (int64_t)obj->getProperty("tokenExpirationDate");

    return u;
}

//==============================================================================
bool User::isValid() const
{
    return !id.empty() && !email.empty();
}
