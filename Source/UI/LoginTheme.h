/*
  ==============================================================================

    LoginTheme.h

    Brand-matched look-and-feel shared by the login screen and the onboarding
    wizard. Mirrors the Angular auth.component.scss: translucent inputs with
    white border, primary button in the blue gradient, secondary button as a
    ghost outline, over a blue-to-black gradient background.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace LoginTheme
{
    static constexpr uint32_t kAccentBlue       = 0xff4272b8;
    static constexpr uint32_t kAccentBlueLight  = 0xff5a8fd8;
    static constexpr uint32_t kInputFill        = 0x1affffff; // rgba(255,255,255,0.10)
    static constexpr uint32_t kInputFillFocus   = 0x26ffffff; // rgba(255,255,255,0.15)
    static constexpr uint32_t kInputBorder      = 0x33ffffff; // rgba(255,255,255,0.20)
    static constexpr uint32_t kInputBorderFocus = 0x66ffffff; // rgba(255,255,255,0.40)
    static constexpr uint32_t kCardFill         = 0x1affffff;
    static constexpr uint32_t kCardBorder       = 0x33ffffff;
    static constexpr uint32_t kPlaceholder      = 0x99ffffff; // rgba(255,255,255,0.60)
    static constexpr uint32_t kBodyText         = 0xffffffff;
    static constexpr uint32_t kSubtleText       = 0xe6ffffff; // rgba(255,255,255,0.90)
    static constexpr uint32_t kDividerText      = 0x80ffffff;
    static constexpr uint32_t kErrorText        = 0xfff87171; // readable red on a dark bg
    static constexpr uint32_t kSuccessText      = 0xff34d399; // readable green on a dark bg

    // Paints the standard top-to-bottom blue -> black gradient used behind
    // both the login screen and the onboarding wizard.
    inline void paintGradientBackground (juce::Graphics& g, juce::Rectangle<int> bounds)
    {
        auto b = bounds.toFloat();
        juce::ColourGradient bg (juce::Colour (kAccentBlue), b.getCentreX(), 0.0f,
                                  juce::Colours::black,       b.getCentreX(), b.getBottom(),
                                  false);
        g.setGradientFill (bg);
        g.fillRect (b);
    }
}

class LoginLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LoginLookAndFeel()
    {
        setColour (juce::TextEditor::backgroundColourId,    juce::Colour (LoginTheme::kInputFill));
        setColour (juce::TextEditor::textColourId,          juce::Colour (LoginTheme::kBodyText));
        setColour (juce::TextEditor::highlightColourId,     juce::Colour (LoginTheme::kAccentBlue).withAlpha (0.5f));
        setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
        setColour (juce::TextEditor::outlineColourId,       juce::Colour (LoginTheme::kInputBorder));
        setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (LoginTheme::kInputBorderFocus));
        setColour (juce::CaretComponent::caretColourId,     juce::Colours::white);

        setColour (juce::Label::textColourId,               juce::Colour (LoginTheme::kBodyText));

        setColour (juce::TextButton::buttonColourId,        juce::Colour (LoginTheme::kAccentBlue));
        setColour (juce::TextButton::buttonOnColourId,      juce::Colour (LoginTheme::kAccentBlueLight));
        setColour (juce::TextButton::textColourOffId,       juce::Colours::white);
        setColour (juce::TextButton::textColourOnId,        juce::Colours::white);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int /*height*/) override
    {
        return juce::Font (juce::FontOptions (14.0f)).withStyle (juce::Font::plain);
    }

    juce::Font getLabelFont (juce::Label& l) override
    {
        return l.getFont();
    }

    // Translucent rounded-rect inputs to match `.form_input`.
    void fillTextEditorBackground (juce::Graphics& g, int width, int height,
                                   juce::TextEditor& e) override
    {
        const float r = 10.0f;
        g.setColour (e.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (0.0f, 0.0f, (float) width, (float) height, r);
    }

    void drawTextEditorOutline (juce::Graphics& g, int width, int height,
                                juce::TextEditor& e) override
    {
        const float r = 10.0f;
        const auto colour = e.hasKeyboardFocus (true)
                                ? e.findColour (juce::TextEditor::focusedOutlineColourId)
                                : e.findColour (juce::TextEditor::outlineColourId);
        g.setColour (colour);
        g.drawRoundedRectangle (1.0f, 1.0f,
                                (float) width  - 2.0f,
                                (float) height - 2.0f,
                                r, 2.0f);
    }

    // Pill-shaped buttons. Primary buttons (button-primary) get the blue
    // gradient; ghost buttons (button-secondary) are translucent white with a
    // subtle border.
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& /*backgroundColour*/,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float r = juce::jmin (12.0f, bounds.getHeight() * 0.5f);
        const bool ghost = button.getProperties().getWithDefault ("ghost", false);
        const bool greenSelect = button.getProperties().getWithDefault ("greenSelect", false);
        const bool enabled = button.isEnabled();

        if (! enabled)
        {
            g.setColour (juce::Colour (0x33ffffff));
            g.fillRoundedRectangle (bounds, r);
            return;
        }

        if (greenSelect)
        {
            // Solid green pill (matches the SELECT button in the mock-up).
            const auto base = juce::Colour (0xff10b981);
            g.setColour (shouldDrawButtonAsHighlighted ? base.brighter (0.10f) : base);
            g.fillRoundedRectangle (bounds, r);
            if (shouldDrawButtonAsDown)
            {
                g.setColour (juce::Colours::black.withAlpha (0.10f));
                g.fillRoundedRectangle (bounds, r);
            }
            return;
        }

        if (ghost)
        {
            const float a = shouldDrawButtonAsHighlighted ? 0.20f : 0.10f;
            g.setColour (juce::Colours::white.withAlpha (a));
            g.fillRoundedRectangle (bounds, r);

            g.setColour (juce::Colour (LoginTheme::kInputBorder));
            g.drawRoundedRectangle (bounds, r, 1.5f);
            return;
        }

        // Primary: vertical blue gradient.
        const auto top    = juce::Colour (LoginTheme::kAccentBlueLight);
        const auto bottom = juce::Colour (LoginTheme::kAccentBlue);

        juce::ColourGradient grad (
            shouldDrawButtonAsHighlighted ? top.brighter (0.05f) : top,
            bounds.getCentreX(), bounds.getY(),
            shouldDrawButtonAsHighlighted ? bottom.brighter (0.05f) : bottom,
            bounds.getCentreX(), bounds.getBottom(),
            false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, r);

        if (shouldDrawButtonAsDown)
        {
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            g.fillRoundedRectangle (bounds, r);
        }
    }

    juce::Font getTextButtonFontForButton (juce::TextButton& b)
    {
        const bool greenSelect = b.getProperties().getWithDefault ("greenSelect", false);
        return juce::Font (juce::FontOptions (greenSelect ? 13.0f : 14.0f))
                  .withStyle (greenSelect ? juce::Font::bold : juce::Font::plain);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool /*shouldDrawButtonAsHighlighted*/,
                         bool /*shouldDrawButtonAsDown*/) override
    {
        g.setFont (getTextButtonFontForButton (button));
        g.setColour (juce::Colours::white);
        g.drawFittedText (button.getButtonText(),
                          button.getLocalBounds().reduced (2),
                          juce::Justification::centred, 1);
    }
};
