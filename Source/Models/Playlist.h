/*
  ==============================================================================

    Playlist.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Playlist model - playlist entry

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
struct Playlist
{
    std::string id;
    std::string imageUrl;
    std::string songName;
    std::string artistName;

    //==============================================================================
    juce::String toJson() const;
    static Playlist fromJson(const juce::String& json);
    static Playlist fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
};
