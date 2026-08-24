/*
  ==============================================================================

    EditVenueDialog.cpp

  ==============================================================================
*/

#include "EditVenueDialog.h"
#include "BorderlessModalWindow.h"
#include "../Localization/LocalizationManager.h"
#include "../Services/VenueService.h"
#include "../Services/ImageCache.h"

namespace
{
    constexpr int kDialogWidth  = 420;
    constexpr int kDialogHeight = 460;

    constexpr auto kBgColour     = 0xff1a2030;
    constexpr auto kPanelColour  = 0xff0d1527;
    constexpr auto kBorderColour = 0xff2d3a5a;
    constexpr auto kAccentColour = 0xff30daff;
    constexpr auto kTextColour   = 0xfff7f8fa;
    constexpr auto kMutedColour  = 0xffa4b0c4;
    constexpr auto kStatusErrBg  = 0xff7f1d1d;

    void styleField (juce::Label& lbl, const juce::String& text)
    {
        lbl.setText (text, juce::dontSendNotification);
        lbl.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
        lbl.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    }

    void styleEditor (juce::TextEditor& ed, const juce::String& placeholder)
    {
        ed.setTextToShowWhenEmpty (placeholder, juce::Colour (kMutedColour));
        ed.setColour (juce::TextEditor::backgroundColourId,     juce::Colour (kPanelColour));
        ed.setColour (juce::TextEditor::textColourId,           juce::Colour (kTextColour));
        ed.setColour (juce::TextEditor::outlineColourId,        juce::Colour (kBorderColour));
        ed.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour (kAccentColour));
        ed.setIndents (8, 5);
    }
}

//==============================================================================
EditVenueDialog::EditVenueDialog (const juce::String& venueId)
    : venueId_ (venueId)
{
    auto& lm = LocalizationManager::getInstance();

    setSize (kDialogWidth, kDialogHeight);

    titleLabel_.setText (lm.getText ("edit_venue.title"), juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (20.0f)).boldened());
    addAndMakeVisible (titleLabel_);

    closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kMutedColour));
    closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kMutedColour));
    closeButton_.onClick = [this]() { closeDialog (changed_); };
    addAndMakeVisible (closeButton_);

    styleField (nameLabel_, lm.getText ("edit_venue.name_label"));
    addAndMakeVisible (nameLabel_);
    styleEditor (nameEditor_, lm.getText ("edit_venue.name_placeholder"));
    addAndMakeVisible (nameEditor_);

    styleField (addressLabel_, lm.getText ("edit_venue.address_label"));
    addAndMakeVisible (addressLabel_);
    styleEditor (addressEditor_, lm.getText ("edit_venue.address_placeholder"));
    addAndMakeVisible (addressEditor_);

    styleField (cityLabel_, lm.getText ("edit_venue.city_label"));
    addAndMakeVisible (cityLabel_);
    styleEditor (cityEditor_, lm.getText ("edit_venue.city_placeholder"));
    addAndMakeVisible (cityEditor_);

    styleField (countryLabel_, lm.getText ("edit_venue.country_label"));
    addAndMakeVisible (countryLabel_);
    styleEditor (countryEditor_, lm.getText ("edit_venue.country_placeholder"));
    addAndMakeVisible (countryEditor_);

    styleField (logoLabel_, lm.getText ("edit_venue.logo_label"));
    addAndMakeVisible (logoLabel_);
    logoPathLabel_.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
    logoPathLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (logoPathLabel_);

    browseLogoButton_.setButtonText (lm.getText ("edit_venue.browse_logo"));
    browseLogoButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kPanelColour));
    browseLogoButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kTextColour));
    browseLogoButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kTextColour));
    browseLogoButton_.onClick = [this]()
    {
        fileChooser_ = std::make_unique<juce::FileChooser> (
            LocalizationManager::getInstance().getText ("edit_venue.browse_logo"),
            juce::File::getSpecialLocation (juce::File::userHomeDirectory),
            "*.png;*.jpg;*.jpeg;*.gif;*.webp");

        juce::Component::SafePointer<EditVenueDialog> safe (this);
        fileChooser_->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe] (const juce::FileChooser& chooser)
            {
                if (safe == nullptr) return;
                auto file = chooser.getResult();
                if (! file.existsAsFile()) return;

                safe->selectedLogoFile_ = file;
                safe->logoPathLabel_.setText (file.getFileName(), juce::dontSendNotification);
                safe->logoPreview_ = juce::ImageFileFormat::loadFrom (file);
                safe->repaint();
            });
    };
    addAndMakeVisible (browseLogoButton_);

    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    statusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    addAndMakeVisible (statusLabel_);

    cancelButton_.setButtonText (lm.getText ("button.cancel"));
    cancelButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kPanelColour));
    cancelButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kTextColour));
    cancelButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kTextColour));
    cancelButton_.onClick = [this]() { closeDialog (changed_); };
    addAndMakeVisible (cancelButton_);

    saveButton_.setButtonText (lm.getText ("button.save"));
    saveButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kAccentColour));
    saveButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff0d1527));
    saveButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0d1527));
    saveButton_.onClick = [this]() { save(); };
    addAndMakeVisible (saveButton_);

    setStatus (lm.getText ("edit_venue.loading"), false);
    loadVenue();
}

