#include "MenuTheme.h"

namespace
{
    class MenuLookAndFeel final : public juce::LookAndFeel_V4
    {
    public:
        MenuLookAndFeel()
        {
            setColour(juce::TextEditor::backgroundColourId, juce::Colour(MenuTheme::kInputFill));
            setColour(juce::TextEditor::textColourId, juce::Colours::white);
            setColour(juce::TextEditor::outlineColourId, juce::Colour(MenuTheme::kInputBorder));
            setColour(juce::TextEditor::focusedOutlineColourId, juce::Colour(MenuTheme::kInputBorderFocus));
            setColour(juce::CaretComponent::caretColourId, juce::Colours::white);

            setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff0d1630));
            setColour(juce::ComboBox::textColourId, juce::Colours::white);
            setColour(juce::ComboBox::outlineColourId, juce::Colour(MenuTheme::kInputBorder));
            setColour(juce::ComboBox::arrowColourId, juce::Colours::white.withAlpha(0.75f));

            setColour(juce::TextButton::buttonColourId, juce::Colour(0xff4272b8));
            setColour(juce::TextButton::textColourOffId, juce::Colours::white);
            setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        }

        void fillTextEditorBackground(juce::Graphics& g, int width, int height, juce::TextEditor& e) override
        {
            const float radius = 10.0f;
            auto fill = e.findColour(juce::TextEditor::backgroundColourId);
            if (e.hasKeyboardFocus(true))
                fill = fill.interpolatedWith(juce::Colour(MenuTheme::kInputFillFocus), 0.65f);

            g.setColour(fill);
            g.fillRoundedRectangle(0.0f, 0.0f, (float) width, (float) height, radius);
        }

        void drawTextEditorOutline(juce::Graphics& g, int width, int height, juce::TextEditor& e) override
        {
            const float radius = 10.0f;
            const auto c = e.hasKeyboardFocus(true)
                ? e.findColour(juce::TextEditor::focusedOutlineColourId)
                : e.findColour(juce::TextEditor::outlineColourId);

            g.setColour(c);
            g.drawRoundedRectangle(1.0f, 1.0f, (float) width - 2.0f, (float) height - 2.0f, radius, 1.5f);
        }

        void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                  const juce::Colour& backgroundColour,
                                  bool highlighted, bool down) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
            const float radius = juce::jmin(12.0f, bounds.getHeight() * 0.45f);
            const bool ghost = button.getProperties().getWithDefault("ghost", false);

            if (! button.isEnabled())
            {
                g.setColour(juce::Colours::white.withAlpha(0.18f));
                g.fillRoundedRectangle(bounds, radius);
                return;
            }

            if (ghost)
            {
                g.setColour(juce::Colours::white.withAlpha(highlighted ? 0.20f : 0.10f));
                g.fillRoundedRectangle(bounds, radius);
                g.setColour(juce::Colour(MenuTheme::kInputBorder));
                g.drawRoundedRectangle(bounds, radius, 1.2f);
                return;
            }

            auto base = backgroundColour;
            if (base.isTransparent())
                base = button.findColour(juce::TextButton::buttonColourId);

            auto top = base.brighter(highlighted ? 0.20f : 0.12f);
            auto bottom = base.darker(0.15f);

            juce::ColourGradient grad(top, bounds.getCentreX(), bounds.getY(),
                                      bottom, bounds.getCentreX(), bounds.getBottom(), false);
            g.setGradientFill(grad);
            g.fillRoundedRectangle(bounds, radius);

            g.setColour(juce::Colours::white.withAlpha(0.16f));
            g.drawRoundedRectangle(bounds, radius, 1.0f);

            if (down)
            {
                g.setColour(juce::Colours::black.withAlpha(0.12f));
                g.fillRoundedRectangle(bounds, radius);
            }
        }

        void drawComboBox(juce::Graphics& g, int width, int height,
                          bool, int, int, int, int, juce::ComboBox& box) override
        {
            auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.5f);
            const float radius = 10.0f;

            g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
            g.fillRoundedRectangle(r, radius);

            g.setColour(box.findColour(juce::ComboBox::outlineColourId));
            g.drawRoundedRectangle(r, radius, 1.3f);

            auto arrow = juce::Rectangle<float>((float) width - 18.0f, (float) height * 0.5f - 3.0f, 8.0f, 6.0f);
            juce::Path p;
            p.startNewSubPath(arrow.getX(), arrow.getY());
            p.lineTo(arrow.getRight(), arrow.getY());
            p.lineTo(arrow.getCentreX(), arrow.getBottom());
            p.closeSubPath();
            g.setColour(box.findColour(juce::ComboBox::arrowColourId));
            g.fillPath(p);
        }
    };
}

juce::LookAndFeel_V4& MenuTheme::getLookAndFeel()
{
    static MenuLookAndFeel lnf;
    return lnf;
}

void MenuTheme::drawPageBackground(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto f = bounds.toFloat();
    juce::ColourGradient bg(juce::Colour(kBgTop), f.getCentreX(), f.getY(),
                            juce::Colour(kBgBottom), f.getCentreX(), f.getBottom(), false);
    g.setGradientFill(bg);
    g.fillRect(bounds);
}

void MenuTheme::drawHeaderPanel(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    auto rf = bounds.toFloat();
    g.setColour(juce::Colour(kPanelFill));
    g.fillRoundedRectangle(rf, 12.0f);

    g.setColour(juce::Colour(kPanelBorder));
    g.drawRoundedRectangle(rf.reduced(0.5f), 12.0f, 1.0f);

    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRoundedRectangle(juce::Rectangle<float>(rf.getX(), rf.getY(), rf.getWidth(), 18.0f), 12.0f);
}

void MenuTheme::drawGlassCard(juce::Graphics& g, juce::Rectangle<float> bounds, float radius)
{
    // Soft drop shadow
    juce::DropShadow shadow(juce::Colours::black.withAlpha(0.35f), 10, {0, 2});
    juce::Path shadowPath;
    shadowPath.addRoundedRectangle(bounds, radius);
    shadow.drawForPath(g, shadowPath);

    // Gradient fill with slight transparency
    juce::ColourGradient gradient(juce::Colour(kCardFill), bounds.getX(), bounds.getY(),
                                   juce::Colour(kCardFillBottom), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill(gradient);
    g.fillRoundedRectangle(bounds, radius);

    // Subtle highlight at the top (white fade)
    g.setColour(juce::Colours::white.withAlpha(0.06f));
    g.fillRoundedRectangle(juce::Rectangle<float>(bounds.getX(), bounds.getY(), bounds.getWidth(), 16.0f), radius);

    // Single pixel border
    g.setColour(juce::Colour(kCardBorder).withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.reduced(0.5f), radius, 1.0f);
}
