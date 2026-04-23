/*
  ==============================================================================

    SettingsPage.h
    Created: 22 Apr 2026
    Author:  GitHub Copilot

    Settings page – mirrors the Angular settings component's configurable
    options (venue info, queue rules, display prefs, session management).
    All data is sourced from the VenueItem model which maps 1:1 to the
    Firestore venues/ document schema.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "../Localization/LocalizationManager.h"
#include "../Models/VenueItem.h"

// Forward-declared internal panel (defined in SettingsPage.cpp)
class SettingsContentPanel;

//==============================================================================
/**
    Full-featured settings page.

    Sections
    --------
    1. Venue Information – editable name / address / city / country
    2. Venue Identity    – read-only ID, venue code, emergency code
    3. Queue Settings    – songs per singer, singers visible, skips, repeat,
                           auto-approve
    4. Display Settings  – lyrics background, online-songs toggles, memory stats
    5. Session Management– clear recently played, end session & archive

    Data flow
    ---------
    - Call setVenueData() to populate the page from a VenueItem
      (e.g. after a Firestore venue snapshot arrives).
    - Set onSettingsChanged to receive updated VenueItem whenever the user
      saves — the caller is responsible for persisting to Firestore /
      FirebaseManager::updateVenue().
*/
class SettingsPage : public juce::Component
{
public:
    //==========================================================================
    SettingsPage();
    ~SettingsPage() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

    //==========================================================================
    // Data API

    /** Populate all controls from a VenueItem (e.g. Firestore snapshot). */
    void setVenueData(const VenueItem& venue);

    /** Read-back the venue as currently displayed (before any save). */
    const VenueItem& getVenueData() const { return venue_; }

    /** Called whenever the user commits a change that should be persisted.
        Receives the full updated VenueItem. */
    std::function<void(const VenueItem&)> onSettingsChanged;

    /** Cache the current venue to ~/Library/EncoreKaraoke/settings.json.
        Also called internally on each save so data survives restarts. */
    void saveToCache();

    /** Restore venue data from the local cache (used before Firestore loads). */
    void loadFromCache();

    //==========================================================================
    // Action callbacks (wire these from MainComponent / FirebaseManager)

    /** User clicked "Delete Venue". Caller should confirm + delete from Firestore. */
    std::function<void()> onDeleteVenue;

    /** User wants to set the venue code to a specific string. */
    std::function<void(const juce::String& newCode)> onSetVenueCode;

    /** User wants to generate a random venue code. */
    std::function<void()> onGenerateVenueCode;

    /** User wants to set the emergency code. */
    std::function<void(const juce::String& newCode)> onSetEmergencyCode;

    /** User wants to generate a random emergency code. */
    std::function<void()> onGenerateEmergencyCode;

    /** User wants to invite a new user. Provides email and role string ("Basic User" / "Host"). */
    std::function<void(const juce::String& email, const juce::String& role)> onInviteUser;

    /** User wants to change the role of an existing user (identified by email). */
    std::function<void(const juce::String& email, const juce::String& newRole)> onChangeUserRole;

    /** User wants to deactivate an existing user (identified by email). */
    std::function<void(const juce::String& email)> onDeactivateUser;

    /** User wants to remove an existing user (identified by email). */
    std::function<void(const juce::String& email)> onRemoveUser;

    /** User wants to upload a new logo. Provides the local file path chosen. */
    std::function<void(const juce::File& logoFile)> onUploadLogo;

    /** User wants to reset to the default logo. */
    std::function<void()> onResetLogo;

    //==========================================================================
    // Session stats (push live values from FirebaseManager)

    struct SessionStats
    {
        int songsPlayedToday = 0;
        int activeMembers    = 0;
        int singersInQueue   = 0;
        int requestedSongs   = 0;
    };

    /** Update the live session-stats panel. */
    void setSessionStats(const SessionStats& stats);

    //==========================================================================
    // User list (push list of venue users from Firestore)

    struct VenueUser
    {
        juce::String email;
        juce::String role;      // "Basic", "Host", "Admin"
        bool active = true;
    };

    /** Replace the user list displayed in the User Management section. */
    void setUserList(const std::vector<VenueUser>& users);

private:
    //==========================================================================
    // Colours (matching the rest of the app)
    static constexpr uint32_t kBg     = 0xff16213e;
    static constexpr uint32_t kAccent = 0xff7b5ea7;

    //==========================================================================
    static juce::File getCacheFile();

    /** Save cache + fire onSettingsChanged. Called by SettingsContentPanel. */
    void notifyChanged();

    VenueItem venue_;
    juce::Viewport scroll_;
    std::unique_ptr<SettingsContentPanel> panel_;

    // Allow SettingsContentPanel to access venue_ and notifyChanged()
    friend class SettingsContentPanel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SettingsPage)
};
