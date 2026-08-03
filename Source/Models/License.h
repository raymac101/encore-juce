/*
  ==============================================================================

    License.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    License model - software license validation

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
struct License
{
    std::string venueId;
    std::string venueName;
    std::string licenseKey;
    int64_t expiryDate = 0;                 // Epoch ms
    bool isValid_ = false;

    //==============================================================================
    juce::String toJson() const;
    static License fromJson(const juce::String& json);
    static License fromJsonObject(juce::DynamicObject* obj);

    bool isExpired() const
    {
        return expiryDate > 0 && expiryDate < juce::Time::currentTimeMillis();
    }
};
