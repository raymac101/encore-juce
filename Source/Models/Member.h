/*
  ==============================================================================

    Member.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Member and MemberHistory models - singer tracking

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

//==============================================================================
/** A single performance history entry for a member */
struct MemberHistory
{
    std::string artist;
    std::string song;
    std::string songId;
    std::string songVersion;
    int duration = 0;
    float pitch = 0.0f;
    std::string kjId;                       // Host ID
    int64_t dateTime = 0;                   // Epoch ms

    //==============================================================================
    juce::String toJson() const;
    static MemberHistory fromJsonObject(juce::DynamicObject* obj);
};

//==============================================================================
/**
    Represents a regular member/singer tracked across sessions.
    Maps to the Firestore 'members' subcollection under a venue.
*/
struct Member
{
    std::string id;
    std::string singerName;
    std::string profileId;
    int64_t firstDate = 0;                  // First visit (epoch ms)
    int64_t lastDate = 0;                   // Last visit (epoch ms)
    std::vector<MemberHistory> memberHistory;

    //==============================================================================
    juce::String toJson() const;
    static Member fromJson(const juce::String& json);
    static Member fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;

    /** Total number of performances */
    int getTotalPerformances() const { return (int)memberHistory.size(); }
};
