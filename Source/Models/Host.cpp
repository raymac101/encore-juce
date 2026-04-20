/*
  ==============================================================================

    Host.cpp
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Host data model implementation

  ==============================================================================
*/

#include "Host.h"

//==============================================================================
juce::String Host::toJson() const
{
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();

    obj->setProperty("userId", juce::String(userId));
    obj->setProperty("email", juce::String(email));
    obj->setProperty("profileId", juce::String(profileId));
    obj->setProperty("avatarUrl", juce::String(avatarUrl));
    obj->setProperty("stageName", juce::String(stageName));
    obj->setProperty("fullName", juce::String(fullName));
    obj->setProperty("birthday", juce::String(birthday));
    obj->setProperty("country", juce::String(country));
    obj->setProperty("city", juce::String(city));
    obj->setProperty("gender", juce::String(gender));
    obj->setProperty("signUpDate", juce::String(signUpDate));
    obj->setProperty("lastLogin", juce::String(lastLogin));
    obj->setProperty("role", juce::String(AccessRightsUtil::userRoleToString(role)));
    obj->setProperty("accessExpirationDate", (juce::int64)accessExpirationDate);

    return juce::JSON::toString(juce::var(obj.get()));
}

//==============================================================================
Host Host::fromJson(const juce::String& json)
{
    juce::var parsed = juce::JSON::parse(json);
    if (parsed.isObject())
        return fromJsonObject(parsed.getDynamicObject());
    return {};
}

Host Host::fromJsonObject(juce::DynamicObject* obj)
{
    Host h;
    if (obj == nullptr) return h;

    h.userId               = obj->getProperty("userId").toString().toStdString();
    h.email                = obj->getProperty("email").toString().toStdString();
    h.profileId            = obj->getProperty("profileId").toString().toStdString();
    h.avatarUrl            = obj->getProperty("avatarUrl").toString().toStdString();
    h.stageName            = obj->getProperty("stageName").toString().toStdString();
    h.fullName             = obj->getProperty("fullName").toString().toStdString();
    h.birthday             = obj->getProperty("birthday").toString().toStdString();
    h.country              = obj->getProperty("country").toString().toStdString();
    h.city                 = obj->getProperty("city").toString().toStdString();
    h.gender               = obj->getProperty("gender").toString().toStdString();
    h.signUpDate           = obj->getProperty("signUpDate").toString().toStdString();
    h.lastLogin            = obj->getProperty("lastLogin").toString().toStdString();
    h.role                 = AccessRightsUtil::stringToUserRole(obj->getProperty("role").toString().toStdString());
    h.accessExpirationDate = (int64_t)obj->getProperty("accessExpirationDate");

    return h;
}

//==============================================================================
bool Host::isValid() const
{
    return !userId.empty() && !email.empty();
}
