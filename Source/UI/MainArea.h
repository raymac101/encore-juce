/*
  ==============================================================================

    MainArea.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Central content area that swaps child components based on the
    currently selected NavPage.  Each "page" is a placeholder component
    for now; real implementations will replace them later.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "NavBar.h"           // For NavPage enum
#include "HomePage.h"
#include "SearchPage.h"
#include "LibraryPage.h"
#include "ChartsPage.h"
#include "SettingsPage.h"
#include "CompanyAdminPage.h"
#include "CustomerAdminPage.h"
#include "ProfilePage.h"
#include "MixerPage.h"
#include "TestingPage.h"
#include "AdsPage.h"
#include "SongSelectionDialog.h"
#include "SongEditDialog.h"
#include "../Models/VenueItem.h"
#include "../Localization/LocalizationManager.h"
#include <unordered_map>
//==============================================================================
/**
    A simple placeholder page that displays the page name.
    Will be subclassed / replaced with real page implementations.
*/
class PlaceholderPage : public juce::Component
{
public:
    explicit PlaceholderPage(const juce::String& pageName) : name(pageName) {}

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xff16213e));

        g.setColour(juce::Colour(0xffe0e0e0).withAlpha(0.2f));
        g.drawRect(getLocalBounds(), 1);

        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions().withHeight(26.0f)).boldened());
        g.drawText(name, getLocalBounds(), juce::Justification::centred);
    }

    void setPageName(const juce::String& newName) { name = newName; repaint(); }

private:
    juce::String name;
};

//==============================================================================
/**
    Central content area that owns one child component per NavPage and
    shows/hides them in response to setCurrentPage().
*/
class MainArea : public juce::Component
{
public:
    MainArea();
    ~MainArea() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Switch the visible page. */
    void setCurrentPage(NavPage page);
    NavPage getCurrentPage() const { return currentPage; }

    /** Read-only access to the loaded song library. Returns an empty list
        if the library hasn't finished loading yet. */
    const std::vector<CdgSong>& getLibrarySongs() const;

    /** Direct access to the embedded SettingsPage (may be null briefly
        during construction). Used by MainComponent to wire callbacks
        such as onEndSession. */
    SettingsPage* getSettingsPage() noexcept { return settingsPage; }

    /** Re-read all translatable strings from LocalizationManager. */
    void updateAllText();

    /** Provide AudioEngine to pages that need direct DSP control. */
    void setAudioEngine(AudioEngine* engine);

    /** Provide the background-music service to pages that need it (Mixer). */
    void setBackgroundMusicPlayer(BackgroundMusicPlayer* player);

    /** Forwards to MixerPage::setVenueContext -- see that method's comment. */
    void setVenueContext(const juce::String& venueId, const juce::String& venueName);

    /** Forwards to MixerPage::refreshRoomEqProfilePicker -- called by
        MainComponent::setVenueId() after a venue-auto-load check may have
        changed the selected/applied Room EQ profile out from under the
        Mixer page's picker. */
    void refreshRoomEqProfilePicker();

private:
    NavPage currentPage = NavPage::Home;

    // Owns all page components (keyed by NavPage int value)
    std::unordered_map<int, std::unique_ptr<juce::Component>> pages;

    // Concrete page pointers (non-owning, for type-safe access)
    HomePage*     homePage     = nullptr;
    SearchPage*   searchPage   = nullptr;
    LibraryPage*  libraryPage  = nullptr;
    ChartsPage*   chartsPage   = nullptr;
    MixerPage*    mixerPage    = nullptr;
    SettingsPage* settingsPage = nullptr;
    TestingPage*  testingPage  = nullptr;
    CompanyAdminPage* companyAdminPage = nullptr;
    CustomerAdminPage* customerAdminPage = nullptr;
    ProfilePage* profilePage = nullptr;
    AdsPage* adsPage = nullptr;

public:
    /** Push a venue snapshot into the settings page (call from FirebaseManager callback). */
    void setVenueData(const VenueItem& venue)
    {
        if (settingsPage) settingsPage->setVenueData(venue);
    }

