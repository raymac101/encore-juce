/*
  ==============================================================================

    CustomerAdminPage.cpp

  ==============================================================================
*/

#include "CustomerAdminPage.h"
#include "MenuTheme.h"
#include "BulkMetadataTool.h"
#include "../Localization/LocalizationManager.h"
#include "../Models/AccessRights.h"
#include <algorithm>

namespace
{
    const juce::Colour kBg     { 0xff16213e };
    const juce::Colour kPanel  { 0x99182a52 };
    const juce::Colour kBorder { 0x664f78c4 };
    const juce::Colour kAccent { 0xff5a8fd8 };
    const juce::Colour kText   { 0xffffffff };
    const juce::Colour kMuted  { 0xffc7d2e0 };
    const juce::Colour kDanger { 0xffd9534f };
    const juce::Colour kSafe   { 0xff4caf7d };

    void styleLabel (juce::Label& l, float height, bool bold, juce::Colour colour)
    {
        auto font = juce::Font (juce::FontOptions().withHeight (height));
        if (bold) font = font.boldened();
        l.setFont (font);
        l.setColour (juce::Label::textColourId, colour);
    }

    void styleEditor (juce::TextEditor& e, const juce::String& hint)
    {
        e.setTextToShowWhenEmpty (hint, kMuted);
        e.setColour (juce::TextEditor::backgroundColourId, kPanel);
        e.setColour (juce::TextEditor::textColourId, kText);
        e.setColour (juce::TextEditor::outlineColourId, kBorder);
        e.setColour (juce::TextEditor::focusedOutlineColourId, kAccent);
    }

    void styleButton (juce::TextButton& b, juce::Colour bg)
    {
        b.setColour (juce::TextButton::buttonColourId, bg);
        b.setColour (juce::TextButton::textColourOffId, kText);
    }

    juce::String formatHostLine (const CustomerAdminService::HostSummary& h)
    {
        juce::String line = h.email;
        if (h.stageName.isNotEmpty()) line += juce::String(juce::CharPointer_UTF8("  \xe2\x80\x94  ")) + h.stageName;
        if (h.authOnly) line += "  [Auth only, no hosts doc]";
        return line;
    }
}

//==============================================================================
void CustomerAdminPage::ResultsListModel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                             int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) rows.size())
        return;

    if (rowIsSelected)
    {
        g.setColour (kAccent.withAlpha (0.35f));
        g.fillRect (0, 0, width, height);
    }

    const auto& h = rows[(size_t) rowNumber];
    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    g.drawFittedText (formatHostLine (h), juce::Rectangle<int> (10, 0, width - 20, height),
                      juce::Justification::centredLeft, 1);
}

void CustomerAdminPage::VenueListModel::paintListBoxItem (int rowNumber, juce::Graphics& g,
                                                           int width, int height, bool rowIsSelected)
{
    if (rowNumber < 0 || rowNumber >= (int) rows.size())
        return;

    if (rowIsSelected)
    {
        g.setColour (kAccent.withAlpha (0.35f));
        g.fillRect (0, 0, width, height);
    }

    const auto& v = rows[(size_t) rowNumber];
    juce::String line = v.name.isNotEmpty() ? v.name : v.id;
    if (v.city.isNotEmpty()) line += juce::String(juce::CharPointer_UTF8("  \xe2\x80\x94  ")) + v.city;

    g.setColour (kText);
    g.setFont (juce::Font (juce::FontOptions().withHeight (14.0f)));
    g.drawFittedText (line, juce::Rectangle<int> (10, 0, width - 20, height),
                      juce::Justification::centredLeft, 1);
}

