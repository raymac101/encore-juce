#pragma once

#include <JuceHeader.h>

namespace MenuTheme
{
    static constexpr uint32_t kBgTop           = 0xff1b2f5d;
    static constexpr uint32_t kBgBottom        = 0xff0f1832;
    static constexpr uint32_t kPanelFill       = 0xff182a52;
    static constexpr uint32_t kPanelBorder     = 0xff4f78c4;
    static constexpr uint32_t kCardFill        = 0xff1a1f2a;
    static constexpr uint32_t kCardFillBottom  = 0xff141820;
    static constexpr uint32_t kCardBorder      = 0x24000000;
    static constexpr uint32_t kInputFill       = 0x1affffff;
    static constexpr uint32_t kInputFillFocus  = 0x26ffffff;
    static constexpr uint32_t kInputBorder     = 0x33ffffff;
    static constexpr uint32_t kInputBorderFocus= 0x66ffffff;

    juce::LookAndFeel_V4& getLookAndFeel();

    void drawPageBackground(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawHeaderPanel(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawGlassCard(juce::Graphics& g, juce::Rectangle<float> bounds, float radius = 10.0f);
}
