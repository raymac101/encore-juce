/*
  ============================================================================== 

    CompanyAdminPage.h

    Company-admin dashboard page for multi-tenant Encore deployments.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Localization/LocalizationManager.h"

class CompanyAdminPage : public juce::Component
{
public:
    struct Summary
    {
        int venues = 0;
        int hosts = 0;
        int devices = 0;
        int songPackages = 0;
        int campaigns = 0;
    };

    CompanyAdminPage();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void updateAllText();

    void setCompanyContext (const juce::String& companyId, const juce::String& companyRole);
    void setSummary (const Summary& summary);
    void setStatusMessage (const juce::String& message);

    std::function<void(const juce::String&)> onCompanyIdChanged;

private:
    void loadCompanyInfo();
    void saveCompanyInfo();
    void clearLogo();
    void updateLogoPreviewFromFile (const juce::File& file);
    void applyCompanyIdFromEditor();
    void loadMembers();
    void saveMemberMapping();
    bool uploadLogoToStorage (const juce::String& companyId,
                  const juce::File& logoFile,
                  juce::String& outLogoUrl,
                  juce::String& outStoragePath,
                  juce::String& outError) const;

    struct StatCard
    {
        juce::Label title;
        juce::Label value;
    };

    juce::Label title_;
    juce::Label subtitle_;
    juce::Label status_;
    juce::Label companyInfoTitle_;
    juce::Label companyIdLabel_;
    juce::TextEditor companyIdEditor_;
    juce::TextButton applyCompanyIdButton_ { "Use Company ID" };
    juce::Label companyNameLabel_;
    juce::Label companyStatusLabel_;
    juce::Label logoLabel_;
    juce::Label logoPathLabel_;
    juce::TextEditor companyNameEditor_;
    juce::ComboBox companyStatusBox_;
    juce::TextButton browseLogoButton_ { "Browse Logo" };
    juce::TextButton clearLogoButton_ { "Clear Logo" };
    juce::TextButton saveCompanyButton_ { "Save Company Info" };
    juce::TextButton refreshButton_ { "Refresh" };
    juce::TextButton inviteButton_ { "Invite Host" };
    juce::TextButton registerButton_ { "Register Device" };
    juce::TextButton uploadButton_ { "Upload Songs" };

    juce::Label membersTitle_;
    juce::Label memberUserIdLabel_;
    juce::Label memberRoleLabel_;
    juce::TextEditor memberUserIdEditor_;
    juce::ComboBox memberRoleBox_;
    juce::TextButton saveMemberButton_ { "Add / Update Member" };
    juce::TextButton refreshMembersButton_ { "Refresh Members" };
    juce::Label membersListLabel_;

    StatCard venueCard_;
    StatCard hostCard_;
    StatCard deviceCard_;
    StatCard packageCard_;
    StatCard campaignCard_;

    juce::String companyId_;
    juce::String companyRole_;
    Summary summary_;
    juce::String statusMessage_;
    juce::String companyLogoUrl_;
    juce::String companyLogoStoragePath_;
    juce::String companyLogoFileName_;
    juce::Image logoPreview_;
    bool companyDocExists_ = false;
    std::unique_ptr<juce::FileChooser> fileChooser_;
    juce::File selectedLogoFile_;

    void configureCard (StatCard& card, const juce::String& title, const juce::String& initialValue);
    void layoutCard (StatCard& card, juce::Rectangle<int> area);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanyAdminPage)
};