EditVenueDialog::~EditVenueDialog() = default;

//==============================================================================
void EditVenueDialog::loadVenue()
{
    juce::Component::SafePointer<EditVenueDialog> safe (this);
    VenueService::getInstance().loadVenue (venueId_,
        [safe] (bool ok, VenueItem venue, juce::String error)
        {
            if (safe == nullptr) return;

            if (! ok)
            {
                safe->setStatus (error.isNotEmpty() ? error
                    : LocalizationManager::getInstance().getText ("edit_venue.load_failed"), true);
                return;
            }

            safe->venue_ = venue;
            safe->nameEditor_.setText (juce::String (venue.name), juce::dontSendNotification);
            safe->addressEditor_.setText (juce::String (venue.address), juce::dontSendNotification);
            safe->cityEditor_.setText (juce::String (venue.city), juce::dontSendNotification);
            safe->countryEditor_.setText (juce::String (venue.country), juce::dontSendNotification);
            safe->setStatus ({}, false);

            if (venue.logoUrl.empty())
            {
                safe->logoPathLabel_.setText (LocalizationManager::getInstance().getText ("edit_venue.no_logo"), juce::dontSendNotification);
                return;
            }

            const auto logoUrl = juce::String (venue.logoUrl);
            safe->logoPathLabel_.setText (logoUrl, juce::dontSendNotification);
            auto img = ArtworkCache::getInstance().getOrFetch (logoUrl, [safe, logoUrl]()
            {
                if (safe == nullptr) return;
                auto loaded = ArtworkCache::getInstance().getOrFetch (logoUrl, nullptr);
                if (loaded.isValid())
                    safe->logoPreview_ = loaded;
                safe->repaint();
            });
            if (img.isValid())
                safe->logoPreview_ = img;
            safe->repaint();
        });
}

void EditVenueDialog::save()
{
    const auto name = nameEditor_.getText().trim();
    if (name.isEmpty())
    {
        setStatus (LocalizationManager::getInstance().getText ("edit_venue.name_required"), true);
        return;
    }

    saveButton_.setEnabled (false);
    cancelButton_.setEnabled (false);
    setStatus (LocalizationManager::getInstance().getText ("edit_venue.saving"), false);

    venue_.name    = name.toStdString();
    venue_.address = addressEditor_.getText().trim().toStdString();
    venue_.city    = cityEditor_.getText().trim().toStdString();
    venue_.country = countryEditor_.getText().trim().toStdString();

    juce::Component::SafePointer<EditVenueDialog> safe (this);
    const auto venueId = venueId_;
    const auto logoFile = selectedLogoFile_;

    auto finishSave = [safe, venueId] (VenueItem venue)
    {
        VenueService::getInstance().saveVenue (venueId, venue,
            [safe] (bool ok, juce::String error)
            {
                if (safe == nullptr) return;

                if (! ok)
                {
                    safe->saveButton_.setEnabled (true);
                    safe->cancelButton_.setEnabled (true);
                    safe->setStatus (error.isNotEmpty() ? error
                        : LocalizationManager::getInstance().getText ("edit_venue.save_failed"), true);
                    return;
                }

                safe->changed_ = true;
                safe->closeDialog (true);
            });
    };

    if (logoFile.existsAsFile())
    {
        VenueService::getInstance().uploadLogo (venueId, logoFile,
            [safe, finishSave] (bool ok, juce::String logoUrl, juce::String error)
            {
                if (safe == nullptr) return;

                if (! ok)
                {
                    safe->saveButton_.setEnabled (true);
                    safe->cancelButton_.setEnabled (true);
                    safe->setStatus (error.isNotEmpty() ? error
                        : LocalizationManager::getInstance().getText ("edit_venue.logo_upload_failed"), true);
                    return;
                }

                auto venue = safe->venue_;
                venue.logoUrl = logoUrl.toStdString();
                finishSave (venue);
            });
    }
    else
    {
        finishSave (venue_);
    }
}

