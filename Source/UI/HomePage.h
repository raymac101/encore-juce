/*
  ==============================================================================

    HomePage.h
    Created: 19 Apr 2026
    Author:  GitHub Copilot

    Home page component – shows horizontally-scrollable rows of song cards
    grouped by: Recently Played, New Songs, Popular Songs, Recommended Songs.
    Each song card displays artwork, song title and artist name.
    Mirrors the Angular HomeComponent's swiper-based layout.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "SearchPage.h"
#include "../Models/Track.h"
#include "../Models/Playlist.h"
#include "../Models/CdgSong.h"
#include "../Localization/LocalizationManager.h"
#include "../Services/ImageCache.h"
#include <vector>
#include <functional>

//==============================================================================
/**
    A single song card drawn as artwork + artist + title.
*/
class SongCard : public juce::Component
{
public:
    SongCard();

    void paint(juce::Graphics& g) override;
    void mouseEnter(const juce::MouseEvent&) override { hovering = true;  repaint(); }
    void mouseExit(const juce::MouseEvent&) override  { hovering = false; repaint(); }
    void mouseUp(const juce::MouseEvent&) override;

    void setTrack(const juce::String& artist, const juce::String& song,
                  const juce::String& imageUrl, bool isExplicit = false);
    void setArtwork(const juce::Image& img);

    // Starts an async image download for the current imageUrl.
    void loadArtwork();

    std::function<void()> onClick;

private:
    juce::String artistText;
    juce::String songText;
    juce::String imageUrlText;
    juce::Image  artwork;
    bool         hasArtwork = false;
    bool         explicitFlag = false;
    bool         hovering = false;

    // Placeholder colour generated from song name hash
    juce::Colour placeholderColour;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongCard)
};

//==============================================================================
/**
    Horizontal scrollable row of SongCards with a section title label.
    Equivalent to one Swiper section in the Angular home.
*/
class SongRow : public juce::Component
{
public:
    SongRow();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override;

    void setTitle(const juce::String& title);

    // Populate from Track list (online / recently played)
    void setTracks(const std::vector<Track>& tracks);

    // Populate from Playlist list (new / popular / recommended)
    void setPlaylists(const std::vector<Playlist>& playlists);

    // Clear all cards
    void clear();

    std::function<void(int index)> onSongClicked;

    static constexpr int rowHeight    = 230;
    static constexpr int titleHeight  = 32;
    static constexpr int cardWidth    = 160;
    static constexpr int cardSpacing  = 10;

private:
    juce::String          sectionTitle;
    juce::Viewport        viewport;
    juce::Component       strip;
    juce::OwnedArray<SongCard> cards;

    void rebuildStrip();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SongRow)
};

//==============================================================================
/**
    "Recent searches" dropdown shown under the Home search box while it has
    focus. Never takes keyboard focus itself, so clicking a row doesn't blur
    the search box (which is what hides the dropdown).
*/
class RecentSearchesPanel : public juce::Component
{
public:
    RecentSearchesPanel();

    void setItems(const juce::StringArray& items);
    int  getPreferredHeight() const;
    void updateAllText();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& e) override;
    void mouseExit(const juce::MouseEvent& e) override;
    void mouseUp(const juce::MouseEvent& e) override;

    std::function<void(const juce::String& query)> onPick;
    std::function<void()> onClear;

private:
    static constexpr int kHeaderHeight = 44;
    static constexpr int kRowHeight    = 44;
    static constexpr int kFooterHeight = 56;

    juce::StringArray items_;
    int hoverRow_ = -1;
    juce::String title_;
    juce::TextButton clearButton_;

    int rowAt(juce::Point<int> p) const;
};

//==============================================================================
/**
    Home page component containing multiple SongRow sections, plus an
    embedded SearchPage. The two used to be separate NavBar destinations;
    now Home owns the one visible search bar, and typing a query swaps the
    Popular/New rows for the (otherwise unmodified) Search results -- see
    showBrowseView()/showSearchResults().
*/
class HomePage : public juce::Component
{
public:
    HomePage();
    ~HomePage() override = default;

    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;

    /** Populate each section with data. */
    void setRecentlyPlayed(const std::vector<Track>& tracks);

    /** Populate the Recently Played row from a Firestore play-history list.
        Resolves each entry against the library for full CdgSong data. */
    void setRecentlyPlayedFromHistory(const std::vector<Playlist>& items);

    void setNewSongs(const std::vector<Playlist>& playlists);
    void setPopularSongs(const std::vector<Playlist>& playlists);
    void setRecommendedSongs(const std::vector<Playlist>& playlists);

    /**
        Populate all home page rows from the local song library.
        - Popular songs: highest-rated (by average rating)
        - New songs:     newest by release date
        - Recommended:   random selection with metadata
    */
    void setSongsFromLibrary(const std::vector<CdgSong>& songs);

    /** Re-read translatable strings. */
    void updateAllText();

    /** Called when a song card is clicked. Passes the resolved CdgSong so
        the caller can open the Song Selection dialog. The song will be empty
        (isValid()==false) for sections sourced from Track lists that don't
        have a matching library entry. */
    std::function<void(const CdgSong& song)> onSongClicked;

    /** Fired by the embedded Search view's Edit column -- forwarded
        verbatim from SearchPage::onSongEditClicked. */
    std::function<void(const CdgSong& song)> onSongEditClicked;

private:
    std::unique_ptr<SongRow> recentRow;
    std::unique_ptr<SongRow> newSongsRow;
    std::unique_ptr<SongRow> popularRow;
    std::unique_ptr<SongRow> recommendedRow;

    // Declared before searchBox_ so it outlives it: the box's focus-loss
    // callback hides this panel, and that can fire during destruction.
    RecentSearchesPanel               recentPanel_;

    // Top search row -- home icon + search box + submit icon -- and the
    // two mutually-exclusive views it switches between.
    std::unique_ptr<juce::Button>     homeIconButton_;
    std::unique_ptr<juce::TextEditor> searchBox_;
    std::unique_ptr<juce::Button>     searchSubmitButton_;
    std::unique_ptr<juce::Button>     clearSearchButton_;   // X inside the box
    std::unique_ptr<juce::Button>     browseAllButton_;     // right of the X: all songs, no query
    std::unique_ptr<SearchPage>       embeddedSearch_;
    bool                              showingSearch_ = false;

    void showBrowseView();
    void showSearchResults();

    // Recent searches -- most recent first, persisted to settings.xml.
    // A query is recorded on Enter or when a song is picked from its results.
    static constexpr int kMaxRecentSearches = 8;
    static constexpr int kMaxRecentShown    = 5;
    juce::StringArray recentSearches_;
    void loadRecentSearches();
    void saveRecentSearches() const;
    void recordRecentSearch(const juce::String& query);
    void updateRecentPanel();
    void hideRecentPanel();

    // Per-section CdgSong backing lists.  setSongsFromLibrary populates the
    // Recently Played + New rows; the Popular and Recommended rows are
    // populated from venue Firestore playlists via setPopular/RecommendedSongs.
    // Click resolution always reads from these backing lists.
    std::vector<CdgSong> recentSongsSongs_;
    std::vector<CdgSong> newSongsSongs_;
    std::vector<CdgSong> popularSongsSongs_;
    std::vector<CdgSong> recommendedSongsSongs_;

    // Last-known full library, kept so Popular/Recommended playlist entries
    // can be resolved back to a full CdgSong for Play Now / Add to Queue.
    std::vector<CdgSong> libraryRef_;

    juce::Viewport  pageViewport;
    juce::Component pageContent;

    void layoutRows();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HomePage)
};
