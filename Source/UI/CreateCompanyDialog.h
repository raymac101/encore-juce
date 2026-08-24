/*
  ==============================================================================

    CreateCompanyDialog.h

    Self-service "Create Company" dialog, reachable from the TopBar user
    menu's "My Company" item (see MainComponent.cpp) for a signed-in user who
    doesn't yet have a company. Collects the same fields as OnboardingWizard's
    CompanyInfoStep (name/address/city/country) and runs the identical
    CompanyService::createCompany() + addCompanyMember() sequence -- that
    step lives inline in OnboardingWizard.cpp and isn't exported from a
    header, so this is a small standalone duplicate of its form-building
    rather than a shared component, kept intentionally short.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Models/Company.h"

class CreateCompanyDialog : public juce::Component
{
public:
    CreateCompanyDialog();
    ~CreateCompanyDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Fired once, on the message thread, after the company was created and
        the caller was added as its "company_admin" member. Not fired on
        cancel. */
    std::function<void (const Company&)> onCreated;

    static void launch (juce::Component* parent,
                        std::function<void (const Company&)> onCreated);

private:
    void closeDialog();
    void submit();
    void setStatus (const juce::String& message, bool isError);

    juce::Label      titleLabel_;
    juce::Label      subtitleLabel_;
    juce::TextButton closeButton_ { "X" };

    juce::Label      nameLabel_;
    juce::TextEditor nameEditor_;
    juce::Label      addressLabel_;
    juce::TextEditor addressEditor_;
    juce::Label      cityLabel_;
    juce::TextEditor cityEditor_;
    juce::Label      countryLabel_;
    juce::TextEditor countryEditor_;

    juce::Label      statusLabel_;
    juce::TextButton cancelButton_;
    juce::TextButton createButton_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CreateCompanyDialog)
};
