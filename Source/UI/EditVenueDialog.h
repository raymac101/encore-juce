/*
  ==============================================================================

    EditVenueDialog.h

    Edits name/address/city/country/logo for an arbitrary venue (not
    necessarily the one this PC is signed into) -- reachable from a venue
    card's Edit button in CompanyAdminPage. VenueService::saveVenue()/
    uploadLogo() already take an explicit venueId, so this is purely new UI
    over an already-generic data layer.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/VenueItem.h"

class EditVenueDialog : public juce::Component
{
public:
    explicit EditVenueDialog (const juce::String& venueId);
    ~EditVenueDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Fired once, on the message thread, when the dialog closes. `changed`
        is true only if a save actually succeeded (so the caller knows
        whether to refresh its own venue list). */
    std::function<void (bool changed)> onClosed;

    static void launch (juce::Component* parent,
                        const juce::String& venueId,
                        std::function<void (bool changed)> onClosed);

private:
    void loadVenue();
    void save();
    void closeDialog (bool changed);
    void setStatus (const juce::String& message, bool isError);

    juce::String venueId_;
    VenueItem venue_;
    bool changed_ = false;

    juce::Label      titleLabel_;
    juce::TextButton closeButton_ { "X" };

    juce::Label      nameLabel_;
    juce::TextEditor nameEditor_;
    juce::Label      addressLabel_;
    juce::TextEditor addressEditor_;
    juce::Label      cityLabel_;
    juce::TextEditor cityEditor_;
    juce::Label      countryLabel_;
    juce::TextEditor countryEditor_;

    juce::Label      logoLabel_;
    juce::Label      logoPathLabel_;
    juce::Image      logoPreview_;
    juce::TextButton browseLogoButton_ { "Browse Logo" };
    juce::File       selectedLogoFile_;
    std::unique_ptr<juce::FileChooser> fileChooser_;

    juce::Rectangle<int> logoPreviewBounds_;

    juce::Label      statusLabel_;
    juce::TextButton cancelButton_;
    juce::TextButton saveButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EditVenueDialog)
};
