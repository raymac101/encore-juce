/*
  ==============================================================================

    CompanyAdminPage.h

    Company-admin dashboard page for multi-tenant Encore deployments.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Localization/LocalizationManager.h"
#include "../Models/VenueItem.h"
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
    ~CompanyAdminPage() override;

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

    // Company header -- logo left, name in title font, matching the
    // "act like a real dashboard" look Ray asked for. Distinct from
    // companyNameEditor_ etc. below, which are the *editing* form, now
    // relocated under an "Edit Company" toggle rather than being the first
    // thing shown.
    juce::Image companyHeaderLogo_;
    juce::TextButton editCompanyToggle_ { "Edit Company Info" };
    bool companyEditFormVisible_ = false;

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
    juce::TextButton registerButton_ { "Register Device" };

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

    //==========================================================================
    // Venue cards -- logo, name, address/city, live queue/requested counts,
    // songbook sync status, Edit + Enable/Disable buttons. Clicking the card
    // body (not a button) selects it, revealing that venue's staff section
    // below. Mirrors AdsPage::AdTile's shape (Source/UI/AdsPage.h).
    class VenueCard : public juce::Component
    {
    public:
        VenueCard();

        void setVenue (const VenueItem& venue);
        const juce::String& getVenueId() const noexcept { return venueId_; }
        void setSelected (bool selected);
        void setCounts (const juce::String& text);
        void setSyncStatus (const juce::String& text, bool stale);
        void setEnabledState (bool enabled);
        bool isVenueEnabled() const noexcept { return enabled_; }
        void setInteractionsEnabled (bool canInteract) { editButton_.setEnabled (canInteract); enableToggle_.setEnabled (canInteract); }

        void paint (juce::Graphics& g) override;
        void resized() override;
        void mouseUp (const juce::MouseEvent& e) override;

        std::function<void (const juce::String&)> onSelected;
        std::function<void (const juce::String&)> onEdit;
        std::function<void (const juce::String&)> onToggleEnabled;

    private:
        void refreshLogo();

        juce::String venueId_;
        juce::String logoUrl_;
        bool         selected_ = false;
        bool         enabled_ = true;
        juce::Image  logo_;

        juce::Label      nameLabel_;
        juce::Label      addressLabel_;
        juce::Label      countsLabel_;
        juce::Label      syncStatusLabel_;
        juce::TextButton editButton_ { "Edit" };
        juce::TextButton enableToggle_;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VenueCard)
    };

    void toggleVenueEnabled (const juce::String& venueId);
    void selectVenue (const juce::String& venueId);
    void openEditVenueDialog (const juce::String& venueId);
    std::vector<std::unique_ptr<VenueCard>> venueCards_;
    juce::String selectedVenueId_;
    juce::Label venuesTitle_;
    juce::Label venuesEmptyLabel_;
    void layoutVenueCards (juce::Rectangle<int> area);

    /** "Upload Songs" -- pushes this PC's local songbook.json out to every
        venue in venueCards_ via SongbookStorageService::uploadLocalSongbook(),
        then refreshes each card's sync-status label. */
    void pushSongsToCompanyVenues();

    /** Re-checks one venue's songbook sync status (this PC's local copy vs.
        that venue's Storage copy) and updates its card's label in place. */
    void refreshVenueSyncStatus (const juce::String& venueId);

    //==========================================================================
    // Venue staff -- who is assigned to the SELECTED venue (user-venue-lookup),
    // distinct from the company-wide membersTitle_ section above (companies/
    // {id}/members). Populated by selectVenue().
    struct StaffRow
    {
        juce::String userId;
        juce::Label  nameLabel;
        juce::Label  roleStatusLabel;
        juce::TextButton removeButton { "Remove" };
    };
    std::vector<std::unique_ptr<StaffRow>> staffRows_;
    juce::Label staffTitle_;
    juce::Label staffEmptyLabel_;
    juce::TextEditor staffInviteEmailEditor_;
    juce::ComboBox staffInviteRoleBox_;
    juce::TextButton staffInviteButton_ { "Add Host" };
    void loadVenueStaff (const juce::String& venueId);
    void inviteVenueStaff();
    void removeVenueStaffMember (const juce::String& venueId, const juce::String& userId);
    void layoutStaffRows (juce::Rectangle<int> area);

    //==========================================================================
    // Song distribution -- pick local file(s), push to selected/all company
    // venues via SongDeliveryService.
    juce::Label songSectionTitle_;
    juce::Label songFileLabel_;
    juce::TextButton browseSongButton_ { "Choose Song File..." };
    juce::TextButton targetAllVenuesToggle_;
    juce::TextButton uploadSongButton_ { "Send to Venues" };
    juce::File selectedSongFile_;
    struct VenueTargetToggle
    {
        juce::String venueId;
        juce::ToggleButton toggle;
    };
    std::vector<std::unique_ptr<VenueTargetToggle>> venueTargets_;
    bool targetAllVenues_ = true;
    void chooseSongFile();
    void sendSongToTargetVenues();
    void layoutSongSection (juce::Rectangle<int> area);

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

    // Cached each layoutContent() pass so contentHolder_->onPaint can draw
    // panels/logo behind the right area without re-deriving section heights
    // (several sections now have data-dependent heights).
    juce::Rectangle<int> headerPanelBounds_;
    juce::Rectangle<int> headerLogoBounds_;
    juce::Rectangle<int> editFormPanelBounds_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CompanyAdminPage)
};
