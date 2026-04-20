/*
  ==============================================================================

    Emoji.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Emoji model - real-time emoji reactions

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <string>

//==============================================================================
/**
    Represents a real-time emoji reaction on screen.
    Note: HTMLImageElement replaced with JUCE Image for native rendering.
*/
struct Emoji
{
    std::string id;
    std::string emojiName;
    std::string sender;
    std::string source;
    float xPos = 0.0f;
    float yPos = 0.0f;
    juce::Image img;
    float speed = 1.0f;
    float alpha = 1.0f;
    float alphaRate = 0.01f;
    int current = 0;
    int totalFrames = 0;
    int width = 0;
    int height = 0;
    bool isDeleting = false;
    bool isLoading = false;

    //==============================================================================
    /** Update position and alpha for animation frame */
    void update()
    {
        yPos -= speed;
        alpha -= alphaRate;
        ++current;
    }

    bool isFinished() const
    {
        return alpha <= 0.0f || current >= totalFrames || isDeleting;
    }
};
