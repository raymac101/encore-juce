/*
  ==============================================================================

    SfxLibraryListPanel.cpp

  ==============================================================================
*/

#include "SfxLibraryListPanel.h"
#include "SpriteIcon.h"
#include "../Localization/LocalizationManager.h"

namespace
{
    // Matches RibbonMenu.cpp's card/icon colours for visual consistency
    // with the 8 slots sitting directly above this panel.
    const auto kTileBg   = juce::Colour (0xff243047);
    const auto kTileHover = juce::Colour (0xff2a3955);
    const auto kIconTint = juce::Colour (0xffd3d7de);
    const auto kText     = juce::Colours::white;

    constexpr int kTileW = 100;
    constexpr int kTileH = 88;
    constexpr int kTileGap = 10;
}

//==============================================================================
class SfxLibraryListPanel::SoundTile : public juce::Component,
                                       public juce::SettableTooltipClient
{
public:
    SoundTile (const SfxLibraryService::Entry& entry, SfxLibraryListPanel& owner)
        : entry_ (entry), owner_ (owner)
    {
        icon_ = entry_.iconFile.existsAsFile()
            ? SpriteIcon::createFromSvgFile (entry_.iconFile, kIconTint)
            : SpriteIcon::create ("icon-volume-high", kIconTint);

        nameLabel_.setText (entry_.name, juce::dontSendNotification);
        nameLabel_.setJustificationType (juce::Justification::centred);
        nameLabel_.setFont (juce::Font (juce::FontOptions().withHeight (11.0f)));
        nameLabel_.setColour (juce::Label::textColourId, kText);
        nameLabel_.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (nameLabel_);

        setTooltip (entry_.name);
    }

    const juce::String& getSoundName() const noexcept { return entry_.name; }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        g.setColour (isMouseOver() ? kTileHover : kTileBg);
        g.fillRoundedRectangle (b, 8.0f);

        if (icon_ != nullptr)
        {
            auto iconArea = getLocalBounds().withTrimmedBottom (22).reduced (16);
            icon_->drawWithin (g, iconArea.toFloat(), juce::RectanglePlacement::centred, 1.0f);
        }
    }

    void resized() override
    {
        nameLabel_.setBounds (getLocalBounds().removeFromBottom (20).reduced (2, 0));
    }

    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit  (const juce::MouseEvent&) override { repaint(); }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasDraggedSinceMouseDown())
            return;

        if (owner_.onPreviewRequested)
            owner_.onPreviewRequested (entry_.name);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() < 6)
            return;

        if (auto* dnd = juce::DragAndDropContainer::findParentDragContainerFor (this))
        {
            if (! dnd->isDragAndDropActive())
            {
                auto img = createComponentSnapshot (getLocalBounds(), true);
                dnd->startDragging (juce::var (entry_.name), this, juce::ScaledImage (img),
                                    /*allowDraggingToOtherWindows*/ false);
            }
        }
    }

private:
    SfxLibraryService::Entry entry_;
    SfxLibraryListPanel& owner_;
    juce::Label nameLabel_;
    std::unique_ptr<juce::Drawable> icon_;
};

//==============================================================================
// Plain wrapping-grid content component living inside the Viewport. Not a
// juce::Component::resized() override -- layout is driven explicitly by
// layoutTilesForWidth(), called from the owning panel, so the "set my own
// size based on how many rows I just laid out" step can't recurse into
// itself via JUCE's own resize notifications.
class SfxLibraryListPanel::ListContent : public juce::Component
{
public:
    explicit ListContent (SfxLibraryListPanel& owner)
    {
        for (auto& entry : SfxLibraryService::getInstance().getAllSounds())
        {
            auto tile = std::make_unique<SoundTile> (entry, owner);
            addAndMakeVisible (*tile);
            tiles_.push_back (std::move (tile));
        }
    }

    void setFilter (const juce::String& filterLower)
    {
        for (auto& tile : tiles_)
            tile->setVisible (filterLower.isEmpty()
                               || tile->getSoundName().toLowerCase().contains (filterLower));
    }

    // Lays out every currently-visible tile in a wrapping grid for the
    // given width, then sets this component's own size to fit -- the
    // Viewport reads that size to decide how far it can scroll.
    void layoutTilesForWidth (int width)
    {
        const int columns = juce::jmax (1, (width - kTileGap) / (kTileW + kTileGap));

        int col = 0;
        int row = 0;
        for (auto& tile : tiles_)
        {
            if (! tile->isVisible())
                continue;

            tile->setBounds (kTileGap + col * (kTileW + kTileGap),
                             kTileGap + row * (kTileH + kTileGap),
                             kTileW, kTileH);
            if (++col >= columns) { col = 0; ++row; }
        }

        const int totalRows = (col == 0) ? row : row + 1;
        setSize (width, kTileGap + juce::jmax (1, totalRows) * (kTileH + kTileGap));
    }

private:
    std::vector<std::unique_ptr<SoundTile>> tiles_;
};

//==============================================================================
SfxLibraryListPanel::SfxLibraryListPanel()
{
    content_ = std::make_unique<ListContent> (*this);

    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (content_.get(), false);
    viewport_.setScrollBarsShown (true, false);

    addAndMakeVisible (filterEditor_);
    filterEditor_.setTextToShowWhenEmpty (
        LocalizationManager::getInstance().getText ("ribbon.sfx.filter_placeholder"),
        kText.withAlpha (0.5f));
    filterEditor_.onTextChange = [this] { applyFilter(); };
}

SfxLibraryListPanel::~SfxLibraryListPanel() = default;

void SfxLibraryListPanel::applyFilter()
{
    content_->setFilter (filterEditor_.getText().trim().toLowerCase());
    content_->layoutTilesForWidth (viewport_.getMaximumVisibleWidth());
}

void SfxLibraryListPanel::resized()
{
    auto area = getLocalBounds();
    filterEditor_.setBounds (area.removeFromTop (28));
    area.removeFromTop (6);
    viewport_.setBounds (area);
    content_->layoutTilesForWidth (viewport_.getMaximumVisibleWidth());
}
