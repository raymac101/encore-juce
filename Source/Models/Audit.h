/*
  ==============================================================================

    Audit.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Audit and UserAudit models - session logging

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

//==============================================================================
/** Basic audit record for a song performance */
struct Audit
{
    std::string venueId;
    std::string venueName;
    std::string songName;
    std::string songId;
    int64_t date = 0;                       // Epoch ms
    std::string artist;
    std::string singerId;
    std::string source;                     // "phone", "host", "unknown"
    std::string deviceId;

    //==============================================================================
    juce::String toJson() const;
    static Audit fromJson(const juce::String& json);
    static Audit fromJsonObject(juce::DynamicObject* obj);
};

//==============================================================================
/** Detailed user audit record with full track metadata */
struct UserAudit
{
    std::string venueId;
    std::string venueName;
    int64_t date = 0;                       // Epoch ms
    std::string album;
    std::string artist;
    int durationMS = 0;
    bool isExplicit = false;
    std::vector<std::string> genres;
    std::string id;
    std::string imageUrl;
    std::string keySignature;
    std::string lyricUrl;
    std::string songName;
    std::string songId;
    std::string releaseDate;
    double tempo = 0.0;
    std::string type;
    std::string singerName;
    std::string avatar;
    std::string profileId;
    std::string deviceId;
    std::string foxId;
    std::string songVersion;
    float pitch = 0.0f;
    std::string kjId;

    //==============================================================================
    juce::String toJson() const;
    static UserAudit fromJson(const juce::String& json);
    static UserAudit fromJsonObject(juce::DynamicObject* obj);
};
