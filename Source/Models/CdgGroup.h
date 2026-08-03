/*
  ==============================================================================

    CdgGroup.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    CDG Group model - groups of related CDG files

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

//==============================================================================
/**
    Represents a group/collection of CDG karaoke files (e.g., a disc set).
*/
struct CdgGroup
{
    std::string id;
    std::string name;
    std::string type;                       // Group type identifier
    std::vector<std::string> data;          // Associated data entries

    //==============================================================================
    juce::String toJson() const;
    static CdgGroup fromJson(const juce::String& json);
    static CdgGroup fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;
};
