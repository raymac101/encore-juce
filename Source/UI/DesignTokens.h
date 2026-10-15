/*
  ==============================================================================

    DesignTokens.h

    Single source of truth for Encore's colour palette. Before this file,
    TopBar/NavBar/BottomBar/QueueBar each independently declared their own
    (mostly-matching) grey+cyan constants, while MenuTheme (navy) and
    LoginTheme (blue gradient) invented two more unrelated palettes, plus a
    grab-bag of one-off hex literals scattered through dialogs and pages.
    kAccent below is not a new colour -- it's the exact 0xff30daff the chrome
    bars already used in five different files; this header just gives it one
    home so the next screen reaches for DesignTokens instead of inventing a
    sixth copy.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace DesignTokens
{
    // Neutral scale. kSurface/kSurfaceHover/kSurface2 are the pre-existing
    // chrome values (TopBar/NavBar/BottomBar/QueueBar's 0xff262626 panel bg,
    // NavBar's 0xff161616 hover, BottomBar's 0xff1e1e1e section bg) --
    // centralised, not changed. kBg is the one new tone: a darker page
    // canvas for MainArea, replacing MenuTheme's old navy gradient.
    static constexpr uint32_t kBg           = 0xff141414;
    static constexpr uint32_t kSurface      = 0xff262626;
    static constexpr uint32_t kSurfaceHover = 0xff161616;
    static constexpr uint32_t kSurface2     = 0xff1e1e1e;
    static constexpr uint32_t kBorder       = 0xff3a3a3a;

    static constexpr uint32_t kText      = 0xffffffff;
    static constexpr uint32_t kTextMuted = 0xffb8c1cf; // already used by TopBar's Key/BPM values
    static constexpr uint32_t kTextDim   = 0xff8a8a8a;

    static constexpr uint32_t kAccent     = 0xff30daff; // Encore's existing brand accent -- unchanged value
    static constexpr uint32_t kAccentSoft = 0x2630daff; // ~15% alpha, for highlight fills/rings

    static constexpr uint32_t kSuccess    = 0xff34d399;
    static constexpr uint32_t kWarning    = 0xfffbbf24;
    static constexpr uint32_t kDanger     = 0xfff87171; // replaces four independent "reds"
    static constexpr uint32_t kDangerSoft = 0x26f87171;

    inline juce::Colour colour(uint32_t argb) noexcept { return juce::Colour(argb); }

    // Type scale (point sizes), mirroring the design review's display/
    // heading/body/label/mono hierarchy. Not yet wired everywhere -- use
    // when touching a component's fonts so sizes converge over time.
    static constexpr float kFontDisplay = 28.0f;
    static constexpr float kFontHeading = 18.0f;
    static constexpr float kFontBody    = 14.0f;
    static constexpr float kFontLabel   = 11.0f;
    static constexpr float kFontMono    = 13.0f;
}
