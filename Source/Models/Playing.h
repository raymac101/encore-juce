/*
  ==============================================================================

    Playing.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Playing model - currently playing track state

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

//==============================================================================
/**
    Represents the currently playing track state, including singer info.
*/
struct Playing
{
    std::string album;
    std::string artists;
    int durationMS = 0;
    bool isExplicit = false;
    std::vector<std::string> genres;
    std::string id;
    std::string imageUrl;
    std::string keySignature;
    std::string lyricUrl;
    std::string songName;
    std::string songId;
    std::string songVersion;
    float pitch = 0.0f;
    std::string releaseDate;
    double tempo = 0.0;
    std::string type;                       // "karaoke" or "singalong"

    // Singer info
    std::string singerName;
    std::string avatar;
    std::string profileId;
    std::string deviceId;
    std::string foxId;
    std::string kdId;                       // Host's ID

    //==============================================================================
    juce::String toJson() const;
    static Playing fromJson(const juce::String& json);
    static Playing fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
    juce::String getFormattedDuration() const;
};
