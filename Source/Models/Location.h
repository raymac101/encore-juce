/*
  ==============================================================================

    Location.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Location / geocoding models

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
struct Coordinates
{
    double lat = 0.0;
    double lng = 0.0;
};

//==============================================================================
struct PlaceLocation : public Coordinates
{
    std::string address;
    std::string staticMapImageUrl;

    juce::String toJson() const;
    static PlaceLocation fromJsonObject(juce::DynamicObject* obj);
};
