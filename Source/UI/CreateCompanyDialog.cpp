/*
  ==============================================================================

    CreateCompanyDialog.cpp

  ==============================================================================
*/

#include "CreateCompanyDialog.h"
#include "BorderlessModalWindow.h"
#include "../Localization/LocalizationManager.h"
#include "../Services/CompanyService.h"
#include "../Services/FirestoreClient.h"

namespace
{
    constexpr int kDialogWidth  = 420;
    constexpr int kDialogHeight = 400;

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
CreateCompanyDialog::CreateCompanyDialog()
{
    auto& lm = LocalizationManager::getInstance();

    setSize (kDialogWidth, kDialogHeight);

    titleLabel_.setText (lm.getText ("onboarding.company_info.heading"), juce::dontSendNotification);
    titleLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    titleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (20.0f)).boldened());
    addAndMakeVisible (titleLabel_);

    subtitleLabel_.setText (lm.getText ("onboarding.company_info.subtitle"), juce::dontSendNotification);
    subtitleLabel_.setColour (juce::Label::textColourId, juce::Colour (kMutedColour));
    subtitleLabel_.setFont (juce::Font (juce::FontOptions().withHeight (13.0f)));
    addAndMakeVisible (subtitleLabel_);

    closeButton_.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kMutedColour));
    closeButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kMutedColour));
    closeButton_.onClick = [this]() { closeDialog(); };
    addAndMakeVisible (closeButton_);

    styleField (nameLabel_, lm.getText ("onboarding.company_info.name_label"));
    addAndMakeVisible (nameLabel_);
    styleEditor (nameEditor_, lm.getText ("onboarding.company_info.name_placeholder"));
    addAndMakeVisible (nameEditor_);

    styleField (addressLabel_, lm.getText ("onboarding.company_info.address_label"));
    addAndMakeVisible (addressLabel_);
    styleEditor (addressEditor_, lm.getText ("onboarding.company_info.address_placeholder"));
    addAndMakeVisible (addressEditor_);

    styleField (cityLabel_, lm.getText ("onboarding.company_info.city_label"));
    addAndMakeVisible (cityLabel_);
    styleEditor (cityEditor_, lm.getText ("onboarding.company_info.city_placeholder"));
    addAndMakeVisible (cityEditor_);

    styleField (countryLabel_, lm.getText ("onboarding.company_info.country_label"));
    addAndMakeVisible (countryLabel_);
    styleEditor (countryEditor_, lm.getText ("onboarding.company_info.country_placeholder"));
    addAndMakeVisible (countryEditor_);

    statusLabel_.setColour (juce::Label::textColourId, juce::Colour (kTextColour));
    statusLabel_.setFont (juce::Font (juce::FontOptions().withHeight (12.0f)));
    statusLabel_.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel_);

    cancelButton_.setButtonText (lm.getText ("button.cancel"));
    cancelButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kPanelColour));
    cancelButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (kTextColour));
    cancelButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (kTextColour));
    cancelButton_.onClick = [this]() { closeDialog(); };
    addAndMakeVisible (cancelButton_);

    createButton_.setButtonText (lm.getText ("company.create_dialog.create_button"));
    createButton_.setColour (juce::TextButton::buttonColourId, juce::Colour (kAccentColour));
    createButton_.setColour (juce::TextButton::textColourOnId,  juce::Colour (0xff0d1527));
    createButton_.setColour (juce::TextButton::textColourOffId, juce::Colour (0xff0d1527));
    createButton_.onClick = [this]() { submit(); };
    addAndMakeVisible (createButton_);
}

CreateCompanyDialog::~CreateCompanyDialog() = default;

//==============================================================================
void CreateCompanyDialog::setStatus (const juce::String& message, bool isError)
{
    statusLabel_.setText (message, juce::dontSendNotification);
    statusLabel_.setColour (juce::Label::backgroundColourId,
                            isError ? juce::Colour (kStatusErrBg) : juce::Colours::transparentBlack);
}

