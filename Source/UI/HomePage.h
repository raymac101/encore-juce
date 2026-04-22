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
    Home page component containing multiple SongRow sections.
*/
class HomePage : public juce::Component
{
public:
    HomePage();
    ~HomePage() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

    /** Populate each section with data. */
    void setRecentlyPlayed(const std::vector<Track>& tracks);
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

    /** Called when a song card is clicked. */
    std::function<void(const juce::String& sectionId, int index)> onSongClicked;

private:
    std::unique_ptr<SongRow> recentRow;
    std::unique_ptr<SongRow> newSongsRow;
    std::unique_ptr<SongRow> popularRow;
    std::unique_ptr<SongRow> recommendedRow;

    juce::Viewport  pageViewport;
    juce::Component pageContent;

    void layoutRows();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HomePage)
};
