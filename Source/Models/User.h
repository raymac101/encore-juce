/*
  ==============================================================================

    User.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    User authentication model - migrated from TypeScript User class

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
/**
    Represents an authenticated user session.
*/
struct User
{
    std::string id;
    std::string email;
    std::string token;
    int64_t tokenExpirationDate = 0;        // Epoch ms

    //==============================================================================
    /** Returns the token if still valid, or empty string if expired */
    std::string getUserToken() const
    {
        if (tokenExpirationDate <= juce::Time::currentTimeMillis())
            return {};
        return token;
    }

    bool isTokenValid() const
    {
        return !token.empty() && tokenExpirationDate > juce::Time::currentTimeMillis();
    }

    //==============================================================================
    juce::String toJson() const;
    static User fromJson(const juce::String& json);
    static User fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
};
