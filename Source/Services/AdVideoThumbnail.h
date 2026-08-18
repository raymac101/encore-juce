#pragma once

#include <juce_graphics/juce_graphics.h>

class AdVideoThumbnail
{
public:
    static juce::Image create (const juce::File& videoFile, int maximumSize);
};