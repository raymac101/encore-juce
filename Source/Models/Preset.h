/*
  ==============================================================================

    Preset.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Preset model - lyric display styling presets

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
struct Preset
{
    std::string id;
    std::string preset;                     // Preset name
    std::string font;
    float fontSize = 24.0f;
    bool bold = false;
    bool italic = false;
    bool glow = false;
    float lineSpacing = 1.2f;
    int numLines = 4;
    std::string highlightLyricFill;         // Colour string for highlighted lyrics
    std::string highlightLyricOutline;      // Colour string for highlighted outline
    std::string normLyricFill;              // Colour string for normal lyrics
    std::string normLyricOutline;           // Colour string for normal outline

    //==============================================================================
    juce::String toJson() const;
    static Preset fromJson(const juce::String& json);
    static Preset fromJsonObject(juce::DynamicObject* obj);
    bool isValid() const;

    /** Convert fill/outline strings to JUCE Colours */
    juce::Colour getHighlightFillColour() const { return juce::Colour::fromString(juce::String(highlightLyricFill)); }
    juce::Colour getHighlightOutlineColour() const { return juce::Colour::fromString(juce::String(highlightLyricOutline)); }
    juce::Colour getNormFillColour() const { return juce::Colour::fromString(juce::String(normLyricFill)); }
    juce::Colour getNormOutlineColour() const { return juce::Colour::fromString(juce::String(normLyricOutline)); }
};