    /** Tells the Library page which venue's Storage songbook.json copy
        should be refreshed after each completed scan. */
    void setActiveVenueId(const juce::String& venueId)
    {
        if (libraryPage) libraryPage->setActiveVenueId(venueId);
    }

    /** Switch to the Library page and kick off the full initial song-load
        flow (file-chooser + scan). Used after a venue switch. */
    void triggerInitialSongLoad()
    {
        setCurrentPage(NavPage::Library);
        if (libraryPage) libraryPage->startInitialSongLoad();
    }

    /** Direct access to the Library page so the app shell can issue
        single-song writes (upsertSong / deleteSong) from the Edit dialog. */
    LibraryPage* getLibraryPage() const noexcept { return libraryPage; }

    /** Direct access to the Home page so the app shell can push
        venue-side playlist data (Popular / Recommended) into its rows. */
    HomePage* getHomePage() const noexcept { return homePage; }

    /** Direct access to the testing tool page. */
    TestingPage* getTestingPage() const noexcept { return testingPage; }

    /** Direct access to the company-admin dashboard page. */
    CompanyAdminPage* getCompanyAdminPage() const noexcept { return companyAdminPage; }

    /** Direct access to the EnterpriseAdmin-only customer support page. */
    CustomerAdminPage* getCustomerAdminPage() const noexcept { return customerAdminPage; }

    /** Direct access to the self-service "Edit Profile" page (reachable only
        via the TopBar user-menu dropdown, not the sidebar). */
    ProfilePage* getProfilePage() const noexcept { return profilePage; }

    /** Direct access to the Ads management page. */
    AdsPage* getAdsPage() const noexcept { return adsPage; }

    /** Direct access to the charts/analytics page. */
    ChartsPage* getChartsPage() const noexcept { return chartsPage; }

    /** Update company-dashboard context for company-mode users. */
    void setCompanyContext (const juce::String& companyId, const juce::String& companyRole)
    {
        if (companyAdminPage)
            companyAdminPage->setCompanyContext (companyId, companyRole);
        if (chartsPage)
            chartsPage->setCompanyScope (companyId);
    }

    /** Fired when the user saves a setting. Wire to FirebaseManager::updateVenue(). */
    std::function<void(const VenueItem&)> onVenueSettingsChanged;

    /** Fired with a short phase description during a Library scan/metadata/
        upload sequence ("Scanning Folders...", "Uploading Songbook...", etc.),
        and with an empty string once nothing is in progress. MainComponent
        forwards this to the BottomBar status area. */
    std::function<void(const juce::String&)> onLibraryStatusMessage;

    /** Fired when the user presses Add to Queue / Play Next / Play Now in the
        Song Selection dialog (or cancels it). Wire this up in the app shell
        to drive the queue / player. */
    std::function<void(const SongSelectionResult&)> onSongSelectionResult;

    /** Fired when the user saves or deletes a song in the Song Edit dialog
        (Edit column on a SearchPage row). The shell is responsible for
        persisting the updated CdgSong to the song database / songbook.json,
        deleting it from Firestore, and patching playlist memberships. */
    std::function<void(const SongEditResult&)> onSongEditResult;

    /** Fired once per "Add Songs" import with exactly the songs newly added
        that time (never for the initial full library scan). The shell
        pushes these into the venue's Firestore "new songs" feed. */
    std::function<void(const std::vector<CdgSong>&)> onSongsAddedViaAddSongs;

    /** Synchronous query that fills in the initial playlist-membership
        flags for the Edit dialog (whether the song is currently in
        New / Popular / Recommended for this venue). */
    std::function<void(const CdgSong&, SongEditDialog::InitialPlaylists&)>
        onSongEditPlaylistQuery;

    /** Optional async metadata fetcher (Spotify-style). Set on MainArea so
        the Edit dialog can call it directly via SongEditDialog::launch. */
    SongEditDialog::MetadataFetcher onSongEditFetchMetadata;

private:

    void addPage(NavPage page, const juce::String& label);
    void loadBackgroundTile();

    juce::Image backgroundTile_;
    int tileSize_ = 340;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainArea)
};
