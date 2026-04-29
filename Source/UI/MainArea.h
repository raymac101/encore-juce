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
#include "SettingsPage.h"
#include "SongSelectionDialog.h"
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
    ~MainArea() override = default;

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

private:
    NavPage currentPage = NavPage::Home;

    // Owns all page components (keyed by NavPage int value)
    std::unordered_map<int, std::unique_ptr<juce::Component>> pages;

    // Concrete page pointers (non-owning, for type-safe access)
    HomePage*     homePage     = nullptr;
    SearchPage*   searchPage   = nullptr;
    LibraryPage*  libraryPage  = nullptr;
    SettingsPage* settingsPage = nullptr;

public:
    /** Push a venue snapshot into the settings page (call from FirebaseManager callback). */
    void setVenueData(const VenueItem& venue)
    {
        if (settingsPage) settingsPage->setVenueData(venue);
    }

    /** Switch to the Library page and kick off the full initial song-load
        flow (file-chooser + scan). Used after a venue switch. */
    void triggerInitialSongLoad()
    {
        setCurrentPage(NavPage::Library);
        if (libraryPage) libraryPage->startInitialSongLoad();
    }

    /** Fired when the user saves a setting. Wire to FirebaseManager::updateVenue(). */
    std::function<void(const VenueItem&)> onVenueSettingsChanged;

    /** Fired when the user presses Add to Queue / Play Next / Play Now in the
        Song Selection dialog (or cancels it). Wire this up in the app shell
        to drive the queue / player. */
    std::function<void(const SongSelectionResult&)> onSongSelectionResult;

private:

    void addPage(NavPage page, const juce::String& label);
    void loadBackgroundTile();

    juce::Image backgroundTile_;
    int tileSize_ = 340;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainArea)
};
