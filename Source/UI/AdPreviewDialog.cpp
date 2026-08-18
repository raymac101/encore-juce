/*
  ==============================================================================

    AdPreviewDialog.cpp

  ==============================================================================
*/

#include "AdPreviewDialog.h"
#include "../Services/AdMediaCache.h"
#include "../Services/ImageCache.h"
#include "../Localization/LocalizationManager.h"

namespace
{
    constexpr int kDialogWidth  = 720;
    constexpr int kDialogHeight = 540;

    constexpr auto kBgColour     = 0xff1a2030;
    constexpr auto kBorderColour = 0xff2d3a5a;
    constexpr auto kTextColour   = 0xfff7f8fa;
    constexpr auto kMutedColour  = 0xffa4b0c4;
}

//==============================================================================
AdPreviewDialog::AdPreviewDialog (const AdMetadata& ad)
    : ad_ (ad)
{
    setSize (kDialogWidth, kDialogHeight);

    titleLabel_.setText (ad_.name, juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (18.0f)).boldened());
    addAndMakeVisible (titleLabel_);

    closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kMutedColour));
    closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kMutedColour));
    closeButton_.onClick = [this]()
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (closeButton_);

    statusLabel_.setJustificationType (juce::Justification::centred);
    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
    addAndMakeVisible (statusLabel_);

    if (ad_.isVideo())
    {
        videoComponent_ = std::make_unique<AdVideoPreviewComponent>();
        addAndMakeVisible (videoComponent_.get());
        statusLabel_.setText (LocalizationManager::getInstance().getText ("ads.loading"),
                              juce::dontSendNotification);
        statusLabel_.toFront (false);

        juce::Component::SafePointer<AdPreviewDialog> safe (this);
        AdMediaCache::getOrFetch (ad_.url, ad_.name,
            [safe] (bool ok, juce::File file, const juce::String& error)
            {
                if (safe == nullptr)
                    return;

                if (! ok)
                {
                    safe->statusLabel_.setText (error, juce::dontSendNotification);
                    return;
                }

                safe->videoComponent_->load (file,
                    [safe] (juce::Result result)
                    {
                        if (safe == nullptr)
                            return;

                        if (result.failed())
                        {
                            safe->statusLabel_.setText (result.getErrorMessage(), juce::dontSendNotification);
                            return;
                        }

                        safe->statusLabel_.setVisible (false);
                        safe->videoComponent_->play();
                    });
            });
    }
    else
    {
        imageComponent_.setImagePlacement (juce::RectanglePlacement::centred
                                           | juce::RectanglePlacement::onlyReduceInSize);
        addAndMakeVisible (imageComponent_);

        juce::Component::SafePointer<AdPreviewDialog> safe (this);
        auto img = ArtworkCache::getInstance().getOrFetch (ad_.url, [safe]()
        {
            if (safe == nullptr) return;
            auto loaded = ArtworkCache::getInstance().getOrFetch (safe->ad_.url, nullptr);
            if (loaded.isValid())
                safe->imageComponent_.setImage (loaded);
        });
        if (img.isValid())
            imageComponent_.setImage (img);
    }

    resized();
}

AdPreviewDialog::~AdPreviewDialog() = default;

//==============================================================================
void AdPreviewDialog::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (kBgColour));
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (juce::Colour (kBorderColour));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.f);
}

void AdPreviewDialog::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    auto titleRow = area.removeFromTop (28);
    closeButton_.setBounds (titleRow.removeFromRight (28));
    titleLabel_.setBounds (titleRow);
    area.removeFromTop (10);

    if (videoComponent_ != nullptr)
        videoComponent_->setBounds (area);
    else
        imageComponent_.setBounds (area);

    statusLabel_.setBounds (area);
}

//==============================================================================
void AdPreviewDialog::launch (juce::Component* parent, const AdMetadata& ad)
{
    auto content = std::make_unique<AdPreviewDialog> (ad);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (content.release());
    opts.dialogTitle                  = LocalizationManager::getInstance().getText ("ads.preview.title");
    opts.dialogBackgroundColour       = juce::Colour (kBgColour);
    opts.componentToCentreAround      = parent;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar            = false;
    opts.resizable                    = false;
    opts.launchAsync();
}
