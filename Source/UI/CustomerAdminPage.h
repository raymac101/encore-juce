/*
  ==============================================================================

    CustomerAdminPage.h

    EnterpriseAdmin-only page (see NavBar::MenuItem::enterpriseAdminOnly and
    Source/Services/CustomerAdminService.h). Two views:

      Unassigned Users — hosts with no active venue association; assign a
      venue + role to bring a legacy account back into normal use.

      Customer Search — find a customer by email/stage name, view a
      combined profile (hosts + read-only legacy `users`/TAGG profile +
      venue associations + Firebase Auth status), and act on the account:
      send a password-reset email OR set a new password directly, and
      deactivate (reversible) OR permanently delete (irreversible, requires
      typing the account's exact email to confirm).

    Distinct from CompanyAdminPage, which is company/subscription-scoped
    and untouched by this feature.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Services/CustomerAdminService.h"

class CustomerAdminPage : public juce::Component
{
public:
    CustomerAdminPage();
    ~CustomerAdminPage() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void updateAllText();

private:
    //==========================================================================
    // Shared results list (used by both tabs)
    class ResultsListModel : public juce::ListBoxModel
    {
    public:
        std::vector<CustomerAdminService::HostSummary> rows;
        std::function<void (int)> onRowClicked;

        int getNumRows() override { return (int) rows.size(); }
        void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (onRowClicked) onRowClicked (row); }
    };

    // Venue search/pick list for the "assign venue + role" flow.
    class VenueListModel : public juce::ListBoxModel
    {
    public:
        std::vector<CustomerAdminService::VenueSummary> rows;   // filtered, displayed
        std::function<void (int)> onRowClicked;

        int getNumRows() override { return (int) rows.size(); }
        void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override { if (onRowClicked) onRowClicked (row); }
    };

    //==========================================================================
    void showUnassignedTab();
    void showSearchTab();

    void refreshUnassigned (bool resetCursor);
    void submitVenueAssignment();
    void loadAllVenuesIfNeeded();
    void filterVenueList();
    void selectVenue (int filteredRow);

    void performSearch (bool resetCursor);
    void loadProfile (const juce::String& uid);
    void refreshProfileView();
    void clearProfileView();

    void onSendResetEmailClicked();
    void onSetPasswordClicked();
    void onDeactivateClicked();
    void onReactivateClicked();
    void updateHardDeleteButtonState();
    void onHardDeleteClicked();

    // The right-side panel shows EITHER the selected host's profile OR the
    // selected venue's details -- whichever was clicked most recently.
    void showHostProfilePanel();
    void showVenueDetailsPanel();
    void refreshVenueDetailsView();
    void updateDeleteVenueButtonState();
    void onDeleteVenueClicked();

    void setStatus (const juce::String& message, bool isError = false);

    // Plain Component with a paint callback -- lets the decorative header
    // panels scroll along with the rest of the content (see LibraryPage for
    // the same pattern and rationale).
    class ContentHolder : public juce::Component
    {
    public:
        std::function<void(juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    };

    std::unique_ptr<ContentHolder> contentHolder_;
    juce::Viewport viewport_;
    void layoutContent();

    //==========================================================================
    juce::Label title_, subtitle_, statusLabel_;
    juce::TextButton tabUnassignedButton_ { "Unassigned Users" };
    juce::TextButton tabSearchButton_ { "Customer Search" };
    juce::TextButton bulkMetadataButton_ { "Bulk Metadata" };
    bool showingSearchTab_ = false;

    //--- Unassigned Users tab -------------------------------------------------
    juce::Label unassignedTitle_;
    juce::TextButton refreshUnassignedButton_ { "Refresh" };
    juce::TextButton unassignedMoreButton_ { "Load More" };
    ResultsListModel unassignedModel_;
    juce::ListBox unassignedList_ { "unassignedList", &unassignedModel_ };
    juce::String unassignedCursor_;
    juce::String selectedUnassignedUid_;
    juce::String selectedUnassignedEmail_;

    juce::Label assignHeaderLabel_;
    juce::TextEditor venueSearchEditor_;
    VenueListModel venueListModel_;
    juce::ListBox venueListBox_ { "venueList", &venueListModel_ };
    juce::Label selectedVenueLabel_;
    juce::Label assignRoleLabel_;
    juce::ComboBox assignRoleBox_;
    juce::TextButton assignSubmitButton_ { "Assign Venue + Role" };

    std::vector<CustomerAdminService::VenueSummary> allVenues_;   // full cached list, loaded once
    bool venuesLoaded_ = false;
    bool venuesLoading_ = false;
    juce::String selectedVenueId_, selectedVenueName_, selectedVenueCity_;

    //--- Customer Search tab --------------------------------------------------
    juce::Label searchLabel_;
    juce::TextEditor searchQueryEditor_;
    juce::TextButton searchButton_ { "Search" };
    juce::TextButton searchMoreButton_ { "Load More" };
    ResultsListModel searchModel_;
    juce::ListBox searchList_ { "searchList", &searchModel_ };
    juce::String searchCursor_;

    //--- Profile detail panel (shown for either tab once a row is picked) ----
    juce::Label profileHeaderLabel_;
    juce::Label profileDetailsLabel_;
    juce::Label legacyProfileLabel_;
    juce::Label venuesLabel_;

    juce::Label resetSectionLabel_;
    juce::TextButton sendResetEmailButton_ { "Send Reset Email" };
    juce::TextEditor newPasswordEditor_;
    juce::TextButton setPasswordButton_ { "Set New Password Directly" };

    juce::Label removalSectionLabel_;
    juce::TextButton deactivateButton_ { "Deactivate (reversible)" };
    juce::TextButton reactivateButton_ { "Reactivate" };
    juce::Label confirmEmailLabel_;
    juce::TextEditor confirmEmailEditor_;
    juce::TextButton hardDeleteButton_ { "Permanently Delete" };

    CustomerAdminService::UserProfile currentProfile_;
    bool profileLoaded_ = false;

    //--- Venue details panel (shares the right column with the profile panel,
    //    shown instead of it when a venue was the most recently clicked row) --
    bool showingVenueInRightPanel_ = false;
    juce::Label venueDetailsHeaderLabel_;
    juce::Label venueDetailsBodyLabel_;
    juce::Label deleteVenueSectionLabel_;
    juce::Label confirmVenueNameLabel_;
    juce::TextEditor confirmVenueNameEditor_;
    juce::TextButton deleteVenueButton_ { "Delete Venue" };
    CustomerAdminService::VenueSummary currentVenueDetails_;
    bool venueDetailsLoaded_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CustomerAdminPage)
};
