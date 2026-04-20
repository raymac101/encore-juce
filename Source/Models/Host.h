/*
  ==============================================================================

    Host.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Host data model - migrated from TypeScript Host class

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include "AccessRights.h"

//==============================================================================
/**
    Represents a karaoke host profile from the Firestore 'hosts' collection.
*/
struct Host
{
    std::string userId;
    std::string email;
    std::string profileId;
    std::string avatarUrl;
    std::string stageName;
    std::string fullName;
    std::string birthday;
    std::string country;
    std::string city;
    std::string gender;
    std::string signUpDate;
    std::string lastLogin;
    UserRole role = UserRole::Basic;
    int64_t accessExpirationDate = 0;       // Epoch ms

    //==============================================================================
    juce::String toJson() const;
    static Host fromJson(const juce::String& json);
    static Host fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
};