//==============================================================================
CustomerAdminPage::CustomerAdminPage()
    : contentHolder_ (std::make_unique<ContentHolder>())
{
    setOpaque (true);
    addAndMakeVisible (viewport_);
    viewport_.setViewedComponent (contentHolder_.get(), false);
    viewport_.setScrollBarsShown (true, false);

    auto& lm = LocalizationManager::getInstance();

    styleLabel (title_, 28.0f, true, kText);
    title_.setText (lm.getText ("page.customer_admin.title"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (title_);

    styleLabel (subtitle_, 13.0f, false, kMuted);
    subtitle_.setText (lm.getText ("page.customer_admin.subtitle"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (subtitle_);

    styleLabel (statusLabel_, 12.0f, false, kMuted);
    contentHolder_->addAndMakeVisible (statusLabel_);

    styleButton (tabUnassignedButton_, kAccent);
    tabUnassignedButton_.onClick = [this] { showUnassignedTab(); };
    contentHolder_->addAndMakeVisible (tabUnassignedButton_);

    styleButton (tabSearchButton_, kPanel);
    tabSearchButton_.onClick = [this] { showSearchTab(); };
    contentHolder_->addAndMakeVisible (tabSearchButton_);

    styleButton (bulkMetadataButton_, kPanel);
    bulkMetadataButton_.onClick = [this] { BulkMetadataTool::launch (this); };
    contentHolder_->addAndMakeVisible (bulkMetadataButton_);

    //--- Unassigned Users tab ------------------------------------------------
    styleLabel (unassignedTitle_, 15.0f, true, kText);
    unassignedTitle_.setText (lm.getText ("page.customer_admin.unassigned_title"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (unassignedTitle_);

    styleButton (refreshUnassignedButton_, kPanel);
    refreshUnassignedButton_.onClick = [this] { refreshUnassigned (true); };
    contentHolder_->addAndMakeVisible (refreshUnassignedButton_);

    styleButton (unassignedMoreButton_, kPanel);
    unassignedMoreButton_.onClick = [this] { refreshUnassigned (false); };
    contentHolder_->addAndMakeVisible (unassignedMoreButton_);

    unassignedList_.setColour (juce::ListBox::backgroundColourId, kPanel);
    unassignedList_.setColour (juce::ListBox::outlineColourId, kBorder);
    contentHolder_->addAndMakeVisible (unassignedList_);

    unassignedModel_.onRowClicked = [this] (int row)
    {
        if (row < 0 || row >= (int) unassignedModel_.rows.size()) return;
        const auto& h = unassignedModel_.rows[(size_t) row];
        selectedUnassignedUid_   = h.userId;
        selectedUnassignedEmail_ = h.email;
        selectedVenueId_.clear();
        selectedVenueName_.clear();
        selectedVenueCity_.clear();
        selectedVenueLabel_.setText (LocalizationManager::getInstance().getText ("page.customer_admin.no_venue_selected"),
                                     juce::dontSendNotification);
        showHostProfilePanel();
        loadProfile (h.userId);
    };

    styleLabel (assignHeaderLabel_, 14.0f, true, kText);
    assignHeaderLabel_.setText (lm.getText ("page.customer_admin.assign_header"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (assignHeaderLabel_);

    styleEditor (venueSearchEditor_, lm.getText ("page.customer_admin.venue_search_hint"));
    venueSearchEditor_.onTextChange = [this] { filterVenueList(); };
    contentHolder_->addAndMakeVisible (venueSearchEditor_);

    venueListBox_.setColour (juce::ListBox::backgroundColourId, kPanel);
    venueListBox_.setColour (juce::ListBox::outlineColourId, kBorder);
    contentHolder_->addAndMakeVisible (venueListBox_);
    venueListModel_.onRowClicked = [this] (int row) { selectVenue (row); };

    styleLabel (selectedVenueLabel_, 12.0f, true, kMuted);
    selectedVenueLabel_.setText (lm.getText ("page.customer_admin.no_venue_selected"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (selectedVenueLabel_);

    styleLabel (assignRoleLabel_, 12.0f, false, kMuted);
    assignRoleLabel_.setText (lm.getText ("page.customer_admin.venue_role"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (assignRoleLabel_);

    assignRoleBox_.addItem ("Host", 1);
    assignRoleBox_.addItem ("Admin", 2);
    assignRoleBox_.addItem ("Tester", 3);
    assignRoleBox_.addItem ("EnterpriseAdmin", 4);
    assignRoleBox_.setSelectedId (1, juce::dontSendNotification);
    assignRoleBox_.setColour (juce::ComboBox::backgroundColourId, kPanel);
    assignRoleBox_.setColour (juce::ComboBox::textColourId, kText);
    assignRoleBox_.setColour (juce::ComboBox::outlineColourId, kBorder);
    contentHolder_->addAndMakeVisible (assignRoleBox_);

    styleButton (assignSubmitButton_, kSafe);
    assignSubmitButton_.onClick = [this] { submitVenueAssignment(); };
    contentHolder_->addAndMakeVisible (assignSubmitButton_);

    //--- Customer Search tab --------------------------------------------------
    styleLabel (searchLabel_, 12.0f, false, kMuted);
    searchLabel_.setText (lm.getText ("page.customer_admin.search_hint"), juce::dontSendNotification);
    contentHolder_->addChildComponent (searchLabel_);

    styleEditor (searchQueryEditor_, lm.getText ("page.customer_admin.search_placeholder"));
    searchQueryEditor_.onReturnKey = [this] { performSearch (true); };
    contentHolder_->addChildComponent (searchQueryEditor_);

    styleButton (searchButton_, kAccent);
    searchButton_.onClick = [this] { performSearch (true); };
    contentHolder_->addChildComponent (searchButton_);

    styleButton (searchMoreButton_, kPanel);
    searchMoreButton_.onClick = [this] { performSearch (false); };
    contentHolder_->addChildComponent (searchMoreButton_);

    searchList_.setColour (juce::ListBox::backgroundColourId, kPanel);
    searchList_.setColour (juce::ListBox::outlineColourId, kBorder);
    contentHolder_->addChildComponent (searchList_);

    searchModel_.onRowClicked = [this] (int row)
    {
        if (row < 0 || row >= (int) searchModel_.rows.size()) return;
        showHostProfilePanel();
        loadProfile (searchModel_.rows[(size_t) row].userId);
    };

    //--- Profile detail panel (shared) ---------------------------------------
    styleLabel (profileHeaderLabel_, 15.0f, true, kText);
    contentHolder_->addAndMakeVisible (profileHeaderLabel_);

    styleLabel (profileDetailsLabel_, 13.0f, false, kMuted);
    profileDetailsLabel_.setJustificationType (juce::Justification::topLeft);
    contentHolder_->addAndMakeVisible (profileDetailsLabel_);

    styleLabel (venuesLabel_, 12.0f, false, kMuted);
    venuesLabel_.setJustificationType (juce::Justification::topLeft);
    contentHolder_->addAndMakeVisible (venuesLabel_);

    styleLabel (legacyProfileLabel_, 12.0f, false, kMuted);
    legacyProfileLabel_.setJustificationType (juce::Justification::topLeft);
    contentHolder_->addAndMakeVisible (legacyProfileLabel_);

    styleLabel (resetSectionLabel_, 13.0f, true, kText);
    resetSectionLabel_.setText (lm.getText ("page.customer_admin.reset_section"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (resetSectionLabel_);

    styleButton (sendResetEmailButton_, kAccent);
    sendResetEmailButton_.onClick = [this] { onSendResetEmailClicked(); };
    contentHolder_->addAndMakeVisible (sendResetEmailButton_);

    styleEditor (newPasswordEditor_, lm.getText ("page.customer_admin.new_password_hint"));
    newPasswordEditor_.setPasswordCharacter (juce::juce_wchar ('*'));
    contentHolder_->addAndMakeVisible (newPasswordEditor_);

    styleButton (setPasswordButton_, kDanger.withMultipliedSaturation (0.7f));
    setPasswordButton_.onClick = [this] { onSetPasswordClicked(); };
    contentHolder_->addAndMakeVisible (setPasswordButton_);

    styleLabel (removalSectionLabel_, 13.0f, true, kText);
    removalSectionLabel_.setText (lm.getText ("page.customer_admin.removal_section"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (removalSectionLabel_);

    styleButton (deactivateButton_, kSafe);
    deactivateButton_.onClick = [this] { onDeactivateClicked(); };
    contentHolder_->addAndMakeVisible (deactivateButton_);

    styleButton (reactivateButton_, kPanel);
    reactivateButton_.onClick = [this] { onReactivateClicked(); };
    contentHolder_->addAndMakeVisible (reactivateButton_);

    styleLabel (confirmEmailLabel_, 12.0f, false, kMuted);
    confirmEmailLabel_.setText (lm.getText ("page.customer_admin.confirm_email_hint"), juce::dontSendNotification);
    contentHolder_->addAndMakeVisible (confirmEmailLabel_);

    styleEditor (confirmEmailEditor_, lm.getText ("page.customer_admin.confirm_email_placeholder"));
    confirmEmailEditor_.onTextChange = [this] { updateHardDeleteButtonState(); };
    contentHolder_->addAndMakeVisible (confirmEmailEditor_);

    styleButton (hardDeleteButton_, kDanger);
    hardDeleteButton_.onClick = [this] { onHardDeleteClicked(); };
    hardDeleteButton_.setEnabled (false);
    contentHolder_->addAndMakeVisible (hardDeleteButton_);

    //--- Venue details panel (shares the right column; starts hidden -- see
    //    showHostProfilePanel()/showVenueDetailsPanel()) -----------------------
    styleLabel (venueDetailsHeaderLabel_, 15.0f, true, kText);
    contentHolder_->addChildComponent (venueDetailsHeaderLabel_);

    styleLabel (venueDetailsBodyLabel_, 13.0f, false, kMuted);
    venueDetailsBodyLabel_.setJustificationType (juce::Justification::topLeft);
    contentHolder_->addChildComponent (venueDetailsBodyLabel_);

    styleLabel (deleteVenueSectionLabel_, 13.0f, true, kText);
    deleteVenueSectionLabel_.setText (lm.getText ("page.customer_admin.delete_venue_section"), juce::dontSendNotification);
    contentHolder_->addChildComponent (deleteVenueSectionLabel_);

    styleLabel (confirmVenueNameLabel_, 12.0f, false, kMuted);
    confirmVenueNameLabel_.setText (lm.getText ("page.customer_admin.confirm_venue_name_hint"), juce::dontSendNotification);
    contentHolder_->addChildComponent (confirmVenueNameLabel_);

    styleEditor (confirmVenueNameEditor_, lm.getText ("page.customer_admin.confirm_venue_name_placeholder"));
    confirmVenueNameEditor_.onTextChange = [this] { updateDeleteVenueButtonState(); };
    contentHolder_->addChildComponent (confirmVenueNameEditor_);

    styleButton (deleteVenueButton_, kDanger);
    deleteVenueButton_.onClick = [this] { onDeleteVenueClicked(); };
    deleteVenueButton_.setEnabled (false);
    contentHolder_->addChildComponent (deleteVenueButton_);

    // Decorative header/column panels, drawn against contentHolder_'s own
    // bounds (not CustomerAdminPage's) so they scroll along with the rest
    // of the content.
    contentHolder_->onPaint = [this] (juce::Graphics& g)
    {
        auto bounds = contentHolder_->getLocalBounds().reduced (22);
        auto header = bounds.removeFromTop (96);
        MenuTheme::drawHeaderPanel (g, header);

        bounds.removeFromTop (14);
        auto leftColumn = bounds.removeFromLeft (bounds.getWidth() * 3 / 5);
        MenuTheme::drawHeaderPanel (g, leftColumn);

        bounds.removeFromLeft (14);
        MenuTheme::drawHeaderPanel (g, bounds);
    };

    clearProfileView();
    showHostProfilePanel();
    showUnassignedTab();
}

CustomerAdminPage::~CustomerAdminPage() = default;

//==============================================================================
void CustomerAdminPage::updateAllText()
{
    auto& lm = LocalizationManager::getInstance();
    title_.setText (lm.getText ("page.customer_admin.title"), juce::dontSendNotification);
    subtitle_.setText (lm.getText ("page.customer_admin.subtitle"), juce::dontSendNotification);
    unassignedTitle_.setText (lm.getText ("page.customer_admin.unassigned_title"), juce::dontSendNotification);
    assignHeaderLabel_.setText (lm.getText ("page.customer_admin.assign_header"), juce::dontSendNotification);
    resetSectionLabel_.setText (lm.getText ("page.customer_admin.reset_section"), juce::dontSendNotification);
    removalSectionLabel_.setText (lm.getText ("page.customer_admin.removal_section"), juce::dontSendNotification);
    confirmEmailLabel_.setText (lm.getText ("page.customer_admin.confirm_email_hint"), juce::dontSendNotification);
    deleteVenueSectionLabel_.setText (lm.getText ("page.customer_admin.delete_venue_section"), juce::dontSendNotification);
    confirmVenueNameLabel_.setText (lm.getText ("page.customer_admin.confirm_venue_name_hint"), juce::dontSendNotification);
}

//==============================================================================
void CustomerAdminPage::showUnassignedTab()
{
    showingSearchTab_ = false;
    styleButton (tabUnassignedButton_, kAccent);
    styleButton (tabSearchButton_, kPanel);

    unassignedTitle_.setVisible (true);
    refreshUnassignedButton_.setVisible (true);
    unassignedMoreButton_.setVisible (true);
    unassignedList_.setVisible (true);
    assignHeaderLabel_.setVisible (true);
    venueSearchEditor_.setVisible (true);
    venueListBox_.setVisible (true);
    selectedVenueLabel_.setVisible (true);
    assignRoleLabel_.setVisible (true);
    assignRoleBox_.setVisible (true);
    assignSubmitButton_.setVisible (true);

    searchLabel_.setVisible (false);
    searchQueryEditor_.setVisible (false);
    searchButton_.setVisible (false);
    searchMoreButton_.setVisible (false);
    searchList_.setVisible (false);

    if (unassignedModel_.rows.empty())
        refreshUnassigned (true);

    loadAllVenuesIfNeeded();

    resized();
}

void CustomerAdminPage::showSearchTab()
{
    showingSearchTab_ = true;
    styleButton (tabUnassignedButton_, kPanel);
    styleButton (tabSearchButton_, kAccent);

    unassignedTitle_.setVisible (false);
    refreshUnassignedButton_.setVisible (false);
    unassignedMoreButton_.setVisible (false);
    unassignedList_.setVisible (false);
    assignHeaderLabel_.setVisible (false);
    venueSearchEditor_.setVisible (false);
    venueListBox_.setVisible (false);
    selectedVenueLabel_.setVisible (false);
    assignRoleLabel_.setVisible (false);
    assignRoleBox_.setVisible (false);
    assignSubmitButton_.setVisible (false);

    searchLabel_.setVisible (true);
    searchQueryEditor_.setVisible (true);
    searchButton_.setVisible (true);
    searchMoreButton_.setVisible (true);
    searchList_.setVisible (true);

    resized();
}

//==============================================================================
void CustomerAdminPage::refreshUnassigned (bool resetCursor)
{
    if (resetCursor)
    {
        unassignedCursor_.clear();
        unassignedModel_.rows.clear();
        unassignedList_.updateContent();
    }

    setStatus ("Loading unassigned hosts...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().listUnassignedHosts (unassignedCursor_,
        [safe, resetCursor] (bool ok, std::vector<CustomerAdminService::HostSummary> results,
                             juce::String nextCursor, juce::String error)
        {
            if (safe == nullptr) return;
            if (! ok) { safe->setStatus ("Failed to load: " + error, true); return; }

            if (resetCursor)
                safe->unassignedModel_.rows = std::move (results);
            else
                for (auto& r : results) safe->unassignedModel_.rows.push_back (r);

            safe->unassignedCursor_ = nextCursor;
            safe->unassignedList_.updateContent();
            safe->setStatus (juce::String (safe->unassignedModel_.rows.size()) + " unassigned host(s) loaded.");
        });
}

void CustomerAdminPage::submitVenueAssignment()
{
    if (selectedUnassignedUid_.isEmpty())
    {
        setStatus ("Select a host from the list first.", true);
        return;
    }

    if (selectedVenueId_.isEmpty())
    {
        setStatus ("Search and select a venue first.", true);
        return;
    }

    const auto role = assignRoleBox_.getText();

    setStatus ("Assigning venue...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    const auto uid = selectedUnassignedUid_;
    const auto email = selectedUnassignedEmail_;
    const auto venueId = selectedVenueId_;
    const auto venueName = selectedVenueName_;
    const auto venueCity = selectedVenueCity_;

    CustomerAdminService::getInstance().assignVenueRole (uid, venueId, role, venueName, venueCity, email,
        [safe, uid] (bool ok, juce::String error)
        {
            if (safe == nullptr) return;
            if (! ok) { safe->setStatus ("Assignment failed: " + error, true); return; }

            safe->setStatus ("Venue assigned.");
            safe->selectedUnassignedUid_.clear();
            safe->selectedVenueId_.clear();
            safe->selectedVenueName_.clear();
            safe->selectedVenueCity_.clear();
            safe->selectedVenueLabel_.setText (LocalizationManager::getInstance()
                                                    .getText ("page.customer_admin.no_venue_selected"),
                                               juce::dontSendNotification);
            safe->refreshUnassigned (true);
            safe->loadProfile (uid);
        });
}

//==============================================================================
void CustomerAdminPage::loadAllVenuesIfNeeded()
{
    if (venuesLoaded_ || venuesLoading_)
        return;

    venuesLoading_ = true;
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().listVenues (
        [safe] (bool ok, std::vector<CustomerAdminService::VenueSummary> venues, juce::String error)
        {
            if (safe == nullptr) return;
            safe->venuesLoading_ = false;
            if (! ok)
            {
                safe->setStatus ("Failed to load venues: " + error, true);
                return;
            }
            safe->allVenues_ = std::move (venues);
            safe->venuesLoaded_ = true;
            safe->filterVenueList();
        });
}

void CustomerAdminPage::filterVenueList()
{
    const auto query = venueSearchEditor_.getText().trim().toLowerCase();

    venueListModel_.rows.clear();
    for (auto& v : allVenues_)
    {
        if (query.isEmpty()
            || v.name.toLowerCase().contains (query)
            || v.city.toLowerCase().contains (query)
            || v.id.toLowerCase().contains (query))
        {
            venueListModel_.rows.push_back (v);
        }
    }
    venueListBox_.updateContent();
}

void CustomerAdminPage::selectVenue (int filteredRow)
{
    if (filteredRow < 0 || filteredRow >= (int) venueListModel_.rows.size())
        return;

    const auto& v = venueListModel_.rows[(size_t) filteredRow];
    selectedVenueId_   = v.id;
    selectedVenueName_ = v.name;
    selectedVenueCity_ = v.city;

    juce::String label = v.name.isNotEmpty() ? v.name : v.id;
    if (v.city.isNotEmpty())
        label << "  (" << v.city << ")";
    selectedVenueLabel_.setText (label, juce::dontSendNotification);

    currentVenueDetails_ = v;
    venueDetailsLoaded_ = true;
    confirmVenueNameEditor_.setText ({}, false);
    showVenueDetailsPanel();
    refreshVenueDetailsView();
}

//==============================================================================
void CustomerAdminPage::performSearch (bool resetCursor)
{
    const auto query = searchQueryEditor_.getText().trim();
    if (query.isEmpty())
    {
        setStatus ("Enter an email or stage name to search.", true);
        return;
    }

    if (resetCursor)
    {
        searchCursor_.clear();
        searchModel_.rows.clear();
        searchList_.updateContent();
    }

    setStatus ("Searching...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().searchUsers (query, searchCursor_,
        [safe, resetCursor] (bool ok, std::vector<CustomerAdminService::HostSummary> results,
                            juce::String nextCursor, juce::String error)
        {
            if (safe == nullptr) return;
            if (! ok) { safe->setStatus ("Search failed: " + error, true); return; }

            if (resetCursor)
                safe->searchModel_.rows = std::move (results);
            else
                for (auto& r : results) safe->searchModel_.rows.push_back (r);

            safe->searchCursor_ = nextCursor;
            safe->searchList_.updateContent();
            safe->setStatus (juce::String (safe->searchModel_.rows.size()) + " result(s).");
        });
}

//==============================================================================
void CustomerAdminPage::loadProfile (const juce::String& uid)
{
    setStatus ("Loading profile...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().getUserProfile (uid, [safe, uid] (CustomerAdminService::UserProfile profile)
    {
        if (safe == nullptr) return;
        if (! profile.ok)
        {
            safe->setStatus ("Failed to load profile: " + profile.error, true);
            return;
        }
        safe->currentProfile_ = profile;

        // Auth-only accounts (no `hosts` doc) come back from the server
        // with host.userId left empty -- there's no hosts doc to have
        // populated it from -- which then made every write action below
        // (setUserPassword/deactivate/reactivate/hardDelete, all keyed off
        // currentProfile_.host.userId) silently send an empty uid and fail
        // server-side validation. `uid` here is the real Firebase Auth uid
        // this profile was fetched for either way, so it's always the
        // right fallback.
        if (safe->currentProfile_.host.userId.isEmpty())
            safe->currentProfile_.host.userId = uid;

        safe->profileLoaded_ = true;
        safe->confirmEmailEditor_.setText ({}, false);
        safe->newPasswordEditor_.setText ({}, false);
        safe->refreshProfileView();
        safe->setStatus ("Profile loaded.");
    });
}

void CustomerAdminPage::clearProfileView()
{
    profileLoaded_ = false;
    profileHeaderLabel_.setText ({}, juce::dontSendNotification);
    profileDetailsLabel_.setText ({}, juce::dontSendNotification);
    venuesLabel_.setText ({}, juce::dontSendNotification);
    legacyProfileLabel_.setText ({}, juce::dontSendNotification);
    hardDeleteButton_.setEnabled (false);
}

void CustomerAdminPage::refreshProfileView()
{
    if (! profileLoaded_)
    {
        clearProfileView();
        return;
    }

    const auto& h = currentProfile_.host;
    profileHeaderLabel_.setText (h.email.isNotEmpty() ? h.email : h.userId, juce::dontSendNotification);

    juce::String details;
    details << "Stage name: " << (h.stageName.isNotEmpty() ? h.stageName : "(none)") << "\n";
    details << "Full name: "  << (h.fullName.isNotEmpty()  ? h.fullName  : "(none)") << "\n";
    details << "Role: "       << (h.role.isNotEmpty() ? h.role : "(none)") << "\n";
    details << "Location: "   << (h.city.isNotEmpty() || h.country.isNotEmpty()
                                    ? (h.city + (h.city.isNotEmpty() && h.country.isNotEmpty() ? ", " : "") + h.country)
                                    : "(unknown)") << "\n";
    details << "Signed up: "  << (h.signUpDate.isNotEmpty() ? h.signUpDate : "(unknown)") << "\n";
    details << "Last login: " << (h.lastLogin.isNotEmpty() ? h.lastLogin : "(never recorded)")
            << "   (" << h.loginCount << " login" << (h.loginCount == 1 ? "" : "s") << " recorded)\n";
    details << "Account status: " << h.accountStatus;

    if (currentProfile_.hasAuthRecord)
    {
        details << "\nAuth: " << (currentProfile_.authDisabled ? "DISABLED" : "enabled");
        if (currentProfile_.authProviders.size() > 0)
            details << "  [" << currentProfile_.authProviders.joinIntoString (", ") << "]";
    }
    profileDetailsLabel_.setText (details, juce::dontSendNotification);

    juce::String venuesText = "Venue associations:\n";
    if (currentProfile_.venues.empty())
    {
        venuesText << "(none)";
    }
    else
    {
        for (auto& v : currentProfile_.venues)
            venuesText << juce::String(juce::CharPointer_UTF8("  \xe2\x80\xa2 ")) << (v.venueName.isNotEmpty() ? v.venueName : v.venueId)
                       << "  (" << v.role << ")\n";
    }
    venuesLabel_.setText (venuesText, juce::dontSendNotification);

    if (currentProfile_.hasLegacyProfile)
        legacyProfileLabel_.setText ("Legacy TAGG profile on file (read-only reference).",
                                     juce::dontSendNotification);
    else
        legacyProfileLabel_.setText ("No legacy TAGG profile found.", juce::dontSendNotification);

    deactivateButton_.setEnabled (currentProfile_.hasAuthRecord && ! currentProfile_.authDisabled);
    reactivateButton_.setEnabled (currentProfile_.hasAuthRecord && currentProfile_.authDisabled);

    updateHardDeleteButtonState();
}

//==============================================================================
void CustomerAdminPage::onSendResetEmailClicked()
{
    if (! profileLoaded_ || currentProfile_.host.email.isEmpty())
    {
        setStatus ("No profile loaded.", true);
        return;
    }

    setStatus ("Sending reset email...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().sendPasswordResetEmail (currentProfile_.host.email,
        [safe] (bool ok, juce::String error)
        {
            if (safe == nullptr) return;
            safe->setStatus (ok ? "Reset email sent." : ("Failed to send reset email: " + error), ! ok);
        });
}

void CustomerAdminPage::onSetPasswordClicked()
{
    if (! profileLoaded_)
    {
        setStatus ("No profile loaded.", true);
        return;
    }

    const auto newPassword = newPasswordEditor_.getText();
    if (newPassword.length() < 6)
    {
        setStatus ("New password must be at least 6 characters.", true);
        return;
    }

    setStatus ("Setting new password...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().setUserPassword (currentProfile_.host.userId, newPassword,
        [safe] (bool ok, juce::String error)
        {
            if (safe == nullptr) return;
            if (ok) safe->newPasswordEditor_.setText ({}, false);
            safe->setStatus (ok ? "Password updated." : ("Failed to set password: " + error), ! ok);
        });
}

void CustomerAdminPage::onDeactivateClicked()
{
    if (! profileLoaded_) return;

    setStatus ("Deactivating account...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().deactivateUser (currentProfile_.host.userId,
        [safe] (bool ok, juce::String error)
        {
            if (safe == nullptr) return;
            if (! ok) { safe->setStatus ("Failed to deactivate: " + error, true); return; }
            safe->setStatus ("Account deactivated.");
            safe->loadProfile (safe->currentProfile_.host.userId);
        });
}

void CustomerAdminPage::onReactivateClicked()
{
    if (! profileLoaded_) return;

    setStatus ("Reactivating account...");
    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().reactivateUser (currentProfile_.host.userId,
        [safe] (bool ok, juce::String error)
        {
            if (safe == nullptr) return;
            if (! ok) { safe->setStatus ("Failed to reactivate: " + error, true); return; }
            safe->setStatus ("Account reactivated.");
            safe->loadProfile (safe->currentProfile_.host.userId);
        });
}

void CustomerAdminPage::updateHardDeleteButtonState()
{
    if (! profileLoaded_ || currentProfile_.host.email.isEmpty())
    {
        hardDeleteButton_.setEnabled (false);
        return;
    }

    const auto typed = confirmEmailEditor_.getText().trim();
    const auto matches = typed.isNotEmpty()
                       && typed.equalsIgnoreCase (currentProfile_.host.email);
    hardDeleteButton_.setEnabled (matches);
}

void CustomerAdminPage::onHardDeleteClicked()
{
    if (! profileLoaded_) return;

    const auto uid = currentProfile_.host.userId;
    const auto confirmEmail = confirmEmailEditor_.getText().trim();

    setStatus ("Deleting account...");
    hardDeleteButton_.setEnabled (false);

    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().hardDeleteUser (uid, confirmEmail,
        [safe] (bool ok, juce::String error)
        {
            if (safe == nullptr) return;
            if (! ok)
            {
                safe->setStatus ("Delete failed: " + error, true);
                safe->updateHardDeleteButtonState();
                return;
            }
            safe->setStatus ("Account permanently deleted.");
            safe->clearProfileView();
            if (safe->showingSearchTab_)
                safe->performSearch (true);
            else
                safe->refreshUnassigned (true);
        });
}

//==============================================================================
void CustomerAdminPage::showHostProfilePanel()
{
    showingVenueInRightPanel_ = false;

    profileHeaderLabel_.setVisible (true);
    profileDetailsLabel_.setVisible (true);
    venuesLabel_.setVisible (true);
    legacyProfileLabel_.setVisible (true);
    resetSectionLabel_.setVisible (true);
    sendResetEmailButton_.setVisible (true);
    newPasswordEditor_.setVisible (true);
    setPasswordButton_.setVisible (true);
    removalSectionLabel_.setVisible (true);
    deactivateButton_.setVisible (true);
    reactivateButton_.setVisible (true);
    confirmEmailLabel_.setVisible (true);
    confirmEmailEditor_.setVisible (true);
    hardDeleteButton_.setVisible (true);

    venueDetailsHeaderLabel_.setVisible (false);
    venueDetailsBodyLabel_.setVisible (false);
    deleteVenueSectionLabel_.setVisible (false);
    confirmVenueNameLabel_.setVisible (false);
    confirmVenueNameEditor_.setVisible (false);
    deleteVenueButton_.setVisible (false);

    resized();
}

void CustomerAdminPage::showVenueDetailsPanel()
{
    showingVenueInRightPanel_ = true;

    profileHeaderLabel_.setVisible (false);
    profileDetailsLabel_.setVisible (false);
    venuesLabel_.setVisible (false);
    legacyProfileLabel_.setVisible (false);
    resetSectionLabel_.setVisible (false);
    sendResetEmailButton_.setVisible (false);
    newPasswordEditor_.setVisible (false);
    setPasswordButton_.setVisible (false);
    removalSectionLabel_.setVisible (false);
    deactivateButton_.setVisible (false);
    reactivateButton_.setVisible (false);
    confirmEmailLabel_.setVisible (false);
    confirmEmailEditor_.setVisible (false);
    hardDeleteButton_.setVisible (false);

    venueDetailsHeaderLabel_.setVisible (true);
    venueDetailsBodyLabel_.setVisible (true);
    deleteVenueSectionLabel_.setVisible (true);
    confirmVenueNameLabel_.setVisible (true);
    confirmVenueNameEditor_.setVisible (true);
    deleteVenueButton_.setVisible (true);

    resized();
}

void CustomerAdminPage::refreshVenueDetailsView()
{
    if (! venueDetailsLoaded_)
    {
        venueDetailsHeaderLabel_.setText ({}, juce::dontSendNotification);
        venueDetailsBodyLabel_.setText ({}, juce::dontSendNotification);
        deleteVenueButton_.setEnabled (false);
        return;
    }

    const auto& v = currentVenueDetails_;
    venueDetailsHeaderLabel_.setText (v.name.isNotEmpty() ? v.name : v.id, juce::dontSendNotification);

    juce::String details;
    details << "Venue ID: " << v.id << "\n";
    details << "Address: "  << (v.address.isNotEmpty() ? v.address : "(none)") << "\n";
    details << "City: "     << (v.city.isNotEmpty() ? v.city : "(unknown)")
            << (v.country.isNotEmpty() ? (", " + v.country) : juce::String()) << "\n";
    details << "Code / Backup code: " << (v.code.isNotEmpty() ? v.code : "(none)")
            << " / " << (v.codePlus.isNotEmpty() ? v.codePlus : "(none)") << "\n";
    details << "Admin email: " << (v.adminEmail.isNotEmpty() ? v.adminEmail : "(none)") << "\n";
    details << "Max songs/singer: " << v.numSongs << "   Singers shown: " << v.numSingers
            << "   Strikes allowed: " << v.numStrikes << "\n";
    details << "Repeat songs: " << (v.repeatSongs ? "yes" : "no")
            << "   Auto-approve: " << (v.autoapprove ? "yes" : "no");
    venueDetailsBodyLabel_.setText (details, juce::dontSendNotification);

    updateDeleteVenueButtonState();
}

void CustomerAdminPage::updateDeleteVenueButtonState()
{
    if (! venueDetailsLoaded_ || currentVenueDetails_.name.isEmpty())
    {
        deleteVenueButton_.setEnabled (false);
        return;
    }

    const auto typed = confirmVenueNameEditor_.getText().trim();
    deleteVenueButton_.setEnabled (typed.isNotEmpty() && typed.equalsIgnoreCase (currentVenueDetails_.name));
}

void CustomerAdminPage::onDeleteVenueClicked()
{
    if (! venueDetailsLoaded_) return;

    const auto venueId = currentVenueDetails_.id;
    const auto confirmName = confirmVenueNameEditor_.getText().trim();

    setStatus ("Deleting venue...");
    deleteVenueButton_.setEnabled (false);

    juce::Component::SafePointer<CustomerAdminPage> safe (this);
    CustomerAdminService::getInstance().deleteVenue (venueId, confirmName,
        [safe, venueId] (bool ok, juce::String error)
        {
            if (safe == nullptr) return;
            if (! ok)
            {
                safe->setStatus ("Venue delete failed: " + error, true);
                safe->updateDeleteVenueButtonState();
                return;
            }

            safe->setStatus ("Venue permanently deleted.");
            safe->venueDetailsLoaded_ = false;
            safe->refreshVenueDetailsView();
            safe->selectedVenueId_.clear();
            safe->selectedVenueName_.clear();
            safe->selectedVenueCity_.clear();
            safe->selectedVenueLabel_.setText (LocalizationManager::getInstance()
                                                    .getText ("page.customer_admin.no_venue_selected"),
                                               juce::dontSendNotification);

            // Drop it from the cached list + current filter so it stops
            // showing up in the picker without a full reload.
            auto& all = safe->allVenues_;
            all.erase (std::remove_if (all.begin(), all.end(),
                                       [&venueId] (const CustomerAdminService::VenueSummary& v)
                                       { return v.id == venueId; }),
                       all.end());
            safe->filterVenueList();
        });
}

//==============================================================================
void CustomerAdminPage::setStatus (const juce::String& message, bool isError)
{
    statusLabel_.setColour (juce::Label::textColourId, isError ? kDanger : kMuted);
    statusLabel_.setText (message, juce::dontSendNotification);
}

//==============================================================================
void CustomerAdminPage::paint (juce::Graphics& g)
{
    MenuTheme::drawPageBackground (g, getLocalBounds());
}

void CustomerAdminPage::resized()
{
    viewport_.setBounds (getLocalBounds());

    const int startingWidth  = juce::jmax (900, viewport_.getWidth() - viewport_.getScrollBarThickness());
    const int startingHeight = juce::jmax (contentHolder_->getHeight(), 700);
    contentHolder_->setSize (startingWidth, startingHeight);
    layoutContent();

    // Grow to fit whichever column (left tab or right panel) actually
    // needed more room -- lets the viewport's scrollbar reach the bottom
    // of either on any window size. layoutContent() only depends on width,
    // so a second pass at the corrected height reproduces the same
    // positions.
    const int leftBottom  = showingSearchTab_ ? searchList_.getBottom() : assignSubmitButton_.getBottom();
    const int rightBottom = showingVenueInRightPanel_ ? deleteVenueButton_.getBottom() : hardDeleteButton_.getBottom();
    const int neededHeight = juce::jmax (leftBottom, rightBottom) + 22;
    if (neededHeight != contentHolder_->getHeight())
    {
        contentHolder_->setSize (startingWidth, neededHeight);
        layoutContent();
    }
}

void CustomerAdminPage::layoutContent()
{
    auto bounds = contentHolder_->getLocalBounds().reduced (22);

    auto header = bounds.removeFromTop (96).reduced (16, 12);
    title_.setBounds (header.removeFromTop (32));
    subtitle_.setBounds (header.removeFromTop (20));
    auto tabsRow = header.removeFromTop (28);
    tabUnassignedButton_.setBounds (tabsRow.removeFromLeft (160));
    tabsRow.removeFromLeft (8);
    tabSearchButton_.setBounds (tabsRow.removeFromLeft (160));
    tabsRow.removeFromLeft (8);
    bulkMetadataButton_.setBounds (tabsRow.removeFromLeft (160));
    tabsRow.removeFromLeft (12);
    statusLabel_.setBounds (tabsRow);

    bounds.removeFromTop (14);

    auto leftColumn = bounds.removeFromLeft (bounds.getWidth() * 3 / 5).reduced (16, 12);
    bounds.removeFromLeft (14);
    auto rightColumn = bounds.reduced (16, 12);

    //--- Left column: whichever tab is active --------------------------------
    if (! showingSearchTab_)
    {
        unassignedTitle_.setBounds (leftColumn.removeFromTop (24));
        auto btnRow = leftColumn.removeFromTop (28);
        refreshUnassignedButton_.setBounds (btnRow.removeFromLeft (100));
        btnRow.removeFromLeft (8);
        unassignedMoreButton_.setBounds (btnRow.removeFromLeft (100));
        leftColumn.removeFromTop (8);
        // Fixed height, not "whatever's left" -- the content now grows to
        // fit its own needs rather than being squeezed into a given window
        // size, so there's no natural "remainder" to fill.
        unassignedList_.setBounds (leftColumn.removeFromTop (220));
        leftColumn.removeFromTop (10);

        assignHeaderLabel_.setBounds (leftColumn.removeFromTop (22));
        venueSearchEditor_.setBounds (leftColumn.removeFromTop (26).reduced (0, 2));
        leftColumn.removeFromTop (4);
        venueListBox_.setBounds (leftColumn.removeFromTop (90));
        leftColumn.removeFromTop (6);
        selectedVenueLabel_.setBounds (leftColumn.removeFromTop (18));
        leftColumn.removeFromTop (6);
        assignRoleLabel_.setBounds (leftColumn.removeFromTop (16));
        assignRoleBox_.setBounds (leftColumn.removeFromTop (26).withWidth (200));
        leftColumn.removeFromTop (6);
        assignSubmitButton_.setBounds (leftColumn.removeFromTop (30).withWidth (220));
    }
    else
    {
        searchLabel_.setBounds (leftColumn.removeFromTop (18));
        auto searchRow = leftColumn.removeFromTop (28);
        searchButton_.setBounds (searchRow.removeFromRight (90));
        searchRow.removeFromRight (8);
        searchQueryEditor_.setBounds (searchRow);
        leftColumn.removeFromTop (8);
        searchMoreButton_.setBounds (leftColumn.removeFromTop (26).withWidth (110));
        leftColumn.removeFromTop (6);
        // Fixed height, not "whatever's left" -- see the unassignedList_
        // comment above for why.
        searchList_.setBounds (leftColumn.removeFromTop (320));
    }

    //--- Right column: profile panel OR venue-details panel ------------------
    // (whichever was most recently selected -- see showHostProfilePanel()/
    // showVenueDetailsPanel()). Both branches lay out into the same
    // rightColumn; only one set of controls is ever visible at a time.
    if (! showingVenueInRightPanel_)
    {
        profileHeaderLabel_.setBounds (rightColumn.removeFromTop (24));
        rightColumn.removeFromTop (4);
        profileDetailsLabel_.setBounds (rightColumn.removeFromTop (150));
        rightColumn.removeFromTop (6);
        venuesLabel_.setBounds (rightColumn.removeFromTop (70));
        rightColumn.removeFromTop (6);
        legacyProfileLabel_.setBounds (rightColumn.removeFromTop (20));
        rightColumn.removeFromTop (10);

        resetSectionLabel_.setBounds (rightColumn.removeFromTop (20));
        auto resetRow = rightColumn.removeFromTop (30);
        sendResetEmailButton_.setBounds (resetRow.removeFromLeft (160));
        rightColumn.removeFromTop (6);
        auto pwRow = rightColumn.removeFromTop (30);
        newPasswordEditor_.setBounds (pwRow.removeFromLeft (pwRow.getWidth() - 200));
        pwRow.removeFromLeft (8);
        setPasswordButton_.setBounds (pwRow);
        rightColumn.removeFromTop (14);

        removalSectionLabel_.setBounds (rightColumn.removeFromTop (20));
        auto removalRow = rightColumn.removeFromTop (30);
        deactivateButton_.setBounds (removalRow.removeFromLeft (170));
        removalRow.removeFromLeft (8);
        reactivateButton_.setBounds (removalRow.removeFromLeft (120));
        rightColumn.removeFromTop (10);

        confirmEmailLabel_.setBounds (rightColumn.removeFromTop (16));
        auto deleteRow = rightColumn.removeFromTop (28);
        confirmEmailEditor_.setBounds (deleteRow.removeFromLeft (deleteRow.getWidth() - 170));
        deleteRow.removeFromLeft (8);
        hardDeleteButton_.setBounds (deleteRow);
    }
    else
    {
        venueDetailsHeaderLabel_.setBounds (rightColumn.removeFromTop (24));
        rightColumn.removeFromTop (4);
        venueDetailsBodyLabel_.setBounds (rightColumn.removeFromTop (180));
        rightColumn.removeFromTop (16);

        deleteVenueSectionLabel_.setBounds (rightColumn.removeFromTop (20));
        confirmVenueNameLabel_.setBounds (rightColumn.removeFromTop (16));
        auto deleteVenueRow = rightColumn.removeFromTop (28);
        confirmVenueNameEditor_.setBounds (deleteVenueRow.removeFromLeft (deleteVenueRow.getWidth() - 150));
        deleteVenueRow.removeFromLeft (8);
        deleteVenueButton_.setBounds (deleteVenueRow);
    }
}
