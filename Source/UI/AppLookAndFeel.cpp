#include "AppLookAndFeel.h"
#include "DesignTokens.h"

AppLookAndFeel::AppLookAndFeel()
{
    using namespace DesignTokens;

    setColour(juce::ResizableWindow::backgroundColourId, colour(kBg));
    setColour(juce::DocumentWindow::backgroundColourId, colour(kBg));

    setColour(juce::TextButton::buttonColourId, colour(kSurface2));
    setColour(juce::TextButton::buttonOnColourId, colour(kAccent));
    setColour(juce::TextButton::textColourOffId, colour(kText));
    setColour(juce::TextButton::textColourOnId, colour(kBg));

    setColour(juce::ToggleButton::textColourId, colour(kText));
    setColour(juce::ToggleButton::tickColourId, colour(kAccent));
    setColour(juce::ToggleButton::tickDisabledColourId, colour(kTextDim));

    setColour(juce::Slider::backgroundColourId, colour(kSurface2));
    setColour(juce::Slider::trackColourId, colour(kSurface2));
    setColour(juce::Slider::thumbColourId, colour(kAccent));
    setColour(juce::Slider::rotarySliderFillColourId, colour(kAccent));
    setColour(juce::Slider::rotarySliderOutlineColourId, colour(kSurface2));

    setColour(juce::TextEditor::backgroundColourId, colour(kSurface2));
    setColour(juce::TextEditor::textColourId, colour(kText));
    setColour(juce::TextEditor::outlineColourId, colour(kBorder));
    setColour(juce::TextEditor::focusedOutlineColourId, colour(kAccent));
    setColour(juce::CaretComponent::caretColourId, colour(kAccent));

    setColour(juce::ComboBox::backgroundColourId, colour(kSurface2));
    setColour(juce::ComboBox::textColourId, colour(kText));
    setColour(juce::ComboBox::outlineColourId, colour(kBorder));
    setColour(juce::ComboBox::arrowColourId, colour(kTextMuted));

    setColour(juce::Label::textColourId, colour(kText));

    setColour(juce::PopupMenu::backgroundColourId, colour(kSurface));
    setColour(juce::PopupMenu::textColourId, colour(kText));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, colour(kAccent));
    setColour(juce::PopupMenu::highlightedTextColourId, colour(kBg));

    setColour(juce::ScrollBar::thumbColourId, colour(kBorder));
    setColour(juce::ScrollBar::trackColourId, colour(kSurface));

    setColour(juce::AlertWindow::backgroundColourId, colour(kSurface));
    setColour(juce::AlertWindow::textColourId, colour(kText));
    setColour(juce::AlertWindow::outlineColourId, colour(kBorder));
}
