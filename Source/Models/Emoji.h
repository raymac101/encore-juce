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
    float speed = 60.0f;        // px/second (real-time, not per-tick)
    float alpha = 1.0f;
    float alphaRate = 0.35f;    // per-second fade rate, once fadeStartYPos is crossed
    int current = 0;
    int totalFrames = 0;
    int width = 0;
    int height = 0;
    bool isDeleting = false;
    bool isLoading = false;

    // Set once at spawn time from the host component's bounds, so update()
    // stays resolution-independent without needing to know screen size.
    float fadeStartYPos = 200.0f;
    float drawSize = 100.0f;
    float frameAccumulatorSeconds = 0.0f;
    static constexpr float framesPerSecond = 24.0f;

    //==============================================================================
    /** Advance position, fade and sprite frame by dtSeconds of real time. */
    void update (double dtSeconds)
    {
        yPos -= speed * (float) dtSeconds;

        if (yPos < fadeStartYPos)
            alpha = juce::jmax (0.0f, alpha - alphaRate * (float) dtSeconds);

        if (totalFrames > 0)
        {
            frameAccumulatorSeconds += (float) dtSeconds;
            const float frameDuration = 1.0f / framesPerSecond;
            while (frameAccumulatorSeconds >= frameDuration)
            {
                frameAccumulatorSeconds -= frameDuration;
                current = (current + 1) % totalFrames;
            }
        }
    }

    bool isFinished() const
    {
        // Rises off the top of the screen, same lifetime rule as the legacy
        // Angular animation (yPos < -10). Sprite frames loop continuously
        // rather than ending the animation, matching the original's
        // wrap-around frame stepping.
        return alpha <= 0.0f || yPos < -20.0f || isDeleting;
    }
};