void CreateCompanyDialog::submit()
{
    auto& lm = LocalizationManager::getInstance();

    const auto name = nameEditor_.getText().trim();
    if (name.isEmpty())
    {
        setStatus (lm.getText ("onboarding.company_info.error_name_required"), true);
        return;
    }

    createButton_.setEnabled (false);
    cancelButton_.setEnabled (false);
    setStatus (lm.getText ("onboarding.company_info.status_creating"), false);

    Company c;
    c.name        = name.toStdString();
    c.address     = addressEditor_.getText().trim().toStdString();
    c.city        = cityEditor_.getText().trim().toStdString();
    c.country     = countryEditor_.getText().trim().toStdString();
    c.ownerUserId = FirestoreClient::getInstance().getUserId().toStdString();

    juce::Component::SafePointer<CreateCompanyDialog> safe (this);
    CompanyService::getInstance().createCompany (c,
        [safe, c] (bool ok, juce::String companyId, juce::String error)
        {
            if (safe == nullptr) return;

            if (! ok)
            {
                safe->createButton_.setEnabled (true);
                safe->cancelButton_.setEnabled (true);
                safe->setStatus (error.isNotEmpty() ? error
                    : LocalizationManager::getInstance().getText ("onboarding.company_info.error_generic"), true);
                return;
            }

            CompanyService::getInstance().addCompanyMember (
                companyId, FirestoreClient::getInstance().getUserId(), "company_admin", nullptr);

            Company created = c;
            created.id = companyId.toStdString();

            auto cb = safe->onCreated;

            if (auto* dw = safe->findParentComponentOfClass<juce::DocumentWindow>())
                dw->exitModalState (0);

            if (cb)
                juce::MessageManager::callAsync ([cb, created]() { cb (created); });
        });
}

void CreateCompanyDialog::closeDialog()
{
    if (auto* dw = findParentComponentOfClass<juce::DocumentWindow>())
        dw->exitModalState (0);
}

//==============================================================================
void CreateCompanyDialog::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (kBgColour));
    g.fillRoundedRectangle (r, 12.f);
    g.setColour (juce::Colour (kBorderColour));
    g.drawRoundedRectangle (r.reduced (0.5f), 12.f, 1.f);
}

void CreateCompanyDialog::resized()
{
    auto area = getLocalBounds().reduced (20, 14);

    auto titleRow = area.removeFromTop (28);
    closeButton_.setBounds (titleRow.removeFromRight (28));
    titleLabel_.setBounds (titleRow);
    subtitleLabel_.setBounds (area.removeFromTop (20));
    area.removeFromTop (10);

    nameLabel_.setBounds (area.removeFromTop (16));
    nameEditor_.setBounds (area.removeFromTop (32));
    area.removeFromTop (10);

    addressLabel_.setBounds (area.removeFromTop (16));
    addressEditor_.setBounds (area.removeFromTop (32));
    area.removeFromTop (10);

    {
        auto row = area.removeFromTop (16);
        const int half = (row.getWidth() - 12) / 2;
        cityLabel_.setBounds (row.removeFromLeft (half));
        row.removeFromLeft (12);
        countryLabel_.setBounds (row);

        auto editRow = area.removeFromTop (32);
        cityEditor_.setBounds (editRow.removeFromLeft (half));
        editRow.removeFromLeft (12);
        countryEditor_.setBounds (editRow);
    }
    area.removeFromTop (10);

    statusLabel_.setBounds (area.removeFromTop (20));

    auto actionRow = getLocalBounds().removeFromBottom (56).reduced (20, 12);
    const int gap  = 10;
    const int btnW = (actionRow.getWidth() - gap) / 2;
    cancelButton_.setBounds (actionRow.removeFromLeft (btnW));
    actionRow.removeFromLeft (gap);
    createButton_.setBounds (actionRow);
}

//==============================================================================
void CreateCompanyDialog::launch (juce::Component* parent,
                                  std::function<void (const Company&)> onCreated)
{
    auto* content = new CreateCompanyDialog();
    content->onCreated = std::move (onCreated);
    BorderlessModalWindow::launch (parent, content, juce::Colour (kBgColour));
}