void EditVenueDialog::setStatus (const juce::String& message, bool isError)
{
    statusLabel_.setText (message, juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::backgroundColourId,
                            isError ? juce::Colour (kStatusErrBg) : juce::Colours::transparentBlack);
}

void EditVenueDialog::closeDialog (bool changed)
{
    changed_ = changed;
    auto cb = onClosed;

    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->exitModalState (0);

    if (cb)
        juce::MessageManager::callAsync ([cb, changed]() { cb (changed); });
}

//==============================================================================
void EditVenueDialog::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (kBgColour));
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (juce::Colour (kBorderColour));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.f);

    if (! logoPreviewBounds_.isEmpty())
    {
        g.setColour (juce::Colour (kBorderColour));
        g.drawRoundedRectangle (logoPreviewBounds_.toFloat(), 6.0f, 1.0f);
        if (logoPreview_.isValid())
            g.drawImageWithin (logoPreview_, logoPreviewBounds_.getX() + 4, logoPreviewBounds_.getY() + 4,
                               logoPreviewBounds_.getWidth() - 8, logoPreviewBounds_.getHeight() - 8,
                               juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize,
                               false);
    }
}

void EditVenueDialog::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    auto titleRow = area.removeFromTop (28);
    closeButton_.setBounds (titleRow.removeFromRight (28));
    titleLabel_.setBounds (titleRow);
    area.removeFromTop (10);

    nameLabel_.setBounds (area.removeFromTop (16));
    nameEditor_.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);

    addressLabel_.setBounds (area.removeFromTop (16));
    addressEditor_.setBounds (area.removeFromTop (30));
    area.removeFromTop (8);

    {
        auto row = area.removeFromTop (16);
        const int half = (row.getWidth() - 12) / 2;
        cityLabel_.setBounds (row.removeFromLeft (half));
        row.removeFromLeft (12);
        countryLabel_.setBounds (row);

        auto editRow = area.removeFromTop (30);
        cityEditor_.setBounds (editRow.removeFromLeft (half));
        editRow.removeFromLeft (12);
        countryEditor_.setBounds (editRow);
    }
    area.removeFromTop (10);

    logoLabel_.setBounds (area.removeFromTop (16));
    auto logoRow = area.removeFromTop (60);
    logoPreviewBounds_ = logoRow.removeFromLeft (60);
    logoRow.removeFromLeft (8);
    browseLogoButton_.setBounds (logoRow.removeFromTop (28).withWidth (140));
    logoPathLabel_.setBounds (logoRow.withTrimmedTop (32));

    area.removeFromTop (8);
    statusLabel_.setBounds (area.removeFromTop (20));

    auto actionRow = getLocalBounds().removeFromBottom (56).reduced (20, 12);
    const int gap  = 10;
    const int btnW = (actionRow.getWidth() - gap) / 2;
    cancelButton_.setBounds (actionRow.removeFromLeft (btnW));
    actionRow.removeFromLeft (gap);
    saveButton_.setBounds (actionRow);
}

//==============================================================================
void EditVenueDialog::launch (juce::Component* parent,
                              const juce::String& venueId,
                              std::function<void (bool changed)> onClosed)
{
    auto* content = new EditVenueDialog (venueId);
    content->onClosed = std::move (onClosed);
    BorderlessModalWindow::launch (parent, content, juce::Colour (kBgColour));
}
