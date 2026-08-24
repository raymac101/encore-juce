/*
  ============================================================================== 

    CompanyAdminPage.h

    Company-admin dashboard page for multi-tenant Encore deployments.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Localization/LocalizationManager.h"
#include <vector>
#include <memory>

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
    void loadCompanyVenues();
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
    juce::Label memberStatusLabel_;
    juce::TextEditor memberUserIdEditor_;
    juce::ComboBox memberRoleBox_;
    juce::ComboBox memberStatusBox_;
    juce::TextButton saveMemberButton_ { "Add / Update Member" };
    juce::TextButton refreshMembersButton_ { "Refresh Members" };
    juce::Label membersListLabel_;

    StatCard venueCard_;
    StatCard hostCard_;
    StatCard deviceCard_;
    StatCard packageCard_;
    StatCard campaignCard_;

    // Read-only per-venue oversight list (name + live queue/requested
    // counts), populated by loadCompanyVenues() via
    // VenueService::getVenuesForCompany() + checkExistingSessionData().
    // No online/offline status: the data model has no real presence signal
    // (VenueItem's timestamps are "last code change" / "last songbook
    // refresh", not a running-app heartbeat) -- showing one anyway would be
    // guesswork dressed up as fact.
    struct VenueRow
    {
        juce::String venueId;
        bool         enabled = true;
        juce::Label  name;
        juce::Label  counts;
        juce::Label  syncStatus;
        juce::TextButton enableToggle;
    };
    void toggleVenueEnabled (const juce::String& venueId);
    std::vector<std::unique_ptr<VenueRow>> venueRows_;
    juce::Label venuesTitle_;
    juce::Label venuesEmptyLabel_;
    void layoutVenueRows (juce::Rectangle<int> area);

    /** "Upload Songs" -- pushes this PC's local songbook.json out to every
        venue in venueRows_ via SongbookStorageService::uploadLocalSongbook(),
        then refreshes each row's sync-status label. */
    void pushSongsToCompanyVenues();

    /** Re-checks one venue's songbook sync status (this PC's local copy vs.
        that venue's Storage copy) and updates its row's label in place. */
    void refreshVenueSyncStatus (const juce::String& venueId);

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

    // Plain Component with a paint callback -- lets the decorative header
    // panels / stat-card glass backgrounds scroll along with the rest of
    // the content (see LibraryPage for the same pattern and rationale).
    class ContentHolder : public juce::Component
    {
    public:
        std::function<void(juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    std::unique_ptr<ContentHolder> contentHolder_;
    juce::Viewport viewport_;
    void layoutContent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanyAdminPage)
};