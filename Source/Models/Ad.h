/*
  ==============================================================================

    Ad.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Ad model - advertisement media and scheduling

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
struct Ad
{
    std::string id;
    std::string mediaUrl;
    std::string mediaType;
    int64_t createdDate = 0;                // Epoch ms
    std::string createdBy;
    int64_t startDate = 0;                  // Epoch ms
    int64_t endDate = 0;                    // Epoch ms
    int plays = 0;
    std::string title;
    std::string description;
    int duration = 0;                       // Duration in seconds

    //==============================================================================
    juce::String toJson() const;
    static Ad fromJson(const juce::String& json);
    static Ad fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;

    bool isActive() const
    {
        auto now = juce::Time::currentTimeMillis();
        return startDate <= now && endDate >= now;
    }
};
