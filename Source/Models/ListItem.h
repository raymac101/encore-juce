/*
  ==============================================================================

    ListItem.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    ListItem model - generic selectable list item for UI

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
struct ListItem
{
    std::string id;
    std::string name;
    std::string url;
    std::string icon;
    bool selected = false;
};
