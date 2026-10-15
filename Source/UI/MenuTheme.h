#pragma once

#include <JuceHeader.h>
#include "DesignTokens.h"

namespace MenuTheme
{
    // Was an independent navy palette (0xff1b2f5d family) -- repointed onto
    // DesignTokens so MainArea's pages/dialogs finally match the chrome
    // instead of reading as a second app. The translucent white input/glass
    // overlays below are left as-is; they're surface-agnostic by design.
    static constexpr uint32_t kBgTop           = DesignTokens::kSurface2;
    static constexpr uint32_t kBgBottom        = DesignTokens::kBg;
    static constexpr uint32_t kPanelFill       = DesignTokens::kSurface;
    static constexpr uint32_t kPanelBorder     = DesignTokens::kBorder;
    static constexpr uint32_t kCardFill        = DesignTokens::kSurface2;
    static constexpr uint32_t kCardFillBottom  = DesignTokens::kBg;
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
