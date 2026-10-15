/*
  ==============================================================================

    AppLookAndFeel.h

    App-wide default LookAndFeel, built from DesignTokens. Installed once via
    juce::LookAndFeel::setDefaultLookAndFeel() in Main.cpp so any component
    that doesn't attach its own custom LookAndFeel (MenuTheme, LoginTheme,
    the Mixer fader, the Pitch/Volume knobs) inherits the new palette instead
    of falling back to stock JUCE grey.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AppLookAndFeel();
};
